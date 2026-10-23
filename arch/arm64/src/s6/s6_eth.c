/****************************************************************************
 * arch/arm64/src/s6/s6_eth.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <debug.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/cache.h>
#include <nuttx/irq.h>
#include <nuttx/wqueue.h>
#include <nuttx/net/ethernet.h>
#include <nuttx/net/gmii.h>
#include <nuttx/net/mii.h>
#include <nuttx/net/netdev_lowerhalf.h>

#include "arm64_internal.h"
#include "s6_eth.h"
#include "hardware/s6_eth.h"

#ifdef CONFIG_S6_ETH

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define S6_ETH_NTXDESC          16
#define S6_ETH_NRXDESC          16

/* RX buffers are whole cache lines so that invalidating one never touches
 * anything else.
 */

#define S6_ETH_LINESIZE         64
#define S6_ETH_BUFSIZE          1536
#define S6_ETH_FCS_LEN          4

#define S6_ETH_PHYADDR          0
#define S6_ETH_TIMEOUT_MS       1000
#define S6_ETH_POLL_DELAY       SEC2TICK(1)

/* RTL8211F PHY specific status register, page 0xa43 */

#define RTL8211F_PAGSR          0x1f
#define RTL8211F_PHYSR_PAGE     0xa43
#define RTL8211F_PHYSR          0x1a
#define RTL8211F_PHYSR_DUPLEX   (1 << 3)
#define RTL8211F_PHYSR_SPEED    (3 << 4)
#define RTL8211F_PHYSR_100      (1 << 4)
#define RTL8211F_PHYSR_1000     (2 << 4)

/* Transmit sends straight out of the IOB, so a frame must fit in one */

#if CONFIG_IOB_BUFSIZE < CONFIG_NET_LL_GUARDSIZE - ETH_HDRLEN + \
                         CONFIG_NET_ETH_PKTSIZE
#  error "CONFIG_IOB_BUFSIZE cannot hold a full Ethernet frame"
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct s6_eth_desc_s
{
  volatile uint32_t des0;
  volatile uint32_t des1;
  volatile uint32_t des2;
  volatile uint32_t des3;
};

struct s6_eth_descmem_s
{
  struct s6_eth_desc_s tx[S6_ETH_NTXDESC];
  struct s6_eth_desc_s rx[S6_ETH_NRXDESC];
};

struct s6_eth_driver_s
{
  struct netdev_lowerhalf_s dev;            /* Must be first */
  struct work_s pollwork;                   /* Link status polling */
  FAR netpkt_t *txpkt[S6_ETH_NTXDESC];      /* Packets owned by TX DMA */
  unsigned int txhead;                      /* Free running, next to fill */
  unsigned int txtail;                      /* Free running, next to free */
  unsigned int rxhead;                      /* Next RX descriptor to check */
  bool link;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int s6_eth_ifup(FAR struct netdev_lowerhalf_s *dev);
static int s6_eth_ifdown(FAR struct netdev_lowerhalf_s *dev);
static int s6_eth_transmit(FAR struct netdev_lowerhalf_s *dev,
                           FAR netpkt_t *pkt);
static FAR netpkt_t *s6_eth_receive(FAR struct netdev_lowerhalf_s *dev);
static void s6_eth_reclaim(FAR struct netdev_lowerhalf_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct netdev_ops_s g_s6_eth_ops =
{
  .ifup     = s6_eth_ifup,
  .ifdown   = s6_eth_ifdown,
  .transmit = s6_eth_transmit,
  .receive  = s6_eth_receive,
  .reclaim  = s6_eth_reclaim
};

/* The DMA writes back 16 byte descriptors, less than a cache line, so they
 * go to the non-cacheable section.
 */

static struct s6_eth_descmem_s g_desc locate_data(".noncache");
static FAR struct s6_eth_desc_s *const g_txdesc = g_desc.tx;
static FAR struct s6_eth_desc_s *const g_rxdesc = g_desc.rx;

static uint8_t g_rxbuf[S6_ETH_NRXDESC][S6_ETH_BUFSIZE]
  aligned_data(S6_ETH_LINESIZE);

static struct s6_eth_driver_s g_s6_eth;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: s6_eth_mdio_wait
 *
 * Description:
 *   Wait for the MDIO bus to finish the current frame.
 *
 ****************************************************************************/

static int s6_eth_mdio_wait(void)
{
  int i;

  for (i = 0; i < S6_ETH_TIMEOUT_MS * 10; i++)
    {
      if ((getreg32(S6_ETH_BASE + S6_ETH_MAC_MDIO_ADDR) &
           S6_ETH_MDIO_GB) == 0)
        {
          return OK;
        }

      up_udelay(100);
    }

  nerr("MDIO timeout\n");
  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: s6_eth_mdio_read
 *
 * Description:
 *   Read a PHY register, returning 0xffff when the bus does not respond.
 *
 ****************************************************************************/

static uint16_t s6_eth_mdio_read(uint8_t reg)
{
  putreg32(S6_ETH_MDIO_PA(S6_ETH_PHYADDR) | S6_ETH_MDIO_RDA(reg) |
           S6_ETH_MDIO_CR(S6_ETH_MDIO_CR_150_250M) |
           S6_ETH_MDIO_GOC_READ | S6_ETH_MDIO_GB,
           S6_ETH_BASE + S6_ETH_MAC_MDIO_ADDR);

  if (s6_eth_mdio_wait() < 0)
    {
      return 0xffff;
    }

  return getreg32(S6_ETH_BASE + S6_ETH_MAC_MDIO_DATA) & 0xffff;
}

/****************************************************************************
 * Name: s6_eth_mdio_write
 ****************************************************************************/

static void s6_eth_mdio_write(uint8_t reg, uint16_t value)
{
  putreg32(value, S6_ETH_BASE + S6_ETH_MAC_MDIO_DATA);
  putreg32(S6_ETH_MDIO_PA(S6_ETH_PHYADDR) | S6_ETH_MDIO_RDA(reg) |
           S6_ETH_MDIO_CR(S6_ETH_MDIO_CR_150_250M) |
           S6_ETH_MDIO_GOC_WRITE | S6_ETH_MDIO_GB,
           S6_ETH_BASE + S6_ETH_MAC_MDIO_ADDR);
  s6_eth_mdio_wait();
}

/****************************************************************************
 * Name: s6_eth_rxgive
 *
 * Description:
 *   Hand RX descriptor i with its buffer back to the DMA.  The tail
 *   pointer is set to the descriptor just given back, so the DMA can run
 *   up to it whether it treats the tail as inclusive or not.
 *
 ****************************************************************************/

static void s6_eth_rxgive(unsigned int i)
{
  FAR struct s6_eth_desc_s *desc = &g_rxdesc[i];

  desc->des0 = (uint32_t)(uintptr_t)g_rxbuf[i];
  desc->des1 = 0;
  desc->des2 = 0;
  UP_DMB();
  desc->des3 = S6_ETH_RDES3_OWN | S6_ETH_RDES3_IOC | S6_ETH_RDES3_BUF1V;
  UP_DSB();

  putreg32((uint32_t)(uintptr_t)desc,
           S6_ETH_BASE + S6_ETH_DMA_CH0_RXDESC_TAIL);
}

/****************************************************************************
 * Name: s6_eth_reclaim
 *
 * Description:
 *   Free the packets whose descriptors the TX DMA has finished with.
 *
 ****************************************************************************/

static void s6_eth_reclaim(FAR struct netdev_lowerhalf_s *dev)
{
  FAR struct s6_eth_driver_s *priv = (FAR struct s6_eth_driver_s *)dev;

  while (priv->txtail != priv->txhead)
    {
      unsigned int i = priv->txtail % S6_ETH_NTXDESC;
      FAR struct s6_eth_desc_s *desc = &g_txdesc[i];

      if ((desc->des3 & S6_ETH_TDES3_OWN) != 0)
        {
          break;
        }

      if ((desc->des3 & S6_ETH_TDES3_ES) != 0)
        {
          NETDEV_TXERRORS(&dev->netdev);
        }

      netpkt_free(dev, priv->txpkt[i], NETPKT_TX);
      priv->txpkt[i] = NULL;
      priv->txtail++;
    }
}

/****************************************************************************
 * Name: s6_eth_transmit
 *
 * Description:
 *   Queue one packet.  The DMA reads the frame straight from the IOB, so
 *   the packet is kept until s6_eth_reclaim() sees its descriptor done.
 *
 ****************************************************************************/

static int s6_eth_transmit(FAR struct netdev_lowerhalf_s *dev,
                           FAR netpkt_t *pkt)
{
  FAR struct s6_eth_driver_s *priv = (FAR struct s6_eth_driver_s *)dev;
  unsigned int len = netpkt_getdatalen(dev, pkt);
  uintptr_t data = (uintptr_t)netpkt_getdata(dev, pkt);
  unsigned int i;
  FAR struct s6_eth_desc_s *desc;

  s6_eth_reclaim(dev);

  /* A full ring would put the tail pointer on the oldest busy descriptor,
   * which the DMA reads as an empty ring.
   */

  if (priv->txhead - priv->txtail >= S6_ETH_NTXDESC - 1)
    {
      return -EBUSY;
    }

  up_clean_dcache(data, data + len);

  i = priv->txhead % S6_ETH_NTXDESC;
  desc = &g_txdesc[i];
  desc->des0 = (uint32_t)data;
  desc->des1 = 0;
  desc->des2 = len | S6_ETH_TDES2_IOC;
  UP_DMB();
  desc->des3 = len | S6_ETH_TDES3_FD | S6_ETH_TDES3_LD | S6_ETH_TDES3_OWN;
  UP_DSB();

  priv->txpkt[i] = pkt;
  priv->txhead++;

  putreg32((uint32_t)(uintptr_t)&g_txdesc[priv->txhead % S6_ETH_NTXDESC],
           S6_ETH_BASE + S6_ETH_DMA_CH0_TXDESC_TAIL);
  return OK;
}

/****************************************************************************
 * Name: s6_eth_receive
 *
 * Description:
 *   Copy the next received frame into a new packet.  Bad frames and frames
 *   that find no free packet are dropped.
 *
 ****************************************************************************/

static FAR netpkt_t *s6_eth_receive(FAR struct netdev_lowerhalf_s *dev)
{
  FAR struct s6_eth_driver_s *priv = (FAR struct s6_eth_driver_s *)dev;

  for (; ; )
    {
      unsigned int i = priv->rxhead;
      FAR struct s6_eth_desc_s *desc = &g_rxdesc[i];
      FAR netpkt_t *pkt = NULL;
      uint32_t des3;
      unsigned int len;

      des3 = desc->des3;
      if ((des3 & S6_ETH_RDES3_OWN) != 0)
        {
          return NULL;
        }

      UP_DMB();

      /* Every frame fits in one buffer.  The length includes the FCS. */

      len = des3 & S6_ETH_RDES3_PL_MASK;
      if ((des3 & (S6_ETH_RDES3_ES | S6_ETH_RDES3_FD | S6_ETH_RDES3_LD)) !=
          (S6_ETH_RDES3_FD | S6_ETH_RDES3_LD) || len <= S6_ETH_FCS_LEN)
        {
          NETDEV_RXERRORS(&dev->netdev);
        }
      else
        {
          len -= S6_ETH_FCS_LEN;
          pkt = netpkt_alloc(dev, NETPKT_RX);
          if (pkt != NULL)
            {
              up_invalidate_dcache((uintptr_t)g_rxbuf[i],
                                   (uintptr_t)g_rxbuf[i] + len);
              if (netpkt_copyin(dev, pkt, g_rxbuf[i], len, 0) < 0)
                {
                  netpkt_free(dev, pkt, NETPKT_RX);
                  pkt = NULL;
                }
            }

          if (pkt == NULL)
            {
              NETDEV_RXDROPPED(&dev->netdev);
            }
        }

      s6_eth_rxgive(i);
      priv->rxhead = (i + 1) % S6_ETH_NRXDESC;

      if (pkt != NULL)
        {
          return pkt;
        }
    }
}

/****************************************************************************
 * Name: s6_eth_interrupt
 *
 * Description:
 *   Acknowledge the DMA channel and wake up the upper half.  The line is
 *   set up as edge triggered, so clear until nothing is left pending: a
 *   status bit left set would hold the line high and lose every later
 *   edge.
 *
 ****************************************************************************/

static int s6_eth_interrupt(int irq, FAR void *context, FAR void *arg)
{
  FAR struct s6_eth_driver_s *priv = arg;
  uint32_t status;

  while ((status = getreg32(S6_ETH_BASE + S6_ETH_DMA_CH0_STATUS) &
                   S6_ETH_DMA_STAT_W1C) != 0)
    {
      putreg32(status, S6_ETH_BASE + S6_ETH_DMA_CH0_STATUS);

      if ((status & S6_ETH_DMA_STAT_RI) != 0)
        {
          netdev_lower_rxready(&priv->dev);
        }

      if ((status & S6_ETH_DMA_STAT_TI) != 0)
        {
          netdev_lower_txdone(&priv->dev);
        }

      if ((status & S6_ETH_DMA_STAT_FBE) != 0)
        {
          nerr("DMA fatal bus error, status %08" PRIx32 "\n", status);
        }
    }

  return OK;
}

/****************************************************************************
 * Name: s6_eth_poll_work
 *
 * Description:
 *   Poll the PHY link once a second.  On link up the MAC is set to the
 *   negotiated speed and duplex and TX/RX are turned on.
 *
 ****************************************************************************/

static void s6_eth_poll_work(FAR void *arg)
{
  FAR struct s6_eth_driver_s *priv = arg;
  FAR const char *ifname = priv->dev.netdev.d_ifname;
  bool link;

  link = (s6_eth_mdio_read(MII_MSR) & MII_MSR_LINKSTATUS) != 0;
  if (link != priv->link)
    {
      priv->link = link;
      if (link)
        {
          uint32_t cfg = S6_ETH_MAC_CONFIG_TE | S6_ETH_MAC_CONFIG_RE;
          uint16_t physr;
          int speed;

          s6_eth_mdio_write(RTL8211F_PAGSR, RTL8211F_PHYSR_PAGE);
          physr = s6_eth_mdio_read(RTL8211F_PHYSR);
          s6_eth_mdio_write(RTL8211F_PAGSR, 0);

          switch (physr & RTL8211F_PHYSR_SPEED)
            {
              case RTL8211F_PHYSR_1000:
                speed = 1000;
                break;

              case RTL8211F_PHYSR_100:
                cfg |= S6_ETH_MAC_CONFIG_PS | S6_ETH_MAC_CONFIG_FES;
                speed = 100;
                break;

              default:
                cfg |= S6_ETH_MAC_CONFIG_PS;
                speed = 10;
                break;
            }

          if ((physr & RTL8211F_PHYSR_DUPLEX) != 0)
            {
              cfg |= S6_ETH_MAC_CONFIG_DM;
            }

          putreg32(cfg, S6_ETH_BASE + S6_ETH_MAC_CONFIG);
          syslog(LOG_INFO, "%s: link up, %d Mbps %s duplex\n", ifname,
                 speed, (physr & RTL8211F_PHYSR_DUPLEX) ? "full" : "half");
          netdev_lower_carrier_on(&priv->dev);
        }
      else
        {
          modifyreg32(S6_ETH_BASE + S6_ETH_MAC_CONFIG,
                      S6_ETH_MAC_CONFIG_TE | S6_ETH_MAC_CONFIG_RE, 0);
          syslog(LOG_INFO, "%s: link down\n", ifname);
          netdev_lower_carrier_off(&priv->dev);
        }
    }

  work_queue(LPWORK, &priv->pollwork, s6_eth_poll_work, priv,
             S6_ETH_POLL_DELAY);
}

/****************************************************************************
 * Name: s6_eth_ifup
 *
 * Description:
 *   Reset the MAC and bring up DMA channel 0, MTL queue 0 and the MAC.
 *   TX/RX in the MAC are turned on by the link poll.
 *
 ****************************************************************************/

static int s6_eth_ifup(FAR struct netdev_lowerhalf_s *dev)
{
  FAR struct s6_eth_driver_s *priv = (FAR struct s6_eth_driver_s *)dev;
  FAR const uint8_t *mac = dev->netdev.d_mac.ether.ether_addr_octet;
  uint32_t intr;
  int i;

  /* The reset needs the RX clock from the PHY to complete */

  putreg32(S6_ETH_DMA_MODE_SWR, S6_ETH_BASE + S6_ETH_DMA_MODE);
  for (i = 0; (getreg32(S6_ETH_BASE + S6_ETH_DMA_MODE) &
               S6_ETH_DMA_MODE_SWR) != 0; i++)
    {
      if (i >= S6_ETH_TIMEOUT_MS)
        {
          nerr("DMA reset timeout, no RX clock from the PHY?\n");
          return -ETIMEDOUT;
        }

      up_mdelay(1);
    }

  /* The MMC counter interrupts are not used */

  putreg32(0xffffffff, S6_ETH_BASE + S6_ETH_MMC_RX_INTR_MASK);
  putreg32(0xffffffff, S6_ETH_BASE + S6_ETH_MMC_TX_INTR_MASK);
  putreg32(0xffffffff, S6_ETH_BASE + S6_ETH_MMC_IPC_RX_INTR_MASK);

  /* Descriptor rings */

  memset(g_txdesc, 0, sizeof(struct s6_eth_desc_s) * S6_ETH_NTXDESC);
  up_invalidate_dcache((uintptr_t)g_rxbuf,
                       (uintptr_t)g_rxbuf + sizeof(g_rxbuf));

  priv->txhead = 0;
  priv->txtail = 0;
  priv->rxhead = 0;

  putreg32(S6_ETH_DMA_CH_PBLX8, S6_ETH_BASE + S6_ETH_DMA_CH0_CONTROL);
  putreg32(S6_ETH_DMA_TX_PBL(8) | S6_ETH_DMA_TX_OSF,
           S6_ETH_BASE + S6_ETH_DMA_CH0_TX_CONTROL);
  putreg32(S6_ETH_DMA_RX_PBL(8) | S6_ETH_DMA_RX_RBSZ(S6_ETH_BUFSIZE),
           S6_ETH_BASE + S6_ETH_DMA_CH0_RX_CONTROL);

  putreg32(S6_ETH_NTXDESC - 1, S6_ETH_BASE + S6_ETH_DMA_CH0_TXDESC_RING);
  putreg32((uint32_t)(uintptr_t)g_txdesc,
           S6_ETH_BASE + S6_ETH_DMA_CH0_TXDESC_LIST);
  putreg32((uint32_t)(uintptr_t)g_txdesc,
           S6_ETH_BASE + S6_ETH_DMA_CH0_TXDESC_TAIL);

  putreg32(S6_ETH_NRXDESC - 1, S6_ETH_BASE + S6_ETH_DMA_CH0_RXDESC_RING);
  putreg32((uint32_t)(uintptr_t)g_rxdesc,
           S6_ETH_BASE + S6_ETH_DMA_CH0_RXDESC_LIST);
  for (i = 0; i < S6_ETH_NRXDESC; i++)
    {
      s6_eth_rxgive(i);
    }

  /* MTL queue 0 in store and forward mode */

  putreg32(S6_ETH_MTL_TXQ_TSF | S6_ETH_MTL_TXQ_TXQEN |
           S6_ETH_MTL_TXQ_TQS(2048 / 256 - 1),
           S6_ETH_BASE + S6_ETH_MTL_TXQ0_OP_MODE);
  putreg32(S6_ETH_MTL_RXQ_RSF | S6_ETH_MTL_RXQ_RQS(4096 / 256 - 1),
           S6_ETH_BASE + S6_ETH_MTL_RXQ0_OP_MODE);

  /* MAC: our address plus broadcast */

  putreg32(S6_ETH_MAC_RXQ0EN_DCB, S6_ETH_BASE + S6_ETH_MAC_RXQ_CTRL0);
  putreg32(S6_ETH_MAC_ADDR_AE | mac[5] << 8 | mac[4],
           S6_ETH_BASE + S6_ETH_MAC_ADDR0_HIGH);
  putreg32((uint32_t)mac[3] << 24 | mac[2] << 16 | mac[1] << 8 | mac[0],
           S6_ETH_BASE + S6_ETH_MAC_ADDR0_LOW);
  putreg32(0, S6_ETH_BASE + S6_ETH_MAC_PACKET_FILTER);

  /* Interrupts and DMA start */

  intr = S6_ETH_DMA_INT_TIE | S6_ETH_DMA_INT_RIE | S6_ETH_DMA_INT_FBE;
  if ((getreg32(S6_ETH_BASE + S6_ETH_MAC_VERSION) & 0xff) >= 0x41)
    {
      intr |= S6_ETH_DMA_INT_NIE_4_10 | S6_ETH_DMA_INT_AIE_4_10;
    }
  else
    {
      intr |= S6_ETH_DMA_INT_NIE | S6_ETH_DMA_INT_AIE;
    }

  putreg32(intr, S6_ETH_BASE + S6_ETH_DMA_CH0_INTR_EN);

  modifyreg32(S6_ETH_BASE + S6_ETH_DMA_CH0_TX_CONTROL, 0, S6_ETH_DMA_TX_ST);
  modifyreg32(S6_ETH_BASE + S6_ETH_DMA_CH0_RX_CONTROL, 0, S6_ETH_DMA_RX_SR);
  up_enable_irq(S6_ETH_IRQ);

  priv->link = false;
  work_queue(LPWORK, &priv->pollwork, s6_eth_poll_work, priv, 0);
  return OK;
}

/****************************************************************************
 * Name: s6_eth_ifdown
 ****************************************************************************/

static int s6_eth_ifdown(FAR struct netdev_lowerhalf_s *dev)
{
  FAR struct s6_eth_driver_s *priv = (FAR struct s6_eth_driver_s *)dev;

  work_cancel_sync(LPWORK, &priv->pollwork);
  up_disable_irq(S6_ETH_IRQ);

  modifyreg32(S6_ETH_BASE + S6_ETH_MAC_CONFIG,
              S6_ETH_MAC_CONFIG_TE | S6_ETH_MAC_CONFIG_RE, 0);
  modifyreg32(S6_ETH_BASE + S6_ETH_DMA_CH0_TX_CONTROL, S6_ETH_DMA_TX_ST, 0);
  modifyreg32(S6_ETH_BASE + S6_ETH_DMA_CH0_RX_CONTROL, S6_ETH_DMA_RX_SR, 0);

  while (priv->txtail != priv->txhead)
    {
      unsigned int i = priv->txtail++ % S6_ETH_NTXDESC;

      netpkt_free(dev, priv->txpkt[i], NETPKT_TX);
      priv->txpkt[i] = NULL;
    }

  priv->link = false;
  netdev_lower_carrier_off(dev);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: s6_eth_initialize
 *
 * Description:
 *   Register the S6 Ethernet MAC as eth0.  The clocks, pins, PHY reset
 *   and RGMII delays are left as U-Boot configured them.
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

int s6_eth_initialize(void)
{
  FAR struct s6_eth_driver_s *priv = &g_s6_eth;
  FAR uint8_t *mac = priv->dev.netdev.d_mac.ether.ether_addr_octet;
  static const uint8_t zero[IFHWADDRLEN];
  static const uint8_t ones[IFHWADDRLEN] =
  {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff
  };

  static const uint8_t fallback[IFHWADDRLEN] =
  {
    0x02, 0x00, 0x00, 0x00, 0x53, 0x06
  };

  uint32_t hi;
  uint32_t lo;
  uint16_t reg;

  /* Keep the address U-Boot used, it is lost on the first DMA reset */

  hi = getreg32(S6_ETH_BASE + S6_ETH_MAC_ADDR0_HIGH);
  lo = getreg32(S6_ETH_BASE + S6_ETH_MAC_ADDR0_LOW);
  mac[0] = lo;
  mac[1] = lo >> 8;
  mac[2] = lo >> 16;
  mac[3] = lo >> 24;
  mac[4] = hi;
  mac[5] = hi >> 8;

  if (memcmp(mac, zero, IFHWADDRLEN) == 0 ||
      memcmp(mac, ones, IFHWADDRLEN) == 0)
    {
      memcpy(mac, fallback, IFHWADDRLEN);
    }

  syslog(LOG_INFO, "s6_eth: DWMAC %08" PRIx32 ", PHY %04x:%04x, "
         "MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
         getreg32(S6_ETH_BASE + S6_ETH_MAC_VERSION),
         s6_eth_mdio_read(MII_PHYID1), s6_eth_mdio_read(MII_PHYID2),
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  /* Stay at 100M for now: the RX delay for 1000M is not set up yet */

  reg = s6_eth_mdio_read(GMII_1000BTCR);
  s6_eth_mdio_write(GMII_1000BTCR, reg & ~(GMII_1000BTCR_1000BASETHALF |
                                           GMII_1000BTCR_1000BASETFULL));
  reg = s6_eth_mdio_read(MII_MCR);
  s6_eth_mdio_write(MII_MCR, reg | MII_MCR_ANENABLE | MII_MCR_ANRESTART);

  /* Clear the status in s6_eth_interrupt() until it reads back zero, which
   * works for both edge and level.  The other S6 peripherals turned out to
   * be edge triggered whatever the device tree says.
   */

  irq_attach(S6_ETH_IRQ, s6_eth_interrupt, priv);
  up_set_irq_type(S6_ETH_IRQ, IRQ_RISING_EDGE);

  /* One TX descriptor always stays empty (see s6_eth_transmit()), and
   * s6_eth_receive() hands each RX packet straight to the upper half.
   */

  priv->dev.ops = &g_s6_eth_ops;
  priv->dev.quota[NETPKT_TX] = S6_ETH_NTXDESC - 1;
  priv->dev.quota[NETPKT_RX] = 1;
  priv->dev.rxtype = NETDEV_RX_WORK;
  priv->dev.priority = LPWORK;

  return netdev_lower_register(&priv->dev, NET_LL_ETHERNET);
}

#endif /* CONFIG_S6_ETH */

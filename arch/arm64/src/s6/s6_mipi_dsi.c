/****************************************************************************
 * arch/arm64/src/s6/s6_mipi_dsi.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/* Amlogic Meson S6 MIPI DSI lower-half.
 *
 * U-Boot initializes the whole display pipeline (panel, DSI, VENC) and
 * leaves OSD1 running.  This driver inherits that pipeline, re-points
 * OSD1 at a NuttX-owned framebuffer and provides the framebuffer lower-half.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <debug.h>
#include <nuttx/arch.h>
#include <nuttx/kmalloc.h>
#include <nuttx/video/fb.h>

#include "arm64_arch.h"
#include "hardware/s6_viu.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define S6_FB_WIDTH   CONFIG_S6_FB_WIDTH
#define S6_FB_HEIGHT  CONFIG_S6_FB_HEIGHT
#define S6_FB_BPP     16
#define S6_FB_FMT     FB_FMT_RGB16_565

#define S6_FB_ALIGN   64

#define S6_FB_STRIDE \
  (((S6_FB_WIDTH * (S6_FB_BPP / 8)) + (S6_FB_ALIGN - 1)) & \
   ~(S6_FB_ALIGN - 1))
#define S6_FB_LEN     (S6_FB_STRIDE * S6_FB_HEIGHT)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct s6_dsi_fb_s
{
  struct fb_vtable_s vtable;  /* vtable is the first member */
  FAR void *fbmem;            /* Allocated framebuffer */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int s6_dsi_getvideoinfo(FAR struct fb_vtable_s *vtable,
                               FAR struct fb_videoinfo_s *vinfo);
static int s6_dsi_getplaneinfo(FAR struct fb_vtable_s *vtable, int planeno,
                               FAR struct fb_planeinfo_s *pinfo);
static int s6_dsi_setpower(FAR struct fb_vtable_s *vtable, int power);
static int s6_dsi_updatearea(FAR struct fb_vtable_s *vtable,
                             FAR const struct fb_area_s *area);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct s6_dsi_fb_s g_s6_dsi_fb =
{
  .vtable =
    {
      .getvideoinfo = s6_dsi_getvideoinfo,
      .getplaneinfo = s6_dsi_getplaneinfo,
      .updatearea   = s6_dsi_updatearea,
      .setpower     = s6_dsi_setpower,
    },
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Point OSD1 at the framebuffer.  Most of the OSD state (layer enabled,
 * linear MIF, RGB565) is inherited from U-Boot; only the windows, the
 * frame address, the stride and the scaler differ.
 */

static void s6_dsi_osd_program(uintptr_t paddr)
{
  putreg32(S6_OSD_WIN_X(S6_FB_WIDTH - 1, 0), S6_VIU_OSD1_BLK0_CFG_W1);
  putreg32(S6_OSD_WIN_Y(S6_FB_HEIGHT - 1, 0), S6_VIU_OSD1_BLK0_CFG_W2);
  putreg32(S6_OSD_DISP_X(S6_FB_WIDTH - 1, 0), S6_VIU_OSD1_BLK0_CFG_W3);
  putreg32(S6_OSD_DISP_Y(S6_FB_HEIGHT - 1, 0), S6_VIU_OSD1_BLK0_CFG_W4);

  putreg32(S6_OSD_MIF_ADDR(paddr), S6_VIU_OSD1_BLK1_CFG_W4);
  modreg32(S6_OSD_MIF_STRIDE(S6_FB_STRIDE), S6_OSD_MIF_STRIDE_MASK,
           S6_VIU_OSD1_BLK2_CFG_W4);

  putreg32(0, S6_VPP_OSD_SC_CTRL0);
}

static int s6_dsi_getvideoinfo(FAR struct fb_vtable_s *vtable,
                               FAR struct fb_videoinfo_s *vinfo)
{
  DEBUGASSERT(vtable != NULL && vinfo != NULL);

  vinfo->fmt     = S6_FB_FMT;
  vinfo->xres    = S6_FB_WIDTH;
  vinfo->yres    = S6_FB_HEIGHT;
  vinfo->nplanes = 1;
  return OK;
}

static int s6_dsi_getplaneinfo(FAR struct fb_vtable_s *vtable, int planeno,
                               FAR struct fb_planeinfo_s *pinfo)
{
  FAR struct s6_dsi_fb_s *priv = (FAR struct s6_dsi_fb_s *)vtable;

  DEBUGASSERT(vtable != NULL && pinfo != NULL);

  if (planeno != 0)
    {
      return -EINVAL;
    }

  pinfo->fbmem        = priv->fbmem;
  pinfo->fblen        = S6_FB_LEN;
  pinfo->stride       = S6_FB_STRIDE;
  pinfo->display      = 0;
  pinfo->bpp          = S6_FB_BPP;
  pinfo->xres_virtual = S6_FB_WIDTH;
  pinfo->yres_virtual = S6_FB_HEIGHT;
  pinfo->xoffset      = 0;
  pinfo->yoffset      = 0;
  return OK;
}

/* FBIOSET_POWER toggles the OSD1 layer; panel power stays with U-Boot. */

static int s6_dsi_setpower(FAR struct fb_vtable_s *vtable, int power)
{
  FAR struct s6_dsi_fb_s *priv = (FAR struct s6_dsi_fb_s *)vtable;

  DEBUGASSERT(vtable != NULL);

  if (priv->fbmem == NULL)
    {
      return -ENODEV;
    }

  modreg32(power > 0 ? S6_OSD_CTRL_EN : 0, S6_OSD_CTRL_EN,
           S6_VIU_OSD1_CTRL_STAT);
  return OK;
}

/* The OSD fetches from DDR without snooping the CPU caches; flush the
 * dirty rows.
 */

static int s6_dsi_updatearea(FAR struct fb_vtable_s *vtable,
                             FAR const struct fb_area_s *area)
{
  FAR struct s6_dsi_fb_s *priv = (FAR struct s6_dsi_fb_s *)vtable;
  uintptr_t start;
  uintptr_t end;
  unsigned int endrow;

  DEBUGASSERT(vtable != NULL && area != NULL);

  if (priv->fbmem == NULL)
    {
      return -ENODEV;
    }

  if (area->w == 0 || area->h == 0 ||
      area->x >= S6_FB_WIDTH || area->y >= S6_FB_HEIGHT)
    {
      return OK;
    }

  endrow = area->y + area->h;
  if (endrow > S6_FB_HEIGHT)
    {
      endrow = S6_FB_HEIGHT;
    }

  start = (uintptr_t)priv->fbmem + area->y * S6_FB_STRIDE;
  end   = (uintptr_t)priv->fbmem + endrow * S6_FB_STRIDE;
  up_clean_dcache(start, end);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int up_fbinitialize(int display)
{
  FAR struct s6_dsi_fb_s *priv = &g_s6_dsi_fb;

  if (display != 0)
    {
      return -ENODEV;
    }

  if (priv->fbmem != NULL)
    {
      return OK;
    }

  priv->fbmem = kmm_memalign(S6_FB_ALIGN, S6_FB_LEN);
  if (priv->fbmem == NULL)
    {
      gerr("ERROR: Failed to allocate %d byte framebuffer\n", S6_FB_LEN);
      return -ENOMEM;
    }

  memset(priv->fbmem, 0, S6_FB_LEN);
  up_clean_dcache((uintptr_t)priv->fbmem,
                  (uintptr_t)priv->fbmem + S6_FB_LEN);

  s6_dsi_osd_program((uintptr_t)priv->fbmem);

  ginfo("fb0: %dx%d RGB565 at %p\n",
        S6_FB_WIDTH, S6_FB_HEIGHT, priv->fbmem);
  return OK;
}

FAR struct fb_vtable_s *up_fbgetvplane(int display, int vplane)
{
  if (display != 0 || vplane != 0)
    {
      return NULL;
    }

  return &g_s6_dsi_fb.vtable;
}

void up_fbuninitialize(int display)
{
  FAR struct s6_dsi_fb_s *priv = &g_s6_dsi_fb;

  if (display != 0 || priv->fbmem == NULL)
    {
      return;
    }

  modreg32(0, S6_OSD_CTRL_EN, S6_VIU_OSD1_CTRL_STAT);

  kmm_free(priv->fbmem);
  priv->fbmem = NULL;
}

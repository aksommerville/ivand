#ifndef DRMFB_INTERNAL_H
#define DRMFB_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "drmfb.h"

// We're scaling in software; don't go too high.
#define DRMFB_SCALE_LIMIT 6

struct drmfb {
  
  int fd;
  
  uint32_t connid,encid,crtcid;
  drmModeCrtcPtr crtc_restore;
  drmModeModeInfo mode;
  
  int fbw,fbh; // input size, established at init (expect 96x64)
  int rate; // initially requested rate, then actual
  int screenw,screenh; // screen size, also size of our framebuffers
  int dstx,dsty,dstw,dsth; // scale target within (screen)
  int scale; // 1..DRMFB_SCALE_LIMIT; fbw*scale=dstw<=screenw
  int stridewords;
  
  struct drmfb_fb {
    uint32_t fbid;
    int handle;
    int size;
    void *v;
  } fbv[2];
  int fbp; // (0,1) which is attached -- draw to the other
};

int drmfb_init_connection(struct drmfb *drmfb);
int drmfb_init_buffers(struct drmfb *drm);
int drmfb_calculate_output_bounds(struct drmfb *drm);
void drmfb_scale(struct drmfb_fb *dst,struct drmfb *drm,const uint16_t *src);

#endif

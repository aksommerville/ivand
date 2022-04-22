#include "drmfb_internal.h"

/* Initialize one framebuffer.
 */
 
static int drmfb_fb_init(struct drmfb *drmfb,struct drmfb_fb *fb) {

  struct drm_mode_create_dumb creq={
    .width=drmfb->screenw,
    .height=drmfb->screenh,
    .bpp=16,
    .flags=0,
  };
  if (ioctl(drmfb->fd,DRM_IOCTL_MODE_CREATE_DUMB,&creq)<0) {
    fprintf(stderr,"DRM_IOCTL_MODE_CREATE_DUMB: %m\n");
    return -1;
  }
  fb->handle=creq.handle;
  fb->size=creq.size;
  drmfb->stridewords=creq.pitch;
  drmfb->stridewords>>=1;
  
  if (drmModeAddFB(
    drmfb->fd,
    creq.width,creq.height,
    16,
    creq.bpp,creq.pitch,
    fb->handle,
    &fb->fbid
  )<0) {
    fprintf(stderr,"drmModeAddFB: %m\n");
    return -1;
  }
  
  struct drm_mode_map_dumb mreq={
    .handle=fb->handle,
  };
  if (ioctl(drmfb->fd,DRM_IOCTL_MODE_MAP_DUMB,&mreq)<0) {
    fprintf(stderr,"DRM_IOCTL_MODE_MAP_DUMB: %m\n");
    return -1;
  }
  
  fb->v=mmap(0,fb->size,PROT_READ|PROT_WRITE,MAP_SHARED,drmfb->fd,mreq.offset);
  if (fb->v==MAP_FAILED) {
    fprintf(stderr,"mmap: %m\n");
    return -1;
  }
  
  return 0;
}

/* Init.
 */
 
int drmfb_init_buffers(struct drmfb *drmfb) {
  
  if (drmfb_fb_init(drmfb,drmfb->fbv+0)<0) return -1;
  if (drmfb_fb_init(drmfb,drmfb->fbv+1)<0) return -1;
  
  if (drmfb_calculate_output_bounds(drmfb)<0) return -1;
  
  if (drmModeSetCrtc(
    drmfb->fd,
    drmfb->crtcid,
    drmfb->fbv[0].fbid,
    0,0,
    &drmfb->connid,
    1,
    &drmfb->mode
  )<0) {
    fprintf(stderr,"drmModeSetCrtc: %m\n");
    return -1;
  }
  
  return 0;
}

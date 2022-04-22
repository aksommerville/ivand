#include "drmfb_internal.h"

/* Delete.
 */
 
static void drmfb_fb_cleanup(struct drmfb *drmfb,struct drmfb_fb *fb) {
  if (fb->fbid) {
    drmModeRmFB(drmfb->fd,fb->fbid);
  }
  if (fb->v) {
    munmap(fb->v,fb->size);
  }
}
 
void drmfb_del(struct drmfb *drmfb) {
  if (!drmfb) return;
  
  drmfb_fb_cleanup(drmfb,drmfb->fbv+0);
  drmfb_fb_cleanup(drmfb,drmfb->fbv+1);

  if (drmfb->crtc_restore) {
    if (drmfb->fd>=0) {
      drmModeCrtcPtr crtc=drmfb->crtc_restore;
      drmModeSetCrtc(
        drmfb->fd,
        crtc->crtc_id,
        crtc->buffer_id,
        crtc->x,
        crtc->y,
        &drmfb->connid,
        1,
        &crtc->mode
      );
    }
    drmModeFreeCrtc(drmfb->crtc_restore);
  }

  if (drmfb->fd>=0) {
    close(drmfb->fd);
    drmfb->fd=-1;
  }
  
  free(drmfb);
}

/* New.
 */
 
struct drmfb *drmfb_new(int w,int h,int rate) {
  if ((w<1)||(h<1)) return 0;
  if (rate<1) rate=60;
  
  struct drmfb *drmfb=calloc(1,sizeof(struct drmfb));
  if (!drmfb) return 0;
  
  drmfb->fbw=w;
  drmfb->fbh=h;
  drmfb->rate=rate;
  
  if (
    (drmfb_init_connection(drmfb)<0)||
    (drmfb_init_buffers(drmfb)<0)||
  0) {
    drmfb_del(drmfb);
    return 0;
  }
  
  return drmfb;
}

/* Swap buffers.
 */
 
void drmfb_swap(struct drmfb *drmfb,const void *src) {
  
  drmfb->fbp^=1;
  struct drmfb_fb *fb=drmfb->fbv+drmfb->fbp;
  
  drmfb_scale(fb,drmfb,src);
  
  while (1) {
    if (drmModePageFlip(drmfb->fd,drmfb->crtcid,fb->fbid,0,drmfb)<0) {
      if (errno==EBUSY) { // waiting for prior flip
        usleep(1000);
        continue;
      }
      fprintf(stderr,"drmModePageFlip: %m\n");
      return;
    } else {
      break;
    }
  }
}

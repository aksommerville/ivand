/* drmfb.h
 * Minimal interface to Linux DRM for software rendering only.
 */
 
#ifndef DRMFB_H
#define DRMFB_H

struct drmfb;

void drmfb_del(struct drmfb *drmfb);

/* You will get exactly the dimensions you ask for.
 * We scale or crop as needed.
 * (rate) is a hint only, we might pick something different.
 */
struct drmfb *drmfb_new(
  int w,int h,int rate
);

/* (fb) must be the dimensions you declared at new.
 * Big-endian BGR565 (what the Tiny uses).
 */
void drmfb_swap(struct drmfb *drmfb,const void *fb);

#endif

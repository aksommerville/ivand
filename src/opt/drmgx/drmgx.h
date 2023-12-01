/* drmgx.h
 * Minimal interface to Linux Direct Rendering Manager, with an OpenGL context.
 * OpenGL is only in play under the hood; client only provides a raw framebuffer.
 */
 
#ifndef DRMGX_H
#define DRMGX_H

#define DRMGX_FMT_Y8 3
#define DRMGX_FMT_TINY8 8
#define DRMGX_FMT_TINY16 16
#define DRMGX_FMT_RGB 24
#define DRMGX_FMT_RGBX 32

struct drmgx;

void drmgx_del(struct drmgx *drmgx);

/* (device) null, we default to "/dev/dri/card0".
 * (rate) in Hz, a suggestion only.
 * (w,h) are your framebuffer size in pixels. We'll look for an amenable monitor size.
 * (fmt) is the format of the framebuffers you will provide, see above.
 */
struct drmgx *drmgx_new(const char *device,int rate,int w,int h,int fmt,int filter,int glslversion);

int drmgx_swap(struct drmgx *drmgx,const void *fb);

#endif

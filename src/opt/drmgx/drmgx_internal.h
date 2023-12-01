#ifndef DRMGX_INTERNAL_H
#define DRMGX_INTERNAL_H

#include "drmgx.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>

struct drmgx_fb {
  uint32_t fbid;
  int handle;
  int size;
};

struct drmgx {
  int fd;

  // From init, never modified:
  char *reqdevice;
  int reqrate;
  int fbw,fbh,fbfmt;
  int reqfilter;
  int glslversion;
  
  int mmw,mmh; // monitor's physical size
  int w,h; // monitor's logical size in pixels
  int rate; // monitor's refresh rate in hertz
  drmModeModeInfo mode; // ...and more in that line
  int connid,encid,crtcid;
  drmModeCrtcPtr crtc_restore;
  
  int flip_pending;
  struct drmgx_fb fbv[2];
  int fbp;
  struct gbm_bo *bo_pending;
  int crtcunset;
  
  struct gbm_device *gbmdevice;
  struct gbm_surface *gbmsurface;
  EGLDisplay egldisplay;
  EGLContext eglcontext;
  EGLSurface eglsurface;

  GLuint texid;
  GLuint programid;
  void *fbcvt;
  GLfloat dstr_left,dstr_top,dstr_right,dstr_bottom;
};

int drmgx_open_file(struct drmgx *drmgx);
int drmgx_configure(struct drmgx *drmgx);
int drmgx_init_gx(struct drmgx *drmgx);

void drmgx_render_quit(struct drmgx *drmgx);
int drmgx_render_init(struct drmgx *drmgx);
void drmgx_render(struct drmgx *drmgx,const void *fb);

#endif

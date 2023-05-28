#ifndef BCM_INTERNAL_H
#define BCM_INTERNAL_H

#include "bcm.h"
#include <bcm_host.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>

/* Context.
 */

struct bcm {
// Populated at init:
  DISPMANX_DISPLAY_HANDLE_T vcdisplay;
  DISPMANX_ELEMENT_HANDLE_T vcelement;
  DISPMANX_UPDATE_HANDLE_T vcupdate;
  EGL_DISPMANX_WINDOW_T eglwindow;
  EGLDisplay egldisplay;
  EGLSurface eglsurface;
  EGLContext eglcontext;
  EGLConfig eglconfig;
  int screenw,screenh;
  int fbw,fbh;
// Populated by renderer:
  uint8_t *swapbuf;
  GLuint programid;
  GLuint texid;
  GLfloat dstl,dstr,dstt,dstb;
};

int bcm_render_init(struct bcm *bcm);

#endif

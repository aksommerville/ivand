#include "bcm_internal.h"

/* Init.
 */

static int bcm_init(struct bcm *bcm) {
  bcm_host_init();

  // We enforce a screen size sanity limit of 4096. Could be as high as 32767 if we felt like it.
  int screenw,screenh;
  graphics_get_display_size(0,&screenw,&screenh);
  if ((screenw<1)||(screenh<1)) return -1;
  if ((screenw>4096)||(screenh>4096)) return -1;

  if (!(bcm->vcdisplay=vc_dispmanx_display_open(0))) return -1;
  if (!(bcm->vcupdate=vc_dispmanx_update_start(0))) return -1;

  int logw=screenw-80;
  int logh=screenh-50;
  VC_RECT_T srcr={0,0,screenw<<16,screenh<<16};
  VC_RECT_T dstr={(screenw>>1)-(logw>>1),(screenh>>1)-(logh>>1),logw,logh};
  VC_DISPMANX_ALPHA_T alpha={DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,0xffffffff};
  if (!(bcm->vcelement=vc_dispmanx_element_add(
    bcm->vcupdate,bcm->vcdisplay,1,&dstr,0,&srcr,DISPMANX_PROTECTION_NONE,&alpha,0,0
  ))) return -1;

  bcm->eglwindow.element=bcm->vcelement;
  bcm->eglwindow.width=screenw;
  bcm->eglwindow.height=screenh;

  if (vc_dispmanx_update_submit_sync(bcm->vcupdate)<0) return -1;

  static const EGLint eglattr[]={
    EGL_RED_SIZE,8,
    EGL_GREEN_SIZE,8,
    EGL_BLUE_SIZE,8,
    EGL_ALPHA_SIZE,0,
    EGL_DEPTH_SIZE,0,
    EGL_LUMINANCE_SIZE,EGL_DONT_CARE,
    EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
    EGL_SAMPLES,1,
  EGL_NONE};
  static EGLint ctxattr[]={
    EGL_CONTEXT_CLIENT_VERSION,2,
  EGL_NONE};

  bcm->egldisplay=eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglInitialize(bcm->egldisplay,0,0);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglBindAPI(EGL_OPENGL_ES_API);

  EGLint configc=0;
  eglChooseConfig(bcm->egldisplay,eglattr,&bcm->eglconfig,1,&configc);
  if (eglGetError()!=EGL_SUCCESS) return -1;
  if (configc<1) return -1;

  bcm->eglsurface=eglCreateWindowSurface(bcm->egldisplay,bcm->eglconfig,&bcm->eglwindow,0);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  bcm->eglcontext=eglCreateContext(bcm->egldisplay,bcm->eglconfig,0,ctxattr);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglMakeCurrent(bcm->egldisplay,bcm->eglsurface,bcm->eglsurface,bcm->eglcontext);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglSwapInterval(bcm->egldisplay,1);

  bcm->screenw=screenw;
  bcm->screenh=screenh;

  return 0;
}

/* New.
 */

struct bcm *bcm_new(int fbw,int fbh) {
  struct bcm *bcm=calloc(1,sizeof(struct bcm));
  if (!bcm) return 0;
  bcm->fbw=fbw;
  bcm->fbh=fbh;
  if ((bcm_init(bcm)<0)||(bcm_render_init(bcm)<0)) {
    bcm_del(bcm);
    return 0;
  }
  return bcm;
}

/* Quit.
 */

void bcm_del(struct bcm *bcm) {
  if (!bcm) return;
  eglMakeCurrent(bcm->egldisplay,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
  eglDestroySurface(bcm->egldisplay,bcm->eglsurface);
  eglTerminate(bcm->egldisplay);
  eglReleaseThread();
  bcm_host_deinit();
  free(bcm);
}

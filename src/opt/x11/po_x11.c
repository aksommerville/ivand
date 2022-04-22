#include "po_x11.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

typedef uint16_t po_pixel_t;

#define KeyRepeat (LASTEvent+2)
#define PO_X11_KEY_REPEAT_INTERVAL 10

#define PO_X11_SCALE_LIMIT 16

/* Type definition. 
 */
 
struct po_x11 {
  int fbw,fbh; // 96,64 but we're flexible
  int winw,winh; // total output (client) area
  int fullscreen;
  
  void *userdata;
  int (*cb_button)(struct po_x11 *x11,uint8_t btnid,int value);
  int (*cb_close)(struct po_x11 *x11);
  
  Display *dpy;
  int screen;
  Window win;
  GC gc;
  
  // If (dstdirty), we need to recalculate image size and position.
  // That could mean destroying and rebuilding the image object.
  // This image is sized to the logical output: fb<=image<=win
  XImage *image;
  int dstx,dsty;
  int dstdirty;
  int rshift,gshift,bshift;
  int scale;
  
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom__NET_WM_STATE;
  Atom atom__NET_WM_STATE_FULLSCREEN;
  Atom atom__NET_WM_STATE_ADD;
  Atom atom__NET_WM_STATE_REMOVE;
  Atom atom__NET_WM_ICON;
  
  int cursor_visible;
  int screensaver_inhibited;
  int focus;
};

/* Cleanup.
 */
  
void po_x11_del(struct po_x11 *x11) {
  if (!x11) return;
  
  if (x11->dpy) {
    if (x11->image) XDestroyImage(x11->image);
    if (x11->gc) XFreeGC(x11->dpy,x11->gc);
    XCloseDisplay(x11->dpy);
  }
  
  free(x11);
}

/* Init.
 */
 
static int po_x11_init(struct po_x11 *x11) {
  
  if (!(x11->dpy=XOpenDisplay(0))) return -1;
  x11->screen=DefaultScreen(x11->dpy);

  #define GETATOM(tag) x11->atom_##tag=XInternAtom(x11->dpy,#tag,0);
  GETATOM(WM_PROTOCOLS)
  GETATOM(WM_DELETE_WINDOW)
  GETATOM(_NET_WM_STATE)
  GETATOM(_NET_WM_STATE_FULLSCREEN)
  GETATOM(_NET_WM_STATE_ADD)
  GETATOM(_NET_WM_STATE_REMOVE)
  GETATOM(_NET_WM_ICON)
  #undef GETATOM
  
  XSetWindowAttributes wattr={
    .background_pixel=0,
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      FocusChangeMask|
    0,
  };
  
  //int w=WidthOfScreen(ScreenOfDisplay(x11->dpy,0));
  //int h=HeightOfScreen(ScreenOfDisplay(x11->dpy,0));
  int scale=8;
  x11->winw=x11->fbw*scale;
  x11->winh=x11->fbh*scale;
  
  if (!(x11->win=XCreateWindow(
    x11->dpy,RootWindow(x11->dpy,x11->screen),
    0,0,x11->winw,x11->winh,0,
    DefaultDepth(x11->dpy,x11->screen),InputOutput,CopyFromParent,
    CWBackPixel|CWBorderPixel|CWColormap|CWEventMask,&wattr
  ))) return -1;
  
  if (x11->fullscreen) {
    XChangeProperty(
      x11->dpy,x11->win,
      x11->atom__NET_WM_STATE,
      XA_ATOM,32,PropModeReplace,
      (unsigned char*)&x11->atom__NET_WM_STATE_FULLSCREEN,1
    );
    x11->fullscreen=1;
  }
  
  XMapWindow(x11->dpy,x11->win);
  
  XSync(x11->dpy,0);
  
  XSetWMProtocols(x11->dpy,x11->win,&x11->atom_WM_DELETE_WINDOW,1);
  
  // Hide cursor.
  XColor color;
  Pixmap pixmap=XCreateBitmapFromData(x11->dpy,x11->win,"\0\0\0\0\0\0\0\0",1,1);
  Cursor cursor=XCreatePixmapCursor(x11->dpy,pixmap,pixmap,&color,&color,0,0);
  XDefineCursor(x11->dpy,x11->win,cursor);
  XFreeCursor(x11->dpy,cursor);
  XFreePixmap(x11->dpy,pixmap);
  
  if (!(x11->gc=XCreateGC(x11->dpy,x11->win,0,0))) return -1;
  
  return 0;
}

/* New.
 */
 
struct po_x11 *po_x11_new(
  const char *title,
  int fbw,int fbh,
  int fullscreen,
  int (*cb_button)(struct po_x11 *x11,uint8_t btnid,int value),
  int (*cb_close)(struct po_x11 *x11),
  void *userdata
) {
  if ((fbw<1)||(fbh<1)) return 0;
  struct po_x11 *x11=calloc(1,sizeof(struct po_x11));
  if (!x11) return 0;
  
  x11->fbw=fbw;
  x11->fbh=fbh;
  x11->fullscreen=fullscreen;
  x11->cb_button=cb_button;
  x11->cb_close=cb_close;
  x11->userdata=userdata;
  
  x11->cursor_visible=1;
  x11->focus=1;
  x11->dstdirty=1;
  
  if (po_x11_init(x11)<0) {
    po_x11_del(x11);
    return 0;
  }
  
  if (title&&title[0]) {
    XStoreName(x11->dpy,x11->win,title);
  }
  
  return x11;
}

/* Trivial accessors.
 */
 
void *po_x11_get_userdata(const struct po_x11 *x11) {
  return x11->userdata;
}

/* Select framebuffer's output bounds.
 */
 
static int po_x11_recalculate_output_bounds(struct po_x11 *x11) {

  /* First decide the scale factor:
   *  - At least 1.
   *  - At most PO_X11_SCALE_LIMIT.
   *  - Or lesser of winw/fbw,winh/fbh.
   */
  int scalex=x11->winw/x11->fbw;
  int scaley=x11->winh/x11->fbh;
  x11->scale=(scalex<scaley)?scalex:scaley;
  if (x11->scale<1) x11->scale=1;
  else if (x11->scale>PO_X11_SCALE_LIMIT) x11->scale=PO_X11_SCALE_LIMIT;
  
  /* From there, size and position are trivial:
   */
  int dstw=x11->fbw*x11->scale;
  int dsth=x11->fbh*x11->scale;
  x11->dstx=(x11->winw>>1)-(dstw>>1);
  x11->dsty=(x11->winh>>1)-(dsth>>1);
  
  /* If the image is not yet created, or doesn't match the calculated size, rebuild it.
   */
  if (!x11->image||(x11->image->width!=dstw)||(x11->image->height!=dsth)) {
    if (x11->image) {
      XDestroyImage(x11->image);
      x11->image=0;
    }
    void *pixels=malloc(dstw*4*dsth);
    if (!pixels) return -1;
    if (!(x11->image=XCreateImage(
      x11->dpy,DefaultVisual(x11->dpy,x11->screen),24,ZPixmap,0,pixels,dstw,dsth,32,dstw*4
    ))) {
      free(pixels);
      return -1;
    }
    
    // And recalculate channel shifts...
    if (!x11->image->red_mask||!x11->image->green_mask||!x11->image->blue_mask) return -1;
    uint32_t m;
    x11->rshift=0; m=x11->image->red_mask;   for (;!(m&1);m>>=1,x11->rshift++) ; if (m!=0xff) return -1;
    x11->gshift=0; m=x11->image->green_mask; for (;!(m&1);m>>=1,x11->gshift++) ; if (m!=0xff) return -1;
    x11->bshift=0; m=x11->image->blue_mask;  for (;!(m&1);m>>=1,x11->bshift++) ; if (m!=0xff) return -1;
  }
  
  return 0;
}

/* Swap framebuffer.
 */
 
static inline void po_rgb_from_pixel(uint8_t *rgb,uint16_t src) {
  rgb[0]=(src>>5)&0xf8; rgb[0]|=rgb[0]>>5;
  rgb[1]=((src<<5)&0xe0)|((src>>11)&0x1c); rgb[1]|=rgb[1]>>6;
  rgb[2]=(src&0xf8); rgb[2]|=rgb[2]>>5;
}

int po_x11_swap(struct po_x11 *x11,const void *fb) {
  if (x11->dstdirty) {
    if (po_x11_recalculate_output_bounds(x11)<0) return -1;
    x11->dstdirty=0;
    XClearWindow(x11->dpy,x11->win);
  }
  
  const po_pixel_t *src=fb;
  uint32_t *dst=(void*)x11->image->data;
  int cpc=x11->image->width*4;
  int yi=x11->fbh;
  for (;yi-->0;) {
    uint32_t *dststart=dst;
    int xi=x11->fbw;
    for (;xi-->0;src++) {
      uint8_t rgb[3];
      po_rgb_from_pixel(rgb,*src);
      uint32_t pixel=(rgb[0]<<x11->rshift)|(rgb[1]<<x11->gshift)|(rgb[2]<<x11->bshift);
      int ri=x11->scale;
      for (;ri-->0;dst++) *dst=pixel;
    }
    int ri=x11->scale-1;
    for (;ri-->0;dst+=x11->image->width) memcpy(dst,dststart,cpc);
  }
  
  XPutImage(x11->dpy,x11->win,x11->gc,x11->image,0,0,x11->dstx,x11->dsty,x11->image->width,x11->image->height);
  
  x11->screensaver_inhibited=0;
  return 0;
}

/* Toggle fullscreen.
 */

static void po_x11_enter_fullscreen(struct po_x11 *x11) {
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=x11->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=x11->win,
      .data={.l={
        1,
        x11->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(x11->dpy,RootWindow(x11->dpy,x11->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(x11->dpy);
  x11->fullscreen=1;
}

static void po_x11_exit_fullscreen(struct po_x11 *x11) {
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=x11->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=x11->win,
      .data={.l={
        0,
        x11->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(x11->dpy,RootWindow(x11->dpy,x11->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(x11->dpy);
  x11->fullscreen=0;
}
 
int po_x11_set_fullscreen(struct po_x11 *x11,int state) {
  if (state>0) {
    if (x11->fullscreen) return 1;
    po_x11_enter_fullscreen(x11);
  } else if (state==0) {
    if (!x11->fullscreen) return 0;
    po_x11_exit_fullscreen(x11);
  }
  return x11->fullscreen;
}

/* Inhibit screensaver.
 */
 
void po_x11_inhibit_screensaver(struct po_x11 *x11) {
  if (!x11) return;
  if (x11->screensaver_inhibited) return;
  XForceScreenSaver(x11->dpy,ScreenSaverReset);
  x11->screensaver_inhibited=1;
}

/* Process one event.
 */
 
static int po_x11_receive_event(struct po_x11 *x11,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: 
    case KeyRelease: {
        KeySym keysym=XkbKeycodeToKeysym(x11->dpy,evt->xkey.keycode,0,0);
        uint16_t btnid=0;
        switch (keysym) {
          case XK_Down: btnid=BUTTON_DOWN; break;
          case XK_Up: btnid=BUTTON_UP; break;
          case XK_Left: btnid=BUTTON_LEFT; break;
          case XK_Right: btnid=BUTTON_RIGHT; break;
          case XK_z: btnid=BUTTON_A; break;
          case XK_x: btnid=BUTTON_B; break;
          case XK_Escape: if (x11->cb_close) return x11->cb_close(x11); return 0;
          case XK_F11: if (evt->type==KeyPress) return po_x11_set_fullscreen(x11,x11->fullscreen?0:1); return 0;
        }
        if (btnid&&x11->cb_button) {
          if (evt->type==KeyPress) return x11->cb_button(x11,btnid,1);
          return x11->cb_button(x11,btnid,0);
        }
      } break;
    
    case ClientMessage: {
        if (evt->xclient.message_type==x11->atom_WM_PROTOCOLS) {
          if (evt->xclient.format==32) {
            if (evt->xclient.data.l[0]==x11->atom_WM_DELETE_WINDOW) {
              if (x11->cb_close) return x11->cb_close(x11);
            }
          }
        }
      } break;
    
    case ConfigureNotify: {
        int nw=evt->xconfigure.width,nh=evt->xconfigure.height;
        if ((nw!=x11->winw)||(nh!=x11->winh)) {
          x11->winw=nw;
          x11->winh=nh;
          x11->dstdirty=1;
        }
      } break;
    
  }
  return 0;
}

/* Update.
 */
 
int po_x11_update(struct po_x11 *x11) {
  int evtc=XEventsQueued(x11->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(x11->dpy,&evt);
    
    /* If we detect an auto-repeated key, drop one of the events, and turn the other into KeyRepeat.
     * This is a hack to force single events for key repeat.
     */
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(x11->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-PO_X11_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (po_x11_receive_event(x11,&evt)<0) return -1;
      } else {
        if (po_x11_receive_event(x11,&evt)<0) return -1;
        if (po_x11_receive_event(x11,&next)<0) return -1;
      }
    } else {
      if (po_x11_receive_event(x11,&evt)<0) return -1;
    }
  }
  return 1;
}

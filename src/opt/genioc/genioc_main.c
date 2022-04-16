#include "genioc_internal.h"
#include <signal.h>

struct genioc genioc={0};

/* Quit drivers.
 */
 
static void genioc_quit_drivers() {
  #if PO_USE_x11
    po_x11_del(genioc.x11);
  #endif
  #if PO_USE_alsa
    alsa_del(genioc.alsa);
  #endif
  #if PO_USE_evdev
    po_evdev_del(genioc.evdev);
  #endif
}

/* Signal handler.
 */
 
static void genioc_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(genioc.sigc)>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Driver callbacks.
 */
 
#if PO_USE_x11
 
static int genioc_cb_x11_button(struct po_x11 *x11,uint8_t btnid,int value) {
  if (value) genioc.inputstate|=btnid;
  else genioc.inputstate&=~btnid;
  return 0;
}

static int genioc_cb_x11_close(struct po_x11 *x11) {
  genioc.terminate=1;
  return 0;
}

#endif

static void genioc_cb_pcm(int16_t *v,int c,int chanc) {
  if (chanc<1) {
    memset(v,0,c<<1);
    return;
  }
  switch (chanc) {
    case 1: {
        for (;c-->0;v++) *v=audio_next();
      } break;
    case 2: {
        int framec=c>>1;
        for (;framec-->1;v+=2) v[0]=v[1]=audio_next();
      } break;
    default: {
        int framec=c/chanc;
        for (;framec-->1;) {
          int16_t sample=audio_next();
          int i=chanc;
          for (;i-->0;v++) *v=sample;
        }
      }
  }
}

#if PO_USE_alsa

static int genioc_cb_alsa(int16_t *v,int c,struct alsa *alsa) {
  genioc_cb_pcm(v,c,alsa_get_chanc(alsa));
  return 0;
}

#endif

#if PO_USE_evdev

static int genioc_cb_evdev(struct po_evdev *evdev,uint8_t btnid,int value) {
  if (btnid<BUTTON_NONSTANDARD) {
    if (value) genioc.inputstate|=btnid;
    else genioc.inputstate&=~btnid;
  } else switch (btnid) {
    case BUTTON_QUIT: genioc.terminate=1; break;
  }
  #if PO_USE_x11
    po_x11_inhibit_screensaver(genioc.x11);
  #endif
  return 0;
}

#endif

/* Init drivers.
 */
 
static int genioc_init_drivers() {

  signal(SIGINT,genioc_rcvsig);

  #if PO_USE_x11
    if (!(genioc.x11=po_x11_new(
      "Ivan Denisovich",
      96,64,0,
      genioc_cb_x11_button,
      genioc_cb_x11_close,
      &genioc
    ))) {
      fprintf(stderr,"Failed to initialize X11\n");
      return -1;
    }
  #endif
  
  #if PO_USE_alsa
    struct alsa_delegate alsa_delegate={
      .rate=22050,
      .chanc=1,
      .device=0,
      .cb_pcm_out=genioc_cb_alsa,
    };
    if (!(genioc.alsa=alsa_new(&alsa_delegate))) {
      fprintf(stderr,"Failed to initialize ALSA\n");
      return -1;
    }
  #endif
  
  #if PO_USE_evdev
    if (!(genioc.evdev=po_evdev_new(genioc_cb_evdev,&genioc))) {
      fprintf(stderr,"Failed to initialize evdev. Proceeding without joystick support.\n");
    }
  #endif
  
  return 0;
}

/* Init per client (noop).
 */
 
uint8_t platform_init() {
  return 1;
}

/* Update.
 */
 
uint8_t platform_update() {
  #if PO_USE_x11
    po_x11_update(genioc.x11);
  #endif
  #if PO_USE_evdev
    po_evdev_update(genioc.evdev);
  #endif
  return genioc.inputstate;
}

/* Receive framebuffer.
 */
 
void platform_send_framebuffer(const void *fb) {
  #if PO_USE_x11
    po_x11_swap(genioc.x11,fb);
  #endif
}

/* USB stub.
 */
 
void usb_begin() {
  fprintf(stderr,"STUB:%s\n",__func__);
}

void usb_send(const void *v,int c) {

  const char *src=v;
  int srcc=c;
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  if (srcc>50) srcc=0;
  int i=srcc; while (i-->0) {
    if ((src[i]<0x20)||(src[i]>0x7e)) {
      srcc=0;
      break;
    }
  }
  
  fprintf(stderr,"STUB:%s c=%d %.*s\n",__func__,c,srcc,src);
}

int usb_read(void *dst,int dsta) {
  fprintf(stderr,"STUB:%s\n",__func__);
  return -1;
}

int usb_read_byte() {
  fprintf(stderr,"STUB:%s\n",__func__);
  return -1;
}

/* Main.
 */

int main(int argc,char **argv) {
  if (genioc_init_drivers()<0) {
    fprintf(stderr,"Failed to initialize drivers.\n");
    return 1;
  }
  
  setup();
  
  int framec=0;
  const int64_t frametime=1000000/60;
  int64_t nexttime=now_us();
  int64_t starttime=nexttime;
  while (!genioc.terminate&&!genioc.sigc) {
    
    int64_t now=now_us();
    while (now<nexttime) {
      int64_t sleeptime=nexttime-now;
      if (sleeptime>100000) {
        nexttime=now;
        break;
      }
      if (sleeptime>0) sleep_us(sleeptime);
      now=now_us();
    }
    nexttime+=frametime;
    framec++;
    
    #if PO_USE_alsa
      if (alsa_lock(genioc.alsa)<0) {
        fprintf(stderr,"ERROR! Failed to acquire ALSA lock.\n");
        return 1;
      }
    #endif
    loop();
    #if PO_USE_alsa
      alsa_unlock(genioc.alsa);
    #endif
  }
  
  if (framec>0) {
    double elapsed=(nexttime-starttime)/1000000.0;
    fprintf(stderr,"%d video frames in %.03fs, average %.03f Hz\n",framec,elapsed,framec/elapsed);
  }
  
  genioc_quit_drivers();
  fprintf(stderr,"Normal exit.\n");
  return 0;
}

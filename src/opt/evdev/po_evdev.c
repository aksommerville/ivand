#include "po_evdev.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/poll.h>
#include <sys/inotify.h>
#include <linux/input.h>
#include "main/platform.h"

// Must end with slash.
#define PO_EVDEV_DIR "/dev/input/"

#define PO_EVDEV_AXIS_MODE_BUTTON 1 /* >=hi ON, <hi OFF */
#define PO_EVDEV_AXIS_MODE_TWOWAY 2 /* <=lo left/up, >=hi right/down */
#define PO_EVDEV_AXIS_MODE_HAT    3 /* (v-lo) = (0,1,2,3,4,5,6,7) = (N,NE,E,SE,S,SW,W,NW)... why do these exist */

#define PO_EVDEV_AXIS_HORZ 1
#define PO_EVDEV_AXIS_VERT 2

/* Instance definition.
 */
 
struct po_evdev {
  int (*cb_button)(struct po_evdev *evdev,uint8_t btnid,int value);
  void *userdata;
  int infd;
  int rescan;
  struct po_evdev_device {
    int fd;
    int evno;
    uint16_t vendor,product;
    uint16_t state;
    struct po_evdev_axis {
      uint16_t code;
      int mode;
      int lo,hi;
      int axis;
      int v;
    } *axisv;
    int axisc,axisa;
  } *devicev;
  int devicec,devicea;
  struct pollfd *pollfdv;
  int pollfdc,pollfda;
};

/* Cleanup.
 */
 
static void po_evdev_device_cleanup(struct po_evdev_device *device) {
  if (device->fd>=0) close(device->fd);
  if (device->axisv) free(device->axisv);
}
 
void po_evdev_del(struct po_evdev *evdev) {
  if (!evdev) return;
  
  if (evdev->infd>=0) close(evdev->infd);
  if (evdev->pollfdv) free(evdev->pollfdv);
  
  if (evdev->devicev) {
    while (evdev->devicec-->0) po_evdev_device_cleanup(evdev->devicev+evdev->devicec);
    free(evdev->devicev);
  }
  
  free(evdev);
}

/* Initialize inotify.
 */
 
static int po_evdev_init_inotify(struct po_evdev *evdev) {
  if ((evdev->infd=inotify_init())<0) return -1;
  if (inotify_add_watch(evdev->infd,PO_EVDEV_DIR,IN_CREATE|IN_ATTRIB)<0) {
    close(evdev->infd);
    evdev->infd=-1;
    return -1;
  }
  return 0;
}

/* New.
 */
 
struct po_evdev *po_evdev_new(
  int (*cb_button)(struct po_evdev *evdev,uint8_t btnid,int value),
  void *userdata
) {
  struct po_evdev *evdev=calloc(1,sizeof(struct po_evdev));
  if (!evdev) return 0;
  
  evdev->cb_button=cb_button;
  evdev->userdata=userdata;
  
  if (po_evdev_init_inotify(evdev)<0) {
    fprintf(stderr,"%s: Failed to initialize inotify. Joystick connections will not be detected.\n",PO_EVDEV_DIR);
  }
  evdev->rescan=1;
  
  return evdev;
}

/* Trivial accessors.
 */
 
void *po_evdev_get_userdata(const struct po_evdev *evdev) {
  return evdev->userdata;
}

/* Find connected device.
 */
 
static struct po_evdev_device *po_evdev_get_device_by_evno(const struct po_evdev *evdev,int evno) {
  int i=evdev->devicec;
  struct po_evdev_device *device=evdev->devicev;
  for (;i-->0;device++) if (device->evno==evno) return device;
  return 0;
}
 
static struct po_evdev_device *po_evdev_get_device_by_fd(const struct po_evdev *evdev,int fd) {
  int i=evdev->devicec;
  struct po_evdev_device *device=evdev->devicev;
  for (;i-->0;device++) if (device->fd==fd) return device;
  return 0;
}

/* Add device.
 */
 
static struct po_evdev_device *po_evdev_add_device(struct po_evdev *evdev,int evno,int fd) {
  if (fd<0) return 0;
  
  if (evdev->devicec>=evdev->devicea) {
    int na=evdev->devicea+8;
    if (na>INT_MAX/sizeof(struct po_evdev_device)) return 0;
    void *nv=realloc(evdev->devicev,sizeof(struct po_evdev_device)*na);
    if (!nv) return 0;
    evdev->devicev=nv;
    evdev->devicea=na;
  }
  
  struct po_evdev_device *device=evdev->devicev+evdev->devicec++;
  memset(device,0,sizeof(struct po_evdev_device));
  device->evno=evno;
  device->fd=fd;
  return device;
}

/* Add axis to device, or ignore it.
 */
 
static int po_evdev_device_add_axis(
  struct po_evdev *evdev,
  struct po_evdev_device *device,
  uint16_t code,
  struct input_absinfo *absinfo
) {
  
  if (device->axisc>=device->axisa) {
    int na=device->axisa+8;
    if (na>INT_MAX/sizeof(struct po_evdev_axis)) return -1;
    void *nv=realloc(device->axisv,sizeof(struct po_evdev_axis)*na);
    if (!nv) return -1;
    device->axisv=nv;
    device->axisa=na;
  }
  struct po_evdev_axis *axis=device->axisv+device->axisc;
  
  //fprintf(stderr,"  ABS %04x %d..%d =%d\n",code,absinfo->minimum,absinfo->maximum,absinfo->value);
  
  // 8BitDo Tech Co., Ltd. 8BitDo Zero 2 gamepad: D-pad is two 0..255 axes, but their initial value is always zero.
  if ((device->vendor==0x2dc8)&&(device->product==0x9018)) {
    switch (code) {
      case 0: absinfo->value=128; break;
      case 1: absinfo->value=128; break;
    }
  }
  
  // 8Bitdo  8BitDo M30 gamepad (Genesis like), same problem.
  if ((device->vendor==0x2dc8)&&(device->product==0x5006)) {
    switch (code) {
      case 0: absinfo->value=128; break;
      case 1: absinfo->value=128; break;
    }
  }
  
  // Resting value between limits exclusive? Call it two-way.
  if ((absinfo->value>absinfo->minimum)&&(absinfo->value<absinfo->maximum)) {
    memset(axis,0,sizeof(struct po_evdev_axis));
    axis->code=code;
    axis->mode=PO_EVDEV_AXIS_MODE_TWOWAY;
    int mid=(absinfo->minimum+absinfo->maximum)>>1;
    axis->lo=(absinfo->minimum+mid)>>1;
    axis->hi=(absinfo->maximum+mid)>>1;
    if (axis->lo>=mid) axis->lo=mid-1;
    if (axis->hi<=mid) axis->hi=mid+1;
    // Mostly Linux's X axes have even codes... pick off the exceptions.
    switch (code) {
      case ABS_RX: axis->axis=PO_EVDEV_AXIS_HORZ; break;
      case ABS_RY: axis->axis=PO_EVDEV_AXIS_VERT; break;
      default: if (code&1) axis->axis=PO_EVDEV_AXIS_VERT; else axis->axis=PO_EVDEV_AXIS_HORZ;
    }
    device->axisc++;
    return 0;
  }
  
  // At least two values, and resting at the very bottom? Call it button.
  if ((absinfo->maximum>=absinfo->minimum+1)&&(absinfo->value==absinfo->minimum)) {
    memset(axis,0,sizeof(struct po_evdev_axis));
    axis->code=code;
    axis->mode=PO_EVDEV_AXIS_MODE_BUTTON;
    axis->hi=(absinfo->minimum+absinfo->maximum)>>1;
    axis->axis=(code&1)?BUTTON_A:BUTTON_B;
    device->axisc++;
    return 0;
  }
  
  // Exactly 8 values? Call it hat.
  if (absinfo->maximum==absinfo->minimum+7) {
    memset(axis,0,sizeof(struct po_evdev_axis));
    axis->code=code;
    axis->mode=PO_EVDEV_AXIS_MODE_HAT;
    axis->lo=absinfo->minimum;
    axis->v=-1;
    device->axisc++;
    return 0;
  }
  
  // Something else, ignore it.
  return 0;
}

/* Try to connect to one file, no harm if we can't open it or whatever.
 */
 
static int po_evdev_try_file(struct po_evdev *evdev,const char *base,int basec) {
  if (basec<0) { basec=0; while (base[basec]) basec++; }
  
  if ((basec<6)||memcmp(base,"event",5)) return 0;
  int evno=0,p=5;
  for (;p<basec;p++) {
    int digit=base[p]-'0';
    if ((digit<0)||(digit>9)) return 0;
    evno*=10;
    evno+=digit;
  }
  if (po_evdev_get_device_by_evno(evdev,evno)) return 0;
  
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%.*s",(int)sizeof(PO_EVDEV_DIR)-1,PO_EVDEV_DIR,basec,base);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  int version=0;
  if (ioctl(fd,EVIOCGVERSION,&version)<0) {
    close(fd);
    return 0;
  }
  
  int grab=1;
  ioctl(fd,EVIOCGRAB,grab);
  
  struct input_id id={0};
  ioctl(fd,EVIOCGID,&id);
  char name[64];
  int namec=ioctl(fd,EVIOCGNAME(sizeof(name)),name);
  if ((namec<0)||(namec>sizeof(name))) namec=0;
  
  struct po_evdev_device *device=po_evdev_add_device(evdev,evno,fd);
  if (!device) {
    close(fd);
    return -1;
  }
  device->vendor=id.vendor;
  device->product=id.product;
  
  uint8_t absbit[(ABS_CNT+7)>>3]={0};
  ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(absbit)),absbit);
  int major=0; for (;major<sizeof(absbit);major++) {
    if (!absbit[major]) continue;
    int minor=0; for (;minor<8;minor++) {
      if (!(absbit[major]&(1<<minor))) continue;
      int code=(major<<3)|minor;
      
      struct input_absinfo absinfo={0};
      if (ioctl(fd,EVIOCGABS(code),&absinfo)<0) continue;
      
      po_evdev_device_add_axis(evdev,device,code,&absinfo);
    }
  }
  
  fprintf(stderr,"Connected input device: %.*s\n",namec,name);
  return 1;
}

/* Scan.
 */
 
static int po_evdev_scan(struct po_evdev *evdev) {
  DIR *dir=opendir(PO_EVDEV_DIR);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (po_evdev_try_file(evdev,de->d_name,-1)<0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

/* Close any file (inotify or device).
 */
 
static int po_evdev_drop_fd(struct po_evdev *evdev,int fd) {
  if (fd==evdev->infd) {
    fprintf(stderr,"%s: inotify shut down. Joystick connections will not be detected.\n",PO_EVDEV_DIR);
    close(evdev->infd);
    evdev->infd=-1;
    return 0;
  }
  struct po_evdev_device *device=evdev->devicev;
  int i=0;
  for (;i<evdev->devicec;i++,device++) {
    if (device->fd==fd) {
      int err=0;
      if (device->state) {
        uint16_t mask=0x8000;
        for (;mask;mask>>=1) {
          if (device->state&mask) {
            if (evdev->cb_button(evdev,mask,0)<0) err=-1;
          }
        }
      }
      po_evdev_device_cleanup(device);
      evdev->devicec--;
      memmove(device,device+1,sizeof(struct po_evdev_device)*(evdev->devicec-i));
      return err;
    }
  }
  return 0;
}

/* Read inotify.
 */
 
static int po_evdev_read_inotify(struct po_evdev *evdev) {
  char buf[1024];
  int bufc=read(evdev->infd,buf,sizeof(buf));
  if (bufc<=0) return po_evdev_drop_fd(evdev,evdev->infd);
  int bufp=0;
  while (bufp<=bufc-sizeof(struct inotify_event)) {
    struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-event->len) break;
    bufp+=event->len;
    int basec=0;
    while ((basec<event->len)&&event->name[basec]) basec++;
    po_evdev_try_file(evdev,event->name,basec);
  }
  return 0;
}

/* Find axis.
 */
 
static struct po_evdev_axis *po_evdev_device_find_axis(
  const struct po_evdev_device *device,
  uint16_t code
) {
  int lo=0,hi=device->axisc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct po_evdev_axis *axis=device->axisv+ck;
         if (code<axis->code) hi=ck;
    else if (code>axis->code) lo=ck+1;
    else return axis;
  }
  return 0;
}

/* Hard-coded input mapping.
 * This is obviously not ideal but whatever. TODO make more ideal
 */

static const struct po_evdev_button_map {
  uint16_t vendor;
  uint16_t product;
  uint16_t code;
  uint8_t btnid;
} po_evdev_button_mapv[]={

  // 'USB Gamepad ' (SNES knockoff)
  {0x0079,0x0011,0x0120,BUTTON_B}, // east (north naturally maps to A)
  {0x0079,0x0011,0x0121,BUTTON_A}, // south
  {0x0079,0x0011,0x0123,BUTTON_B}, // west
  {0x0079,0x0011,0x0129,BUTTON_QUIT}, // start
  
  // Microsoft X-Box 360 pad
  {0x045e,0x028e,0x013c,BUTTON_QUIT}, // heart
  
  // 8Bitdo  8BitDo M30 gamepad. (black) I'll leave C,Z reversed in case somebody likes it that way.
  {0x2dc8,0x5006,0x0131,BUTTON_A}, // B
  {0x2dc8,0x5006,0x0134,BUTTON_B}, // Y
  {0x2dc8,0x5006,0x0130,BUTTON_A}, // A
  {0x2dc8,0x5006,0x0133,BUTTON_B}, // X
  {0x2dc8,0x5006,0x013b,BUTTON_QUIT}, // start
  
  //2dc8:6001 '8Bitdo SF30 Pro   8Bitdo SN30 Pro' (gray)
  {0x2dc8,0x6001,0x0131,BUTTON_A}, // south
  {0x2dc8,0x6001,0x0134,BUTTON_B}, // west
  {0x2dc8,0x6001,0x013e,BUTTON_QUIT}, // right plunger

  // 8BitDo Tech Co., Ltd. 8BitDo Zero 2 gamepad (pink)
  {0x2dc8,0x9018,0x0131,BUTTON_A}, // south
  {0x2dc8,0x9018,0x0134,BUTTON_B}, // west
  {0x2dc8,0x9018,0x013b,BUTTON_QUIT}, // start
};
 
static uint16_t po_evdev_map_button(uint16_t vendor,uint16_t product,uint16_t code) {

  // KEY codes below 256 are keyboard-related; those are handled separately.
  if (code<0x100) return 0;
  
  // Look for hard-coded exceptions to the even/odd rule.
  int lo=0,hi=sizeof(po_evdev_button_mapv)/sizeof(struct po_evdev_button_map);
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct po_evdev_button_map *map=po_evdev_button_mapv+ck;
         if (vendor<map->vendor) hi=ck;
    else if (vendor>map->vendor) lo=ck+1;
    else if (product<map->product) hi=ck;
    else if (product>map->product) lo=ck+1;
    else if (code<map->code) hi=ck;
    else if (code>map->code) lo=ck+1;
    else return map->btnid;
  }
  
  // Finally, whatever, there's only two output buttons. Alternate even/odd.
  if (code&1) return BUTTON_B;
  return BUTTON_A;
}

/* Read device.
 */
 
static int po_evdev_key(struct po_evdev *evdev,struct po_evdev_device *device,uint8_t btnid,int value) {
  if (btnid<BUTTON_NONSTANDARD) {
    if (value) {
      if (device->state&btnid) return 0;
      device->state|=btnid;
      value=1;
    } else {
      if (!(device->state&btnid)) return 0;
      device->state&=~btnid;
    }
  } else {
    if (!value) return 0;
  }
  return evdev->cb_button(evdev,btnid,value);
}
 
static int po_evdev_event(
  struct po_evdev *evdev,
  struct po_evdev_device *device,
  const struct input_event *event
) {
  switch (event->type) {
  
    case EV_ABS: {
        struct po_evdev_axis *axis=po_evdev_device_find_axis(device,event->code);
        if (axis) switch (axis->mode) {
        
          case PO_EVDEV_AXIS_MODE_BUTTON: {
              int v=(event->value>=axis->hi)?1:0;
              if (v!=axis->v) {
                axis->v=v;
                if (v) device->state|=axis->axis;
                else device->state&=~axis->axis;
                return evdev->cb_button(evdev,axis->axis,v);
              }
            } break;
            
          case PO_EVDEV_AXIS_MODE_TWOWAY: {
              int v=(event->value<=axis->lo)?-1:(event->value>=axis->hi)?1:0;
              if (v!=axis->v) {
                axis->v=v;
                if (axis->axis==PO_EVDEV_AXIS_HORZ) {
                  if ((v<=0)&&(device->state&BUTTON_RIGHT)) {
                    device->state&=~BUTTON_RIGHT;
                    if (evdev->cb_button(evdev,BUTTON_RIGHT,0)<0) return -1;
                  }
                  if ((v>=0)&&(device->state&BUTTON_LEFT)) {
                    device->state&=~BUTTON_LEFT;
                    if (evdev->cb_button(evdev,BUTTON_LEFT,0)<0) return -1;
                  }
                  if ((v<0)&&!(device->state&BUTTON_LEFT)) {
                    device->state|=BUTTON_LEFT;
                    if (evdev->cb_button(evdev,BUTTON_LEFT,1)<0) return -1;
                  }
                  if ((v>0)&&!(device->state&BUTTON_RIGHT)) {
                    device->state|=BUTTON_RIGHT;
                    if (evdev->cb_button(evdev,BUTTON_RIGHT,1)<0) return -1;
                  }
                } else {
                  if ((v<=0)&&(device->state&BUTTON_DOWN)) {
                    device->state&=~BUTTON_DOWN;
                    if (evdev->cb_button(evdev,BUTTON_DOWN,0)<0) return -1;
                  }
                  if ((v>=0)&&(device->state&BUTTON_UP)) {
                    device->state&=~BUTTON_UP;
                    if (evdev->cb_button(evdev,BUTTON_UP,0)<0) return -1;
                  }
                  if ((v<0)&&!(device->state&BUTTON_UP)) {
                    device->state|=BUTTON_UP;
                    if (evdev->cb_button(evdev,BUTTON_UP,1)<0) return -1;
                  }
                  if ((v>0)&&!(device->state&BUTTON_DOWN)) {
                    device->state|=BUTTON_DOWN;
                    if (evdev->cb_button(evdev,BUTTON_DOWN,1)<0) return -1;
                  }
                }
              }
            } break;
            
          case PO_EVDEV_AXIS_MODE_HAT: {
              int v=event->value-axis->lo;
              if ((v<0)||(v>=8)) v=-1;
              if (v!=axis->v) {
                uint16_t nstate=0;
                switch (axis->v=v) {
                  case 0: nstate=BUTTON_UP; break;
                  case 1: nstate=BUTTON_UP|BUTTON_RIGHT; break;
                  case 2: nstate=BUTTON_RIGHT; break;
                  case 3: nstate=BUTTON_RIGHT|BUTTON_DOWN; break;
                  case 4: nstate=BUTTON_DOWN; break;
                  case 5: nstate=BUTTON_DOWN|BUTTON_LEFT; break;
                  case 6: nstate=BUTTON_LEFT; break;
                  case 7: nstate=BUTTON_LEFT|BUTTON_UP; break;
                }
                uint16_t ostate=device->state&(BUTTON_UP|BUTTON_DOWN|BUTTON_LEFT|BUTTON_RIGHT);
                device->state=(device->state&(BUTTON_A|BUTTON_B))|nstate;
                #define _(tag) \
                  if ((nstate&BUTTON_##tag)&&!(ostate&BUTTON_##tag)) { \
                    if (evdev->cb_button(evdev,BUTTON_##tag,1)<0) return -1; \
                  } else if (!(nstate&BUTTON_##tag)&&(ostate&BUTTON_##tag)) { \
                    if (evdev->cb_button(evdev,BUTTON_##tag,0)<0) return -1; \
                  }
                _(UP)
                _(DOWN)
                _(LEFT)
                _(RIGHT)
                #undef _
              }
            } break;
        }
      } break;
      
    case EV_KEY: switch (event->code) {
    
        // If we encounter a USB keyboard (actually not likely), we don't need to care which vendor:
        case KEY_ESC: return po_evdev_key(evdev,device,BUTTON_QUIT,event->value);
        case KEY_UP: return po_evdev_key(evdev,device,BUTTON_UP,event->value);
        case KEY_DOWN: return po_evdev_key(evdev,device,BUTTON_DOWN,event->value);
        case KEY_LEFT: return po_evdev_key(evdev,device,BUTTON_LEFT,event->value);
        case KEY_RIGHT: return po_evdev_key(evdev,device,BUTTON_RIGHT,event->value);
        case KEY_Z: return po_evdev_key(evdev,device,BUTTON_A,event->value);
        case KEY_X: return po_evdev_key(evdev,device,BUTTON_B,event->value);
        
        default: {
            int btnid=po_evdev_map_button(device->vendor,device->product,event->code);
            if (!btnid) return 0;
            return po_evdev_key(evdev,device,btnid,event->value);
          }
      } break;
      
  }
  return 0;
}
 
static int po_evdev_read_device(struct po_evdev *evdev,struct po_evdev_device *device) {
  if (!device) return -1;
  struct input_event eventv[32];
  int eventc=read(device->fd,eventv,sizeof(eventv));
  if (eventc<=0) return po_evdev_drop_fd(evdev,device->fd);
  eventc/=sizeof(struct input_event);
  const struct input_event *event=eventv;
  for (;eventc-->0;event++) {
    if (po_evdev_event(evdev,device,event)<0) return -1;
  }
  return 0;
}

/* Add to poll list.
 */
 
static int po_evdev_pollfdv_add(struct po_evdev *evdev,int fd) {
  
  if (evdev->pollfdc>=evdev->pollfda) {
    int na=evdev->pollfda+8;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(evdev->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    evdev->pollfdv=nv;
    evdev->pollfda=na;
  }
  
  struct pollfd *pollfd=evdev->pollfdv+evdev->pollfdc++;
  pollfd->fd=fd;
  pollfd->events=POLLIN|POLLERR|POLLHUP;
  pollfd->revents=0;
  return 0;
}

/* Update.
 */
 
int po_evdev_update(struct po_evdev *evdev) {
  if (evdev->rescan) {
    po_evdev_scan(evdev);
    evdev->rescan=0;
  }
  
  evdev->pollfdc=0;
  if (evdev->infd>=0) po_evdev_pollfdv_add(evdev,evdev->infd);
  struct po_evdev_device *device=evdev->devicev;
  int i=evdev->devicec;
  for (;i-->0;device++) po_evdev_pollfdv_add(evdev,device->fd);
  if (evdev->pollfdc<1) return 0;
  
  if (poll(evdev->pollfdv,evdev->pollfdc,0)<1) return 0;
  
  struct pollfd *pollfd=evdev->pollfdv;
  for (i=evdev->pollfdc;i-->0;pollfd++) {
    if (pollfd->revents&(POLLERR|POLLHUP)) {
      po_evdev_drop_fd(evdev,pollfd->fd);
    } else if (pollfd->revents&POLLIN) {
      if (pollfd->fd==evdev->infd) {
        po_evdev_read_inotify(evdev);
      } else {
        po_evdev_read_device(evdev,po_evdev_get_device_by_fd(evdev,pollfd->fd));
      }
    }
  }
  
  return 0;
}

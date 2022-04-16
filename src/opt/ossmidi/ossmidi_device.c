#include "ossmidi_internal.h"
#include <unistd.h>
#include <fcntl.h>

/* Delete.
 */
 
void ossmidi_device_del(struct ossmidi_device *device) {
  if (!device) return;
  if (device->refc-->1) return;
  
  if (device->fd>=0) close(device->fd);
  if (device->path) free(device->path);
  if (device->name) free(device->name);
  
  free(device);
}

/* Retain.
 */
 
int ossmidi_device_ref(struct ossmidi_device *device) {
  if (!device) return -1;
  if (device->refc<1) return -1;
  if (device->refc==INT_MAX) return -1;
  device->refc++;
  return 0;
}

/* Get name.
 * No error if we fail to find it from the system -- we'll make some default.
 */
 
static int ossmidi_device_acquire_name(struct ossmidi_device *device) {

  // If the path ends with a decimal integer, try looking it up in sndstat.
  int pathp=device->pathc;
  int devid=0,coef=1;
  while (pathp&&(device->path[pathp-1]>='0')&&(device->path[pathp-1]<='9')) {
    pathp--;
    devid+=(device->path[pathp]-'0')*coef;
    coef*=10;
  }
  if ((pathp!=device->pathc)&&(devid>=0)) {
    char tmp[256];
    int tmpc=ossmidi_find_device_name(tmp,sizeof(tmp),devid,0);
    if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
      if (!(device->name=malloc(tmpc+1))) return -1;
      memcpy(device->name,tmp,tmpc);
      device->name[tmpc]=0;
      device->namec=tmpc;
      return 0;
    }
  }
  
  // Use entire path for the name if present.
  if (device->pathc) {
    if (!(device->name=malloc(device->pathc+1))) return -1;
    memcpy(device->name,device->path,device->pathc+1);
    device->namec=device->pathc;
  
  // This should never happen, but if all else fails, our name is "?".
  } else {
    if (!(device->name=malloc(2))) return -1;
    device->name[0]='?';
    device->name[1]=0;
    device->namec=1;
  }
  return 0;
}

/* Open device file.
 */
 
static int ossmidi_device_open(struct ossmidi_device *device) {
  if ((device->fd=open(device->path,O_RDONLY))<0) return -1;
  return 0;
}

/* New.
 */

struct ossmidi_device *ossmidi_device_new(
  const char *path,
  int (*cb)(struct ossmidi_device *device,const struct midi_event *event),
  void *userdata
) {
  if (!path||!path[0]||!cb) return 0;
  
  struct ossmidi_device *device=calloc(1,sizeof(struct ossmidi_device));
  if (!device) return 0;
  
  device->refc=1;
  device->cb=cb;
  device->userdata=userdata;
  device->fd=-1;
  
  int pathc=0;
  while (path[pathc]) pathc++;
  if (!(device->path=malloc(pathc+1))) {
    ossmidi_device_del(device);
    return 0;
  }
  memcpy(device->path,path,pathc+1);
  device->pathc=pathc;
  
  if (
    (ossmidi_device_acquire_name(device)<0)||
    (ossmidi_device_open(device)<0)
  ) {
    ossmidi_device_del(device);
    return 0;
  }
  
  return device;
}

/* Receive input.
 */
 
int ossmidi_device_read(struct ossmidi_device *device) {
  char tmp[256];
  int tmpc=read(device->fd,tmp,sizeof(tmp));
  if (tmpc<=0) return -1;
  return ossmidi_device_receive_input(device,tmp,tmpc);
}

int ossmidi_device_receive_input(struct ossmidi_device *device,const void *src,int srcc) {
  int srcp=0,err;
  while (srcp<srcc) {
    struct midi_event event={0};
    if ((err=midi_stream_decode(&event,&device->stream,(char*)src+srcp,srcc-srcp))<=0) return -1;
    srcp+=err;
    if ((err=device->cb(device,&event))<0) return -1;
  }
  return 0;
}

/* Trivial accessors.
 */
 
void *ossmidi_device_get_userdata(const struct ossmidi_device *device) {
  return device->userdata;
}

int ossmidi_device_get_fd(const struct ossmidi_device *device) {
  return device->fd;
}

const char *ossmidi_device_get_name(const struct ossmidi_device *device) {
  return device->name;
}

const char *ossmidi_device_get_path(const struct ossmidi_device *device) {
  return device->path;
}

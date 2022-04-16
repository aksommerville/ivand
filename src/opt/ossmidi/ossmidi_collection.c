#include "ossmidi_internal.h"

/* Delete.
 */
 
void ossmidi_del(struct ossmidi *ossmidi) {
  if (!ossmidi) return;
  if (ossmidi->refc-->1) return;
  
  if (ossmidi->devv) {
    while (ossmidi->devc-->0) {
      struct ossmidi_device *device=ossmidi->devv[ossmidi->devc];
      poller_remove_file(ossmidi->poller,device->fd);
      ossmidi_device_del(device);
    }
    free(ossmidi->devv);
  }
  
  if (ossmidi->inotify) {
    poller_remove_file(ossmidi->poller,inotify_get_fd(ossmidi->inotify));
    inotify_del(ossmidi->inotify);
  }
  
  poller_del(ossmidi->poller);
  
  free(ossmidi);
}

/* Retain.
 */
 
int ossmidi_ref(struct ossmidi *ossmidi) {
  if (!ossmidi) return -1;
  if (ossmidi->refc<1) return -1;
  if (ossmidi->refc==INT_MAX) return -1;
  ossmidi->refc++;
  return 0;
}

/* Event callback from device.
 */
 
static int ossmidi_cb_event(struct ossmidi_device *device,const struct midi_event *event) {
  struct ossmidi *ossmidi=ossmidi_device_get_userdata(device);
  if (!ossmidi) return -1;
  if (ossmidi->delegate.cb_event) {
    int err=ossmidi->delegate.cb_event(ossmidi,device,event);
    if (err<0) return err;
  }
  return 0;
}

/* Callbacks from poller.
 */
 
static int ossmidi_cb_device_error(int fd,void *userdata) {
  struct ossmidi *ossmidi=userdata;
  if (!ossmidi) return -1;
  struct ossmidi_device *device=ossmidi_get_device_by_fd(ossmidi,fd);
  if (device) {
    ossmidi_drop_device(ossmidi,device);
  }
  return 0;
}

static int ossmidi_cb_device_read(int fd,void *userdata,const void *src,int srcc) {
  struct ossmidi *ossmidi=userdata;
  if (!ossmidi) return -1;
  struct ossmidi_device *device=ossmidi_get_device_by_fd(ossmidi,fd);
  if (!device) return 0;
  if (ossmidi_device_receive_input(device,src,srcc)<0) {
    ossmidi_drop_device(ossmidi,device);
  }
  return 0;
}

/* Callback from inotify.
 */
 
static int ossmidi_cb_inotify(const char *path,const char *base,int wd,void *userdata) {
  struct ossmidi *ossmidi=userdata;
  struct ossmidi_device *device=0;
  if (ossmidi_examine_file(&device,ossmidi,path,base)<0) return -1;
  if (!device) return 0;
  return 0;
}

/* Inotify readable.
 */
 
static int ossmidi_cb_inotify_readable(int fd,void *userdata) {
  return inotify_read(userdata);
}

/* Create and attach an inotify.
 */
 
static int ossmidi_init_inotify(struct ossmidi *ossmidi) {
  if (!(ossmidi->inotify=inotify_new(ossmidi_cb_inotify,ossmidi))) return -1;
  if (inotify_watch(ossmidi->inotify,ossmidi->path)<0) return -1;
  if (inotify_scan(ossmidi->inotify)<0) return -1;
  struct poller_file file={
    .fd=inotify_get_fd(ossmidi->inotify),
    .userdata=ossmidi->inotify,
    .cb_readable=ossmidi_cb_inotify_readable,
  };
  if (poller_add_file(ossmidi->poller,&file)<0) return -1;
  return 0;
}

/* New.
 */

struct ossmidi *ossmidi_new(
  const char *path,
  struct poller *poller,
  const struct ossmidi_delegate *delegate
) {
  if (!path||!path[0]) path="/dev";
  if (!poller) return 0;
  
  struct ossmidi *ossmidi=calloc(1,sizeof(struct ossmidi));
  if (!ossmidi) return 0;
  
  ossmidi->refc=1;
  if (delegate) ossmidi->delegate=*delegate;
  
  while (path[ossmidi->pathc]) ossmidi->pathc++;
  if (!(ossmidi->path=malloc(ossmidi->pathc+1))) {
    ossmidi_del(ossmidi);
    return 0;
  }
  memcpy(ossmidi->path,path,ossmidi->pathc+1);
  
  if (poller_ref(poller)<0) {
    ossmidi_del(ossmidi);
    return 0;
  }
  ossmidi->poller=poller;
  
  if (ossmidi_init_inotify(ossmidi)<0) {
    ossmidi_del(ossmidi);
    return 0;
  }
  
  return ossmidi;
}

/* Trivial accessors.
 */
 
void *ossmidi_get_userdata(const struct ossmidi *ossmidi) {
  return ossmidi->delegate.userdata;
}

/* Drop device.
 */
 
int ossmidi_drop_device(struct ossmidi *ossmidi,struct ossmidi_device *device) {
  if (!device) return -1;
  int i=ossmidi->devc;
  while (i-->0) {
    if (ossmidi->devv[i]==device) {
      ossmidi->devc--;
      memmove(ossmidi->devv+i,ossmidi->devv+i+1,sizeof(void*)*(ossmidi->devc-i));
      poller_remove_file(ossmidi->poller,device->fd);
      int err=0;
      if (ossmidi->delegate.cb_disconnect) {
        err=ossmidi->delegate.cb_disconnect(ossmidi,device);
      }
      ossmidi_device_del(device);
      return err;
    }
  }
  return -1;
}

/* Consider one file.
 */
 
int ossmidi_examine_file(
  struct ossmidi_device **devicertn,
  struct ossmidi *ossmidi,
  const char *path,const char *base
) {
  if (!ossmidi||!path) return 0;
  
  // Not interested if we already have it.
  int pathc=0; while (path[pathc]) pathc++;
  if (ossmidi_get_device_by_path(ossmidi,path,pathc)) return 0;
  
  if (!base) {
    const char *v=path;
    for (;*v;v++) if (*v=='/') base=v+1;
  }

  // We're only interested if it's under our path.
  if (memcmp(path,ossmidi->path,ossmidi->pathc)) return 0;
  if (path[ossmidi->pathc]!='/') return 0;
  
  // Basename must be "midiN" where N is a decimal integer.
  int p,devid=0;
  if (!memcmp(base,"midi",4)) p=4;
  else return 0;
  for (;base[p];p++) {
    int digit=base[p]-'0';
    if ((digit<0)||(digit>9)) return 0;
    devid*=10;
    devid+=digit;
  }
  
  // Make room for it.
  if (ossmidi->devc>=ossmidi->deva) {
    int na=ossmidi->deva+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ossmidi->devv,sizeof(void*)*na);
    if (!nv) return -1;
    ossmidi->devv=nv;
    ossmidi->deva=na;
  }
  
  // Attempt to connect.
  struct ossmidi_device *device=ossmidi_device_new(path,ossmidi_cb_event,ossmidi);
  if (!device) return 0;
  ossmidi->devv[ossmidi->devc++]=device;
  
  // Notify delegate.
  if (ossmidi->delegate.cb_connect) {
    int err=ossmidi->delegate.cb_connect(ossmidi,device);
    if (err<0) return err;
    // Delegate is allowed to drop the device. If it did, get out fast.
    if (!ossmidi_get_device_by_path(ossmidi,path,pathc)) return 0;
  }
  
  // Register device with the poller.
  struct poller_file file={
    .fd=device->fd,
    .userdata=ossmidi,
    .cb_error=ossmidi_cb_device_error,
    .cb_read=ossmidi_cb_device_read,
  };
  if (poller_add_file(ossmidi->poller,&file)<0) {
    ossmidi_drop_device(ossmidi,device);
    return -1;
  }
  
  if (devicertn) *devicertn=device;
  return 1;
}

/* Find device.
 */
 
struct ossmidi_device *ossmidi_get_device_by_path(const struct ossmidi *ossmidi,const char *path,int pathc) {
  if (!ossmidi||!path) return 0;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  int i=ossmidi->devc;
  while (i-->0) {
    struct ossmidi_device *device=ossmidi->devv[i];
    if (device->pathc!=pathc) continue;
    if (memcmp(device->path,path,pathc)) continue;
    return device;
  }
  return 0;
}

struct ossmidi_device *ossmidi_get_device_by_fd(const struct ossmidi *ossmidi,int fd) {
  if (!ossmidi) return 0;
  int i=ossmidi->devc;
  while (i-->0) {
    struct ossmidi_device *device=ossmidi->devv[i];
    if (device->fd==fd) return device;
  }
  return 0;
}

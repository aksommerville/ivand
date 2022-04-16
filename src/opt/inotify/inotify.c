#include "inotify_internal.h"

/* Delete.
 */

void inotify_del(struct inotify *ino) {
  if (!ino) return;
  if (ino->refc-->1) return;
  if (ino->fd>=0) close(ino->fd);
  if (ino->watchv) {
    while (ino->watchc-->0) {
      struct inotify_watch *watch=ino->watchv+ino->watchc;
      if (watch->path) free(watch->path);
    }
    free(ino->watchv);
  }
  if (ino->inbuf) free(ino->inbuf);
  free(ino);
}

/* Retain.
 */
 
int inotify_ref(struct inotify *ino) {
  if (!ino) return -1;
  if (ino->refc<1) return -1;
  if (ino->refc==INT_MAX) return -1;
  ino->refc++;
  return 0;
}

/* New.
 */

struct inotify *inotify_new(
  int (*cb)(const char *path,const char *base,int wd,void *userdata),
  void *userdata
) {
  if (!cb) return 0;
  struct inotify *ino=calloc(1,sizeof(struct inotify));
  if (!ino) return 0;
  
  ino->refc=1;
  if ((ino->fd=inotify_init())<0) {
    free(ino);
    return 0;
  }
  ino->cb=cb;
  ino->userdata=userdata;
  
  return ino;
}

/* Add watch.
 */

int inotify_watch(struct inotify *ino,const char *path) {
  if (!ino||!path||!path[0]) return -1;
  
  if (ino->watchc>=ino->watcha) {
    int na=ino->watcha+8;
    if (na>INT_MAX/sizeof(struct inotify_watch)) return -1;
    void *nv=realloc(ino->watchv,sizeof(struct inotify_watch)*na);
    if (!nv) return -1;
    ino->watchv=nv;
    ino->watcha=na;
  }
  
  int wd=inotify_add_watch(ino->fd,path,IN_CREATE|IN_ATTRIB|IN_MODIFY|IN_MOVED_TO);
  if (wd<0) return -1;
  
  struct inotify_watch *watch=ino->watchv+ino->watchc++;
  watch->wd=wd;
  if (!(watch->path=strdup(path))) {
    inotify_rm_watch(ino->fd,wd);
    ino->watchc--;
    return -1;
  }
  
  return wd;
}

/* Remove watch.
 */
 
int inotify_unwatch(struct inotify *ino,int wd) {
  if (!ino) return -1;
  int i=ino->watchc;
  struct inotify_watch *watch=ino->watchv+i-1;
  for (;i-->0;watch--) {
    if (watch->wd==wd) {
      inotify_rm_watch(ino->fd,watch->wd);
      if (watch->path) free(watch->path);
      ino->watchc--;
      memmove(watch,watch+1,sizeof(struct inotify_watch));
      return 0;
    }
  }
  return -1;
}

/* Report event to user.
 */
 
static int inotify_report_event(struct inotify *ino,struct inotify_event *event) {

  // Measure basename.
  const char *base=event->name;
  int basec=0;
  while ((basec<event->len)&&base[basec]) basec++;
  
  // Find watch. Report it anyway, if not found.
  const char *pfx=0;
  const struct inotify_watch *watch=ino->watchv;
  int i=ino->watchc;
  for (;i-->0;watch++) {
    if (watch->wd==event->wd) {
      pfx=watch->path;
      break;
    }
  }
  int pfxc=0;
  if (pfx) while (pfx[pfxc]) pfxc++;
  while (pfxc&&(pfx[pfxc-1]=='/')) pfxc--;
  
  // Assemble full path.
  char path[1024];
  if (pfxc+1+basec<sizeof(path)) {
    memcpy(path,pfx,pfxc);
    path[pfxc]='/';
    memcpy(path+pfxc+1,base,basec);
    path[pfxc+1+basec]=0;
  } else if (basec>=sizeof(path)) {
    return -1;
  } else {
    memcpy(path,base,basec);
    path[basec]=0;
  }
  
  return ino->cb(path,base,event->wd,ino->userdata);
}

/* Read events from input buffer.
 */

int inotify_consume_input(struct inotify *ino) {
  while (ino->inbufc>=sizeof(struct inotify_event)) {
  
    struct inotify_event *event=(struct inotify_event*)(ino->inbuf+ino->inbufp);
    int evlen=sizeof(struct inotify_event)+event->len;
    if (evlen>ino->inbufc) break;
    ino->inbufp+=evlen;
    ino->inbufc-=evlen;
    
    int err=inotify_report_event(ino,event);
    if (err<0) return err;
    
  }
  return 0;
}

/* Grow input buffer.
 */
 
static int inotify_inbuf_require(struct inotify *ino,int addc) {
  if (addc<1) return 0;
  if (ino->inbufc>INT_MAX-addc) return -1;
  if (ino->inbufp+ino->inbufc<=ino->inbufa-addc) return 0;
  if (ino->inbufp) {
    memmove(ino->inbuf,ino->inbuf+ino->inbufp,ino->inbufc);
    ino->inbufp=0;
    if (ino->inbufc<=ino->inbufa-addc) return 0;
  }
  int na=ino->inbufc+addc;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(ino->inbuf,na);
  if (!nv) return -1;
  ino->inbuf=nv;
  ino->inbufa=na;
  return 0;
}

/* Read.
 */

int inotify_read(struct inotify *ino) {
  if (!ino||(ino->fd<0)) return -1;
  if (inotify_inbuf_require(ino,1024)<0) return -1;
  int err=read(ino->fd,ino->inbuf+ino->inbufp+ino->inbufc,ino->inbufa-ino->inbufc-ino->inbufp);
  if (err<=0) return -1;
  ino->inbufc+=err;
  return inotify_consume_input(ino);
}
  
int inotify_receive_input(struct inotify *ino,const void *src,int srcc) {
  if (!ino||(srcc<0)||(srcc&&!src)) return -1;
  if (inotify_inbuf_require(ino,srcc)<0) return -1;
  memcpy(ino->inbuf+ino->inbufp+ino->inbufc,src,srcc);
  ino->inbufc+=srcc;
  return inotify_consume_input(ino);
}

/* Scan.
 */

struct inotify_scan_ctx {
  struct inotify *ino;
  struct inotify_watch *watch;
};

static int inotify_scan_cb(const char *path,const char *base,char type,void *userdata) {
  struct inotify_scan_ctx *ctx=userdata;
  int err=ctx->ino->cb(path,base,ctx->watch->wd,ctx->ino->userdata);
  if (err<0) return err;
  return 0;
}
 
int inotify_scan(struct inotify *ino) {
  if (!ino) return -1;
  struct inotify_scan_ctx ctx={.ino=ino};
  int i=ino->watchc; while (i-->0) {
    ctx.watch=ino->watchv+i;
    int err=dir_read(ctx.watch->path,inotify_scan_cb,&ctx);
    if (err<0) return err;
  }
  return 0;
}

/* Trivial accessors.
 */

void *inotify_get_userdata(const struct inotify *ino) {
  return ino->userdata;
}

int inotify_get_fd(const struct inotify *ino) {
  return ino->fd;
}

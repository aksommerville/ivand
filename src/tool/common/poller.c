#include "poller.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/poll.h>

/* Delete.
 */
 
static void poller_file_cleanup(struct poller_file *file) {
  if (file->ownfd&&(file->fd>=0)) close(file->fd);
  if (file->wbuf) free(file->wbuf);
}

void poller_del(struct poller *poller) {
  if (!poller) return;
  if (poller->refc-->1) return;
  if (poller->filev) {
    while (poller->filec-->0) {
      poller_file_cleanup(poller->filev+poller->filec);
    }
    free(poller->filev);
  }
  if (poller->tov) free(poller->tov);
  if (poller->intervalv) free(poller->intervalv);
  if (poller->pollfdv) free(poller->pollfdv);
  free(poller);
}

/* Retain.
 */
 
int poller_ref(struct poller *poller) {
  if (!poller) return -1;
  if (poller->refc<1) return -1;
  if (poller->refc==INT_MAX) return -1;
  poller->refc++;
  return 0;
}

/* New.
 */

struct poller *poller_new() {
  struct poller *poller=calloc(1,sizeof(struct poller));
  if (!poller) return 0;
  
  poller->refc=1;
  
  return poller;
}

/* File list.
 */
 
static int poller_filev_search(const struct poller *poller,int fd) {
  int lo=0,hi=poller->filec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct poller_file *file=poller->filev+ck;
         if (fd<file->fd) hi=ck;
    else if (fd>file->fd) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct poller_file *poller_file_by_fd(const struct poller *poller,int fd) {
  int p=poller_filev_search(poller,fd);
  if (p<0) return 0;
  return poller->filev+p;
}

static struct poller_file *poller_filev_insert(struct poller *poller,int p,int fd) {
  if (fd<0) return 0;
  if (p<0) {
    p=poller_filev_search(poller,fd);
    if (p>=0) return 0;
    p=-p-1;
  }
  if ((p<0)||(p>poller->filec)) return 0;
  if (p&&(fd<=poller->filev[p-1].fd)) return 0;
  if ((p<poller->filec)&&(fd>=poller->filev[p].fd)) return 0;
  
  if (poller->filec>=poller->filea) {
    int na=poller->filea+8;
    if (na>INT_MAX/sizeof(struct poller_file)) return 0;
    void *nv=realloc(poller->filev,sizeof(struct poller_file)*na);
    if (!nv) return 0;
    poller->filev=nv;
    poller->filea=na;
  }
  
  struct poller_file *file=poller->filev+p;
  memmove(file+1,file,sizeof(struct poller_file)*(poller->filec-p));
  poller->filec++;
  memset(file,0,sizeof(struct poller_file));
  file->fd=fd;
  
  return file;
}

/* Rebuild pollfdv.
 */
 
static int poller_add_pollfd(struct poller *poller,int fd,int events) {
  if (poller->pollfdc>=poller->pollfda) {
    int na=poller->pollfda+8;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(poller->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    poller->pollfdv=nv;
    poller->pollfda=na;
  }
  struct pollfd *pollfd=((struct pollfd*)poller->pollfdv)+poller->pollfdc++;
  pollfd->fd=fd;
  pollfd->events=events|POLLHUP|POLLERR;
  pollfd->revents=0;
  return 0;
}
 
static int poller_rebuild_pollfdv(struct poller *poller) {
  poller->pollfdc=0;
  struct poller_file *file=poller->filev;
  int i=poller->filec;
  for (;i-->0;file++) {
    int events=0;
    if (file->cb_readable||file->cb_read||file->cb_accept) events|=POLLIN;
    if (file->writeable||file->wbufc) events|=POLLOUT;
    if (!events) continue;
    if (poller_add_pollfd(poller,file->fd,events)<0) return -1;
  }
  return 0;
}

/* Check for any expired timeout, trigger and remove if found.
 */

static int poller_expire_timeouts(struct poller *poller) {
  if ((poller->toc<1)&&(poller->intervalc<1)) return 0;
  int64_t now=poller_time_now();
  
  int i=poller->toc;
  while (i-->0) {
    struct poller_timeout *to=poller->tov+i;
    if (to->expiry_us<=now) {
      int (*cb)(void*)=to->cb;
      void *userdata=to->userdata;
      poller->toc--;
      memmove(to,to+1,sizeof(struct poller_timeout)*(poller->toc-i));
      int err=cb(userdata);
      if (err<0) return -1;
    }
  }
  
  i=poller->intervalc;
  while (i-->0) {
    struct poller_interval *interval=poller->intervalv+i;
    if (interval->next_us<=now) {
      int err=interval->cb(interval->userdata);
      if (err<0) return -1;
      interval->next_us+=interval->interval_us;
      if (interval->next_us<=now) {
        // Missed a beat. Don't try to figure out how far out of sync we are; just reset it.
        interval->next_us=now+interval->interval_us;
      }
    }
  }
  
  return 0;
}

/* Return milliseconds to next timeout expiry, or <0 if none.
 */
 
static int poller_get_next_timeout_delay(const struct poller *poller) {
  if ((poller->toc<1)&&(poller->intervalc<1)) return -1;
  int64_t now=poller_time_now();
  int delay=INT_MAX;
  
  if (poller->toc>0) {
    const struct poller_timeout *to=poller->tov;
    int i=poller->toc;
    for (;i-->0;to++) {
      int64_t us=to->expiry_us-now;
      if (us<=0) return 0;
      int ms=(us+999)/1000; // round up to next millisecond
      if (ms<delay) delay=ms;
    }
  }
  
  if (poller->intervalc>0) {
    const struct poller_interval *interval=poller->intervalv;
    int i=poller->intervalc;
    for (;i-->0;interval++) {
      int64_t us=interval->next_us-now;
      if (us<=0) return 0;
      int ms=(us+999)/1000;
      if (ms<delay) delay=ms;
    }
  }
    
  return delay;
}

/* React to errors.
 */
 
static int poller_file_user_error(struct poller *poller,int fd) {
  poller_remove_file(poller,fd);
  return -1;
}

static int poller_file_io_error(struct poller *poller,int fd) {
  struct poller_file *file=poller_file_by_fd(poller,fd);
  if (!file) return 0;
  
  if (file->cb_error) {
    int err=file->cb_error(fd,file->userdata);
    if (err<0) return poller_file_user_error(poller,fd);
  } else {
    poller_remove_file(poller,fd);
  }
  
  return 0;
}

/* Update readable or failed file.
 */
 
static int poller_file_update_readable(struct poller *poller,struct poller_file *file) {
  int fd=file->fd;
  
  // Easiest thing is they implemented (cb_readable), user takes care of it.
  if (file->cb_readable) {
    int err=file->cb_readable(fd,file->userdata);
    if (err<0) return poller_file_user_error(poller,fd);
    return 0;
  }
  
  // (cb_accept) if they do that.
  if (file->cb_accept) {
    char saddr[128];
    socklen_t saddrc=sizeof(saddr);
    int clientfd=accept(fd,(struct sockaddr*)saddr,&saddrc);
    if (clientfd<0) return poller_file_io_error(poller,fd);
    int err=file->cb_accept(fd,file->userdata,clientfd,saddr,saddrc);
    if (err<0) {
      close(clientfd);
      return poller_file_user_error(poller,fd);
    }
    return 0;
  }
  
  // (cb_read) if they do that, using a temporary buffer.
  if (file->cb_read) {
    char tmp[1024];
    int tmpc=read(fd,tmp,sizeof(tmp));
    if (tmpc<=0) return poller_file_io_error(poller,fd);
    int err=file->cb_read(fd,file->userdata,tmp,tmpc);
    if (err<0) return poller_file_user_error(poller,fd);
    return 0;
  }
  
  // No read hook? Must have been POLLERR or POLLHUP, just call it I/O error.
  return poller_file_io_error(poller,fd);
}

/* Update writeable file.
 */
 
static int poller_file_update_writeable(struct poller *poller,struct poller_file *file) {
  int fd=file->fd;

  if (file->wbufc) {
    int err=write(file->fd,file->wbuf+file->wbufp,file->wbufc);
    if (err<=0) return poller_file_io_error(poller,fd);
    if (file->wbufc-=err) file->wbufp+=err;
    else file->wbufp=0;
    return 0;
  }
  
  if (file->cb_writeable&&file->writeable) {
    if (file->cb_writeable(file->fd,file->userdata)<0) {
      return poller_file_user_error(poller,fd);
    }
    return 0;
  }
  
  // Maybe some other callback modified its write buffer? Whatever, no worries.
  return 0;
}

/* Update one file identified by poll().
 */
 
static int poller_file_update(struct poller *poller,int fd,int revents) {

  // It's possible for file to go missing, eg one updater removes some other file. No worries.
  struct poller_file *file=poller_file_by_fd(poller,fd);
  if (!file) return 0;
  
  if (revents&(POLLIN|POLLERR|POLLHUP)) {
    if (poller_file_update_readable(poller,file)<0) return -1;
    // The read callback may have changed our file list.
    if (revents==POLLIN) return 0;
    if (!(file=poller_file_by_fd(poller,fd))) return 0;
  }
  
  if (revents&POLLOUT) {
    if (poller_file_update_writeable(poller,file)<0) return -1;
  }
  
  return 0;
}

/* Update.
 */

int poller_update(struct poller *poller,int to_ms) {

  if (poller_rebuild_pollfdv(poller)<0) return -1;
  
  // Sleep no longer than the time to the next scheduled timeout.
  // NB This happens even if (to_ms<0).
  int tomsmax=poller_get_next_timeout_delay(poller);
  if (tomsmax>=0) {
    if ((tomsmax<to_ms)||(to_ms<0)) to_ms=tomsmax;
  }
  
  // If we have no files to poll, just sleep and check timeouts.
  if (poller->pollfdc<1) {
    if (to_ms>0) {
      if (to_ms>INT_MAX/1000) usleep(INT_MAX);
      else usleep(to_ms*1000);
    }
    if (poller_expire_timeouts(poller)<0) return -1;
    return 0;
  }
  
  // Poll.
  int readyc=poll(poller->pollfdv,poller->pollfdc,to_ms);
  if (readyc<0) {
    if (errno==EINTR) readyc=0; // Interrupted, not really an error.
    else return -1;
  }
  
  // Find the polled ones and act upon them.
  if (readyc) {
    struct pollfd *pollfd=poller->pollfdv;
    int i=poller->pollfdc;
    for (;i-->0;pollfd++) {
      if (!pollfd->revents) continue;
      if (poller_file_update(poller,pollfd->fd,pollfd->revents)<0) return -1;
      if (!--readyc) break;
    }
  }
  
  // Check timeouts.
  if (poller_expire_timeouts(poller)<0) return -1;

  return 0;
}

/* Add file.
 */

int poller_add_file(struct poller *poller,const struct poller_file *file) {
  if (!poller||!file) return -1;
  if (file->fd<0) return -1;
  
  // (cb_readable,cb_read,cb_accept) are mutually exclusive.
  int rhandlerc=0;
  if (file->cb_readable) rhandlerc++;
  if (file->cb_read) rhandlerc++;
  if (file->cb_accept) rhandlerc++;
  if (rhandlerc>1) return -1;
  
  int p=poller_filev_search(poller,file->fd);
  if (p>=0) return -1;
  p=-p-1;
  struct poller_file *nfile=poller_filev_insert(poller,p,file->fd);
  if (!nfile) return -1;
  
  // Copy only selected members, eg we do not want (wbuf*) and already have (fd).
  nfile->ownfd=file->ownfd;
  nfile->userdata=file->userdata;
  nfile->cb_error=file->cb_error;
  nfile->cb_readable=file->cb_readable;
  nfile->cb_writeable=file->cb_writeable;
  nfile->cb_read=file->cb_read;
  nfile->cb_accept=file->cb_accept;
  
  return 0;
}

/* Remove file.
 */

int poller_remove_file(struct poller *poller,int fd) {
  if (!poller) return -1;
  int p=poller_filev_search(poller,fd);
  if (p<0) return -1;
  
  struct poller_file *file=poller->filev+p;
  poller_file_cleanup(file);
  poller->filec--;
  memmove(file,file+1,sizeof(struct poller_file)*(poller->filec-p));
  
  return 0;
}

/* Queue output.
 */

int poller_queue_output(struct poller *poller,int fd,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  void *dst=0;
  int dsta=poller_prepare_output(&dst,poller,fd,srcc);
  if (dsta<0) return -1;
  memcpy(dst,src,srcc);
  return poller_commit_output(poller,fd,srcc);
}

int poller_prepare_output(void *dstpp,struct poller *poller,int fd,int cmin) {
  struct poller_file *file=poller_file_by_fd(poller,fd);
  if (!file) return -1;
  
  // Output queue is forbidden to files which implement their own (writeable).
  if (file->cb_writeable) return -1;
  
  if (cmin>0) {
    if (file->wbufc>INT_MAX-cmin) return -1;
    if (file->wbufp+file->wbufc>file->wbufa-cmin) {
      if (file->wbufp) {
        memmove(file->wbuf,file->wbuf+file->wbufp,file->wbufc);
        file->wbufp=0;
      }
      int na=file->wbufc+cmin;
      if (na<INT_MAX-256) na=(na+256)&~255;
      if (na>file->wbufa) { // could be less due to wbufp
        void *nv=realloc(file->wbuf,na);
        if (!nv) return -1;
        file->wbuf=nv;
        file->wbufa=na;
      }
    }
  }
  
  if (dstpp) *(void**)dstpp=file->wbuf+file->wbufp+file->wbufc;
  return file->wbufa-file->wbufc-file->wbufp;
}

int poller_commit_output(struct poller *poller,int fd,int c) {
  if (c<0) return -1;
  if (!c) return 0;
  struct poller_file *file=poller_file_by_fd(poller,fd);
  if (!file) return -1;
  if (file->wbufp+file->wbufc>file->wbufa-c) return -1;
  file->wbufc+=c;
  return 0;
}

/* Writeable flag.
 */
 
int poller_set_writeable(struct poller *poller,int fd,int writeable) {
  struct poller_file *file=poller_file_by_fd(poller,fd);
  if (!file) return -1;
  if (!file->cb_writeable) return -1;
  file->writeable=writeable;
  return 0;
}

/* Unused timeout ID.
 * Beware they aren't sorted or anything.
 */
 
static struct poller_timeout *poller_to_by_id(const struct poller *poller,int toid) {
  int i=poller->toc;
  struct poller_timeout *to=poller->tov;
  for (;i-->0;to++) {
    if (to->id==toid) return to;
  }
  return 0;
}
 
static int poller_unused_toid(const struct poller *poller) {
  if (poller->toc<1) return 1;
  int lo=poller->tov[0].id;
  int hi=lo;
  int i=poller->toc;
  const struct poller_timeout *to=poller->tov;
  for (;i-->0;to++) {
    if (to->id<lo) lo=to->id;
    else if (to->id>hi) hi=to->id;
  }
  if (lo>1) return lo-1;
  if (hi<INT_MAX) return hi+1;
  // Unlikely...
  for (i=1;i<INT_MAX;i++) {
    if (!poller_to_by_id(poller,i)) return i;
  }
  return -1;
}

/* Set timeout.
 */
 
int poller_set_timeout(
  struct poller *poller,
  int delay_ms,
  int (*cb)(void *userdata),
  void *userdata
) {
  if (!cb) return -1;
  
  if (poller->toc>=poller->toa) {
    int na=poller->toa+8;
    if (na>INT_MAX/sizeof(struct poller_timeout)) return -1;
    void *nv=realloc(poller->tov,sizeof(struct poller_timeout)*na);
    if (!nv) return -1;
    poller->tov=nv;
    poller->toa=na;
  }
  
  // We can only express delays up to INT_MAX ms, about 24 days.
  int64_t expiry=poller_time_now();
  if (delay_ms>INT_MAX/1000) expiry=INT64_MAX;
  else if (delay_ms>0) expiry+=delay_ms*1000;
  
  int id=poller_unused_toid(poller);
  if (id<=0) return -1;
  
  struct poller_timeout *to=poller->tov+poller->toc++;
  memset(to,0,sizeof(struct poller_timeout));
  to->cb=cb;
  to->userdata=userdata;
  to->id=id;
  to->expiry_us=expiry;
  
  return id;
}

/* Cancel timeout.
 */
 
int poller_cancel_timeout(struct poller *poller,int toid) {
  int i=poller->toc;
  struct poller_timeout *to=poller->tov+i-1;
  for (;i-->0;to--) {
    if (to->id==toid) {
      poller->toc--;
      memmove(to,to+1,sizeof(struct poller_timeout)*(poller->toc-i));
      return 0;
    }
  }
  return -1;
}

/* Unused interval id.
 */
 
static struct poller_interval *poller_interval_by_id(const struct poller *poller,int id) {
  int i=poller->intervalc;
  struct poller_interval *interval=poller->intervalv;
  for (;i-->0;interval++) {
    if (interval->id==id) return interval;
  }
  return 0;
}
 
static int poller_unused_intervalid(const struct poller *poller) {
  if (poller->intervalc<1) return 1;
  int lo=poller->intervalv[0].id;
  int hi=lo;
  int i=poller->intervalc;
  const struct poller_interval *interval=poller->intervalv;
  for (;i-->0;interval++) {
    if (interval->id<lo) lo=interval->id;
    else if (interval->id>hi) hi=interval->id;
  }
  if (lo>1) return lo-1;
  if (hi<INT_MAX) return hi+1;
  // Unlikely...
  for (i=1;i<INT_MAX;i++) {
    if (!poller_interval_by_id(poller,i)) return i;
  }
  return -1;
}
 
/* Add interval.
 */
 
int poller_set_interval_us(
  struct poller *poller,
  int64_t interval_us,
  int (*cb)(void *userdata),
  void *userdata
) {
  if (!cb||(interval_us<1)) return -1;
  
  if (poller->intervalc>=poller->intervala) {
    int na=poller->intervala+8;
    if (na>INT_MAX/sizeof(struct poller_interval)) return -1;
    void *nv=realloc(poller->intervalv,sizeof(struct poller_interval)*na);
    if (!nv) return -1;
    poller->intervalv=nv;
    poller->intervala=na;
  }
  
  int id=poller_unused_intervalid(poller);
  if (id<=0) return -1;
  
  struct poller_interval *interval=poller->intervalv+poller->intervalc++;
  memset(interval,0,sizeof(struct poller_interval));
  interval->cb=cb;
  interval->userdata=userdata;
  interval->id=id;
  interval->interval_us=interval_us;
  interval->next_us=poller_time_now()+interval_us;
  
  return id;
}

/* Cancel interval.
 */

int poller_cancel_interval(struct poller *poller,int intid) {
  int i=poller->intervalc;
  struct poller_interval *interval=poller->intervalv+i-1;
  for (;i-->0;interval--) {
    if (interval->id==intid) {
      poller->intervalc--;
      memmove(interval,interval+1,sizeof(struct poller_interval)*(poller->intervalc-i));
      return 0;
    }
  }
  return -1;
}

/* Current time.
 */
 
int64_t poller_time_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

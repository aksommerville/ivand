/* poller.h
 */
 
#ifndef POLLER_H
#define POLLER_H

#include <stdint.h>

struct poller {
  int refc;
  
  struct poller_file {
    int fd;
    int ownfd;
    void *userdata;
    int (*cb_error)(int fd,void *userdata);
    int (*cb_readable)(int fd,void *userdata);
    int (*cb_writeable)(int fd,void *userdata);
    int (*cb_read)(int fd,void *userdata,const void *src,int srcc);
    int (*cb_accept)(int fd,void *userdata,int clientfd_HANDOFF,const void *saddr,int saddrc);
    char *wbuf;
    int wbufp,wbufc,wbufa;
    int writeable;
  } *filev;
  int filec,filea;
  
  struct poller_timeout {
    int64_t expiry_us;
    int id;
    void *userdata;
    int (*cb)(void *userdata);
  } *tov;
  int toc,toa;
  
  struct poller_interval {
    int64_t next_us;
    int id;
    int interval_us;
    void *userdata;
    int (*cb)(void *userdata);
  } *intervalv;
  int intervalc,intervala;
  
  void *pollfdv;
  int pollfdc,pollfda;
};

void poller_del(struct poller *poller);
int poller_ref(struct poller *poller);

struct poller *poller_new();

int poller_update(struct poller *poller,int to_ms);

/* (file) contains the fd and callbacks. (wbuf*) are ignored.
 * If (file->ownfd), poller takes ownership of the file and will eventually close it.
 * You should implement one of (cb_readable,cb_read,cb_accept) and MUST NOT implement more than one.
 * (cb_read) has only one chance to see the incoming data, eg you can't return <srcc and get the rest later.
 * If you implement (cb_writeable), poller_queue_output() on this file will fail.
 * If you implement (cb_error) and return >=0 from it, removing the file is up to you.
 * Negative results from your callbacks are fatal -- the poller_update() that called them will fail.
 */
int poller_add_file(struct poller *poller,const struct poller_file *file);

int poller_remove_file(struct poller *poller,int fd);

/* Each file may have an output buffer, which if set we will poll and write out when possible.
 * poller_queue_output() to copy some existing content at the end of the buffer.
 * "prepare" then "commit" to allocate some space at the end, then declare it occupied.
 */
int poller_queue_output(struct poller *poller,int fd,const void *src,int srcc);
int poller_prepare_output(void *dstpp,struct poller *poller,int fd,int cmin);
int poller_commit_output(struct poller *poller,int fd,int c);

/* If you're using (cb_writeable), call this to indicate you're ready to write.
 * (cb_writeable) will never be called when this is false, we won't even poll for it.
 * We never unset this flag, you should probably do so in the (cb_writeable) implementation when you're done writing.
 */
int poller_set_writeable(struct poller *poller,int fd,int writeable);

/* Arrange for (cb) to be called (delay_ms) milliseconds in the future.
 * We're not super precise about timing.
 * The callback will happen during a future poller_update(), 
 * and is guaranteed not to happen during this set_timeout call (even for a <=0 delay).
 */
int poller_set_timeout(
  struct poller *poller,
  int delay_ms,
  int (*cb)(void *userdata),
  void *userdata
);
int poller_cancel_timeout(struct poller *poller,int toid);

/* Intervals are the same thing as timeouts, but they'll be called regularly until cancelled.
 * (same semantics as the Web setTimeout and setInterval).
 * Beware, interval times are specified in microseconds where everything else is milliseconds.
 */
int poller_set_interval_us(
  struct poller *poller,
  int64_t interval_us,
  int (*cb)(void *userdata),
  void *userdata
);
int poller_cancel_interval(struct poller *poller,int intid);

// Current calendar time in microseconds.
int64_t poller_time_now();

#endif

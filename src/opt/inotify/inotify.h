/* inotify.h
 * Shallow wrapper around Linux's inotify system.
 * Each inotify instance has a single file and userdata, and can watch any number of directories.
 * Only files immediately under the watched directory are reported.
 *
 * *** pokorc: For use in tools only. Refers to some sources under tool/common/ ***
 */
 
#ifndef INOTIFY_H
#define INOTIFY_H

struct inotify;

void inotify_del(struct inotify *ino);
int inotify_ref(struct inotify *ino);

struct inotify *inotify_new(
  int (*cb)(const char *path,const char *base,int wd,void *userdata),
  void *userdata
);

int inotify_watch(struct inotify *ino,const char *path); // => wd
int inotify_unwatch(struct inotify *ino,int wd);

int inotify_read(struct inotify *ino);
int inotify_receive_input(struct inotify *ino,const void *src,int srcc);

/* A convenience, not actually inotify-related.
 * Read each directory that we're watching and report everything as if it was just created.
 * You'll normally want to do this right after setup.
 */
int inotify_scan(struct inotify *ino);

void *inotify_get_userdata(const struct inotify *ino);
int inotify_get_fd(const struct inotify *ino);

#endif

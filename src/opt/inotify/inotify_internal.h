#ifndef INOTIFY_INTERNAL_H
#define INOTIFY_INTERNAL_H

#include "inotify.h"
#include "tool/common/fs.h"
#include <sys/inotify.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct inotify {
  int refc;
  int fd;
  int (*cb)(const char *path,const char *base,int wd,void *userdata);
  void *userdata;
  struct inotify_watch {
    int wd;
    char *path;
  } *watchv;
  int watchc,watcha;
  char *inbuf;
  int inbufp,inbufc,inbufa;
};

int inotify_consume_input(struct inotify *ino);

#endif

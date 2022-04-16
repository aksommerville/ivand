#ifndef OSSMIDI_INTERNAL_H
#define OSSMIDI_INTERNAL_H

#include "ossmidi.h"
#include "tool/common/midi.h"
#include "tool/common/poller.h"
#include "opt/inotify/inotify.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct ossmidi {
  int refc;
  struct inotify *inotify;
  struct poller *poller;
  struct ossmidi_device **devv;
  int devc,deva;
  struct ossmidi_delegate delegate;
  char *path;
  int pathc;
};

struct ossmidi_device {
  int refc;
  int fd;
  char *path;
  int pathc;
  char *name;
  int namec;
  struct midi_stream stream;
  int (*cb)(struct ossmidi_device *device,const struct midi_event *event);
  void *userdata;
};

#endif

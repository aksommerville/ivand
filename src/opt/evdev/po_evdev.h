#ifndef PO_EVDEV_H
#define PO_EVDEV_H

#include <stdint.h>
 
// We'll report nonstandard buttons as >=0xf0 (multiple bits at once)
// Nonstandard buttons are ON only, no OFF.
#define BUTTON_NONSTANDARD 0xf0
#define BUTTON_QUIT 0xf0

struct po_evdev;

void po_evdev_del(struct po_evdev *evdev);

struct po_evdev *po_evdev_new(
  int (*cb_button)(struct po_evdev *evdev,uint8_t btnid,int value),
  void *userdata
);

void *po_evdev_get_userdata(const struct po_evdev *evdev);

int po_evdev_update(struct po_evdev *evdev);

#endif

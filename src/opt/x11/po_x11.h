#ifndef PO_X11_H
#define PO_X11_H

#include <stdint.h>

struct po_x11;

void po_x11_del(struct po_x11 *x11);

struct po_x11 *po_x11_new(
  const char *title,
  int fbw,int fbh,
  int fullscreen,
  int (*cb_button)(struct po_x11 *x11,uint8_t btnid,int value),
  int (*cb_close)(struct po_x11 *x11),
  void *userdata
);

void *po_x11_get_userdata(const struct po_x11 *x11);
int po_x11_swap(struct po_x11 *x11,const void *fb);
int po_x11_set_fullscreen(struct po_x11 *x11,int state);
void po_x11_inhibit_screensaver(struct po_x11 *x11);
int po_x11_update(struct po_x11 *x11);

#endif

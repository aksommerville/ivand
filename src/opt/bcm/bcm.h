/* bcm.h
 * Interface to Broadcom VideoCore for Raspberry Pi (older models, that don't support libdrm).
 */

#ifndef BCM_H
#define BCM_H

struct bcm;

void bcm_del(struct bcm *bcm);
struct bcm *bcm_new(int w,int h);
void bcm_swap(struct bcm *bcm,const void *fb);

#endif

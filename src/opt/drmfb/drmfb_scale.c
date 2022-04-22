#include "drmfb_internal.h"

/* Given (fbw,fbh,screenw,screenh), calculate reasonable values for (scale,dstx,dsty,dstw,dsth).
 */
 
int drmfb_calculate_output_bounds(struct drmfb *drmfb) {
  
  // Use the largest legal scale factor.
  // Too small is very unlikely -- our input is 96x64. So we're not going to handle that case, just fail.
  int scalex=drmfb->screenw/drmfb->fbw;
  int scaley=drmfb->screenh/drmfb->fbh;
  drmfb->scale=(scalex<scaley)?scalex:scaley;
  if (drmfb->scale<1) {
    fprintf(stderr,
      "Unable to fit %dx%d framebuffer on this %dx%d screen.\n",
      drmfb->fbw,drmfb->fbh,drmfb->screenw,drmfb->screenh
    );
    return -1;
  }
  if (drmfb->scale>DRMFB_SCALE_LIMIT) drmfb->scale=DRMFB_SCALE_LIMIT;
  
  drmfb->dstw=drmfb->scale*drmfb->fbw;
  drmfb->dsth=drmfb->scale*drmfb->fbh;
  drmfb->dstx=(drmfb->screenw>>1)-(drmfb->dstw>>1);
  drmfb->dsty=(drmfb->screenh>>1)-(drmfb->dsth>>1);
  
  return 0;
}

/* Scale image into framebuffer.
 */
 
void drmfb_scale(struct drmfb_fb *dst,struct drmfb *drmfb,const uint16_t *src) {
  uint16_t *dstrow=dst->v;
  dstrow+=drmfb->dsty*drmfb->stridewords+drmfb->dstx;
  int cpc=drmfb->dstw*sizeof(uint16_t);
  const uint16_t *srcrow=src;
  int yi=drmfb->fbh;
  for (;yi-->0;srcrow+=drmfb->fbw) {
    uint16_t *dstp=dstrow;
    const uint16_t *srcp=srcrow;
    int xi=drmfb->fbw;
    for (;xi-->0;srcp++) {
      uint16_t pixel=*srcp;
      
      // Don't ask me why the Tiny does it this way...
      uint8_t r=(pixel>>8)&0x1f;
      uint8_t b=(pixel>>3)&0x1f;
      uint8_t g=((pixel<<3)&0x38)|(pixel>>13);
      pixel=(r<<11)|(g<<5)|b;
      
      int ri=drmfb->scale;
      for (;ri-->0;dstp++) {
        *dstp=pixel;
      }
    }
    dstp=dstrow;
    dstrow+=drmfb->stridewords;
    int ri=drmfb->scale-1;
    for (;ri-->0;dstrow+=drmfb->stridewords) {
      memcpy(dstrow,dstp,cpc);
    }
  }
}

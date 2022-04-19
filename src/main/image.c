#include "platform.h"
#include <string.h>
#include <stdio.h>

/* Blit preamble.
 */
 
#define PREBLIT \
  if (!dst||!src) return; \
  if (dstx<0) { srcx-=dstx; w+=dstx; dstx=0; } \
  if (dsty<0) { srcy-=dsty; h+=dsty; dsty=0; } \
  if (dstx>dst->w-w) w=dst->w-dstx; \
  if (dsty>dst->h-h) h=dst->h-dsty; \
  if ((w<1)||(h<1)) return;

/* Blit opaque.
 */
 
void image_blit_opaque(
  struct image *dst,int16_t dstx,int16_t dsty,
  const struct image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h
) {
  PREBLIT
  uint16_t *dstrow=dst->v+dsty*dst->stride+dstx;
  const uint16_t *srcrow=src->v+srcy*src->stride+srcx;
  int yi=h;
  int cpc=w<<1;
  for (;yi-->0;dstrow+=dst->stride,srcrow+=src->stride) {
    memcpy(dstrow,srcrow,cpc);
  }
}

/* Blit with colorkey.
 */
 
void image_blit_colorkey(
  struct image *dst,int16_t dstx,int16_t dsty,
  const struct image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h
) {
  PREBLIT
  uint16_t *dstrow=dst->v+dsty*dst->stride+dstx;
  const uint16_t *srcrow=src->v+srcy*src->stride+srcx;
  int yi=h;
  for (;yi-->0;dstrow+=dst->stride,srcrow+=src->stride) {
    uint16_t *dstp=dstrow;
    const uint16_t *srcp=srcrow;
    int xi=w;
    for (;xi-->0;dstp++,srcp++) {
      if (*srcp) *dstp=*srcp;
    }
  }
}

/* Blit with colorkey, reversing X axis.
 */
 
void image_blit_colorkey_flop(
  struct image *dst,int16_t dstx,int16_t dsty,
  const struct image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h
) {

  // Not quite PREBLIT, we have to switch some things.
  if (!dst||!src) return;
  if (dstx<0) { w+=dstx; dstx=0; }
  if (dsty<0) { srcy-=dsty; h+=dsty; dsty=0; }
  if (dstx>dst->w-w) { srcx+=dstx+w-dst->w; w=dst->w-dstx; }
  if (dsty>dst->h-h) h=dst->h-dsty;
  if ((w<1)||(h<1)) return;
  
  uint16_t *dstrow=dst->v+dsty*dst->stride+dstx;
  const uint16_t *srcrow=src->v+srcy*src->stride+srcx+w-1;
  int yi=h;
  for (;yi-->0;dstrow+=dst->stride,srcrow+=src->stride) {
    uint16_t *dstp=dstrow;
    const uint16_t *srcp=srcrow;
    int xi=w;
    for (;xi-->0;dstp++,srcp--) {
      if (*srcp) *dstp=*srcp;
    }
  }
}

/* Blit from 1-bit glyph.
 */
 
uint8_t image_blit_glyph(
  struct image *dst,int16_t dstx,int16_t dsty,
  uint32_t glyph,uint16_t color
) {
  if (!glyph) return 4; // space. no output but do advance horizontally
  uint8_t srcy=glyph>>30;
  uint8_t srcw=(glyph>>27)&7;
  if (!srcw) return 0; // nonzero but zero width denotes a "nothing" glyph (which we don't use)
  dsty+=srcy;
  
  uint16_t *dstrow=dst->v+dsty*dst->stride+dstx;
  uint32_t mask=0x04000000;
  uint8_t yi=8;
  for (;yi-->0;dstrow+=dst->stride,dsty++) {
    if (dsty>=dst->h) return srcw+1;
    if (dsty<0) {
      mask>>=srcw;
      continue;
    }
    uint16_t *dstp=dstrow;
    uint8_t xi=srcw;
    for (;xi-->0;dstp++,mask>>=1) {
      if (!mask) return srcw+1;
      if (glyph&mask) {
        if ((dstx>=0)&&(dstx<dst->w)) {
          *dstp=color;
        }
      }
    }
  }
  return srcw+1;
}

/* Blit string (convenience).
 */
 
uint16_t image_blit_string(
  struct image *dst,int16_t dstx,int16_t dsty,
  const char *src,int8_t srcc,uint16_t color,
  const uint32_t *font
) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int16_t dstx0=dstx;
  for (;srcc-->0;src++) {
    if ((*src<0x20)||(*src>0x7e)) continue;
    dstx+=image_blit_glyph(dst,dstx,dsty,font[(*src)-0x20],color);
  }
  return dstx-dstx0;
}

/* Fill rect.
 */
 
void image_fill_rect(struct image *image,int16_t x,int16_t y,int16_t w,int16_t h,uint16_t color) {
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>image->w-w) w=image->w-x;
  if (y>image->h-h) h=image->h-y;
  if ((w<1)||(h<1)) return;
  uint16_t *dstrow=image->v+y*image->w+x;
  for (;h-->0;dstrow+=image->stride) {
    uint16_t *dstp=dstrow;
    int16_t xi=w;
    for (;xi-->0;dstp++) *dstp=color;
  }
}

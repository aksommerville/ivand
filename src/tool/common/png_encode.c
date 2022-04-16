#include "png.h"
#include "decoder.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Encoder context, only present during IDAT.
 */
 
struct png_encoder {
  struct encoder *dst;
  const struct png_image *image;
  z_stream z;
  int fstride; // 1+stride
  uint8_t *rowbuf;
};

static void png_encoder_cleanup(struct png_encoder *encoder) {
  deflateEnd(&encoder->z);
  if (encoder->rowbuf) free(encoder->rowbuf);
}

/* IDAT row.
 */
 
static int png_encode_IDAT_row(struct png_encoder *encoder,const uint8_t *src,const uint8_t *pv) {
  
  //TODO If we want to get real, filter the row 5 times and use some heuristic to pick one.
  // eg the one with the most zeroes, or longest run.
  // For now, we are not using filters at all, and compression suffers for it.
  encoder->rowbuf[0]=0x00;
  memcpy(encoder->rowbuf+1,src,encoder->image->stride);
  
  encoder->z.next_in=(Bytef*)encoder->rowbuf;
  encoder->z.avail_in=encoder->fstride;
  while (encoder->z.avail_in>0) {
    if (encoder_require(encoder->dst,256)<0) return -1;
    encoder->z.next_out=(Bytef*)encoder->dst->v+encoder->dst->c;
    encoder->z.avail_out=encoder->dst->a-encoder->dst->c;
    int ao0=encoder->z.avail_out;
    int err=deflate(&encoder->z,Z_NO_FLUSH);
    if (err<0) return -1;
    int addc=ao0-encoder->z.avail_out;
    encoder->dst->c+=addc;
  }
  
  return 0;
}

/* End of IDAT; drain compressor.
 */
 
static int png_encode_IDAT_finish(struct png_encoder *encoder) {
  while (1) {
    if (encoder_require(encoder->dst,256)<0) return -1;
    encoder->z.next_out=(Bytef*)encoder->dst->v+encoder->dst->c;
    encoder->z.avail_out=encoder->dst->a-encoder->dst->c;
    int ao0=encoder->z.avail_out;
    int err=deflate(&encoder->z,Z_FINISH);
    if (err<0) return -1;
    int addc=ao0-encoder->z.avail_out;
    encoder->dst->c+=addc;
    if (err==Z_STREAM_END) return 0;
  }
}

/* IDAT.
 */
 
static int png_encode_IDAT(struct encoder *dst,const struct png_image *image) {
  struct png_encoder encoder={
    .dst=dst,
    .image=image,
    .fstride=1+image->stride,
  };
  if (!(encoder.rowbuf=malloc(encoder.fstride))) return -1;
  
  if (deflateInit(&encoder.z,Z_BEST_COMPRESSION)<0) {
    free(encoder.rowbuf);
    return -1;
  }
  
  int hdrp=dst->c;
  const uint8_t *srcrow=image->pixels,*pvrow=0;
  int y=0;
  for (;y<image->h;y++,pvrow=srcrow,srcrow+=image->stride) {
    if (png_encode_IDAT_row(&encoder,srcrow,pvrow)<0) {
      png_encoder_cleanup(&encoder);
      return -1;
    }
  }
  if (png_encode_IDAT_finish(&encoder)<0) {
    png_encoder_cleanup(&encoder);
    return -1;
  }
  
  png_encoder_cleanup(&encoder);
  
  int len=dst->c-hdrp;
  uint8_t hdr[8]={len>>24,len>>16,len>>8,len,'I','D','A','T'};
  if (encoder_replace(dst,hdrp,0,hdr,sizeof(hdr))<0) return -1;
  
  int crc=crc32(0,(Bytef*)dst->v+hdrp+4,4+len);
  if (encode_intbe(dst,crc,4)<0) return -1;
  
  return 0;
}

/* IHDR.
 */
 
static int png_encode_IHDR(struct encoder *dst,const struct png_image *image) {
  if (encode_intbe(dst,13,4)<0) return -1;
  if (encode_raw(dst,"IHDR",4)<0) return -1;
  if (encode_intbe(dst,image->w,4)<0) return -1;
  if (encode_intbe(dst,image->h,4)<0) return -1;
  if (encode_intbe(dst,image->depth,1)<0) return -1;
  if (encode_intbe(dst,image->colortype,1)<0) return -1;
  if (encode_raw(dst,"\0\0\0",3)<0) return -1; // compression,filter,interlace
  int crc=crc32(0,(Bytef*)dst->v+dst->c-17,17);
  if (encode_intbe(dst,crc,4)<0) return -1;
  return 0;
}

/* Extra chunks, other than (IHDR,IDAT,IEND).
 */
 
static int png_encode_extras(struct encoder *dst,const struct png_image *image) {
  const struct png_chunk *chunk=image->chunkv;
  int i=image->chunkc;
  for (;i-->0;chunk++) {
    if (encode_intbe(dst,chunk->c,4)<0) return -1;
    if (encode_intbe(dst,chunk->id,4)<0) return -1;
    if (encode_raw(dst,chunk->v,chunk->c)<0) return -1;
    int crc=crc32(0,(Bytef*)dst->v+dst->c-chunk->c-4,chunk->c+4);
    if (encode_intbe(dst,crc,4)<0) return -1;
  }
  return 0;
}

/* Encode, main entry point.
 */
 
int png_encode(struct encoder *dst,const struct png_image *image) {
  if (!dst||!image) return -1;
  if (encode_raw(dst,"\x89PNG\r\n\x1a\n",8)<0) return -1;
  if (png_encode_IHDR(dst,image)<0) return -1;
  if (png_encode_extras(dst,image)<0) return -1;
  if (png_encode_IDAT(dst,image)<0) return -1;
  if (encode_raw(dst,"\0\0\0\0IEND\xae\x42\x60\x82",12)<0) return -1;
  return 0;
}

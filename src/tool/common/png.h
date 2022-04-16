/* png.h
 * PNG image file decoder and encoder.
 * Possible deviations from the spec:
 *  - Interlaced images not supported.
 *  - CRCs are not validated at decode.
 *  - PLTE not validated at decode -- Indexed images with no PLTE behave like gray.
 *  - Width in bits must be <=INT_MAX, and total image size in bytes too.
 *  - Conversion forces pixels thru 32-bit RGBA; fine information from 16-bit channels will be lost.
 *  - At decode, we tolerate ancillary chunks before IHDR. Only IDAT must come after IHDR.
 *  - At decode, we do not require IEND.
 *  - We don't fail on unknown critical chunks.
 */
 
#ifndef PNG_H
#define PNG_H

#include <stdint.h>

#define PNG_COLORTYPE_GRAY    0x00 /* 1 channel at (1,2,4,8,16) bits */
#define PNG_COLORTYPE_RGB     0x02 /* 3 channels at (8,16) bits */
#define PNG_COLORTYPE_INDEX   0x03 /* 1 channel at (1,2,4,8) bits */
#define PNG_COLORTYPE_GRAYA   0x04 /* 2 channels at (8,16) bits */
#define PNG_COLORTYPE_RGBA    0x06 /* 4 channels at (8,16) bits */

// case label does not reduce to an integer constant :(
//#define PNG_ID(str) ((str[0]<<24)|(str[1]<<16)|(str[2]<<8)|str[3])
#define PNG_ID(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|d)

struct png_decoder;
struct png_encoder;
struct png_image;
struct encoder;

/* Image object.
 ******************************************************/

struct png_image {
  int refc; // 0=immortal

  void *pixels;
  int stride; // bytes
  int pixelsize; // bits, full pixel ie 1..64
  
  // IHDR:
  int32_t w,h;
  uint8_t depth,colortype;
  
  // All chunks except IHDR,IDAT,IEND:
  struct png_chunk {
    uint32_t id; // big-endian
    void *v;
    int c;
  } *chunkv;
  int chunkc,chunka;
};

/* Make a static image by initializing to zero (in particular, (refc) must be zero).
 * You can safely (del) static images same as dynamic ones.
 * Cleanup zeroes the struct, so you won't accidentally do something else with it later.
 * Ref fails for static images.
 */
void png_image_cleanup(struct png_image *image);
void png_image_del(struct png_image *image);
int png_image_ref(struct png_image *image);
struct png_image *png_image_new();

/* Overwrite (dst) with a copy of (src) possibly converting format.
 * Extra chunks will not be copied. In particular, if you ask for INDEX you will *not* get a PLTE.
 */
int png_image_convert(
  struct png_image *dst,
  uint8_t depth,uint8_t colortype,
  const struct png_image *src
);

/* Free existing pixels and replace: pixels,stride,pixelsize,w,h,depth,colortype
 */
int png_image_allocate_pixels(
  struct png_image *image,
  int32_t w,int32_t h,
  uint8_t depth,uint8_t colortype
);

int png_image_add_chunk_handoff(struct png_image *image,uint32_t id,void *v,int c);
int png_image_add_chunk_copy(struct png_image *image,uint32_t id,const void *v,int c);

// Return WEAK the first chunk matching (id).
int png_image_get_chunk_by_id(void *dstpp,const struct png_image *image,uint32_t id);

// Full pixel size in bits (1..64), or zero if invalid.
int png_pixelsize_for_format(uint8_t depth,uint8_t colortype);

/* Helpers to read or write one pixel using normalized 32-bit RGBA.
 * Accessors exist for all valid formats.
 * INDEX behaves like GRAY (NB It does normalize values).
 * 16-bit channels work, but there can be some data loss.
 */
typedef uint32_t (*png_pxrd_fn)(const void *src,int x);
typedef void (*png_pxwr_fn)(void *dst,int x,uint32_t src);
png_pxrd_fn png_get_pxrd(uint8_t depth,uint8_t colortype);
png_pxwr_fn png_get_pxwr(uint8_t depth,uint8_t colortype);

/* Decode.
 *****************************************************/
 
// Convenience so you don't have to deal with a decoder, if you've got the full serial data.
struct png_image *png_decode(const void *src,int srcc);

/* NULL on success, otherwise a canned error message.
 * Valid PNG have an IEND chunk -- you can safely decode valid ones with (srcc==INT_MAX), length not necessary.
 * We do check:
 *  - Signature
 *  - Chunk framing
 *  - IHDR,IDAT,IEND present
 *  - IHDR at least 13 bytes
 * We could, but do not, check:
 *  - Chunk CRCs
 *  - PLTE for indexed pixels
 *  - Valid colortype, depth, dimensions
 *  - tRNS agrees with colortype
 */
const char *png_validate(const void *src,int srcc);
 
void png_decoder_del(struct png_decoder *decoder);
struct png_decoder *png_decoder_new();

/* Give some input to a decoder.
 * You can give it the whole file at once, or one byte at a time, or anything in between.
 * At each call to this function, we advance the decode process as far as possible.
 * This will always fail if we have ERROR status.
 */
int png_decoder_provide_input(struct png_decoder *decoder,const void *src,int srcc);

/* Image is accessible after we've processed the IHDR chunk.
 * Its pixels are incomplete until we process the last IDAT.
 * Returns a WEAK but retainable image.
 */
struct png_image *png_decoder_get_image(const struct png_decoder *decoder);

#define PNG_DECODER_ERROR      -1 /* Failed, no more progress possible. */
#define PNG_DECODER_IHDR        0 /* Initialized, awaiting IHDR. */
#define PNG_DECODER_IDAT        1 /* Awaiting or in the middle of IDATs. */
#define PNG_DECODER_IEND        2 /* Pixels complete, file not yet terminated. */
#define PNG_DECODER_COMPLETE    3 /* EOF */
int png_decoder_get_status(const struct png_decoder *decoder);
const char *png_decoder_get_error_message(const struct png_decoder *decoder);

/* Encode.
 *****************************************************/

/* Produces a full PNG file.
 * Any extra chunks in (image) are copied verbatim, between IHDR and IDAT.
 */
int png_encode(struct encoder *dst,const struct png_image *image);

#endif

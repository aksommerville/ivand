#include "decoder.h"
#include "serial.h"
#include <string.h>
#include <stdlib.h>

/* Binary scalars.
 */

int decode_intle(int *dst,struct decoder *decoder,int size) {
  int err=sr_intle_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp,size);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int decode_intbe(int *dst,struct decoder *decoder,int size) {
  int err=sr_intbe_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp,size);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int decode_vlq(int *dst,struct decoder *decoder) {
  int err=sr_vlq_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int decode_vlq5(int *dst,struct decoder *decoder) {
  int err=sr_vlq5_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int decode_utf8(int *dst,struct decoder *decoder) {
  int err=sr_utf8_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int decode_fixed(double *dst,struct decoder *decoder,int size,int fractbitc) {
  int err=sr_fixed_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp,size,fractbitc);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int decode_fixedf(float *dst,struct decoder *decoder,int size,int fractbitc) {
  int err=sr_fixedf_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp,size,fractbitc);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

/* Decode raw data.
 */

int decode_raw(void *dstpp,struct decoder *decoder,int len) {
  if (len<0) return -1;
  if (len>decoder_remaining(decoder)) return -1;
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

int decode_intlelen(void *dstpp,struct decoder *decoder,int size) {
  int p0=decoder->srcp,len;
  if (decode_intle(&len,decoder,size)<0) return -1;
  if (len>decoder_remaining(decoder)) { decoder->srcp=p0; return -1; }
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

int decode_intbelen(void *dstpp,struct decoder *decoder,int size) {
  int p0=decoder->srcp,len;
  if (decode_intbe(&len,decoder,size)<0) return -1;
  if (len>decoder_remaining(decoder)) { decoder->srcp=p0; return -1; }
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

int decode_vlqlen(void *dstpp,struct decoder *decoder) {
  int p0=decoder->srcp,len;
  if (decode_vlq(&len,decoder)<0) return -1;
  if (len>decoder_remaining(decoder)) { decoder->srcp=p0; return -1; }
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

int decode_vlq5len(void *dstpp,struct decoder *decoder) {
  int p0=decoder->srcp,len;
  if (decode_vlq5(&len,decoder)<0) return -1;
  if (len>decoder_remaining(decoder)) { decoder->srcp=p0; return -1; }
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

/* Signature assertion.
 */
 
int decode_assert(struct decoder *decoder,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (srcc>decoder_remaining(decoder)) return -1;
  if (memcmp((char*)decoder->src+decoder->srcp,src,srcc)) return -1;
  decoder->srcp+=srcc;
  return 0;
}

/* Line of text.
 */
 
int decode_line(void *dstpp,struct decoder *decoder) {
  const char *src=(char*)decoder->src+decoder->srcp;
  int srcc=decoder_remaining(decoder);
  int c=0;
  while (c<srcc) {
    if (src[c++]==0x0a) break;
  }
  decoder->srcp+=c;
  *(const void**)dstpp=src;
  return c;
}

/* Begin decoding a bit of JSON.
 * Asserts no sticky error, skips whitespace, asserts some content available.
 */
 
static int decode_json_prepare(struct decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  while ((decoder->srcp<decoder->srcc)&&(((unsigned char*)decoder->src)[decoder->srcp]<=0x20)) decoder->srcp++;
  if (decoder->srcp>=decoder->srcc) return decoder->jsonctx=-1;
  return 0;
}

/* Start JSON structure.
 */

int decode_json_object_start(struct decoder *decoder) {
  if (decode_json_prepare(decoder)<0) return -1;
  if (((char*)decoder->src)[decoder->srcp]!='{') return decoder->jsonctx=-1;
  int jsonctx=decoder->jsonctx;
  decoder->srcp++;
  decoder->jsonctx='{';
  return jsonctx;
}

int decode_json_array_start(struct decoder *decoder) {
  if (decode_json_prepare(decoder)<0) return -1;
  if (((char*)decoder->src)[decoder->srcp]!='[') return decoder->jsonctx=-1;
  int jsonctx=decoder->jsonctx;
  decoder->srcp++;
  decoder->jsonctx='[';
  return jsonctx;
}

/* End JSON structure.
 */
  
int decode_json_object_end(struct decoder *decoder,int jsonctx) {
  if (decoder->jsonctx!='{') return -1;
  if (decode_json_prepare(decoder)<0) return -1;
  if (((char*)decoder->src)[decoder->srcp]!='}') return decoder->jsonctx=-1;
  decoder->srcp++;
  decoder->jsonctx=jsonctx;
  return 0;
}

int decode_json_array_end(struct decoder *decoder,int jsonctx) {
  if (decoder->jsonctx!='[') return -1;
  if (decode_json_prepare(decoder)<0) return -1;
  if (((char*)decoder->src)[decoder->srcp]!=']') return decoder->jsonctx=-1;
  decoder->srcp++;
  decoder->jsonctx=jsonctx;
  return 0;
}

/* Next field in JSON structure.
 */
 
int decode_json_next(void *kpp,struct decoder *decoder) {

  // This block means we accept and ignore redundant commas, in violation of the spec. I feel that's ok.
  while (1) {
    if (decode_json_prepare(decoder)<0) return -1;
    if (((char*)decoder->src)[decoder->srcp]==',') decoder->srcp++;
    else break;
  }
  
  if (decoder->jsonctx=='{') {
    if (!kpp) return decoder->jsonctx=-1;
    if (((char*)decoder->src)[decoder->srcp]=='}') {
      return 0;
    }
    const char *k=(char*)decoder->src+decoder->srcp;
    int kc=sr_string_measure(k,decoder_remaining(decoder),0);
    if (kc<2) return decoder->jsonctx=-1;
    decoder->srcp+=kc;
    if (kc>2) { k+=1; kc-=2; } // drop quotes if not empty
    *(const char**)kpp=k;
    if (decode_json_prepare(decoder)<0) return -1;
    if (((char*)decoder->src)[decoder->srcp++]!=':') return decoder->jsonctx=-1;
    return kc;
    
  } else if (decoder->jsonctx=='[') {
    if (kpp) return decoder->jsonctx=-1;
    if (((char*)decoder->src)[decoder->srcp]==']') {
      return 0;
    }
    return 1;
    
  } else return decoder->jsonctx=-1;
  return 0;
}

/* JSON primitives.
 */

int decode_json_raw(void *dstpp,struct decoder *decoder) {
  if (decode_json_prepare(decoder)<0) return -1;
  const char *src=(char*)decoder->src+decoder->srcp;
  int c=sr_json_measure(src,decoder_remaining(decoder));
  if (c<1) return decoder->jsonctx=-1;
  decoder->srcp+=c;
  
  while (c&&((unsigned char)src[c-1]<=0x20)) c--;
  while (c&&((unsigned char)src[0]<=0x20)) { c--; src++; }
  if (dstpp) *(const char**)dstpp=src;
  return c;
}

int decode_json_skip(struct decoder *decoder) {
  return decode_json_raw(0,decoder);
}

int decode_json_save_for_later(struct decoder *later,struct decoder *decoder) {
  const char *src=0;
  int srcc=decode_json_raw(&src,decoder);
  if (srcc<1) return -1;
  later->src=src;
  later->srcc=srcc;
  later->srcp=0;
  later->jsonctx=0;
  return 0;
}

int decode_json_int(int *dst,struct decoder *decoder) {
  const char *src=0;
  int srcc=decode_json_raw(&src,decoder);
  if (srcc<0) return -1;
  if (sr_int_from_json(dst,src,srcc)<0) return decoder->jsonctx=-1;
  return 0;
}
  
int decode_json_float(double *dst,struct decoder *decoder) {
  const char *src=0;
  int srcc=decode_json_raw(&src,decoder);
  if (srcc<0) return -1;
  if (sr_float_from_json(dst,src,srcc)<0) return decoder->jsonctx=-1;
  return 0;
}

int decode_json_string(char *dst,int dsta,struct decoder *decoder) {
  int p0=decoder->srcp;
  const char *src=0;
  int srcc=decode_json_raw(&src,decoder);
  if (srcc<0) return -1;
  int dstc=sr_string_from_json(dst,dsta,src,srcc);
  if (dstc<0) return decoder->jsonctx=-1;
  if (dstc>dsta) decoder->srcp=p0;
  return dstc;
}

int decode_json_string_to_encoder(struct encoder *dst,struct decoder *decoder) {
  const char *src=0;
  int srcc=decode_json_raw(&src,decoder);
  if (srcc<0) return -1;
  while (1) {
    int err=sr_string_from_json(dst->v+dst->c,dst->a-dst->c,src,srcc);
    if (err<0) return decoder->jsonctx=-1;
    if (dst->c<=dst->a-err) {
      dst->c+=err;
      return 0;
    }
    if (encoder_require(dst,err)<0) return -1;
  }
}

char *decode_json_zstring(struct decoder *decoder) {
  const char *src=0;
  int srcc=decode_json_raw(&src,decoder);
  if (srcc<0) return 0;
  // No JSON string construction can make the output larger, so (srcc) is a perfect upper bound.
  char *dst=malloc(srcc);
  if (!dst) return 0;
  int dstc=sr_string_from_json(dst,srcc,src,srcc);
  if ((dstc<0)||(dstc>=srcc)) {
    free(dst);
    return 0;
  }
  dst[dstc]=0;
  while (dstc-->0) if (!dst[dstc]) dst[dstc]=0x20; // can't allow inner nuls
  return dst;
}

/* JSON peek.
 */

char decode_json_get_type(const struct decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  const char *src=decoder->src;
  src+=decoder->srcp;
  int srcc=decoder_remaining(decoder);
  while (srcc&&(((unsigned char)*src<=0x20)||(*src==','))) { src++; srcc--; }
  if (!srcc) return 0;
  switch (*src) {
    case '{': case '[': case '"': case 't': case 'f': case 'n': return *src;
    case '-': return '#';
  }
  if ((*src>='0')&&(*src<='9')) return '#';
  return 0;
}

/* Assert JSON completion.
 */
 
int decode_json_done(const struct decoder *decoder) {
  if (decoder->jsonctx) return -1;
  return 0;
}

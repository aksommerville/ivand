#include "decoder.h"
#include "serial.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

/* Cleanup.
 */

void encoder_cleanup(struct encoder *encoder) {
  if (encoder->v) free(encoder->v);
  memset(encoder,0,sizeof(struct encoder));
}

/* Grow buffer.
 */
 
int encoder_require(struct encoder *encoder,int addc) {
  if (addc<1) return 0;
  if (encoder->c<=encoder->a-addc) return 0;
  if (encoder->c>INT_MAX-addc) return -1;
  int na=encoder->c+addc;
  if (na<INT_MAX-256) na=(na+256)&~255;
  void *nv=realloc(encoder->v,na);
  if (!nv) return -1;
  encoder->v=nv;
  encoder->a=na;
  return 0;
}

/* Replace content.
 */

int encoder_replace(struct encoder *encoder,int p,int c,const void *src,int srcc) {
  if (p<0) p=encoder->c;
  if (c<0) c=encoder->c-p;
  if ((p<0)||(c<0)||(p>encoder->c-c)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (srcc!=c) {
    if (encoder_require(encoder,srcc-c)<0) return -1;
    memmove(encoder->v+p+srcc,encoder->v+p+c,encoder->c-c-p);
    encoder->c+=srcc-c;
  }
  memcpy(encoder->v+p,src,srcc);
  return 0;
}

/* Append chunks with optional length.
 */
 
int encode_null(struct encoder *encoder,int c) {
  if (c<1) return 0;
  if (encoder_require(encoder,c)<0) return -1;
  memset(encoder->v+encoder->c,0,c);
  encoder->c+=c;
  return 0;
}

int encode_raw(struct encoder *encoder,const void *src,int srcc) {
  return encoder_replace(encoder,encoder->c,0,src,srcc);
}

int encode_intlelen(struct encoder *encoder,const void *src,int srcc,int size) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (encode_intle(encoder,srcc,size)<0) return -1;
  if (encoder_replace(encoder,encoder->c,0,src,srcc)<0) return -1;
  return 0;
}

int encode_intbelen(struct encoder *encoder,const void *src,int srcc,int size) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (encode_intbe(encoder,srcc,size)<0) return -1;
  if (encoder_replace(encoder,encoder->c,0,src,srcc)<0) return -1;
  return 0;
}

int encode_vlqlen(struct encoder *encoder,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (encode_vlq(encoder,srcc)<0) return -1;
  if (encoder_replace(encoder,encoder->c,0,src,srcc)<0) return -1;
  return 0;
}

int encode_vlq5len(struct encoder *encoder,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (encode_vlq5(encoder,srcc)<0) return -1;
  if (encoder_replace(encoder,encoder->c,0,src,srcc)<0) return -1;
  return 0;
}

/* Insert length in the past.
 */

int encoder_insert_intlelen(struct encoder *encoder,int p,int size) {
  if ((p<0)||(p>encoder->c)) return -1;
  char tmp[5];
  int tmpc=sr_intle_encode(tmp,sizeof(tmp),encoder->c-p,size);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  return encoder_replace(encoder,p,0,tmp,tmpc);
}

int encoder_insert_intbelen(struct encoder *encoder,int p,int size) {
  if ((p<0)||(p>encoder->c)) return -1;
  char tmp[5];
  int tmpc=sr_intbe_encode(tmp,sizeof(tmp),encoder->c-p,size);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  return encoder_replace(encoder,p,0,tmp,tmpc);
}

int encoder_insert_vlqlen(struct encoder *encoder,int p) {
  if ((p<0)||(p>encoder->c)) return -1;
  char tmp[5];
  int tmpc=sr_vlq_encode(tmp,sizeof(tmp),encoder->c-p);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  return encoder_replace(encoder,p,0,tmp,tmpc);
}

int encoder_insert_vlq5len(struct encoder *encoder,int p) {
  if ((p<0)||(p>encoder->c)) return -1;
  char tmp[5];
  int tmpc=sr_vlq_encode(tmp,sizeof(tmp),encoder->c-p);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  return encoder_replace(encoder,p,0,tmp,tmpc);
}

/* Append formatted string.
 */

int encode_fmt(struct encoder *encoder,const char *fmt,...) {
  if (!fmt||!fmt[0]) return 0;
  while (1) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf(encoder->v+encoder->c,encoder->a-encoder->c,fmt,vargs);
    if ((err<0)||(err==INT_MAX)) return -1;
    if (encoder->c<encoder->a-err) { // sic < not <=
      encoder->c+=err;
      return err;
    }
    if (encoder_require(encoder,err+1)<0) return -1;
  }
}

/* Append scalars.
 */

int encode_intle(struct encoder *encoder,int src,int size) {
  if (encoder_require(encoder,4)<0) return -1;
  int err=sr_intle_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int encode_intbe(struct encoder *encoder,int src,int size) {
  if (encoder_require(encoder,4)<0) return -1;
  int err=sr_intbe_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int encode_vlq(struct encoder *encoder,int src) {
  if (encoder_require(encoder,4)<0) return -1;
  int err=sr_vlq_encode(encoder->v+encoder->c,encoder->a-encoder->c,src);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int encode_vlq5(struct encoder *encoder,int src) {
  if (encoder_require(encoder,5)<0) return -1;
  int err=sr_vlq5_encode(encoder->v+encoder->c,encoder->a-encoder->c,src);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int encode_utf8(struct encoder *encoder,int src) {
  if (encoder_require(encoder,4)<0) return -1;
  int err=sr_utf8_encode(encoder->v+encoder->c,encoder->a-encoder->c,src);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int encode_fixed(struct encoder *encoder,double src,int size,int fractbitc) {
  if (encoder_require(encoder,4)<0) return -1;
  int err=sr_fixed_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size,fractbitc);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int encode_fixedf(struct encoder *encoder,float src,int size,int fractbitc) {
  if (encoder_require(encoder,4)<0) return -1;
  int err=sr_fixedf_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size,fractbitc);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

/* Append conversions.
 */
 
int encode_base64(struct encoder *encoder,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  while (1) {
    int err=sr_base64_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,srcc);
    if (err<0) return -1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (encoder_require(encoder,err)<0) return -1;
  }
  return 0;
}

/* Append a JSON string, regardless of context.
 */
 
int encode_json_string_token(struct encoder *encoder,const char *src,int srcc) {
  while (1) {
    int err=sr_string_repr(encoder->v+encoder->c,encoder->a-encoder->c,src,srcc);
    if (err<0) return -1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (encoder_require(encoder,err)<0) return -1;
  }
}

/* Append a comma if we are inside a structure, and the last non-space character was not the opener.
 */
 
static int encode_json_comma_if_needed(struct encoder *encoder) {
  if ((encoder->jsonctx!='{')&&(encoder->jsonctx!='[')) return 0;
  int insp=encoder->c;
  while ((insp>0)&&((unsigned char)encoder->v[insp-1]<=0x20)) insp--;
  if (!insp) return -1; // No opener even? Something is fubar'd.
  if (encoder->v[insp-1]==encoder->jsonctx) return 0; // emitting first member, no comma
  return encoder_replace(encoder,insp,0,",",1);
}

/* Begin JSON expression. Check for errors, emit key, etc.
 */
 
static int encode_json_prepare(struct encoder *encoder,const char *k,int kc) {
  if (encoder->jsonctx<0) return -1;
  if (encoder->jsonctx=='{') {
    if (!k) return encoder->jsonctx=-1;
    if (encode_json_comma_if_needed(encoder)<0) return encoder->jsonctx=-1;
    if (encode_json_string_token(encoder,k,kc)<0) return encoder->jsonctx=-1;
    if (encode_raw(encoder,":",1)<0) return encoder->jsonctx=-1;
    return 0;
  }
  if (k) return encoder->jsonctx=-1;
  if (encoder->jsonctx=='[') {
    if (encode_json_comma_if_needed(encoder)<0) return encoder->jsonctx=-1;
  }
  return 0;
}

/* Start JSON structure.
 */

int encode_json_object_start(struct encoder *encoder,const char *k,int kc) {
  if (encode_json_prepare(encoder,k,kc)<0) return -1;
  if (encode_raw(encoder,"{",1)<0) return encoder->jsonctx=-1;
  int jsonctx=encoder->jsonctx;
  encoder->jsonctx='{';
  return jsonctx;
}

int encode_json_array_start(struct encoder *encoder,const char *k,int kc) {
  if (encode_json_prepare(encoder,k,kc)<0) return -1;
  if (encode_raw(encoder,"[",1)<0) return encoder->jsonctx=-1;
  int jsonctx=encoder->jsonctx;
  encoder->jsonctx='[';
  return jsonctx;
}

/* End JSON structure.
 */
 
int encode_json_object_end(struct encoder *encoder,int jsonctx) {
  if (encoder->jsonctx!='{') return encoder->jsonctx=-1;
  if (encode_raw(encoder,"}",1)<0) return encoder->jsonctx=-1;
  encoder->jsonctx=jsonctx;
  return 0;
}

int encode_json_array_end(struct encoder *encoder,int jsonctx) {
  if (encoder->jsonctx!='[') return encoder->jsonctx=-1;
  if (encode_raw(encoder,"]",1)<0) return encoder->jsonctx=-1;
  encoder->jsonctx=jsonctx;
  return 0;
}

/* JSON primitives.
 */

int encode_json_preencoded(struct encoder *encoder,const char *k,int kc,const char *v,int vc) {
  if (encode_json_prepare(encoder,k,kc)<0) return -1;
  if (encode_raw(encoder,v,vc)<0) return encoder->jsonctx=-1;
  return 0;
}

int encode_json_null(struct encoder *encoder,const char *k,int kc) {
  return encode_json_preencoded(encoder,k,kc,"null",4);
}

int encode_json_boolean(struct encoder *encoder,const char *k,int kc,int v) {
  return encode_json_preencoded(encoder,k,kc,v?"true":"false",-1);
}

int encode_json_int(struct encoder *encoder,const char *k,int kc,int v) {
  if (encode_json_prepare(encoder,k,kc)<0) return -1;
  while (1) {
    int err=sr_decsint_repr(encoder->v+encoder->c,encoder->a-encoder->c,v);
    if (err<0) return encoder->jsonctx=-1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (encoder_require(encoder,err)<0) return -1;
  }
}

int encode_json_float(struct encoder *encoder,const char *k,int kc,double v) {
  if (encode_json_prepare(encoder,k,kc)<0) return -1;
  while (1) {
    int err=sr_float_repr(encoder->v+encoder->c,encoder->a-encoder->c,v);
    if (err<0) return encoder->jsonctx=-1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (encoder_require(encoder,err)<0) return -1;
  }
}

int encode_json_string(struct encoder *encoder,const char *k,int kc,const char *v,int vc) {
  if (encode_json_prepare(encoder,k,kc)<0) return -1;
  if (encode_json_string_token(encoder,v,vc)<0) return encoder->jsonctx=-1;
  return 0;
}

/* Assert JSON complete.
 */

int encode_json_done(const struct encoder *encoder) {
  if (encoder->jsonctx) return -1;
  return 0;
}

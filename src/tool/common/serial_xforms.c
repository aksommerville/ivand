#include "serial.h"
#include <stdint.h>

/* Encode Base64.
 */
 
static const char *sr_base64_alphabet=
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789+/"
;
 
int sr_base64_encode(char *dst,int dsta,const void *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  const uint8_t *SRC=src;
  int dstc=0;
  for (;srcc>=3;SRC+=3,srcc-=3) {
    if (dstc<=dsta-4) {
      dst[dstc++]=sr_base64_alphabet[SRC[0]>>2];
      dst[dstc++]=sr_base64_alphabet[((SRC[0]<<4)|(SRC[1]>>4))&0x3f];
      dst[dstc++]=sr_base64_alphabet[((SRC[1]<<2)|(SRC[2]>>6))&0x3f];
      dst[dstc++]=sr_base64_alphabet[SRC[2]&0x3f];
    } else dstc+=4;
  }
  switch (srcc) {
    case 0: break;
    case 1: {
        if (dstc<=dsta-4) {
          dst[dstc++]=sr_base64_alphabet[SRC[0]>>2];
          dst[dstc++]=sr_base64_alphabet[(SRC[0]<<4)&0x3f];
          dst[dstc++]='=';
          dst[dstc++]='=';
        } else dstc+=4;
      } break;
    case 2: {
        if (dstc<=dsta-4) {
          dst[dstc++]=sr_base64_alphabet[SRC[0]>>2];
          dst[dstc++]=sr_base64_alphabet[((SRC[0]<<4)|(SRC[1]>>4))&0x3f];
          dst[dstc++]=sr_base64_alphabet[(SRC[1]<<2)&0x3f];
          dst[dstc++]='=';
        } else dstc+=4;
      } break;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Decode Base64.
 */
 
int sr_base64_decode(void *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  uint8_t *DST=dst;
  uint8_t tmp[4]; // four of 0..63
  int dstc=0,tmpc=0;
  for (;srcc-->0;src++) {
         if ((*src>='A')&&(*src<='Z')) tmp[tmpc++]=*src-'A';
    else if ((*src>='a')&&(*src<='z')) tmp[tmpc++]=*src-'a'+26;
    else if ((*src>='0')&&(*src<='9')) tmp[tmpc++]=*src-'0'+52;
    else if (*src=='+') tmp[tmpc++]=62;
    else if (*src=='/') tmp[tmpc++]=63;
    else if (*src=='=') continue;
    else if ((unsigned char)(*src)<=0x20) continue;
    else return -1; // Not [A-Za-z0-9+/=] or whitespace, error.
    if (tmpc>=4) {
      if (dstc<=dsta-3) {
        DST[dstc++]=(tmp[0]<<2)|(tmp[1]>>4);
        DST[dstc++]=(tmp[1]<<4)|(tmp[2]>>2);
        DST[dstc++]=(tmp[2]<<6)|tmp[3];
      } else dstc+=3;
      tmpc=0;
    }
  }
  switch (tmpc) {
    case 0: break; // landed on a group boundary, cool.
    case 1: tmp[1]=0; // 1 byte leftover is not legal, but we'll treat it as 2, with the second value zero.
    case 2: {
        if (dstc<=dsta-1) {
          DST[dstc++]=(tmp[0]<<2)|(tmp[1]>>4);
        } else dstc+=1;
      } break;
    case 3: {
        if (dstc<=dsta-2) {
          DST[dstc++]=(tmp[0]<<2)|(tmp[1]>>4);
          DST[dstc++]=(tmp[1]<<4)|(tmp[2]>>2);
        } else dstc+=2;
      } break;
  }
  if (dstc<dsta) DST[dstc]=0;
  return dstc;
}

/* URL-Encode.
 */
 
int sr_urlencode_encode(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0;
  for (;srcc-->0;src++) {
    // I've never read the spec, instead did this in Firefox:
    //   for (let i=0x20;i<0x7f;i++) { const s=String.fromCharCode(i); if (encodeURIComponent(s).length===1) console.log(s); }
    if (
      ((*src>='a')&&(*src<='z'))||
      ((*src>='A')&&(*src<='Z'))||
      ((*src>='0')&&(*src<='9'))||
      (*src=='~')||
      (*src=='-')||
      (*src=='_')||
      (*src=='.')||
      (*src=='*')||
      (*src=='(')||
      (*src==')')||
      (*src=='\'')||
      (*src=='!')
    ) {
      if (dstc<dsta) dst[dstc]=*src;
      dstc++;
    } else {
      if (dstc<=dsta-3) {
        dst[dstc++]='%';
        dst[dstc++]=sr_hexdigit_repr((*src)>>4);
        dst[dstc++]=sr_hexdigit_repr((*src)&15);
      } else dstc+=3;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* URL-Decode.
 */

int sr_urlencode_decode(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if ((src[srcp]=='%')&&(srcp<=srcc-3)) {
      int hi=sr_digit_eval(src[srcp+1]);
      int lo=sr_digit_eval(src[srcp+2]);
      if ((hi>=0)&&(lo>=0)&&(hi<16)&&(lo<16)) {
        if (dstc<dsta) dst[dstc]=(hi<<4)|lo;
        dstc++;
        srcp+=3;
        continue;
      }
    }
    if (dstc<dsta) dst[dstc]=src[srcp];
    dstc++;
    srcp++;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Hex string.
 */

int sr_hexstring_encode(char *dst,int dsta,const void *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  const uint8_t *SRC=src;
  int dstc=0;
  for (;srcc-->0;SRC++) {
    if (dstc<=dsta-2) {
      dst[dstc++]=sr_hexdigit_repr((*SRC)>>4);
      dst[dstc++]=sr_hexdigit_repr(*SRC);
    } else dstc+=2;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int sr_hexstring_decode(void *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  uint8_t *DST=dst;
  int dstc=0,hi=-1;
  for (;srcc-->0;src++) {
    if ((unsigned char)*src<=0x20) continue;
    int digit=sr_digit_eval(*src);
    if ((digit<0)||(digit>15)) return -1;
    if (hi<0) {
      hi=digit;
    } else {
      if (dstc<dsta) DST[dstc]=(hi<<4)|digit;
      dstc++;
      hi=-1;
    }
  }
  if (hi>=0) {
    if (dstc<dsta) DST[dstc]=hi<<4;
    dstc++;
  }
  if (dstc<dsta) DST[dstc]=0;
  return dstc;
}

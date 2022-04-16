#include "serial.h"
#include <limits.h>
#include <string.h>
#include <stdint.h>

/* Plain integers.
 */
 
int sr_intbe_decode(int *dst,const void *src,int srcc,int size) {
  int asize=size;
  if (asize<0) asize=-asize;
  if ((asize<1)||(asize>4)) return -1;
  if (asize>srcc) return -1;
  const uint8_t *SRC=src;
  int mask;
  switch (asize) {
    case 1: mask=0xffffff80; *dst=SRC[0]; break;
    case 2: mask=0xffff8000; *dst=(SRC[0]<<8)|SRC[1]; break;
    case 3: mask=0xff800000; *dst=(SRC[0]<<16)|(SRC[1]<<8)|SRC[2]; break;
    case 4: mask=0x80000000; *dst=(SRC[0]<<24)|(SRC[1]<<16)|(SRC[2]<<8)|SRC[3]; break;
  }
  if ((size<0)&&((*dst)&mask)) (*dst)|=mask;
  return asize;
}

int sr_intle_decode(int *dst,const void *src,int srcc,int size) {
  int asize=size;
  if (asize<0) asize=-asize;
  if ((asize<1)||(asize>4)) return -1;
  if (asize>srcc) return -1;
  const uint8_t *SRC=src;
  int mask;
  switch (asize) {
    case 1: mask=0xffffff80; *dst=SRC[0]; break;
    case 2: mask=0xffff8000; *dst=SRC[0]|(SRC[1]<<8); break;
    case 3: mask=0xff800000; *dst=SRC[0]|(SRC[1]<<8)|(SRC[2]<<16); break;
    case 4: mask=0x80000000; *dst=SRC[0]|(SRC[1]<<8)|(SRC[2]<<16)|(SRC[3]<<24); break;
  }
  if ((size<0)&&((*dst)&mask)) (*dst)|=mask;
  return asize;
}

int sr_intle_encode(void *dst,int dsta,int src,int size) {
  if (size<0) size=-size;
  if ((size<1)||(size>4)) return -1;
  uint8_t *DST=dst;
  if (size<dsta) switch (size) {
    case 1: DST[0]=src; break;
    case 2: DST[0]=src; DST[1]=src>>8; break;
    case 3: DST[0]=src; DST[1]=src>>8; DST[2]=src>>16; break;
    case 4: DST[0]=src; DST[1]=src>>8; DST[2]=src>>16; DST[3]=src>>24; break;
  }
  return size;
}

int sr_intbe_encode(void *dst,int dsta,int src,int size) {
  if (size<0) size=-size;
  if ((size<1)||(size>4)) return -1;
  uint8_t *DST=dst;
  if (size<dsta) switch (size) {
    case 1: DST[0]=src; break;
    case 2: DST[0]=src>>8; DST[1]=src; break;
    case 3: DST[0]=src>>16; DST[1]=src>>8; DST[2]=src; break;
    case 4: DST[0]=src>>24; DST[1]=src>>16; DST[2]=src>>8; DST[3]=src; break;
  }
  return size;
}

/* VLQ.
 */

int sr_vlq_decode(int *dst,const void *src,int srcc) {
  const uint8_t *SRC=src;
  if (srcc<1) return -1;
  if (!(SRC[0]&0x80)) {
    *dst=SRC[0];
    return 1;
  }
  if (srcc<2) return -1;
  if (!(SRC[1]&0x80)) {
    *dst=((SRC[0]&0x7f)<<7)|SRC[1];
    return 2;
  }
  if (srcc<3) return -1;
  if (!(SRC[2]&0x80)) {
    *dst=((SRC[0]&0x7f)<<14)|((SRC[1]&0x7f)<<7)|SRC[2];
    return 3;
  }
  if (srcc<4) return -1;
  if (!(SRC[3]&0x80)) {
    *dst=((SRC[0]&0x7f)<<21)|((SRC[1]&0x7f)<<14)|((SRC[2]&0x7f)<<7)|SRC[3];
    return 4;
  }
  return -1;
}

int sr_vlq5_decode(int *dst,const void *src,int srcc) {
  const uint8_t *SRC=src;
  if (srcc<1) return -1;
  if (!(SRC[0]&0x80)) {
    *dst=SRC[0];
    return 1;
  }
  if (srcc<2) return -1;
  if (!(SRC[1]&0x80)) {
    *dst=((SRC[0]&0x7f)<<7)|SRC[1];
    return 2;
  }
  if (srcc<3) return -1;
  if (!(SRC[2]&0x80)) {
    *dst=((SRC[0]&0x7f)<<14)|((SRC[1]&0x7f)<<7)|SRC[2];
    return 3;
  }
  if (srcc<4) return -1;
  if (!(SRC[3]&0x80)) {
    *dst=((SRC[0]&0x7f)<<21)|((SRC[1]&0x7f)<<14)|((SRC[2]&0x7f)<<7)|SRC[3];
    return 4;
  }
  if (srcc<5) return -1;
  if (!(SRC[4]&0x80)) {
    *dst=((SRC[0]&0x7f)<<28)|((SRC[1]&0x7f)<<21)|((SRC[2]&0x7f)<<14)||((SRC[3]&0x7f)<<7)|SRC[4];
    return 5;
  }
  return -1;
}

int sr_vlq_encode(void *dst,int dsta,int src) {
  uint8_t *DST=dst;
  if (src<0) return -1;
  if (src<0x80) {
    if (dsta>=1) {
      DST[0]=src;
    }
    return 1;
  }
  if (src<0x4000) {
    if (dsta>=2) {
      DST[0]=0x80|(src>>7);
      DST[1]=(src&0x7f);
    }
    return 2;
  }
  if (src<0x200000) {
    if (dsta>=3) {
      DST[0]=0x80|(src>>14);
      DST[1]=0x80|(src>>7);
      DST[2]=(src&0x7f);
    }
    return 3;
  }
  if (src<0x10000000) {
    if (dsta>=4) {
      DST[0]=0x80|(src>>21);
      DST[1]=0x80|(src>>14);
      DST[2]=0x80|(src>>7);
      DST[3]=(src&0x7f);
    }
    return 4;
  }
  return -1;
}

int sr_vlq5_encode(void *dst,int dsta,int src) {
  uint8_t *DST=dst;
  if (src&0xf0000000) {
    if (dsta>=5) {
      DST[0]=0x80|(src>>28);
      DST[1]=0x80|(src>>21);
      DST[2]=0x80|(src>>14);
      DST[3]=0x80|(src>>7);
      DST[4]=(src&0x7f);
    }
    return 5;
  }
  if (src&0xffe00000) {
    if (dsta>=4) {
      DST[0]=0x80|(src>>21);
      DST[1]=0x80|(src>>14);
      DST[2]=0x80|(src>>7);
      DST[3]=(src&0x7f);
    }
    return 4;
  }
  if (src&0xffffc000) {
    if (dsta>=3) {
      DST[0]=0x80|(src>>14);
      DST[1]=0x80|(src>>7);
      DST[2]=(src&0x7f);
    }
    return 3;
  }
  if (src&0xffffff80) {
    if (dsta>=2) {
      DST[0]=0x80|(src>>7);
      DST[1]=(src&0x7f);
    }
    return 2;
  }
  if (dsta>=1) {
    DST[0]=src;
  }
  return 1;
}

/* UTF-8.
 */

int sr_utf8_decode(int *dst,const void *src,int srcc) {
  if (srcc<1) return -1;
  const uint8_t *SRC=src;
  if (!(SRC[0]&0x80)) {
    *dst=SRC[0];
    return 1;
  }
  if (!(SRC[0]&0x40)) return -1;
  if (!(SRC[0]&0x20)) {
    if (srcc<2) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    *dst=((SRC[0]&0x1f)<<6)|(SRC[1]&0x3f);
    return 2;
  }
  if (!(SRC[0]&0x10)) {
    if (srcc<3) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    *dst=((SRC[0]&0x0f)<<12)|((SRC[1]&0x3f)<<6)|(SRC[2]&0x3f);
    return 3;
  }
  if (!(SRC[0]&0x08)) {
    if (srcc<4) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    if ((SRC[3]&0xc0)!=0x80) return -1;
    *dst=((SRC[0]&0x07)<<18)|((SRC[1]&0x3f)<<12)|((SRC[2]&0x3f)<<6)|(SRC[3]&0x3f);
    return 4;
  }
  return -1;
}

int sr_utf8_encode(void *dst,int dsta,int src) {
  if (src<0) return -1;
  uint8_t *DST=dst;
  if (src<0x80) {
    if (dsta>=1) {
      DST[0]=src;
    }
    return 1;
  }
  if (src<0x800) {
    if (dsta>=2) {
      DST[0]=0xc0|(src>>6);
      DST[1]=0x80|(src&0x3f);
    }
    return 2;
  }
  if (src<0x10000) {
    if (dsta>=3) {
      DST[0]=0xe0|(src>>12);
      DST[1]=0x80|((src>>6)&0x3f);
      DST[2]=0x80|(src&0x3f);
    }
    return 3;
  }
  if (src<0x110000) {
    if (dsta>=4) {
      DST[0]=0xf0|(src>>18);
      DST[1]=0x80|((src>>12)&0x3f);
      DST[2]=0x80|((src>>6)&0x3f);
      DST[3]=0x80|(src&0x3f);
    }
    return 4;
  }
  return -1;
}

/* Fixed-point numbers.
 */
 
int sr_fixed_decode(double *dst,const void *src,int srcc,int size,int fractbitc) {
  int sign=0,asize=size;
  if (size<0) {
    asize=-asize;
    sign=1;
  }
  if ((asize<1)||(asize>4)) return -1;
  if ((fractbitc<0)||(fractbitc>asize<<3)) return -1;
  int32_t i;
  int err=sr_intbe_decode(&i,src,srcc,size);
  if (err<0) return -1;
  *dst=(i/(double)(1<<fractbitc));
  return err;
}

int sr_fixedf_decode(float *dst,const void *src,int srcc,int size,int fractbitc) {
  double d;
  int err=sr_fixed_decode(&d,src,srcc,size,fractbitc);
  if (err>=0) *dst=(float)d;
  return err;
}

int sr_fixed_encode(void *dst,int dsta,double src,int size,int fractbitc) {
  int sign=0,asize=size;
  if (size<0) {
    asize=-asize;
    sign=1;
  }
  if ((asize<1)||(asize>4)) return -1;
  if ((fractbitc<0)||(fractbitc>asize<<3)) return -1;
  uint32_t i=src*(1u<<fractbitc);
  return sr_intbe_encode(dst,dsta,i,size);
}

int sr_fixedf_encode(void *dst,int dsta,float src,int size,int fractbitc) {
  return sr_fixed_encode(dst,dsta,(double)src,size,fractbitc);
}

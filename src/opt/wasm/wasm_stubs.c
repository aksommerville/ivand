#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>

FILE *const stderr=0;

int fprintf(FILE *f,const char *fmt,...) {
  // Only used for debugging.
  return 0;
}

void *memcpy(void *dst,const void *src,size_t c) {
  uint8_t *DST=dst;
  const uint8_t *SRC=src;
  for (;c-->0;DST++,SRC++) *DST=*SRC;
  return dst;
}

void *memmove(void *dst,const void *src,size_t c) {
  uint8_t *DST=dst;
  const uint8_t *SRC=src;
  if (DST<=SRC) {
    for (;c-->0;DST++,SRC++) *DST=*SRC;
  } else {
    while (c-->0) DST[c]=SRC[c];
  }
  return dst;
}

void *memset(void *v,int src,unsigned long c) {
  uint8_t *dst=v;
  for (;c-->0;dst++) *dst=src;
  return v;
}

int snprintf(char *dst,size_t dsta,const char *fmt,...) {
  int dstc=0;
  va_list vargs;
  va_start(vargs,fmt);
  while (*fmt) {
    if (*fmt=='%') {
      fmt++;
      if (*fmt=='%') {
        if (dstc<dsta) dst[dstc]='%';
        dstc++;
        fmt++;
      } else {
      
        // The only format unit we'll see is "%d", possibly with length and padding.
        char pad=' ';
        int minlen=0;
        if (*fmt=='0') { pad='0'; fmt++; }
        while ((*fmt>='0')&&(*fmt<='9')) {
          minlen*=10;
          minlen+=(*fmt)-'0';
          fmt++;
        }
        if (*fmt=='d') {
          fmt++;
        
          unsigned int v=va_arg(vargs,unsigned int);
          unsigned int limit=10;
          int digitc=1;
          while (v>=limit) { digitc++; if (limit>UINT_MAX/10) break; limit*=10; }
          int padc=minlen-digitc;
          if (padc>0) {
            if (dstc<=dsta-padc) memset(dst+dstc,pad,padc);
            dstc+=padc;
          }
          if (dstc<=dsta-digitc) {
            int i=digitc;
            for (;i-->0;v/=10) dst[dstc+i]='0'+v%10;
          }
          dstc+=digitc;
        
        } else {
          // oops
        }
      }
    } else {
      if (dstc<dsta) dst[dstc]=*fmt;
      dstc++;
      fmt++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  else if (dstc>0) dst[dsta-1]=0; // behave like standard snprintf re termination
  return dstc;
}

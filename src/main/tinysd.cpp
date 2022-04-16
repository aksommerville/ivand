#include <SD.h>
#include "tinysd.h"

static uint8_t sdinit=0;

static int8_t tinysd_require() {
  if (!sdinit) {
    if (SD.begin()<0) return -1;
    sdinit=1;
  }
  return 0;
}

int32_t tinysd_read(void *dst,int32_t dsta,const char *path) {
  if (tinysd_require()<0) return -1;
  File file=SD.open(path);
  if (!file) return -1;
  int32_t dstc=file.read(dst,dsta);
  file.close();
  return dstc;
}

int32_t tinysd_write(const char *path,const void *src,int32_t srcc) {
  if (tinysd_require()<0) return -1;
  File file=SD.open(path,O_RDWR|O_CREAT);
  if (!file) return -1;
  size_t err=file.write((const uint8_t*)src,(size_t)srcc);
  file.close();
  if (err<0) return -1;
  return 0;
}

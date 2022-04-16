#include "tiny_internal.h"
#include <SD.h>

/* Globals.
 */
 
static uint8_t sdinit=0;

/* Initialize if necessary.
 */
 
static int8_t ma_sd_require() {
  if (!sdinit) {
    if (SD.begin()<0) return -1;
    sdinit=1;
  }
  return 0;
}

/* Read file.
 */
 
int32_t ma_file_read(void *dst,int32_t dsta,const char *path,int32_t seek) {
  if (dsta>0xffff) dsta=0xffff;
  if (ma_sd_require()<0) return -1;
  File file=SD.open(path);
  if (!file) return -1;
  if (seek&&(file.seek(seek)!=seek)) {
    file.close();
    return 0;
  }
  int32_t dstc=file.read(dst,dsta);
  file.close();
  return dstc;
}

/* Write file.
 */
 
int32_t ma_file_write(const char *path,const void *src,int32_t srcc,int32_t seek) {
  if (srcc<0) return -1;
  if (ma_sd_require()<0) return -1;
  File file=SD.open(path,O_RDWR|O_CREAT);
  if (!file) return -1;
  
  if (seek) {
    int32_t p=file.seek(seek);
    if (p<0) {
      file.close();
      return -1;
    }
    while (p<seek) {
      uint8_t buf[32]={0};
      int c=seek-p;
      if (c>sizeof(buf)) c=sizeof(buf);
      if (file.write(buf,c)!=c) {
        file.close();
        return -1;
      }
      p+=c;
    }
  }
  
  size_t err=file.write((const uint8_t*)src,(size_t)srcc);
  file.close();
  if (err<0) return -1;
  return 0;
}

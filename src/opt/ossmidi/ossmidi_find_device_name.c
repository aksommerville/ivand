#include "ossmidi.h"
#include "tool/common/fs.h"
#include "tool/common/decoder.h"
#include <stdlib.h>
#include <string.h>

/* Find device name.
 */
 
int ossmidi_find_device_name(char *dst,int dsta,int devid,const char *path) {
  if (!path||!path[0]) {
    int dstc=0;
    if ((dstc=ossmidi_find_device_name(dst,dsta,devid,"/proc/asound/oss/sndstat"))>=0) return dstc;
    if ((dstc=ossmidi_find_device_name(dst,dsta,devid,"/dev/sndstat"))>=0) return dstc;
    return -1;
  }
  char *src=0;
  int srcc=file_read(&src,path);
  if ((srcc<0)||!src) return -1;
  int readingdevices=0;
  struct decoder decoder={.src=src,.srcc=srcc};
  const char *line;
  int linec;
  while ((linec=decode_line(&line,&decoder))>0) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    
    if (readingdevices) {
      if (!linec) break; // end of "Midi devices:" block.
      int linedevid=0,linep=0;
      while ((linep<linec)&&(line[linep]>='0')&&(line[linep]<='9')) {
        linedevid*=10;
        linedevid+=line[linep]-'0';
        linep++;
      }
      if (!linep) break; // unexpected line in "Midi devices:" block, stop reading.
      if (linedevid==devid) {
        if ((linep<linec)&&(line[linep]==':')) linep++;
        while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
        int dstc=linec-linep;
        if (dstc<=dsta) {
          memcpy(dst,line+linep,dstc);
          if (dstc<dsta) dst[dstc]=0;
        }
        return dstc;
      }
      
    } else if ((linec==13)&&!memcmp(line,"Midi devices:",13)) {
      readingdevices=1;
    }
  }
  free(src);
  return -1;
}

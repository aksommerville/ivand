#include "http.h"
#include "tool/common/serial.h"
#include <string.h>
#include <stdint.h>

/* Method.
 */
 
int http_method_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!sr_memcasecmp(src,#tag,srcc)) return HTTP_METHOD_##tag;
    _(GET)
    _(POST)
    _(PUT)
    _(DELETE)
    _(PATCH)
    _(HEAD)
    _(OPTIONS)
    _(CONNECT)
  #undef _
  return -1;
}

const char *http_method_repr(int method) {
  switch (method) {
    #define _(tag) case HTTP_METHOD_##tag: return #tag;
    _(GET)
    _(POST)
    _(PUT)
    _(DELETE)
    _(PATCH)
    _(HEAD)
    _(OPTIONS)
    _(CONNECT)
    #undef _
  }
  return 0;
}

int http_method_expects_body(int method) {
  switch (method) {
    case HTTP_METHOD_GET:
    case HTTP_METHOD_DELETE:
    case HTTP_METHOD_OPTIONS:
      return 0;
  }
  return 1;
}

/* Content-Type from lowercase suffix, null if unknown.
 */
 
static const char *http_content_type_from_suffix(const char *src,int srcc) {
  switch (srcc) {
    case 1: switch (src[0]) {
      } break;
    case 2: {
        if (!memcmp(src,"js",2)) return "application/javascript";
      } break;
    case 3: {
        if (!memcmp(src,"css",3)) return "text/css";
        if (!memcmp(src,"xml",3)) return "application/xml";
        if (!memcmp(src,"png",3)) return "image/png";
        if (!memcmp(src,"gif",3)) return "image/gif";
        if (!memcmp(src,"mid",3)) return "audio/midi";
        if (!memcmp(src,"wav",3)) return "audio/wave";
        if (!memcmp(src,"txt",3)) return "text/plain";
        if (!memcmp(src,"htm",3)) return "text/html";
        if (!memcmp(src,"jpg",3)) return "image/jpeg";
      } break;
    case 4: {
        if (!memcmp(src,"html",4)) return "text/html";
        if (!memcmp(src,"jpeg",4)) return "image/jpeg";
        if (!memcmp(src,"json",4)) return "application/json";
        if (!memcmp(src,"wasm",4)) return "application/wasm";
      } break;
  }
  return 0;
}

/* Guess content type.
 */
 
const char *http_guess_content_type(const char *path,int pathc,const void *src,int srcc) {
  
  // Blindly trust the path suffix if present.
  if (path) {
    if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
    const char *sfxsrc=0;
    int sfxc=0,pathp=0;
    for (;pathp<pathc;pathp++) {
      if (path[pathp]=='/') {
        sfxsrc=0;
        sfxc=0;
      } else if (path[pathp]=='.') {
        sfxsrc=path+pathp+1;
        sfxc=0;
      } else if (sfxsrc) {
        sfxc++;
      }
    }
    if ((sfxc>0)&&(sfxc<=8)) {
      char sfx[8];
      int i=sfxc; while (i-->0) {
        if ((sfxsrc[i]>='A')&&(sfxsrc[i]<='Z')) sfx[i]=sfxsrc[i]+0x20;
        else sfx[i]=sfxsrc[i];
      }
      const char *type=http_content_type_from_suffix(sfx,sfxc);
      if (type) return type;
    }
  }
  
  // Empty content is presumed binary.
  if (!src) return "application/octet-stream";
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (!srcc) return "application/octet-stream";
  
  // Look for definite signatures.
  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return "image/png";
  if ((srcc>=6)&&!memcmp(src,"GIF87a",6)) return "image/gif";
  if ((srcc>=6)&&!memcmp(src,"GIF89a",6)) return "image/gif";
  if ((srcc>=8)&&!memcmp(src,"MThd\0\0\0",7)) return "audio/midi";
  if ((srcc>=4)&&!memcmp(src,"\0asm",4)) return "application/wasm";
  
  // If it doesn't look like text at this point, call it generic binary.
  int text=1;
  int ckc=srcc;
  if (ckc>256) ckc=256;
  while (ckc-->0) {
    uint8_t b=((uint8_t*)src)[ckc];
    if (b>=0xf8) { text=0; break; } // high five set is never legal in utf-8
    if (b==0x09) continue;
    if (b==0x0a) continue;
    if (b==0x0d) continue;
    if (b<0x20) { text=0; break; } // below G0 and not HT,LF,CR
  }
  if (!text) return "application/octet-stream";
  
  // Fuzzier text guessing.
  const char *SRC=src;
  if ((srcc>=15)&&!memcmp(src,"<!DOCTYPE html>",15)) return "text/html";
  if ((srcc>=5)&&!memcmp(src,"<?xml",5)) return "application/xml";
  if ((srcc>=1)&&(SRC[0]=='{')) return "application/json";
  
  // OK I don't know. Generic text.
  return "text/plain";
}

/* Split URL.
 */
 
int http_url_split(struct http_url *url,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  memset(url,0,sizeof(struct http_url));
  int srcp=0;
  
  // Scheme.
  if ((srcp<srcc)&&(src[srcp]!='/')&&(src[srcp]!='#')) {
    url->scheme=src+srcp;
    while ((srcp<srcc)&&(src[srcp]!=':')) { srcp++; url->schemec++; }
    if ((srcp>=srcc)||(src[srcp++]!=':')) return -1;
  }
  
  // Host.
  if ((srcp<=srcc-2)&&(src[srcp]=='/')&&(src[srcp+1]=='/')) {
    srcp+=2;
    url->host=src+srcp;
    while ((srcp<srcc)&&(src[srcp]!=':')&&(src[srcp]!='/')) { srcp++; url->hostc++; }
    // Port.
    if ((srcp<srcc)&&(src[srcp]==':')) {
      srcp++;
      while ((srcp<srcc)&&(src[srcp]!='/')) {
        int digit=src[srcp++]-'0';
        if ((digit<0)||(digit>9)) return -1;
        url->port*=10;
        url->port+=digit;
        if (url->port>0xffff) return -1;
      }
    }
  }
  
  // Port from scheme if it wasn't provided.
  if (!url->port) {
    if ((url->schemec==4)&&!memcmp(url->scheme,"http",4)) url->port=80;
    else if ((url->schemec==5)&&!memcmp(url->scheme,"https",5)) url->port=443;
  }
  
  // Path.
  if ((srcp<srcc)&&(src[srcp]=='/')) {
    url->path=src+srcp;
    while ((srcp<srcc)&&(src[srcp]!='#')) { srcp++; url->pathc++; }
  }
  
  // Fragment.
  if ((srcp<srcc)&&(src[srcp]=='#')) {
    url->fragment=src+srcp;
    url->fragmentc=srcc-srcp;
    srcp=srcc;
  }
  
  if (srcp<srcc) return -1;
  return 0;
}

/* Measure line with terminator.
 */
 
int http_measure_line(const char *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (src[srcp++]==0x0a) return srcp;
  }
  return 0;
}

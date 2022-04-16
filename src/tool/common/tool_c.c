#include "tool/common/tool_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Preamble.
 */
 
int tool_generate_c_preamble(struct tool *tool) {
  if (encode_raw(&tool->dst,"#include <stdint.h>\n",-1)<0) return -1;
  if (tool->tiny) {
    if (encode_raw(&tool->dst,"#include <avr/pgmspace.h>\n",-1)<0) return -1;
  }
  return 0;
}

/* Validate C identifier.
 */
 
static inline int tool_isident(char ch) {
  if ((ch>='a')&&(ch<='z')) return 1;
  if ((ch>='A')&&(ch<='Z')) return 1;
  if ((ch>='0')&&(ch<='9')) return 1;
  if (ch=='_') return 1;
  return 0;
}

static int tool_is_c_identifier(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<1) return 0;
  if (srcc>255) return 0;
  if ((src[0]>='0')&&(src[0]<='9')) return 0;
  for (;srcc-->0;src++) if (!tool_isident(*src)) return 0;
  return 1;
}

/* Guess type for C output.
 */
 
static int tool_guess_c_type(const char **dst,struct tool *tool,const char *name,int namec,const void *src,int srcc) {
  *dst="uint8_t";
  return 7;
}

/* Guess name for C member.
 */
 
int tool_guess_c_name(const char **dst,struct tool *tool,const char *type,int typec,const void *src,int srcc) {
  if (!tool->srcpath) {
    *dst="data";
    return 4;
  }
  const char *base=tool->srcpath;
  int basec=0,p=0,reading=1;
  for (;tool->srcpath[p];p++) {
    if (tool->srcpath[p]=='/') {
      base=tool->srcpath+p+1;
      basec=0;
      reading=1;
    } else if ((tool->srcpath[p]=='.')||(tool->srcpath[p]=='-')) {
      reading=0;
    } else if (!reading) {
    } else if (!tool_isident(tool->srcpath[p])) {
      base=0;
      basec=0;
      reading=0;
    } else {
      basec++;
    }
  }
  if (basec) {
    *dst=base;
    return basec;
  }
  *dst="data";
  return 4;
}

/* Size in bytes of C type.
 * We don't allow spaces so no eg "unsigned char" -- no particular reason to forbid this, if we ever want it.
 * We don't support float or double because I don't expect to need them on the Arduino.
 * No "long" or "uintptr_t" or such because they well could have different sizes across different platforms.
 * (that's not an unsolvable problem; just I don't think there's any need).
 */
 
static int tool_get_c_type_size(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<1) return 0;
  
  // stdint
  {
    const char *digits=src;
    int digitsc=srcc;
    if (digitsc&&(digits[0]=='u')) { digits++; digitsc--; }
    if ((digitsc>5)&&!memcmp(digits,"int",3)&&!memcmp(digits+digitsc-2,"_t",2)) {
      digits+=3;
      digitsc-=5;
      int bitsize=0;
      for (;digitsc-->0;digits++) {
        if ((*digits<'0')||(*digits>'9')) {
          bitsize=0;
          break;
        }
        bitsize*=10;
        bitsize+=(*digits)-'0';
      }
      if ((bitsize>0)&&!(bitsize&7)) {
        return bitsize>>3;
      }
    }
  }
  
  if ((srcc==4)&&!memcmp(src,"char",4)) return 1;
  if ((srcc==5)&&!memcmp(src,"short",5)) return 2;
  if ((srcc==3)&&!memcmp(src,"int",3)) return 4;
  // Not going to speculate about "long", "float", whatever else...
  return 0;
}

/* Array body.
 */
 
#define TOOL_LINE_LENGTH_LIMIT 100
 
static int tool_generate_c_array_body_1(struct encoder *dst,const uint8_t *src,int srcc) {
  int linelen=0,err;
  for (;srcc-->0;src++) {
    if ((err=encode_fmt(dst,"%d,",*src))<0) return -1;
    linelen+=err;
    if (linelen>=TOOL_LINE_LENGTH_LIMIT) {
      encode_raw(dst,"\n",1);
      linelen=0;
    }
  }
  if (linelen) encode_raw(dst,"\n",1);
  return 0;
}
 
static int tool_generate_c_array_body_2(struct encoder *dst,const uint16_t *src,int srcc) {
  int linelen=0,err;
  for (;srcc-->0;src++) {
    if ((err=encode_fmt(dst,"%d,",*src))<0) return -1;
    linelen+=err;
    if (linelen>=TOOL_LINE_LENGTH_LIMIT) {
      encode_raw(dst,"\n",1);
      linelen=0;
    }
  }
  if (linelen) encode_raw(dst,"\n",1);
  return 0;
}
 
static int tool_generate_c_array_body_4(struct encoder *dst,const uint32_t *src,int srcc) {
  int linelen=0,err;
  for (;srcc-->0;src++) {
    if ((err=encode_fmt(dst,"%d,",*src))<0) return -1;
    linelen+=err;
    if (linelen>=TOOL_LINE_LENGTH_LIMIT) {
      encode_raw(dst,"\n",1);
      linelen=0;
    }
  }
  if (linelen) encode_raw(dst,"\n",1);
  return 0;
}
 
static int tool_generate_c_array_body_8(struct encoder *dst,const uint64_t *src,int srcc) {
  int linelen=0,err;
  for (;srcc-->0;src++) {
    if ((err=encode_fmt(dst,"%lld,",(long long)*src))<0) return -1;
    linelen+=err;
    if (linelen>=TOOL_LINE_LENGTH_LIMIT) {
      encode_raw(dst,"\n",1);
      linelen=0;
    }
  }
  if (linelen) encode_raw(dst,"\n",1);
  return 0;
}

/* Array.
 */
 
int tool_generate_c_array(
  struct tool *tool,
  const char *type,int typec,
  const char *name,int namec,
  const void *src,int srcc
) {
  if (!type) typec=0; else if (typec<0) { typec=0; while (type[typec]) typec++; }
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  
  if (!typec) {
    if ((typec=tool_guess_c_type(&type,tool,name,namec,src,srcc))<1) return -1;
  }
  if (!namec) {
    if ((namec=tool_guess_c_name(&name,tool,type,typec,src,srcc))<1) return -1;
  }
  if (!tool_is_c_identifier(type,typec)) {
    fprintf(stderr,"%s: Array type is not a C identifier: '%.*s'\n",tool->exename,typec,type);
    return -1;
  }
  if (!tool_is_c_identifier(name,namec)) {
    fprintf(stderr,"%s: Array name is not a C identifier: '%.*s'\n",tool->exename,namec,name);
    return -1;
  }
  int unitlen=tool_get_c_type_size(type,typec);
  if (unitlen<1) {
    fprintf(stderr,"%s: Unknown C type '%.*s'\n",tool->exename,typec,type);
    return -1;
  }
  if (srcc%unitlen) {
    fprintf(stderr,"%s: Input content length %d must be a multiple of %d (%.*s)\n",tool->srcpath,srcc,unitlen,typec,type);
    return -1;
  }
  
  if (encode_fmt(&tool->dst,"const %.*s %.*s[]%s={\n",typec,type,namec,name,tool->tiny?" PROGMEM":"")<0) return -1;
  switch (unitlen) {
    case 1: if (tool_generate_c_array_body_1(&tool->dst,src,srcc)<0) return -1; break;
    case 2: if (tool_generate_c_array_body_2(&tool->dst,src,srcc>>1)<0) return -1; break;
    case 4: if (tool_generate_c_array_body_4(&tool->dst,src,srcc>>2)<0) return -1; break;
    case 8: if (tool_generate_c_array_body_8(&tool->dst,src,srcc>>3)<0) return -1; break;
    default: {
        fprintf(stderr,"%s: Type unit length must be (1,2,4,8) for array output (have '%.*s' = %d)\n",tool->exename,typec,type,unitlen);
        return -1;
      }
  }
  if (encode_raw(&tool->dst,"};\n",-1)<0) return -1;

  return 0;
}

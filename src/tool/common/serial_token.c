#include "serial.h"
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

/* Measure number.
 */
 
int sr_number_measure(const char *src,int srcc,int *flags) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int _flags; if (!flags) flags=&_flags;
  *flags=0;
  int srcp=0;
  
  // Sign.
  if (srcp>=srcc) return 0;
  if ((src[srcp]=='-')||(src[srcp]=='+')) {
    (*flags)|=SR_NUMBER_SIGN;
    if (++srcp>=srcc) return 0;
  }
  
  // Alternate bases are allowed but the first character after sign must be a digit, firm requirement.
  if ((src[srcp]<'0')||(src[srcp]>'9')) return 0;
  if ((src[srcp]=='0')&&(srcp<srcc-2)) switch (src[srcp+1]) {
    case 'x': case 'X':
    case 'd': case 'D':
    case 'o': case 'O':
    case 'b': case 'B': (*flags)|=SR_NUMBER_BASE; srcp+=2; break;
  }
  
  // Whole digits. Any digit or letter is allowed.
  while ((srcp<srcc)&&(sr_digit_eval(src[srcp])>=0)) srcp++;
  
  // Fraction. Only decimal digits permitted.
  if ((srcp<=srcc-2)&&(src[srcp]=='.')&&(src[srcp+1]>='0')&&(src[srcp+1]<='9')) {
    (*flags)|=SR_NUMBER_FRACTION;
    srcp+=2;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) srcp++;
  }
  
  // Exponent.
  if ((srcp<srcc)&&((src[srcp]=='e')||(src[srcp]=='E'))) {
    (*flags)|=SR_NUMBER_EXPONENT;
    if (++srcp>=srcc) return -1;
    if ((src[srcp]=='+')||(src[srcp]=='-')) {
      if (++srcp>=srcc) return -1;
    }
    // At least one digit required, and must be decimal.
    if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
    srcp++;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) srcp++;
    
  // Check for exponent without fraction -- we'd have skipped it during Whole.
  } else if ((srcp<srcc)&&!((*flags)&(SR_NUMBER_FRACTION|SR_NUMBER_BASE))) {
    if ((src[srcp-1]=='e')||(src[srcp-1]=='E')) {
      (*flags)|=SR_NUMBER_EXPONENT;
      if ((src[srcp]=='-')||(src[srcp]=='+')) {
        if (++srcp>=srcc) return -1;
      }
      if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
      srcp++;
      while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) srcp++;
    }
  }
  
  // Assert clean token break.
  if (srcp>=srcc) return srcp;
  if (sr_digit_eval(src[srcp])>=0) return -1;
  return srcp;
}

/* Measure string.
 */
 
int sr_string_measure(const char *src,int srcc,int *flags) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<2) return 0;
  if ((src[0]!='"')&&(src[0]!='\'')&&(src[0]!='`')) return 0;
  int _flags;
  if (!flags) flags=&_flags;
  *flags=SR_STRING_SIMPLE;
  int srcp=1;
  while (1) {
    if (srcp>=srcc) return -1;
    if (src[srcp]==src[0]) return srcp+1;
    if (src[srcp]=='\\') {
      (*flags)&=~SR_STRING_SIMPLE;
      if (++srcp>=srcc) return -1;
    }
    if ((src[srcp]<0x20)||(src[srcp]>0x7e)) return -1;
    srcp++;
  }
}

/* Measure identifier.
 */
 
int sr_ident_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  
  // First character must not be a digit, but they're cool anywhere else.
  if ((src[0]>='0')&&(src[0]<='9')) return 0;
  
  int srcp=0;
  while ((srcp<srcc)&&sr_isident(src[srcp])) srcp++;
  return srcp;
}

/* Measure any JSON expression.
 */
 
static int sr_json_measure_string(const char *src,int srcc) {
  int srcp=1;
  while (1) {
    if (srcp>=srcc) return -1;
    if (src[srcp]=='\\') {
      if (srcp>srcc-2) return -1;
      srcp+=2;
    } else {
      if (src[srcp++]=='"') return srcp;
    }
  }
}
 
static int sr_json_measure_structure(const char *src,int srcc,char term) {
  int srcp=1;
  while (1) {
    if (srcp>=srcc) return -1;
    int err=-1;
    switch (src[srcp]) {
      case '{': err=sr_json_measure_structure(src+srcp,srcc-srcp,'}'); break;
      case '[': err=sr_json_measure_structure(src+srcp,srcc-srcp,']'); break;
      case '"': err=sr_json_measure_string(src+srcp,srcc-srcp); break;
      default: {
          if (src[srcp]==term) return srcp+1;
          err=1;
        }
    }
    if (err<1) return -1;
    srcp+=err;
  }
}

static int sr_json_measure_ident_or_number(const char *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
         if ((src[srcp]>='0')&&(src[srcp]<='9')) ;
    else if ((src[srcp]>='a')&&(src[srcp]<='z')) ;
    else if ((src[srcp]>='A')&&(src[srcp]<='Z')) ;
    else if (src[srcp]=='.') ;
    else if (src[srcp]=='+') ;
    else if (src[srcp]=='-') ;
    else return srcp;
    srcp++;
  }
  return srcp;
}
 
int sr_json_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  // Skip leading whitespace.
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  // First character tells us its type.
  if (srcp>=srcc) return 0;
  int err=-1;
  switch (src[srcp]) {
    case '{': err=sr_json_measure_structure(src+srcp,srcc-srcp,'}'); break;
    case '[': err=sr_json_measure_structure(src+srcp,srcc-srcp,']'); break;
    case '"': err=sr_json_measure_string(src+srcp,srcc-srcp); break;
    default: err=sr_json_measure_ident_or_number(src+srcp,srcc-srcp); break;
  }
  if (err<1) return -1;
  srcp+=err;
  
  // Skip trailing whitespace.
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  return srcp;
}

/* Evaluate integer.
 */

int sr_int_eval(int *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,base=10,positive=1;
  
  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    positive=0;
    if (++srcp>=srcc) return -1;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }
  
  if ((src[srcp]=='0')&&(srcp<srcc-2)) switch (src[srcp+1]) {
    case 'x': case 'X': base=16; srcp+=2; break;
    case 'd': case 'D': base=10; srcp+=2; break;
    case 'o': case 'O': base=8; srcp+=2; break;
    case 'b': case 'B': base=2; srcp+=2; break;
  }
  
  int limit=(positive?(UINT_MAX/base):(INT_MIN/base));
  int overflow=0;
  *dst=0;
  while (srcp<srcc) {
    int digit=sr_digit_eval(src[srcp]);
    if ((digit<0)||(digit>=base)) return -1;
    srcp++;
    if (positive) {
      if ((unsigned int)(*dst)>(unsigned int)limit) overflow=1;
      (*dst)*=base;
      if ((unsigned int)(*dst)>UINT_MAX-digit) overflow=1;
      (*dst)+=digit;
    } else {
      if (*dst<limit) overflow=1;
      (*dst)*=base;
      if (*dst<INT_MIN+digit) overflow=1;
      (*dst)-=digit;
    }
  }
  
  if (overflow) return 0;
  if (positive&&(*dst<0)) return 1;
  return 2;
}

/* Represent integer.
 */
 
int sr_decsint_repr(char *dst,int dsta,int src) {
  int dstc;
  if (src<0) {
    int limit=-10,i;
    dstc=2;
    while (src<=limit) { dstc++; if (limit<INT_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    for (i=dstc;i-->1;src/=10) dst[i]='0'-src%10;
    dst[0]='-';
  } else {
    int limit=10,i;
    dstc=1;
    while (src>=limit) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    for (i=dstc;i-->0;src/=10) dst[i]='0'+src%10;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int sr_decuint_repr(char *dst,int dsta,int src,int digitc) {
  int dstc;
  if (digitc<0) {
    dstc=-digitc;
  } else {
    unsigned int limit=10;
    dstc=1;
    while ((unsigned int)src>=limit) { dstc++; if (limit>UINT_MAX/10) break; limit*=10; }
    if (dstc<digitc) dstc=digitc;
  }
  if (dstc>dsta) return dstc;
  int i=dstc;
  for (;i-->0;src=((unsigned int)src)/10) dst[i]='0'+((unsigned int)src)%10;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Represent hexadecimal integer.
 */
 
int sr_hexuint_repr(char *dst,int dsta,int src,int digitc,int prefix) {
  int dstc;
  if (digitc<0) {
    dstc=-digitc;
  } else {
    unsigned int limit=0x10;
    dstc=1;
    while ((unsigned int)src>=limit) { dstc++; if (limit&~(UINT_MAX>>4)) break; limit<<=4; }
    if (dstc<digitc) dstc=digitc;
  }
  if (dstc>1024) dstc=1024; // sanity limit, i mean come on...
  if (prefix) dstc+=2;
  if (dstc>dsta) return dstc;
  int i=dstc;
  for (;i-->0;src=((unsigned int)src)>>4) dst[i]=sr_hexdigit_repr(src);
  if (prefix) {
    dst[0]='0';
    dst[1]='x';
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Evaluate float.
 */
 
int sr_float_eval(double *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,positive=1;
  *dst=0.0;
  
  // sign
  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    if (++srcp>=srcc) return -1;
    positive=0;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }
  
  // whole: at least one is required
  int prep=srcp;
  while (srcp<srcc) {
    int digit=sr_digit_eval(src[srcp]);
    if ((digit<0)||(digit>9)) break;
    srcp++;
    (*dst)*=10.0;
    (*dst)+=digit;
  }
  if (srcp==prep) return -1;
  
  // fraction: require at least one if introduced.
  if ((srcp<srcc)&&(src[srcp]=='.')) {
    if (++srcp>=srcc) return -1;
    prep=srcp;
    double coef=1.0;
    while (srcp<srcc) {
      int digit=sr_digit_eval(src[srcp]);
      if ((digit<0)||(digit>9)) break;
      srcp++;
      coef/=10.0;
      (*dst)+=digit*coef;
    }
    if (srcp==prep) return -1;
  }
  
  // exponent
  if ((srcp<srcc)&&((src[srcp]=='e')||(src[srcp]=='E'))) {
    if (++srcp>=srcc) return -1;
    int epositive=1,exp=0;
    if (src[srcp]=='-') {
      if (++srcp>=srcc) return -1;
      epositive=0;
    } else if (src[srcp]=='+') {
      if (++srcp>=srcc) return -1;
    }
    prep=srcp;
    while (srcp<srcc) {
      int digit=sr_digit_eval(src[srcp]);
      if ((digit<0)||(digit>9)) break;
      exp*=10;
      exp+=digit;
      if (exp>999) exp=999; // guard against integer overflow; by 999 the output will saturate for sure
    }
    if (srcp==prep) return -1;
    if (epositive) {
      while (exp-->0) (*dst)*=10.0;
    } else {
      while (exp-->0) (*dst)/=10.0;
    }
  }
  
  // assert completion
  if (srcp<srcc) return -1;
  if (!positive) (*dst)=-(*dst);
  return 0;
}

/* Represent float.
 */
 
int sr_float_repr(char *dst,int dsta,double src) {
  
  // Pick off oddballs and represent as "0.0" or "1e999"
  switch (fpclassify(src)) {
    case FP_ZERO: case FP_NAN: {
        if (dsta>=3) {
          memcpy(dst,"0.0",3);
          if (dsta>3) dst[3]=0;
        }
        return 3;
      } break;
    case FP_INFINITE: if (src>0.0) {
        if (dsta>=5) {
          memcpy(dst,"1e999",5);
          if (dsta>5) dst[5]=0;
        }
        return 5;
      } else {
        if (dsta>=6) {
          memcpy(dst,"-1e999",6);
          if (dsta>6) dst[6]=0;
        }
        return 6;
      }
  }

  // Force positive.
  int dstc=0;
  if (src<0.0) {
    if (dstc<dsta) dst[dstc]='-';
    dstc++;
    src=-src;
  }
  
  // Use an exponent if outside 10^(-3..6)
  int exp=0;
  if (src>=1000000.0) {
    while ((src>=10.0)&&(exp<99)) { exp++; src/=10.0; }
  } else if (src<0.001) {
    while ((src<1.0)&&(exp>-99)) { exp--; src*=10.0; }
  }
  
  // Split whole and fraction; emit whole as an integer since it must be in 0..1M
  double whole,fract;
  fract=modf(src,&whole);
  dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,(int)whole,0);
  
  // Emit 1..6 fraction digits. Always emit, even if it's zero.
  char frv[6];
  int frc=0;
  for (;frc<sizeof(frv);frc++) {
    fract*=10.0;
    int digit=(int)fract;
    if (digit<0) digit=0;
    else if (digit>9) digit=9;
    fract-=digit;
    frv[frc]='0'+digit;
  }
  while ((frc>1)&&(frv[frc-1]=='0')) frc--;
  // If there's at least 3 nines, and not ALL nines, round it up.
  int ninec=0;
  while ((ninec<frc)&&(frv[frc-ninec-1]=='9')) ninec++;
  if ((ninec>=3)&&(ninec<frc)) {
    frc-=ninec;
    frv[frc-1]++;
  }
  if (dstc<dsta) dst[dstc]='.';
  dstc++;
  if (dstc<=dsta-frc) memcpy(dst+dstc,frv,frc);
  dstc+=frc;
  
  // Emit exponent if we have one. Borrow int repr.
  if (exp) {
    if (dstc<dsta) dst[dstc]='e';
    dstc++;
    dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,exp,0);
  }
  
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Evaluate string.
 */
 
int sr_string_eval(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc<2)||(src[0]!=src[srcc-1])) return -1;
  if ((src[0]!='"')&&(src[0]!='\'')&&(src[0]!='`')) return -1;
  src++; srcc-=2;
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='\\') {
      if (++srcp>=srcc) return -1;
      switch (src[srcp++]) {
      
        // Literals.
        case '\\': case '"': case '\'': case '`': case '/': {
            if (dstc<dsta) dst[dstc]=src[srcp-1];
            dstc++;
          } break;
          
        // Named bytes. JSON, and also "\0"
        case '0': if (dstc<dsta) dst[dstc]=0x00; dstc++; break;
        case 'b': if (dstc<dsta) dst[dstc]=0x08; dstc++; break;
        case 't': if (dstc<dsta) dst[dstc]=0x09; dstc++; break;
        case 'n': if (dstc<dsta) dst[dstc]=0x0a; dstc++; break;
        case 'f': if (dstc<dsta) dst[dstc]=0x0c; dstc++; break;
        case 'r': if (dstc<dsta) dst[dstc]=0x0d; dstc++; break;
        
        // Hexadecimal byte. JSON doesn't allow it but we do.
        case 'x': {
            if (srcp>srcc-2) return -1;
            int hi=sr_digit_eval(src[srcp++]);
            int lo=sr_digit_eval(src[srcp++]);
            if ((hi<0)||(lo<0)||(hi>15)||(lo>15)) return -1;
            if (dstc<dsta) dst[dstc]=(hi<<4)|lo;
            dstc++;
          } break;
          
        // Unicode code point. Watch for surrogate pairs.
        case 'u': {
            if (srcp>srcc-4) return -1;
            int ch=0,i=4;
            for (;i-->0;srcp++) {
              int digit=sr_digit_eval(src[srcp]);
              if ((digit<0)||(digit>15)) return -1;
              ch<<=4;
              ch|=digit;
            }
            if ((ch>=0xd800)&&(ch<0xdc00)&&(srcp<=srcc-6)&&(src[srcp]=='\\')&&(src[srcp+1]=='u')) {
              int lo=0;
              for (i=0;i<4;i++) {
                int digit=sr_digit_eval(src[srcp+2+i]);
                if ((digit<0)||(digit>15)) { lo=0; break; }
                lo<<=4;
                lo|=digit;
              }
              if ((lo>=0xdc00)&&(lo<0xe000)) {
                srcp+=6;
                ch=0x10000+((ch&0x3ff)<<10)+(lo&0x3ff);
              }
            }
            int err=sr_utf8_encode(dst+dstc,dsta-dstc,ch);
            if (err<0) return -1; // shouldn't be possible
            dstc+=err;
          } break;
          
        // Unknown escape.
        default: return -1;
      }
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Represent string.
 */
 
static void sr_string_repr_ucs2(char *dst/* 6 */,int src/* 0..0xffff */) {
  dst[0]='\\';
  dst[1]='u';
  dst[2]=sr_hexdigit_repr(src>>12);
  dst[3]=sr_hexdigit_repr(src>>8);
  dst[4]=sr_hexdigit_repr(src>>4);
  dst[5]=sr_hexdigit_repr(src);
}

int sr_string_repr(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  if (dstc<dsta) dst[dstc]='"';
  dstc++;
  while (srcp<srcc) {
    switch (src[srcp]) {
    
      case '"': case '\\': {
          if (dstc<=dsta-2) {
            dst[dstc++]='\\';
            dst[dstc++]=src[srcp];
          } else {
            dstc+=2;
          }
          srcp++;
        } break;
        
      case 0x08: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='b'; } dstc+=2; srcp++; break;
      case 0x09: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='t'; } dstc+=2; srcp++; break;
      case 0x0a: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='n'; } dstc+=2; srcp++; break;
      case 0x0c: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='f'; } dstc+=2; srcp++; break;
      case 0x0d: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='r'; } dstc+=2; srcp++; break;
        
      default: if ((src[srcp]>=0x20)&&(src[srcp]<=0x7e)) {
          if (dstc<dsta) dst[dstc]=src[srcp];
          dstc++;
          srcp++;
        } else {
          int ch,seqlen;
          if ((seqlen=sr_utf8_decode(&ch,src+srcp,srcc-srcp))<1) {
            ch=(uint8_t)src[srcp];
            seqlen=1;
          }
          if (ch>=0x10000) { // split to surrogate pair
            if (dstc<=dsta-12) {
              ch-=0x10000;
              int hi=0xd800|(ch>>10);
              int lo=0xdc00|(ch&0x3ff);
              sr_string_repr_ucs2(dst+dstc,hi);
              sr_string_repr_ucs2(dst+dstc+6,lo);
            }
            dstc+=12;
          } else { // bmp
            if (dstc<=dsta-6) {
              sr_string_repr_ucs2(dst+dstc,ch);
            }
            dstc+=6;
          }
          srcp+=seqlen;
        }
    }
  }
  if (dstc<dsta) dst[dstc]='"';
  dstc++;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int sr_string_repr_x(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  if (dstc<dsta) dst[dstc]='"';
  dstc++;
  for (;srcp<srcc;srcp++) { // no text encoding here; it's always one source byte at a time
    switch (src[srcp]) {
    
      case '"': case '\\': {
          if (dstc<=dsta-2) {
            dst[dstc++]='\\';
            dst[dstc++]=src[srcp];
          } else {
            dstc+=2;
          }
        } break;
        
      case 0x00: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='0'; } dstc+=2; break;
      case 0x08: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='b'; } dstc+=2; break;
      case 0x09: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='t'; } dstc+=2; break;
      case 0x0a: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='n'; } dstc+=2; break;
      case 0x0c: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='f'; } dstc+=2; break;
      case 0x0d: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='r'; } dstc+=2; break;
        
      default: if ((src[srcp]>=0x20)&&(src[srcp]<=0x7e)) {
          if (dstc<dsta) dst[dstc]=src[srcp];
          dstc++;
        } else {
          if (dstc<=dsta-4) {
            dst[dstc++]='\\';
            dst[dstc++]='x';
            dst[dstc++]=sr_hexdigit_repr(src[srcp]>>4);
            dst[dstc++]=sr_hexdigit_repr(src[srcp]);
          } else {
            dstc+=4;
          }
        }
    }
  }
  if (dstc<dsta) dst[dstc]='"';
  dstc++;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Evaluate some JSON expression as a given C type.
 */
 
int sr_int_from_json(int *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;
  
  // If it's a number, evaluate first as int then float.
  if ((src[0]=='-')||((src[0]>='0')&&(src[0]<='9'))) {
    if (sr_int_eval(dst,src,srcc)>=1) return 0; // int, if we got 32 bits properly
    double d;
    if (sr_float_eval(&d,src,srcc)>=0) {
      if (d<=INT_MIN) *dst=INT_MIN;
      else if (d>=INT_MAX) *dst=INT_MAX;
      else *dst=(int)d;
      return 0;
    }
    return -1;
  }
  
  // If it's a string, three things could happen...
  if (src[0]=='"') {
    // 1. Empty string is 0
    if (srcc==2) { *dst=0; return 0; }
    // 2. Attempt evaluation, if the content forms an exact JSON token.
    char tmp[32];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),src,srcc);
    if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
      if (sr_int_from_json(dst,tmp,tmpc)>=0) return 0;
    }
    // 3. Non-empty, non-JSON string is -1
    *dst=-1;
    return 0;
  }
  
  // Objects and arrays are 0 if empty, or -1 if full.
  if ((src[0]=='{')||(src[0]=='[')) {
    int nonspacep=1;
    while ((nonspacep<srcc)&&((unsigned char)src[nonspacep]<=0x20)) nonspacep++;
    if (nonspacep==srcc-1) { // empty!
      *dst=0;
    } else {
      *dst=-1;
    }
    return 0;
  }
  
  // (null,false,true) are (0,0,1)
  if ((srcc==4)&&!memcmp(src,"null",4)) { *dst=0; return 0; }
  if ((srcc==5)&&!memcmp(src,"false",5)) { *dst=0; return 0; }
  if ((srcc==4)&&!memcmp(src,"true",4)) { *dst=1; return 0; }
  
  // Anything else is not valid JSON.
  return -1;
}

int sr_float_from_json(double *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;
  
  // If it's a number, evaluate first as float then int.
  if ((src[0]=='-')||((src[0]>='0')&&(src[0]<='9'))) {
    if (sr_float_eval(dst,src,srcc)>=0) return 0;
    int i;
    switch (sr_int_eval(&i,src,srcc)) {
      case 2: *dst=(double)i; return 0;
      case 1: *dst=(double)(unsigned int)i; return 0;
      case 0: if (src[0]=='-') *dst=(double)INT_MIN; else *dst=(double)INT_MAX; return 0;
    }
    return -1;
  }
  
  // If it's a string, three things could happen...
  if (src[0]=='"') {
    // 1. Empty string is 0.0
    if (srcc==2) { *dst=0.0; return 0; }
    // 2. Attempt evaluation, if the content forms an exact JSON token.
    char tmp[32];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),src,srcc);
    if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
      if (sr_float_from_json(dst,tmp,tmpc)>=0) return 0;
    }
    // 3. Non-empty, non-JSON string is -1.0
    *dst=-1.0;
    return 0;
  }
  
  // Objects and arrays are NAN if empty, or -1.0 if full.
  if ((src[0]=='{')||(src[0]=='[')) {
    int nonspacep=1;
    while ((nonspacep<srcc)&&((unsigned char)src[nonspacep]<=0x20)) nonspacep++;
    if (nonspacep==srcc-1) { // empty!
      *dst=NAN;
    } else {
      *dst=-1.0;
    }
    return 0;
  }
  
  // (null,false,true) are (NAN,0.0,1.0)
  if ((srcc==4)&&!memcmp(src,"null",4)) { *dst=NAN; return 0; }
  if ((srcc==5)&&!memcmp(src,"false",5)) { *dst=0.0; return 0; }
  if ((srcc==4)&&!memcmp(src,"true",4)) { *dst=1.0; return 0; }
  
  // Anything else is not valid JSON.
  return -1;
}

int sr_string_from_json(char *dst,int dsta,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;
  
  // Strings evaluate as you'd expect.
  if (src[0]=='"') return sr_string_eval(dst,dsta,src,srcc);
  
  // Everything else is verbatim. Assume that the input is valid, whatever.
  if (srcc<=dsta) {
    memcpy(dst,src,srcc);
    if (srcc<dsta) dst[srcc]=0;
  }
  return srcc;
}

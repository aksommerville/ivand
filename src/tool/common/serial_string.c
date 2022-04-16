#include "serial.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* memcasecmp
 */
 
int sr_memcasecmp(const void *a,const void *b,int c) {
  if (c<1) return 0;
  while (c--) {
    uint8_t cha=*(uint8_t*)a; a=(uint8_t*)a+1;
    uint8_t chb=*(uint8_t*)b; b=(uint8_t*)b+1;
    if ((cha>=0x41)&&(cha<=0x5a)) cha+=0x20;
    if ((chb>=0x41)&&(chb<=0x5a)) chb+=0x20;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
  return 0;
}

/* Case-insensitive search.
 */
 
int sr_isearch(const char *haystack,int hc,const char *needle,int nc) {
  if (!needle) return 0;
  if (nc<0) { nc=0; while (needle[nc]) nc++; }
  if (!nc) return 0;
  if (!haystack) return -1;
  if (hc<0) { hc=0; while (haystack[hc]) hc++; }
  int lastp=hc-nc;
  int hp=0;
  for (;hp<=lastp;hp++) {
    if (!haystack[hp]) break; // (hc) is a limit rather than a strict count, and we assume (needle) has no nuls
    if (!sr_memcasecmp(haystack+hp,needle,nc)) return hp;
  }
  return -1;
}

/* Match pattern.
 */
 
static int sr_pattern_match_1(const char *src,int srcc,const char *pat,int patc) {
  int srcp=0,patp=0;
  while (1) {
  
    // End of pattern?
    if (patp>=patc) {
      if (srcp>=srcc) return 1;
      return 0;
    }
    
    // Wildcard?
    if (pat[patp]=='*') {
      patp++;
      while ((patp<patc)&&(pat[patp]=='*')) patp++;
      if (patp>=patc) return 1; // trailing wildcard always matches
      while (srcp<srcc) {
        if (sr_pattern_match_1(src+srcp,srcc-srcp,pat+patp,patc-patp)) return 1;
        srcp++;
      }
      return 0;
    }
    
    // End of input?
    if (srcp>=srcc) return 0;
    
    // Escape?
    if (pat[patp]=='\\') {
      if (++patp>=patc) return 0;
      if (src[srcp]!=pat[patp]) return 0;
      srcp++;
      patp++;
      continue;
    }
    
    // Whitespace?
    if ((unsigned char)pat[patp]<=0x20) {
      if ((unsigned char)src[srcp]>0x20) return 0;
      while ((patp<patc)&&((unsigned char)pat[patp]<=0x20)) patp++;
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      continue;
    }
    
    // All else case-insensitive or verbatim.
    char cha=src[srcp++]; if ((cha>='A')&&(cha<='Z')) cha+=0x20;
    char chb=pat[patp++]; if ((chb>='A')&&(chb<='Z')) chb+=0x20;
    if (cha!=chb) return 0;
  }
}
 
int sr_pattern_match(const char *src,int srcc,const char *pat,int patc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!pat) patc=0; else if (patc<0) { patc=0; while (pat[patc]) patc++; }
  while (srcc&&((unsigned char)src[srcc]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  while (patc&&((unsigned char)pat[patc]<=0x20)) patc--;
  while (patc&&((unsigned char)pat[0]<=0x20)) { patc--; pat++; }
  return sr_pattern_match_1(src,srcc,pat,patc);
}

#include "tool/common/tool_utils.h"
#include "tool/common/serial.h"
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* Overwrite buffer with a single-period sine wave.
 */
 
static void print_sine(double *v,int c) {
  double t=0.0,dt=(M_PI*2.0)/c;
  for (;c-->0;v++,t+=dt) *v=sin(t);
}

/* Harmonics.
 */
 
static void print_harmonics(double *dst,double *scratch,int c,const double *coefv,int coefc) {
  if (coefc>c) coefc=c;
  memset(scratch,0,sizeof(double)*c);
  int i=0; for (;i<coefc;i++) {
    switch (fpclassify(coefv[i])) {
      case FP_ZERO: case FP_NAN: continue;
    }
    int step=i+1;
    int p=0;
    int j=c;
    double *v=scratch;
    for (;j-->0;v++,p+=step) {
      if (p>=c) p-=c;
      (*v)+=dst[p]*coefv[i];
    }
  }
  memcpy(dst,scratch,sizeof(double)*c);
}

/* Single-period FM.
 */
 
static void print_fm(double *dst,double *scratch,int c,int rate,double range) {
  memcpy(scratch,dst,sizeof(double)*c);
  double modp=0.0; // radians
  double modpd=(rate*M_PI*2.0)/c;
  double srcpf=0.0;
  double scale=1.0;
  double *v=dst;
  int i=c;
  for (;i-->0;v++,modp+=modpd) {
  
    int srcpi=(int)srcpf;
    while (srcpi>=c) { srcpi-=c; srcpf-=c; }
    while (srcpi<0) { srcpi+=c; srcpf+=c; }
    *v=scratch[srcpi];
    
    double mod=scale+scale*sin(modp)*range;
    srcpf+=mod;
  }
}

/* Normalize.
 */
 
static void normalize(double *dst,int c,double peak) {
  double min=dst[0],max=dst[0];
  double *v=dst;
  int i=c;
  for (;i-->0;v++) {
    if (*v<min) min=*v;
    else if (*v>max) max=*v;
  }
  if (min<0.0) min=-min;
  if (max<0.0) max=-max;
  if (min>max) max=min;
  switch (fpclassify(max)) {
    case FP_NAN: case FP_ZERO: return;
  }
  double scale=peak/max;
  for (v=dst,i=c;i-->0;v++) (*v)*=scale;
}

/* Clamp.
 */
 
static void clamp(double *v,int c,double peak) {
  double npeak=-peak;
  for (;c-->0;v++) {
    if (*v<npeak) *v=npeak;
    else if (*v>peak) *v=peak;
  }
}

/* Smooth.
 */
 
static void smooth(double *dst,double *scratch,int c,int size) {
  if (size<2) return;
  if (size>=c) { // Silly-large kernel; we become DC.
    double avg=0.0;
    double *v=dst;
    int i=c;
    for (;i-->0;v++) avg+=*v;
    avg/=c;
    for (v=dst,i=c;i-->0;v++) *v=avg;
    return;
  }
  
  // Start with the average of the final (size) samples.
  // "average" = before dividing
  double avg=0.0;
  int i=size;
  while (i-->0) avg+=dst[c-i-1];
  int tailp=c-size;
  
  // Keep a dry copy for reading the tail.
  memcpy(scratch,dst,sizeof(double)*c);
  
  // Now stream the moving average.
  double *v=dst;
  for (i=c;i-->0;v++) {
    avg+=(*v);
    avg-=scratch[tailp];
    *v=avg/size;
    tailp++;
    if (tailp>=c) tailp=0;
  }
}

/* Shift phase.
 */
 
static void phase(double *dst,double *scratch,int c,double p) {
  int headc=p*c;
  if (headc<1) return;
  if (headc>=c) return;
  int tailc=c-headc;
  memcpy(scratch,dst,sizeof(double)*headc);
  memmove(dst,dst+headc,sizeof(double)*tailc);
  memcpy(dst+tailc,scratch,sizeof(double)*headc);
}

/* Process one line of input.
 */
 
static int mkwave_line(double *dst,double *scratch,int c,const char *src,int srcc,const char *path,int lineno) {
  
  // Strip line comment and trailing space, stop if empty.
  int p=0;
  for (;p<srcc;p++) if (src[p]=='#') srcc=p;
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  if (!srcc) return 0;
  p=0;
  while ((p<srcc)&&((unsigned char)src[p]<=0x20)) p++;
  
  // Every valid line begins with a keyword.
  const char *kw=src+p;
  int kwc=0;
  while ((p<srcc)&&((unsigned char)src[p]>0x20)) { p++; kwc++; }
  while ((p<srcc)&&((unsigned char)src[p]<=0x20)) p++;
  
  #define RDFLT(name,desc) { \
    const char *token=src+p; \
    int tokenc=0; \
    while ((p<srcc)&&((unsigned char)src[p]>0x20)) { p++; tokenc++; } \
    while ((p<srcc)&&((unsigned char)src[p]<=0x20)) p++; \
    if (!tokenc) { \
      fprintf(stderr,"%s:%d: Expected float '%s'\n",path,lineno,desc); \
      return -1; \
    } \
    if (sr_float_eval(&(name),token,tokenc)<0) { \
      fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' as float for '%s'\n",path,lineno,tokenc,token,desc); \
      return -1; \
    } \
  }
  #define RDINT(name,lo,hi,desc) { \
    const char *token=src+p; \
    int tokenc=0; \
    while ((p<srcc)&&((unsigned char)src[p]>0x20)) { p++; tokenc++; } \
    while ((p<srcc)&&((unsigned char)src[p]<=0x20)) p++; \
    if (!tokenc) { \
      fprintf(stderr,"%s:%d: Expected int '%s'\n",path,lineno,desc); \
      return -1; \
    } \
    if (sr_int_eval(&(name),token,tokenc)<1) { \
      fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' as int for '%s'\n",path,lineno,tokenc,token,desc); \
      return -1; \
    } \
    if (((name)<lo)||((name)>hi)) { \
      fprintf(stderr,"%s:%d: %d out of range (%d..%d) for '%s'\n",path,lineno,(name),lo,hi,desc); \
      return -1; \
    } \
  }
  
  if ((kwc==9)&&!memcmp(kw,"harmonics",9)) {
    double coefv[16];
    int coefc=0;
    while (p<srcc) {
      double n; RDFLT(n,"coefficient")
      if (coefc<16) coefv[coefc++]=n;
    }
    print_harmonics(dst,scratch,c,coefv,coefc);
    
  } else if ((kwc==2)&&!memcmp(kw,"fm",2)) {
    int rate; RDINT(rate,1,INT_MAX,"rate")
    double range; RDFLT(range,"range")
    print_fm(dst,scratch,c,rate,range);
    
  } else if ((kwc==9)&&!memcmp(kw,"normalize",9)) {
    double peak=1.0;
    if (p<srcc) { RDFLT(peak,"peak") }
    normalize(dst,c,peak);
    
  } else if ((kwc==5)&&!memcmp(kw,"clamp",5)) {
    double peak=1.0;
    if (p<srcc) { RDFLT(peak,"peak") }
    clamp(dst,c,peak);
    
  } else if ((kwc==6)&&!memcmp(kw,"smooth",6)) {
    int size; RDINT(size,1,INT_MAX,"size")
    smooth(dst,scratch,c,size);
    
  } else if ((kwc==5)&&!memcmp(kw,"phase",5)) {
    double ph; RDFLT(ph,"phase")
    phase(dst,scratch,c,ph);
    
  } else {
    fprintf(stderr,"%s:%d: Unknown keyword '%.*s'\n",path,lineno,kwc,kw);
    return -1;
  }
  
  #undef RDINT
  #undef RDFLT
  if (p<srcc) {
    fprintf(stderr,"%s:%d:WARNING: Ignoring unexpected tokens '%.*s'\n",path,lineno,srcc-p,src+p);
  }
  return 0;
}

/* Convert to 16-bit integers.
 */
 
static void quantize(int16_t *dst,const double *src,int c) {
  for (;c-->0;dst++,src++) {
    int sample=(int)((*src)*32767.0);
    if (sample>=32767) *dst=32767;
    else if (sample<=-32768) *dst=-32768;
    else *dst=sample;
  }
}

/* Read text, generate wave.
 */
 
static int mkwave(int16_t *dst,int dstc,struct tool *tool) {
  if (dstc<1) return -1;
  double *fv=malloc(sizeof(double)*dstc);
  double *scratch=malloc(sizeof(double)*dstc);
  if (!fv||!scratch) return -1;
  print_sine(fv,dstc);
  struct decoder decoder={.src=tool->src,.srcc=tool->srcc};
  int lineno=0,linec;
  const char *line;
  while ((linec=decode_line(&line,&decoder))>0) {
    lineno++;
    if (mkwave_line(fv,scratch,dstc,line,linec,tool->srcpath,lineno)<0) {
      free(fv);
      free(scratch);
      return -1;
    }
  }
  quantize(dst,fv,dstc);
  free(fv);
  free(scratch);
  return 0;
}

/* Extra arguments.
 */
 
static int BINARY_STDOUT=0;

static int cb_arg(struct tool *tool,const char *arg) {
  if (!strcmp(arg,"STDOUT")) {
    BINARY_STDOUT=1;
    return 1;
  }
  return 0;
}

/* Main.
 */

int main(int argc,char **argv) {
  struct tool tool={0};
  if (tool_startup(&tool,argc,argv,cb_arg)<0) return 1;
  if (tool.terminate) return 0;
  if (tool_read_input(&tool)<0) return 1;
  
  int16_t wave[512];
  if (mkwave(wave,512,&tool)<0) {
    fprintf(stderr,"%s: Failed to decode text wave\n",tool.srcpath);
    return 1;
  }
  
  if (BINARY_STDOUT) {
    if (write(STDOUT_FILENO,wave,sizeof(wave))!=sizeof(wave)) {
      fprintf(stderr,"%s: Failed to write binary output to stdout.\n",tool.exename);
      return 1;
    }
  } else {
    if (tool_generate_c_preamble(&tool)<0) return 1;
    if (tool_generate_c_array(&tool,"int16_t",7,0,0,wave,sizeof(wave))<0) return 1;
    if (tool_write_output(&tool)<0) return 1;
  }
  return 0;
}

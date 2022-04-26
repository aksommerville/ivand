#include "http.h"
#include "tool/common/serial.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Delete.
 */

void http_xfer_del(struct http_xfer *xfer) {
  if (!xfer) return;
  if (xfer->refc-->1) return;
  
  encoder_cleanup(&xfer->preamble);
  encoder_cleanup(&xfer->body);
  
  if (xfer->headerv) {
    struct http_header *header=xfer->headerv;
    int i=xfer->headerc;
    for (;i-->0;header++) {
      if (header->k) free(header->k);
      if (header->v) free(header->v);
    }
    free(xfer->headerv);
  }
  
  free(xfer);
}

/* Retain.
 */
 
int http_xfer_ref(struct http_xfer *xfer) {
  if (!xfer) return -1;
  if (xfer->refc<1) return -1;
  if (xfer->refc==INT_MAX) return -1;
  xfer->refc++;
  return 0;
}

/* New.
 */

struct http_xfer *http_xfer_new(int role) {
  if ((role!=HTTP_ROLE_CLIENT)&&(role!=HTTP_ROLE_SERVER)) return 0;
  struct http_xfer *xfer=calloc(1,sizeof(struct http_xfer));
  if (!xfer) return 0;
  xfer->refc=1;
  xfer->role=role;
  return xfer;
}

/* New, assembling request.
 */
 
struct http_xfer *http_xfer_new_request(
  int method,const char *path,int pathc,
  const char *content_type,
  const void *body,int bodyc
) {
  struct http_xfer *xfer=http_xfer_new(HTTP_ROLE_CLIENT);
  if (!xfer) return 0;
  if (http_xfer_set_request_line(xfer,method,path,pathc,0,0)<0) {
    http_xfer_del(xfer);
    return 0;
  }
  if (body) {
    if (bodyc<0) { bodyc=0; while (((char*)body)[bodyc]) bodyc++; }
    if (!content_type) content_type=http_guess_content_type(0,0,body,bodyc);
    if (http_xfer_add_header(xfer,"Content-Type",12,content_type,-1)<0) {
      http_xfer_del(xfer);
      return 0;
    }
    if (encode_raw(&xfer->body,body,bodyc)<0) {
      http_xfer_del(xfer);
      return 0;
    }
  }
  return xfer;
}

/* Drop content.
 */
 
void http_xfer_clear(struct http_xfer *xfer) {
  if (!xfer) return;
  xfer->preamble.c=0;
  while (xfer->headerc>0) {
    xfer->headerc--;
    struct http_header *header=xfer->headerv+xfer->headerc;
    if (header->k) free(header->k);
    if (header->v) free(header->v);
  }
  xfer->body.c=0;
}

/* Set preamble.
 */
 
int http_xfer_set_preamble(struct http_xfer *xfer,const char *src,int srcc) { 
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  xfer->preamble.c=0;
  if (encode_raw(&xfer->preamble,src,srcc)<0) return -1;
  return 0;
}

int http_xfer_set_request_line(struct http_xfer *xfer,int method,const char *path,int pathc,const char *proto,int protoc) {
  if (xfer->role!=HTTP_ROLE_CLIENT) return -1;
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (!proto) protoc=0; else if (protoc<0) { protoc=0; while (proto[protoc]) protoc++; }
  if (!pathc) { path="/"; pathc=1; }
  if (!protoc) { proto="HTTP/1.1"; protoc=8; }
  int i=pathc; while (i-->0) if ((unsigned char)path[i]<=0x20) return -1;
  i=protoc; while (i-->0) if ((unsigned char)proto[i]<=0x20) return -1;
  const char *methodstr=http_method_repr(method);
  if (!methodstr) return -1;
  xfer->preamble.c=0;
  if (encode_fmt(&xfer->preamble,"%s %.*s %.*s",methodstr,pathc,path,protoc,proto)<0) return -1;
  return 0;
}

int http_xfer_set_status_line(struct http_xfer *xfer,const char *proto,int protoc,int status,const char *msg,int msgc) {
  if (xfer->role!=HTTP_ROLE_SERVER) return -1;
  if ((status<100)||(status>999)) return -1;
  if (!proto) protoc=0; else if (protoc<0) { protoc=0; while (proto[protoc]) protoc++; }
  if (!msg) msgc=0; else if (msgc<0) { msgc=0; while (msg[msgc]) msgc++; }
  if (!protoc) { proto="HTTP/1.1"; protoc=8; }
  int i=protoc; while (i-->0) if ((unsigned char)proto[i]<=0x20) return -1;
  i=msgc; while (i-->0) if ((unsigned char)msg[i]<0x20) return -1; // sic '<', spaces are fine here
  xfer->preamble.c=0;
  if (encode_fmt(&xfer->preamble,"%.*s %d %.*s",protoc,proto,status,msgc,msg)<0) return -1;
  return 0;
}

/* Parse preamble.
 */

int http_xfer_get_method(void *dstpp,const struct http_xfer *xfer) {
  if (xfer->role!=HTTP_ROLE_CLIENT) return -1;
  const char *src=xfer->preamble.v;
  int srcc=0;
  while ((srcc<xfer->preamble.c)&&((unsigned char)src[srcc]>0x20)) srcc++;
  if (dstpp) *(const void**)dstpp=src;
  return srcc;
}

int http_xfer_parse_method(const struct http_xfer *xfer) {
  const char *src=0;
  int srcc=http_xfer_get_method(&src,xfer);
  return http_method_eval(src,srcc);
}

int http_xfer_get_path(void *dstpp,const struct http_xfer *xfer) {
  if (xfer->role!=HTTP_ROLE_CLIENT) return -1;
  const char *src=xfer->preamble.v;
  int srcp=0,srcc=xfer->preamble.c;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (dstpp) *(const void**)dstpp=src+srcp;
  int dstc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; dstc++; }
  return dstc;
}

int http_xfer_get_path_only(void *dstpp,const struct http_xfer *xfer) {
  const char *src=0;
  int srcc=http_xfer_get_path(&src,xfer);
  if (srcc<1) return -1;
  if (dstpp) *(const void**)dstpp=src;
  int dstc=0;
  while ((dstc<srcc)&&(src[dstc]!='?')&&(src[dstc]!='#')) dstc++;
  return dstc;
}

int http_xfer_get_protocol(void *dstpp,const struct http_xfer *xfer) {
  const char *src=xfer->preamble.v;
  int srcp=0,srcc=xfer->preamble.c;
  switch (xfer->role) {
    case HTTP_ROLE_CLIENT: {
        while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++;
        while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
        while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++;
        while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
        if (dstpp) *(const void**)dstpp=src+srcp;
        return srcc-srcp;
      } break;
    case HTTP_ROLE_SERVER: {
        if (dstpp) *(const void**)dstpp=src;
        int dstc=0;
        while ((dstc<srcc)&&((unsigned char)src[dstc]>0x20)) dstc++;
        return dstc;
      } break;
  }
  return -1;
}

int http_xfer_get_status(const struct http_xfer *xfer) {
  if (!xfer) return -1;
  if (xfer->role!=HTTP_ROLE_SERVER) return -1;
  const char *src=xfer->preamble.v;
  int srcp=0,srcc=xfer->preamble.c;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *token=src+srcp;
  int tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  int n=0;
  if (sr_int_eval(&n,token,tokenc)>=2) {
    if ((n>=100)&&(n<=999)) return n;
  }
  return -1;
}

int http_xfer_get_status_message(void *dstpp,const struct http_xfer *xfer) {
  if (!xfer) return -1;
  if (xfer->role!=HTTP_ROLE_SERVER) return -1;
  const char *src=xfer->preamble.v;
  int srcp=0,srcc=xfer->preamble.c;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (dstpp) *(const void**)dstpp=src+srcp;
  return srcc-srcp;
}

/* Parse query.
 */
 
static int http_xfer_parse_query_1(
  const char *src,int srcc,
  int (*cb)(void *userdata,const char *k,int kc,const char *v,int vc),
  void *userdata
) {
  int srcp=0,err;
  while (srcp<srcc) {
    const char *k=src+srcp;
    int kc=0;
    while ((srcp<srcc)&&(src[srcp]!='=')&&(src[srcp]!='&')) { srcp++; kc++; }
    const char *v=0;
    int vc=0;
    if ((srcp<srcc)&&(src[srcp]=='=')) {
      srcp++;
      v=src+srcp;
      while ((srcp<srcc)&&(src[srcp]!='&')) { srcp++; vc++; }
    }
    if (err=cb(userdata,k,kc,v,vc)) return err;
    if ((srcp<srcc)&&(src[srcp]=='&')) srcp++;
  }
  return 0;
}

int http_xfer_parse_query(
  const struct http_xfer *xfer,
  int (*cb)(void *userdata,const char *k,int kc,const char *v,int vc),
  void *userdata
) {
  const char *src;
  int srcc,err;
  if ((srcc=http_xfer_get_query(&src,xfer))>0) {
    if (err=http_xfer_parse_query_1(src,srcc,cb,userdata)) return err;
  }
  if ((srcc=http_xfer_get_aux_query(&src,xfer))>0) {
    if (err=http_xfer_parse_query_1(src,srcc,cb,userdata)) return err;
  }
  return 0;
}
  
int http_xfer_get_query(void *dstpp,const struct http_xfer *xfer) {
  const char *path=0;
  int pathc=http_xfer_get_path(&path,xfer);
  if (pathc<1) return 0;
  int p=0;
  for (;p<pathc;p++) {
    if (path[p]=='?') {
      if (dstpp) *(const void**)dstpp=path+p+1;
      return pathc-p-1;
    }
  }
  return 0;
}

int http_xfer_get_aux_query(void *dstpp,const struct http_xfer *xfer) {
  const char *ct=0;
  int ctc=http_xfer_get_header(&ct,xfer,"Content-Type",12);
  if ((ctc!=33)||memcmp(ct,"application/x-www-form-urlencoded",33)) return 0;
  if (dstpp) *(void**)dstpp=xfer->body.v;
  return xfer->body.c;
}

struct http_xfer_get_query {
  void *dstpp;
  int result;
  const char *k;
  int kc;
};

static int http_xfer_get_query_cb(void *userdata,const char *k,int kc,const char *v,int vc) {
  struct http_xfer_get_query *ctx=userdata;
  if (kc!=ctx->kc) return 0;
  if (memcmp(k,ctx->k,kc)) return 0;
  if (ctx->dstpp) *(const void**)ctx->dstpp=v;
  ctx->result=vc;
  return 1;
}
  
int http_xfer_get_query_string(void *dstpp,const struct http_xfer *xfer,const char *k,int kc) {
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  struct http_xfer_get_query ctx={
    .dstpp=dstpp,
    .result=-1,
    .k=k,
    .kc=kc,
  };
  http_xfer_parse_query(xfer,http_xfer_get_query_cb,&ctx);
  return ctx.result;
}

int http_xfer_get_query_int(const struct http_xfer *xfer,const char *k,int kc,int fallback) {
  const char *src=0;
  int srcc=http_xfer_get_query_string(&src,xfer,k,kc);
  if (srcc<0) return fallback;
  int n;
  if (sr_int_eval(&n,src,srcc)<2) return fallback;
  return n;
}

int http_xfer_decode_query_string(struct encoder *dst,const struct http_xfer *xfer,const char *k,int kc) {
  const char *src=0;
  int srcc=http_xfer_get_query_string(&src,xfer,k,kc);
  if (srcc<=0) return 0;
  while (1) {
    int err=sr_urlencode_decode(dst->v+dst->c,dst->a-dst->c,src,srcc);
    if (err<0) return -1;
    if (dst->c<=dst->a-err) {
      dst->c+=err;
      return 0;
    }
    if (encoder_require(dst,err)<0) return -1;
  }
}

struct http_xfer_query_present {
  const char *k;
  int kc;
  int result;
};

static int http_xfer_query_present_cb(void *userdata,const char *k,int kc,const char *v,int vc) {
  struct http_xfer_query_present *ctx=userdata;
  if ((kc==ctx->kc)&&!memcmp(k,ctx->k,kc)) {
    ctx->result=1;
    return 1;
  }
  return 0;
}

int http_xfer_query_present(const struct http_xfer *xfer,const char *k,int kc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  struct http_xfer_query_present ctx={.k=k,.kc=kc};
  http_xfer_parse_query(xfer,http_xfer_query_present_cb,&ctx);
  return ctx.result;
}

/* Get header.
 */

int http_xfer_get_header(void *dstpp,const struct http_xfer *xfer,const char *k,int kc) {
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  const struct http_header *header=xfer->headerv;
  int i=xfer->headerc;
  for (;i-->0;header++) {
    if (kc!=header->kc) continue;
    if (sr_memcasecmp(header->k,k,kc)) continue;
    if (dstpp) *(void**)dstpp=header->v;
    return header->vc;
  }
  return -1;
}

int http_xfer_get_header_int(const struct http_xfer *xfer,const char *k,int kc,int fallback) {
  const char *v=0;
  int vc=http_xfer_get_header(&v,xfer,k,kc);
  if (vc<0) return fallback;
  int n;
  if (sr_int_eval(&n,v,vc)<2) return fallback;
  return n;
}

/* Add header.
 */
 
static int http_xfer_headerv_require(struct http_xfer *xfer) {
  if (xfer->headerc<xfer->headera) return 0;
  int na=xfer->headera+8;
  if (na>INT_MAX/sizeof(struct http_header)) return -1;
  void *nv=realloc(xfer->headerv,sizeof(struct http_header)*na);
  if (!nv) return -1;
  xfer->headerv=nv;
  xfer->headera=na;
  return 0;
}

int http_xfer_set_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc) {
  if (!xfer) return -1;
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  char *nv=malloc(vc+1);
  if (!nv) return -1;
  memcpy(nv,v,vc);
  nv[vc]=0;
  
  if (http_xfer_headerv_require(xfer)<0) return -1;
  struct http_header *header=xfer->headerv;
  int i=xfer->headerc;
  for (;i-->0;header++) {
    if (header->kc!=kc) continue;
    if (sr_memcasecmp(header->k,k,kc)) continue;
    if (header->v) free(header->v);
    header->v=nv;
    header->vc=vc;
    return 0;
  }
  
  if (!(header->k=malloc(kc+1))) return -1;
  memcpy(header->k,k,kc);
  header->k[kc]=0;
  header->kc=kc;
  header->v=nv;
  header->vc=vc;
  xfer->headerc++;
  return 0;
}

int http_xfer_add_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc) {
  if (!xfer) return -1;
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!kc) return -1;
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  if (http_xfer_headerv_require(xfer)<0) return -1;
  struct http_header *header=xfer->headerv+xfer->headerc;
  if (!(header->k=malloc(kc+1))) return -1;
  if (!(header->v=malloc(vc+1))) {
    free(header->k);
    return -1;
  }
  memcpy(header->k,k,kc);
  memcpy(header->v,v,vc);
  header->k[kc]=0;
  header->v[vc]=0;
  header->kc=kc;
  header->vc=vc;
  xfer->headerc++;
  return 0;
}

int http_xfer_set_header_int(struct http_xfer *xfer,const char *k,int kc,int v) {
  char tmp[32];
  int tmpc=sr_decsint_repr(tmp,sizeof(tmp),v);
  if (tmpc<0) tmpc=0;
  return http_xfer_set_header(xfer,k,kc,tmp,tmpc);
}

/* One-shot response, no body.
 */
 
int http_respond(struct http_xfer *xfer,int status,const char *msgfmt,...) {
  http_xfer_clear(xfer);
  char msg[256];
  int msgc=0;
  if (msgfmt&&msgfmt[0]) {
    va_list vargs;
    va_start(vargs,msgfmt);
    msgc=vsnprintf(msg,sizeof(msg),msgfmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  }
  if (http_xfer_set_status_line(xfer,0,0,status,msg,msgc)<0) return -1;
  return 0;
}

#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

/* Delete.
 */
 
void http_listener_del(struct http_listener *listener) {
  if (!listener) return;
  if (listener->refc-->1) return;
  
  if (listener->methodv) free(listener->methodv);
  if (listener->prefix) free(listener->prefix);
  
  free(listener);
}

/* Retain.
 */
 
int http_listener_ref(struct http_listener *listener) {
  if (!listener) return -1;
  if (listener->refc<1) return -1;
  if (listener->refc==INT_MAX) return -1;
  listener->refc++;
  return 0;
}

/* New.
 */
 
struct http_listener *http_listener_new(const struct http_listener_delegate *delegate) {
  struct http_listener *listener=calloc(1,sizeof(struct http_listener));
  if (!listener) return 0;
  
  listener->refc=1;
  if (delegate) memcpy(&listener->delegate,delegate,sizeof(struct http_listener_delegate));
  
  return listener;
}

/* Trivial accessors.
 */
 
void *http_listener_get_userdata(const struct http_listener *listener) {
  return listener->delegate.userdata;
}

/* Add method.
 */
 
int http_listener_add_method(struct http_listener *listener,int method) {
  if (method<1) return -1;
  if (listener->methodc>=listener->methoda) {
    int na=listener->methoda+2;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(listener->methodv,sizeof(int)*na);
    if (!nv) return -1;
    listener->methodv=nv;
    listener->methoda=na;
  }
  listener->methodv[listener->methodc++]=method;
  return 0;
}

/* Set path prefix.
 */
 
int http_listener_set_prefix(struct http_listener *listener,const char *prefix,int prefixc) {
  if (!prefix) prefixc=0; else if (prefixc<0) { prefixc=0; while (prefix[prefixc]) prefixc++; }
  while ((prefixc>1)&&(prefix[prefixc-1]=='/')) prefixc--;
  char *nv=0;
  if (prefixc) {
    if (!(nv=malloc(prefixc+1))) return -1;
    memcpy(nv,prefix,prefixc);
    nv[prefixc]=0;
  }
  if (listener->prefix) free(listener->prefix);
  listener->prefix=nv;
  listener->prefixc=prefixc;
  return 0;
}

/* Match request.
 */
 
int http_listener_match(struct http_listener *listener,const struct http_xfer *req) {
  if (!listener||!req) return 0;
  
  if (listener->methodc) {
    int method=http_xfer_parse_method(req);
    const int *v=listener->methodv;
    int i=listener->methodc,ok=0;
    for (;i-->0;v++) {
      if (*v==method) {
        ok=1;
        break;
      }
    }
    if (!ok) return 0;
  }
  
  if (listener->prefixc) {
    const char *path=0;
    int pathc=http_xfer_get_path_only(&path,req);
    if (pathc<listener->prefixc) return 0;
    if (memcmp(path,listener->prefix,listener->prefixc)) return 0;
    if ((listener->prefixc<pathc)&&(path[listener->prefixc]!='/')) return 0;
  }
  
  if (listener->delegate.cb_match) {
    return listener->delegate.cb_match(listener,req);
  }
  return 1;
}

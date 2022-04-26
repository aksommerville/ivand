#include "http1.h"
#include "http.h"
#include "tool/common/poller.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

static int http1_check_io(struct http1 *http1);

/* Delete context.
 */

void http1_del(struct http1 *http1) {
  if (!http1) return;
  
  if (http1->conn) {
    if (http1->poller) poller_remove_file(http1->poller,http1->conn->fd);
    http_conn_del(http1->conn);
    http1->conn=0;
  }
  if (http1->websocket) {
    if (http1->poller) poller_remove_file(http1->poller,http1->websocket->fd);
    http_conn_del(http1->websocket);
    http1->websocket=0;
  }
  
  if (http1->poller) {
    poller_del(http1->poller);
    http1->poller=0;
  }
  
  if (http1->requestv) {
    while (http1->requestc>0) {
      http1->requestc--;
      struct http1_request *request=http1->requestv+http1->requestp++;
      if (http1->requestp>=http1->requesta) http1->requestp=0;
      if (request->cb_err) {
        request->cb_err(request->userdata,0);
      }
      http_xfer_del(request->req);
    }
    free(http1->requestv);
  }
  
  if (http1->host) free(http1->host);
  
  free(http1);
}

/* New context.
 */
 
static int http1_init(struct http1 *http1,const char *host,int port,struct poller *poller) {
  
  int hostc=0;
  while (host[hostc]) hostc++;
  if (!(http1->host=malloc(hostc+1))) return -1;
  memcpy(http1->host,host,hostc);
  http1->host[hostc]=0;
  http1->hostc=hostc;
  
  http1->port=port;
  http1->reqid_next=1;
  http1->connfd=-1;
  
  if (poller) {
    if (poller_ref(poller)<0) return -1;
    http1->poller=poller;
  } else {
    if (!(http1->poller=poller_new())) return -1;
  }
  
  return 0;
}
 
struct http1 *http1_new(const char *host,int port,struct poller *poller) {
  if (!host||!host[0]) return 0;
  if ((port<1)||(port>0xffff)) return 0;
  
  struct http1 *http1=calloc(1,sizeof(struct http1));
  if (!http1) return 0;
  if (http1_init(http1,host,port,poller)<0) {
    http1_del(http1);
    return 0;
  }
  
  return http1;
}

/* I/O callbacks.
 */
 
static int http1_cb_write_complete(struct http_conn *conn) {
  struct http1 *http1=conn->delegate.userdata;
  poller_set_writeable(http1->poller,conn->fd,0);
  return 0;
}
 
static int http1_cb_response_ready(struct http_conn *conn,struct http_xfer *resp) {
  struct http1 *http1=conn->delegate.userdata;
  
  // Find the request. Don't panic if it's missing, it could have been cancelled.
  struct http1_request *request=0;
  while (1) {
    if (http1->requestc<1) { request=0; break; }
    request=http1->requestv+http1->requestp;
    if (!request->reqid) {
      http1->requestc--;
      http1->requestp++;
      if (http1->requestp>=http1->requesta) http1->requestp=0;
      continue;
    }
    break;
  }
  if (request->reqid!=http1->reqid_pending) {
    return http1_check_io(http1);
  }
  
  http_xfer_del(request->req);
  http1->requestc--;
  http1->requestp++;
  if (http1->requestp>=http1->requesta) http1->requestp=0;
  http1->reqid_pending=0;
  int err=0;
  if (request->cb_ok) err=request->cb_ok(request->userdata,resp);
  
  if (http1_check_io(http1)<0) err=-1;
  return err;
}

static int http1_cb_writeable(int fd,void *userdata) {
  struct http1 *http1=userdata;
  return http_conn_write(http1->conn);
}

static int http1_cb_readable(int fd,void *userdata) {
  struct http1 *http1=userdata;
  int err=http_conn_read(http1->conn);
  if (err) {
    if (err<0) {
      fprintf(stderr,"http1: Lost connection. fd=%d\n",http1->connfd);
      poller_remove_file(http1->poller,http1->connfd);
      http_conn_del(http1->conn);
      http1->conn=0;
      http1->connfd=-1;
      return 0;
    }
  }
  return err;
}

/* Reconnect if needed.
 */
 
int http1_conn_require(struct http1 *http1) {
  if (http1->conn) return 0;
  struct http_conn_delegate delegate={
    .userdata=http1,
    .response_ready=http1_cb_response_ready,
    .write_complete=http1_cb_write_complete,
  };
  if (!(http1->conn=http_conn_new_tcp_client(&delegate,http1->host,http1->hostc,http1->port))) {
    //fprintf(stderr,"http1: Failed to connect to %.*s:%d\n",http1->hostc,http1->host,http1->port);
    return -1;
  }
  
  struct poller_file file={
    .userdata=http1,
    .fd=http1->conn->fd,
    .ownfd=0,
    .cb_writeable=http1_cb_writeable,
    .cb_readable=http1_cb_readable,
  };
  if (poller_add_file(http1->poller,&file)<0) {
    http_conn_del(http1->conn);
    http1->conn=0;
    return -1;
  }
  
  http1->connfd=http1->conn->fd;
  fprintf(stderr,"http1: Connected to %.*s:%d, fd=%d\n",http1->hostc,http1->host,http1->port,http1->conn->fd);
  return 0;
}

/* Check I/O status. Encode the next request if ready.
 */
 
static int http1_check_io(struct http1 *http1) {
  while (1) {
    if (http1->reqid_pending) return 0;
    if (http1->requestc<1) return 0;
    struct http1_request *request=http1->requestv+http1->requestp;
    
    // Defunct request (eg was cancelled but had neighbors fore and aft). Drop it.
    if (!request->reqid) {
      http1->requestc--;
      http1->requestp++;
      if (http1->requestp>=http1->requesta) http1->requestp=0;
      continue;
    }
  
    if (http_conn_encode_xfer(http1->conn,request->req)<0) {
      fprintf(stderr,"%s failed to encode request!\n",__func__);
      if (request->cb_err) request->cb_err(request->userdata,0);
      http_xfer_del(request->req);
      http1->requestc--;
      http1->requestp++;
      if (http1->requestp>=http1->requesta) http1->requestp=0;
      continue;
    }
    
    poller_set_writeable(http1->poller,http1->conn->fd,1);
    http1->reqid_pending=request->reqid;
    //fprintf(stderr,"SENDING REQUEST: %.*s\n",request->req->preamble.c,request->req->preamble.v);
    
    break;
  }
  return 0;
}

/* Ensure some head room exists in requestv.
 */
 
static int http1_requestv_require(struct http1 *http1) {
  if (http1->requestc<http1->requesta) return 0;
  int na=http1->requesta+32;
  if (na>INT_MAX/sizeof(struct http1_request)) return -1;
  void *nv=realloc(http1->requestv,sizeof(struct http1_request)*na);
  if (!nv) return -1;
  http1->requestv=nv;
  
  // We are a ring buffer, so there can be two segments.
  int tailc=http1->requesta-http1->requestp;
  if (tailc<http1->requestc) { // ...it is indeed split
    int np=na-tailc;
    memmove(http1->requestv+np,http1->requestv+http1->requestp,sizeof(struct http1_request)*tailc);
    http1->requestp=np;
  }
  
  http1->requesta=na;
  return 0;
}

/* Cancel every pending request and reset reqid_next to 1.
 * This happens every 2 billion requests or so, and it's not pretty.
 */
 
static void http1_reset_reqid(struct http1 *http1) {
  if (http1->requestc) {
    fprintf(stderr,"http1:WARNING: Cancelling %d requests because we've exhausted the reqid space.\n",http1->requestc);
  }
  while (http1->requestc>0) {
    struct http1_request *request=http1->requestv+http1->requestp++;
    if (http1->requestp>=http1->requesta) http1->requestp=0;
    http1->requestc--;
    http_xfer_del(request->req);
    if (request->cb_err) request->cb_err(request->userdata,0);
  }
  http1->reqid_next=1;
}

/* Request, with prepared xfer.
 */

int http1_request(
  struct http1 *http1,
  struct http_xfer *xfer,
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
) {
  if (!http1) return -1;
  if (http1->reqid_next==INT_MAX) http1_reset_reqid(http1);
  if (http1_requestv_require(http1)<0) return -1;
  if (http1_conn_require(http1)<0) return -1;
  if (http_xfer_ref(xfer)<0) return -1;
  int p=http1->requestp+http1->requestc++;
  if (p>=http1->requesta) p-=http1->requesta;
  struct http1_request *request=http1->requestv+p;
  request->reqid=http1->reqid_next++;
  request->req=xfer;
  request->userdata=userdata;
  request->cb_ok=cb_ok;
  request->cb_err=cb_err;
  if (http1_check_io(http1)<0) {
    http1->requestc--;
    http_xfer_del(request->req);
    return -1;
  }
  return request->reqid;
}

/* GET helper.
 */
 
int http1_get(
  struct http1 *http1,
  const char *path,int pathc, // with encoded query
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
) {
  if (!http1) return -1;
  struct http_xfer *xfer=http_xfer_new(HTTP_ROLE_CLIENT);
  if (!xfer) return -1;
  if (http_xfer_set_request_line(xfer,HTTP_METHOD_GET,path,pathc,0,0)<0) {
    http_xfer_del(xfer);
    return -1;
  }
  int reqid=http1_request(http1,xfer,userdata,cb_ok,cb_err);
  http_xfer_del(xfer);
  return reqid;
}

/* POST helper.
 */
 
int http1_post(
  struct http1 *http1,
  const char *path,int pathc, // with encoded query
  const void *body,int bodyc,
  const char *content_type,
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
) {
  if (!http1) return -1;
  struct http_xfer *xfer=http_xfer_new(HTTP_ROLE_CLIENT);
  if (!xfer) return -1;
  if (
    (http_xfer_set_request_line(xfer,HTTP_METHOD_POST,path,pathc,0,0)<0)||
    (content_type&&content_type[0]&&(http_xfer_add_header(xfer,"Content-Type",12,content_type,-1)<0))||
    (encode_raw(&xfer->body,body,bodyc)<0)||
  0) {
    http_xfer_del(xfer);
    return -1;
  }
  int reqid=http1_request(http1,xfer,userdata,cb_ok,cb_err);
  http_xfer_del(xfer);
  return reqid;
}

/* PUT helper.
 */
 
int http1_put(
  struct http1 *http1,
  const char *path,int pathc,
  const void *body,int bodyc,
  const char *content_type,
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
) {
  if (!http1) return -1;
  struct http_xfer *xfer=http_xfer_new(HTTP_ROLE_CLIENT);
  if (!xfer) return -1;
  if (
    (http_xfer_set_request_line(xfer,HTTP_METHOD_PUT,path,pathc,0,0)<0)||
    (content_type&&content_type[0]&&(http_xfer_add_header(xfer,"Content-Type",12,content_type,-1)<0))||
    (encode_raw(&xfer->body,body,bodyc)<0)||
  0) {
    http_xfer_del(xfer);
    return -1;
  }
  int reqid=http1_request(http1,xfer,userdata,cb_ok,cb_err);
  http_xfer_del(xfer);
  return reqid;
}

/* DELETE helper.
 */
 
int http1_delete(
  struct http1 *http1,
  const char *path,int pathc,
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
) {
  if (!http1) return -1;
  struct http_xfer *xfer=http_xfer_new(HTTP_ROLE_CLIENT);
  if (!xfer) return -1;
  if (
    (http_xfer_set_request_line(xfer,HTTP_METHOD_DELETE,path,pathc,0,0)<0)||
  0) {
    http_xfer_del(xfer);
    return -1;
  }
  int reqid=http1_request(http1,xfer,userdata,cb_ok,cb_err);
  http_xfer_del(xfer);
  return reqid;
}

/* Cancel request.
 */

void *http1_cancel(struct http1 *http1,int reqid) {
  if (!http1) return 0;
  int i=http1->requestc;
  while (i-->0) {
    int p=http1->requestp+i;
    if (p>=http1->requesta) p-=http1->requesta;
    struct http1_request *request=http1->requestv+p;
    if (request->reqid!=reqid) continue;
    
    void *userdata=request->userdata;
    http_xfer_del(request->req);
    memset(request,0,sizeof(struct http1_request));
    
    // If it was the first or last, shorten the list.
    // Otherwise, it's enough to zero the request record. We'll skip it when it arrives for service.
    if (i==http1->requestc-1) http1->requestc--;
    else if (!i) {
      http1->requestc--;
      http1->requestp++;
      if (http1->requestp>=http1->requesta) http1->requestp=0;
    }
    
    return userdata;
  }
  return 0;
}

/* WebSocket packet received.
 */
 
static int http1_cb_websocket(struct http_conn *conn,int type,const void *src,int srcc) {
  struct http1 *http1=conn->delegate.userdata;
  
  if (type==8) {
    poller_remove_file(http1->poller,conn->fd);
    http_conn_del(http1->websocket);
    http1->websocket=0;
  }
  
  if (http1->cb_websocket) {
    if (http1->cb_websocket(conn,type,src,srcc,http1->userdata)<0) return -1;
  }
  
  return 0;
}

static int http1_cb_websocket_writeable(int fd,void *userdata) {
  struct http1 *http1=userdata;
  struct http_conn *conn=http1->websocket;
  if (conn->wbufp>=conn->wbuf.c) {
    return poller_set_writeable(http1->poller,fd,0);
  } else {
    return http_conn_write(conn);
  }
}

static int http1_cb_websocket_readable(int fd,void *userdata) {
  struct http1 *http1=userdata;
  struct http_conn *conn=http1->websocket;
  return http_conn_read(conn);
}

static int http1_cb_websocket_defunct(void *userdata) {
  struct http1 *http1=userdata;
  if (http1->websocket&&(http1->websocket->fd<0)) {
    http_conn_del(http1->websocket);
    http1->websocket=0;
  }
  return 0;
}

static int http1_cb_websocket_eof(struct http_conn *conn) {
  struct http1 *http1=conn->delegate.userdata;
  poller_remove_file(http1->poller,conn->fd);
  // Don't delete the connection right now; we are its sole owner and it still has cleanup to do.
  poller_set_timeout(http1->poller,10,http1_cb_websocket_defunct,http1);
  return 0;
}

/* Connect WebSocket.
 */

int http1_websocket_connect(
  struct http1 *http1,
  int (*cb)(struct http_conn *conn,int type,const void *src,int srcc,void *userdata),
  void *userdata
) {
  if (http1->websocket) {
    if (http1->websocket->fd<0) { // defunct; drop it now
      http1_cb_websocket_defunct(http1);
    }
  }
  
  struct http_conn_delegate delegate={
    .userdata=http1,
    .ws_packet=http1_cb_websocket,
    .eof=http1_cb_websocket_eof,
  };
  if (!(http1->websocket=http_conn_new_tcp_client(&delegate,http1->host,http1->hostc,http1->port))) {
    return -1;
  }
  if (http_conn_initiate_websocket(http1->websocket)<0) {
    http1_cb_websocket_defunct(http1);
    return -1;
  }
  
  struct poller_file file={
    .userdata=http1,
    .fd=http1->websocket->fd,
    .ownfd=0,
    .cb_writeable=http1_cb_websocket_writeable,
    .cb_readable=http1_cb_websocket_readable,
    .writeable=1,
  };
  if (poller_add_file(http1->poller,&file)<0) {
    http_conn_del(http1->websocket);
    http1->websocket=0;
    return -1;
  }
  
  http1->cb_websocket=cb;
  http1->userdata=userdata;
  return 0;
}

/* Send WebSocket packet.
 */

int http1_websocket_send(struct http1 *http1,int type,const void *src,int srcc) {
  if (!http1) return -1;
  if (!http1->websocket||(http1->websocket->fd<0)) {
    if (!http1->cb_websocket) return -1; // must have explicitly connected before
    if (http1_websocket_connect(http1,http1->cb_websocket,http1->userdata)<0) return -1;
  }
  poller_set_writeable(http1->poller,http1->websocket->fd,1);
  return http_conn_send_websocket(http1->websocket,type,src,srcc);
}

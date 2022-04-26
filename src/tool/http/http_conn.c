#include "http.h"
#include "tool/common/poller.h"
#include "tool/common/decoder.h"
#include "tool/common/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>

/* Delete.
 */
 
void http_conn_del(struct http_conn *conn) {
  if (!conn) return;
  if (conn->refc-->1) return;
  if ((conn->fd>=0)&&conn->ownfd) close(conn->fd);
  encoder_cleanup(&conn->rbuf);
  encoder_cleanup(&conn->wbuf);
  http_xfer_del(conn->xfer);
  if (conn->remotehost) free(conn->remotehost);
  http_listener_del(conn->wslistener);
  free(conn);
}

/* Retain.
 */
 
int http_conn_ref(struct http_conn *conn) {
  if (!conn) return -1;
  if (conn->refc<1) return -1;
  if (conn->refc==INT_MAX) return -1;
  conn->refc++;
  return 0;
}

/* New unconfigured conn.
 */

struct http_conn *http_conn_new(const struct http_conn_delegate *delegate) {
  struct http_conn *conn=calloc(1,sizeof(struct http_conn));
  if (!conn) return 0;
  conn->refc=1;
  conn->fd=-1;
  conn->ownfd=1;
  conn->role=HTTP_ROLE_SERVER; // until someone says otherwise
  if (delegate) {
    memcpy(&conn->delegate,delegate,sizeof(struct http_conn_delegate));
    if (delegate->request_ready) conn->role=HTTP_ROLE_SERVER;
    else if (delegate->response_ready) conn->role=HTTP_ROLE_CLIENT;
  }
  return conn;
}

/* New conn with existing fd.
 */
 
struct http_conn *http_conn_new_borrow(const struct http_conn_delegate *delegate,int fd) {
  struct http_conn *conn=http_conn_new(delegate);
  if (!conn) return 0;
  conn->fd=fd;
  conn->ownfd=0;
  return conn;
}

struct http_conn *http_conn_new_handoff(const struct http_conn_delegate *delegate,int fd) {
  struct http_conn *conn=http_conn_new(delegate);
  if (!conn) return 0;
  conn->fd=fd;
  conn->ownfd=1;
  return conn;
}

/* Connect client.
 */
 
static int http_conn_connect(struct http_conn *conn,const char *host,int hostc,int port) {
  if (conn->fd>=0) return -1;
  
  struct addrinfo hints={
    .ai_flags=AI_ADDRCONFIG,
    .ai_socktype=SOCK_STREAM,
  };
  struct addrinfo *ai=0;
  char zhost[64];
  if (hostc>=sizeof(zhost)) return -1;
  memcpy(zhost,host,hostc);
  zhost[hostc]=0;
  char servicename[32];
  snprintf(servicename,sizeof(servicename),"%d",port);
  if (getaddrinfo(zhost,servicename,&hints,&ai)<0) {
    return -1;
  }
  if (!ai) return -1;
  
  int fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
  if (fd<0) {
    freeaddrinfo(ai);
    return -1;
  }
  if (connect(fd,ai->ai_addr,ai->ai_addrlen)<0) {
    freeaddrinfo(ai);
    close(fd);
    return -1;
  }
  freeaddrinfo(ai);
  
  conn->fd=fd;
  conn->ownfd=1;
  
  if (conn->remotehost) free(conn->remotehost);
  if (conn->remotehost=malloc(hostc+1)) {
    memcpy(conn->remotehost,host,hostc);
    conn->remotehost[hostc]=0;
  }
  conn->remoteport=port;
  
  return 0;
}

/* Encode request or response.
 */
 
int http_conn_encode_xfer(struct http_conn *conn,struct http_xfer *request) {
  if (encode_fmt(&conn->wbuf,"%.*s\r\n",request->preamble.c,request->preamble.v)<0) return -1;
  const struct http_header *header=request->headerv;
  int i=request->headerc;
  for (;i-->0;header++) {
    if (encode_fmt(&conn->wbuf,"%.*s: %.*s\r\n",header->kc,header->k,header->vc,header->v)<0) return -1;
  }
  if ((request->role==HTTP_ROLE_SERVER)||http_method_expects_body(http_xfer_parse_method(request))) {
    if (encode_fmt(&conn->wbuf,"Content-Length: %d\r\n",request->body.c)<0) return -1;
    if (encode_raw(&conn->wbuf,"\r\n",2)<0) return -1;
    if (encode_raw(&conn->wbuf,request->body.v,request->body.c)<0) return -1;
    //TODO Is this too much body-copying?
  } else {
    if (encode_raw(&conn->wbuf,"\r\n",2)<0) return -1;
  }
  return 0;
}

/* New unconnected client xfer from structured request.
 */
 
struct http_conn *http_conn_new_client(
  const struct http_conn_delegate *delegate,
  struct http_xfer *request
) {
  if (!request||(request->role!=HTTP_ROLE_CLIENT)) return 0;
  struct http_conn *conn=http_conn_new(delegate);
  if (!conn) return 0;
  conn->role=HTTP_ROLE_CLIENT;
  if (http_conn_encode_xfer(conn,request)<0) {
    http_conn_del(conn);
    return 0;
  }
  return conn;
}

/* New connected client from unstructured request.
 */
 
struct http_conn *http_conn_new_request(
  const struct http_conn_delegate *delegate,
  int method,const char *url,int urlc,
  const char *content_type,
  const void *body,int bodyc
) {
  struct http_url spliturl={0};
  if (http_url_split(&spliturl,url,urlc)<0) return 0;
  if (!spliturl.port) spliturl.port=80;
  
  struct http_xfer *request=http_xfer_new_request(method,spliturl.path,spliturl.pathc,content_type,body,bodyc);
  if (!request) return 0;
  
  struct http_conn *conn=http_conn_new_client(delegate,request);
  http_xfer_del(request);
  if (!conn) return 0;
  
  if (http_conn_connect(conn,spliturl.host,spliturl.hostc,spliturl.port)<0) {
    http_conn_del(conn);
    return 0;
  }
  
  return conn;
}

/* New connected client without an HTTP request.
 */
 
struct http_conn *http_conn_new_tcp_client(
  const struct http_conn_delegate *delegate,
  const char *host,int hostc,int port
) {
  struct http_conn *conn=http_conn_new(delegate);
  if (!conn) return 0;
  conn->role=HTTP_ROLE_CLIENT;
  if (http_conn_connect(conn,host,hostc,port)<0) {
    http_conn_del(conn);
    return 0;
  }
  return conn;
}

/* Trivial accessors.
 */
 
void *http_conn_get_userdata(const struct http_conn *conn) {
  return conn->delegate.userdata;
}

/* I/O status.
 */
 
char http_conn_get_io_status(const struct http_conn *conn) {
  if (conn->fd<0) return '!';
  if (conn->wbufp<conn->wbuf.c) return 'w';
  return 'r';
}

/* Close connection.
 */
 
static void _http_conn_close(struct http_conn *conn) {
  if (conn->fd>=0) {
    if (conn->context) poller_remove_file(conn->context->poller,conn->fd);
    if (conn->ownfd) close(conn->fd);
    conn->fd=-1;
  }
}

/* Log transaction, after encoding the response.
 * (resp) is allowed to be null, for emergency error cases only.
 */
 
static void http_conn_log_transaction(struct http_conn *conn,struct http_xfer *req,struct http_xfer *resp) {
  time_t now=time(0);
  struct tm tm={0};
  localtime_r(&now,&tm); // or gmtime_r()? Since we only operate on the one site, I think local is friendlier.
  int status=resp?http_xfer_get_status(resp):0;
  if ((status<0)||(status>999)) status=0;
  const char *method=0,*path=0;
  int methodc=http_xfer_get_method(&method,req); 
  if (methodc<1) { method="?"; methodc=1; }
  int pathc=http_xfer_get_path(&path,req);
  if (pathc<1) { path="?"; pathc=1; }
  fprintf(stderr,
    "%04d-%02d-%02dT%02d:%02d:%02d %3d %.*s %.*s => %d\n",
    tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,
    tm.tm_hour,tm.tm_min,tm.tm_sec,
    status,methodc,method,pathc,path,
    resp?resp->body.c:0
  );
}

/* Last chance to report a sane error.
 */
 
static int http_conn_emergency_response(struct http_conn *conn) {
  http_conn_log_transaction(conn,conn->xfer,0);
  conn->wbufp=0;
  conn->wbuf.c=0;
  return encode_raw(&conn->wbuf,
    "HTTP/1.1 500 Internal server error\r\n"
    "Content-Length: 0\r\n"
    "\r\n"
  ,-1);
}

/* Populate xfer with the canned errors.
 * Leaving open the possibility that we might send richer data here.
 */
 
static int http_conn_set_error_no_listener(struct http_conn *conn,struct http_xfer *xfer) {
  http_xfer_clear(xfer);
  if (
    (http_xfer_set_status_line(xfer,0,0,404,"Not found",-1)<0)
  ) return -1;
  return 0;
}

static int http_conn_set_error_listener_fail(struct http_conn *conn,struct http_xfer *xfer) {
  http_xfer_clear(xfer);
  if (
    (http_xfer_set_status_line(xfer,0,0,500,"Internal server error",-1)<0)
  ) return -1;
  return 0;
}

static int http_conn_set_error_websocket_fail(struct http_conn *conn,struct http_xfer *xfer) {
  http_xfer_clear(xfer);
  if (
    (http_xfer_set_status_line(xfer,0,0,500,"Internal server error",-1)<0)
  ) return -1;
  return 0;
}

/* Sanitize response prior to encoding.
 */
 
static int http_conn_sanitize_response(struct http_conn *conn,struct http_xfer *req,struct http_xfer *resp) {
  if (!resp->preamble.c) {
    if (http_xfer_set_status_line(resp,0,0,200,"OK",2)<0) return -1;
  }
  if (resp->body.c) {
    if (http_xfer_get_header(0,resp,"Content-Type",12)<0) {
      const char *path=0;
      int pathc=http_xfer_get_path_only(&path,req);
      if (http_xfer_add_header(resp,"Content-Type",12,http_guess_content_type(path,pathc,resp->body.v,resp->body.c),-1)<0) return -1;
    }
  }
  return 0;
}

/* Upgrade to WebSocket, server side.
 */
 
static int http_conn_websocket_handshake(struct http_conn *conn,struct http_xfer *resp,struct http_listener *listener) {

  const char *hupgrade,*hkey;
  int hupgradec=http_xfer_get_header(&hupgrade,conn->xfer,"Upgrade",7);
  if ((hupgradec!=9)||sr_memcasecmp(hupgrade,"websocket",9)) return -1;
  int hkeyc=http_xfer_get_header(&hkey,conn->xfer,"Sec-WebSocket-Key",17);
  if (hkeyc<1) return -1;
  // There should also be a "Connection" header with "Upgrade" in its comma-delimited list, but the "Upgrade" header covers it.

  char prekey[256],digest[20];
  int prekeyc=hkeyc+36;
  if (prekeyc>sizeof(prekey)) return -1;
  memcpy(prekey,hkey,hkeyc);
  memcpy(prekey+hkeyc,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);
  sr_sha1(digest,sizeof(digest),prekey,prekeyc);
  char postkey[256];
  int postkeyc=sr_base64_encode(postkey,sizeof(postkey),digest,20);
  if ((postkeyc<0)||(postkeyc>sizeof(postkey))) return -1;
  
  if (http_xfer_set_header(resp,"Sec-WebSocket-Accept",20,postkey,postkeyc)<0) return -1;
  if (http_xfer_set_header(resp,"Upgrade",7,"WebSocket",9)<0) return -1;
  if (http_xfer_set_header(resp,"Connection",10,"Upgrade",7)<0) return -1;
  
  if (http_xfer_set_status_line(resp,0,0,101,"Upgrade to WebSocket",-1)<0) return -1;
    
  conn->state=HTTP_STATE_WEBSOCKET;
  
  if (http_listener_ref(listener)<0) return -1;
  http_listener_del(conn->wslistener);
  conn->wslistener=listener;
  
  return 0;
}

/* Deliver response.
 */
 
static int http_conn_deliver_response(struct http_conn *conn) {
  if (conn->delegate.response_ready) {
    if (conn->delegate.response_ready(conn,conn->xfer)<0) return -1;
  }
  if (conn->context) {
    poller_set_timeout(conn->context->poller,5000,(void*)http_context_drop_idle_clients,conn->context);
  }
  return 0;
}

/* Serve incoming request, or deliver response.
 */
 
static int http_conn_respond(struct http_conn *conn) {
  conn->state=HTTP_STATE_IDLE;
  
  if (conn->role==HTTP_ROLE_CLIENT) return http_conn_deliver_response(conn);
  
  // Prepare response container.
  struct http_xfer *resp=http_xfer_new(HTTP_ROLE_SERVER);
  if (!resp) return http_conn_emergency_response(conn);
  
  // Find the listener, generate response.
  struct http_listener *listener=0;
  if (conn->delegate.request_ready) {
    if (conn->delegate.request_ready(conn,conn->xfer,resp)<0) {
      if (http_conn_set_error_listener_fail(conn,resp)<0) {
        http_xfer_del(resp);
        return http_conn_emergency_response(conn);
      }
    }
  } else {
    listener=http_context_find_listener_for_request(conn->context,conn->xfer);
    if (!listener) {
      if (http_conn_set_error_no_listener(conn,resp)<0) {
        http_xfer_del(resp);
        return http_conn_emergency_response(conn);
      }
    } else if (listener->delegate.cb_request) {
      if (listener->delegate.cb_request(listener,conn->xfer,resp)<0) {
        if (http_conn_set_error_listener_fail(conn,resp)<0) {
          http_xfer_del(resp);
          return http_conn_emergency_response(conn);
        }
      }
    } else if (listener->delegate.cb_ws_connect||listener->delegate.cb_ws_recv) {
      if (http_conn_websocket_handshake(conn,resp,listener)<0) {
        if (http_conn_set_error_websocket_fail(conn,resp)<0) {
          http_xfer_del(resp);
          return http_conn_emergency_response(conn);
        }
      }
    }
  }
  
  // Listener might leave things unset -- fill that all in.
  if (http_conn_sanitize_response(conn,conn->xfer,resp)<0) {
    http_xfer_del(resp);
    return http_conn_emergency_response(conn);
  }
  
  // Encode it.
  int err=http_conn_encode_xfer(conn,resp);
  if (err<0) {
    http_xfer_del(resp);
    return http_conn_emergency_response(conn);
  }
  
  http_conn_log_transaction(conn,conn->xfer,resp);
  http_xfer_del(resp);
  
  // Finally, if it upgraded to Websocket, tell the delegate -- don't do it before this point!
  if (conn->state==HTTP_STATE_WEBSOCKET) {
    if (listener&&listener->delegate.cb_ws_connect) {
      if (listener->delegate.cb_ws_connect(listener,conn,conn->xfer,resp)<0) return -1;
    }
  }
  
  return 0;
}

/* Receive preamble.
 */
 
static int http_conn_receive_preamble(struct http_conn *conn,const char *src,int srcc) {
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  
  // Make a fresh xfer to hold the incoming request. Its role is opposite ours.
  // I guess we could keep and reuse... probly doesn't matter.
  if (conn->xfer) {
    http_xfer_del(conn->xfer);
    conn->xfer=0;
  }
  if (conn->role==HTTP_ROLE_CLIENT) {
    if (!(conn->xfer=http_xfer_new(HTTP_ROLE_SERVER))) return -1;
  } else if (conn->role==HTTP_ROLE_SERVER) {
    if (!(conn->xfer=http_xfer_new(HTTP_ROLE_CLIENT))) return -1;
  } else return -1;
  
  if (http_xfer_set_preamble(conn->xfer,src,srcc)<0) {
    fprintf(stderr,"Error parsing HTTP preamble: %.*s\n",srcc,src);
    return -1;
  }
  
  conn->state=HTTP_STATE_HEADER;
  return 0;
}

/* End of headers.
 */
 
static int http_conn_receive_end_of_headers(struct http_conn *conn) {
  
  // If a body is expected, update state accordingly and return.
  int len=http_xfer_get_header_int(conn->xfer,"Content-Length",14,-1);
  if (len>0) {
    conn->bodychunked=0;
    conn->bodyexpect=len;
    conn->state=HTTP_STATE_BODY;
    return 0;
  }
  const char *te=0;
  int tec=http_xfer_get_header(&te,conn->xfer,"Transfer-Encoding",17);
  if ((tec==7)&&!sr_memcasecmp(te,"chunked",7)) {
    conn->bodychunked=1;
    conn->bodyexpect=0;
    conn->state=HTTP_STATE_BODY;
    return 0;
  }
  
  // No body. We're ready to serve it.
  if (http_conn_respond(conn)<0) return -1;
  
  return 0;
}

/* Receive header.
 */
 
static int http_conn_receive_header(struct http_conn *conn,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return http_conn_receive_end_of_headers(conn);
  const char *k=src+srcp;
  int kc=0;
  while ((srcp<srcc)&&(src[srcp++]!=':')) kc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *v=src+srcp;
  int vc=srcc-srcp;
  while (vc&&((unsigned char)v[vc-1]<=0x20)) vc--;
  
  if (http_xfer_add_header(conn->xfer,k,kc,v,vc)<0) return -1;
  
  return 0;
}

/* Receive body.
 */
 
static int http_conn_receive_body(struct http_conn *conn,const char *src,int srcc) {

  // Awaiting raw data? Easy.
  if (conn->bodyexpect>0) {
    int cpc=conn->bodyexpect;
    if (cpc>srcc) cpc=srcc;
    if (encode_raw(&conn->xfer->body,src,cpc)<0) return -1;
    conn->bodyexpect-=cpc;
    if (!conn->bodyexpect&&!conn->bodychunked) {
      if (http_conn_respond(conn)<0) return -1;
    }
    return cpc;
  }
  
  // If we're chunked, bodyexpect==0 means we're awaiting the next chunk length.
  if (conn->bodychunked) {
    int linec=http_measure_line(src,srcc);
    if (linec<1) {
      if (srcc>16) return -1; // give up, what's going on?
      return 0;
    }
    const char *token=src;
    int tokenc=srcc;
    while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
    while (tokenc&&((unsigned char)token[0]<=0x20)) { tokenc--; token++; }
    if (!tokenc) return -1;
    int len=0,i=tokenc;
    for (;i-->0;token++) {
      int digit=sr_hexdigit_eval(*token);
      if (digit<0) return -1;
      if (len&~(INT_MAX>>4)) return -1;
      len<<=4;
      len|=digit;
    }
    if (!len) {
      if (http_conn_respond(conn)<0) return -1;
      return linec;
    }
    conn->bodyexpect=len;
    return linec;
  }
  
  // I don't think this point is reachable, but if so, the meaning is clear: done receiving body.
  return http_conn_respond(conn);
}

/* Decode and deliver incoming Websocket content.
 */
 
static int http_conn_receive_websocket(struct http_conn *conn,const void *src,int srcc) {
  uint8_t *SRC=(uint8_t*)src; // not const; we're going to overwrite it if masked. We know that (src) belongs to conn->rbuf
  
  // Fixed-length preamble.
  if (srcc<2) return 0;
  uint8_t flags=SRC[0]&0xf0;
  uint8_t opcode=SRC[0]&0x0f;
  uint8_t hasmask=SRC[1]&0x80;
  const uint8_t *mask=0;
  int len=SRC[1]&0x7f;
  int srcp=2;
  
  if (!(flags&0x80)) {
    fprintf(stderr,"WebSocket packet with continuation: We don't support this.\n");
    return -1;
  }
  
  // Length.
  if (len==0x7e) {
    if (srcc<4) return 0;
    len=(SRC[2]<<8)|SRC[3];
    srcp=4;
  } else if (len==0x7f) {
    if (srcc<10) return 0;
    if (memcmp(SRC+2,"\0\0\0\0\0",5)) return -1; // >24-bit length, forget that.
    len=(SRC[7]<<16)|(SRC[8]<<8)|SRC[9];
    srcp=10;
  }
  
  // Mask.
  if (hasmask) {
    if (srcp>srcc-4) return 0;
    mask=SRC+srcp;
    srcp+=4;
  }
  
  // Payload.
  if (srcp>srcc-len) return 0;
  uint8_t *body=SRC+srcp;
  srcp+=len;
  
  // Apply mask.
  if (hasmask) {
    int i=0; for (;i<len;i++) body[i]^=mask[i&3];
  }
  
  // Send to delegate.
  if (conn->wslistener&&conn->wslistener->delegate.cb_ws_recv) {
    if (conn->wslistener->delegate.cb_ws_recv(conn->wslistener,conn,opcode,body,len)<0) {
      return -1;
    }
  } else if (conn->delegate.ws_packet) {
    if (conn->delegate.ws_packet(conn,opcode,body,len)<0) return -1;
  }
  
  return srcp;
}

/* Receive the server's response to Websocket upgrade request.
 */
 
static int http_conn_receive_websocket_ack(struct http_conn *conn,const char *src,int srcc) {
  
  // Measure the entire Status-Line and Headers, we're not going to use all the xfer plumbing.
  int len=0,srcp=0,stop=srcc-4;
  for (;srcp<=stop;srcp++) {
    if (src[srcp+0]!=0x0d) continue;
    if (src[srcp+1]!=0x0a) continue;
    if (src[srcp+2]!=0x0d) continue;
    if (src[srcp+3]!=0x0a) continue;
    len=srcp+4;
    break;
  }
  if (len<4) return 0;
  
  // Find status code, it's the second space-delimited word.
  srcp=0;
  while ((srcp<len)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  while ((srcp<len)&&((unsigned char)src[srcp]>0x20)) srcp++; // protocol
  while ((srcp<len)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int status=0;
  while ((srcp<len)&&((unsigned char)src[srcp]>0x20)) {
    int digit=src[srcp++]-'0';
    if ((digit<0)||(digit>9)) return -1;
    status*=10;
    status+=digit;
  }
  if (status!=101) return -1;
  
  conn->state=HTTP_STATE_WEBSOCKET;
  return len;
}

/* Decode incoming content. 
 * Return length consumed.
 * Do not advance rbufp, caller does it.
 * Return zero if input incomplete.
 */
 
static int http_conn_advance(struct http_conn *conn,const char *src,int srcc) {
  switch (conn->state) {
  
    case HTTP_STATE_IDLE: conn->state=HTTP_STATE_PREAMBLE; // pass
    case HTTP_STATE_PREAMBLE: {
        int spacec=0;
        while ((spacec<srcc)&&((unsigned char)src[spacec]<=0x20)) spacec++;
        if (spacec) return spacec; // Drop leading whitespace. eg if remote sends a bunch of newlines, just ignore it.
        int linec=http_measure_line(src,srcc);
        if (linec<1) return 0;
        if (http_conn_receive_preamble(conn,src,linec)<0) return -1;
        return linec;
      }
      
    case HTTP_STATE_HEADER: {
        int linec=http_measure_line(src,srcc);
        if (linec<1) return 0;
        if (http_conn_receive_header(conn,src,linec)<0) return -1;
        return linec;
      }
      
    case HTTP_STATE_BODY: return http_conn_receive_body(conn,src,srcc);
    
    case HTTP_STATE_WEBSOCKET: return http_conn_receive_websocket(conn,src,srcc);
    case HTTP_STATE_WEBSOCKET_INITIATE: return http_conn_receive_websocket_ack(conn,src,srcc);
  }
  return -1;
}

/* Decode incoming content, outer loop.
 */
 
static int http_conn_drain_input(struct http_conn *conn) {
  while (conn->rbufp<conn->rbuf.c) {
    const char *src=conn->rbuf.v+conn->rbufp;
    int srcc=conn->rbuf.c-conn->rbufp;
    int err=http_conn_advance(conn,src,srcc);
    if (err<0) {
      fprintf(stderr,"%s:%d: Error parsing HTTP.\n",__FILE__,__LINE__);
      return -1;
    }
    if (!err) break;
    conn->rbufp+=err;
    if (conn->rbufp>conn->rbuf.c) {
      conn->rbufp=0;
      conn->rbuf.c=0;
      return -1;
    }
  }
  if (conn->rbufp>=conn->rbuf.c) {
    conn->rbufp=0;
    conn->rbuf.c=0;
  }
  return 0;
}

/* Grow rbuf if needed.
 */
 
static int http_conn_rbuf_require(struct http_conn *conn) {
  if (conn->rbuf.c<conn->rbuf.a) return 0;
  if (conn->rbufp) {
    conn->rbuf.c-=conn->rbufp;
    memmove(conn->rbuf.v,conn->rbuf.v+conn->rbufp,conn->rbuf.c);
    conn->rbufp=0;
    return 0;
  }
  return encoder_require(&conn->rbuf,1024);
}

/* Read.
 */
 
int http_conn_read(struct http_conn *conn) {
  if (conn->fd<0) return -1;
  if (http_conn_rbuf_require(conn)<0) return -1;
  int err=read(conn->fd,conn->rbuf.v+conn->rbuf.c,conn->rbuf.a-conn->rbuf.c);
  if (err<=0) {
    if (conn->delegate.eof) {
      if (conn->delegate.eof(conn)<0) return -1;
    } else if (conn->wslistener) {
      if (conn->wslistener->delegate.cb_ws_recv) {
        if (conn->wslistener->delegate.cb_ws_recv(conn->wslistener,conn,8,0,0)<0) return -1;
      }
    }
    _http_conn_close(conn);
  } else {
    conn->rbuf.c+=err;
  }
  if (http_conn_drain_input(conn)<0) return -1;
  
  if (conn->fd<0) { // deferred error or completion
    if (conn->context&&(conn->state==HTTP_STATE_IDLE)) {
      return http_context_remove_conn(conn->context,conn);
    } else if (conn->delegate.eof||conn->wslistener) {
      return 0;
    } else {
      return -1;
    }
  }
  
  if (conn->context) {
    if (conn->wbufp<conn->wbuf.c) {
      poller_set_writeable(conn->context->poller,conn->fd,1);
    }
  }
  return 0;
}

/* Write.
 */
 
int http_conn_write(struct http_conn *conn) {
  if (conn->wbufp>=conn->wbuf.c) return -1;
  if (conn->fd<0) return -1;
  int err=write(conn->fd,conn->wbuf.v+conn->wbufp,conn->wbuf.c-conn->wbufp);
  if (err<=0) return -1;
  conn->wbufp+=err;
  if (conn->wbufp>=conn->wbuf.c) {
    conn->wbufp=0;
    conn->wbuf.c=0;
    if (conn->context) {
      poller_set_writeable(conn->context->poller,conn->fd,0);
    }
    if (conn->delegate.write_complete) {
      return conn->delegate.write_complete(conn);
    }
  }
  return 0;
}

/* Close.
 */
 
void http_conn_close(struct http_conn *conn) {
  if (!conn) return;
  if (conn->fd<0) return;
  if (conn->context) {
    poller_remove_file(conn->context->poller,conn->fd);
  }
  if (conn->ownfd) close(conn->fd);
  conn->fd=-1;
}

/* Encode outgoing Websocket packet.
 */
 
int http_conn_send_websocket(struct http_conn *conn,int type,const void *src,int srcc) {
  if (!conn) return -1;
  if ( // Allow sending before the handshake completes, assume it's going to ack.
    (conn->state!=HTTP_STATE_WEBSOCKET)&&
    (conn->state!=HTTP_STATE_WEBSOCKET_INITIATE)
  ) return -1;
  if (conn->fd<0) return -1;
  if ((type<0)||(type>15)) return -1;
  if ((srcc<0)||(srcc&&!src)) return -1;
  if (srcc>0x00ffffff) return -1;
  
  if (encode_intbe(&conn->wbuf,0x80|type,1)<0) return -1;
  if (srcc<126) {
    if (encode_intbe(&conn->wbuf,srcc,1)<0) return -1;
  } else if (srcc<0x10000) {
    if (encode_intbe(&conn->wbuf,0x7e,1)<0) return -1;
    if (encode_intbe(&conn->wbuf,srcc,2)<0) return -1;
  } else {
    if (encode_intbe(&conn->wbuf,0x7f,1)<0) return -1;
    if (encode_raw(&conn->wbuf,"\0\0\0\0",4)<0) return -1;
    if (encode_intbe(&conn->wbuf,srcc,4)<0) return -1;
  }
  if (encode_raw(&conn->wbuf,src,srcc)<0) return -1;
  
  if (conn->context) {
    poller_set_writeable(conn->context->poller,conn->fd,1);
  }
  
  return 0;
}

/* Initiate client-side Websocket connection.
 */
 
int http_conn_initiate_websocket(struct http_conn *conn) {
  if (!conn||(conn->fd<0)) return -1;
  if (conn->state==HTTP_STATE_WEBSOCKET) return -1;
  if (conn->state==HTTP_STATE_WEBSOCKET_INITIATE) return -1;
  
  const char request[]=
    "GET /websocket HTTP/1.1\r\n" //TODO get path from the caller!
    "Connection: Upgrade\r\n"
    "Upgrade: websocket\r\n"
    "Sec-WebSocket-Key: 12345\r\n" // we aren't going to validate
    "\r\n";
  
  if (encode_raw(&conn->wbuf,request,sizeof(request)-1)<0) return -1;
  conn->state=HTTP_STATE_WEBSOCKET_INITIATE;
  
  if (conn->context) {
    poller_set_writeable(conn->context->poller,conn->fd,1);
  }
  
  return 0;
}

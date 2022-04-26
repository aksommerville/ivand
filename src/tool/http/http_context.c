#include "http.h"
#include "tool/common/poller.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>

/* Delete.
 */

void http_context_del(struct http_context *context) {
  if (!context) return;
  if (context->refc-->1) return;
  
  if (context->connv) {
    while (context->connc-->0) {
      struct http_conn *conn=context->connv[context->connc];
      conn->context=0;
      if (conn->fd>=0) poller_remove_file(context->poller,conn->fd);
      http_conn_del(conn);
    }
    free(context->connv);
  }
  
  if (context->serverv) {
    while (context->serverc-->0) {
      int fd=context->serverv[context->serverc];
      poller_remove_file(context->poller,fd);
      close(fd);
    }
    free(context->serverv);
  }
  
  if (context->listenerv) {
    while (context->listenerc-->0) http_listener_del(context->listenerv[context->listenerc]);
    free(context->listenerv);
  }
  
  if (context->idle_timeout_id>0) {
    poller_cancel_timeout(context->poller,context->idle_timeout_id);
  }
  poller_del(context->poller);
  
  free(context);
}

/* Retain.
 */

int http_context_ref(struct http_context *context) {
  if (!context) return -1;
  if (context->refc<1) return -1;
  if (context->refc==INT_MAX) return -1;
  context->refc++;
  return 0;
}

/* New.
 */

struct http_context *http_context_new(struct poller *poller) {
  struct http_context *context=calloc(1,sizeof(struct http_context));
  if (!context) return 0;
  
  context->refc=1;
  
  if (poller) {
    if (poller_ref(poller)<0) {
      http_context_del(context);
      return 0;
    }
    context->poller=poller;
  } else {
    if (!(context->poller=poller_new())) {
      http_context_del(context);
      return 0;
    }
  }
  
  context->idle_timeout_id=poller_set_timeout(context->poller,10000,(void*)http_context_drop_idle_clients,context);
  
  return context;
}

/* Accept incoming connection.
 */
 
static int http_context_cb_accept(int fd,void *userdata,int clientfd,const void *saddr,int saddrc) {
  struct http_context *context=userdata;
  struct http_conn_delegate delegate={};
  struct http_conn *conn=http_conn_new_handoff(&delegate,clientfd);
  if (!conn) return -1;
  if (http_context_add_conn(context,conn)<0) {
    conn->fd=-1;
    http_conn_del(conn);
    return -1;
  }
  return 0;
}

/* Add existing server (handoff).
 */

int http_context_serve_fd(struct http_context *context,int fd) {
  if (context->serverc>=context->servera) {
    int na=context->servera+4;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(context->serverv,sizeof(int)*na);
    if (!nv) return -1;
    context->serverv=nv;
    context->servera=na;
  }
  
  struct poller_file file={
    .fd=fd,
    .ownfd=0,
    .userdata=context,
    .cb_accept=http_context_cb_accept,
  };
  if (poller_add_file(context->poller,&file)<0) return -1;
  
  context->serverv[context->serverc++]=fd;
  return 0;
}

/* Add TCP server.
 */

int http_context_serve_tcp(struct http_context *context,const char *host,int port) {
  struct addrinfo hints={
    .ai_flags=AI_ADDRCONFIG|AI_PASSIVE,
    .ai_socktype=SOCK_STREAM,
    //.ai_family=AF_INET,
  };
  struct addrinfo *ai=0;
  char servicename[32];
  snprintf(servicename,sizeof(servicename),"%d",port);
  if (getaddrinfo(host,servicename,&hints,&ai)<0) return -1;
  if (!ai) return -1;
  
  int fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
  if (fd<0) {
    freeaddrinfo(ai);
    return -1;
  }
  int one=1;
  setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
  if (bind(fd,ai->ai_addr,ai->ai_addrlen)<0) {
    freeaddrinfo(ai);
    close(fd);
    return -1;
  }
  freeaddrinfo(ai);
  
  if (listen(fd,10)<0) {
    close(fd);
    return -1;
  }
  
  if (http_context_serve_fd(context,fd)<0) {
    close(fd);
    return -1;
  }
  
  return fd;
}

/* Add UNIX server.
 */

int http_context_serve_unix(struct http_context *context,const char *path) {
  if (!path) return -1;
  int pathc=0;
  while (path[pathc]) pathc++;
  int socklen=sizeof(struct sockaddr_un)+pathc;
  struct sockaddr_un *sun=calloc(1,socklen+1);
  if (!sun) return -1;
  sun->sun_family=AF_UNIX;
  memcpy(sun->sun_path,path,pathc);
  sun->sun_path[pathc]=0; // unclear whether this is needed
  
  int fd=socket(PF_UNIX,SOCK_STREAM,0);
  if (fd<0) {
    free(sun);
    return -1;
  }
  int one=1;
  setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
  if (bind(fd,(struct sockaddr*)sun,socklen)<0) {
    close(fd);
    free(sun);
    return -1;
  }
  free(sun);
  
  if (listen(fd,10)<0) {
    close(fd);
    unlink(path);
    return -1;
  }
  
  if (http_context_serve_fd(context,fd)<0) {
    close(fd);
    unlink(path);
    return -1;
  }
  
  return fd;
}

/* Close and remove server.
 */

int http_context_unserve(struct http_context *context,int fd) {
  if (!context) return -1;
  int i=context->serverc;
  while (i-->0) {
    if (context->serverv[i]==fd) {
      context->serverc--;
      memmove(context->serverv+i,context->serverv+i+1,sizeof(int)*(context->serverc-i));
      close(fd);
      poller_remove_file(context->poller,fd);
      return 0;
    }
  }
  return -1;
}

/* Connection events.
 */
 
static int http_context_conn_error(int fd,void *userdata) {
  struct http_conn *conn=userdata;
  struct http_context *context=conn->context;
  return http_context_remove_conn(context,conn);
}

static int http_context_conn_readable(int fd,void *userdata) {
  struct http_conn *conn=userdata;
  struct http_context *context=conn->context;
  if (http_conn_read(conn)<0) return -1;
  return 0;
}

static int http_context_conn_writeable(int fd,void *userdata) {
  struct http_conn *conn=userdata;
  struct http_context *context=conn->context;
  if (http_conn_write(conn)<0) return -1;
  return 0;
}

/* Connections list.
 */

int http_context_add_conn(struct http_context *context,struct http_conn *conn) {
  if (conn->fd<0) return -1;
  if (conn->context==context) return 0;
  if (conn->context) return -1;
  
  if (context->connc>=context->conna) {
    int na=context->conna+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(context->connv,sizeof(void*)*na);
    if (!nv) return -1;
    context->connv=nv;
    context->conna=na;
  }
  
  struct poller_file file={
    .fd=conn->fd,
    .ownfd=0,
    .userdata=conn,
    .cb_error=http_context_conn_error,
    .cb_readable=http_context_conn_readable,
    .cb_writeable=http_context_conn_writeable,
  };
  if (poller_add_file(context->poller,&file)<0) {
    return -1;
  }
  
  if (http_conn_ref(conn)<0) {
    poller_remove_file(context->poller,conn->fd);
    return -1;
  }
  context->connv[context->connc++]=conn;
  conn->context=context;
  
  if (http_conn_get_io_status(conn)=='w') {
    poller_set_writeable(context->poller,conn->fd,1);
  }
  
  return 0;
}

int http_context_remove_conn(struct http_context *context,struct http_conn *conn) {
  int i=context->connc;
  while (i-->0) {
    if (context->connv[i]==conn) {
      context->connc--;
      memmove(context->connv+i,context->connv+i+1,sizeof(void*)*(context->connc-i));
      poller_remove_file(context->poller,conn->fd);
      conn->context=0;
      http_conn_del(conn);
      return 0;
    }
  }
  return -1;
}

/* Listeners list.
 */

int http_context_add_listener(struct http_context *context,struct http_listener *listener) {
  if (context->listenerc>=context->listenera) {
    int na=context->listenera+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(context->listenerv,sizeof(void*)*na);
    if (!nv) return -1;
    context->listenerv=nv;
    context->listenera=na;
  }
  if (http_listener_ref(listener)<0) return -1;
  context->listenerv[context->listenerc++]=listener;
  return 0;
}

int http_context_remove_listener(struct http_context *context,struct http_listener *listener) {
  int i=context->listenerc;
  while (i-->0) {
    if (context->listenerv[i]==listener) {
      context->listenerc--;
      memmove(context->listenerv+i,context->listenerv+i+1,sizeof(void*)*(context->listenerc-i));
      http_listener_del(listener);
      return 0;
    }
  }
  return -1;
}

struct http_listener *http_context_listen(
  struct http_context *context,
  int method,const char *path,
  int (*cb)(struct http_listener *listener,struct http_xfer *req,struct http_xfer *resp),
  void *userdata
) {
  struct http_listener_delegate delegate={
    .userdata=userdata,
    .cb_request=cb,
  };
  struct http_listener *listener=http_listener_new(&delegate);
  if (!listener) return 0;
  if (
    (method&&(http_listener_add_method(listener,method)<0))||
    (path&&path[0]&&(http_listener_set_prefix(listener,path,-1)<0))||
    (http_context_add_listener(context,listener)<0)
  ) {
    http_listener_del(listener);
    return 0;
  }
  http_listener_del(listener);
  return listener;
}

struct http_listener *http_context_listen_websocket(
  struct http_context *context,
  const char *path,
  int (*cb_connect)(struct http_listener *listener,struct http_conn *conn,struct http_xfer *req,struct http_xfer *resp),
  int (*cb_recv)(struct http_listener *listener,struct http_conn *conn,int type,const void *src,int srcc),
  void *userdata
) {
  if (!cb_connect&&!cb_recv) return 0;
  struct http_listener_delegate delegate={
    .userdata=userdata,
    .cb_ws_connect=cb_connect,
    .cb_ws_recv=cb_recv,
  };
  struct http_listener *listener=http_listener_new(&delegate);
  if (!listener) return 0;
  if (
    (http_listener_add_method(listener,HTTP_METHOD_GET)<0)||
    (path&&path[0]&&(http_listener_set_prefix(listener,path,-1)<0))||
    (http_context_add_listener(context,listener)<0)
  ) {
    http_listener_del(listener);
    return 0;
  }
  http_listener_del(listener);
  return listener;
}

struct http_listener *http_context_find_listener_for_request(
  const struct http_context *context,
  const struct http_xfer *request
) {
  if (!context||!request) return 0;
  int i=0;
  for (;i<context->listenerc;i++) {
    struct http_listener *listener=context->listenerv[i];
    if (http_listener_match(listener,request)) return listener;
  }
  return 0;
}

/* Find a reusable client.
 */
 
static struct http_conn *http_context_find_idle_client_for_request(
  const struct http_context *context,
  const char *host,int hostc,int port
) {
  int i=context->connc;
  while (i-->0) {
    struct http_conn *conn=context->connv[i];
    if (conn->role!=HTTP_ROLE_CLIENT) continue;
    if (conn->state!=HTTP_STATE_IDLE) continue;
    if (conn->remoteport!=port) continue;
    if (!conn->remotehost) continue;
    if (memcmp(conn->remotehost,host,hostc)) continue;
    if (conn->remotehost[hostc]) continue;
    return conn;
  }
  return 0;
}

/* Client request, convenience.
 */
 
int http_request(
  struct http_context *context,
  int method,const char *url,int urlc,
  const char *content_type,
  const void *body,int bodyc,
  int (*cb)(struct http_conn *conn,struct http_xfer *resp),
  void *userdata
) {

  struct http_url split={0};
  if (http_url_split(&split,url,urlc)<0) return -1;
  if (!split.port) split.port=80;
  struct http_conn *conn=http_context_find_idle_client_for_request(context,split.host,split.hostc,split.port);
  if (conn) {
    struct http_xfer *xfer=http_xfer_new_request(method,split.path,split.pathc,content_type,body,bodyc);
    if (xfer) {
      if (http_conn_encode_xfer(conn,xfer)<0) {
        http_xfer_del(xfer);
        http_context_remove_conn(context,conn);
        return -1;
      }
      http_xfer_del(xfer);
      poller_set_writeable(context->poller,conn->fd,1);
      conn->delegate.userdata=userdata;
      conn->delegate.response_ready=cb;
      return 0;
    }
  }

  struct http_conn_delegate delegate={
    .userdata=userdata,
    .response_ready=cb,
  };
  conn=http_conn_new_request(&delegate,method,url,urlc,content_type,body,bodyc);
  if (!conn) return -1;
  if (http_context_add_conn(context,conn)<0) {
    http_conn_del(conn);
    return -1;
  }
  http_conn_del(conn);
  return 0;
}

/* Look for idle clients, drop any we find.
 * Clients trigger this after some delay, when they go idle.
 */
 
int http_context_drop_idle_clients(struct http_context *context) {
  int i=context->connc;
  while (i-->0) {
    struct http_conn *conn=context->connv[i];
    if (conn->fd>=0) {
      if (conn->role==HTTP_ROLE_SERVER) continue;
      if (conn->state!=HTTP_STATE_IDLE) continue;
    }
    http_context_remove_conn(context,conn);
  }
  context->idle_timeout_id=poller_set_timeout(context->poller,10000,(void*)http_context_drop_idle_clients,context);
  return 0;
}

/* Iterate websocket connections.
 */
 
int http_context_for_each_websocket(
  struct http_context *context,
  int (*cb)(struct http_conn *conn,void *userdata),
  void *userdata
) {
  if (!context||!cb) return -1;
  int i=context->connc,err;
  while (i-->0) {
    struct http_conn *conn=context->connv[i];
    if (conn->state==HTTP_STATE_WEBSOCKET) {
      if (err=cb(conn,userdata)) return err;
    }
  }
  return 0;
}

/* http1.h
 * Simplified client-side alternative to http.h
 * We maintain a single connection to the remote, reconnecting as needed.
 * Requests pile up in a queue and we send them one by one, one in flight at a time.
 */
 
#ifndef HTTP1_H
#define HTTP1_H

struct poller;
struct http_conn;
struct http_xfer;

struct http1_request {
  int reqid;
  struct http_xfer *req;
  void *userdata;
  int (*cb_ok)(void *userdata,struct http_xfer *resp);
  int (*cb_err)(void *userdata,struct http_xfer *resp);
};

struct http1 {
  struct poller *poller;
  char *host;
  int hostc;
  int port;
  struct http_conn *conn;
  struct http1_request *requestv;
  int requestp,requestc,requesta;
  int reqid_next;
  int reqid_pending;
  int connfd; // WEAK, keep here too because conn will drop it before we can tell poller
  struct http_conn *websocket;
  int (*cb_websocket)(struct http_conn *conn,int type,const void *src,int srcc,void *userdata);
  void *userdata;
};

void http1_del(struct http1 *http1);
struct http1 *http1_new(const char *host,int port,struct poller *poller);

/* File a request with an http_xfer of your own construction, or with these GET and POST helpers.
 * All return (reqid) or <0 for error, never zero.
 * We guarantee that one of (cb_ok,cb_err) will eventually be called unless you cancel the request.
 * (in particular, we call cb_err on every pending request at context deletion).
 * cb_ok will always get a response with status in 200..299.
 * cb_err response can be null, if it failed without receiving a response, eg transport issues.
 */
int http1_request(
  struct http1 *http1,
  struct http_xfer *xfer,
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
);
int http1_get(
  struct http1 *http1,
  const char *path,int pathc, // with encoded query
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
);
int http1_post(
  struct http1 *http1,
  const char *path,int pathc, // with encoded query
  const void *body,int bodyc,
  const char *content_type,
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
);
int http1_put(
  struct http1 *http1,
  const char *path,int pathc,
  const void *body,int bodyc,
  const char *content_type,
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
);
int http1_delete(
  struct http1 *http1,
  const char *path,int pathc,
  void *userdata,
  int (*cb_ok)(void *userdata,struct http_xfer *resp),
  int (*cb_err)(void *userdata,struct http_xfer *resp)
);

/* Cancel a pending request.
 * If found, we return its (userdata) -- you'll want to do whatever cleanup you would have done in the callback.
 */
void *http1_cancel(struct http1 *http1,int reqid);

/* Succeeds if a connection is established.
 * Establishes the connection synchronously if we don't have it already.
 * Normally you don't call this, let 'request' do it for you.
 */
int http1_conn_require(struct http1 *http1);

int http1_websocket_connect(
  struct http1 *http1,
  int (*cb)(struct http_conn *conn,int type,const void *src,int srcc,void *userdata),
  void *userdata
);

int http1_websocket_send(struct http1 *http1,int type,const void *src,int srcc);

#endif

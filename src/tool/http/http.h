/* http.h
 * HTTP server and client.
 */
 
#ifndef HTTP_H
#define HTTP_H

#include "tool/common/decoder.h"

#define HTTP_ROLE_CLIENT 1
#define HTTP_ROLE_SERVER 2

#define HTTP_METHOD_GET     1
#define HTTP_METHOD_POST    2
#define HTTP_METHOD_PUT     3
#define HTTP_METHOD_DELETE  4
#define HTTP_METHOD_PATCH   5
#define HTTP_METHOD_HEAD    6
#define HTTP_METHOD_OPTIONS 7
#define HTTP_METHOD_CONNECT 8

struct http_conn;
struct http_listener;
struct http_xfer;
struct http_context;
struct poller;

/* Connection: A socket on which multiple requests can happen.
 * Suitable for both clients and servers.
 ****************************************************************/
 
#define HTTP_STATE_IDLE      0 /* Awaiting preamble. */
#define HTTP_STATE_PREAMBLE  1 /* Receiving preamble. */
#define HTTP_STATE_HEADER    2 /* Awaiting or receiving headers. */
#define HTTP_STATE_BODY      3 /* Awaiting or receiving body. */
#define HTTP_STATE_WEBSOCKET 4 /* Upgraded to websocket -- state now fully derivable from buffers */
#define HTTP_STATE_WEBSOCKET_INITIATE 5 /* Websocket client: Sent handshake, awaiting ack */
 
struct http_conn_delegate {
  void *userdata;
  int (*request_ready)(struct http_conn *conn,struct http_xfer *req,struct http_xfer *resp);
  int (*response_ready)(struct http_conn *conn,struct http_xfer *resp);
  int (*write_complete)(struct http_conn *conn);
  int (*ws_packet)(struct http_conn *conn,int type,const void *src,int srcc);
  int (*eof)(struct http_conn *conn);
};
 
struct http_conn {
  struct http_context *context; // WEAK
  int refc;
  int fd;
  int ownfd;
  int role;
  int state;
  struct http_conn_delegate delegate;
  struct encoder rbuf,wbuf;
  int rbufp,wbufp;
  struct http_xfer *xfer;
  int bodyexpect; // >0 if we are in BODY state and reading raw data
  int bodychunked;
  char *remotehost;
  int remoteport;
  struct http_listener *wslistener;
};

void http_conn_del(struct http_conn *conn);
int http_conn_ref(struct http_conn *conn);

struct http_conn *http_conn_new(const struct http_conn_delegate *delegate);
struct http_conn *http_conn_new_borrow(const struct http_conn_delegate *delegate,int fd);
struct http_conn *http_conn_new_handoff(const struct http_conn_delegate *delegate,int fd);

struct http_conn *http_conn_new_client(
  const struct http_conn_delegate *delegate,
  struct http_xfer *request
);
struct http_conn *http_conn_new_request(
  const struct http_conn_delegate *delegate,
  int method,const char *url,int urlc,
  const char *content_type,
  const void *body,int bodyc
);
struct http_conn *http_conn_new_tcp_client(
  const struct http_conn_delegate *delegate,
  const char *host,int hostc,int port
);

void *http_conn_get_userdata(const struct http_conn *conn);

// What needs done? '!'=defunct, 'r'=read, 'w'=write.
char http_conn_get_io_status(const struct http_conn *conn);

/* Perform I/O against our buffers and trigger callbacks as warranted.
 * Conn does not poll, that's our owner's job.
 */
int http_conn_read(struct http_conn *conn);
int http_conn_write(struct http_conn *conn);

// Context does this when reusing a connection. You should probly keep away.
int http_conn_encode_xfer(struct http_conn *conn,struct http_xfer *request);

// Encodes for transmit, no I/O. Fails if not a Websocket.
int http_conn_send_websocket(struct http_conn *conn,int type,const void *src,int srcc);

// For clients. Sends the HTTP Websocket bootstrap.
int http_conn_initiate_websocket(struct http_conn *conn);

void http_conn_close(struct http_conn *conn);

/* Transfer: One side of a single HTTP transaction.
 *****************************************************************/
 
struct http_xfer {
  int refc;
  int role; // CLIENT=request, SERVER=response
  struct encoder preamble; // Request-Line or Status-Line
  struct http_header {
    char *k,*v;
    int kc,vc;
  } *headerv;
  int headerc,headera;
  struct encoder body;
};

void http_xfer_del(struct http_xfer *xfer);
int http_xfer_ref(struct http_xfer *xfer);

struct http_xfer *http_xfer_new(int role);

struct http_xfer *http_xfer_new_request(
  int method,const char *path,int pathc,
  const char *content_type,
  const void *body,int bodyc
);

void http_xfer_clear(struct http_xfer *xfer);

// Preamble features.
int http_xfer_set_preamble(struct http_xfer *xfer,const char *src,int srcc);
int http_xfer_set_request_line(struct http_xfer *xfer,int method,const char *path,int pathc,const char *proto,int protoc);
int http_xfer_set_status_line(struct http_xfer *xfer,const char *proto,int protoc,int status,const char *msg,int msgc);
int http_xfer_get_method(void *dstpp,const struct http_xfer *xfer);
int http_xfer_parse_method(const struct http_xfer *xfer);
int http_xfer_get_path(void *dstpp,const struct http_xfer *xfer); // with query
int http_xfer_get_path_only(void *dstpp,const struct http_xfer *xfer); // without query
int http_xfer_get_protocol(void *dstpp,const struct http_xfer *xfer);
int http_xfer_get_status(const struct http_xfer *xfer);
int http_xfer_get_status_message(void *dstpp,const struct http_xfer *xfer);

// All returned query keys and values are url-encoded.
// Only http_xfer_parse_query() and http_xfer_query_present() can distinguish between empty and absent params.
int http_xfer_parse_query(
  const struct http_xfer *xfer,
  int (*cb)(void *userdata,const char *k,int kc,const char *v,int vc),
  void *userdata
);
int http_xfer_get_query(void *dstpp,const struct http_xfer *xfer); // from path
int http_xfer_get_aux_query(void *dstpp,const struct http_xfer *xfer); // body of form data
int http_xfer_get_query_string(void *dstpp,const struct http_xfer *xfer,const char *k,int kc); // first only; url-encoded
int http_xfer_get_query_int(const struct http_xfer *xfer,const char *k,int kc,int fallback); // first only
int http_xfer_decode_query_string(struct encoder *dst,const struct http_xfer *xfer,const char *k,int kc);
int http_xfer_query_present(const struct http_xfer *xfer,const char *k,int kc);

int http_xfer_get_header(void *dstpp,const struct http_xfer *xfer,const char *k,int kc);
int http_xfer_get_header_int(const struct http_xfer *xfer,const char *k,int kc,int fallback);
int http_xfer_set_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc); // overwrites
int http_xfer_add_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc); // appends, even if duplicate
int http_xfer_set_header_int(struct http_xfer *xfer,const char *k,int kc,int v);

// Convenience, esp for errors. We clear any existing content first.
int http_respond(struct http_xfer *xfer,int status,const char *msgfmt,...);

/* Listener: Callback for filtered requests.
 * A listener with no cb_match and no configured criteria, will match everything.
 ***************************************************************/
 
struct http_listener_delegate {
  void *userdata;
  int (*cb_match)(struct http_listener *listener,const struct http_xfer *req);
  int (*cb_request)(struct http_listener *listener,struct http_xfer *req,struct http_xfer *resp);
  int (*cb_ws_connect)(struct http_listener *listener,struct http_conn *conn,struct http_xfer *req,struct http_xfer *resp);
  int (*cb_ws_recv)(struct http_listener *listener,struct http_conn *conn,int type,const void *src,int srcc);
};
 
struct http_listener {
  int refc;
  struct http_listener_delegate delegate;
  int *methodv; // Empty list matches all methods (otherwise only known ones can match)
  int methodc,methoda;
  char *prefix; // Path prefix. Incoming path must break clean at the end of it (eg '/')
  int prefixc;
};

void http_listener_del(struct http_listener *listener);
int http_listener_ref(struct http_listener *listener);

struct http_listener *http_listener_new(const struct http_listener_delegate *delegate);

void *http_listener_get_userdata(const struct http_listener *listener);

int http_listener_add_method(struct http_listener *listener,int method);
int http_listener_set_prefix(struct http_listener *listener,const char *prefix,int prefixc);

// >0 if it matches
int http_listener_match(struct http_listener *listener,const struct http_xfer *req);

/* Context: You should have just one. Contains servers and transactions in progress.
 ****************************************************/
 
struct http_context {
  int refc;
  struct http_conn **connv;
  int connc,conna;
  int *serverv;
  int serverc,servera;
  struct http_listener **listenerv;
  int listenerc,listenera;
  struct poller *poller;
  int idle_timeout_id;
};

void http_context_del(struct http_context *context);
int http_context_ref(struct http_context *context);

/* Provide a poller if you have one.
 * Otherwise we create our own -- you must update it periodically.
 */
struct http_context *http_context_new(struct poller *poller);

/* All servers are alike.
 * We don't track which server a request comes in on.
 */
int http_context_serve_tcp(struct http_context *context,const char *host,int port);
int http_context_serve_unix(struct http_context *context,const char *path);
int http_context_serve_fd(struct http_context *context,int fd); // HANDOFF
int http_context_unserve(struct http_context *context,int fd); // closes fd if present

int http_context_add_conn(struct http_context *context,struct http_conn *conn);
int http_context_remove_conn(struct http_context *context,struct http_conn *conn);

int http_request(
  struct http_context *context,
  int method,const char *url,int urlc,
  const char *content_type,
  const void *body,int bodyc,
  int (*cb)(struct http_conn *conn,struct http_xfer *resp),
  void *userdata
);

/* Listeners are tested in the order you add them, first match wins.
 */
int http_context_add_listener(struct http_context *context,struct http_listener *listener);
int http_context_remove_listener(struct http_context *context,struct http_listener *listener);
struct http_listener *http_context_listen( // => WEAK
  struct http_context *context,
  int method,const char *path,
  int (*cb)(struct http_listener *listener,struct http_xfer *req,struct http_xfer *resp),
  void *userdata
);
struct http_listener *http_context_listen_websocket( // => WEAK
  struct http_context *context,
  const char *path,
  int (*cb_connect)(struct http_listener *listener,struct http_conn *conn,struct http_xfer *req,struct http_xfer *resp),
  int (*cb_recv)(struct http_listener *listener,struct http_conn *conn,int type,const void *src,int srcc),
  void *userdata
);

struct http_listener *http_context_find_listener_for_request(
  const struct http_context *context,
  const struct http_xfer *request
);

int http_context_drop_idle_clients(struct http_context *context);

int http_context_for_each_websocket(
  struct http_context *context,
  int (*cb)(struct http_conn *conn,void *userdata),
  void *userdata
);

/* Stateless helpers.
 **********************************************************/
 
int http_method_eval(const char *src,int srcc);
const char *http_method_repr(int method);
int http_method_expects_body(int method);

const char *http_guess_content_type(const char *path,int pathc,const void *src,int srcc);

struct http_url {
  const char *scheme; int schemec; // eg "http"
  const char *host; int hostc; // Between slashes, exclusive. Port not included.
  int port; // From host part, or scheme, or zero.
  const char *path; int pathc; // Start with slash, includes query.
  const char *fragment; int fragmentc; // From '#' to end. (including '#')
};
int http_url_split(struct http_url *url,const char *src,int srcc);

// Line length including terminator, or zero.
int http_measure_line(const char *src,int srcc);

#endif

/* http_main.c
 * I can't get node http-server to serve the right MIME type for wasm files and I'm sick of trying.
 * So this is a basic HTTP server that I wrote for Romassist.
 */

#include "http.h"
#include "tool/common/poller.h"
#include "tool/common/fs.h"
#include "tool/common/decoder.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static struct http_context *context=0;
static volatile int sigc=0;
static const char *htdocs=0;
static int htdocsc=0;

/* Signals.
 */
 
static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++sigc>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Serve a validated path.
 */
 
static int serve_local(struct http_xfer *req,struct http_xfer *resp,const char *path) {

  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return http_respond(resp,404,"Not found");
  int err=encode_raw(&resp->body,src,srcc);
  free(src);
  if (err<0) return err;
  
  return 0;
}

/* Serve something.
 */
 
static int cb_serve(struct http_listener *listener,struct http_xfer *req,struct http_xfer *resp) {

  int method=http_xfer_parse_method(req);
  if (method!=HTTP_METHOD_GET) return http_respond(resp,405,"GET only");

  // If there's a query in the path, ignore it.
  const char *path=0;
  int pathc=http_xfer_get_path_only(&path,req);
  if (pathc<0) return -1;
  
  // Request for the root becomes "/index.html", don't bother generalizing this.
  if ((pathc==1)&&(path[0]=='/')) {
    path="/index.html";
    pathc=11;
  }
  
  // "/ivand.wasm" is stored only in our output directory (the rest serves from src).
  if ((pathc==11)&&!memcmp(path,"/ivand.wasm",11)) {
    return serve_local(req,resp,"out/www/ivand.wasm");
  }
  
  char prepath[1024];
  int prepathc=snprintf(prepath,sizeof(prepath),"%s/%.*s",htdocs,pathc,path);
  if ((prepathc<1)||(prepathc>=sizeof(prepath))) return http_respond(resp,404,"Not found");
  
  char *localpath=realpath(prepath,0);
  if (!localpath) return http_respond(resp,404,"Not found");
  if (memcmp(localpath,htdocs,htdocsc)) {
    fprintf(stderr,"Local path escapes htdocs: %s\n",localpath);
    free(localpath);
    return http_respond(resp,404,"Not found");
  }
  
  int err=serve_local(req,resp,localpath);
  free(localpath);
  return err;
}

/* Main.
 */
 
int main(int argc,char **argv) {

  signal(SIGINT,rcvsig);
  
  int i=1; for (;i<argc;i++) {
    const char *arg=argv[i];
    if (!memcmp(arg,"--htdocs=",9)) { htdocs=arg+9; continue; }
    fprintf(stderr,"%s: Unexpected argument '%s'\n",argv[0],arg);
    return 1;
  }
  if (!htdocs) {
    fprintf(stderr,"Usage: %s HTDOCS\n",argv[0]);
    return 1;
  }
  htdocsc=strlen(htdocs);

  if (!(context=http_context_new(0))) return 1;
  
  const char *host="localhost";
  int port=8080;
  if (http_context_serve_tcp(context,host,port)<0) {
    fprintf(stderr,"Failed to open HTTP server on %s:%d\n",host,port);
    return 1;
  }
  if (
    !http_context_listen(context,HTTP_METHOD_GET,"",cb_serve,0)||
  0) {
    http_context_del(context);
    return 1;
  }
  
  fprintf(stderr,"Serving HTTP on %s:%d, SIGINT to quit...\n",host,port);
  while (!sigc) {
    if (poller_update(context->poller,1000)<0) {
      fprintf(stderr,"*** error ***\n");
      http_context_del(context);
      return 1;
    }
  }

  http_context_del(context);
  return 0;
}

#include "tool_utils.h"
#include "tool/common/fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unexpected argument.
 */
 
static int tool_arg_unexpected(
  struct tool *tool,
  const char *arg,
  int (*cb_arg)(struct tool *tool,const char *arg)
) {
  if (cb_arg) {
    int err=cb_arg(tool,arg);
    if (err>=0) return 0;
  }
  fprintf(stderr,"%s: Unexpected argument '%s'\n",tool->exename,arg);
  return -1;
}

/* Positional arguments.
 */
 
static int tool_arg_positional(
  struct tool *tool,
  const char *arg,
  int (*cb_arg)(struct tool *tool,const char *arg)
) {
  if (!tool->srcpath) {
    tool->srcpath=arg;
    return 0;
  }
  return tool_arg_unexpected(tool,arg,cb_arg);
}

/* --help
 */
 
static void tool_print_help(struct tool *tool) {
  if (tool->help_text) {
    fprintf(stderr,"%s\n",tool->help_text);
  } else {
    fprintf(stderr,"Usage: %s [OPTIONS] -oOUTPUT INPUT\n",tool->exename);
    fprintf(stderr,
      "OPTIONS:\n"
      "  --help         Print this message.\n"
      "  --tiny         Target Tiny (use PROGMEM if generating C).\n"
    );
  }
}

/* Options.
 * (v) must be NUL-terminated, whether length provided or not.
 * We may retain pointers into (v).
 */
 
static int tool_arg_opt(
  struct tool *tool,
  const char *k,int kc,
  const char *v,int vc,
  int (*cb_arg)(struct tool *tool,const char *arg)
) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    tool_print_help(tool);
    tool->terminate=1;
    return 0;
  }
  
  if ((kc==1)&&!memcmp(k,"o",1)) {
    if (tool->dstpath) {
      fprintf(stderr,"%s: Multiple output paths\n",tool->exename);
      return -1;
    }
    tool->dstpath=v;
    return 0;
  }
  
  if ((kc==4)&&!memcmp(k,"tiny",4)) {
    tool->tiny=1;
    return 0;
  }
  
  //TODO options
  
  char zk[64];
  if (kc<sizeof(zk)) {
    memcpy(zk,k,kc);
    zk[kc]=0;
  } else {
    zk[0]='?';
    zk[1]=0;
  }
  return tool_arg_unexpected(tool,zk,cb_arg);
}

/* Startup.
 */
 
int tool_startup(
  struct tool *tool,
  int argc,char **argv,
  int (*cb_arg)(struct tool *tool,const char *arg)
) {

  int argp=0;
  if (argp<argc) {
    tool->exename=argv[argp++];
  }
  if (!tool->exename||!tool->exename[0]) {
    tool->exename="(pokorc tool)";
  }
  
  while (argp<argc) {
    const char *arg=argv[argp++];
    
    // Ignore empty and blank.
    if (!arg||!arg[0]) continue;
    
    // Positional arguments.
    if (arg[0]!='-') {
      if (tool_arg_positional(tool,arg,cb_arg)<0) return -1;
      continue;
    }
    
    // "-" alone: Not defined.
    if (!arg[1]) {
      if (tool_arg_unexpected(tool,arg,cb_arg)<0) return -1;
      continue;
    }
    
    // Single dash: Single-char options
    if (arg[1]!='-') {
      char k[2]={arg[1],0};
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argp<argc)&&argv[argp]&&(argv[argp][0]!='-')) v=argv[argp++];
      if (tool_arg_opt(tool,k,1,v,-1,cb_arg)<0) return -1;
      continue;
    }
    
    // "--" alone: Not defined.
    if (!arg[2]) {
      if (tool_arg_unexpected(tool,arg,cb_arg)<0) return -1;
      continue;
    }
    
    // Long option.
    arg+=2;
    while (*arg=='-') arg++;
    const char *k=arg;
    int kc=0;
    while (arg[kc]&&(arg[kc]!='=')) kc++;
    const char *v=0;
    if (arg[kc]=='=') v=arg+kc+1;
    else if ((argp<argc)&&argv[argp]&&(argv[argp][0]!='-')) v=argv[argp++];
    if (tool_arg_opt(tool,k,kc,v,-1,cb_arg)<0) return -1;
  }
  
  return 0;
}

/* Read input.
 */
 
int tool_read_input(struct tool *tool) {
  if (!tool->srcpath||!tool->srcpath[0]) {
    fprintf(stderr,"%s: Input path required\n",tool->exename);
    return -1;
  }
  if (tool->src) free(tool->src);
  tool->src=0;
  if ((tool->srcc=file_read(&tool->src,tool->srcpath))<0) {
    tool->srcc=0;
    fprintf(stderr,"%s: Failed to read file\n",tool->srcpath);
    return -1;
  }
  return 0;
}

/* Write output.
 */
 
int tool_write_output(struct tool *tool) {
  if (!tool->dstpath||!tool->dstpath[0]) {
    fprintf(stderr,"%s: Output path required\n",tool->exename);
    return -1;
  }
  if (file_write(tool->dstpath,tool->dst.v,tool->dst.c)<0) {
    fprintf(stderr,"%s: Failed to write %d bytes output\n",tool->dstpath,tool->dst.c);
    return -1;
  }
  return 0;
}

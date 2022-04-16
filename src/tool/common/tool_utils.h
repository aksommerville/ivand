#ifndef TOOL_UTILS_H
#define TOOL_UTILS_H

#include "decoder.h"

struct tool {
  const char *exename;
  const char *srcpath;
  const char *dstpath;
  void *src;
  int srcc;
  struct encoder dst;
  void *userdata;
  int terminate; // nonzero after tool_startup() for immediate quit (eg --help)
  const char *help_text; // set before tool_startup() to replace the content for --help
  int tiny; // If nonzero, use PROGMEM
};

/* Read (argv).
 * Unknown arguments, we call (cb_arg) if provided, otherwise log and fail.
 * You should zero (tool) first. Anything set initially may stick.
 */
int tool_startup(
  struct tool *tool,
  int argc,char **argv,
  int (*cb_arg)(struct tool *tool,const char *arg)
);

/* Require (srcpath) and read it into (src,srcc).
 * Fails if unset or any other error, and logs.
 */
int tool_read_input(struct tool *tool);

/* Append C code to (dst).
 * Null (type,name) for "no preference", we'll select something reasonable.
 * (srcc) is always in bytes. We confirm that it agrees with (type).
 * Multi-byte words must be in the current host byte order.
 */
int tool_generate_c_preamble(struct tool *tool);
int tool_generate_c_array(
  struct tool *tool,
  const char *type,int typec,
  const char *name,int namec,
  const void *src,int srcc
);

int tool_guess_c_name(const char **dst,struct tool *tool,const char *type,int typec,const void *src,int srcc);

/* Write (dst) to (dstpath).
 * Log and fail if (dstpath) unset.
 */
int tool_write_output(struct tool *tool);

#endif

#include "tool/common/tool_utils.h"

int main(int argc,char **argv) {
  struct tool tool={0};
  if (tool_startup(&tool,argc,argv,0)<0) return 1;
  if (tool.terminate) return 0;
  if (tool_read_input(&tool)<0) return 1;
  if (tool_generate_c_preamble(&tool)<0) return 1;
  if (tool_generate_c_array(&tool,0,0,0,0,tool.src,tool.srcc)<0) return 1;
  if (tool_write_output(&tool)<0) return 1;
  return 0;
}

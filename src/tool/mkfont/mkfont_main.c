#include "tool/common/tool_utils.h"
#include "tool/common/png.h"
#include <stdio.h>

/* Generate our special font format from an 8-bit gray PNG image.
 */
 
static int row_empty(const uint8_t *v,int c) {
  for (;c-->0;v++) if (*v) return 0;
  return 1;
}

static int col_empty(const uint8_t *v,int c,int stride) {
  for (;c-->0;v+=stride) if (*v) return 0;
  return 1;
}
 
static int mkfont1(uint32_t *dst,const uint8_t *src,int srcstride,struct tool *tool,int codepoint) {
  
  // Measure glyph.
  int y=0,w=8,h=8;
  while (h&&row_empty(src,w)) { src+=srcstride; y++; h--; }
  while (h&&row_empty(src+srcstride*(h-1),w)) h--;
  while (w&&col_empty(src,h,srcstride)) { src++; w--; }
  while (w&&col_empty(src+w-1,h,srcstride)) w--;
  
  // If it's empty, the answer is zero.
  if (!w&&!h) {
    *dst=0;
    return 0;
  }
  
  // (y) can't go above 3: Add blank rows at the top if needed.
  while (y>3) { src-=srcstride; y--; h++; }
  
  // Assemble the glyph header.
  *dst=(y<<30)|(w<<27);
  
  // Assemble the image. Keep going even if we run out of output space, to verify input is all zeroes.
  uint32_t mask=0x04000000;
  for (;h-->0;src+=srcstride) {
    const uint8_t *srcp=src;
    int xi=w;
    for (;xi-->0;srcp++,mask>>=1) {
      if (*srcp) {
        if (!mask) {
          fprintf(stderr,
            "%s: Codepoint 0x%02x contains ON pixels more than 26 spaces apart. Make it smaller.\n",
            tool->srcpath,codepoint
          );
          return -1;
        }
        (*dst)|=mask;
      }
    }
  }
  
  return 0;
}
 
static int mkfont(uint32_t *dst/*96*/,const struct png_image *src,struct tool *tool) {
  int codepoint=0x20,row=0;
  for (;row<6;row++) {
    int col=0;
    for (;col<16;col++,codepoint++,dst++) {
      if (mkfont1(dst,(uint8_t*)src->pixels+row*8*src->stride+col*8,src->stride,tool,codepoint)<0) {
        fprintf(stderr,"%s: Error at codepoint 0x%02x\n",tool->srcpath,codepoint);
        return -1;
      }
    }
  }
  return 0;
}

/* Main.
 */

int main(int argc,char **argv) {
  struct tool tool={0};
  if (tool_startup(&tool,argc,argv,0)<0) return 1;
  if (tool.terminate) return 0;
  if (tool_read_input(&tool)<0) return 1;
  
  // Decode PNG and convert to 8-bit gray. (we only need 1 bit, but 8 is easier to work with).
  struct png_image *image=png_decode(tool.src,tool.srcc);
  if (!image) {
    fprintf(stderr,"%s: Failed to decode PNG\n",tool.srcpath);
    return 1;
  }
  struct png_image *converted=png_image_new();
  if (png_image_convert(converted,8,0,image)<0) return 1;
  png_image_del(image);
  image=converted;
  
  // Input must be exactly 16x6 tiles of 8x8 pixels.
  if ((image->w!=128)||(image->h!=48)) {
    fprintf(stderr,"%s: Font input must be 128x48 pixels (16x6 tiles of 8x8 pixels). Found %dx%d\n",tool.srcpath,image->w,image->h);
    return 1;
  }
  
  // Output is 96 32-bit words.
  // mkfont() logs all its errors.
  uint32_t bin[96]={0};
  if (mkfont(bin,image,&tool)<0) return 1;
  
  if (tool_generate_c_preamble(&tool)<0) return 1;
  if (tool_generate_c_array(&tool,"uint32_t",8,0,0,bin,sizeof(bin))<0) return 1;
  if (tool_write_output(&tool)<0) return 1;
  return 0;
}

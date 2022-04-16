#include "tool/common/tool_utils.h"
#include "tool/common/png.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Guess output format based on path.
 */
 
#define CVTIMG_FORMAT_TSV 1
#define CVTIMG_FORMAT_C   2
 
static int cvtimg_guess_output_format(const char *path) {
  if (!path) return 0;
  char sfx[8];
  int sfxc=0,reading=0;
  for (;*path;path++) {
    if (*path=='/') {
      sfxc=0;
      reading=0;
    } else if (*path=='.') {
      sfxc=0;
      reading=1;
    } else if (reading) {
      if (sfxc>=sizeof(sfx)) {
        sfxc=0;
        reading=0;
      } else {
        if ((*path>='A')&&(*path<='Z')) sfx[sfxc++]=(*path)+0x20;
        else sfx[sfxc++]=*path;
      }
    }
  }
  if ((sfxc==3)&&!memcmp(sfx,"tsv",3)) return CVTIMG_FORMAT_TSV;
  if ((sfxc==1)&&!memcmp(sfx,"c",1)) return CVTIMG_FORMAT_C;
  return 0;
}

/* Convert decoded PNG image to RGB565.
 * Output will have the same dimensions as input and minimum stride (total size w*h*2).
 */
 
static uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b) {
  return ((r<<5)&0x1f00)|(g>>5)|((g<<11)&0xe000)|(b&0x00f8);
}
 
static uint16_t rgb565nz(uint8_t r,uint8_t g,uint8_t b) {
  uint16_t rgb=((r<<5)&0x1f00)|(g>>5)|((g<<11)&0xe000)|(b&0x00f8);
  if (!rgb) return 0x2000; // don't let it come out zero
  return rgb;
}
 
static void cvtimg_rgb565_y8(uint16_t *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src++) {
    *dst=rgb565(*src,*src,*src);
  }
}

static void cvtimg_rgb565_rgb8(uint16_t *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=3) {
    *dst=rgb565(src[0],src[1],src[2]);
  }
}

static void cvtimg_rgb565_rgba8(uint16_t *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) {
    if (src[3]) *dst=rgb565nz(src[0],src[1],src[2]);
    else *dst=0;
  }
}
 
static void *cvtimg_to_rgb565(const struct png_image *src) {
  int pixelc=src->w*src->h;
  void *dst=malloc(pixelc*2);
  if (!dst) return 0;
  // I'm not going to support every valid input format, just the easy and common ones.
  // Inputs with no alpha, that's a hint that we are going to render opaque, so zero is just a color.
  // If alpha is present, we force opaque pixels to be nonzero.
  switch (src->colortype) {
    case 0: switch (src->depth) {
        case 8: cvtimg_rgb565_y8(dst,src->pixels,pixelc); return dst;
      } break;
    case 2: switch (src->depth) {
        case 8: cvtimg_rgb565_rgb8(dst,src->pixels,pixelc); return dst;
      } break;
    case 6: switch (src->depth) {
        case 8: cvtimg_rgb565_rgba8(dst,src->pixels,pixelc); return dst;
      } break;
  }
  fprintf(stderr,"PNG pixel format not supported! depth=%d colortype=%d\n",src->depth,src->colortype);
  free(dst);
  return 0;
}

/* Generate TSV from (image) into (tool->dst).
 */
 
static int cvtimg_generate_tsv(struct tool *tool,struct png_image *image) {

  void *src=cvtimg_to_rgb565(image);
  if (!src) {
    fprintf(stderr,"%s: Failed to convert image\n",tool->srcpath);
    return 0;
  }
  
  if (tool->dst.v) free(tool->dst.v);
  tool->dst.v=src;
  tool->dst.c=image->w*image->h*2;
  tool->dst.a=tool->dst.c;
  
  return 0;
}

/* Generate C from (image) into (tool->dst).
 */
 
static int cvtimg_generate_c(struct tool *tool,struct png_image *image) {

  void *src=cvtimg_to_rgb565(image);
  if (!src) {
    fprintf(stderr,"%s: Failed to convert image\n",tool->srcpath);
    return 0;
  }
  
  const char *namestem=0;
  int namestemc=tool_guess_c_name(&namestem,tool,0,0,tool->src,tool->srcc);
  if (namestemc<1) {
    fprintf(stderr,"%s: Unable to guess object name from file name\n",tool->srcpath);
    return -1;
  }
  
  if (tool_generate_c_preamble(tool)<0) return 1;
  if (encode_raw(&tool->dst,"#include \"platform.h\"\n",-1)<0) return -1;
  
  char storagename[256];
  int storagenamec=snprintf(storagename,sizeof(storagename),"%.*s_STORAGE",namestemc,namestem);
  if ((storagenamec<1)||(storagenamec>=sizeof(storagename))) return -1;
  if (tool_generate_c_array(tool,"uint16_t",-1,storagename,storagenamec,src,image->w*image->h*2)<0) return 1;
  free(src);
  
  if (encode_fmt(&tool->dst,"const struct image %.*s={\n",namestemc,namestem)<0) return -1;
  if (encode_fmt(&tool->dst,"  .v=(void*)%.*s,\n",storagenamec,storagename)<0) return -1;
  if (encode_fmt(&tool->dst,"  .w=%d,\n",image->w)<0) return -1;
  if (encode_fmt(&tool->dst,"  .h=%d,\n",image->h)<0) return -1;
  if (encode_fmt(&tool->dst,"  .stride=%d,\n",image->w)<0) return -1;
  if (encode_fmt(&tool->dst,"};\n")<0) return -1;
  
  return 0;
}

/* Main.
 */

int main(int argc,char **argv) {
  struct tool tool={0};
  if (tool_startup(&tool,argc,argv,0)<0) return 1;
  if (tool.terminate) return 0;
  if (tool_read_input(&tool)<0) return 1;
  
  struct png_image *image=png_decode(tool.src,tool.srcc);
  if (!image) {
    fprintf(stderr,"%s: Failed to decode as PNG\n",tool.srcpath);
    return 1;
  }
  
  switch (cvtimg_guess_output_format(tool.dstpath)) {
    case CVTIMG_FORMAT_TSV: if (cvtimg_generate_tsv(&tool,image)<0) return 1; break;
    case CVTIMG_FORMAT_C: if (cvtimg_generate_c(&tool,image)<0) return 1; break;
    default: {
        fprintf(stderr,"%s: Unable to guess output format from file name\n",tool.dstpath);
        return 1;
      }
  }
  
  if (tool_write_output(&tool)<0) return 1;
  return 0;
}

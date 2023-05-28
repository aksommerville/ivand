#include "bcm_internal.h"

/* Shaders.
 */

struct bcm_vertex {
  GLfloat x;
  GLfloat y;
  GLubyte tx;
  GLubyte ty;
};

static const char bcm_vshader[]=
  "#version 100\n"
  "precision mediump float;\n"
  "attribute vec2 position;\n"
  "attribute vec2 texcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
  "  gl_Position=vec4(position,0.0,1.0);\n"
  "  vtexcoord=texcoord;\n"
  "}\n"
;

static const char bcm_fshader[]=
  "#version 100\n"
  "precision mediump float;\n"
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
  "  gl_FragColor=texture2D(sampler,vtexcoord);\n"
  "}\n"
;

/* Initialize render stuff.
 */

int bcm_render_init(struct bcm *bcm) {

  if (!(bcm->swapbuf=malloc(bcm->fbw*bcm->fbh*2))) return -1;

  GLuint vshader=glCreateShader(GL_VERTEX_SHADER);
  GLuint fshader=glCreateShader(GL_FRAGMENT_SHADER);
  const GLchar *vsv[]={bcm_vshader};
  const GLchar *fsv[]={bcm_fshader};
  GLint vslenv[]={sizeof(bcm_vshader)};
  GLint fslenv[]={sizeof(bcm_fshader)};
  glShaderSource(vshader,1,vsv,vslenv);
  glShaderSource(fshader,1,fsv,fslenv);
  glCompileShader(vshader);
  glCompileShader(fshader);
  GLint status;
  glGetShaderiv(vshader,GL_COMPILE_STATUS,&status);
  if (!status) return -1;
  glGetShaderiv(fshader,GL_COMPILE_STATUS,&status);
  if (!status) return -1;

  bcm->programid=glCreateProgram();
  glAttachShader(bcm->programid,vshader);
  glAttachShader(bcm->programid,fshader);
  glDeleteShader(vshader);
  glDeleteShader(fshader);
  glBindAttribLocation(bcm->programid,0,"position");
  glBindAttribLocation(bcm->programid,1,"texcoord");
  glLinkProgram(bcm->programid);
  glGetProgramiv(bcm->programid,GL_LINK_STATUS,&status);
  if (!status) {
    char log[4096];
    GLint logc=0;
    glGetProgramInfoLog(bcm->programid,sizeof(log),&logc,log);
    if ((logc>0)&&(logc<sizeof(log))) {
      fprintf(stderr,"LINK FAILED:\n%.*s\n",logc,log);
    }
    return -1;
  }

  glGenTextures(1,&bcm->texid);
  if (!bcm->texid) glGenTextures(1,&bcm->texid);
  if (!bcm->texid) return -1;

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,bcm->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

  int dstw,dsth,dstx,dsty;
  dstw=(bcm->fbw*bcm->screenh)/bcm->fbh;
  if (dstw<=bcm->screenw) {
    dsth=bcm->screenh;
  } else {
    dsth=(bcm->fbh*bcm->screenw)/bcm->fbw;
    dstw=bcm->screenw;
  }
  dstx=(bcm->screenw>>1)-(dstw>>1);
  dsty=(bcm->screenh>>1)-(dsth>>1);
  bcm->dstl=(dstx*2.0f)/bcm->screenw-1.0f;
  bcm->dstr=((dstx+dstw)*2.0f)/bcm->screenw-1.0f;
  bcm->dstt=1.0f-(dsty*2.0f)/bcm->screenh;
  bcm->dstb=1.0f-((dsty+dsth)*2.0f)/bcm->screenh;

  fprintf(stderr,"output bounds x=%f,%f y=%f,%f\n",bcm->dstl,bcm->dstr,bcm->dstt,bcm->dstb);

  return 0;
}

/* Swap bytes in framebuffer.
 */

static void bcm_swap_bytes(struct bcm *bcm,const uint8_t *src) {
  uint8_t *dst=bcm->swapbuf;
  int i=bcm->fbw*bcm->fbh;
  for (;i-->0;src+=2,dst+=2) {
    dst[0]=src[1];
    dst[1]=src[0];
  }
}

/* Copy framebuffer and swap, public entry point.
 */

void bcm_swap(struct bcm *bcm,const void *fb) {

  // We need GL_UNSIGNED_SHORT_5_6_5_REV but i guess that doesn't exist in ES.
  //glPixelStorei(GL_PACK_SWAP_BYTES,1); // oh dear this doesn't seem to exist either.
  bcm_swap_bytes(bcm,fb);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,bcm->fbw,bcm->fbh,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,bcm->swapbuf);

  glViewport(0,0,bcm->screenw,bcm->screenh);
  //glClearColor(0.0f,0.5f,0.0f,1.0f);
  //glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(bcm->programid);

  struct bcm_vertex vtxv[]={
    {bcm->dstl,bcm->dstt,0,0},
    {bcm->dstr,bcm->dstt,1,0},
    {bcm->dstl,bcm->dstb,0,1},
    {bcm->dstr,bcm->dstb,1,1},
  };

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_FLOAT,0,sizeof(struct bcm_vertex),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_UNSIGNED_BYTE,0,sizeof(struct bcm_vertex),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);

  eglSwapBuffers(bcm->egldisplay,bcm->eglsurface);
}

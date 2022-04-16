#include "world.h"
#include "data.h"
#include "platform.h"
#include <string.h>
#include <stdio.h>

/* Globals.
 */
 
uint8_t grid[WORLD_W_TILES*WORLD_H_TILES]={0};
struct sprite spritev[SPRITE_LIMIT]={0};
struct camera camera={0};

/* Make initial grid.
 */
 
void grid_default() {
  int16_t horizon=WORLD_H_TILES>>1;
  int16_t skysize=WORLD_W_TILES*horizon;
  memset(grid,0x00,skysize);
  memset(grid+skysize,0x11,WORLD_W_TILES);
  memset(grid+skysize+WORLD_W_TILES,0x21,sizeof(grid)-WORLD_W_TILES-skysize);
  
  // XXX TEMP
  grid[WORLD_W_TILES*horizon]=0x01;
  #define WALL(x,y) grid[(y)*WORLD_W_TILES+(x)]=0x43;
  WALL(20,15)
  WALL(24,15)
  WALL(24,14)
  WALL(28,15)
  WALL(28,14)
  WALL(28,13)
  #undef WALL
}

/* Update camera.
 */
 
void camera_update(const struct sprite *focus) {
  camera.w=CAMERA_W_MM;
  camera.h=CAMERA_H_MM;
  //return;
  
  // Initial guess, with (focus) perfectly centered, or just stay where we were.
  if (focus) {
    camera.x=focus->x+(focus->w>>1)-(camera.w>>1);
    camera.y=focus->y+(focus->h>>1)-(camera.h>>1);
  }
  
  // Clamp to vertical bounds.
  if (camera.y<0) camera.y=0;
  else if (camera.y>WORLD_H_MM-camera.h) camera.y=WORLD_H_MM-camera.h;
  
  // Wrap left edge to stay in world -- right can go off.
  while (camera.x<0) camera.x+=WORLD_W_MM;
  camera.x%=WORLD_W_MM;
}

/* Render grid.
 */
 
void grid_render(
  struct image *dst,int16_t dstxpx,int16_t dstypx,
  int16_t srcxmm,int16_t srcymm,
  int16_t wmm,int16_t hmm
) {

  // Source bounds should always be valid, but we'll double-check.
  int16_t cola=srcxmm/TILE_W_MM;
  int16_t colz=(srcxmm+wmm-1)/TILE_W_MM;
  int16_t rowa=srcymm/TILE_H_MM;
  int16_t rowz=(srcymm+hmm-1)/TILE_H_MM;
  if (cola<0) { dstxpx+=cola*-TILE_W_PIXELS; cola=0; }
  if (colz>=WORLD_W_TILES) colz=WORLD_W_TILES-1;
  if (rowa<0) { dstypx+=rowa*-TILE_H_PIXELS; rowa=0; }
  if (rowz>=WORLD_H_TILES) rowz=WORLD_H_TILES-1;
  
  // Start drawing at tile boundaries.
  dstxpx-=(srcxmm%TILE_W_MM)/MM_PER_PIXEL;
  dstypx-=(srcymm%TILE_H_MM)/MM_PER_PIXEL;
  
  const uint8_t *srcrow=grid+rowa*WORLD_W_TILES+cola;
  int16_t yi=rowz-rowa+1;
  int16_t colc=colz-cola+1;
  for (;yi-->0;srcrow+=WORLD_W_TILES,dstypx+=TILE_H_PIXELS) {
    const uint8_t *srcp=srcrow;
    int16_t xi=colc;
    int16_t xp=dstxpx;
    for (;xi-->0;srcp++,xp+=TILE_W_PIXELS) {
      int16_t srcx=((*srcp)&0x0f)*TILE_W_PIXELS;
      int16_t srcy=((*srcp)>>4)*TILE_H_PIXELS;
      image_blit_opaque(dst,xp,dstypx,&bgtiles,srcx,srcy,TILE_W_PIXELS,TILE_H_PIXELS);
    }
  }
}

/* Test grid cells.
 */
 
uint8_t grid_contains_any_solid(int16_t xmm,int16_t ymm,int16_t wmm,int16_t hmm) {

  if (ymm+hmm>WORLD_H_MM) return 1; // tiles below the world are implicitly solid
  if (ymm<0) { hmm+=ymm; ymm=0; }
  if (hmm<1) return 0;

  while (xmm<0) xmm+=WORLD_W_MM;
  while (xmm>=WORLD_W_MM) xmm-=WORLD_W_MM;
  if (xmm+wmm>WORLD_W_MM) {
    if (grid_contains_any_solid(0,ymm,xmm+wmm-WORLD_W_MM,hmm)) return 1;
    wmm=WORLD_W_MM-xmm;
  }
  if (wmm<1) return 0;
  
  int16_t rowa=ymm/TILE_H_MM;
  int16_t rowz=(ymm+hmm-1)/TILE_H_MM;
  int16_t cola=xmm/TILE_W_MM;
  int16_t colz=(xmm+wmm-1)/TILE_W_MM;
  const uint8_t *srcrow=grid+rowa*WORLD_W_TILES+cola;
  for (;rowa<=rowz;rowa++,srcrow+=WORLD_W_TILES) {
    const uint8_t *srcp=srcrow;
    int16_t i=colz-cola+1;
    for (;i-->0;srcp++) {
      if (*srcp>=0x10) return 1;
    }
  }
  
  return 0;
}

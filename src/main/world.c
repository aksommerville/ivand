#include "world.h"
#include "data.h"
#include "game.h"
#include "platform.h"
#include <string.h>
#include <stdio.h>

/* Globals.
 */
 
uint8_t grid[WORLD_W_TILES*WORLD_H_TILES]={0};
struct sprite spritev[SPRITE_LIMIT]={0};
struct camera camera={0};

static uint16_t thumbnail_storage[THUMBNAIL_W*THUMBNAIL_H];
struct image thumbnail={
  .v=thumbnail_storage,
  .w=THUMBNAIL_W,
  .h=THUMBNAIL_H,
  .stride=THUMBNAIL_W,
};

/* Make initial grid.
 */
 
void grid_default() {

  int16_t horizon=WORLD_H_TILES>>1;
  int16_t skysize=WORLD_W_TILES*horizon;
  memset(grid,0x00,skysize);
  memset(grid+skysize,0x2e,WORLD_W_TILES);
  memset(grid+skysize+WORLD_W_TILES,0x2f,sizeof(grid)-WORLD_W_TILES-skysize);
  
  // Truck. Update sprite_guard.c:violation_truck() if you move it. Also timed_tasks.c:execute_task().
  grid[WORLD_W_TILES*14+11]=0x30;
  grid[WORLD_W_TILES*15+10]=0x31;
  grid[WORLD_W_TILES*15+11]=0x32;
  grid[WORLD_W_TILES*15+12]=0x33;
  grid[WORLD_W_TILES*15+13]=0x34;
  grid[WORLD_W_TILES*15+14]=0x35;
  
  // Little hill with statue on top -- statue must start higher than truck.
  grid[WORLD_W_TILES*16+45]=0x2f;
  grid[WORLD_W_TILES*16+46]=0x2f;
  grid[WORLD_W_TILES*16+47]=0x2f;
  grid[WORLD_W_TILES*15+45]=0x2c;
  grid[WORLD_W_TILES*15+46]=0x2f;
  grid[WORLD_W_TILES*15+47]=0x2a;
  grid[WORLD_W_TILES*14+46]=0x28;
  grid[WORLD_W_TILES*13+46]=0x12;
  
  /* XXX TEMP Put the statue higher while testing, it's getting annoying. *
  grid[WORLD_W_TILES*13+46]=0x2f;
  grid[WORLD_W_TILES*12+46]=0x2f;
  grid[WORLD_W_TILES*11+46]=0x2f;
  grid[WORLD_W_TILES*10+46]=0x12;
  /**/
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

/* Join neighbors among the 9 cells centered at (x,y).
 * For now at least, this only applies to dirt.
 */
 
static inline uint8_t grid_tile_is_dirt(uint8_t tileid) {
  return ((tileid&0xf0)==0x20);
}
 
static void grid_join_neighbors_1(int16_t x,int16_t y) {

  if ((y<0)||(y>=WORLD_H_TILES)) return;
  if (x<0) x+=WORLD_W_TILES;
  else if (x>=WORLD_W_TILES) x-=WORLD_W_TILES;
  uint8_t *p=grid+y*WORLD_W_TILES+x;
  if (!grid_tile_is_dirt(*p)) return;
  
  // Which of my neighbors are dirt? Only the cardinal neighbors matter.
  // If it's OOB vertically call it a match.
  #define DIRT(dx,dy) ({ \
    uint8_t _result=1; \
    int16_t qx=x+dx; \
    if (qx<0) qx=WORLD_W_TILES-1; \
    else if (qx>=WORLD_W_TILES) qx=0; \
    int16_t qy=y+dy; \
    if ((qy>=0)&&(qy<WORLD_H_TILES)) { \
      _result=grid_tile_is_dirt(grid[qy*WORLD_W_TILES+qx]); \
    } \
    _result; \
  })
  uint8_t neighbors=
    (DIRT( 0,-1)?DIR_N:0)|
    (DIRT(-1, 0)?DIR_W:0)|
    (DIRT( 1, 0)?DIR_E:0)|
    (DIRT( 0, 1)?DIR_S:0)|
  0;
  #undef DIRT
  
  *p=0x20+neighbors;
}
 
static void grid_join_neighbors(int16_t x,int16_t y) {
  if ((x<0)||(y<0)||(x>=WORLD_W_TILES)||(y>=WORLD_H_TILES)) return;
  int8_t dx,dy;
  for (dx=-1;dx<=1;dx++) {
    for (dy=-1;dy<=1;dy++) {
      grid_join_neighbors_1(x+dx,y+dy);
    }
  }
}

/* Toggle dirt.
 */
 
uint8_t grid_remove_dirt(int16_t x,int16_t y) {
  if ((y<0)||(y>=WORLD_H_TILES)) return 0;
  if (x<0) return 0;
  if (x>=WORLD_W_TILES) x-=WORLD_W_TILES;
  if (x>=WORLD_W_TILES) return 0;
  int16_t p=y*WORLD_W_TILES+x;
  if (!grid_tile_is_dirt(grid[p])) return 0;
  if ((y>0)&&(grid[p-WORLD_W_TILES]>=0x10)) return 0; // Next row up must be empty.
  grid[p]=0x00;
  grid_join_neighbors(x,y);
  return 1;
}

uint8_t grid_add_dirt(int16_t x,int16_t y) {
  if ((y<0)||(y>=WORLD_H_TILES)) return 0;
  if (x<0) return 0;
  if (x>=WORLD_W_TILES) x-=WORLD_W_TILES;
  if (x>=WORLD_W_TILES) return 0;
  int16_t p=y*WORLD_W_TILES+x;
  if (grid[p]!=0x00) return 0; // Can only add dirt on wide-open cells.
  if ((y<WORLD_H_TILES-1)&&(grid[p+WORLD_W_TILES]<0x10)) return 0; // Next row down must be solid.
  grid[p]=0x20;
  grid_join_neighbors(x,y);
  return 1;
}

/* Check whether a given cell is surrounded by dirt.
 */
 
uint8_t grid_cell_buried(int16_t x,int16_t y) {
  if (x<0) x+=WORLD_W_TILES;
  if (x>=WORLD_W_TILES) x-=WORLD_W_TILES;
  // y in (1..WORLD_H_TILES-1): It can't be on the top, but we'll pretend everything below the world is dirt.
  if ((x<0)||(y<1)||(x>=WORLD_W_TILES)||(y>=WORLD_H_TILES)) return 0;
  
  int16_t lx=x?(x-1):(WORLD_W_TILES-1);
  int16_t rx=x+1; if (rx>=WORLD_W_TILES) rx=0;
  
  #define SOLID(n) ((n)>=0x10)
  if (!SOLID(grid[(y-1)*WORLD_W_TILES+lx])) return 0;
  if (!SOLID(grid[(y-1)*WORLD_W_TILES+x])) return 0;
  if (!SOLID(grid[(y-1)*WORLD_W_TILES+rx])) return 0;
  if (!SOLID(grid[y*WORLD_W_TILES+lx])) return 0;
  if (!SOLID(grid[y*WORLD_W_TILES+rx])) return 0;
  if (y<WORLD_H_TILES-1) {
    if (!SOLID(grid[(y+1)*WORLD_W_TILES+lx])) return 0;
    if (!SOLID(grid[(y+1)*WORLD_W_TILES+x])) return 0;
    if (!SOLID(grid[(y+1)*WORLD_W_TILES+rx])) return 0;
  }
  #undef SOLID
  return 1;
}

/* Draw thumbnail.
 */
 
void thumbnail_draw() {
  const uint16_t color_frame=0x0000;
  const uint16_t color_sky  =0xffff;
  const uint16_t color_dirt =0x1084;
  const uint16_t color_other=0x0842;
  
  // 1-pixel frame.
  image_fill_rect(&thumbnail,0,0,thumbnail.w,1,color_frame);
  image_fill_rect(&thumbnail,0,thumbnail.h-1,thumbnail.w,1,color_frame);
  image_fill_rect(&thumbnail,0,1,1,thumbnail.h-2,color_frame);
  image_fill_rect(&thumbnail,thumbnail.w-1,1,1,thumbnail.h-2,color_frame);
  
  // For the interior, every 4 cells of the grid correspond to 1 pixel of the thumbnail.
  // Check it using the constants to be safe, this check should disappear during compilation.
  if ((THUMBNAIL_W-2==WORLD_W_TILES>>1)&&(THUMBNAIL_H-2==WORLD_H_TILES>>1)) {
    const uint8_t *srcrow=grid;
    uint16_t *dstrow=thumbnail.v+thumbnail.stride+1;
    int16_t yi=THUMBNAIL_H-2;
    for (;yi-->0;srcrow+=WORLD_W_TILES*2,dstrow+=thumbnail.stride) {
      const uint8_t *srcp=srcrow;
      uint16_t *dstp=dstrow;
      int16_t xi=THUMBNAIL_W-2;
      #define DESC1(tile) switch ((tile)&0xf0) { \
        case 0x00: sky=1; break; \
        case 0x20: dirt=1; break; \
        default: other=1; break; \
      }
      #define DESCRIBE \
        uint8_t sky=0,dirt=0,other=0; \
        DESC1(srcp[0]) \
        DESC1(srcp[1]) \
        DESC1(srcp[WORLD_W_TILES]) \
        DESC1(srcp[WORLD_W_TILES+1])
      if (yi>=((THUMBNAIL_H-2)>>1)) { // upper half, accentuate dirt
        for (;xi-->0;srcp+=2,dstp++) {
          DESCRIBE
          if (other) *dstp=color_other;
          else if (dirt) *dstp=color_dirt;
          else *dstp=color_sky;
        }
      } else { // lower half, accentuate sky
        for (;xi-->0;srcp+=2,dstp++) {
          DESCRIBE
          if (other) *dstp=color_other;
          else if (sky) *dstp=color_sky;
          else *dstp=color_dirt;
        }
      }
      #undef DESC1
      #undef DESCRIBE
    }
  
  } else { // dammit andy you broke something
    image_fill_rect(&thumbnail,1,1,thumbnail.w-2,thumbnail.h-2,0xff00);
  }
}

/* Scoring.
 */
 
uint32_t get_elevation_score() {
  const uint8_t *p=grid;
  uint16_t emptyc=0;
  uint16_t i=WORLD_H_TILES>>1;
  for (;i-->0;) {
    // There is no empty tile except zero that could appear alone at ground level, so we only need to look for zeroes.
    uint8_t empty=1;
    uint16_t xi=WORLD_W_TILES;
    for (;xi-->0;p++) {
      if (*p) { empty=0; break; }
    }
    if (!empty) break;
    emptyc++;
  }
  return (WORLD_H_TILES>>1)-emptyc;
}

uint32_t get_depth_score() {
  // Same idea as elevation score, but count backward from the end.
  const uint8_t *p=grid+WORLD_W_TILES*WORLD_H_TILES-1;
  uint16_t fullc=0;
  uint16_t i=WORLD_H_TILES>>1;
  for (;i-->0;) {
    // Full is anything >=0x10
    uint8_t full=1;
    uint16_t xi=WORLD_W_TILES;
    for (;xi-->0;p--) {
      if (*p<0x10) { full=0; break; }
    }
    if (!full) break;
    fullc++;
  }
  return (WORLD_H_TILES>>1)-fullc;
}

const char *get_validation_message() {
  // Order matters:
  if (!hp) return "Must be alive to win.";
  if (violation_statue()) return "Statue not on top.";
  if (violation_truck()) return "Truck not unloaded.";
  if (hero_is_holding_barrel()||violation_barrel()) return "Barrel not buried.";
  return 0;
}

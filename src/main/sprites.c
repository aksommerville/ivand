#include "game.h"
#include "world.h"
#include "platform.h"
#include "data.h"
#include <stdio.h>
#include <string.h>

/* Find any unused sprite.
 */
 
struct sprite *sprite_new() {
  struct sprite *sprite=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;sprite++) {
    if (sprite->controller==SPRITE_CONTROLLER_NONE) {
      memset(sprite,0,sizeof(struct sprite));
      return sprite;
    }
  }
  return 0;
}

/* Test feet on solid ground.
 */
 
uint8_t sprite_is_grounded(const struct sprite *sprite) {
  int16_t bottom=sprite->y+sprite->h;

  // The absolute floor is solid, and don't let a sprite pass through it either.
  if (bottom>=WORLD_H_MM) return 1;

  // Feet align with the grid? Check the row below them.
  if (!(bottom%TILE_H_MM)) {
    if (grid_contains_any_solid(sprite->x,bottom,sprite->w,1)) {
      return 1;
    }
  }

  return 0;
}

/* Move horizontally.
 */
 
int16_t sprite_move_horz(struct sprite *sprite,int16_t dx) {

  // Restrict horizontal movement to 1 tile/frame.
  // This means we can be certain that no more than 1 new column of the grid is in play per movement.
  if (dx<-TILE_W_MM) dx=-TILE_W_MM;
  else if (dx>TILE_W_MM) dx=TILE_W_MM;
  
  // Measure the space newly covered.
  int16_t ckx,ckw;
  if (dx<0) {
    ckx=sprite->x+dx;
    ckw=-dx;
  } else if (dx>0) {
    ckx=sprite->x+sprite->w;
    ckw=dx;
  } else return 0;
  
  // If there's a grid collision, clamp to the next column boundary.
  if (grid_contains_any_solid(ckx,sprite->y,ckw,sprite->h)) {
    if (dx<0) {
      dx=TILE_W_MM-sprite->x%TILE_W_MM;
      if (dx==TILE_W_MM) return 0;
    } else {
      int16_t rightx=sprite->x+sprite->w+dx;
      int16_t wallx=rightx-rightx%TILE_W_MM;
      if (!(dx=wallx-sprite->w-sprite->x)) return 0;
    }
  }
  
  // Commit the move and check for wrap.
  sprite->x+=dx;
  if (dx<0) {
    if (sprite->x<0) sprite->x+=WORLD_W_MM;
  } else {
    if (sprite->x>=WORLD_W_MM) sprite->x-=WORLD_W_MM;
  }
  
  return dx;
}

/* Render position.
 */
 
void sprite_get_render_position(int16_t *x,int16_t *y,const struct sprite *sprite) {
  // Careful here, it's floor division, so we want to first scale everything to pixels, then translate.
  *x=sprite->x/MM_PER_PIXEL-camera.x/MM_PER_PIXEL;
  if (
    (camera.x+camera.w>WORLD_W_MM)&&
    (sprite->x+sprite->w<=camera.x)
  ) (*x)+=WORLD_W_PIXELS;
  *y=sprite->y/MM_PER_PIXEL-camera.y/MM_PER_PIXEL;
}

/* Dummy.
 */
 
void sprite_render_dummy(struct sprite *sprite) {
//TODO
}

/* Guard.
 */

void sprite_update_guard(struct sprite *sprite) {
//TODO
}

void sprite_render_guard(struct sprite *sprite) {
//TODO
}

/* Shovel.
 */
 
#define SHOVEL_ANIMCLOCK (sprite->opaque[0])
#define SHOVEL_TATTLE (sprite->opaque[1]) /* NB: Ivan cheats and accesses this directly */
 
void sprite_render_shovel(struct sprite *sprite) {

  if (SHOVEL_ANIMCLOCK) SHOVEL_ANIMCLOCK--;
  else SHOVEL_ANIMCLOCK=40;
  
  uint8_t frame=0;
  if (SHOVEL_ANIMCLOCK>=20) frame=1;
  else frame=0;
  
  int16_t x,y;
  sprite_get_render_position(&x,&y,sprite);
  image_blit_colorkey(&fb,x,y,&fgbits,0+frame*13,12,13,5);
  
  if (SHOVEL_TATTLE) {
    int16_t midx=x+(sprite->w>>1)/MM_PER_PIXEL;
    int16_t bubblew=40;
    render_dialogue_bubble(midx-(bubblew>>1),y-19,bubblew,14,midx);
    image_blit_colorkey(&fb,midx-17,y-16,&fgbits,15,28,5,5);
    image_blit_string(&fb,midx-9,y-17,"Pick up",7,0x0000,font);
  }
}

#undef SHOVEL_ANIMCLOCK
#undef SHOVEL_TATTLE

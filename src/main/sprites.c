#include "game.h"
#include "world.h"
#include "platform.h"
#include <stdio.h>

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
  
  // Check other sprites, if our bottom matches their top exactly.
  const struct sprite *other=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;other++) {
    if (other->y!=bottom) continue;
    //TODO accomodate horizontal wraparound
    if (sprite->x>=other->x+other->w) continue;
    if (other->x>=sprite->x+sprite->w) continue;
    return 1;
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
  
  // Check solid sprites. TODO will there ever be solid sprites? maybe we can skip this
  int16_t bottom=sprite->y+sprite->h;
  int16_t left=sprite->x+dx; // the proposed new left
  int16_t right=left+sprite->w;
  const struct sprite *other=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;other++) {
    if (other->y>=bottom) continue;
    if (other->y+other->h<=sprite->y) continue;
    if (other==sprite) continue;
    if (other->x>=right) continue;
    if (other->x+other->w<=left) continue;
    //TODO sprite-on-sprite horz collision
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

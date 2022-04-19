#include "game.h"
#include "world.h"
#include "data.h"
#include "platform.h"
#include <stdio.h>

#define OVERSIGHT_NEAR_DISTANCE (TILE_W_MM*3)
#define OVERSIGHT_FAR_DISTANCE (TILE_W_MM*4)
#define WALK_SPEED ((MM_PER_PIXEL*6)/8)
#define WALK_FRAME_TIME 6
#define JUMP_POWER_INITIAL ((MM_PER_PIXEL*12)/8)
#define JUMP_DECAY ((MM_PER_PIXEL*1)/8)
#define CLIMB_SPEED ((MM_PER_PIXEL*2)/8)
#define CLIMB_FRAME_TIME 8

/* Object definition.
 */
 
struct sprite_guard {
  SPRITE_HEADER
  int8_t facedir;
  uint8_t grounded; // 0|1, updated from scratch early in the cycle
  int8_t motion; // (-1,0,1) which way are we moving (NB not related to facedir)
  uint8_t jump_power; // counts down; nonzero midjump
  uint8_t climbing;
  uint8_t animclock;
  uint8_t animframe;
};

#define SPRITE ((struct sprite_guard*)sprite)

/* Check the grid in case some asshole dropped a load of dirt on me.
 */
 
static void guard_check_fill_in(struct sprite *sprite) {
  if (grid_contains_any_solid(sprite->x,sprite->y,sprite->w,sprite->h)) {
    sprite->y-=TILE_H_MM;
  }
}

/* Check my footing and fall if appropriate.
 */
 
static void guard_update_gravity(struct sprite *sprite) {

  if (SPRITE->climbing) {
    sprite->y-=CLIMB_SPEED;
    return;
  }

  if (SPRITE->jump_power) {
    sprite->y-=SPRITE->jump_power;
    if (SPRITE->jump_power<=JUMP_DECAY) SPRITE->jump_power=0;
    else SPRITE->jump_power-=JUMP_DECAY;
    return;
  }

  if (sprite_move_vert(sprite,MM_PER_PIXEL)) {
    if (SPRITE->grounded) {
      // begin falling...
      SPRITE->grounded=0;
    }

  } else {
    if (!SPRITE->grounded) {
      // hit the ground...
      SPRITE->grounded=1;
    } else {
      guard_check_fill_in(sprite);
    }
  }
}

/* Determine the nearest direction to the hero (beware of wrapping), and face that way.
 * Decide whether to advance or retreat or hold steady.
 */
 
static void guard_update_oversight(struct sprite *sprite) {
  struct sprite *hero=game_get_hero();
  if (!hero) return;
  
  // If more than half the world width, or less than negative half, we need to wrap it.
  int16_t dist=hero->x-sprite->x;
  if (dist>WORLD_W_MM>>1) dist-=WORLD_W_MM;
  else if (dist<-(WORLD_W_MM>>1)) dist+=WORLD_W_MM;
  
  if (dist<0) {
    SPRITE->facedir=-1;
    if (dist<-OVERSIGHT_FAR_DISTANCE) SPRITE->motion=-1;
    else if (dist>-OVERSIGHT_NEAR_DISTANCE) SPRITE->motion=1;
    else SPRITE->motion=0;
  } else {
    SPRITE->facedir=1;
    if (dist>OVERSIGHT_FAR_DISTANCE) SPRITE->motion=1;
    else if (dist<OVERSIGHT_NEAR_DISTANCE) SPRITE->motion=-1;
    else SPRITE->motion=0;
  }
}

/* Move horizontally if needed.
 */
 
static void guard_update_motion(struct sprite *sprite) {
  if (!SPRITE->motion) return;
  
  // If we're able to move, abort climbing and get on with our lives.
  // Note that this is the only thing that ends climbing, we don't specifically track the wall or anything.
  int16_t dx=sprite_move_horz(sprite,WALK_SPEED*SPRITE->motion);
  if (dx) {
    SPRITE->climbing=0;
    return;
  }
  
  // If we're blocked while walking backward, just stop.
  if (SPRITE->motion!=SPRITE->facedir) return;
  
  // If we're already jumping or climbing, let it ride.
  if (SPRITE->jump_power||SPRITE->climbing) return;
  
  // Find the two tiles at my face.
  int16_t col=(sprite->x+(sprite->w>>1))/TILE_W_MM+SPRITE->motion;
  if (col<0) col+=WORLD_W_TILES;
  else if (col>=WORLD_W_TILES) col-=WORLD_W_TILES;
  if ((col<0)||(col>=WORLD_W_TILES)) return;
  int16_t row=(sprite->y+sprite->h)/TILE_W_MM-1;
  if ((row<0)||(row>=WORLD_H_TILES)) return;
  uint8_t lotile=grid[WORLD_W_TILES*row+col];
  uint8_t hitile=row?grid[WORLD_W_TILES*(row-1)+col]:0;
  
  // If the upper tile is vacant, we can jump.
  if (hitile<=0x10) {
    if (sprite_is_grounded(sprite)) {
      SPRITE->jump_power=JUMP_POWER_INITIAL;
    }
    return;
  }
  
  // Both are solid, so we need to climb.
  SPRITE->climbing=1;
  SPRITE->animclock=0;
  SPRITE->animframe=0;
}

/* Update.
 */

void sprite_update_guard(struct sprite *sprite) {
  guard_update_gravity(sprite);
  guard_update_oversight(sprite);
  guard_update_motion(sprite);
}

/* Render.
 */

void sprite_render_guard(struct sprite *sprite) {
  int16_t x,y;
  sprite_get_render_position(&x,&y,sprite);
  
  uint8_t torsoframe=0,legframe=0,climbframe=0xff;
  
  if (SPRITE->climbing) {
    if (SPRITE->animclock>0) {
      SPRITE->animclock--;
    } else {
      SPRITE->animclock=CLIMB_FRAME_TIME;
      SPRITE->animframe++;
      if (SPRITE->animframe>=3) SPRITE->animframe=0;
    }
    climbframe=SPRITE->animframe;
  } else if (SPRITE->motion) {
    if (SPRITE->animclock>0) {
      SPRITE->animclock--;
    } else {
      SPRITE->animclock=WALK_FRAME_TIME;
      SPRITE->animframe++;
      if (SPRITE->animframe>=4) SPRITE->animframe=0;
    }
    legframe=SPRITE->animframe;
  } else {
    SPRITE->animclock=0;
    SPRITE->animframe=0;
  }
  torsoframe=legframe;
  
  // Draw head, torso, and legs.
  if (SPRITE->facedir<0) {
    if (climbframe<0xff) {
      image_blit_colorkey_flop(&fb,x,y+4,&fgbits,28+climbframe*5,5,5,8);
    } else {
      image_blit_colorkey_flop(&fb,x-2,y+4,&fgbits,22+torsoframe*9,0,9,5);
      image_blit_colorkey_flop(&fb,x-1,y+8,&fgbits,legframe*7,9,7,3);
    }
    image_blit_colorkey_flop(&fb,x-2,y,&fgbits,10,0,7,5);
  } else {
    if (climbframe<0xff) {
      image_blit_colorkey(&fb,x,y+4,&fgbits,28+climbframe*5,5,5,8);
    } else {
      image_blit_colorkey(&fb,x-2,y+4,&fgbits,22+torsoframe*9,0,9,5);
      image_blit_colorkey(&fb,x-1,y+8,&fgbits,legframe*7,9,7,3);
    }
    image_blit_colorkey(&fb,x,y,&fgbits,10,0,7,5);
  }
}

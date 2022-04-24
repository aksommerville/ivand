#include "game.h"
#include "world.h"
#include "data.h"
#include "platform.h"
#include <stdio.h>

#define OVERSIGHT_NEAR_DISTANCE (TILE_W_MM*3)
#define OVERSIGHT_FAR_DISTANCE (TILE_W_MM*4)
#define SHOOT_RANGE_MM (TILE_W_MM*6)
#define WALK_SPEED ((MM_PER_PIXEL*6)/8)
#define WALK_FRAME_TIME 6
#define JUMP_POWER_INITIAL ((MM_PER_PIXEL*12)/8)
#define JUMP_DECAY ((MM_PER_PIXEL*1)/8)
#define CLIMB_SPEED ((MM_PER_PIXEL*2)/8)
#define CLIMB_FRAME_TIME 8
#define RULES_CLOCK_TIME 60
#define RELOAD_TIME_FRAMES 60

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
  uint8_t rulesclock;
  uint8_t violation; // TATTLE_{NONE,STATUE,TRUCK,BARREL}
  uint8_t reload; // counts down after firing gun
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
  
  // If climbing, don't let it be zero, otherwise we won't notice the top edge.
  if (dist<0) {
    SPRITE->facedir=-1;
    if (dist<-OVERSIGHT_FAR_DISTANCE) SPRITE->motion=-1;
    else if (dist>-OVERSIGHT_NEAR_DISTANCE) SPRITE->motion=1;
    else if (SPRITE->climbing) SPRITE->motion=-1;
    else SPRITE->motion=0;
  } else {
    SPRITE->facedir=1;
    if (dist>OVERSIGHT_FAR_DISTANCE) SPRITE->motion=1;
    else if (dist<OVERSIGHT_NEAR_DISTANCE) SPRITE->motion=-1;
    else if (SPRITE->climbing) SPRITE->motion=1;
    else SPRITE->motion=0;
  }
}

/* Move horizontally if needed.
 */
 
static void guard_update_motion(struct sprite *sprite) {
  if (!SPRITE->motion) return;
  
  // If we're able to move, abort climbing and get on with our lives.
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
  if (hitile<0x10) {
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

/* Check world for specific violations.
 */
 
// Truck must be unloaded if anything is there. 0x33,0x34,0x35.
// This is in a fixed position. The bed is cells (12,14),(13,14),(14,14)
uint8_t violation_truck() {
  const uint8_t *p=grid+WORLD_W_TILES*14+12;
  uint8_t i=3;
  for (;i-->0;p++) {
    if (*p>=0x10) return 1;
  }
  return 0;
}

// The statue (0x12) must be the tallest non-vacant thing, even a tie is a violation.
// If no statue found, the player must be carrying it. That's a violation too.
uint8_t violation_statue() {
  const uint8_t *p=grid;
  uint8_t y=WORLD_H_TILES;
  for (;y-->0;) {
    uint8_t x=WORLD_W_TILES,statue=0;
    for (;x-->0;p++) {
      if (*p==0x12) statue=1;
      else if (*p>=0x10) return 1;
    }
    if (statue) return 0;
  }
  return 1; // no statue and also nothing else...
}

// If a barrel (0x11) exists, it must have dirt (0x20..0x2f) on all 8 sides.
uint8_t violation_barrel() {
  const uint8_t *p=grid;
  uint8_t y=0;
  for (;y<WORLD_H_TILES;y++) {
    uint8_t x=0;
    for (;x<WORLD_W_TILES;x++,p++) {
      if (*p==0x11) {
        if (!grid_cell_buried(x,y)) return 1;
      }
    }
  }
  return 0;
}

/* Update rules.
 * If there is a violation in progress, just confirm that it is still in violation.
 * Otherwise tick the rules clock and each reset, look for new violations.
 */
 
static void guard_update_rules(struct sprite *sprite) {

  if (SPRITE->violation) {
    set_tattle(sprite->x+(sprite->w>>1),sprite->y-MM_PER_PIXEL*2,SPRITE->violation);
    switch (SPRITE->violation) {
      case TATTLE_TRUCK: if (!violation_truck()) { SPRITE->rulesclock=RULES_CLOCK_TIME; SPRITE->violation=TATTLE_NONE; } break;
      case TATTLE_STATUE: if (!violation_statue()) { SPRITE->rulesclock=RULES_CLOCK_TIME; SPRITE->violation=TATTLE_NONE; } break;
      case TATTLE_BARREL: if (!violation_barrel()) { SPRITE->rulesclock=RULES_CLOCK_TIME; SPRITE->violation=TATTLE_NONE; } break;
    }
    return;
  }
  
  if (SPRITE->rulesclock) {
    SPRITE->rulesclock--;
    return;
  }
  SPRITE->rulesclock=RULES_CLOCK_TIME;
  
  // TRUCK before BARREL, otherwise he says "Bury barrel" when it first appears, which feels wrong.
       if (violation_statue()) SPRITE->violation=TATTLE_STATUE;
  else if (violation_truck()) SPRITE->violation=TATTLE_TRUCK;
  else if (violation_barrel()) SPRITE->violation=TATTLE_BARREL;
  
  if (SPRITE->violation) set_tattle(sprite->x+(sprite->w>>1),sprite->y-MM_PER_PIXEL*2,SPRITE->violation);
}

/* If a violation is in progress and we're done reloading and Ivan is in sight, shoot him.
 * Updates the reload counter.
 */
 
static void guard_update_gun(struct sprite *sprite) {
  
  if (SPRITE->reload>0) {
    SPRITE->reload--;
    return;
  }
  
  if (!SPRITE->violation) return;
  if (SPRITE->climbing||SPRITE->jump_power) return;
  if (!hp) return;
  
  struct sprite *hero=game_get_hero();
  if (!hero) return;
  
  // We'll call "line of sight" as "my middle within his vertical bounds"
  int16_t midy=sprite->y+(sprite->h>>1);
  if (midy<hero->y) return;
  if (midy>=hero->y+hero->h) return;
  
  // Sanity check on the total distance. No sense firing if we're on the other side of the world.
  int16_t dx=sprite->x-hero->x;
  if (dx<0) dx=-dx;
  if (dx>WORLD_W_MM>>1) dx=WORLD_W_MM-dx;
  if (dx>SHOOT_RANGE_MM) return;
  
  struct sprite *bullet=sprite_new();
  if (!bullet) return;
  bullet->controller=SPRITE_CONTROLLER_BULLET;
  bullet->w=2*MM_PER_PIXEL;
  bullet->h=2*MM_PER_PIXEL;
  bullet->y=sprite->y+5*MM_PER_PIXEL;
  if (SPRITE->facedir<0) {
    bullet->x=sprite->x-bullet->w;
    bullet->opaque[0]=0xff;
  } else {
    bullet->x=sprite->x+sprite->w;
    bullet->opaque[0]=0x01;
  }
  
  SPRITE->reload=RELOAD_TIME_FRAMES;
  //TODO sound effect
}

/* Update.
 */

void sprite_update_guard(struct sprite *sprite) {
  guard_update_gravity(sprite);
  guard_update_oversight(sprite);
  guard_update_motion(sprite);
  guard_update_rules(sprite);
  guard_update_gun(sprite);
}

/* Render.
 */

void sprite_render_guard(struct sprite *sprite) {
  int16_t x,y;
  sprite_get_render_position(&x,&y,sprite);
  
  // Head: Doesn't change much.
  if (SPRITE->facedir<0) {
    image_blit_colorkey_flop(&fb,x-2,y,&fgbits,10,0,7,5);
  } else {
    image_blit_colorkey(&fb,x,y,&fgbits,10,0,7,5);
  }
  
  // If climbing, the whole body is one image and it animates.
  if (SPRITE->climbing) {
    if (SPRITE->animclock>0) {
      SPRITE->animclock--;
    } else {
      SPRITE->animclock=CLIMB_FRAME_TIME;
      SPRITE->animframe++;
      if (SPRITE->animframe>=3) SPRITE->animframe=0;
    }
    if (SPRITE->facedir<0) {
      image_blit_colorkey_flop(&fb,x,y+4,&fgbits,28+SPRITE->animframe*5,5,5,8);
    } else {
      image_blit_colorkey(&fb,x,y+4,&fgbits,28+SPRITE->animframe*5,5,5,8);
    }
    return;
  }
  
  // Legs work about the same whether Idle, Walk, or Violation.
  uint8_t legframe=0;
  if (SPRITE->motion) {
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
  if (SPRITE->facedir<0) {
    image_blit_colorkey_flop(&fb,x-1,y+8,&fgbits,legframe*7,9,7,3);
  } else {
    image_blit_colorkey(&fb,x-1,y+8,&fgbits,legframe*7,9,7,3);
  }
  
  // Violation torso is a single frame. Otherwise it animates with the legs.
  if (SPRITE->violation) {
    if (SPRITE->facedir<0) {
      image_blit_colorkey_flop(&fb,x-3,y+4,&fgbits,43,5,8,5);
    } else {
      image_blit_colorkey(&fb,x,y+4,&fgbits,43,5,8,5);
    }
  } else {
    if (SPRITE->facedir<0) {
      image_blit_colorkey_flop(&fb,x-2,y+4,&fgbits,22+legframe*9,0,9,5);
    } else {
      image_blit_colorkey(&fb,x-2,y+4,&fgbits,22+legframe*9,0,9,5);
    }
  }
}

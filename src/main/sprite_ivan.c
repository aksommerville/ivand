#include "game.h"
#include "world.h"
#include "data.h"
#include "platform.h"
#include <stdio.h>

#define WALK_SPEED MM_PER_PIXEL
#define WALK_FRAME_TIME 5
#define JUMP_POWER_MAX 12
#define INJURY_TIME 30

#define CARRYING_NONE 0
#define CARRYING_SHOVEL 1
#define CARRYING_SHOVEL_FULL 2
#define CARRYING_BRICK 3
#define CARRYING_STATUE 4
#define CARRYING_BARREL 5

/* Object definition.
 */
 
struct sprite_ivan {
  SPRITE_HEADER
  int8_t facedir; // -1,1 = left,right
  int8_t dx; // (-1,0,1) while button held
  int8_t dy; // ''
  int8_t dyimpulse; // (-1,0,1), nonzero for one frame at a time
  uint8_t injump; // 1 while held
  uint8_t inaux; // 1 momentarily at press
  uint8_t animclock;
  uint8_t animframe;
  uint8_t jumppower;
  uint8_t carrying; // either the shovel or a block over my head, or nothing
  uint8_t injury_highlight;
  uint16_t idleframec;
  uint8_t fairy_triggered;
};

#define SPRITE ((struct sprite_ivan*)sprite)

/* Get the hero sprite.
 */

struct sprite *game_get_hero() {
  struct sprite *sprite=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;sprite++) if (sprite->controller==SPRITE_CONTROLLER_IVAN) return sprite;
  return 0;
}

/* Receive input.
 */
 
void sprite_input_ivan(struct sprite *sprite,uint8_t input,uint8_t pvinput) {

  switch (input&(BUTTON_LEFT|BUTTON_RIGHT)) {
    case BUTTON_LEFT: SPRITE->dx=SPRITE->facedir=-1; break;
    case BUTTON_RIGHT: SPRITE->dx=SPRITE->facedir=1; break;
    default: SPRITE->dx=0; break;
  }
  
  SPRITE->dyimpulse=0;
  switch (input&(BUTTON_UP|BUTTON_DOWN)) {
    case BUTTON_UP: {
        if (SPRITE->dy>=0) SPRITE->dyimpulse=-1;
        SPRITE->dy=-1;
      } break;
    case BUTTON_DOWN: {
        if (SPRITE->dy<=0) SPRITE->dyimpulse=1;
        SPRITE->dy=1;
      } break;
    default: SPRITE->dy=0; break;
  }
  
  SPRITE->injump=(input&BUTTON_A)?1:0;
  
  if ((input&BUTTON_B)&&!(pvinput&BUTTON_B)) {
    SPRITE->inaux=1;
  } else {
    SPRITE->inaux=0;
  }
}

/* Walking.
 */
 
static void ivan_update_walk(struct sprite *sprite) {
  if (SPRITE->dx) { // Walking explicitly.
    sprite_move_horz(sprite,SPRITE->dx*WALK_SPEED);
  } else { // Cheat to the nearest tile (aligning horizontal centers)
    int16_t refx=sprite->x+(sprite->w>>1)-(TILE_W_MM>>1);
    int16_t slop=refx%TILE_W_MM;
    if (slop) {
      // We could check like if (slop) is very high or very low, cheat that way regardless of motion.
      // For now I'm saying continue in the direction the user pressed.
      // So the tiniest tap in either direction moves you by exactly one tile.
      if (SPRITE->facedir<0) {
        if (slop<WALK_SPEED) sprite_move_horz(sprite,-slop);
        else sprite_move_horz(sprite,-WALK_SPEED);
      } else {
        if (slop>TILE_W_MM-WALK_SPEED) sprite_move_horz(sprite,TILE_W_MM-slop);
        else sprite_move_horz(sprite,WALK_SPEED);
      }
    }
  }
}

/* Jumping and gravity.
 */
 
static void ivan_update_jump(struct sprite *sprite) {
  if (SPRITE->injump&&SPRITE->jumppower) {
    SPRITE->jumppower--;
    uint8_t power=SPRITE->jumppower>>1;
    if (power<1) power=1;
    else if (power>2) power=2;
    sprite->y-=power*MM_PER_PIXEL;
  } else {
    if (sprite_is_grounded(sprite)) {
      if (SPRITE->injump) SPRITE->jumppower=0;
      else SPRITE->jumppower=JUMP_POWER_MAX;
    } else {
      sprite->y+=GRAVITY;
      SPRITE->jumppower=0;
    }
  }
}

/* Tattle.
 */

static void ivan_check_tattle(struct sprite *sprite) {

  // No tattles from us if we're carrying something.
  if (SPRITE->carrying) return;
  
  int16_t midx=sprite->x+(sprite->w>>1);
  int16_t midy=sprite->y+(sprite->h>>1);
  
  struct sprite *other=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;other++) {
    if (!other->controller) continue;
    if (midx<other->x) continue;
    if (midy<other->y) continue;
    if (midx>=other->x+other->w) continue;
    if (midy>=other->y+other->h) continue;
    switch (other->controller) {
      case SPRITE_CONTROLLER_SHOVEL: {
          set_tattle(other->x+(other->w>>1),other->y,TATTLE_SHOVEL);
        } return;
    }
  }
  
  if (sprite_is_grounded(sprite)) {
    int16_t col=midx/TILE_W_MM;
    if (col>=WORLD_W_TILES) col-=WORLD_W_TILES;
    if ((col>=0)&&(col<WORLD_W_TILES)) {
      int16_t row=(sprite->y+sprite->h+(TILE_H_MM>>1))/TILE_H_MM;
      if ((row>=0)&&(row<WORLD_H_TILES)) {
        uint8_t tile=grid[row*WORLD_W_TILES+col];
        switch (tile) {
          case 0x10: 
          case 0x11: 
          case 0x12: {
            set_tattle(col*TILE_W_MM+(TILE_W_MM>>1),row*TILE_H_MM-(12*MM_PER_PIXEL),TATTLE_PICKUP);
          } return;
        }
      }
    }
  }
}

/* Shovel actions.
 */
 
static uint8_t ivan_pickup_shovel(struct sprite *sprite) {
  if (SPRITE->carrying!=CARRYING_NONE) return 0;
  int16_t x=sprite->x+(sprite->w>>1);
  int16_t y=sprite->y+(sprite->h>>1);
  struct sprite *shovel=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;shovel++) {
    if (shovel->controller!=SPRITE_CONTROLLER_SHOVEL) continue;
    if (x<shovel->x) continue;
    if (y<shovel->y) continue;
    if (x>=shovel->x+shovel->w) continue;
    if (y>=shovel->y+shovel->h) continue;
    shovel->controller=SPRITE_CONTROLLER_NONE;
    SPRITE->carrying=CARRYING_SHOVEL;
    return 1;
  }
  return 0;
}
 
static void ivan_drop_shovel(struct sprite *sprite) {
  
  if (SPRITE->carrying!=CARRYING_SHOVEL) return;
  if (!sprite_is_grounded(sprite)) return;
  
  SPRITE->carrying=CARRYING_NONE;
  
  struct sprite *shovel=sprite_new();
  if (shovel) {
    shovel->controller=SPRITE_CONTROLLER_SHOVEL;
    shovel->w=13*MM_PER_PIXEL;
    shovel->h=5*MM_PER_PIXEL;
    shovel->x=sprite->x+(sprite->w>>1)-(shovel->w>>1);
    shovel->y=sprite->y+4*MM_PER_PIXEL;
  }
}
 
static void ivan_dig(struct sprite *sprite) {
  if (SPRITE->carrying!=CARRYING_SHOVEL) return;
  if (!sprite_is_grounded(sprite)) return;
  int row=(sprite->y+sprite->h+(TILE_H_MM>>1))/TILE_H_MM;
  int col=(sprite->x+(sprite->w>>1))/TILE_W_MM;
  if (grid_remove_dirt(col,row)) {
    thumbnail_draw();//TODO consider incremental redraw; only one pixel could have changed
    SPRITE->carrying=CARRYING_SHOVEL_FULL;
    // No need to move Ivan; gravity will take over.
  }
}
 
static void ivan_deposit(struct sprite *sprite) {
  if (SPRITE->carrying!=CARRYING_SHOVEL_FULL) return;
  if (!sprite_is_grounded(sprite)) return;
  // In any other game we would need to validate headroom.
  // A quirk of this one is there will never be ceilings, so no need for that.
  int row=(sprite->y+sprite->h-(TILE_H_MM>>1))/TILE_H_MM;
  int col=(sprite->x+(sprite->w>>1))/TILE_W_MM;
  if (grid_add_dirt(col,row)) {
    thumbnail_draw();//TODO consider incremental redraw; only one pixel could have changed
    SPRITE->carrying=CARRYING_SHOVEL;
    sprite->y-=TILE_H_MM;
  }
}

/* Pickup and drop tile objects.
 */
 
static void ivan_pickup(struct sprite *sprite) {
  if (SPRITE->carrying!=CARRYING_NONE) return;
  if (!sprite_is_grounded(sprite)) return;
  int16_t midx=sprite->x+(sprite->w>>1);
  int16_t col=midx/TILE_W_MM;
  if (col>=WORLD_W_TILES) col-=WORLD_W_TILES;
  if ((col<0)||(col>=WORLD_W_TILES)) return;
  int16_t row=(sprite->y+sprite->h+(TILE_H_MM>>1))/TILE_H_MM;
  if ((row<0)||(row>=WORLD_H_TILES)) return;
  
  uint8_t tile=grid[row*WORLD_W_TILES+col];
  switch (tile) {
    case 0x10: SPRITE->carrying=CARRYING_BRICK; break;
    case 0x11: SPRITE->carrying=CARRYING_BARREL; break;
    case 0x12: SPRITE->carrying=CARRYING_STATUE; break;
    default: return;
  }
  grid[row*WORLD_W_TILES+col]=0x00;
  thumbnail_draw();//TODO consider incremental redraw; only one pixel could have changed
}
 
static void ivan_drop(struct sprite *sprite) {
  
  // Verify we're carrying something droppable, and record the tileid.
  uint8_t tileid;
  switch (SPRITE->carrying) {
    case CARRYING_BRICK: tileid=0x10; break;
    case CARRYING_BARREL: tileid=0x11; break;
    case CARRYING_STATUE: tileid=0x12; break;
    default: return;
  }
  if (!sprite_is_grounded(sprite)) return;
  
  // Find the cell where our crotch is and verify it's fully empty.
  int16_t midx=sprite->x+(sprite->w>>1);
  int16_t col=midx/TILE_W_MM;
  if (col>=WORLD_W_TILES) col-=WORLD_W_TILES;
  if ((col<0)||(col>=WORLD_W_TILES)) return;
  int16_t row=(sprite->y+sprite->h-(TILE_H_MM>>1))/TILE_H_MM;
  if ((row<0)||(row>=WORLD_H_TILES)) return;
  if (grid[row*WORLD_W_TILES+col]!=0x00) return;
  
  // In any other game, we'd have to check for headroom, but in this one there are no ceilings.
  grid[row*WORLD_W_TILES+col]=tileid;
  SPRITE->carrying=CARRYING_NONE;
  sprite->y-=TILE_H_MM;
  thumbnail_draw();//TODO consider incremental redraw; only one pixel could have changed
}

/* After horz movement, vert movement, and tattles, check for other occasional actions.
 */
 
static void ivan_check_actions(struct sprite *sprite) {

  // Pick up shovel?
  if ((SPRITE->dyimpulse<0)&&!SPRITE->carrying) {
    if (ivan_pickup_shovel(sprite)) return;
  }
  
  // UP while carrying an empty shovel: drop it. Note that you can't drop a loaded shovel.
  if ((SPRITE->carrying==CARRYING_SHOVEL)&&(SPRITE->dyimpulse<0)) {
    ivan_drop_shovel(sprite);
    return;
  }
  
  // B with the shovel armed to dig or deposit.
  if (SPRITE->inaux) switch (SPRITE->carrying) {
    case CARRYING_SHOVEL: ivan_dig(sprite); return;
    case CARRYING_SHOVEL_FULL: ivan_deposit(sprite); return;
  }
  
  // DOWN to pick up or drop tile objects.
  if (SPRITE->dyimpulse>0) switch (SPRITE->carrying) {
    case CARRYING_NONE: ivan_pickup(sprite); return;
    case CARRYING_BRICK: ivan_drop(sprite); return;
    case CARRYING_STATUE: ivan_drop(sprite); return;
    case CARRYING_BARREL: ivan_drop(sprite); return;
  }

}

/* If we are at (x,y), and trapped in a hole with walls at (lx) and (rx), is the shovel reachable?
 */
 
static uint8_t shovel_is_reachable(struct sprite *sprite,int16_t x,int16_t y,int16_t lx,int16_t rx) {

  // First, best case scenario: If we're holding the shovel, it is reachable.
  if (SPRITE->carrying==CARRYING_SHOVEL) return 1;
  if (SPRITE->carrying==CARRYING_SHOVEL_FULL) return 1;
  
  struct sprite *shovel=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;shovel++) {
  
    if (shovel->controller!=SPRITE_CONTROLLER_SHOVEL) continue;
  
    int16_t sx=(shovel->x+(shovel->w>>1))/TILE_W_MM;
    if (sx>=WORLD_W_TILES) sx-=WORLD_W_TILES;
    if ((sx<0)||(sx>=WORLD_W_TILES)) continue;
    int16_t sy=(shovel->y+(shovel->h>>1))/TILE_H_MM;
    if ((sy<0)||(sy>=WORLD_H_TILES)) continue;
    
    if (lx<rx) {
      if (sx<=lx) continue;
      if (sx>=rx) continue;
    } else {
      if (sx>=lx) continue;
      if (sx<=rx) continue;
    }
    
    if (grid[sy*WORLD_W_TILES+sx]>=0x10) {
      // Shovel is buried. Oh Ivan what have you done?
      continue;
    }
    if ((sy<WORLD_H_TILES-1)&&(grid[(sy+1)*WORLD_W_TILES+sx]<0x10)) {
      // There is air under the shovel. It might still be reachable by jumping, but let's stop there.
      continue;
    }
    return 1;
  }
  return 0;
}

/* How many solid cells present in column (x) beginning at row (y) and proceeding upward?
 */
 
static int16_t get_elevation(int16_t x,int16_t y) {
  if (x<0) return 0;
  if (y<0) return 0;
  if (x>=WORLD_W_TILES) return 0;
  if (y>=WORLD_H_TILES) y=WORLD_H_TILES-1;
  const uint8_t *v=grid+y*WORLD_W_TILES+x;
  int16_t elevation=0;
  while (y>=0) {
    if (*v<0x10) break; // not solid
    elevation++;
    y--;
    v-=WORLD_W_TILES;
  }
  return elevation;
}

/* How many dirts between (lx) and (rx) exclusive?
 */
 
static int16_t count_dirt_tiles(int16_t lx,int16_t rx) {
  if (lx>rx) {
    return count_dirt_tiles(-1,rx)+count_dirt_tiles(lx+1,WORLD_W_TILES);
  }
  lx+=1;
  rx-=1;
  if (lx<0) lx=0;
  if (rx>=WORLD_W_TILES) rx=WORLD_W_TILES-1;
  int16_t dirtc=0;
  while (lx<=rx) {
    const uint8_t *v=grid+lx;
    uint8_t yi=WORLD_H_TILES;
    for (;yi-->0;v+=WORLD_W_TILES) if ((*v>=0x20)&&(*v<=0x2f)) dirtc++;
    lx++;
  }
  return dirtc;
}

/* Check whether I am trapped, ie should we summon a fairy?
 * "Trapped" means one of:
 *   - There is a wall on each side of me more than 2 tiles high, and the shovel is unreachable.
 *   - Shovel is reachable, and the walls on each side of me are taller than the available dirt can pile.
 */
 
static uint8_t ivan_is_trapped(struct sprite *sprite) {
  
  // We're going to analyze based on the grid only, so pick the hero's position in grid space.
  int16_t x=(sprite->x+(sprite->w>>1))/TILE_W_MM;
  if (x>=WORLD_W_TILES) x-=WORLD_W_TILES;
  if ((x<0)||(x>=WORLD_W_TILES)) return 0;
  int16_t y=(sprite->y+(sprite->h>>1))/TILE_H_MM;
  if ((y<0)||(y>=WORLD_H_TILES)) return 0;
  
  // Walk each direction and find the ground level at each column.
  // If any single-pair slope exceeds 2, it's blocked.
  // If both directions are blocked at different places, we're trapped.
  int16_t lx=x-1;
  int16_t fatal_elevation=2,prev_elevation=0;;
  while (1) {
    if (lx<0) lx+=WORLD_W_TILES;
    if (lx==x) return 0; // circled the world; he's not trapped.
    int16_t ly=get_elevation(lx,y);
    if (ly>prev_elevation+2) break;
    lx--;
    prev_elevation=ly;
  }
  // And the same thing rightward...
  int16_t rx=x+1;
  fatal_elevation=2;
  prev_elevation=0;
  while (1) {
    if (rx>=WORLD_W_TILES) rx-=WORLD_W_TILES;
    if (rx==x) return 0;
    if (rx==lx) return 0; // a single pole somewhere is not fairy-worthy
    int16_t ry=get_elevation(rx,y);
    if (ry>prev_elevation+2) break;
    rx++;
    prev_elevation=ry;
  }
  
  // If the shovel is reachable, measure the shovelable dirt between lx and rx exclusive.
  if (shovel_is_reachable(sprite,x,y,lx,rx)) {
    int16_t dirtc=count_dirt_tiles(lx,rx);
    int16_t lelev=get_elevation(lx,y);//WORLD_H_TILES-1);
    int16_t relev=get_elevation(rx,y);//WORLD_H_TILES-1);
    int16_t elevation=(lelev>relev)?lelev:relev;
    int16_t w=(lx<rx)?(rx-lx-1):(rx+WORLD_W_TILES-lx-1);
    if ((dirtc*4>=w*elevation)&&(elevation<=w<<1)) {
      // This formula is not exact, it's a little forgiving.
      // But basically, if we have 1/4 of the enclosed area as dirt, and elevation is less than double width, you can dig out.
      return 0;
    }
  }
  
  return 1;
}

/* Is there a fairy guardmother in play already? We can't have two.
 */
 
static uint8_t fairy_exists() {
  const struct sprite *sprite=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;sprite++) {
    if (sprite->controller==SPRITE_CONTROLLER_FAIRY) return 1;
  }
  return 0;
}

/* Create the fairy guardmother if needed.
 * She appears when you are trapped and have left the controls idle for more than say 2 seconds.
 */
 
static void ivan_check_fairy(struct sprite *sprite) {

  // Any input resets the state.
  if (SPRITE->dx||SPRITE->dy||SPRITE->injump||SPRITE->inaux) {
    SPRITE->idleframec=0;
    SPRITE->fairy_triggered=0;
    return;
  }
  
  // Already been here? Must reset before reappearing.
  if (SPRITE->fairy_triggered) return;
  
  SPRITE->idleframec++;
  if (SPRITE->idleframec>=120) {
    SPRITE->idleframec=0;
    SPRITE->fairy_triggered=1;
    if (fairy_exists()) return;
    if (ivan_is_trapped(sprite)) {
      struct sprite *fairy=sprite_new();
      if (fairy) {
        fairy->controller=SPRITE_CONTROLLER_FAIRY;
        fairy->w=12*MM_PER_PIXEL;
        fairy->h=11*MM_PER_PIXEL;
        fairy->x=sprite->x-(CAMERA_W_MM>>1)-fairy->w;
        fairy->y=sprite->y-(CAMERA_W_MM>>2);
      }
    }
  }
}

/* Update.
 */
 
void sprite_update_ivan(struct sprite *sprite) {

  if (hp) {
    ivan_update_walk(sprite);
    ivan_update_jump(sprite);
    ivan_check_tattle(sprite);
    ivan_check_actions(sprite);
    ivan_check_fairy(sprite);
    if (SPRITE->injury_highlight) SPRITE->injury_highlight--;
  } else {
  }
  
  // Clear impulse inputs.
  SPRITE->dyimpulse=0;
  SPRITE->inaux=0;
}

/* Render.
 */
 
void sprite_render_ivan(struct sprite *sprite) {
  int16_t x,y;
  sprite_get_render_position(&x,&y,sprite);
  x+=1;
  
  // Draw the death sequence if appropriate, it's completely different.
  if (!hp) {
    SPRITE->animclock++;
    if (SPRITE->animclock>=4) {
      SPRITE->animclock=0;
      if (SPRITE->animframe<10) SPRITE->animframe++; // 11 frames
    }
    image_blit_colorkey(&fb,x-4,y,&fgbits,SPRITE->animframe*11,61,11,11);
    return;
  }
  
  uint8_t headframe=0,torsoframe=0,legframe=0;
  
  int16_t addy=0;
  if (SPRITE->injury_highlight&4) addy=33;
  
  // Animate legs if walking: 0..3
  if (SPRITE->dx) {
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
  
  // Torso animates like legs if not carrying anything, otherwise it has one fixed frame per carry type.
  switch (SPRITE->carrying) {
    case CARRYING_NONE: torsoframe=legframe; break;
    case CARRYING_SHOVEL:
    case CARRYING_SHOVEL_FULL: break; // 0
    case CARRYING_BRICK:
    case CARRYING_BARREL:
    case CARRYING_STATUE: torsoframe=4; headframe=1; break;
  }
  
  // Draw head, torso, and legs.
  if (SPRITE->facedir<0) {
    image_blit_colorkey_flop(&fb,x-2,y+4,&fgbits,22+torsoframe*9,addy,9,5);
    image_blit_colorkey_flop(&fb,x-1,y+8,&fgbits,legframe*7,9+addy,7,3);
    image_blit_colorkey_flop(&fb,x,y,&fgbits,headframe*5,addy,5,5);
  } else {
    image_blit_colorkey(&fb,x-2,y+4,&fgbits,22+torsoframe*9,addy,9,5);
    image_blit_colorkey(&fb,x-1,y+8,&fgbits,legframe*7,9+addy,7,3);
    image_blit_colorkey(&fb,x,y,&fgbits,headframe*5,addy,5,5);
  }
  
  // Draw the carry item, and forward arm if needed.
  uint8_t tileid;
  switch (SPRITE->carrying) {
  
    case CARRYING_SHOVEL:
    case CARRYING_SHOVEL_FULL: {
        int16_t dirtx;
        if (SPRITE->facedir<0) {
          image_blit_colorkey_flop(&fb,x-7,y+6,&fgbits,0,17+addy,14,3);
          dirtx=x-7;
        } else {
          image_blit_colorkey(&fb,x-1,y+6,&fgbits,0,17+addy,14,3);
          dirtx=x+8;
        }
        if (SPRITE->carrying==CARRYING_SHOVEL_FULL) {
          image_blit_colorkey(&fb,dirtx,y+3,&fgbits,17,addy,5,5);
        }
      } break;
    
    case CARRYING_BRICK: tileid=0x10; goto _overhead_;
    case CARRYING_BARREL: tileid=0x11; goto _overhead_;
    case CARRYING_STATUE: tileid=0x12; goto _overhead_;
    _overhead_: {
        int16_t dstx=(SPRITE->facedir<0)?(x-2):(x-1);
        if (tileid==0x34) { // special colorkey version of statue
          image_blit_colorkey(&fb,dstx,y-8,&fgbits,5,20,8,8);
        } else { // everything else is square
          image_blit_opaque(&fb,dstx,y-8,&bgtiles,(tileid&0x0f)*TILE_W_PIXELS,(tileid>>4)*TILE_H_PIXELS,8,8);
        }
        if (SPRITE->facedir<0) { // forward arm
          image_blit_colorkey_flop(&fb,x+2,y-1,&fgbits,30,28+addy,3,5);
        } else {
          image_blit_colorkey(&fb,x,y-1,&fgbits,30+(addy?3:0),28,3,5);
        }
      } break;
  }
}

/* Set injury highlight.
 */
 
void hero_highlight_injury(struct sprite *sprite) {
  SPRITE->injury_highlight=INJURY_TIME;
  if (!hp) {
    SPRITE->animclock=0;
    SPRITE->animframe=0;
  }
}

#include "game.h"
#include "world.h"
#include "data.h"
#include "platform.h"
#include <stdio.h>

#define WALK_SPEED MM_PER_PIXEL
#define WALK_FRAME_TIME 5
#define JUMP_POWER_MAX 12

#define CARRYING_NONE 0
#define CARRYING_SHOVEL 1
#define CARRYING_SHOVEL_FULL 2
#define CARRYING_BRICK 3
#define CARRYING_STATUE 4
#define CARRYING_BARREL 5

#define TATTLE_NONE 0
#define TATTLE_SHOVEL 1

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
  uint8_t tattle; // nonzero if some context-sensitive message is displayed
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

/* Create the hero sprite.
 */
 
struct sprite *game_hero_init() {
  struct sprite *sprite=spritev+0;//TODO maybe put him in the middle? this controls z index
  
  sprite->controller=SPRITE_CONTROLLER_IVAN;
  sprite->w=TILE_W_MM;
  sprite->h=11*MM_PER_PIXEL;
  int16_t herocol=WORLD_W_TILES>>1;
  int16_t herorow=WORLD_H_TILES>>1;
  sprite->x=herocol*TILE_W_MM+(TILE_W_MM>>1);
  sprite->y=herorow*TILE_H_MM-sprite->h;
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

// We are carrying somthing but a tattle is still up. Dismiss it.
static void ivan_dismiss_tattle(struct sprite *sprite) {
  switch (SPRITE->tattle) {
    case TATTLE_SHOVEL: {
        struct sprite *other=spritev;
        uint8_t i=SPRITE_LIMIT;
        for (;i-->0;other++) {
          if (other->controller!=SPRITE_CONTROLLER_SHOVEL) continue;
          other->opaque[1]=0;
          break;
        }
      } break;
  }
  SPRITE->tattle=TATTLE_NONE;
}

// We are not carrying anything; set the most appropriate tattle.
static void ivan_check_tattle(struct sprite *sprite) {
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
          SPRITE->tattle=TATTLE_SHOVEL; 
          other->opaque[1]=1;
        } return;
    }
  }
  if (SPRITE->tattle) ivan_dismiss_tattle(sprite);
}

/* Shovel actions.
 */
 
static void ivan_pickup_shovel(struct sprite *sprite) {
  if (SPRITE->carrying!=CARRYING_NONE) return; // will never happen but play it safe
  SPRITE->carrying=CARRYING_SHOVEL;
  SPRITE->tattle=TATTLE_NONE;
  
  // There can never be more than one shovel sprite. If that changes, update this:
  struct sprite *shovel=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;shovel++) {
    if (shovel->controller==SPRITE_CONTROLLER_SHOVEL) {
      shovel->controller=SPRITE_CONTROLLER_NONE;
      break;
    }
  }
}
 
static void ivan_drop_shovel(struct sprite *sprite) {
  
  if (SPRITE->carrying!=CARRYING_SHOVEL) return;
  if (!sprite_is_grounded(sprite)) return;
  
  SPRITE->carrying=CARRYING_NONE;
  // Tattle will take care of itself; leave unset.
  
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
    SPRITE->carrying=CARRYING_SHOVEL;
    sprite->y-=TILE_H_MM;
  }
}

/* Pickup and drop tile objects.
 */
 
static void ivan_pickup(struct sprite *sprite) {
  fprintf(stderr,"%s\n",__func__);//TODO
  //TODO examine grid
}
 
static void ivan_drop(struct sprite *sprite) {
  fprintf(stderr,"%s\n",__func__);//TODO
  //TODO update grid
  SPRITE->carrying=CARRYING_NONE;
}

/* After horz movement, vert movement, and tattles, check for other occasional actions.
 */
 
static void ivan_check_actions(struct sprite *sprite) {

  // We're tattling a shovel and he pressed up: pick it up. (implicitly CARRYING_NONE in this case)
  if ((SPRITE->tattle==TATTLE_SHOVEL)&&(SPRITE->dyimpulse<0)) {
    ivan_pickup_shovel(sprite);
    return;
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

/* Update.
 */
 
void sprite_update_ivan(struct sprite *sprite) {
  ivan_update_walk(sprite);
  ivan_update_jump(sprite);
  if (SPRITE->carrying==CARRYING_NONE) ivan_check_tattle(sprite);
  else if (SPRITE->tattle!=TATTLE_NONE) ivan_dismiss_tattle(sprite);
  ivan_check_actions(sprite);
  
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
  
  uint8_t headframe=0,torsoframe=0,legframe=0;
  
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
    default: break; // TODO overhead carry
  }
  
  // Draw head, torso, and legs.
  if (SPRITE->facedir<0) {
    image_blit_colorkey_flop(&fb,x-2,y+4,&fgbits,17+torsoframe*9,0,9,5);
    image_blit_colorkey_flop(&fb,x-1,y+8,&fgbits,legframe*7,9,7,3);
    image_blit_colorkey_flop(&fb,x,y,&fgbits,0,0,5,5);
  } else {
    image_blit_colorkey(&fb,x-2,y+4,&fgbits,17+torsoframe*9,0,9,5);
    image_blit_colorkey(&fb,x-1,y+8,&fgbits,legframe*7,9,7,3);
    image_blit_colorkey(&fb,x,y,&fgbits,0,0,5,5);
  }
  
  // Draw the carry item, and forward arm if needed.
  switch (SPRITE->carrying) {
    case CARRYING_SHOVEL:
    case CARRYING_SHOVEL_FULL: {
        int16_t dirtx;
        if (SPRITE->facedir<0) {
          image_blit_colorkey_flop(&fb,x-7,y+6,&fgbits,0,17,14,3);
          dirtx=x-7;
        } else {
          image_blit_colorkey(&fb,x-1,y+6,&fgbits,0,17,14,3);
          dirtx=x+8;
        }
        if (SPRITE->carrying==CARRYING_SHOVEL_FULL) {
          image_blit_colorkey(&fb,dirtx,y+3,&fgbits,12,0,5,5);
        }
      } break;
    //TODO overhead carry
  }
}

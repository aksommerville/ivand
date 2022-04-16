#include "game.h"
#include "world.h"
#include "data.h"
#include "platform.h"

#define WALK_SPEED MM_PER_PIXEL
#define WALK_FRAME_TIME 5
#define JUMP_POWER_MAX 12

/* Object definition.
 */
 
struct sprite_ivan {
  SPRITE_HEADER
  int8_t facedir; // -1,1 = left,right
  int8_t dx; // (-1,0,1) while button held
  int8_t dy; // ''
  uint8_t injump; // 1 while held
  uint8_t inaux; // 1 momentarily at press
  uint8_t animclock;
  uint8_t animframe;
  uint8_t jumppower;
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
  sprite->w=5*MM_PER_PIXEL;
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
  
  switch (input&(BUTTON_UP|BUTTON_DOWN)) {
    case BUTTON_UP: SPRITE->dy=-1; break;
    case BUTTON_DOWN: SPRITE->dy=1; break;
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
  if (SPRITE->dx) {
    sprite_move_horz(sprite,SPRITE->dx*WALK_SPEED);
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
    }
  }
}

/* Update.
 */
 
void sprite_update_ivan(struct sprite *sprite) {
  ivan_update_walk(sprite);
  ivan_update_jump(sprite);

  //TODO gravity
}

/* Render.
 */
 
void sprite_render_ivan(struct sprite *sprite) {
  // Careful here, it's floor division, so we want to first scale everything to pixels, then translate.
  int16_t x=sprite->x/MM_PER_PIXEL-camera.x/MM_PER_PIXEL;
  if (x<0) x+=WORLD_W_PIXELS;
  int16_t y=sprite->y/MM_PER_PIXEL-camera.y/MM_PER_PIXEL;
  
  // body, walking animation.
  uint8_t bodyframe=0;
  if (SPRITE->dx) {
    if (SPRITE->animclock>0) {
      SPRITE->animclock--;
    } else {
      SPRITE->animclock=WALK_FRAME_TIME;
      SPRITE->animframe++;
      if (SPRITE->animframe>=4) SPRITE->animframe=0;
    }
    bodyframe=SPRITE->animframe;
  } else {
    SPRITE->animclock=0;
    SPRITE->animframe=0;
  }
  
  //TODO shovel
  //TODO carrying item
  
  if (SPRITE->facedir<0) {
    image_blit_colorkey_flop(&fb,x-2,y+4,&fgbits,bodyframe*9,5,9,7);
    image_blit_colorkey_flop(&fb,x,y,&fgbits,0,0,5,5);
  } else {
    image_blit_colorkey(&fb,x-2,y+4,&fgbits,bodyframe*9,5,9,7);
    image_blit_colorkey(&fb,x,y,&fgbits,0,0,5,5);
  }
}

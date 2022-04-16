#include "game.h"
#include "platform.h"
#include "data.h"
#include "synth.h"
#include "world.h"
#include <string.h>
#include <stdio.h>

/* Globals.
 */

/* End.
 */
 
void game_end() {
  //TODO
}

/* Begin.
 */
 
void game_begin() {
  
  grid_default();
  //TODO init, setup
  
  memset(spritev,0,sizeof(spritev));
  game_hero_init();
}

/* Receive input.
 */
 
void game_input(uint8_t input,uint8_t pvinput) {
  struct sprite *sprite=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;sprite++) switch (sprite->controller) {
    case SPRITE_CONTROLLER_IVAN: sprite_input_ivan(sprite,input,pvinput); break;
  }
}

/* Update.
 */
 
void game_update() {
  struct sprite *sprite=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;sprite++) switch (sprite->controller) {
    case SPRITE_CONTROLLER_IVAN: sprite_update_ivan(sprite); break;
    case SPRITE_CONTROLLER_GUARD: sprite_update_guard(sprite); break;
  }
  //TODO master clock
  //TODO other global game update stuff?
}

/* Render scene.
 */
 
void game_render() {

  // Background grid, overwrites entire framebuffer.
  struct sprite *hero=spritev+0;
  camera_update(hero);
  int16_t camright=camera.x+camera.w;
  int16_t cambottom=camera.y+camera.h;
  if (camright>WORLD_W_MM) {
    int16_t leftwmm=WORLD_W_MM-camera.x;
    int16_t leftwpx=(leftwmm+MM_PER_PIXEL-1)/MM_PER_PIXEL;
    grid_render(&fb,0,0,camera.x,camera.y,leftwmm,camera.h);
    grid_render(&fb,leftwpx,0,0,camera.y,CAMERA_W_MM-leftwmm,camera.h);
  } else {
    grid_render(&fb,0,0,camera.x,camera.y,camera.w,camera.h);
  }
  
  // Sprites.
  struct sprite *sprite=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;sprite++) {
  
    // Skip fast if it won't draw anything.
    //TODO (x,y,w,h) are *physical* bounds. Need to check *visible* bounds here instead.
    if (sprite->controller==SPRITE_CONTROLLER_NONE) continue;
    if (sprite->y>=cambottom) continue;
    if (sprite->y+sprite->h<=camera.y) continue;
    if (sprite->x>=camright) continue;
    if (camright>WORLD_W_MM) { // could be in the natural or wrapped slice
      if ((sprite->x>=camera.x+camera.w-WORLD_W_MM)&&(sprite->x+sprite->w<=camera.x)) continue;
    } else { // single slice, easy
      if (sprite->x+sprite->w<=camera.x) continue;
    }
    
    switch (sprite->controller) {
      case SPRITE_CONTROLLER_IVAN: sprite_render_ivan(sprite); break;
      case SPRITE_CONTROLLER_DUMMY: sprite_render_dummy(sprite); break;
      case SPRITE_CONTROLLER_GUARD: sprite_render_guard(sprite); break;
    }
  }
  
  // Overlay TODO
}

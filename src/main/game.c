#include "game.h"
#include "platform.h"
#include "data.h"
#include "synth.h"
#include "world.h"
#include <string.h>
#include <stdio.h>

#define FADE_OUT_TIME (60*5)

/* Globals.
 */
 
static uint8_t tattle=TATTLE_NONE;
static int16_t tattlex,tattley;
uint32_t gameclock;
uint8_t hp;

/* End.
 */
 
void game_end() {
  //TODO
}

/* Begin.
 */
 
void game_begin() {
  
  grid_default();
  thumbnail_draw();
  gameclock=GAME_DURATION_FRAMES;
  hp=HP_MAX;
  
  memset(spritev,0,sizeof(spritev));
  struct sprite *sprite;
  
  // Ivan.
  if (sprite=sprite_new()) {
    sprite->controller=SPRITE_CONTROLLER_IVAN;
    sprite->w=TILE_W_MM;
    sprite->h=11*MM_PER_PIXEL;
    int16_t herocol=WORLD_W_TILES>>1;
    int16_t herorow=WORLD_H_TILES>>1;
    sprite->x=herocol*TILE_W_MM;
    sprite->y=herorow*TILE_H_MM-sprite->h;
  }
  
  // The Shovel.
  if (sprite=sprite_new()) {
    sprite->controller=SPRITE_CONTROLLER_SHOVEL;
    sprite->w=13*MM_PER_PIXEL;
    sprite->h=5*MM_PER_PIXEL;
    sprite->x=35*TILE_W_MM;
    sprite->y=16*TILE_H_MM-sprite->h-(2*MM_PER_PIXEL);
  }
  
  // The Guard.
  if (sprite=sprite_new()) {
    sprite->controller=SPRITE_CONTROLLER_GUARD;
    sprite->w=5*MM_PER_PIXEL;
    sprite->h=11*MM_PER_PIXEL;
    sprite->x=40*TILE_W_MM;
    sprite->y=(WORLD_H_TILES>>1)*TILE_H_MM-sprite->h;
  }
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

  tattle=TATTLE_NONE;
  struct sprite *sprite=spritev;
  uint8_t i=SPRITE_LIMIT;
  for (;i-->0;sprite++) switch (sprite->controller) {
    case SPRITE_CONTROLLER_IVAN: sprite_update_ivan(sprite); break;
    case SPRITE_CONTROLLER_GUARD: sprite_update_guard(sprite); break;
    case SPRITE_CONTROLLER_BULLET: sprite_update_bullet(sprite); break;
  }
  
  if (gameclock) gameclock--;
  //TODO master clock triggers
  
  //TODO other global game update stuff?
}

/* Request tattle.
 */
 
void set_tattle(int16_t x,int16_t y,uint8_t reqtattle) {
  
  // Tattle IDs are arranged such that higher is more significant.
  if (reqtattle<=tattle) return;
  
  tattle=reqtattle;
  tattlex=x/MM_PER_PIXEL;
  tattley=y/MM_PER_PIXEL;
}

/* Injure the hero (eg by bullet).
 */
 
void injure_hero(struct sprite *sprite) {
  if (!hp) return;
  if (!sprite) sprite=game_get_hero();
  if (hp>1) {
    hp--;
    //TODO sound effect
  } else {
    hp=0;
    if (gameclock>250) gameclock=250;
  }
  if (sprite) hero_highlight_injury(sprite);
}

/* Render dialogue bubble.
 */
 
void render_dialogue_bubble(int16_t x,int16_t y,int16_t w,int16_t h,int16_t focusx) {
  
  // Corners.
  image_blit_colorkey(&fb,x,y,&fgbits,0,20,2,2);
  image_blit_colorkey(&fb,x+w-2,y,&fgbits,3,20,2,2);
  image_blit_colorkey(&fb,x,y+h-4,&fgbits,0,23,2,2);
  image_blit_colorkey(&fb,x+w-2,y+h-4,&fgbits,3,23,2,2);
  
  // Edges and middle (fill, not blit, we read one pixel from the image for each region).
  #define C(x,y) fgbits.v[y*fgbits.stride+x]
  image_fill_rect(&fb,x+2,y,w-4,1,C(2,20));
  image_fill_rect(&fb,x+2,y+1,w-4,1,C(2,21));
  image_fill_rect(&fb,x,y+2,1,h-6,C(0,22));
  image_fill_rect(&fb,x+1,y+2,1,h-6,C(1,22));
  image_fill_rect(&fb,x+w-1,y+2,1,h-6,C(4,22));
  image_fill_rect(&fb,x+w-2,y+2,1,h-6,C(3,22));
  image_fill_rect(&fb,x+2,y+h-3,w-4,1,C(2,24));
  image_fill_rect(&fb,x+2,y+h-4,w-4,1,C(2,23));
  image_fill_rect(&fb,x+2,y+2,w-4,h-6,C(2,22));
  #undef C
  
  // Focus. It's 3 pixels high, not 2, the top row overwrites the bubble's bottom edge.
  image_blit_colorkey(&fb,focusx-1,y+h-3,&fgbits,1,25,3,3);
  
}

static void render_tattle() {
  switch (tattle) {

    case TATTLE_SHOVEL: {
        int16_t dstw=40;
        int16_t dsth=14;
        int16_t dstx=tattlex-(dstw>>1)-camera.x/MM_PER_PIXEL;
        int16_t dsty=tattley-dsth-camera.y/MM_PER_PIXEL;
        render_dialogue_bubble(dstx,dsty,dstw,dsth,dstx+(dstw>>1));
        image_blit_colorkey(&fb,dstx+3,dsty+3,&fgbits,15,28,5,5);
        image_blit_string(&fb,dstx+11,dsty+2,"Pick up",7,0x0000,font);
      } break;
      
    case TATTLE_PICKUP: {
        int16_t dstw=40;
        int16_t dsth=14;
        int16_t dstx=tattlex-(dstw>>1)-camera.x/MM_PER_PIXEL;
        int16_t dsty=tattley-dsth-camera.y/MM_PER_PIXEL;
        if (
          (camera.x+camera.w>WORLD_W_MM)&&
          (dstx+dstw<=0)
        ) dstx+=WORLD_W_PIXELS;
        render_dialogue_bubble(dstx,dsty,dstw,dsth,dstx+(dstw>>1));
        image_blit_colorkey(&fb,dstx+3,dsty+3,&fgbits,10,28,5,5);
        image_blit_string(&fb,dstx+11,dsty+2,"Pick up",7,0x0000,font);
      } break;
      
    case TATTLE_TRUCK: {//TODO spacing, verbiage
        int16_t dstw=24;
        int16_t dsth=13;
        int16_t dstx=tattlex-(dstw>>1)-camera.x/MM_PER_PIXEL;
        int16_t dsty=tattley-dsth-camera.y/MM_PER_PIXEL;
        render_dialogue_bubble(dstx,dsty,dstw,dsth,dstx+(dstw>>1));
        image_blit_string(&fb,dstx+3,dsty+2,"Truck",5,0x0000,font);
      } break;
      
    case TATTLE_HOLE: {//TODO spacing, verbiage
        int16_t dstw=40;
        int16_t dsth=14;
        int16_t dstx=tattlex-(dstw>>1)-camera.x/MM_PER_PIXEL;
        int16_t dsty=tattley-dsth-camera.y/MM_PER_PIXEL;
        render_dialogue_bubble(dstx,dsty,dstw,dsth,dstx+(dstw>>1));
        image_blit_string(&fb,dstx,dsty,"Dig hole",8,0x0000,font);
      } break;
      
    case TATTLE_WALL: {//TODO spacing, verbiage
        int16_t dstw=40;
        int16_t dsth=14;
        int16_t dstx=tattlex-(dstw>>1)-camera.x/MM_PER_PIXEL;
        int16_t dsty=tattley-dsth-camera.y/MM_PER_PIXEL;
        render_dialogue_bubble(dstx,dsty,dstw,dsth,dstx+(dstw>>1));
        image_blit_string(&fb,dstx,dsty,"Build wall",10,0x0000,font);
      } break;
      
    case TATTLE_STATUE: {
        int16_t dstw=40;
        int16_t dsth=14;
        int16_t dstx=tattlex-(dstw>>1)-camera.x/MM_PER_PIXEL;
        int16_t dsty=tattley-dsth-camera.y/MM_PER_PIXEL;
        render_dialogue_bubble(dstx,dsty,dstw,dsth,dstx+(dstw>>1));
        image_blit_colorkey(&fb,dstx+2,dsty+2,&fgbits,5,20,8,8);
        image_blit_string(&fb,dstx+12,dsty+2,"on top",6,0x0000,font);
      } break;
      
    case TATTLE_BARREL: {//TODO spacing, verbiage
        int16_t dstw=40;
        int16_t dsth=14;
        int16_t dstx=tattlex-(dstw>>1)-camera.x/MM_PER_PIXEL;
        int16_t dsty=tattley-dsth-camera.y/MM_PER_PIXEL;
        render_dialogue_bubble(dstx,dsty,dstw,dsth,dstx+(dstw>>1));
        image_blit_string(&fb,dstx,dsty,"Bury barrel",11,0x0000,font);
      } break;
  }
}

/* Render thumbnail ornaments.
 */
 
static void game_render_thumbnail_ornaments() {
  const int16_t x0=fb.w-thumbnail.w+1;
  const int16_t y0=1;
  const int16_t w0=THUMBNAIL_W-2;
  const int16_t h0=THUMBNAIL_H-2;
  int16_t x,y;
  struct sprite *sprite;
  
  if (sprite=game_get_hero()) {
    // phrase first in pixels, it would overflow if we kept in mm to the end
    x=(sprite->x+(sprite->w>>1))/MM_PER_PIXEL;
    y=(sprite->y+(sprite->h>>1))/MM_PER_PIXEL;
    if (x>=WORLD_W_PIXELS) x-=WORLD_W_PIXELS;
    x=(x*w0)/WORLD_W_PIXELS;
    y=(y*h0)/WORLD_H_PIXELS;
    x+=x0;
    y+=y0;
    const uint16_t color=(framec&0x10)?0xffff:0x0000;
    fb.v[y*fb.stride+x]=color;
  }
}

/* Render the clock.
 */
 
static void game_render_clock() {
  uint32_t sec=gameclock/60;
  uint32_t min=sec/60;
  sec%=60;
  char text[32];
  int32_t textc=snprintf(text,sizeof(text),"%0d:%02d",min,sec);
  if (textc>0) {
    image_blit_string(&fb,1,1,text,textc,0x0000,font);
  }
}

/* Render hit points.
 */
 
static void game_render_hp() {
  int16_t x=18;
  uint8_t i=0;
  for (;i<HP_MAX;i++,x+=6) {
    int16_t srcx=(i<hp)?7:2;
    image_blit_colorkey(&fb,x,1,&fgbits,srcx,5,5,4);
  }
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
      case SPRITE_CONTROLLER_SHOVEL: sprite_render_shovel(sprite); break;
      case SPRITE_CONTROLLER_BULLET: sprite_render_bullet(sprite); break;
    }
  }
  
  // Thumbnail.
  image_blit_opaque(&fb,fb.w-thumbnail.w,0,&thumbnail,0,0,thumbnail.w,thumbnail.h);
  game_render_thumbnail_ornaments();
  
  // Clock.
  if (hp) game_render_clock();
  
  // HP.
  game_render_hp();
  
  // Tattle.
  render_tattle();
  
  // Fade out near the end.
  if (gameclock<FADE_OUT_TIME) {
    int8_t fade=20-(gameclock*20)/FADE_OUT_TIME;
    if (fade>0) {
      uint16_t *p=fb.v;
      uint16_t fi=fb.w*fb.h;
      for (;fi-->0;p++) {
        uint8_t y=((*p)>>8)&0x1f;
        if (y>fade) y-=fade;
        else y=0;
        *p=(y<<8)|(y<<3)|(y>>2)|(y<<13);
      }
    }
  }
}

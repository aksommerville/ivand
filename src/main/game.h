/* game.h
 * Game logic and rendering here.
 * There will be some outer menu, that's your problem.
 * You must start with game_begin(), end with game_end().
 * All other game_* calls can only come between those. Obviously...
 */
 
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

struct image;
struct synth;
struct sprite;

extern struct image fb;
extern struct synth synth;
extern uint32_t framec;

#define DIR_N    0x01
#define DIR_W    0x02
#define DIR_E    0x04
#define DIR_S    0x08

void game_end();

void game_begin();

void game_input(uint8_t input,uint8_t pvinput);
void game_update();
void game_render();

struct sprite *sprite_new();
uint8_t sprite_is_grounded(const struct sprite *sprite);
int16_t sprite_move_horz(struct sprite *sprite,int16_t dx); // => actual movement
void sprite_get_render_position(int16_t *x,int16_t *y,const struct sprite *sprite);

struct sprite *game_get_hero();
struct sprite *game_hero_init();

void sprite_input_ivan(struct sprite *sprite,uint8_t input,uint8_t pvinput);
void sprite_update_ivan(struct sprite *sprite);
void sprite_update_guard(struct sprite *sprite);
void sprite_render_ivan(struct sprite *sprite);
void sprite_render_dummy(struct sprite *sprite);
void sprite_render_guard(struct sprite *sprite);
void sprite_render_shovel(struct sprite *sprite);

/* Renders a bubble with a downward indicator, you can render text onto it.
 * (x,y,w,h,focusx) all in framebuffer pixels.
 * (w>=4) (h>=6), (focusx) in (x+2..x+w-3).
 * The lowest 2 rows are used by the indicator.
 * Leave a 2-pixel margin around your content. (4 pixels at the bottom)
 */
void render_dialogue_bubble(int16_t x,int16_t y,int16_t w,int16_t h,int16_t focusx);

#endif

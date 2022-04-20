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

#define TATTLE_NONE 0
#define TATTLE_SHOVEL 1
#define TATTLE_PICKUP 2
#define TATTLE_TRUCK 3
#define TATTLE_HOLE 4
#define TATTLE_WALL 5
#define TATTLE_STATUE 6
#define TATTLE_BARREL 7

void game_end();

void game_begin();

void game_input(uint8_t input,uint8_t pvinput);
void game_update();
void game_render();

struct sprite *sprite_new();
uint8_t sprite_is_grounded(const struct sprite *sprite);
int16_t sprite_move_horz(struct sprite *sprite,int16_t dx); // => actual movement
int16_t sprite_move_vert(struct sprite *sprite,int16_t dy); // => actual movement
void sprite_get_render_position(int16_t *x,int16_t *y,const struct sprite *sprite);

struct sprite *game_get_hero();

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

/* Request that a tattle be rendered on the top, at world position (x,y)mm.
 * You have to re-request it every frame.
 */
void set_tattle(int16_t x,int16_t y,uint8_t tattle);

#endif

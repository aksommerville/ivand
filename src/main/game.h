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

void game_end();

void game_begin();

void game_input(uint8_t input,uint8_t pvinput);
void game_update();
void game_render();

uint8_t sprite_is_grounded(const struct sprite *sprite);
int16_t sprite_move_horz(struct sprite *sprite,int16_t dx); // => actual movement

struct sprite *game_get_hero();
struct sprite *game_hero_init();

void sprite_input_ivan(struct sprite *sprite,uint8_t input,uint8_t pvinput);
void sprite_update_ivan(struct sprite *sprite);
void sprite_update_guard(struct sprite *sprite);
void sprite_render_ivan(struct sprite *sprite);
void sprite_render_dummy(struct sprite *sprite);
void sprite_render_guard(struct sprite *sprite);

#endif

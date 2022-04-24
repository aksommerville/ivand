/* menu.h
 * Everything around the intro splash (song select).
 */
 
#ifndef MENU_H
#define MENU_H

#include <stdint.h>

struct image;
struct synth;

extern struct image fb;
extern struct synth synth;

void menu_end();

void menu_begin(uint8_t initial);

void menu_input(uint8_t input,uint8_t pvinput);
uint8_t menu_update();
void menu_render();

// menu_update() returns one of these:
#define MENU_UPDATE_CONTINUE 0 /* Stay in the menu. */
#define MENU_UPDATE_GAME     1 /* Begin the game. */

#endif

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

void menu_begin();

void menu_input(uint8_t input,uint8_t pvinput);
void menu_update();
void menu_render();

#endif

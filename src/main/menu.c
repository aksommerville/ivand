#include "menu.h"
#include "data.h"
#include "synth.h"
#include <string.h>
#include <stdio.h>

/* Globals.
 */

/* Quit.
 */
 
void menu_end() {
  //TODO
}

/* Init.
 */
 
void menu_begin() {
  //TODO
}

/* Input.
 */

void menu_input(uint8_t input,uint8_t pvinput) {
  #define PRESS(tag) ((input&BUTTON_##tag)&&!(pvinput&BUTTON_##tag))
  //TODO
  #undef PRESS
}

/* Update.
 */
 
void menu_update() {
}

/* Render.
 */
 
void menu_render() {
  memset(fb.v,0,fb.w*fb.h*2);
}

#include "menu.h"
#include "data.h"
#include "synth.h"
#include <string.h>
#include <stdio.h>

#define BLACKOUT_TIME_FRAMES 60

/* Globals.
 */
 
static uint8_t videodirty;
static uint8_t nextstate;
static uint8_t blackout;

/* Quit.
 */
 
void menu_end() {
  //TODO
}

/* Init.
 */
 
void menu_begin() {
  videodirty=1;
  nextstate=MENU_UPDATE_CONTINUE;
  blackout=0;//BLACKOUT_TIME_FRAMES; TODO blackout if not the first visit (we are both "hello" and "game over")
}

/* Input.
 */

void menu_input(uint8_t input,uint8_t pvinput) {
  if (blackout) return;
  #define PRESS(tag) ((input&BUTTON_##tag)&&!(pvinput&BUTTON_##tag))
  if (PRESS(A)||PRESS(B)) nextstate=MENU_UPDATE_GAME;
  #undef PRESS
}

/* Update.
 */
 
uint8_t menu_update() {
  if (blackout) {
    blackout--;
    if (!blackout) videodirty=1;
  }
  return nextstate;
}

/* Render.
 */
 
void menu_render() {

  //TODO Alternate view for after a round has been played. Report score.

  // The framebuffer is persistent. No need to redraw it if nothing changed.
  if (!videodirty) return;
  
  memset(fb.v,0,fb.w*fb.h*2);
  
  image_blit_string(&fb,4,0,"One Day in the Life of",22,0xffff,font);
  image_blit_string(&fb,4,9,"Ivan Denisovich",-15,0xffff,font);
  
  image_blit_string(&fb,4,27,"Story: AI Solzhenitsyn",22,0x18c6,font);
  image_blit_string(&fb,4,36,"Game: AK Sommerville",20,0x18c6,font);
  
  if (!blackout) {
    image_blit_string(&fb,4,54,"Press button to begin",21,0x1084,font);
  }
  
  videodirty=0;
}

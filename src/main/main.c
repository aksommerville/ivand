#include "platform.h"
#include "synth.h"
#include "data.h"
#include "menu.h"
#include "game.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Globals.
 */

static uint16_t fbstorage[96*64];
struct image fb={
  .v=fbstorage,
  .w=96,
  .h=64,
  .stride=96,
};

#define MAINSTATE_INIT 0
#define MAINSTATE_GAME 1
#define MAINSTATE_MENU 2
static uint8_t mainstate=MAINSTATE_INIT;

static uint8_t input=0;
static uint8_t pvinput=0;
static uint32_t framec=0;

struct synth synth={0};

/* Synthesizer.
 */

int16_t audio_next() {
  return synth_update(&synth);
}
 
void loop() {
  framec++;

  input=platform_update();
  if (input!=pvinput) {
    switch (mainstate) {
      case MAINSTATE_GAME: game_input(input,pvinput); break;
      case MAINSTATE_MENU: menu_input(input,pvinput); break;
    }
    pvinput=input;
  }
  
  switch (mainstate) {
    case MAINSTATE_GAME: game_update(); game_render(); break;
    case MAINSTATE_MENU: menu_update(); menu_render(); break;
    default: memset(fb.v,0,fb.w*fb.h*2);
  }
  
  platform_send_framebuffer(fb.v);
}

/* Init.
 */

void setup() {
  platform_init();
  
  synth.wavev[0]=wave0;
  synth.wavev[1]=wave1;
  synth.wavev[2]=wave2;
  synth.wavev[3]=wave3;
  synth.wavev[4]=wave4;
  synth.wavev[5]=wave5;
  synth.wavev[6]=wave6;
  synth.wavev[7]=wave7;
  
  //TODO Starting game immediately during initial work. Eventually should have a menu here.
  game_begin(&fb,&synth);
  mainstate=MAINSTATE_GAME;
}

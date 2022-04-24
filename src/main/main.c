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
uint32_t framec=0;

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
  
    case MAINSTATE_GAME: {
        game_update();
        game_render();
        if (!gameclock) {
          mainstate=MAINSTATE_MENU;
          game_end();
          menu_begin(0);
        }
      } break;
      
    case MAINSTATE_MENU: {
        uint8_t outcome=menu_update();
        menu_render();
        switch (outcome) {
          case MENU_UPDATE_GAME: {
              mainstate=MAINSTATE_GAME;
              menu_end();
              game_begin();
            } break;
        }
      } break;
  
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
  
  menu_begin(1);
  mainstate=MAINSTATE_MENU;
}

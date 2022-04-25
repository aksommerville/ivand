#include "menu.h"
#include "data.h"
#include "synth.h"
#include "world.h"
#include "game.h"
#include "highscore.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define BLACKOUT_TIME_FRAMES 120

/* Globals.
 */
 
static uint8_t videodirty;
static uint8_t nextstate;
static uint8_t blackout;
static uint8_t menu_initial;

static uint8_t report[16];
static const char *validation_message;

/* Quit.
 */
 
void menu_end() {
}

/* Generate report.
 */
 
static void generate_report() {

  uint32_t elevation=get_elevation_score();
  uint32_t depth=get_depth_score();
  const char *error=get_validation_message();
  uint32_t validation=error?0:1;
  uint32_t activity=99;
  if (framec) {
    // Technically must be in 0..99, but that's too broad so I'm actually setting a floor of 50.
    activity=50+(activity_framec*50)/framec;
    if (activity<0) activity=0;
    else if (activity>99) activity=99;
  }
  // The constants 37 and 89 were selected to make all scores <10k, and most realistic scores >1k; i want 4 digits
  uint32_t score=validation?(((elevation*depth*37+hp*89)*activity)/100):0;
  if (score>9999) score=9999; // pretty sure that's unreachable but let's be certain
  
  uint32_t hiscore=highscore_get();
  if (score>hiscore) {
    highscore_set(score);
    highscore_send(score);
    validation_message="** New high score! **";
    hiscore=score;
  } else {
    validation_message=error;
  }
  
  fprintf(stderr,
    "SCORE: elv=%02d dep=%02d hp=%d act=%02d val=%d ==> %04d\n",
    elevation,depth,hp,activity,validation,score
  );
  
  report[ 0]=36+(elevation/10)*4;
  report[ 1]=36+(elevation%10)*4;
  report[ 2]=36+(depth/10)*4;
  report[ 3]=36+(depth%10)*4;
  report[ 4]=36+hp*4;
  report[ 5]=36+(activity/10)*4;
  report[ 6]=36+(activity%10)*4;
  report[ 7]=36+validation*4;
  report[ 8]=36+(score/1000)*4;
  report[ 9]=36+((score/100)%10)*4;
  report[10]=36+((score/10)%10)*4;
  report[11]=36+(score%10)*4;
  report[12]=36+(hiscore/1000)*4;
  report[13]=36+((hiscore/100)%10)*4;
  report[14]=36+((hiscore/10)%10)*4;
  report[15]=36+(hiscore%10)*4;
}


/* Init.
 */
 
void menu_begin(uint8_t initial) {
  videodirty=1;
  nextstate=MENU_UPDATE_CONTINUE;
  
  if (menu_initial=initial) {
    blackout=0;
    
  } else {
    blackout=BLACKOUT_TIME_FRAMES;
    generate_report();
  }
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
 
static void draw_digit(int16_t dstx,int16_t dsty,uint8_t srcx) {
  image_blit_opaque(&fb,dstx,dsty,&fgbits,srcx,78,4,5);
}
 
void menu_render() {

  // The framebuffer is persistent. No need to redraw it if nothing changed.
  if (!videodirty) return;
  
  if (menu_initial) {
    memset(fb.v,0,fb.w*fb.h*2);
    image_blit_string(&fb,4,0,"One Day in the Life of",22,0xffff,font);
    image_blit_string(&fb,4,9,"Ivan Denisovich",-15,0xffff,font);
    image_blit_string(&fb,4,27,"Story: AI Solzhenitsyn",22,0x18c6,font);
    image_blit_string(&fb,4,36,"Game: AK Sommerville",20,0x18c6,font);
    if (!blackout) {
      image_blit_string(&fb,4,54,"Press button to begin",21,0x1084,font);
    }
    
  } else {
  
    image_fill_rect(&fb,0,0,96,64,0xd7bd);
    image_blit_opaque(&fb,0, 5,&fgbits,0,83,96,9);
    image_blit_opaque(&fb,0,19,&fgbits,0,92,96,9);
    image_blit_opaque(&fb,0,33,&fgbits,0,101,96,9);
    
    draw_digit( 8, 7,report[ 0]);
    draw_digit(13, 7,report[ 1]);
    draw_digit(28, 7,report[ 2]);
    draw_digit(33, 7,report[ 3]);
    draw_digit(48, 7,report[ 4]);
    draw_digit(69, 7,report[ 5]);
    draw_digit(74, 7,report[ 6]);
    draw_digit(89, 7,report[ 7]);
    draw_digit(74,21,report[ 8]);
    draw_digit(79,21,report[ 9]);
    draw_digit(84,21,report[10]);
    draw_digit(89,21,report[11]);
    draw_digit(74,35,report[12]);
    draw_digit(79,35,report[13]);
    draw_digit(84,35,report[14]);
    draw_digit(89,35,report[15]);
    
    image_blit_string(&fb,4,45,validation_message,-1,0x0000,font);
    if (!blackout) {
      image_blit_string(&fb,2,55,"Press button, play again",24,0x0000,font);
    }
  }
  
  videodirty=0;
}

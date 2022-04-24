#include "timed_tasks.h"
#include "game.h"
#include "world.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TASK_LIMIT 4

// It happens that we create three tasks, one of each ID. That's not a requirement.
#define TASK_ID_BRICK2 1
#define TASK_ID_BRICK3 2
#define TASK_ID_BARREL 3

/* Globals.
 */

// There won't be many tasks so I won't bother sorting them.
static struct task {
  uint8_t id;
  uint32_t time; // trigger when game clock goes below this (video frames)
} taskv[TASK_LIMIT];
uint8_t taskc=0;
static uint8_t tasktimer;
  

/* Init.
 */
 
void timed_tasks_init() {
  tasktimer=60;
  
  // Divide the game's time in 4, and put a task at some random time within each of the first three chunks.
  // No constraint on which task goes in which chunk, but always create one of each task (BRICK2,BRICK3,BARREL).
  // Don't make a task in the last quarter of time, because it might be impossible to complete before expiration.
  uint32_t quarterlen=GAME_DURATION_FRAMES/4;
  taskc=3;
  taskv[0].time=quarterlen*3+rand()%quarterlen;
  taskv[1].time=quarterlen*2+rand()%quarterlen;
  taskv[2].time=quarterlen*1+rand()%quarterlen;
  
  // Since there's only 6 possible orders, don't bother generalizing.
  switch (rand()%6) {
    case 0: taskv[0].id=TASK_ID_BRICK2; taskv[1].id=TASK_ID_BRICK3; taskv[2].id=TASK_ID_BARREL; break;
    case 1: taskv[0].id=TASK_ID_BRICK2; taskv[1].id=TASK_ID_BARREL; taskv[2].id=TASK_ID_BRICK3; break;
    case 2: taskv[0].id=TASK_ID_BRICK3; taskv[1].id=TASK_ID_BRICK2; taskv[2].id=TASK_ID_BARREL; break;
    case 3: taskv[0].id=TASK_ID_BRICK3; taskv[1].id=TASK_ID_BARREL; taskv[2].id=TASK_ID_BRICK2; break;
    case 4: taskv[0].id=TASK_ID_BARREL; taskv[1].id=TASK_ID_BRICK3; taskv[2].id=TASK_ID_BRICK2; break;
    case 5: taskv[0].id=TASK_ID_BARREL; taskv[1].id=TASK_ID_BRICK2; taskv[2].id=TASK_ID_BRICK3; break;
  }
}

/* Nonzero if now is a good time to refill the truck.
 * It must be empty and off camera. We don't check for sprites.
 */
 
static uint8_t truck_available() {
  
  const uint8_t *v=grid+WORLD_W_TILES*14+12;
  if (v[0]||v[1]||v[2]) return 0;
  
  int16_t left=TILE_W_MM*10;
  int16_t right=TILE_W_MM*15;
  if (camera.x>=right) return 1;
  if (camera.x+camera.w<=left) return 1;
  return 0;
}

/* Execute task.
 * Return nonzero if completed; zero to try again later.
 */
 
static uint8_t execute_task(uint8_t taskid) {
  switch (taskid) {
    
    case TASK_ID_BRICK2: {
        if (!truck_available()) return 0;
        grid[WORLD_W_TILES*14+12]=0x10;
        grid[WORLD_W_TILES*14+13]=0x10;
      } return 1;
    
    case TASK_ID_BRICK3: {
        if (!truck_available()) return 0;
        grid[WORLD_W_TILES*14+12]=0x10;
        grid[WORLD_W_TILES*14+13]=0x10;
        grid[WORLD_W_TILES*14+14]=0x10;
      } return 1;
    
    case TASK_ID_BARREL: {
        if (!truck_available()) return 0;
        grid[WORLD_W_TILES*14+13]=0x11;
      } return 1;
    
  }
  // Unknown task, call it complete so it goes away.
  return 1;
}

/* Update.
 */
 
void timed_tasks_update() {

  // Only check every 60th frame, otherwise when a task is impossible it retries a lot.
  if (tasktimer-->0) return;
  tasktimer=60;
  
  uint8_t i=taskc;
  while (i-->0) {
    struct task *task=taskv+i;
    if (task->time>=gameclock) {
      if (execute_task(task->id)) {
        taskc--;
        memmove(task,task+1,sizeof(struct task)*(taskc-i));
      }
    }
  }
}

#include "highscore.h"
#include "platform.h"

#if PO_NATIVE
  #include <stdlib.h>
  #include <stdio.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/stat.h>
  
#else
  #include "tinysd.h"
#endif

/* Globals.
 */
 
static uint32_t highscore=0;
static uint8_t highscore_loaded=0;

/* Path for native builds.
 */
 
#if PO_NATIVE
  static char highscore_path_storage[1024];
  static const char *highscore_path() {
    const char *home=getenv("HOME");
    int c=snprintf(highscore_path_storage,sizeof(highscore_path_storage),"%s/.ivand/highscore",home);
    if ((c<1)||(c>=sizeof(highscore_path_storage))) return 0;
    int slashp=c-1;
    while (slashp&&(highscore_path_storage[slashp]!='/')) slashp--;
    highscore_path_storage[slashp]=0;
    mkdir(highscore_path_storage,0775);
    highscore_path_storage[slashp]='/';
    return highscore_path_storage;
  }
#endif

/* Read from disk.
 */
 
static void highscore_load() {
  #if PO_NATIVE
    const char *path=highscore_path();
    if (!path) return;
    int fd=open(path,O_RDONLY);
    if (fd<0) return;
    int len=read(fd,&highscore,sizeof(highscore));
    close(fd);
  #else
    int32_t len=tinysd_read(&highscore,sizeof(highscore),"/ivand/hiscore.bin");
  #endif
  highscore_loaded=1;
}

/* Write to disk.
 */
 
static void highscore_save() {
  #if PO_NATIVE
    const char *path=highscore_path();
    if (!path) return;
    int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd<0) return;
    int dummy=write(fd,&highscore,sizeof(highscore));
    close(fd);
  #else
    tinysd_write("/ivand/hiscore.bin",&highscore,sizeof(highscore));
  #endif
}

/* Get one high score.
 */
 
uint32_t highscore_get() {
  if (!highscore_loaded) highscore_load();
  return highscore;
}

/* Set one high score.
 */
 
void highscore_set(uint32_t score) {
  if (score==highscore) return;
  highscore=score;
  highscore_save();
}

/* Send score to server.
 */
 
void highscore_send(uint32_t score) {
  char msg[64];
  int msgc=snprintf(msg,sizeof(msg),"score:%d:%d:%d\n",99,score,0);
  if ((msgc<1)||(msgc>=sizeof(msg))) return;
  usb_send(msg,msgc);
}

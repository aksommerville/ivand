#include "midi.h"
#include "decoder.h"
#include "serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Delete.
 */
 
void midi_file_del(struct midi_file *file) {
  if (!file) return;
  if (file->refc-->1) return;
  if (file->trackv) {
    while (file->trackc-->0) {
      void *v=file->trackv[file->trackc].v;
      if (v) free(v);
    }
    free(file->trackv);
  }
  free(file);
}

/* Retain.
 */
 
int midi_file_ref(struct midi_file *file) {
  if (!file) return -1;
  if (file->refc<1) return -1;
  if (file->refc==INT_MAX) return -1;
  file->refc++;
  return 0;
}

/* Receive MThd.
 */
 
static int midi_file_decode_MThd(struct midi_file *file,const uint8_t *src,int srcc) {

  if (file->division) return -1; // Multiple MThd
  if (srcc<6) return -1; // Malformed MThd
  
  file->format=(src[0]<<8)|src[1];
  file->track_count=(src[2]<<8)|src[3];
  file->division=(src[4]<<8)|src[5];
  
  if (!file->division) return -1; // Division must be nonzero, both mathematically and because we use it as "MThd present" flag.
  if (file->division&0x8000) return -1; // SMPTE timing not supported (TODO?)
  // (format) must be 1, but I think we can take whatever.

  return 0;
}

/* Receive MTrk.
 */
 
static int midi_file_decode_MTrk(struct midi_file *file,const void *src,int srcc) {

  if (file->trackc>=file->tracka) {
    int na=file->tracka+8;
    if (na>INT_MAX/sizeof(struct midi_track)) return -1;
    void *nv=realloc(file->trackv,sizeof(struct midi_track)*na);
    if (!nv) return -1;
    file->trackv=nv;
    file->tracka=na;
  }
  
  void *nv=malloc(srcc?srcc:1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  
  struct midi_track *track=file->trackv+file->trackc++;
  track->v=nv;
  track->c=srcc;

  return 0;
}

/* Receive unknown chunk.
 */
 
static int midi_file_decode_other(struct midi_file *file,int chunkid,const void *src,int srcc) {
  // Would be reasonable to call it an error. Whatever.
  return 0;
}

/* Finish decoding, validate.
 */
 
static int midi_file_decode_finish(struct midi_file *file) {
  if (!file->division) return -1; // No MThd
  if (file->trackc<1) return -1; // No MTrk
  return 0;
}

/* Decode.
 */
 
static int midi_file_decode(struct midi_file *file,const void *src,int srcc) {
  struct decoder decoder={.src=src,.srcc=srcc};
  while (decoder_remaining(&decoder)) {
    int chunkid,c;
    const void *v;
    if (decode_intbe(&chunkid,&decoder,4)<0) return -1;
    if ((c=decode_intbelen(&v,&decoder,4))<0) return -1;
    switch (chunkid) {
      case ('M'<<24)|('T'<<16)|('h'<<8)|'d': if (midi_file_decode_MThd(file,v,c)<0) return -1; break;
      case ('M'<<24)|('T'<<16)|('r'<<8)|'k': if (midi_file_decode_MTrk(file,v,c)<0) return -1; break;
      default: if (midi_file_decode_other(file,chunkid,v,c)<0) return -1;
    }
  }
  return midi_file_decode_finish(file);
}

/* New.
 */
 
struct midi_file *midi_file_new(const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return 0;
  struct midi_file *file=calloc(1,sizeof(struct midi_file));
  if (!file) return 0;
  file->refc=1;
  if (midi_file_decode(file,src,srcc)<0) {
    midi_file_del(file);
    return 0;
  }
  return file;
}

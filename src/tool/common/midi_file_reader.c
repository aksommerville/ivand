#include "midi.h"
#include "serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

/* Object lifecycle.
 */
 
void midi_file_reader_del(struct midi_file_reader *reader) {
  if (!reader) return;
  if (reader->refc-->1) return;
  midi_file_del(reader->file);
  if (reader->trackv) free(reader->trackv);
  free(reader);
}

int midi_file_reader_ref(struct midi_file_reader *reader) {
  if (!reader) return -1;
  if (reader->refc<1) return -1;
  if (reader->refc==INT_MAX) return -1;
  reader->refc++;
  return 0;
}

/* Initialize tracks.
 */
 
static int midi_file_reader_init_tracks(struct midi_file_reader *reader) {
  reader->trackc=reader->file->trackc;
  if (!(reader->trackv=calloc(sizeof(struct midi_track_reader),reader->trackc))) return -1;
  const struct midi_track *ktrack=reader->file->trackv;
  struct midi_track_reader *rtrack=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;ktrack++,rtrack++) {
    rtrack->track=*ktrack;
    rtrack->delay=rtrack->delay0=-1;
  }
  return 0;
}

/* New.
 */
 
struct midi_file_reader *midi_file_reader_new(struct midi_file *file,int rate) {
  if (rate<1) return 0;
  struct midi_file_reader *reader=calloc(1,sizeof(struct midi_file_reader));
  if (!reader) return 0;
  
  reader->refc=1;
  reader->rate=0;
  reader->repeat=0;
  reader->usperqnote=500000;
  reader->usperqnote0=reader->usperqnote;
  
  if (midi_file_ref(file)<0) {
    midi_file_reader_del(reader);
    return 0;
  }
  reader->file=file;
  
  if (midi_file_reader_init_tracks(reader)<0) {
    midi_file_reader_del(reader);
    return 0;
  }
  
  if (midi_file_reader_set_rate(reader,rate)<0) {
    midi_file_reader_del(reader);
    return 0;
  }
  
  return reader;
}

/* Recalculate tempo derivative fields from (rate,usperqnote).
 */
 
static void midi_file_reader_recalculate_tempo(struct midi_file_reader *reader) {
  // We could combine (rate/(division*M)) into one coefficient that only changes with rate,
  // but really tempo changes aren't going to happen often enough to worry about it.
  reader->framespertick=((double)reader->usperqnote*(double)reader->rate)/((double)reader->file->division*1000000.0);
}

/* Change rate.
 */

int midi_file_reader_set_rate(struct midi_file_reader *reader,int rate) {
  if (rate<1) return -1;
  if (rate==reader->rate) return 0;
  reader->rate=rate;
  midi_file_reader_recalculate_tempo(reader);
  return 0;
}

/* Return to repeat point and return new delay (always >0).
 */
 
static int midi_file_a_capa(struct midi_file_reader *reader) {
  if (reader->usperqnote!=reader->usperqnote0) {
    reader->usperqnote=reader->usperqnote0;
    midi_file_reader_recalculate_tempo(reader);
  }
  int delay=INT_MAX;
  struct midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    track->p=track->p0;
    track->term=track->term0;
    track->delay=track->delay0;
    track->status=track->status0;
    if ((track->delay>=0)&&(track->delay<delay)) delay=track->delay;
  }
  if (delay<1) {
    for (track=reader->trackv,i=reader->trackc;i-->0;track++) {
      if (track->delay>=0) track->delay++;
    }
    return 1;
  }
  return delay;
}

/* Set the repeat point from current state.
 */
 
static void midi_file_reader_set_capa(struct midi_file_reader *reader) {
  reader->usperqnote0=reader->usperqnote;
  struct midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    track->p0=track->p;
    track->term0=track->term;
    track->delay0=track->delay;
    track->status0=track->status;
  }
}

/* Examine an event before returning it, and update any reader state, eg tempo.
 * We can cause errors but can not modify the event or prevent it being returned, that's by design.
 */
 
static int midi_file_reader_local_event(struct midi_file_reader *reader,const struct midi_event *event) {
  switch (event->opcode) {
    case MIDI_OPCODE_META: switch (event->a) {
    
        case 0x51: { // Set Tempo
            if (event->c==3) {
              const uint8_t *B=event->v;
              int usperqnote=(B[0]<<16)|(B[1]<<8)|B[2];
              if (usperqnote&&(usperqnote!=reader->usperqnote)) {
                reader->usperqnote=usperqnote;
                midi_file_reader_recalculate_tempo(reader);
              }
            }
          } break;
          
        // We can't do 0x2f End of Track because we don't know anymore which track caused it.
        // I feel 0x2f is redundant anyway.
          
      } break;
    case MIDI_OPCODE_SYSEX: {
    
        // Highly nonstandard loop declaration, it's my own thing.
        if ((event->c==9)&&!memcmp(event->v,"BBx:START",9)) {
          midi_file_reader_set_capa(reader);
        }
    
      } break;
  }
  return 0;
}

/* Read delay from one track.
 * Sets (term) at EOF or error, otherwise sets (delay>=0).
 * Call when (delay<0).
 */
 
static void midi_track_read_delay(struct midi_file_reader *reader,struct midi_track_reader *track) {
  if (track->p>=track->track.c) {
    track->term=1;
    return;
  }
  int err=sr_vlq_decode(&track->delay,track->track.v+track->p,track->track.c-track->p);
  if ((err<=0)||(track->delay<0)) {
    track->term=1;
    return;
  }
  track->p+=err;
  if (track->delay) { // zero is zero no matter what
    // ...and nonzero is nonzero no matter what
    int framec=lround(track->delay*reader->framespertick);
    if (framec<1) track->delay=1;
    else track->delay=framec;
  }
}

/* Read one event (after delay).
 * On success, returns >0 and fully updates track.
 * Anything else, caller should mark the track terminated and stop using it.
 * This does not change any reader state, only track state.
 */
 
static int midi_track_read_event(struct midi_event *event,struct midi_file_reader *reader,struct midi_track_reader *track) {
  
  // EOF?
  if (track->p>=track->track.c) return -1;
  
  // Acquire status byte.
  uint8_t status=track->status;
  if (track->track.v[track->p]&0x80) {
    status=track->track.v[track->p++];
  }
  if (!status) return -1;
  track->status=status;
  track->delay=-1;
  
  event->opcode=status&0xf0;
  event->chid=status&0x0f;
  switch (status&0xf0) {
    #define DATA_A { \
      if (track->p>track->track.c-1) return -1; \
      event->a=track->track.v[track->p++]; \
    }
    #define DATA_AB { \
      if (track->p>track->track.c-2) return -1; \
      event->a=track->track.v[track->p++]; \
      event->b=track->track.v[track->p++]; \
    }
    case MIDI_OPCODE_NOTE_OFF: DATA_AB break;
    case MIDI_OPCODE_NOTE_ON: DATA_AB if (!event->b) { event->opcode=MIDI_OPCODE_NOTE_OFF; event->b=0x40; } break;
    case MIDI_OPCODE_NOTE_ADJUST: DATA_AB break;
    case MIDI_OPCODE_CONTROL: DATA_AB break;
    case MIDI_OPCODE_PROGRAM: DATA_A break;
    case MIDI_OPCODE_PRESSURE: DATA_A break;
    case MIDI_OPCODE_WHEEL: DATA_AB break;
    #undef DATA_A
    #undef DATA_AB
    default: track->status=0; event->chid=MIDI_CHID_ALL; switch (status) {
    
        case 0xf0: case 0xf7: {
            event->opcode=MIDI_OPCODE_SYSEX;
            int len,err;
            if ((err=sr_vlq_decode(&len,track->track.v+track->p,track->track.c-track->p))<1) return -1;
            track->p+=err;
            if (track->p>track->track.c-len) return -1;
            event->v=track->track.v+track->p;
            event->c=len;
            track->p+=len;
            if ((event->c>=1)&&(((uint8_t*)event->v)[event->c-1]==0xf7)) event->c--;
          } return 1;
    
        case 0xff: {
            event->opcode=MIDI_OPCODE_META;
            if (track->p>=track->track.c) return -1;
            event->a=track->track.v[track->p++];
            int len,err;
            if ((err=sr_vlq_decode(&len,track->track.v+track->p,track->track.c-track->p))<1) return -1;
            track->p+=err;
            if (track->p>track->track.c-len) return -1;
            event->v=track->track.v+track->p;
            event->c=len;
            track->p+=len;
          } return 1;
    
        default: return -1;
      }
  }
  
  return 1;
}

/* Next event, main entry point.
 */
 
int midi_file_reader_update(struct midi_event *event,struct midi_file_reader *reader) {

  // Already delaying? Get out.
  if (reader->delay) return reader->delay;

  // One event at time zero, or determine the delay to the next one.
  int delay=INT_MAX;
  struct midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    if (track->term) continue;
    if (track->delay<0) {
      midi_track_read_delay(reader,track);
      if (track->term) continue;
    }
    if (track->delay) {
      if (track->delay<delay) delay=track->delay;
      continue;
    }
    if (midi_track_read_event(event,reader,track)>0) {
      if (midi_file_reader_local_event(reader,event)<0) return -1;
      return 0;
    } else {
      track->term=1;
    }
  }
  
  // If we didn't select a delay, we're at EOF.
  if (delay==INT_MAX) {
    if (reader->repeat) {
      return midi_file_a_capa(reader);
    }
    return -1;
  }
  
  // Sleep.
  reader->delay=delay;
  return delay;
}

/* Advance clock.
 */
 
int midi_file_reader_advance(struct midi_file_reader *reader,int framec) {
  if (framec<0) return -1;
  if (framec>reader->delay) return -1;
  reader->delay-=framec;
  struct midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    if (track->delay>=framec) track->delay-=framec;
  }
  return 0;
}

/* Check termination.
 */
 
int midi_file_reader_is_terminated(const struct midi_file_reader *reader) {
  if (!reader) return 1;
  if (reader->delay) return 0;
  struct midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    if (!track->term) return 0;
  }
  return 1;
}

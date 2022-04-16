#include "midi.h"
#include <string.h>
#include <stdlib.h>

/* Replace Note On at velocity zero, with Note Off at velocity 0x40.
 */
 
static inline void midi_note_off_if_v0_note_on(struct midi_event *event) {
  if (event->opcode==MIDI_OPCODE_NOTE_ON) {
    if (event->b==0x00) {
      event->opcode=MIDI_OPCODE_NOTE_OFF;
      event->b=0x40;
    }
  }
}

/* Decode event from stream.
 */
 
int midi_stream_decode(struct midi_event *event,struct midi_stream *stream,const void *src,int srcc) {
  if (srcc<1) return 0;
  const uint8_t *SRC=src;
  int srcp=0;
  
  // Consume Status byte if there is one.
  // If we had an incomplete event pending, it's lost.
  int expectc;
  switch (SRC[srcp]&0xf0) {
    case MIDI_OPCODE_NOTE_ON: stream->status=SRC[srcp++]; stream->dc=0; break;
    case MIDI_OPCODE_NOTE_OFF: stream->status=SRC[srcp++]; stream->dc=0; break;
    case MIDI_OPCODE_NOTE_ADJUST: stream->status=SRC[srcp++]; stream->dc=0; break;
    case MIDI_OPCODE_CONTROL: stream->status=SRC[srcp++]; stream->dc=0; break;
    case MIDI_OPCODE_PROGRAM: stream->status=SRC[srcp++]; stream->dc=0; break;
    case MIDI_OPCODE_PRESSURE: stream->status=SRC[srcp++]; stream->dc=0; break;
    case MIDI_OPCODE_WHEEL: stream->status=SRC[srcp++]; stream->dc=0; break;
    case 0xf0: switch (SRC[srcp]) {
      case MIDI_OPCODE_SYSEX: stream->status=SRC[srcp++]; stream->dc=0; break;
      case MIDI_OPCODE_EOX: { // End Of Exclusive behaves like Realtime, but resets Status.
          stream->status=0;
          stream->dc=0;
          event->opcode=SRC[srcp++];
          event->chid=MIDI_CHID_ALL;
        } return srcp;
      case MIDI_OPCODE_CLOCK: // realtime...
      case MIDI_OPCODE_START:
      case MIDI_OPCODE_CONTINUE:
      case MIDI_OPCODE_STOP:
      case MIDI_OPCODE_ACTIVE_SENSE:
      case MIDI_OPCODE_SYSTEM_RESET: {
          event->opcode=SRC[srcp++];
          event->chid=MIDI_CHID_ALL;
        } return srcp;
      default: { // Unsupported opcode, probably System Common. Consume whatever data bytes there are after it.
          stream->status=0;
          stream->dc=0;
          event->opcode=SRC[srcp++];
          event->chid=MIDI_CHID_ALL;
          event->v=SRC+srcp;
          event->c=0;
          while ((srcp<srcc)&&!(SRC[srcp]&0x80)) { srcp++; event->c++; }
          if (event->c>=2) { event->a=((uint8_t*)event->v)[0]; event->b=((uint8_t*)event->v)[1]; }
          else if (event->c>=1) { event->a=((uint8_t*)event->v)[0]; }
          return srcp;
        }
      } break;
  }
  
  // If we're doing Sysex, consume as much as possible.
  // Report it as SYSEX event if the terminator is already present, otherwise PARTIAL.
  if (stream->status==MIDI_OPCODE_SYSEX) {
    event->opcode=MIDI_OPCODE_PARTIAL;
    event->chid=MIDI_CHID_ALL;
    event->v=SRC+srcp;
    event->c=0;
    while ((srcp<srcc)&&!(SRC[srcp]&0x80)) { event->c++; srcp++; }
    if ((srcp<srcc)&&(SRC[srcp]==MIDI_OPCODE_EOX)) {
      stream->status=0;
      event->opcode=MIDI_OPCODE_SYSEX;
      srcp++;
    }
    return srcp;
  }
  
  // If we don't have a status, something is wrong. Report a PARTIAL with data bytes.
  if (!stream->status) {
    event->opcode=MIDI_OPCODE_PARTIAL;
    event->chid=MIDI_CHID_ALL;
    event->v=SRC+srcp;
    event->c=0;
    while ((srcp<srcc)&&!(SRC[srcp]&0x80)) { event->c++; srcp++; }
    return srcp;
  }
  
  // Determine how many data bytes we're looking for, based on Status.
  int datac;
  switch (stream->status&0xf0) {
    case MIDI_OPCODE_NOTE_OFF: datac=2; break;
    case MIDI_OPCODE_NOTE_ON: datac=2; break;
    case MIDI_OPCODE_NOTE_ADJUST: datac=2; break;
    case MIDI_OPCODE_CONTROL: datac=2; break;
    case MIDI_OPCODE_PROGRAM: datac=1; break;
    case MIDI_OPCODE_PRESSURE: datac=1; break;
    case MIDI_OPCODE_WHEEL: datac=2; break;
    default: return -1;
  }
  while (stream->dc<datac) {
    if ((srcp>=srcc)||(SRC[srcp]&0x80)) {
      event->opcode=MIDI_OPCODE_STALL;
      return srcp;
    }
    stream->d[stream->dc++]=SRC[srcp++];
  }
  event->opcode=stream->status&0xf0;
  event->chid=stream->status&0x0f;
  event->a=stream->d[0];
  event->b=stream->d[1];
  stream->dc=0;
  midi_note_off_if_v0_note_on(event);
  return srcp;
}

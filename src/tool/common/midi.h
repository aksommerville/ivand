/* midi.h
 * Decode MIDI files and streams.
 */
 
#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>

/* Event.
 *****************************************************************/
 
// Channel Voice Messages.
#define MIDI_OPCODE_NOTE_OFF     0x80
#define MIDI_OPCODE_NOTE_ON      0x90
#define MIDI_OPCODE_NOTE_ADJUST  0xa0
#define MIDI_OPCODE_CONTROL      0xb0
#define MIDI_OPCODE_PROGRAM      0xc0
#define MIDI_OPCODE_PRESSURE     0xd0
#define MIDI_OPCODE_WHEEL        0xe0

// System Common Messages. (stream only)
#define MIDI_OPCODE_TIMECODE      0xf1
#define MIDI_OPCODE_SONG_POSITION 0xf2
#define MIDI_OPCODE_SONG_SELECT   0xf3
#define MIDI_OPCODE_TUNE_REQUEST  0xf6
#define MIDI_OPCODE_EOX           0xf7

// Real Time Messages. (stream only)
#define MIDI_OPCODE_CLOCK         0xf8
#define MIDI_OPCODE_START         0xfa
#define MIDI_OPCODE_CONTINUE      0xfb
#define MIDI_OPCODE_STOP          0xfc
#define MIDI_OPCODE_ACTIVE_SENSE  0xfe
#define MIDI_OPCODE_SYSTEM_RESET  0xff

// Other Messages.
#define MIDI_OPCODE_SYSEX         0xf0 /* stream or file. You'll get this only for complete events. */
#define MIDI_OPCODE_META          0x01 /* file only. (a)=type */
#define MIDI_OPCODE_PARTIAL       0x02 /* dump of undecodable data, eg Sysex interrupted by Realtime */
#define MIDI_OPCODE_STALL         0x03 /* next event not fully received yet */

#define MIDI_CHID_ALL 0xff

// Channel Mode Messages. (Control keys)
#define MIDI_CONTROL_SOUND_OFF     0x78
#define MIDI_CONTROL_RESET_CONTROL 0x79
#define MIDI_CONTROL_LOCAL         0x7a
#define MIDI_CONTROL_NOTES_OFF     0x7b /* 0x7b..0x7f are all "notes off" */
#define MIDI_CONTROL_OMNI_OFF      0x7c
#define MIDI_CONTROL_OMNI_ON       0x7d
#define MIDI_CONTROL_MONOPHONIC    0x7e
#define MIDI_CONTROL_POLYPHONIC    0x7f

struct midi_event {
  uint8_t opcode;
  uint8_t chid;
  uint8_t a,b;
  const void *v;
  int c;
};

/* Stream.
 *********************************************************/
 
struct midi_stream {
  uint8_t status; // CV opcode+chid, MIDI_OPCODE_SYSEX, or zero
  uint8_t d[2];
  int dc;
};

/* Consume some amount of (src), populate (event), return the length consumed.
 * We do not support Channel Voice events split by Realtime.
 */
int midi_stream_decode(struct midi_event *event,struct midi_stream *stream,const void *src,int srcc);

/* File.
 ********************************************************/
 
struct midi_file {
  int refc;
  
  int format;
  int track_count;
  int division;
  
  struct midi_track {
    uint8_t *v;
    int c;
  } *trackv;
  int trackc,tracka;
};

struct midi_file_reader {
  int refc;
  
  int repeat;
  int rate;
  int usperqnote,usperqnote0;
  int delay; // frames
  double framespertick;
  
  struct midi_file *file;
  struct midi_track_reader {
    struct midi_track track; // (v) is borrowed
    int p,p0;
    int term,term0;
    int delay,delay0; // <0 if not decoded yet; >0 in frames
    uint8_t status,status0;
  } *trackv;
  int trackc;
};

void midi_file_del(struct midi_file *file);
int midi_file_ref(struct midi_file *file);
struct midi_file *midi_file_new(const void *src,int srcc);

void midi_file_reader_del(struct midi_file_reader *reader);
int midi_file_reader_ref(struct midi_file_reader *reader);
struct midi_file_reader *midi_file_reader_new(struct midi_file *file,int rate);

int midi_file_reader_set_rate(struct midi_file_reader *reader,int rate);

/* Read the next event from this file, if it is not delayed.
 * Returns:
 *   >0: No event. Return value is frame count to next event.
 *   <0: No event. End of file or error.
 *    0: Event produced.
 * When it returns zero, you should process the event and immediately call again.
 * When >0, we will continue returning the same thing until you "advance" the clock.
 * In "repeat" mode, there is a brief implicit delay at the end of the file.
 */
int midi_file_reader_update(struct midi_event *event,struct midi_file_reader *reader);

/* Advance the reader's clock by the given frame count.
 * If this is greater than the delay to the next event, we report an error.
 */
int midi_file_reader_advance(struct midi_file_reader *reader,int framec);

/* Nonzero if we're at EOF.
 * Use this to distinguish "done" from "error", after midi_file_reader_update() returns <0.
 */
int midi_file_reader_is_terminated(const struct midi_file_reader *reader);

#endif

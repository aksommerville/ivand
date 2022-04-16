/* !!! BYTE ORDER !!!
 * We emit a C array of uint8_t of which portions will be read as uint16_t or uint32_t.
 * In other words we hard-code an assumption of the consumer's byte order.
 * This will only work if the build host has the same order as the target host.
 * x86 and Arduino are both little-endian. (as is pretty much everything in 2022).
 * For now, I'm only dealing with little-endian machines, so not addressing the issue.
 * Why the big warning? Most of our tools are byte-order-independent; this one is an oddball.
 */

#include "mksong_internal.h"

/* Append a single song event.
 */
 
static int mksong_song_append_delay(struct mksong *mksong,uint8_t tickc) {
  //fprintf(stderr,"emit delay %d\n",tickc);
  if (encode_intle(&mksong->song,tickc&0x7f,1)<0) return -1;
  mksong->timeticks+=(tickc&0x7f);
  return 0;
}

static int mksong_song_append_fireforget(struct mksong *mksong,uint8_t waveid,uint8_t noteid,uint8_t duration) {
  uint8_t raw[]={
    0x80|(waveid&0x07),
    noteid&0x7f,
    duration,
  };
  if (encode_raw(&mksong->song,raw,sizeof(raw))<0) return -1;
  return 0;
}

static int mksong_song_append_note_on(struct mksong *mksong,uint8_t waveid,uint8_t noteid) {
  uint8_t raw[]={
    0xe0|(waveid&0x07),
    noteid&0x7f,
  };
  if (encode_raw(&mksong->song,raw,sizeof(raw))<0) return -1;
  return 0;
}

static int mksong_song_append_note_off(struct mksong *mksong,uint8_t waveid,uint8_t noteid) {
  uint8_t raw[]={
    0xc0|(waveid&0x07),
    noteid&0x7f,
  };
  if (encode_raw(&mksong->song,raw,sizeof(raw))<0) return -1;
  return 0;
}

/* Append fakesheet event.
 */

static int mksong_fakesheet_append(
  struct mksong *mksong,
  int time_ticks,
  uint8_t channel,uint8_t wave,uint8_t note
) {
  if ((time_ticks<0)||(time_ticks>0xffff)) return -1;
  if (channel>4) return -1;
  if (wave>7) return -1;
  if (note>0x7f) return -1;
  uint32_t raw=(time_ticks<<16)|(channel<<12)|(wave<<8)|note;
  if (encode_raw(&mksong->fakesheet,&raw,sizeof(raw))<0) return -1; // NB byte order assumption
  return 0;
}

/* Advance clock.
 */
 
static int mksong_advance_clock(struct mksong *mksong,int framec) {
  if (midi_file_reader_advance(mksong->reader,framec)<0) {
    fprintf(stderr,"%s: Error parsing MIDI file.\n",TOOL->srcpath);
    return -1;
  }
  mksong->time+=framec;
  mksong->parttime+=framec;
  while (1) {
    int delaytickc=mksong->parttime/MKSONG_FRAMES_PER_TICK;
    if (delaytickc<1) break;
    if (delaytickc>0x7f) delaytickc=0x7f;
    if (mksong_song_append_delay(mksong,delaytickc)<0) return -1;
    mksong->parttime-=delaytickc*MKSONG_FRAMES_PER_TICK;
  }
  return 0;
}

/* Program Change.
 */
 
static int mksong_event_program_change(struct mksong *mksong,uint8_t chid,uint8_t pid) {
  if (chid>=16) return 0;
  uint8_t waveid=pid&0x07;
  mksong->wave_by_channel[chid]=waveid;
  return 0;
}

/* Note Off.
 */
 
static int mksong_event_note_off(struct mksong *mksong,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  if (chid>=16) return 0;
  struct mksong_hold *hold=mksong->holdv;
  int i=MKSONG_HOLD_LIMIT;
  for (;i-->0;hold++) {
    if (hold->chid!=chid) continue;
    if (hold->noteid!=noteid) continue;
    
    if (hold->input) {
      // Releasing an input note. We don't record the end of inputs. Do nothing but clear the hold.
      memset(hold,0,sizeof(struct mksong_hold));
      return 0;
    }
    // Releasing a playback note.
    //TODO Capture hold time, and convert to fireforget if we can. Remember to update (mksong->termsongp) if so.
    if (mksong_song_append_note_off(mksong,hold->waveid,noteid)<0) return -1;
    memset(hold,0,sizeof(struct mksong_hold));
    return 0;
  }
  // Note was not present. Whatever, do nothing.
  return 0;
}

/* Add hold. Fails if there isn't space.
 */
 
static struct mksong_hold *mksong_hold_add(struct mksong *mksong,uint8_t chid,uint8_t noteid) {
  struct mksong_hold *available=0,*hold=mksong->holdv;
  int i=MKSONG_HOLD_LIMIT;
  for (;i-->0;hold++) {
    if ((hold->chid==chid)&&(hold->noteid==noteid)) {
      fprintf(stderr,"%s:ERROR: Repeated note %d on channel %d\n",TOOL->srcpath,noteid,chid);
      return 0;
    }
    if (!available&&!hold->noteid) available=hold;
  }
  if (!available) {
    fprintf(stderr,
      "%s:ERROR: Too many simultaneous notes, limit %d. Around note %d on channel %d.\n",
      TOOL->srcpath,MKSONG_HOLD_LIMIT,noteid,chid
    );
    return 0;
  }
  available->chid=chid;
  available->noteid=noteid;
  available->waveid=mksong->wave_by_channel[chid];
  available->input=0;
  return available;
}

/* Note On.
 * midi_file_reader takes care of the zero-velocity implicit Note Off for us.
 */
 
static int mksong_event_note_on(struct mksong *mksong,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  if (chid>=16) return 0;
  if (!noteid) return 0; // We depend on noteid being nonzero. I don't believe zero is useful as a MIDI note anyway.
  struct mksong_hold *hold=mksong_hold_add(mksong,chid,noteid);
  if (!hold) return -1;
  mksong->termevc++;
  switch (velocity>>4) {
    case 3: hold->input=1; break;
    case 4: hold->input=2; break;
    case 5: hold->input=3; break;
    case 6: hold->input=4; break;
    case 7: hold->input=5; break;
  }
  if (hold->input) {
    if (mksong_fakesheet_append(
      mksong,
      mksong->timeticks,
      hold->input-1,
      mksong->wave_by_channel[chid],
      noteid
    )<0) return -1;
  } else {
    if (mksong_song_append_note_on(mksong,mksong->wave_by_channel[chid],noteid)<0) return -1;
  }
  return 0;
}

/* Terminator.
 * Here (mostly) I'm effecting a fix for this bug in Logic Pro X, where it extends one track for a long random (?) tail time.
 * The basic idea is we track the last Terminate event.
 * If there were no significant events (Note On) since the last Terminate, use the Terminate time instead.
 * In the faulty files you see something like this:
 *   ...song...
 *   TERMINATE TRACK 1 (and all other nonzero tracks)
 *   ...minute or so of delay...
 *   TERMINATE TRACK 0
 */
 
static int mksong_event_term(struct mksong *mksong) {
  if (mksong->termevc) {
    // There have been significant events since the last terminator, so record this position.
    mksong->termsongp=mksong->song.c;
    mksong->termtime=mksong->time;
    mksong->termevc=0;
  } else {
    // No significant events since the last terminator, keep the old one.
  }
  return 0;
}

/* Receive MIDI event.
 */
 
static int mksong_receive_event(struct mksong *mksong,const struct midi_event *event) {
  /**
  fprintf(stderr,
    "  %s %02x@%02x (%02x,%02x) c=%d\n",
    __func__,event->opcode,event->chid,
    event->a,event->b,event->c
  );
  /**/
  switch (event->opcode) {
    case MIDI_OPCODE_PROGRAM: return mksong_event_program_change(mksong,event->chid,event->a);
    case MIDI_OPCODE_NOTE_ON: return mksong_event_note_on(mksong,event->chid,event->a,event->b);
    case MIDI_OPCODE_NOTE_OFF: return mksong_event_note_off(mksong,event->chid,event->a,event->b);
    case MIDI_OPCODE_META: switch (event->a) {
        case 0x2f: return mksong_event_term(mksong);
      } break;
  }
  return 0;
}

/* Finish conversion.
 */
 
static int mksong_finish(struct mksong *mksong) {

  // The Logic Bug. Truncate song to the last recorded Terminate if we haven't got any significant events.
  if (!mksong->termevc) {
    if (mksong->termsongp<mksong->song.c) {
      //fprintf(stderr,"%s: Reducing length from %d to %d due to The Logic Bug.\n",__func__,mksong->song.c,mksong->termsongp);
      mksong->song.c=mksong->termsongp;
    }
  }

  // Pad song to a multiple of 4.
  int extra=mksong->song.c&3;
  if (extra) {
    extra=4-extra;
    if (encode_raw(&mksong->song,"\0\0\0\0",extra)<0) return -1;
  }
  
  // Fakesheet must also pad to 4, but it must have done that anyway -- confirm.
  if (mksong->fakesheet.c&3) {
    fprintf(stderr,"%s: Somehow ended up with fakesheet length %d, not a multiple of 4.\n",TOOL->srcpath,mksong->fakesheet.c);
    return -1;
  }
  
  // Emit header.
  int ticksperbeat=((int64_t)mksong->reader->usperqnote*(int64_t)22050)/(int64_t)1000000;
  if ((ticksperbeat<1)||(ticksperbeat>0xffff)) {
    fprintf(stderr,"%s: Unexpressible tempo. us/qnote=%d\n",TOOL->srcpath,mksong->reader->usperqnote);
    return -1;
  }
  uint16_t header[]={
    ticksperbeat,
    0, // addl header len
    mksong->song.c,
    mksong->fakesheet.c,
  };
  if (encode_raw(&mksong->bin,header,sizeof(header))<0) return -1;
  
  // Emit chunks.
  // zero-length additional header -- done
  if (encode_raw(&mksong->bin,mksong->song.v,mksong->song.c)<0) return -1;
  if (encode_raw(&mksong->bin,mksong->fakesheet.v,mksong->fakesheet.c)<0) return -1;

  return 0;
}

/* Read MIDI file from (TOOL->src) and populate (TOOL->bin) with our output binary.
 */
 
static int mksong_process(struct mksong *mksong) {
  //fprintf(stderr,"%s %s %d bytes...\n",__func__,TOOL->srcpath,TOOL->srcc);
  
  struct midi_file *midifile=midi_file_new(TOOL->src,TOOL->srcc);
  if (!midifile) {
    fprintf(stderr,"%s: Failed to decode MIDI file.\n",TOOL->srcpath);
    return -1;
  }
  if (!(mksong->reader=midi_file_reader_new(midifile,MIDI_READ_RATE))) return -1;
  midi_file_del(midifile);
  
  while (1) {
    struct midi_event event={0};
    int err=midi_file_reader_update(&event,mksong->reader);
    if (err<0) {
      if (midi_file_reader_is_terminated(mksong->reader)) break;
      fprintf(stderr,"%s: Error reading MIDI file.\n",TOOL->srcpath);
      return -1;
    }
    if (err) {
      if (mksong_advance_clock(mksong,err)<0) return -1;
    } else {
      if (mksong_receive_event(mksong,&event)<0) return -1;
    }
  }
  
  if (mksong_finish(mksong)<0) return -1;
  
  return 0;
}

/* Main.
 */

int main(int argc,char **argv) {
  struct mksong _mksong={0};
  struct mksong *mksong=&_mksong;
  if (tool_startup(TOOL,argc,argv,0)<0) return 1;
  if (TOOL->terminate) return 0;
  if (tool_read_input(TOOL)<0) return 1;
  
  if (mksong_process(mksong)<0) {
    fprintf(stderr,"%s: Failed to convert song.\n",TOOL->srcpath);
    return 1;
  }
  
  if (tool_generate_c_preamble(TOOL)<0) return 1;
  if (tool_generate_c_array(TOOL,0,0,0,0,mksong->bin.v,mksong->bin.c)<0) return 1;
  if (tool_write_output(TOOL)<0) return 1;
  return 0;
}

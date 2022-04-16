#include "synth.h"
#include <stdio.h>

#define SYNTH_RELEASE_TIME 2000

/* Read and process events from song at the current pointer, advancing state.
 * Stops after we set a delay or loop.
 */
 
static void synth_consume_song(struct synth *synth) {
  while (1) {
  
    // Stop processing at end of song for at least one frame, as a safety measure.
    if (synth->songp>=synth->songc) {
      synth_release_all(synth);
      if (1) { // repeat
        synth->songp=0;
        synth->songtime=0;
      } else {
        synth->song=0;
        synth->songc=0;
        synth->songp=0;
        synth->songtime=0;
      }
      return;
    }
    
    uint8_t lead=synth->song[synth->songp++];
    
    // DELAY
    if (!(lead&0x80)) {
      synth->songdelay=lead*SYNTH_FRAMES_PER_TICK;
      return;
    }
    
    #define REQUIRE(c) { \
      if (synth->songp>synth->songc-(c)) { \
        fprintf(stderr,"Overran song expecting %d bytes at %d/%d\n",c,synth->songp,synth->songc); \
        synth->song=0; \
        synth->songc=0; \
        return; \
      } \
    }
    
    // All other currently-defined commands are distinguishable by the top 5 bits.
    switch (lead&0xf8) {
    
      case 0x80: { // NOTE_FIREFORGET
          REQUIRE(2)
          uint8_t b1=synth->song[synth->songp++];
          uint8_t b2=synth->song[synth->songp++];
          synth_note_fireforget(synth,lead&0x07,b1&0x7f,b2);
        } break;
    
      case 0xe0: { // NOTE_ON
          REQUIRE(1)
          uint8_t b1=synth->song[synth->songp++];
          synth_note_on(synth,lead&0x07,b1&0x7f);
        } break;
    
      case 0xc0: { // NOTE_OFF
          REQUIRE(1)
          uint8_t b1=synth->song[synth->songp++];
          synth_note_off(synth,lead&0x07,b1&0x7f);
        } break;
    
      default: {
          fprintf(stderr,"Unknown song command 0x%02x at %d/%d\n",lead,synth->songp-1,synth->songc);
          synth->song=0;
          synth->songc=0;
          synth_silence_all(synth);
          return;
        }
    }
    #undef REQUIRE
  }
}

/* Update.
 */
 
int16_t synth_update(struct synth *synth) {

  // Update song.
  if (synth->songhold>0) {
    synth->songhold--;
  } else if (synth->songdelay>0) {
    synth->songdelay--;
    synth->songtime++;
  } else if (synth->song) {
    synth->songtime++;
    synth_consume_song(synth);
  }

  // Generate PCM.
  int32_t sample=0;
  struct synth_voice *voice=synth->voicev;
  uint8_t i=synth->voicec;
  for (;i-->0;voice++) {
    if (voice->ttl>0) {
      voice->ttl--;
      int32_t sample1=voice->v[voice->p>>SYNTH_P_SHIFT];
      if (voice->ttl<SYNTH_RELEASE_TIME) {
        if (!voice->ttl) voice->waveid=voice->noteid=0xff;
        sample1*=((voice->ttl*0x400)/SYNTH_RELEASE_TIME);
      } else {
        sample1<<=10;
      }
      sample+=sample1;
      voice->p+=voice->pd;
    }
  }
  sample>>=10;
  if (sample>32767) return 32767;
  if (sample<-32768) return -32768;
  return sample;
}

/* MIDI noteid to 22050-based frequency, normalized to 32 bits.
 */
 
static uint32_t noterates22050[128]={
  2125742,2252146,2386065,2527948,2678268,2837526,3006254,3185015,3374406,3575058,3787642,4012867,4251485,
  4504291,4772130,5055896,5356535,5675051,6012507,6370030,6748811,7150117,7575285,8025735,8502970,9008582,
  9544261,10111792,10713070,11350103,12025015,12740059,13497623,14300233,15150569,16051469,17005939,18017165,
  19088521,20223584,21426141,22700205,24050030,25480119,26995246,28600467,30301139,32102938,34011878,36034330,
  38177043,40447168,42852281,45400411,48100060,50960238,53990491,57200933,60602276,64205876,68023757,72068660,
  76354085,80894335,85704563,90800821,96200119,101920476,107980983,114401866,121204555,128411753,136047513,
  144137319,152708170,161788671,171409126,181601643,192400238,203840952,215961966,228803732,242409110,256823506,
  272095026,288274639,305416341,323577341,342818251,363203285,384800477,407681904,431923931,457607465,484818220,
  513647012,544190053,576549277,610832681,647154683,685636503,726406571,769600953,815363807,863847862,915214929,
  969636441,1027294024,1088380105,1153098554,1221665363,1294309365,1371273005,1452813141,1539201906,1630727614,
  1727695724,1830429858,1939272882,2054588048,2176760211,2306197109,2443330725,2588618730,2742546010,2905626283,
  3078403812,3261455229,
};

/* Get available voice or pick one to overwrite.
 */
 
static struct synth_voice *synth_get_available_voice(struct synth *synth) {
  while (synth->voicec&&!synth->voicev[synth->voicec-1].ttl) synth->voicec--;
  if (synth->voicec<SYNTH_VOICE_LIMIT) return synth->voicev+synth->voicec++;
  struct synth_voice *best=0;
  struct synth_voice *voice=synth->voicev;
  uint8_t i=synth->voicec;
  for (;i-->0;voice++) {
    if (!voice->ttl) return voice;
    if (!best) best=voice;
    else if (voice->ttl<best->ttl) best=voice;
  }
  return best;
}

/* Play notes, user supplies the wave.
 */
 
struct synth_voice *synth_begin_note(struct synth *synth,const int16_t *wave,uint8_t noteid) {
  if (!wave) return 0;
  struct synth_voice *voice=synth_get_available_voice(synth);
  voice->v=wave;
  voice->p=0;
  voice->pd=noterates22050[noteid&0x7f];
  voice->ttl=UINT32_MAX;
  voice->waveid=voice->noteid=0xff;
  return voice;
}

struct synth_voice *synth_fireforget_note(struct synth *synth,const int16_t *wave,uint8_t noteid,uint32_t durframes) {
  if (!wave) return 0;
  struct synth_voice *voice=synth_get_available_voice(synth);
  voice->v=wave;
  voice->p=0;
  voice->pd=noterates22050[noteid&0x7f];
  voice->ttl=durframes;
  voice->waveid=voice->noteid=0xff;
  return voice;
}
 
void synth_end_note(struct synth *synth,struct synth_voice *voice) {
  if (voice<synth->voicev) return;
  if (voice>=synth->voicev+SYNTH_VOICE_LIMIT) return;
  if (voice->ttl>SYNTH_RELEASE_TIME) {
    voice->ttl=SYNTH_RELEASE_TIME;
  }
  voice->waveid=voice->noteid=0xff;
}

/* Play notes, close to encoded format.
 */
 
void synth_note_fireforget(struct synth *synth,uint8_t waveid,uint8_t noteid,uint8_t durticks) {
  if (waveid>=SYNTH_WAVE_COUNT) return;
  struct synth_voice *voice=synth_fireforget_note(synth,synth->wavev[waveid],noteid,durticks*SYNTH_FRAMES_PER_TICK);
}

void synth_note_on(struct synth *synth,uint8_t waveid,uint8_t noteid) {
  if (waveid>=SYNTH_WAVE_COUNT) return;
  struct synth_voice *voice=synth_begin_note(synth,synth->wavev[waveid],noteid);
  if (!voice) return;
  voice->waveid=waveid;
  voice->noteid=noteid;
}

void synth_note_off(struct synth *synth,uint8_t waveid,uint8_t noteid) {
  struct synth_voice *voice=synth->voicev;
  uint8_t i=synth->voicec;
  for (;i-->0;voice++) {
    if (voice->waveid!=waveid) continue;
    if (voice->noteid!=noteid) continue;
    synth_end_note(synth,voice);
    return;
  }
}

/* Two flavors of "shut up".
 */
 
void synth_release_all(struct synth *synth) {
  struct synth_voice *voice=synth->voicev;
  uint8_t i=synth->voicec;
  for (;i-->0;voice++) {
    if (voice->ttl>SYNTH_RELEASE_TIME) {
      voice->ttl=SYNTH_RELEASE_TIME;
    }
    voice->waveid=voice->noteid=0xff;
  }
}

void synth_silence_all(struct synth *synth) {
  synth->voicec=0;
}

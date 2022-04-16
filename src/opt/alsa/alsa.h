/* alsa.h
 * Straight 16-bit PCM out via ALSA (libasound).
 * Also provides promiscuous MIDI input, only useful to our tooling, but super cheap.
 * That's the kind of MIDI you want when connecting to a sequencer.
 * To talk to physical devices, use ossmidi. (ALSA can do that too, but I'm not fond of the API).
 */
 
#ifndef ALSA_H
#define ALSA_H

struct alsa;

#include <stdint.h>

struct alsa_delegate {
  int rate;
  int chanc;
  const char *device; // eg "hw:0,3"
  void *userdata;
  int (*cb_pcm_out)(int16_t *dst,int dsta,struct alsa *alsa);
  // Leave this null for no midi:
  int (*cb_midi_in)(const void *src,int srcc,struct alsa *alsa);
};

void alsa_del(struct alsa *alsa);
int alsa_ref(struct alsa *alsa);

struct alsa *alsa_new(
  const struct alsa_delegate *delegate
);

/* Your callback is guaranteed not running while you hold the lock.
 */
int alsa_lock(struct alsa *alsa);
int alsa_unlock(struct alsa *alsa);

/* (rate,chanc,userdata) won't change once set.
 * (rate,chanc) are not necessarily what you asked for.
 */
int alsa_get_rate(const struct alsa *alsa);
int alsa_get_chanc(const struct alsa *alsa);
void *alsa_get_userdata(const struct alsa *alsa);
int alsa_get_status(const struct alsa *alsa); // => 0,-1

// No harm either way, but only necessary if you're using MIDI in.
int alsa_update(struct alsa *alsa);

#endif

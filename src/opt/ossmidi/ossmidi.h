/* ossmidi.h
 * MIDI-In via OSS interface (should work on ALSA and PulseAudio systems too).
 * The astute user will recognize that this API looks almost identical to evdev.
 *
 * *** pokorc: For use in tools only. Refers to some sources under tool/common/ ***
 */
 
#ifndef OSSMIDI_H
#define OSSMIDI_H

struct ossmidi;
struct ossmidi_device;
struct poller;
struct midi_event;

/* Collection.
 **************************************************************/
 
struct ossmidi_delegate {
  void *userdata;
  int (*cb_connect)(struct ossmidi *ossmidi,struct ossmidi_device *device);
  int (*cb_disconnect)(struct ossmidi *ossmidi,struct ossmidi_device *device);
  int (*cb_event)(struct ossmidi *ossmidi,struct ossmidi_device *device,const struct midi_event *event);
};
 
void ossmidi_del(struct ossmidi *ossmidi);
int ossmidi_ref(struct ossmidi *ossmidi);

struct ossmidi *ossmidi_new(
  const char *path,
  struct poller *poller,
  const struct ossmidi_delegate *delegate
);

void *ossmidi_get_userdata(const struct ossmidi *ossmidi);

// It will arrive again if reconnected.
int ossmidi_drop_device(struct ossmidi *ossmidi,struct ossmidi_device *device);

/* Consider one file.
 * If we connect it, optionally put a WEAK reference in (*device).
 */
int ossmidi_examine_file(
  struct ossmidi_device **device,
  struct ossmidi *ossmidi,
  const char *path,const char *base
);

struct ossmidi_device *ossmidi_get_device_by_path(const struct ossmidi *ossmidi,const char *path,int pathc);
struct ossmidi_device *ossmidi_get_device_by_fd(const struct ossmidi *ossmidi,int fd);

/* On my ALSA system, there is no ioctl that fetches the name for eg "/dev/midi1".
 * So this helper opens "/proc/asound/oss/sndstat" and scans for the given device ID.
 * Null path for defaults, or you can ask for one TOC file specifically.
 * FYI: OSS docs specifically warn against this strategy, but it's the best I can do.
 */
int ossmidi_find_device_name(char *dst,int dsta,int devid,const char *path);

/* Device.
 **************************************************************/
 
void ossmidi_device_del(struct ossmidi_device *device);
int ossmidi_device_ref(struct ossmidi_device *device);

struct ossmidi_device *ossmidi_device_new(
  const char *path,
  int (*cb)(struct ossmidi_device *device,const struct midi_event *event),
  void *userdata
);

int ossmidi_device_read(struct ossmidi_device *device);
int ossmidi_device_receive_input(struct ossmidi_device *device,const void *src,int srcc);

void *ossmidi_device_get_userdata(const struct ossmidi_device *device);
int ossmidi_device_get_fd(const struct ossmidi_device *device);
const char *ossmidi_device_get_name(const struct ossmidi_device *device);
const char *ossmidi_device_get_path(const struct ossmidi_device *device);
 
#endif

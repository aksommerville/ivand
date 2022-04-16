/* data.h
 * References and helpers around the embedded data.
 */
 
#ifndef DATA_H
#define DATA_H

#include "platform.h"

/* IMPORTANT: These must both be 128x128 or we might read undefined memory.
 */
extern struct image bgtiles;
extern struct image fgbits;

extern const int16_t wave0[];
extern const int16_t wave1[];
extern const int16_t wave2[];
extern const int16_t wave3[];
extern const int16_t wave4[];
extern const int16_t wave5[];
extern const int16_t wave6[];
extern const int16_t wave7[];

extern const uint32_t font[96];

#endif

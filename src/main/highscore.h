/* highscore.h
 * Access to persisted high scores file.
 * Not very involved, just a 4-byte binary file with the single high score in native byte order.
 */
 
#ifndef HIGHSCORE_H
#define HIGHSCORE_H

#include <stdint.h>

uint32_t highscore_get();
void highscore_set(uint32_t score);

/* Slightly different concern.
 * Send the score to our server via USB, if possible.
 */
void highscore_send(uint32_t score);

#endif

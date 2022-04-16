#ifndef PLATFORM_H
#define PLATFORM_H

#define BUTTON_LEFT     0x01
#define BUTTON_RIGHT    0x02
#define BUTTON_UP       0x04
#define BUTTON_DOWN     0x08
#define BUTTON_A        0x10
#define BUTTON_B        0x20

#include <stdint.h>

#ifdef __cplusplus
  extern "C" {
#endif

/* Provided by game; called by Arduino or genioc.
 *********************************************************************/

void setup();
void loop();
int16_t audio_next();

/* Provided by driver.
 *********************************************************************/
 
uint8_t platform_init();
uint8_t platform_update();
void platform_send_framebuffer(const void *fb);

void usb_send(const void *v,int c);
int usb_read(void *dst,int dsta);
int usb_read_byte();

/* Imaging.
 *********************************************************************/
 
struct image {
  uint16_t *v;
  int16_t w,h;
  int16_t stride; // in pixels
};

/* We check output bounds but not input -- one presumes you know the input geometry well.
 */
void image_blit_opaque(
  struct image *dst,int16_t dstx,int16_t dsty,
  const struct image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h
);
void image_blit_colorkey(
  struct image *dst,int16_t dstx,int16_t dsty,
  const struct image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h
);
void image_blit_colorkey_flop(
  struct image *dst,int16_t dstx,int16_t dsty,
  const struct image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h
);

/* See etc/doc/font.txt for details about the format.
 * Blit one glyph, top-left corner at (dstx,dsty).
 * Return the total horizontal advancement, including the one pixel space.
 * Space rows by 9 pixels.
 */
uint8_t image_blit_glyph(
  struct image *dst,int16_t dstx,int16_t dsty,
  uint32_t glyph,uint16_t color
);

uint16_t image_blit_string(
  struct image *dst,int16_t dstx,int16_t dsty,
  const char *src,int8_t srcc,uint16_t color,
  const uint32_t *font /*96*/
);

void image_fill_rect(struct image *image,int16_t x,int16_t y,int16_t w,int16_t h,uint16_t color);

#ifdef __cplusplus
  }
#endif

#endif

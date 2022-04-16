/* world.h
 * World map and sprites.
 *
 * There are three units of measure: Tile, Pixel, Mm.
 * Mm are the preferred measure, finer than a pixel, so we don't have to use floats.
 * You must keep the world's width in mm well under 32767.
 * 32 mm/px * 8 px/tile * 60 tiles/world yields 15360 mm wide.
 */
 
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

struct image;

#define MM_PER_PIXEL 32
#define TILE_W_PIXELS 8
#define TILE_H_PIXELS 8
#define TILE_W_MM (TILE_W_PIXELS*MM_PER_PIXEL)
#define TILE_H_MM (TILE_H_PIXELS*MM_PER_PIXEL)
#define WORLD_W_TILES 60
#define WORLD_H_TILES 32
#define WORLD_W_PIXELS (WORLD_W_TILES*TILE_W_PIXELS)
#define WORLD_H_PIXELS (WORLD_H_TILES*TILE_H_PIXELS)
#define WORLD_W_MM (WORLD_W_PIXELS*MM_PER_PIXEL)
#define WORLD_H_MM (WORLD_H_PIXELS*MM_PER_PIXEL)
#define CAMERA_W_PIXELS 96 /* Should match the Tiny's framebuffer. Can go smaller if we want. */
#define CAMERA_H_PIXELS 64
#define CAMERA_W_MM (CAMERA_W_PIXELS*MM_PER_PIXEL)
#define CAMERA_H_MM (CAMERA_H_PIXELS*MM_PER_PIXEL)

#define GRAVITY MM_PER_PIXEL /* mm/frame */

#define SPRITE_LIMIT 32
#define SPRITE_OPAQUE_SIZE 64

extern uint8_t grid[WORLD_W_TILES*WORLD_H_TILES];

#define SPRITE_HEADER \
  uint8_t controller; \
  int16_t x,y,w,h; /* mm, physical bounds */

extern struct sprite {
  SPRITE_HEADER
  uint8_t opaque[SPRITE_OPAQUE_SIZE]; // for controller's use
} spritev[SPRITE_LIMIT];

#define SPRITE_CONTROLLER_NONE 0
#define SPRITE_CONTROLLER_IVAN 1
#define SPRITE_CONTROLLER_DUMMY 2
#define SPRITE_CONTROLLER_GUARD 3

extern struct camera {
  int16_t x,y,w,h; // Boundaries in mm, watch for exceeding left and right world edges.
} camera;

void grid_default();

/* Position the camera so we're looking at (focus).
 * If (focus) null, just establish valid bounds anywhere.
 * Final vertical position will be entirely within the world.
 * Horizontal position may exceed the right edge but never the left: (camera.x) always ends up positive.
 */
void camera_update(const struct sprite *focus);

/* Render one contiguous region of the grid onto (dst).
 * Caller must take care of the horizontal wrapping.
 */
void grid_render(
  struct image *dst,int16_t dstxpx,int16_t dstypx,
  int16_t srcxmm,int16_t srcymm,
  int16_t wmm,int16_t hmm
);

uint8_t grid_contains_any_solid(int16_t xmm,int16_t ymm,int16_t wmm,int16_t hmm);

#endif

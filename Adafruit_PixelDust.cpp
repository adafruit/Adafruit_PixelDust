/*!
 * @file Adafruit_PixelDust.cpp
 *
 * @mainpage Particle simulation for "LED sand."
 *
 * @section intro_sec Introduction
 *
 * This handles the "physics engine" part of a sand/rain simulation.
 * The term "physics" is used loosely here...it's a relatively crude
 * algorithm that's appealing to the eye but takes many shortcuts with
 * collision detection, etc.
 *
 * @section dependencies Dependencies
 *
 * Not dependent on other libraries for compilation. HOWEVER, this code
 * does not actually render anything itself and needs to be used in
 * conjunction with a display-specific library to handle graphics.
 *
 * @section author Author
 *
 * Written by Phil "PaintYourDragon" Burgess for Adafruit Industries.
 *
 * @section license License
 *
 * BSD license, all text here must be included in any redistribution.
 *
 */

#include "Adafruit_PixelDust.h"

Adafruit_PixelDust::Adafruit_PixelDust(dimension_t w, dimension_t h,
                                       grain_count_t n, uint8_t s, uint8_t e,
                                       bool sort)
    : single_width(w), single_height(h), n_grains(n), scale(s), elasticity(e),
      grain(NULL), bitmaps(NULL) {
  (void)sort; // Silence compiler warning about now-unused variable
}

Adafruit_PixelDust::Adafruit_PixelDust(grain_count_t n, uint8_t s, uint8_t e)
    : n_grains(n), scale(s), elasticity(e), grain(NULL), bitmaps(NULL) {}

Adafruit_PixelDust::~Adafruit_PixelDust(void) {
  if (grain)
    free(grain);
  if (bitmaps)
    free(bitmaps);
  if (plane)
    free(plane);
}

static void normalize(float *vec) {
  float d = vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];
  if (d > 0) {
    d = sqrt(d);
    for (uint8_t i=0; i<3; i++) vec[i] /= d;
  }
}

bool Adafruit_PixelDust::begin(Plane *plane_core, uint8_t np) {

  if ((bitmaps))
    return true; // Already allocated

  // If plane_core is NULL and/or num_planes is 0, this is a single-plane
  // instance where width & height were passed to constructor. All other
  // plane data can be faked, so set up a single Plane structure and pass
  // along a pointer to the multi-plane init code that follows. MUST declare
  // this outside the if(){} so it remains active in scope later. If it's
  // not used (if multi-plane invocation), no harm done.
  Plane solo = {
      single_width,
      single_height,
      {1,  0, 0},
      {0, -1, 0},
      {{255, EDGE_NONE}, {255, EDGE_NONE}, {255, EDGE_NONE}, {255, EDGE_NONE}}};
  if ((plane_core == NULL) || (np == 0)) {
    plane_core = &solo;
    np = 1;
  }

  num_planes = np;

  if ((plane = (PlaneDerived *)malloc(num_planes * sizeof(PlaneDerived)))) {
    // Determine total size of all bitmaps, accounting for scanline pads.
    uint32_t bitmap_bytes = 0;
    for (int p = 0; p < num_planes; p++) {
      bitmap_bytes += ((plane_core[p].width + 7) / 8) * plane_core[p].height;
    }
    if ((bitmaps = (uint8_t *)calloc(bitmap_bytes, 1))) {
      if ((!n_grains) || (grain = (Grain *)calloc(n_grains, sizeof(Grain)))) {

        // Everything allocated successfully. Set up structures...

        uint8_t *bitmap_ptr = bitmaps;
        for (int p = 0; p < num_planes; p++) {
          // Save core structure data (in case it was a local var)
          memcpy(&plane[p].core, &plane_core[p], sizeof(Plane));
          // Calculate additional derived values (from core)
          plane[p].xMax = plane_core[p].width * 256 - 1;
          plane[p].yMax = plane_core[p].height * 256 - 1;
          plane[p].bitmap = bitmap_ptr;
          plane[p].w8 = (plane_core[p].width + 7) / 8;
          bitmap_ptr += plane[p].w8 * plane_core[p].height;
          // Normalize X & Y vectors in case passed in un-normalized
          normalize(plane[p].core.x_vec);
          normalize(plane[p].core.y_vec);
          // Z vector is perpendicular cross-product of X & Y vectors
#if 1
          plane[p].z_vec[0] = plane[p].core.x_vec[1] * plane[p].core.y_vec[2] -
                              plane[p].core.x_vec[2] * plane[p].core.y_vec[1];
          plane[p].z_vec[1] = plane[p].core.x_vec[2] * plane[p].core.y_vec[0] -
                              plane[p].core.x_vec[0] * plane[p].core.y_vec[2];
          plane[p].z_vec[2] = plane[p].core.x_vec[0] * plane[p].core.y_vec[1] -
                              plane[p].core.x_vec[1] * plane[p].core.y_vec[0];
#else
          plane[p].z_vec[0] = plane[p].core.x_vec[2] * plane[p].core.y_vec[1] -
                              plane[p].core.x_vec[1] * plane[p].core.y_vec[2];
          plane[p].z_vec[1] = plane[p].core.x_vec[0] * plane[p].core.y_vec[2] -
                              plane[p].core.x_vec[2] * plane[p].core.y_vec[0];
          plane[p].z_vec[2] = plane[p].core.x_vec[1] * plane[p].core.y_vec[0] -
                              plane[p].core.x_vec[0] * plane[p].core.y_vec[1];
#endif
        }

        return true; // Success!

        // Else one or more allocations failed. Clean up any loose ends...

      } else {
        free(bitmaps);
        bitmaps = NULL;
      }
    } else {
      free(plane);
    }
  }
  return false; // You LOSE, good DAY sir!
}

bool Adafruit_PixelDust::setPosition(grain_count_t i, dimension_t x,
                                     dimension_t y, uint8_t p) {
  if (getPixel(x, y, p))
    return false; // Position already occupied
  setPixel(x, y, p);
  grain[i].x = x * 256 + 127; // Near center of pixel
  grain[i].y = y * 256 + 127;
  grain[i].plane = p;

  return true;
}

void Adafruit_PixelDust::getPosition(grain_count_t i, dimension_t *x,
                                     dimension_t *y, uint8_t *p) const {
  *x = grain[i].x / 256;
  *y = grain[i].y / 256;
  if (p)
    *p = grain[i].plane;
}

// Fill grain structures with random positions, making sure no two are
// in the same location.
void Adafruit_PixelDust::randomize(void) {
  for (grain_count_t i = 0; i < n_grains; i++) {
    uint8_t p = random(num_planes);
    while (!setPosition(i, random(plane[p].core.width),
                        random(plane[p].core.height), p))
      ;
  }
}

// Pixel set/read functions for the bitmap buffer

#ifdef __AVR__

// Shift-by-N operations are costly on AVR, so table lookups are used.

static const uint8_t PROGMEM clr[] = {~0x80, ~0x40, ~0x20, ~0x10,
                                      ~0x08, ~0x04, ~0x02, ~0x01},
                             set[] = {0x80, 0x40, 0x20, 0x10,
                                      0x08, 0x04, 0x02, 0x01};

void Adafruit_PixelDust::setPixel(dimension_t x, dimension_t y, uint8_t p) {
  plane[p].bitmap[y * plane[p].w8 + x / 8] |= pgm_read_byte(&set[x & 7]);
}

void Adafruit_PixelDust::clearPixel(dimension_t x, dimension_t y, uint8_t p) {
  plane[p].bitmap[y * plane[p].w8 + x / 8] &= pgm_read_byte(&clr[x & 7]);
}

bool Adafruit_PixelDust::getPixel(dimension_t x, dimension_t y,
                                  uint8_t p) const {
  return plane[p].bitmap[y * plane[p].w8 + x / 8] & pgm_read_byte(&set[x & 7]);
}

#else

// Most other architectures will perform better with shifts.

void Adafruit_PixelDust::setPixel(dimension_t x, dimension_t y, uint8_t p) {
  plane[p].bitmap[y * plane[p].w8 + x / 8] |= (0x80 >> (x & 7));
}

void Adafruit_PixelDust::clearPixel(dimension_t x, dimension_t y, uint8_t p) {
  plane[p].bitmap[y * plane[p].w8 + x / 8] &= (0x7F7F >> (x & 7));
}

bool Adafruit_PixelDust::getPixel(dimension_t x, dimension_t y,
                                  uint8_t p) const {
  return plane[p].bitmap[y * plane[p].w8 + x / 8] & (0x80 >> (x & 7));
}

#endif // !__AVR__

// Clears bitmap buffer.  Grain positions are unchanged,
// probably want to follow up with some place() calls.
void Adafruit_PixelDust::clear(void) {
  if (bitmaps) {
    uint32_t bitmap_bytes = 0;
    for (int p = 0; p < num_planes; p++) {
      bitmap_bytes += ((plane[p].core.width + 7) / 8) * plane[p].core.height;
    }
    memset(bitmaps, 0, bitmap_bytes);
  }
}

// Returns true if new position is occupied or off edge of simulation,
// false if new position is available. Backwards from what one might
// expect, but is to be compatible with getPixel() behavior.
bool Adafruit_PixelDust::cross(Grain *in, Grain *out) {

  // Initially copy 'in' to 'out', and then individual elements of 'out'
  // might get overridden later if needed.
  memcpy(out, in, sizeof(Grain));

  // Assume no edge traversal to start...
  Direction x_edge = EDGE_NONE, y_edge = EDGE_NONE;
  // ...but then set x/y flags if we're off 1 or 2 edges:
  if (in->x < 0) {
    x_edge = EDGE_LEFT; // Attempt leaving plane via left edge
  } else if (in->x > plane[in->plane].xMax) {
    x_edge = EDGE_RIGHT; // Attempt leaving plane via right edge
  }
  if (in->y < 0) {
    y_edge = EDGE_TOP; // Attempt leaving plane via top edge
  } else if (in->y > plane[in->plane].yMax) {
    y_edge = EDGE_BOTTOM; // Attempt leaving plane via bottom edge
  }

  if ((x_edge == EDGE_NONE) && (y_edge == EDGE_NONE)) {

    // Pixel changed, plane has not. Return is-occupied state of new pixel.
    return getPixel(in->x / 256, in->y / 256, in->plane);

  } else if ((x_edge == EDGE_NONE) ^ (y_edge == EDGE_NONE)) {

    // Grain attempting to leave plane via ONE edge only.
    Direction edge = ((x_edge != EDGE_NONE) ? x_edge : y_edge);

    // If no destination plane out that edge though (i.e. a wall),
    // return true (regard as occupied).
    if (plane[in->plane].core.link[edge].plane == 255) {
      return true;
    }

    // It's happening, grain is moving to a new plane.
    out->plane = plane[in->plane].core.link[edge].plane;

    // Reorient X and Y positions and velocities in grain space.
    switch ((edge << 2) | plane[in->plane].core.link[edge].side) {
    case (EDGE_TOP << 2) | EDGE_TOP:
      out->x = plane[out->plane].xMax - in->x;
      out->y = -1 - in->y; // -1=0, -2=1, -3=2 etc.
      out->vx = -in->vx;
      out->vy = -in->vy;
      break;
    case (EDGE_TOP << 2) | EDGE_LEFT:
      out->x = -1 - in->y; // -1=0, -2=1, -3=2 etc.
      out->y = in->x;
      out->vx = -in->vy;
      out->vy = in->vx;
      break;
    case (EDGE_TOP << 2) | EDGE_RIGHT:
      out->x = plane[out->plane].xMax + 1 + in->y;
      out->y = plane[out->plane].yMax - in->x;
      out->vx = in->vy;
      out->vy = -in->vx;
      break;
    case (EDGE_TOP << 2) | EDGE_BOTTOM:
      out->x = in->x;
      out->y = plane[out->plane].yMax + 1 + in->y;
      out->vx = in->vx;
      out->vy = in->vy;
      break;
    case (EDGE_LEFT << 2) | EDGE_TOP:
      out->x = in->y;
      out->y = 1 + in->x;
      out->vx = in->vy;
      out->vy = -in->vx;
      break;
    case (EDGE_LEFT << 2) | EDGE_LEFT:
      out->x = 1 + in->x;
      out->y = plane[out->plane].yMax - in->y;
      out->vx = -in->vx;
      out->vy = -in->vy;
      break;
    case (EDGE_LEFT << 2) | EDGE_RIGHT:
      out->x = plane[out->plane].xMax + 1 + in->x;
      out->y = in->y;
      out->vx = in->vx;
      out->vy = in->vy;
      break;
    case (EDGE_LEFT << 2) | EDGE_BOTTOM:
      out->x = plane[out->plane].xMax - in->y;
      out->y = plane[out->plane].yMax + 1 + in->x;
      out->vx = -in->vy;
      out->vy = in->vx;
      break;
    case (EDGE_RIGHT << 2) | EDGE_TOP:
      out->x = plane[in->plane].yMax - in->y;
      out->y = in->x - plane[in->plane].xMax - 1;
      out->vx = -in->vy;
      out->vy = in->vx;
      break;
    case (EDGE_RIGHT << 2) | EDGE_LEFT:
      out->x = in->x - plane[in->plane].xMax - 1;
      out->y = in->y;
      out->vx = in->vx;
      out->vy = in->vy;
      break;
    case (EDGE_RIGHT << 2) | EDGE_RIGHT:
      out->x = plane[out->plane].xMax - (in->x - plane[in->plane].xMax) - 1;
      out->y = plane[out->plane].yMax - grain->y;
      out->vx = -in->vx;
      out->vy = -in->vy;
      break;
    case (EDGE_RIGHT << 2) | EDGE_BOTTOM:
      out->x = in->y;
      out->y = plane[out->plane].yMax - (in->x - plane[in->plane].xMax) - 1;
      out->vx = in->vy;
      out->vy = -in->vx;
      break;
    case (EDGE_BOTTOM << 2) | EDGE_TOP:
      out->x = in->x;
      out->y = in->y - plane[in->plane].yMax - 1;
      out->vx = in->vx;
      out->vy = in->vy;
      break;
    case (EDGE_BOTTOM << 2) | EDGE_LEFT:
      out->x = in->y - plane[in->plane].yMax - 1;
      out->y = plane[in->plane].xMax - in->x;
      out->vx = in->vy;
      out->vy = -in->vx;
      break;
    case (EDGE_BOTTOM << 2) | EDGE_RIGHT:
      out->x = plane[out->plane].xMax - (in->y - plane[in->plane].yMax - 1);
      out->y = in->x;
      out->vx = -in->vy;
      out->vy = in->vx;
      break;
    case (EDGE_BOTTOM << 2) | EDGE_BOTTOM:
      out->x = plane[in->plane].xMax - in->x;
      out->y = plane[out->plane].yMax - (in->y - plane[in->plane].yMax - 1);
      out->vx = -in->vx;
      out->vy = -in->vy;
      break;
    }
    // Return true if 'out' pixel is occupied, false if available
    return getPixel(out->x / 256, out->y / 256, out->plane);
  }
// Should never reach here with these planes, but we do.
// Oh, i suppose the corner case.

  // Else rare ugly corner case where a grain might leave via BOTH edges.
  // This would explode in complexity as the correct action depends on
  // topology: the motion would be disallowed on a 3-corner pinch (e.g.
  // cube), but valid on coplanar and back-to-back planes...and detecting
  // the cube case gets really difficult because plane orientation is
  // arbitrary. Instead a dirty shortcut is used: this motion is just not
  // allowed. The calling code will then fall back on testing single-axis
  // X-major or Y-major motions and probably go with one of those. Worst
  // case, if both those slots are filled, the grain will stop until one
  // or the other space vacates. In isolation it might look slightly wonky,
  // but quickly integrated over many frames it becomes unnoticeable.
  return true; // Consider it occupied
}

#define BOUNCE(n) n = ((-n) * elasticity / 256) ///< 1-axis bounce

// Calculate one frame of particle interactions
void Adafruit_PixelDust::iterate(int16_t ax, int16_t ay, int16_t az) {

#if 0
  ax = (int32_t)ax * scale / 256;       // Scale down raw accelerometer
  ay = (int32_t)ay * scale / 256;       // inputs to manageable range.
  az = abs((int32_t)az * scale / 2048); // Z is further scaled down 1:8
  // A tiny bit of random motion is applied to each grain, so that tall
  // stacks of pixels tend to topple (else the whole stack slides across
  // the display). This is a function of the Z axis input, so it's more
  // pronounced the more the display is tilted (else the grains shift
  // around too much when the display is held level).
  az = (az >= 4) ? 1 : 5 - az; // Clip & invert
  ax -= az;                    // Subtract 100% Z motion factor from X & Y,
  ay -= az;                    // then compute 200% Z as amount to randomly
  int16_t az2 = az * 2 + 1;    // add back in, yielding +/- az randomness.
#else
  float fx = (float)ax;
  float fy = (float)ay;
  float fz = (float)az;
#endif

  for (uint8_t p = 0; p < num_planes; p++) {
    // Compute accel X/Y/Z2 transformed to each plane's coord space.
    // Accelerometer input coord sys is implied unit vectors along its own
    // X/Y/Z axes (e.g. [1 0 0][0 1 0][0 0 1]). Plane vectors are relative
    // to this and normalized. Results all int16_t, interim work uses floats.
#if 0
    plane[p].accel[0] = (int)(0.5 + (float)ax * plane[p].core.x_vec[0] +
      (float)ay * plane[p].core.y_vec[0] + (float)az * plane[p].z_vec[0]);
    plane[p].accel[1] = (int)(0.5 + (float)ax * plane[p].core.x_vec[1] +
      (float)ay * plane[p].core.y_vec[1] + (float)az * plane[p].z_vec[1]);
//    plane[p].accel[2] = (int)(0.5 + (float)az2 * plane[p].core.x_vec[2] +
//      (float)az2 * plane[p].core.y_vec[2] + (float)az2 * plane[p].z_vec[2]);
    plane[p].accel[2] = 0;
// Wait - shouldn't az2 calc be done on a per-plane basis, after xform?
// That is, the math above should work with the original inputs,
// and those scales/offsets occur when assigning to vars on the way out.
#else
    int32_t ix = (int32_t)(fx * plane[p].core.x_vec[0] +
      fy * plane[p].core.y_vec[0] + fz * plane[p].z_vec[0]) * scale / 256;
    int32_t iy = (int32_t)(fx * plane[p].core.x_vec[1] +
      fy * plane[p].core.y_vec[1] + fz * plane[p].z_vec[1]) * scale / 256;
    int32_t iz = abs((int32_t)(fz * plane[p].core.x_vec[2] +
      fy * plane[p].core.y_vec[2] + fz * plane[p].z_vec[2]) * scale / 2048);
    // A tiny bit of random motion is applied to each grain, so that tall
    // stacks of pixels tend to topple (else the whole stack slides across
    // the display). This is a function of the Z axis input, so it's more
    // pronounced the more the display is tilted (else the grains shift
    // around too much when the display is held level).
    iz = (iz >= 4) ? 1 : 5 - iz; // Clip & invert
    plane[p].accel[0] = ix - iz;
    plane[p].accel[1] = iy - iz;
    plane[p].accel[2] = iz * 2 + 1;
#endif
  }

  grain_count_t i;

  // Apply 2D accel vector to grain velocities...
  int32_t v2; // Velocity squared
  int16_t v;  // Absolute velocity, constrained (-256 to +256)
  for (i = 0; i < n_grains; i++) {
    uint8_t p = grain[i].plane;
    // See notes above, this is actually doing plus-or-minus randomness.
    grain[i].vx += plane[p].accel[0] + random(plane[p].accel[2]);
    grain[i].vy += plane[p].accel[1] + random(plane[p].accel[2]);
    // Terminal velocity (in any direction) is 256 units -- equal to
    // 1 pixel -- which keeps moving grains from passing through each other
    // and other such mayhem. Though it takes some extra math, velocity is
    // clipped as a 2D vector (not separately-limited X & Y) so that
    // diagonal movement isn't faster than horizontal/vertical. This all
    // takes place in the grain's current plane...if it would go off-edge
    // to another plane (and vectors shift around), that's computed later.
    v2 =
        (int32_t)grain[i].vx * grain[i].vx + (int32_t)grain[i].vy * grain[i].vy;
    if (v2 > 65536) { // If v^2 > 65536, then v > 256
      // Technically we'd want ceil(sqrt()) here, but ceil() is a floating-
      // point function and performance is better if getting back to integer
      // space ASAP. int(sqrt())+1 yields the same in all but perfect
      // squares, and in those rare cases the +1 is harmless, just means the
      // vector magnitude will get clipped to 255 instead of 256. Visually
      // imperceptible, and better too small than too big (would cause
      // aforementioned mayhem). Interesting side note, I tried various
      // "optimized" integer square root functions, and yet this round trip
      // through a float and sqrt() was always faster, has me wondering if
      // the compiler recognizes and calls a well-tuned integer sqrt func.
      v = int(sqrt((float)v2)) + 1;        // Velocity vector magnitude
      grain[i].vx = grain[i].vx * 256 / v; // Maintain heading &
      grain[i].vy = grain[i].vy * 256 / v; // limit magnitude
    }
  }

  // ...then update position of each grain, one at a time, checking for
  // collisions and having them react. This really seems like it shouldn't
  // work, as only one grain is considered at a time while the rest are
  // regarded as stationary. Yet this naive algorithm, taking many not-
  // technically-quite-correct steps, and repeated quickly enough,
  // visually integrates into something that somewhat resembles physics.
  // (I'd initially tried implementing this as a bunch of concurrent and
  // "realistic" elastic collisions among circular grains, but the
  // calculations and volume of code quickly got out of hand for both
  // the tiny 8-bit AVR microcontroller and my tiny dinosaur brain.)

#if 1

  for (i = 0; i < n_grains; i++) {
    Grain in; // Temp Grain struct used in motion calc & edge crossing

    // Copy current grain[i] to 'in', apply motion there. This might go
    // outside the current plane bounds...that's okay, is handled later.
    memcpy(&in, &grain[i], sizeof(Grain));
    in.x += in.vx;
    in.y += in.vy;

    // Get grain's pos. in pixel space of current plane, before and after
    // motion. This too may go out of bounds, that's handled later.
    // Negative pixel coordinates are temporarily positive and must be
    // specially handled (because of negative int rounding).
    position_t pixel_x_old = grain[i].x / 256;
    position_t pixel_y_old = grain[i].y / 256;
    position_t pixel_x_new = (in.x < 0 ? (in.x - 255) : in.x) / 256;
    position_t pixel_y_new = (in.y < 0 ? (in.y - 255) : in.y) / 256;

    // Is grain motion minor, within the same pixel?
    if ((pixel_x_new == pixel_x_old) && (pixel_y_new == pixel_y_old)) {

      // Still in same pixel, though x/y in grain space may have changed.
      grain[i].x = in.x; // Update position in grain space only,
      grain[i].y = in.y; // everything else is unchanged.

    } else { // Different pixel, possibly different plane

      // See if grain has crossed planes, and if dest. spot is occupied
      Grain out; // Second temp Grain used for edge crossing calc
      if(cross(&in, &out)) {

        // New pixel is occupied or is off a "wall" edge. Next course of
        // action depends on the type of motion...is new (blocked) pixel
        // directly up/down/left/right, or is it diagonal?

        position_t dx = abs(pixel_x_new - pixel_x_old); // 0 or 1
        if ((dx + abs(pixel_y_new - pixel_y_old)) == 1) {

          // Yes, it's up/down/left/right. Grain will remain in the same
          // pixel & plane, that won't change, but update pos. & velocity.
          if (dx == 1) {         // Blocked by pixel/wall to left or right
            grain[i].y = in.y;   // Apply Y motion only
            BOUNCE(grain[i].vx); // Bounce X velocity
          } else {               // Blocked by pixel/wall up or down
            grain[i].x = in.x;   // Apply X motion only
            BOUNCE(grain[i].vy); // Bounce Y velocity
          }
          continue; // Pixel isn't changing

        } else { // Diagonal intersection is more tricky...

          // Try skidding along just one axis of motion if possible
          // (start w/faster axis).
          memcpy(&in, &grain[i], sizeof(Grain)); // Reset 'in' struct

          if (abs(in.vx) >= abs(in.vy)) { // X axis is faster
            in.x += in.vx; // Apply X motion only
            BOUNCE(in.vy); // Presume bounced Y velocity
            // Must re-do cross() calculation because X/Y-only may yield a
            // different result than the diagonal cross() before.
            if (cross(&in, &out)) {   // X collision? Try Y...
              in.x = grain[i].x;      // Reset X
              in.vy = grain[i].vy;    // Reset Y velocity (bounced above)
              in.y += in.vy;          // Apply Y motion only
              BOUNCE(in.vx);          // Presume bounced X velocity
              if (cross(&in, &out)) { // Y collision
                // No X/Y motion, remain in same pixel & plane, but...
                BOUNCE(grain[i].vx); // Bounce both X
                BOUNCE(grain[i].vy); // and Y velocities
                continue;
              } // else Y-motion pixel 'out' is free, take it
            } // else X-motion pixel 'out' is free, take it
          } else {                    // Y axis is faster, start there
            in.y += in.vy;            // Apply Y motion only
            BOUNCE(in.vx);            // Presume bounced X velocity
            if (cross(&in, &out)) {   // Y collision? Try X...
              in.y = grain[i].y;      // Reset Y
              in.vx = grain[i].vx;    // Reset X velocity (bounced above)
              in.x += in.vx;          // Apply X motion only
              BOUNCE(in.vy);          // Presume bounced Y velocity
              if (cross(&in, &out)) { // X collision
                // No X/Y motion, remain in same pixel & plane, but...
                BOUNCE(grain[i].vx); // Bounce both X
                BOUNCE(grain[i].vy); // and Y velocities
                continue;
              } // else X-motion pixel 'out' is free, take it
            } // else Y-motion pixel 'out' is free, take it
          } // End X/Y axis
        } // End straight/diagonal
      } // else new pixel location is OK, might be same or new plane.

      // Clear old spot, update grain pos. to new coords/plane, set new spot
      clearPixel(grain[i].x / 256, grain[i].y / 256, grain[i].plane);
      memcpy(&grain[i], &out, sizeof(Grain)); // Save result
      setPixel(out.x / 256, out.y / 256, out.plane);
    } // end pixel change
  } // end i (per-grain loop)

#else

// THIS IS THE OLD CODE -----
// (works!)

  position_t newx, newy;
  uint8_t oldplane, newplane;
#ifdef __AVR__
  int16_t oldidx, newidx, delta;
#else
  int32_t oldidx, newidx, delta;
#endif

  for (i = 0; i < n_grains; i++) {

    oldplane = newplane = grain[i].plane;
    newx = grain[i].x + grain[i].vx; // New position in grain space,
    newy = grain[i].y + grain[i].vy; // might be off edge.

    if (newx < 0) {        // If grain would go out of bounds
      newx = 0;            // keep it inside,
      BOUNCE(grain[i].vx); // and bounce off wall
    } else if (newx > plane[oldplane].xMax) {
      newx = plane[oldplane].xMax;
      BOUNCE(grain[i].vx);
    }
    if (newy < 0) {
      newy = 0;
      BOUNCE(grain[i].vy);
    } else if (newy > plane[oldplane].yMax) {
      newy = plane[oldplane].yMax;
      BOUNCE(grain[i].vy);
    }

    // oldidx/newidx are the prior and new pixel index for this grain.
    // It's a little easier to check motion vs handling X & Y separately.
    oldidx = (grain[i].y / 256) * plane[oldplane].core.width + (grain[i].x / 256);
    newidx = (newy / 256) * plane[newplane].core.width + (newx / 256);

    if ((oldidx != newidx) && // If grain is moving to a new pixel...
        getPixel(newx / 256, newy / 256, newplane)) { // but if pixel already occupied...
      delta = abs(newidx - oldidx);         // What direction when blocked?
      if (delta == 1) {                     // 1 pixel left or right)
        newx = grain[i].x;                  // Cancel X motion
        BOUNCE(grain[i].vx);                // and bounce X velocity (Y is OK)
      } else if (delta == plane[oldplane].core.width) { // 1 pixel up or down
        newy = grain[i].y;                       // Cancel Y motion
        BOUNCE(grain[i].vy); // and bounce Y velocity (X is OK)
      } else {               // Diagonal intersection is more tricky...
        // Try skidding along just one axis of motion if possible
        // (start w/faster axis).
        if (abs(grain[i].vx) >= abs(grain[i].vy)) {      // X axis is faster
          if (!getPixel(newx / 256, grain[i].y / 256, newplane)) { // newx, oldy
            // That pixel's free!  Take it!  But...
            newy = grain[i].y;   // Cancel Y motion
            BOUNCE(grain[i].vy); // and bounce Y velocity
          } else {               // X pixel is taken, so try Y...
            if (!getPixel(grain[i].x / 256, newy / 256, newplane)) { // oldx, newy
              // Pixel is free, take it, but first...
              newx = grain[i].x;   // Cancel X motion
              BOUNCE(grain[i].vx); // and bounce X velocity
            } else {               // Both spots are occupied
              newx = grain[i].x;   // Cancel X & Y motion
              newy = grain[i].y;
              BOUNCE(grain[i].vx); // Bounce X & Y velocity
              BOUNCE(grain[i].vy);
            }
          }
        } else { // Y axis is faster, start there
          if (!getPixel(grain[i].x / 256, newy / 256, newplane)) { // oldx, newy
            // Pixel's free!  Take it!  But...
            newx = grain[i].x;   // Cancel X motion
            BOUNCE(grain[i].vx); // and bounce X velocity
          } else {               // Y pixel is taken, so try X...
            if (!getPixel(newx / 256, grain[i].y / 256, newplane)) { // newx, oldy
              // Pixel is free, take it, but first...
              newy = grain[i].y;   // Cancel Y motion
              BOUNCE(grain[i].vy); // and bounce Y velocity
            } else {               // Both spots are occupied
              newx = grain[i].x;   // Cancel X & Y motion
              newy = grain[i].y;
              BOUNCE(grain[i].vx); // Bounce X & Y velocity
              BOUNCE(grain[i].vy);
            }
          }
        }
      }
    }
    // Clear old spot, update grain pos. to new coords/plane, set new spot
    clearPixel(grain[i].x / 256, grain[i].y / 256, oldplane);
    grain[i].x = newx;
    grain[i].y = newy;
    grain[i].plane = newplane;
    setPixel(newx / 256, newy / 256, newplane);
  }
#endif // end old code

}

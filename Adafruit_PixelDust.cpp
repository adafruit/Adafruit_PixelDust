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
    : width(w), height(h), w8((w + 7) / 8), xMax(w * 256 - 1),
      yMax(h * 256 - 1), n_grains(n), scale(s), elasticity(e), bitmap(NULL),
      grain(NULL), sort(sort) {}

Adafruit_PixelDust::~Adafruit_PixelDust(void) {
  if (bitmap) {
    free(bitmap);
    bitmap = NULL;
  }
  if (grain) {
    free(grain);
    grain = NULL;
  }
}

bool Adafruit_PixelDust::begin(void) {
  if ((bitmap))
    return true; // Already allocated
  if ((bitmap = (uint8_t *)calloc(w8 * height, sizeof(uint8_t)))) {
    if ((!n_grains) || (grain = (Grain *)calloc(n_grains, sizeof(Grain))))
      return true; // Success
    free(bitmap);  // Second alloc failed; free first-alloc data too
    bitmap = NULL;
  }
  return false; // You LOSE, good DAY sir!
}

bool Adafruit_PixelDust::setPosition(grain_count_t i, dimension_t x,
                                     dimension_t y) {
  if (getPixel(x, y))
    return false; // Position already occupied
  setPixel(x, y);
  grain[i].x = x * 256;
  grain[i].y = y * 256;
  return true;
}

void Adafruit_PixelDust::getPosition(grain_count_t i, dimension_t *x,
                                     dimension_t *y) const {
  *x = grain[i].x / 256;
  *y = grain[i].y / 256;
}

// Fill grain structures with random positions, making sure no two are
// in the same location.
void Adafruit_PixelDust::randomize(void) {
  for (grain_count_t i = 0; i < n_grains; i++) {
    while (!setPosition(i, random(width), random(height)))
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

void Adafruit_PixelDust::setPixel(dimension_t x, dimension_t y) {
  bitmap[y * w8 + x / 8] |= pgm_read_byte(&set[x & 7]);
}

void Adafruit_PixelDust::clearPixel(dimension_t x, dimension_t y) {
  bitmap[y * w8 + x / 8] &= pgm_read_byte(&clr[x & 7]);
}

bool Adafruit_PixelDust::getPixel(dimension_t x, dimension_t y) const {
  return bitmap[y * w8 + x / 8] & pgm_read_byte(&set[x & 7]);
}

#else

// Most other architectures will perform better with shifts.

void Adafruit_PixelDust::setPixel(dimension_t x, dimension_t y) {
  bitmap[y * w8 + x / 8] |= (0x80 >> (x & 7));
}

void Adafruit_PixelDust::clearPixel(dimension_t x, dimension_t y) {
  bitmap[y * w8 + x / 8] &= (0x7F7F >> (x & 7));
}

bool Adafruit_PixelDust::getPixel(dimension_t x, dimension_t y) const {
  return bitmap[y * w8 + x / 8] & (0x80 >> (x & 7));
}

#endif // !__AVR__

// Clears bitmap buffer.  Grain positions are unchanged,
// probably want to follow up with some place() calls.
void Adafruit_PixelDust::clear(void) {
  if (bitmap)
    memset(bitmap, 0, w8 * height);
}

#define BOUNCE(n) n = ((-n) * elasticity / 256) ///< 1-axis elastic bounce

// Comparison functions for qsort().  Rather than using true position along
// acceleration vector (which would be computationally expensive), an 8-way
// approximation is 'good enough' and quick to compute.  A separate optimized
// function is provided for each of the 8 directions.

static int compare0(const void *a, const void *b) {
  return ((Grain *)b)->x - ((Grain *)a)->x;
}
static int compare1(const void *a, const void *b) {
  return ((Grain *)b)->x + ((Grain *)b)->y - ((Grain *)a)->x - ((Grain *)a)->y;
}
static int compare2(const void *a, const void *b) {
  return ((Grain *)b)->y - ((Grain *)a)->y;
}
static int compare3(const void *a, const void *b) {
  return ((Grain *)a)->x - ((Grain *)a)->y - ((Grain *)b)->x + ((Grain *)b)->y;
}
static int compare4(const void *a, const void *b) {
  return ((Grain *)a)->x - ((Grain *)b)->x;
}
static int compare5(const void *a, const void *b) {
  return ((Grain *)a)->x + ((Grain *)a)->y - ((Grain *)b)->x - ((Grain *)b)->y;
}
static int compare6(const void *a, const void *b) {
  return ((Grain *)a)->y - ((Grain *)b)->y;
}
static int compare7(const void *a, const void *b) {
  return ((Grain *)b)->x - ((Grain *)b)->y - ((Grain *)a)->x + ((Grain *)a)->y;
}
static int (*compare[8])(const void *a, const void *b) = {
    compare0, compare1, compare2, compare3,
    compare4, compare5, compare6, compare7};

// Calculate one frame of particle interactions
void Adafruit_PixelDust::iterate(int16_t ax, int16_t ay, int16_t az) {

  ax = (int32_t)ax * scale / 256;       // Scale down raw accelerometer
  ay = (int32_t)ay * scale / 256;       // inputs to manageable range.
  az = abs((int32_t)az * scale / 2048); // Z is further scaled down 1:8
  // A tiny bit of random motion is applied to each grain, so that tall
  // stacks of pixels tend to topple (else the whole stack slides across
  // the display).  This is a function of the Z axis input, so it's more
  // pronounced the more the display is tilted (else the grains shift
  // around too much when the display is held level).
  az = (az >= 4) ? 1 : 5 - az; // Clip & invert
  ax -= az;                    // Subtract Z motion factor from X, Y,
  ay -= az;                    // then...
  int16_t az2 = az * 2 + 1;    // max random motion to add back in

  grain_count_t i;

  if (sort) {
    int8_t q;
    q = (int)(atan2(ay, ax) * 8.0 / M_PI); // -8 to +8
    if (q >= 0)
      q = (q + 1) / 2;
    else
      q = (q + 16) / 2;
    if (q > 7)
      q = 7;
    // Sort grains by position, bottom-to-top
    qsort(grain, n_grains, sizeof(Grain), compare[q]);
  }

  // Apply 2D accel vector to grain velocities...
  int32_t v2; // Velocity squared
  float v;    // Absolute velocity
  for (i = 0; i < n_grains; i++) {
    grain[i].vx += ax + random(az2);
    grain[i].vy += ay + random(az2);
    // Terminal velocity (in any direction) is 256 units -- equal to
    // 1 pixel -- which keeps moving grains from passing through each other
    // and other such mayhem.  Though it takes some extra math, velocity is
    // clipped as a 2D vector (not separately-limited X & Y) so that
    // diagonal movement isn't faster than horizontal/vertical.
    v2 =
        (int32_t)grain[i].vx * grain[i].vx + (int32_t)grain[i].vy * grain[i].vy;
    if (v2 > 65536) {      // If v^2 > 65536, then v > 256
      v = sqrt((float)v2); // Velocity vector magnitude
      grain[i].vx = (int)(256.0 * (float)grain[i].vx / v); // Maintain heading &
      grain[i].vy = (int)(256.0 * (float)grain[i].vy / v); // limit magnitude
    }
  }

  // ...then update position of each grain, one at a time, checking for
  // collisions and having them react.  This really seems like it shouldn't
  // work, as only one grain is considered at a time while the rest are
  // regarded as stationary.  Yet this naive algorithm, taking many not-
  // technically-quite-correct steps, and repeated quickly enough,
  // visually integrates into something that somewhat resembles physics.
  // (I'd initially tried implementing this as a bunch of concurrent and
  // "realistic" elastic collisions among circular grains, but the
  // calculations and volume of code quickly got out of hand for both
  // the tiny 8-bit AVR microcontroller and my tiny dinosaur brain.)

  position_t newx, newy;
#ifdef __AVR__
  int16_t oldidx, newidx, delta;
#else
  int32_t oldidx, newidx, delta;
#endif

  for (i = 0; i < n_grains; i++) {
    newx = grain[i].x + grain[i].vx; // New position in grain space
    newy = grain[i].y + grain[i].vy;
    if (newx < 0) {        // If grain would go out of bounds
      newx = 0;            // keep it inside,
      BOUNCE(grain[i].vx); // and bounce off wall
    } else if (newx > xMax) {
      newx = xMax;
      BOUNCE(grain[i].vx);
    }
    if (newy < 0) {
      newy = 0;
      BOUNCE(grain[i].vy);
    } else if (newy > yMax) {
      newy = yMax;
      BOUNCE(grain[i].vy);
    }

    // oldidx/newidx are the prior and new pixel index for this grain.
    // It's a little easier to check motion vs handling X & Y separately.
    oldidx = (grain[i].y / 256) * width + (grain[i].x / 256);
    newidx = (newy / 256) * width + (newx / 256);

    if ((oldidx != newidx) && // If grain is moving to a new pixel...
        getPixel(newx / 256, newy / 256)) { // but if pixel already occupied...
      delta = abs(newidx - oldidx);         // What direction when blocked?
      if (delta == 1) {                     // 1 pixel left or right)
        newx = grain[i].x;                  // Cancel X motion
        BOUNCE(grain[i].vx);                // and bounce X velocity (Y is OK)
      } else if (delta == width) {          // 1 pixel up or down
        newy = grain[i].y;                  // Cancel Y motion
        BOUNCE(grain[i].vy);                // and bounce Y velocity (X is OK)
      } else { // Diagonal intersection is more tricky...
        // Try skidding along just one axis of motion if possible
        // (start w/faster axis).
        if (abs(grain[i].vx) >= abs(grain[i].vy)) {      // X axis is faster
          if (!getPixel(newx / 256, grain[i].y / 256)) { // newx, oldy
            // That pixel's free!  Take it!  But...
            newy = grain[i].y;   // Cancel Y motion
            BOUNCE(grain[i].vy); // and bounce Y velocity
          } else {               // X pixel is taken, so try Y...
            if (!getPixel(grain[i].x / 256, newy / 256)) { // oldx, newy
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
          if (!getPixel(grain[i].x / 256, newy / 256)) { // oldx, newy
            // Pixel's free!  Take it!  But...
            newx = grain[i].x;   // Cancel X motion
            BOUNCE(grain[i].vx); // and bounce X velocity
          } else {               // Y pixel is taken, so try X...
            if (!getPixel(newx / 256, grain[i].y / 256)) { // newx, oldy
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
    clearPixel(grain[i].x / 256, grain[i].y / 256); // Clear old spot
    grain[i].x = newx;                              // Update grain position
    grain[i].y = newy;
    setPixel(newx / 256, newy / 256); // Set new spot
  }
}

#include <Adafruit_Sand.h>

Adafruit_Sand::Adafruit_Sand(dimension_t w, dimension_t h, grain_count_t n,
 uint8_t s, uint8_t e) : width(w), height(h), w4((w + 3) / 4),
 xMax(w * 256 - 1), yMax(h * 256 - 1), n_grains(n), scale(s),
 elasticity(e), bitmap(NULL), grain(NULL) {
}

Adafruit_Sand::~Adafruit_Sand(void) {
  if(bitmap) {
    free(bitmap);
    bitmap = NULL;
  }
  if(grain) {
    free(grain);
    grain  = NULL;
  }
}

bool Adafruit_Sand::begin(void) {
  if((bitmap)) return true; // Already allocated
  if((bitmap = (uint8_t *)calloc(w4 * height, sizeof(uint8_t)))) {
    if((!n_grains) || (grain = (Grain *)calloc(n_grains, sizeof(Grain))))
      return true; // Success
    free(bitmap);  // Second alloc failed; free first-alloc data too
    bitmap = NULL;
  }
  return false;    // You LOSE, good DAY sir!
}

bool Adafruit_Sand::place(grain_count_t i, dimension_t x, dimension_t y) {
  if(readPixel(x, y)) return false; // Position already occupied
  setPixel(x, y, 1);
  grain[i].x = x * 256;
  grain[i].y = y * 256;
  return true;
}

// Fill grain structures with random positions, making sure no two are
// in the same location.
void Adafruit_Sand::randomize(void) {
  for(grain_count_t i=0; i<n_grains; i++) {
    while(!place(i, random(width), random(height)));
  }
}

// Pixel set/read functions for the bitmap buffer

const uint8_t PROGMEM mask[]  = { 0x3F, 0xCF, 0xF3, 0xFC },
                      mul[]   = { 0x40, 0x10, 0x04, 0x01 },
                      shift[] = {    6,    4,    2,    0 };

void Adafruit_Sand::setPixel(dimension_t x, dimension_t y, uint8_t n) {
#ifdef __AVR__
  uint16_t bytenum;
#else
  uint32_t bytenum;
#endif
  bytenum = y * w4 + x / 4;
  uint8_t x3 = x & 3;

  bitmap[bytenum] = (bitmap[bytenum] & pgm_read_byte(&mask[x3])) |
    ((n & 3) * pgm_read_byte(&mul[x3]));
}

void Adafruit_Sand::clearPixel(dimension_t x, dimension_t y) {
  bitmap[y * w4 + x / 4] &= pgm_read_byte(&mask[x & 3]);
}

uint8_t Adafruit_Sand::readPixel(dimension_t x, dimension_t y) const {
  return (bitmap[y * w4 + x / 4] >> pgm_read_byte(&shift[x & 3])) & 3;
}

// Clears bitmap buffer.  Grain positions are unchanged,
// probably want to follow up with some place() calls.
void Adafruit_Sand::clear(void) {
  if(bitmap) memset(bitmap, 0, w4 * height);
}

#define BOUNCE(n) n = ((-n) * elasticity / 256)

// Calculate one frame of sand interactions
void Adafruit_Sand::iterate(int16_t ax, int16_t ay, int16_t az) {

  ax = (int32_t)ax * scale / 256;  // Scale down raw accelerometer
  ay = (int32_t)ay * scale / 256;  // inputs to manageable range.
  az = abs((int32_t)az * scale / 2048); // Z is further scaled down 1:8
  // A tiny bit of random motion is applied to each grain, so that tall
  // stacks of pixels tend to topple (else the whole stack slides across
  // the display).  This is a function of the Z axis input, so it's more
  // pronounced the more the display is tilted (else the grains shift
  // around too much when the display is held level).
  az  = (az >= 4) ? 1 : 5 - az; // Clip & invert
  ax -= az;                     // Subtract Z motion factor from X, Y,
  ay -= az;                     // then...
  int16_t az2 = az * 2 + 1;     // max random motion to add back in

  grain_count_t i;

  // Apply 2D accel vector to grain velocities...
  int32_t v2; // Velocity squared
  float   v;  // Absolute velocity
  for(i=0; i<n_grains; i++) {
    grain[i].vx += ax + random(az2);
    grain[i].vy += ay + random(az2);
    // Terminal velocity (in any direction) is 256 units -- equal to
    // 1 pixel -- which keeps moving grains from passing through each other
    // and other such mayhem.  Though it takes some extra math, velocity is
    // clipped as a 2D vector (not separately-limited X & Y) so that
    // diagonal movement isn't faster than horizontal/vertical.
    v2 = (int32_t)grain[i].vx*grain[i].vx+(int32_t)grain[i].vy*grain[i].vy;
    if(v2 > 65536) { // If v^2 > 65536, then v > 256
      v = sqrt((float)v2); // Velocity vector magnitude
      grain[i].vx = (int)(256.0*(float)grain[i].vx/v); // Maintain heading &
      grain[i].vy = (int)(256.0*(float)grain[i].vy/v); // limit magnitude
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

  for(i=0; i<n_grains; i++) {
    newx = grain[i].x + grain[i].vx; // New position in grain space
    newy = grain[i].y + grain[i].vy;
    if(newx < 0) {         // If grain would go out of bounds
      newx = 0;            // keep it inside,
      BOUNCE(grain[i].vx); // and bounce off wall
    } else if(newx > xMax) {
      newx = xMax;
      BOUNCE(grain[i].vx);
    }
    if(newy < 0) {
      newy = 0;
      BOUNCE(grain[i].vy);
    } else if(newy > yMax) {
      newy = yMax;
      BOUNCE(grain[i].vy);
    }

    // oldidx/newidx are the prior and new pixel index for this grain.
    // It's a little easier to check motion vs handling X & Y separately.
    oldidx = (grain[i].y / 256) * width + (grain[i].x / 256);
    newidx = (newy       / 256) * width + (newx       / 256);

    if((oldidx != newidx) &&        // If grain is moving to a new pixel...
     readPixel(newx/256, newy/256)) { // but if pixel already occupied...
      delta = abs(newidx - oldidx); // What direction when blocked?
      if(delta == 1) {              // 1 pixel left or right)
        newx = grain[i].x;          // Cancel X motion
        BOUNCE(grain[i].vx);        // and bounce X velocity (Y is OK)
      } else if(delta == width) {   // 1 pixel up or down
        newy = grain[i].y;          // Cancel Y motion
        BOUNCE(grain[i].vy);        // and bounce Y velocity (X is OK)
      } else { // Diagonal intersection is more tricky...
        // Try skidding along just one axis of motion if possible
        // (start w/faster axis).
        if(abs(grain[i].vx) >= abs(grain[i].vy)) { // X axis is faster
          if(!readPixel(newx / 256, grain[i].y / 256)) { // newx, oldy
            // That pixel's free!  Take it!  But...
            newy = grain[i].y;      // Cancel Y motion
            BOUNCE(grain[i].vy);    // and bounce Y velocity
          } else { // X pixel is taken, so try Y...
            if(!readPixel(grain[i].x / 256, newy / 256)) { // oldx, newy
              // Pixel is free, take it, but first...
              newx = grain[i].x;    // Cancel X motion
              BOUNCE(grain[i].vx);  // and bounce X velocity
            } else { // Both spots are occupied
              newx = grain[i].x;    // Cancel X & Y motion
              newy = grain[i].y;
              BOUNCE(grain[i].vx);  // Bounce X & Y velocity
              BOUNCE(grain[i].vy);
            }
          }
        } else { // Y axis is faster, start there
          if(!readPixel(grain[i].x / 256, newy / 256)) { // oldx, newy
            // Pixel's free!  Take it!  But...
            newx = grain[i].x;      // Cancel X motion
            BOUNCE(grain[i].vx);    // and bounce X velocity
          } else { // Y pixel is taken, so try X...
            if(!readPixel(newx / 256, grain[i].y / 256)) { // newx, oldy
              // Pixel is free, take it, but first...
              newy = grain[i].y;    // Cancel Y motion
              BOUNCE(grain[i].vy);  // and bounce Y velocity
            } else { // Both spots are occupied
              newx = grain[i].x;    // Cancel X & Y motion
              newy = grain[i].y;
              BOUNCE(grain[i].vx);  // Bounce X & Y velocity
              BOUNCE(grain[i].vy);
            }
          }
        }
      }
    }
    clearPixel(grain[i].x / 256, grain[i].y / 256); // Clear old spot
    grain[i].x = newx;                              // Update grain position
    grain[i].y = newy;
    setPixel(newx / 256, newy / 256, 1);            // Set new spot
  }
}


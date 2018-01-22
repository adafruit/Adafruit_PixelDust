#ifndef _ADAFRUIT_SAND_H_
#define _ADAFRUIT_SAND_H_

#include <Arduino.h>

// The internal representation of sand grains places them in an integer
// coordinate space that's 256X the scale of the pixel grid, allowing them
// to move and interact at sub-pixel increments (so motions appear
// relatively smooth) without having to go all floating-point about it.
// Positions are divided by 256 for pixel display and collision detection.

#ifdef _AVR_
// For better performance and RAM utilization on AVR microcontrollers,
// display is limited to maximum 127x127 pixels and 255 grains of sand.
// You can try overriding either or both here, RAM permitting.
typedef uint8_t dimension_t;    // Pixel dimensions
typedef int16_t position_t;     // 'Sand space' coordinates (256X pixel space)
typedef uint8_t grain_count_t;  // Number of grains
#else
// Anything non-AVR is presumed more capable, maybe a Cortex M0 or other
// 32-bit device.  These go up to 32767x32767 pixels and 65535 grains.
typedef uint16_t dimension_t;   // Pixel dimensions
typedef int32_t  position_t;    // 'Sand space' coordinates (256X pixel space)
typedef uint16_t grain_count_t; // Number of grains
#endif
// Velocity type is same on any architecture -- must allow up to +/- 256
typedef int16_t  velocity_t;

// An array of these structures is allocated in the begin() function,
// one per grain.  8 bytes each on AVR, 12 bytes elsewhere.
typedef struct Grain {
  position_t  x,  y;
  velocity_t vx, vy;
};

class Adafruit_Sand {
 public:
  Adafruit_Sand(dimension_t w, dimension_t h, grain_count_t n, uint8_t s,
    uint8_t e=128);
  ~Adafruit_Sand(void);
  bool begin(void),
       readPixel(dimension_t x, dimension_t y) const,
       place(grain_count_t i, dimension_t x, dimension_t y);
  void setPixel(dimension_t x, dimension_t y),
       clearPixel(dimension_t x, dimension_t y),
       randomize(void),
       iterate(int16_t ax, int16_t ay, int16_t az=0);
 private:
  dimension_t   width, height, w8;
  position_t    xMax, yMax;
  grain_count_t n_grains;
  uint8_t       scale, elasticity, *bitmap;
  Grain        *grain;
};

#endif // _ADAFRUIT_SAND_H_


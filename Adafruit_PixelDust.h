/*!
 * @file Adafruit_PixelDust.h
 *
 * Header file to accompany Adafruit_PixelDust.cpp -- particle simulation
 * for "LED sand" (or dust, or snow or rain or whatever).
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * Written by Phil "PaintYourDragon" Burgess for Adafruit Industries.
 *
 * BSD license, all text here must be included in any redistribution.
 *
 */

#ifndef _ADAFRUIT_PIXELDUST_H_
#define _ADAFRUIT_PIXELDUST_H_

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/*! Remap Arduino-style random() to stdlib-style. */
#define random(X) (random() % X)
#endif

// The internal representation of sand grains places them in an integer
// coordinate space that's 256X the scale of the pixel grid, allowing them
// to move and interact at sub-pixel increments (so motions appear
// relatively smooth) without having to go all floating-point about it.
// Positions are divided by 256 for pixel display and collision detection.

#ifdef __AVR__
// For better performance and RAM utilization on AVR microcontrollers,
// display is limited to maximum 127x127 pixels and 255 grains of sand.
// You can try overriding either or both here, RAM permitting.
typedef uint8_t dimension_t;   ///< Pixel dimensions
typedef int16_t position_t;    ///< 'Sand space' coords (256X pixel space)
typedef uint8_t grain_count_t; ///< Number of grains
#else
// Anything non-AVR is presumed more capable, maybe a Cortex M0 or other
// 32-bit device.  These go up to 32767x32767 pixels and 65535 grains.
typedef uint16_t dimension_t;   ///< Pixel dimensions
typedef int32_t position_t;     ///< 'Sand space' coords (256X pixel space)
typedef uint16_t grain_count_t; ///< Number of grains
#endif
// Velocity type is same on any architecture -- must allow up to +/- 256
typedef int16_t velocity_t; ///< Velocity type

/*!
    @brief Per-grain structure holding position and velocity.
    An array of these structures is allocated in the begin() function,
    one per grain.  8 bytes each on AVR, 12 bytes elsewhere.
*/
typedef struct {
  position_t x;  ///< Horizontal position in 'sand space'
  position_t y;  ///< Vertical position in 'sand space'
  velocity_t vx; ///< Horizontal velocity (-255 to +255) in 'sand space'
  velocity_t vy; ///< Vertical velocity (-255 to +255) in 'sand space'
} Grain;

/*!
    @brief Particle simulation class for "LED sand."
    This handles the "physics engine" part of a sand/rain simulation.
    It does not actually render anything itself and needs to work in
    conjunction with a display library to handle graphics. The term
    "physics" is used loosely here...it's a relatively crude algorithm
    that's appealing to the eye but takes many shortcuts with collision
    detection, etc.
*/
class Adafruit_PixelDust {
public:
  /*!
      @brief Constructor -- allocates the basic Adafruit_PixelDust object,
             this should be followed with a call to begin() to allocate
             additional data structures within.
      @param w    Simulation width in pixels (up to 127 on AVR,
                  32767 on other architectures).
      @param h    Simulation height in pixels (same).
      @param n    Number of sand grains (up to 255 on AVR, 65535 elsewhere).
      @param s    Accelerometer scaling (1-255). The accelerometer X, Y and Z
                  values passed to the iterate() function will be multiplied
                  by this value and then divided by 256, e.g. pass 1 to
                  divide accelerometer input by 256, 128 to divide by 2.
      @param e    Particle elasticity (0-255) (optional, default is 128).
                  This determines the sand grains' "bounce" -- higher numbers
                  yield bouncier particles.
      @param sort If true, particles are sorted bottom-to-top when iterating.
                  Sorting sometimes (not always) makes the physics less
                  "Looney Tunes," as lower particles get out of the way of
                  upper particles.  It can be computationally expensive if
                  there's lots of grains, and isn't good if you're coloring
                  grains by index (because they're constantly reordering).
      @param damping Velocity retention per frame (0-256, 256 = none).
                  Lower values make the grains lose momentum faster, which
                  looks heavier and removes residual bouncing.
  */
  Adafruit_PixelDust(dimension_t w, dimension_t h, grain_count_t n, uint8_t s,
                     uint8_t e = 128, bool sort = false,
                     uint16_t damping = 256);

  /*!
      @brief Destructor -- deallocates memory associated with the
             Adafruit_PixelDust object.
  */
  ~Adafruit_PixelDust(void);

  /*!
      @brief  Allocates additional memory required by the
              Adafruit_PixelDust object before placing elements or
              calling iterate().
      @return True on success (memory allocated), otherwise false.
  */
  bool begin(void);

  /*!
      @brief Sets state of one pixel on the pixel grid. This can be
             used for drawing obstacles for sand to fall around.
             Call this function BEFORE placing any sand grains with
             the place() or randomize() functions.  Setting a pixel
             does NOT place a sand grain there, only marks that
             location as an obstacle.
      @param x Horizontal (x) coordinate (0 to width-1).
      @param y Vertical(y) coordinate (0 to height-1).
                      sand grains in the simulation.
  */
  void setPixel(dimension_t x, dimension_t y);

  /*!
      @brief Clear one pixel on the pixel grid (set to 0).
      @param x Horizontal (x) coordinate (0 to width-1).
      @param y Vertical (y) coordinate (0 to height-1).
  */
  void clearPixel(dimension_t x, dimension_t y);

  /*!
      @brief Clear the pixel grid contents.
  */
  void clear(void);

  /*!
      @brief  Get value of one pixel on the pixel grid.
      @param  x Horizontal (x) coordinate (0 to width-1).
      @param  y Vertical (y) coordinate (0 to height-1).
      @return true if spot occupied by a grain or obstacle,
              otherwise false.
  */
  bool getPixel(dimension_t x, dimension_t y) const;

  /*!
      @brief  Position one sand grain on the pixel grid.
      @param  i Grain index (0 to grains-1).
      @param  x Horizontal (x) coordinate (0 to width-1).
      @param  y Vertical (y) coordinate (0 to height-1).
      @return True on success (grain placed), otherwise false
              (position already occupied)
  */
  bool setPosition(grain_count_t i, dimension_t x, dimension_t y);

  /*!
      @brief  Get Position of one sand grain on the pixel grid.
      @param  i Grain index (0 to grains-1).
      @param  x POINTER to store horizontal (x) coord (0 to width-1).
      @param  y POINTER to store vertical (y) coord (0 to height-1).
  */
  void getPosition(grain_count_t i, dimension_t *x, dimension_t *y) const;

  /*!
      @brief  Get the sub-pixel position of one grain. The integer part of
              each value is the pixel coordinate and the low 8 bits are the
              fraction (8.8 fixed point), which is useful for smoothly
              anti-aliased rendering of moving grains.
      @param  i Grain index (0 to grains-1).
      @param  x POINTER to store horizontal fixed-point position.
      @param  y POINTER to store vertical fixed-point position.
  */
  void getPositionFixed(grain_count_t i, position_t *x,
                        position_t *y) const;

  /*!
      @brief  Get the current velocity of one grain, in 1/256 pixel/frame.
      @param  i Grain index (0 to grains-1).
      @param  vx POINTER to store horizontal velocity.
      @param  vy POINTER to store vertical velocity.
  */
  void getVelocity(grain_count_t i, velocity_t *vx, velocity_t *vy) const;

  /*!
      @brief  Clear the pixel grid AND zero every grain's position and
              velocity. Use this instead of clear() when restarting an
              animation, otherwise grains keep their old velocity.
  */
  void reset(void);

  /*!
      @brief  Switch the simulation to a small internal xorshift PRNG.
              This is deterministic and much faster than the platform
              random(): on ESP32 the default random() costs about 22us per
              call, which dominates iterate() when it is called twice per
              grain per frame.
      @param  seed Non-zero seed (0 is treated as 1).
  */
  void setSeed(uint32_t seed);

  /*!
      @brief  Go back to the platform random() generator (the default).
  */
  void useSystemRandom(void);

  /*!
      @brief  Set velocity damping per frame after construction.
      @param  damping 0-256, 256 = no damping.
  */
  void setDamping(uint16_t damping);

  /*!
      @brief Randomize grain coordinates. This assigns random starting
             locations to every grain in the simulation, making sure
             they do not overlap or occupy obstacle pixels placed with
             the setPixel() function. The pixel grid should first be
             cleared with the begin() or clear() functions and any
             obstacles then placed with setPixel(); don't randomize()
             on an already-active field.
  */
  void randomize(void);

  /*!
      @brief Run one iteration (frame) of the particle simulation.
      @param ax Accelerometer X input.
      @param ay Accelerometer Y input.
      @param az Accelerometer Z input (optional, default is 0).
  */
  void iterate(int16_t ax, int16_t ay, int16_t az = 0);

private:
  dimension_t width,      // Width in pixels
      height,             // Height in pixels
      w8;                 // Bitmap scanline bytes ((width + 7) / 8)
  position_t xMax,        // Max X coordinate in grain space
      yMax;               // Max Y coordinate in grain space
  grain_count_t n_grains; // Number of sand grains
  uint8_t scale,          // Accelerometer input scaling = scale/256
      elasticity,         // Grain elasticity (bounce) = elasticity/256
      *bitmap;            // 2-bit-per-pixel bitmap (width padded to byte)
  Grain *grain;           // One per grain, alloc'd in begin()
  bool sort;              // If true, sort bottom-to-top when iterating
  uint16_t damping;       // Velocity retention per frame (256 = none)
  uint32_t rngState;      // 0 = use random(), else xorshift state

  uint32_t nextRandom(void); // Platform random() or internal xorshift
};

#endif // _ADAFRUIT_PIXELDUST_H_

/*!
 * @file Adafruit_Sand.h
 *
 * Header file to accompany Adafruit_Sand.cpp -- particle simulation
 * for "LED sand."
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

#ifndef _ADAFRUIT_SAND_H_
#define _ADAFRUIT_SAND_H_

#include <Arduino.h>

// The internal representation of sand grains places them in an integer
// coordinate space that's 256X the scale of the pixel grid, allowing them
// to move and interact at sub-pixel increments (so motions appear
// relatively smooth) without having to go all floating-point about it.
// Positions are divided by 256 for pixel display and collision detection.

#ifdef __AVR__
// For better performance and RAM utilization on AVR microcontrollers,
// display is limited to maximum 127x127 pixels and 255 grains of sand.
// You can try overriding either or both here, RAM permitting.
typedef uint8_t dimension_t;    ///< Pixel dimensions
typedef int16_t position_t;     ///< 'Sand space' coords (256X pixel space)
typedef uint8_t grain_count_t;  ///< Number of grains
#else
// Anything non-AVR is presumed more capable, maybe a Cortex M0 or other
// 32-bit device.  These go up to 32767x32767 pixels and 65535 grains.
typedef uint16_t dimension_t;   ///< Pixel dimensions
typedef int32_t  position_t;    ///< 'Sand space' coords (256X pixel space)
typedef uint16_t grain_count_t; ///< Number of grains
#endif
// Velocity type is same on any architecture -- must allow up to +/- 256
typedef int16_t  velocity_t;    ///< Velocity type

/*!
    @brief Particle simulation class for "LED sand."
    This handles the "physics engine" part of a sand/rain simulation.
    It does not actually render anything itself and needs to work in
    conjunction with a display library to handle graphics. The term
    "physics" is used loosely here...it's a relatively crude algorithm
    that's appealing to the eye but takes many shortcuts with collision
    detection, etc.
*/
class Adafruit_Sand {
 public:
  /*!
      @brief Constructor -- allocates the basic Adafruit_Sand object,
             this should be followed with a call to begin() to allocate
             additional data structures within.
      @param w Simulation width in pixels (up to 127 on AVR,
               32767 on other architectures).
      @param h Simulation height in pixels (same).
      @param n Number of sand grains (up to 255 on AVR, 65535 elsewhere).
      @param s Accelerometer scaling (1-255). The accelerometer X, Y and Z
               values passed to the iterate() function will be multiplied
               by this value and then divided by 256, e.g. pass 1 to divide
               accelerometer input by 256, 128 to divide by 2.
      @param e Particle elasticity (0-255) (optional, default is 128).
               This determines the sand grains' "bounce" -- higher numbers
               yield bouncier particles.
  */
  Adafruit_Sand(dimension_t w, dimension_t h, grain_count_t n, uint8_t s,
    uint8_t e=128);
  /*!
      @brief Destructor -- deallocates memory associated with the
             Adafruit_Sand object.
  */
  ~Adafruit_Sand(void);
       /*!
           @brief  Allocates additional memory required by the Adafruit_Sand
                   object before placing elements or calling iterate().
           @return True on success (memory allocated), otherwise false.
       */
  bool begin(void),
       /*!
           @brief  Position one sand grain on the pixel grid.
           @param  i Grain index (0 to grains-1).
           @param  x Horizontal (x) coordinate (0 to width-1).
           @param  y Vertical (y) coordinate (0 to height-1).
           @return True on success (grain placed), otherwise false
                   (position already occupied)
       */
       place(grain_count_t i, dimension_t x, dimension_t y);
       /*!
           @brief Sets state of one pixel on the pixel grid. This can be
                  used for drawing obstacles for sand to fall around.
                  Call this function BEFORE placing any sand grains with
                  the place() or randomize() functions.
           @param x Horizontal (x) coordinate (0 to width-1).
           @param y Vertical(y) coordinate (0 to height-1).
           @param n Value: 0=empty, 1=sand grain, 2,3=obstacle. Assigning
                           a pixel a value of '1' does NOT actually place
                           a sand grain there, only sets that location on
                           the grid to a sand-colored value. Use the
                           place() or randomize() functions to position
                           sand grains in the simulation.
       */
  void setPixel(dimension_t x, dimension_t y, uint8_t n),
       /*!
           @brief Clear one pixel on the pixel grid (set to 0). This is
                  equivalent to setPixel(x, y, 0), but slightly faster.
           @param x Horizontal (x) coordinate (0 to width-1).
           @param y Vertical (y) coordinate (0 to height-1).
       */
       clearPixel(dimension_t x, dimension_t y),
       /*!
           @brief Randomize grain coordinates. This assigns random starting
                  locations to every grain in the simulation, making sure
                  they do not overlap or occupy obstacle pixels placed with
                  the setPixel() function. The pixel grid should first be
                  cleared with the begin() or clear() functions and any
                  obstacles then placed with setPixel(); don't randomize()
                  on an already-active field.
       */
       randomize(void),
       /*!
           @brief Run one iteration (frame) of the particle simulation.
           @param ax Accelerometer X input.
           @param ay Accelerometer Y input.
           @param az Accelerometer Z input (optional, default is 0).
       */
       iterate(int16_t ax, int16_t ay, int16_t az=0),
       /*!
           @brief Clear the pixel grid contents.
       */
       clear(void);
  uint8_t
       /*!
           @brief  Get value of one pixel on the pixel grid. Applications
                   that handle device-specific graphics will want to call
                   this function for each pixel of the display. The return
                   value can be used as a color palette index for drawing
                   to a screen.
           @param  x Horizontal (x) coordinate (0 to width-1).
           @param  y Vertical (y) coordinate (0 to height-1).
           @return 0=empty, 1=sand grain, 2-3=obstacle.
       */
       readPixel(dimension_t x, dimension_t y) const;
 private:
  dimension_t   width,      // Width in pixels
                height,     // Height in pixels
                w4;         // Bitmap scanline bytes ((width + 3) / 4)
  position_t    xMax,       // Max X coordinate in grain space
                yMax;       // Max Y coordinate in grain space
  grain_count_t n_grains;   // Number of sand grains
  uint8_t       scale,      // Accelerometer input scaling = scale/256
                elasticity, // Grain elasticity (bounce) = elasticity/256
               *bitmap;     // 2-bit-per-pixel bitmap (width padded to byte)
  struct Grain {            // An array of these structures is allocated in
    position_t  x,  y;      // the begin() function, one per grain.  8 bytes
    velocity_t vx, vy;      // each on AVR, 12 bytes elsewhere.
  } *grain;
};

#endif // _ADAFRUIT_SAND_H_


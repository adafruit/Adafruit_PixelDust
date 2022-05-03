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
  @brief  Per-grain structure holding position and velocity. An array of
          these structures is allocated in the begin() function, one per
          grain. 8 bytes each on AVR, 12 bytes elsewhere.
*/
typedef struct {
  position_t x;       ///< Horizontal position in 'sand space' within plane
  position_t y;       ///< Vertical position in 'sand space' within plane
  signed vx : 12;     ///< Horizontal velocity (-255 to +255) in 'sand space'
  signed vy : 12;     ///< Vertical velocity (-255 to +255) in 'sand space'
  unsigned plane : 8; ///< Corresponding plane index (always 0 if single-plane)
  // Though 9 bits would suffice for vx & vy, 12 bits are used to encourage
  // nybble alignment on AVR, as arbitrary shifts are less efficient there.
  // Unlikely to need so many plane bits, but again, alignment. If status
  // bits or other data is needed in the future, can swipe from these.
} Grain;

/** The four edges of a rectangular plane within its native coordinate
    system orientation. */
typedef enum {
  EDGE_TOP = 0,
  EDGE_LEFT,
  EDGE_RIGHT,
  EDGE_BOTTOM,
  EDGE_NONE
} Direction;

/*!
  @brief  Each rectangular plane has 4 of these structures, indicating
          the connecting plane(s) off each of four sides, and the
          corresponding edge there.
*/
typedef struct {
  uint8_t plane;  ///< Index of plane off this side
  Direction side; ///< Which side this connects to on 'plane'
} Edge;

/*!
  @brief  Per-plane structure holding size and topology. Each PixelDust
          simulation is comprised of one or more planes. These structures
          are provided by user code. Additional data is computed on startup
          and resides in a PlaneDerived struct.
*/
typedef struct {
  dimension_t width;  ///< Plane width in pixels
  dimension_t height; ///< Plane height in pixels
  float x_vec[3];     ///< +X axis vector
  float y_vec[3];     ///< +Y axis vector
  Edge link[4];       ///< Details of plane off each of 4 sides
} Plane;

/*!
  @brief  Per-plane structure with additional data that's calculated by
          the library, no need for user to specify. Original user data is
          kept in the 'core' element.
*/
typedef struct {
  Plane core;        ///< Copy of original Plane data as passed to library.
  position_t xMax;   ///< Max X coordinate in grain space
  position_t yMax;   ///< Max Y coordinate in grain space
  float z_vec[3];    ///< Surface normal (derived from core X,Y vectors)
  uint8_t *bitmap;   ///< 2-bit-per-pixel bitmap (width padded to byte)
  dimension_t w8;    ///< Bitmap scanline bytes ((width + 7) / 8)
  uint16_t accel[3]; ///< Transformed accelerometer X/Y/Z2
} PlaneDerived;

/*!
  @brief  Particle simulation class for "LED sand."
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
    @brief  Constructor for single-plane PixelDust simulation -- allocates
            the basic Adafruit_PixelDust object, this should be followed
            with a call to begin() to allocate additional data structures
            within.
    @param  w     Simulation width in pixels (up to 127 on AVR,
                  32767 on other architectures).
    @param  h     Simulation height in pixels (same).
    @param  n     Number of sand grains (up to 255 on AVR, 65535 elsewhere).
    @param  s     Accelerometer scaling (1-255). The accelerometer X, Y and Z
                  values passed to the iterate() function will be multiplied
                  by this value and then divided by 256, e.g. pass 1 to
                  divide accelerometer input by 256, 128 to divide by 2.
    @param  e     Particle elasticity (0-255) (optional, default is 128).
                  This determines the sand grains' "bounce" -- higher numbers
                  yield bouncier particles.
    @param  sort  Ignored but present for compatibility with old projects.
  */
  Adafruit_PixelDust(dimension_t w, dimension_t h, grain_count_t n, uint8_t s,
                     uint8_t e = 128, bool sort = false);

  /*!
    @brief  Constructor for multi-plane PixelDust simulation -- allocates
            the basic Adafruit_PixelDust object, this should be followed
            with a call to begin() to allocate additional data structures
            within.
    @param  n  Number of sand grains (up to 255 on AVR, 65535 elsewhere).
    @param  s  Accelerometer scaling (1-255). The accelerometer X, Y and Z
               values passed to the iterate() function will be multiplied
               by this value and then divided by 256, e.g. pass 1 to divide
               accelerometer input by 256, 128 to divide by 2.
    @param  e  Particle elasticity (0-255) (optional, default is 128). This
               determines the sand grains' "bounce" -- higher numbers yield
               bouncier particles.
    @note   This is a distinct constructor declaration (rather than putting
            w & h at end of arguments with default values and having one
            constructor) because multi-plane dust was a later addition but
            we'd like to keep compatibility with existing projects.
  */
  Adafruit_PixelDust(grain_count_t n, uint8_t s, uint8_t e = 128);

  /*!
      @brief Destructor -- deallocates memory associated with the
             Adafruit_PixelDust object.
  */
  ~Adafruit_PixelDust(void);

  /*!
    @brief  Allocates additional memory required by the Adafruit_PixelDust
            object before placing elements or calling iterate().
    @param  plane  Array of Plane structures, which describe the size,
                   orientation and connections between planes.
    @param  np     Number of elements in the plane[] array.
    @return True on success (memory allocated), otherwise false.
  */
  bool begin(Plane *plane = NULL, uint8_t np = 0);

  /*!
    @brief  Sets state of one pixel on the pixel grid. This can be used for
            drawing obstacles for sand to fall around. Call this function
            BEFORE placing any sand grains with the place() or randomize()
            functions. Setting a pixel does NOT place a sand grain there,
            only marks that location as an obstacle.
    @param  x  Horizontal (x) coordinate (0 to plane width - 1).
    @param  y  Vertical(y) coordinate (0 to plane height - 1).
    @param  p  Plane number (0 if unspecified).
  */
  void setPixel(dimension_t x, dimension_t y, uint8_t p = 0);

  /*!
    @brief  Clear one pixel on the pixel grid (set to 0).
    @param  x  Horizontal (x) coordinate (0 to plane width - 1).
    @param  y  Vertical (y) coordinate (0 to plane height - 1).
    @param  p  Plane number (0 if unspecified).
  */
  void clearPixel(dimension_t x, dimension_t y, uint8_t p = 0);

  /*!
    @brief  Clear the pixel grid contents on all planes.
  */
  void clear(void);

  /*!
    @brief   Get value of one pixel on a pixel grid.
    @param   x  Horizontal (x) coordinate (0 to plane width - 1).
    @param   y  Vertical (y) coordinate (0 to plane height - 1).
    @param   p  Plane number (0 if unspecified).
    @return  true if spot occupied by a grain or obstacle, otherwise false.
  */
  bool getPixel(dimension_t x, dimension_t y, uint8_t p = 0) const;

  /*!
    @brief   Position one sand grain on a pixel grid.
    @param   i  Grain index (0 to grains-1).
    @param   x  Horizontal (x) coordinate (0 to plane width - 1).
    @param   y  Vertical (y) coordinate (0 to plane height - 1).
    @param   p  Plane number (0 if unspecified).
    @return  True on success (grain placed), otherwise false (position
             already occupied)
  */
  bool setPosition(grain_count_t i, dimension_t x, dimension_t y,
                   uint8_t p = 0);

  /*!
    @brief  Get Position of one sand grain on a pixel grid.
    @param  i  Grain index (0 to grains-1).
    @param  x  POINTER to store horizontal (x) coord (0 to plane width - 1).
    @param  y  POINTER to store vertical (y) coord (0 to plane height - 1).
    @param  p  POINTER to store plane number (0-255), or NULL to ignore.
  */
  void getPosition(grain_count_t i, dimension_t *x, dimension_t *y,
                   uint8_t *p = NULL) const;

  /*!
    @brief  Randomize grain coordinates. This assigns random starting
            locations to every grain in the simulation, making sure they do
            not overlap or occupy obstacle pixels placed with the setPixel()
            function. The pixel grid should first be cleared with the
            begin() or clear() functions and any obstacles then placed with
            setPixel(); never randomize() an already-active field.
  */
  void randomize(void);

  /*!
    @brief  Run one iteration (frame) of the particle simulation.
    @param  ax  Accelerometer X input.
    @param  ay  Accelerometer Y input.
    @param  az  Accelerometer Z input (optional, default is 0).
  */
  void iterate(int16_t ax, int16_t ay, int16_t az = 0);

protected:
  /*!
    @brief   Used internally by library to transform grain coordinates from
             one plane to an adjacent plane.
    @param   in   Pointer to incoming Grain structure in own plane.
    @param   out  Pointer to outgoing Grain structure (plane may change).
                  Do not use the same pointer unless it's a single-plane
                  simulation. Calling code might use a temp var for one
                  or the other as needed.
    @return  true if new position is occupied or off edge of simulation,
             false if new position is available. This is backwards from
             what one might expect but is to be compatible with the
             behavior of getPixel().
  */
  bool cross(Grain *in, Grain *out);
  dimension_t single_width;  ///< Width in pixels w/single-plane constructor
  dimension_t single_height; ///< Height in pixels w/single-plane constructor
  grain_count_t n_grains;    ///< Number of sand grains
  uint8_t scale;             ///< Accelerometer input scaling = scale/256
  uint8_t elasticity;        ///< Grain elasticity (bounce) = elasticity/256
  Grain *grain;              ///< One per grain, alloc'd in begin()
  PlaneDerived *plane;       ///< Array of PlaneDerived structures
  uint8_t num_planes;        ///< Number of elements in plane[] array
  uint8_t *bitmaps;          ///< Single alloc'd buffer for all plane bitmaps
};

#endif // _ADAFRUIT_PIXELDUST_H_

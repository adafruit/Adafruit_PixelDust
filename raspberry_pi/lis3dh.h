/*!
 * @file lis3dh.h
 *
 * Header file to accompany lis3dh.cpp -- EXTREMELY MINIMAL support for
 * LIS3DH accelerometer.
 *
 */

#ifndef _LIS3DH_H_
#define _LIS3DH_H_

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define LIS3DH_DEFAULT_ADDRESS 0x18

// Return codes for the begin() function:
#define LIS3DH_OK 0            ///< Success!
#define LIS3DH_ERR_I2C_OPEN 1  ///< I2C open() failed
#define LIS3DH_ERR_I2C_SLAVE 2 ///< I2C ioctl() slave select failed

// These enums don't see much use (yet?).  They're carried over
// from the Arduino library source in case this library is expanded
// with more equivalent functions.

/*!
    @brief Accelerometer range values.  NOT CURRENTLY USED.
    The library initializes the accelerometer to the 4G range
    and just leaves it there.
*/
typedef enum {
  LIS3DH_RANGE_16_G = 0b11, ///< +/- 16g
  LIS3DH_RANGE_8_G = 0b10,  ///< +/- 8g
  LIS3DH_RANGE_4_G = 0b01,  ///< +/- 4g
  LIS3DH_RANGE_2_G = 0b00   ///< +/- 2g (default value)
} lis3dh_range_t;

/*!
    @brief Accelerometer axes.  NOT CURRENTLY USED.
*/
typedef enum {
  LIS3DH_AXIS_X = 0x0, ///< X axis
  LIS3DH_AXIS_Y = 0x1, ///< Y axis
  LIS3DH_AXIS_Z = 0x2, ///< Z axis
} lis3dh_axis_t;

/*!
    @brief Accelerometer data rates.  NOT CURRENTLY USED.
    The library initializes the accelerometer to the 400 Hz rate
    and just leaves it there.
*/
typedef enum {
  LIS3DH_DATARATE_400_HZ = 0b0111,         ///< 400 Hz
  LIS3DH_DATARATE_200_HZ = 0b0110,         ///< 200 Hz
  LIS3DH_DATARATE_100_HZ = 0b0101,         ///< 100 Hz
  LIS3DH_DATARATE_50_HZ = 0b0100,          ///<  50 Hz
  LIS3DH_DATARATE_25_HZ = 0b0011,          ///<  25 Hz
  LIS3DH_DATARATE_10_HZ = 0b0010,          ///<  10 Hz
  LIS3DH_DATARATE_1_HZ = 0b0001,           ///<   1 Hz
  LIS3DH_DATARATE_POWERDOWN = 0,           ///< Power-down sleep state
  LIS3DH_DATARATE_LOWPOWER_1K6HZ = 0b1000, ///< Low-power state 1
  LIS3DH_DATARATE_LOWPOWER_5KHZ = 0b1001,  ///< Low-power state 2
} lis3dh_dataRate_t;

/*!
    @brief Exceedingly pared-down class for the LIS3DH I2C accelerometer.
    This is culled from the Arduino source, with only the barest of
    functions needed for Adafruit_PixelDust to work.
*/
class Adafruit_LIS3DH {
public:
  /*!
      @brief Constructor -- allocates the basic Adafruit_LIS3DH object,
             this should be followed with a call to begin() to initiate
             I2C communication.
  */
  Adafruit_LIS3DH(void);
  /*!
      @brief Constructor -- closes I2C (if needed) and deallocates memory
             associated with an Adafruit_LIS3DH object.
  */
  ~Adafruit_LIS3DH(void);
  /*!
      @brief  Initiates I2C communication with the LIS3DH accelerometer.
      @param  I2C address of device (optional -- uses default 0x18 if
              unspecified).
      @return LIS3DH_OK on success, else one of the LIS3DH_ERR_* values.
  */
  int begin(uint8_t addr = LIS3DH_DEFAULT_ADDRESS);
  /*!
      @brief 'Raw' reading of accelerometer X/Y/Z.
      @param Pointer to integer to receive X acceleration value.
      @param Pointer to integer to receive Y acceleration value.
      @param Pointer to integer to receive Z acceleration value.
  */
  const void accelRead(int *x, int *y, int *z);
  /*!
      @brief Closes I2C communication with accelerometer.
  */
  void end(void);

private:
  int i2c_fd; // I2C file descriptor
  const void writeRegister8(uint8_t reg, uint8_t value);
  const uint8_t readRegister8(uint8_t reg);
};

#endif // _LIS3DH_H_

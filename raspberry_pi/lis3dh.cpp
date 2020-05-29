/*!
 * @file lis3dh.h
 *
 * EXTREMELY MINIMAL support for LIS3DH accelerometer.
 * This is culled from the Arduino source, with only the barest of
 * functions needed for Adafruit_PixelDust to work.
 *
 */

#include "lis3dh.h"

// Only a few LIS3DH registers are currently referenced
#define LIS3DH_REG_TEMPCFG 0x1F
#define LIS3DH_REG_CTRL1 0x20
#define LIS3DH_REG_CTRL4 0x23
#define LIS3DH_REG_OUT_X_L 0x28

Adafruit_LIS3DH::Adafruit_LIS3DH(void) : i2c_fd(-1) {}

Adafruit_LIS3DH::~Adafruit_LIS3DH(void) { end(); }

int Adafruit_LIS3DH::begin(uint8_t addr) {
  if ((i2c_fd = open("/dev/i2c-1", O_RDWR)) < 0)
    return LIS3DH_ERR_I2C_OPEN;

  if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0)
    return LIS3DH_ERR_I2C_SLAVE;

  writeRegister8(LIS3DH_REG_CTRL1,
                 0x07 | // Enable all axes, normal mode
                     (LIS3DH_DATARATE_400_HZ << 4)); // 400 Hz rate

  // High res & BDU enabled
  writeRegister8(LIS3DH_REG_CTRL4, 0x88);

  // Enable ADCs
  writeRegister8(LIS3DH_REG_TEMPCFG, 0x80);

  uint8_t r = readRegister8(LIS3DH_REG_CTRL4);
  r &= ~(0x30);
  r |= LIS3DH_RANGE_4_G << 4;
  writeRegister8(LIS3DH_REG_CTRL4, r);

  return LIS3DH_OK;
}

const void Adafruit_LIS3DH::writeRegister8(uint8_t reg, uint8_t value) {
  uint8_t buf[2];
  buf[0] = reg;
  buf[1] = value;
  write(i2c_fd, buf, 2);
}

const uint8_t Adafruit_LIS3DH::readRegister8(uint8_t reg) {
  uint8_t result;
  write(i2c_fd, &reg, 1);
  read(i2c_fd, &result, 1);
  return result;
}

const void Adafruit_LIS3DH::accelRead(int *x, int *y, int *z) {
  uint8_t buf[6];
  buf[0] = LIS3DH_REG_OUT_X_L | 0x80; // 0x80 for autoincrement
  write(i2c_fd, buf, 1);
  read(i2c_fd, &buf, 6);
  *x = (buf[0] | ((int)buf[1] << 8));
  *y = (buf[2] | ((int)buf[3] << 8));
  *z = (buf[4] | ((int)buf[5] << 8));
}

void Adafruit_LIS3DH::end(void) {
  if (i2c_fd >= 0) {
    close(i2c_fd);
    i2c_fd = -1;
  }
}

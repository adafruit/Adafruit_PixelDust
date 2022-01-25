//--------------------------------------------------------------------------
// Animated 'sand' for Adafruit Feather.  Uses the following parts:
//   - Feather 32u4 Basic Proto (adafruit.com/product/2771)
//   - Charlieplex FeatherWing (adafruit.com/product/2965 - any color!)
//   - LIS3DH accelerometer (2809)
//   - 350 mAh LiPoly battery (2750)
//   - SPDT Slide Switch (805)
//
// This is NOT good "learn from" code for the IS31FL3731; it is "squeeze
// every last byte from the microcontroller" code.  If you're starting out,
// download the Adafruit_IS31FL3731 and Adafruit_GFX libraries, which
// provide functions for drawing pixels, lines, etc.
//--------------------------------------------------------------------------

#include <Wire.h>               // For I2C communication
#include <Adafruit_LIS3DH.h>    // For accelerometer
#include <Adafruit_PixelDust.h> // For simulation

#define DISP_ADDR  0x74 // Charlieplex FeatherWing I2C address
#define ACCEL_ADDR 0x18 // Accelerometer I2C address
#define N_GRAINS     20 // Number of grains of sand
#define WIDTH        15 // Display width in pixels
#define HEIGHT        7 // Display height in pixels
#define MAX_FPS      45 // Maximum redraw rate, frames/second

// Sand object, last 2 args are accelerometer scaling and grain elasticity
Adafruit_PixelDust sand(WIDTH, HEIGHT, N_GRAINS, 1, 128);

// Since we're not using the GFX library, it's necessary to buffer the
// display contents ourselves (8 bits/pixel with the Charlieplex drivers).
uint8_t pixelBuf[WIDTH * HEIGHT];

Adafruit_LIS3DH accel      = Adafruit_LIS3DH();
uint32_t        prevTime   = 0;      // Used for frames-per-second throttle
uint8_t         backbuffer = 0;      // Index for double-buffered animation

const uint8_t PROGMEM remap[] = {    // In order to redraw the screen super
   0, 90, 75, 60, 45, 30, 15,  0,    // fast, this sketch bypasses the
       0,  0,  0,  0,  0,  0,  0, 0, // Adafruit_IS31FL3731 library and
   0, 91, 76, 61, 46, 31, 16,   1,   // writes to the LED driver directly.
      14, 29, 44, 59, 74, 89,104, 0, // But this means we need to do our
   0, 92, 77, 62, 47, 32, 17,  2,    // own coordinate management, and the
      13, 28, 43, 58, 73, 88,103, 0, // layout of pixels on the Charlieplex
   0, 93, 78, 63, 48, 33, 18,  3,    // Featherwing is strange! This table
      12, 27, 42, 57, 72, 87,102, 0, // remaps LED register indices in
   0, 94, 79, 64, 49, 34, 19,  4,    // sequence to the corresponding pixel
      11, 26, 41, 56, 71, 86,101, 0, // indices in pixelBuf[].
   0, 95, 80, 65, 50, 35, 20,  5,
      10, 25, 40, 55, 70, 85,100, 0,
   0, 96, 81, 66, 51, 36, 21,  6,
       9, 24, 39, 54, 69, 84, 99, 0,
   0, 97, 82, 67, 52, 37, 22,  7,
       8, 23, 38, 53, 68, 83, 98
};

// IS31FL3731-RELATED FUNCTIONS --------------------------------------------

// Begin I2C transmission and write register address (data then follows)
uint8_t writeRegister(uint8_t n) {
  Wire.beginTransmission(DISP_ADDR);
  Wire.write(n); // No endTransmission() - left open for add'l writes
  return 2;      // Always returns 2; count of I2C address + register byte n
}

// Select one of eight IS31FL3731 pages, or the Function Registers
void pageSelect(uint8_t n) {
  writeRegister(0xFD); // Command Register
  Wire.write(n);       // Page number (or 0xB = Function Registers)
  Wire.endTransmission();
}

// SETUP - RUNS ONCE AT PROGRAM START --------------------------------------

void err(int x) {
  uint8_t i;
  pinMode(LED_BUILTIN, OUTPUT);       // Using onboard LED
  for(i=1;;i++) {                     // Loop forever...
    digitalWrite(LED_BUILTIN, i & 1); // LED on/off blink to alert user
    delay(x);
  }
}

void setup(void) {
  uint8_t i, j, bytes;

  if(!sand.begin())            err(1000); // Slow blink = malloc error
  if(!accel.begin(ACCEL_ADDR)) err(250);  // Fast blink = I2C error

  accel.setRange(LIS3DH_RANGE_4_G); // Select accelerometer +/- 4G range

  Wire.setClock(400000); // Run I2C at 400 KHz for faster screen updates

  // Initialize IS31FL3731 Charlieplex LED driver "manually"...
  pageSelect(0x0B);                        // Access the Function Registers
  writeRegister(0);                        // Starting from first...
  for(i=0; i<13; i++) Wire.write(10 == i); // Clear all except Shutdown
  Wire.endTransmission();
  for(j=0; j<2; j++) {                     // For each page used (0 & 1)...
    pageSelect(j);                         // Access the Frame Registers
    for(bytes=i=0; i<180; i++) {           // For each register...
      if(!bytes) bytes = writeRegister(i); // Buf empty? Start xfer @ reg i
      Wire.write(0xFF * (i < 18));         // 0-17 = enable, 18+ = blink+PWM
      if(++bytes >= 32) bytes = Wire.endTransmission();
    }
    if(bytes) Wire.endTransmission();      // Write any data left in buffer
  }

  memset(pixelBuf, 0, sizeof(pixelBuf));   // Clear pixel buffer

  // Draw an obstacle for sand to move around
  for(uint8_t y=0; y<3; y++) {
    for(uint8_t x=0; x<3; x++) {
      sand.setPixel(6+x, 2+y);             // Set pixel in the simulation
      pixelBuf[(2+y) * WIDTH + 6+x] = 5;   // Set pixel in pixelBuf[]
    }
  }

  sand.randomize(); // Initialize random sand positions
}

// MAIN LOOP - RUNS ONCE PER FRAME OF ANIMATION ----------------------------

// Background = black, sand = 80 brightness, obstacles = 5 brightness
const uint8_t PROGMEM color[] = { 0, 80, 5, 5 };

void loop() {
  // Limit the animation frame rate to MAX_FPS.  Because the subsequent sand
  // calculations are non-deterministic (don't always take the same amount
  // of time, depending on their current states), this helps ensure that
  // things like gravity appear constant in the simulation.
  uint32_t t;
  while(((t = micros()) - prevTime) < (1000000L / MAX_FPS));
  prevTime = t;

  // Display frame rendered on prior pass.  It's done immediately after the
  // FPS sync (rather than after rendering) for consistent animation timing.
  pageSelect(0x0B);       // Function registers
  writeRegister(0x01);    // Picture Display reg
  Wire.write(backbuffer); // Page # to display
  Wire.endTransmission();
  backbuffer = 1 - backbuffer; // Swap front/back buffer index

  // Erase old grain positions in pixelBuf[]
  uint8_t     i;
  dimension_t x, y;
  for(i=0; i<N_GRAINS; i++) {
    sand.getPosition(i, &x, &y);
    pixelBuf[y * WIDTH + x] = 0;
  }

  // Read accelerometer...
  accel.read();
  // Run one frame of the simulation
  // X & Y axes are flipped around here to match physical mounting
  sand.iterate(-accel.y, accel.x, accel.z);

  // Draw new grain positions in pixelBuf[]
  for(i=0; i<N_GRAINS; i++) {
    sand.getPosition(i, &x, &y);
    pixelBuf[y * WIDTH + x] = 80;
  }

  // Update pixel data in LED driver
  uint8_t bytes, *ptr = (uint8_t *)remap;
  pageSelect(backbuffer); // Select background buffer
  for(i=bytes=0; i<sizeof(remap); i++) {
    if(!bytes) bytes = writeRegister(0x24 + i);
    Wire.write(pixelBuf[pgm_read_byte(ptr++)]);
    if(++bytes >= 32) bytes = Wire.endTransmission();
  }
  if(bytes) Wire.endTransmission();
}


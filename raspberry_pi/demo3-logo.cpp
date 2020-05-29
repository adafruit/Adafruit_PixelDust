/*!
 * @file demo3-logo.cpp
 *
 * Still-more-complex example for Adafruit_PixelDust on Raspberry Pi.
 * Places a raster obstacle in the middle of the playfield, sand is
 * multi-colored.
 * REQUIRES rpi-rgb-led-matrix LIBRARY!
 * I2C MUST BE ENABLED using raspi-config!
 *
 * This demo DOES NOT YET ADAPT automatically to 32x32 matrix!
 * Parts of this are currently hardcoded for a 64x64 matrix.
 *
 */

#ifndef ARDUINO // Arduino IDE sometimes aggressively builds subfolders

#include "Adafruit_PixelDust.h"
#include "led-matrix-c.h"
#include "lis3dh.h"
#include <signal.h>

#include "logo.h" // This contains the obstacle bitmaps

#define N_GRAINS (8 * 8 * 8) ///< Number of grains of sand on 64x64 matrix

struct RGBLedMatrix *matrix = NULL;
Adafruit_LIS3DH lis3dh;
volatile bool running = true;
int nGrains = N_GRAINS; // Runtime grain count (adapts to res)

uint8_t colors[][3] = { // Sand grain colors, 8 groups...
    0,   0,   0,        // Black
    120, 79,  23,       // Brown
    228, 3,   3,        // Red
    255, 140, 0,        // Orange
    255, 237, 0,        // Yellow
    0,   128, 38,       // Green
    0,   77,  255,      // Blue
    117, 7,   135};     // Purple

#define BG_RED 0 // Background color (r,g,b)
#define BG_GREEN 20
#define BG_BLUE 80

// Signal handler allows matrix to be properly deinitialized.
int sig[] = {SIGHUP,  SIGINT, SIGQUIT, SIGABRT,
             SIGKILL, SIGBUS, SIGSEGV, SIGTERM};
#define N_SIGNALS (int)(sizeof sig / sizeof sig[0])

void irqHandler(int dummy) {
  if (matrix) {
    led_matrix_delete(matrix);
    matrix = NULL;
  }
  for (int i = 0; i < N_SIGNALS; i++)
    signal(sig[i], NULL);
  running = false;
}

int main(int argc, char **argv) {
  struct RGBLedMatrixOptions options;
  struct LedCanvas *canvas;
  int width, height, i, xx, yy, zz;
  Adafruit_PixelDust *sand = NULL;
  dimension_t x, y;

  for (i = 0; i < N_SIGNALS; i++)
    signal(sig[i], irqHandler); // ASAP!

  // Initialize LED matrix defaults
  memset(&options, 0, sizeof(options));
  options.rows = 64;
  options.cols = 64;
  options.chain_length = 1;

  // Parse command line input.  --led-help lists options!
  matrix = led_matrix_create_from_options(&options, &argc, &argv);
  if (matrix == NULL)
    return 1;

  // Create offscreen canvas for double-buffered animation
  canvas = led_matrix_create_offscreen_canvas(matrix);
  led_canvas_get_size(canvas, &width, &height);
  fprintf(stderr, "Size: %dx%d. Hardware gpio mapping: %s\n", width, height,
          options.hardware_mapping);

  if (lis3dh.begin()) {
    puts("LIS3DH init failed");
    return 2;
  }

  // For this demo, the last argument to the PixelDust constructor
  // is set 'false' so the grains are not sorted each frame.
  // This is because the grains have specific colors by index
  // (sorting would mess that up).
  sand = new Adafruit_PixelDust(width, height, nGrains, 1, 64, false);
  if (!sand->begin()) {
    puts("PixelDust init failed");
    return 3;
  }

  // Set up the logo bitmap obstacle in the PixelDust playfield
  int x1 = (width - LOGO_WIDTH) / 2;
  int y1 = (height - LOGO_HEIGHT) / 2;
  for (y = 0; y < LOGO_HEIGHT; y++) {
    for (x = 0; x < LOGO_WIDTH; x++) {
      uint8_t c = logo_mask[y][x / 8];
      if (c & (0x80 >> (x & 7))) {
        sand->setPixel(x1 + x, y1 + y);
      }
    }
  }

  // Set up initial sand coordinates, in 8x8 blocks
  int n = 0;
  for (i = 0; i < 8; i++) {
    xx = i * width / 8;
    yy = height * 7 / 8;
    for (y = 0; y < 8; y++) {
      for (x = 0; x < 8; x++) {
        sand->setPosition(n++, xx + x, yy + y);
      }
    }
  }

  while (running) {
    // Read accelerometer...
    lis3dh.accelRead(&xx, &yy, &zz);

    // Run one frame of the simulation.  Axis flip here
    // depends how the accelerometer is mounted relative
    // to the LED matrix.
    sand->iterate(-xx, -yy, zz);

    // led_canvas_fill() doesn't appear to work properly
    // with the --led-rgb-sequence option...so clear the
    // background manually with a bunch of set_pixel() calls...
    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
        led_canvas_set_pixel(canvas, x, y, BG_RED, BG_GREEN, BG_BLUE);
      }
    }
    // Alpha-blend the logo (white) atop the background...
    for (y = 0; y < LOGO_HEIGHT; y++) {
      for (x = 0; x < LOGO_WIDTH; x++) {
        int a1 = logo_gray[y][x] + 1, a2 = 257 - a1;
        led_canvas_set_pixel(
            canvas, x1 + x, y1 + y, (255 * a1 + BG_RED * a2) >> 8,
            (255 * a1 + BG_GREEN * a2) >> 8, (255 * a1 + BG_BLUE * a2) >> 8);
      }
    }

    // Draw new sand atop canvas
    for (i = 0; i < nGrains; i++) {
      sand->getPosition(i, &x, &y);
      int n = i / 64; // Color index
      led_canvas_set_pixel(canvas, x, y, colors[n][0], colors[n][1],
                           colors[n][2]);
    }

    // Update matrix contents on next vertical sync
    // and provide a new canvas for the next frame.
    canvas = led_matrix_swap_on_vsync(matrix, canvas);
  }

  return 0;
}

#endif // !ARDUINO

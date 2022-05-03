/*!
 * @file FILENAME.cpp
 *
 * Cube for Adafruit_PixelDust on Raspberry Pi.
 * REQUIRES rpi-rgb-led-matrix LIBRARY!
 * I2C MUST BE ENABLED using raspi-config!
 *
 */

#ifndef ARDUINO // Arduino IDE sometimes aggressively builds subfolders

#include <signal.h>
#include <led-matrix.h>
#include "Adafruit_PixelDust.h"
#include "lis3dh.h"
#include "logo.h" // This contains the obstacle bitmaps

using namespace rgb_matrix;

uint8_t colors[][3] = { // Sand grain colors...
 { 255,   0,   0 }, // Red
 { 255, 255,   0 }, // Yellow
 {   0, 255,   0 }, // Green
 {   0, 255, 255 }, // Cyan
 {   0,   0, 255 }, // Blue
 { 255,   0, 255 }, // Magenta
};
#define N_COLORS         (sizeof(colors) / sizeof(colors[0]))
#define GRAINS_PER_COLOR 600
#define TOTAL_GRAINS     (N_COLORS * GRAINS_PER_COLOR)

RGBMatrix *matrix;
FrameCanvas *canvas;
Adafruit_LIS3DH lis3dh;
volatile bool running = true;

Plane plane[] = {
  { 64, 64, {  0, -1,  0 }, { -1,  0,  0 }, {{1, EDGE_LEFT  }, {2, EDGE_TOP   }, {4, EDGE_TOP   }, {3, EDGE_RIGHT }} },
  { 64, 64, {  0,  0,  1 }, {  0, -1,  0 }, {{2, EDGE_LEFT  }, {0, EDGE_TOP   }, {5, EDGE_TOP   }, {4, EDGE_RIGHT }} },
  { 64, 64, { -1,  0,  0 }, {  0,  0,  1 }, {{0, EDGE_LEFT  }, {1, EDGE_TOP   }, {3, EDGE_TOP   }, {5, EDGE_RIGHT }} },
  { 64, 64, {  0,  0, -1 }, {  0, -1,  0 }, {{2, EDGE_RIGHT }, {5, EDGE_BOTTOM}, {0, EDGE_BOTTOM}, {4, EDGE_LEFT  }} },
  { 64, 64, {  1,  0,  0 }, {  0,  0, -1 }, {{0, EDGE_RIGHT }, {3, EDGE_BOTTOM}, {1, EDGE_BOTTOM}, {5, EDGE_LEFT  }} },
  { 64, 64, {  0, -1,  0 }, {  1,  0,  0 }, {{1, EDGE_RIGHT }, {4, EDGE_BOTTOM}, {2, EDGE_BOTTOM}, {3, EDGE_LEFT  }} },
};
#define NUM_PLANES (sizeof(plane) / sizeof(plane[0]))

// Signal handler allows matrix to be properly deinitialized.
int sig[] = {SIGHUP,  SIGINT, SIGQUIT, SIGABRT,
             SIGKILL, SIGBUS, SIGSEGV, SIGTERM};
#define N_SIGNALS (int)(sizeof sig / sizeof sig[0])

void irqHandler(int dummy) {
  for (int i = 0; i < N_SIGNALS; i++)
    signal(sig[i], NULL);
  running = false;
}

int main(int argc, char **argv) {
  RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_opt;
  int i, xx, yy, zz;
  Adafruit_PixelDust *sand = NULL;
  dimension_t x, y;

  for (i = 0; i < N_SIGNALS; i++)
    signal(sig[i], irqHandler); // ASAP!

  // Initialize LED matrix defaults
  matrix_options.rows = matrix_options.cols = 64;
  matrix_options.chain_length = 6;
  runtime_opt.gpio_slowdown = 4; // For Pi 4 w/6 matrices

  // Parse matrix-related command line options first
  if (!ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt)) {
    return 1;
  }

  // Parse command line input.  --led-help lists options!
  if (!(matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt))) {
    fprintf(stderr, "%s: couldn't create matrix object\n", argv[0]);
    return 1;
  }

  // Create offscreen canvas for double-buffered animation
  if (!(canvas = matrix->CreateFrameCanvas())) {
    fprintf(stderr, "%s: couldn't create canvas object\n", argv[0]);
    return 1;
  }

  sand = new Adafruit_PixelDust(64, 64, TOTAL_GRAINS, 1, 180);
  if (!sand->begin(plane, NUM_PLANES)) {
    puts("PixelDust init failed");
    return 2;
  }

  if (lis3dh.begin()) {
    puts("LIS3DH init failed");
    return 3;
  }

  // Set up the logo bitmap obstacle in the PixelDust playfield
  int x1 = (64 - LOGO_WIDTH) / 2;
  int y1 = (64 - LOGO_HEIGHT) / 2;
  for (int p = 0; p < (int)NUM_PLANES; p++) {
    for (y = 0; y < LOGO_HEIGHT; y++) {
      for (x = 0; x < LOGO_WIDTH; x++) {
        uint8_t c = logo_mask[y][x / 8];
        if (c & (0x80 >> (x & 7))) {
          sand->setPixel(x1 + x, y1 + y, p);
        }
      }
    }
  }
  sand->randomize(); // Initialize random particle positions

  while (running) {

// Clear canvas and draw pixels in old positions at half brightness
    canvas->Clear();
    // ADD DRAWING HERE
    for (i = 0; i < (int)TOTAL_GRAINS; i++) {
      uint8_t p;
      sand->getPosition(i, &x, &y, &p);
      int xoffset = p * 64;
      int index = i / GRAINS_PER_COLOR;
      canvas->SetPixel(xoffset + x, y, colors[index][0] / 4, colors[index][1] / 4, colors[index][2] / 4);
    }


    lis3dh.accelRead(&xx, &yy, &zz);
    // Run one frame of the simulation. Use accelerometer
    // readings directly, no axis flips needed, global
    // cube vector data is all relative to accelerometer.
    sand->iterate(xx, yy, zz);

    // Draw logo bitmaps for each face
    for (int p = 0; p < (int)NUM_PLANES; p++) {
      int xoffset = p * 64 + x1;
      for (y = 0; y < LOGO_HEIGHT; y++) {
        for (x = 0; x < LOGO_WIDTH; x++) {
          uint8_t a = logo_gray[y][x];
          canvas->SetPixel(xoffset + x, y1 + y, a, a, a);
        }
      }
    }

    // Erase canvas and draw new sand positions
    for (i = 0; i < (int)TOTAL_GRAINS; i++) {
      uint8_t p;
      sand->getPosition(i, &x, &y, &p);
      int xoffset = p * 64;
      int index = i / GRAINS_PER_COLOR;
      canvas->SetPixel(xoffset + x, y, colors[index][0], colors[index][1], colors[index][2]);
    }

    // Update matrix contents on next vertical sync
    // and provide a new canvas for the next frame.
    canvas = matrix->SwapOnVSync(canvas);
  }

  delete matrix;

  return 0;
}

#endif // !ARDUINO

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <cmath>

// panel dimensions & hall‐sensor pin
#define PANEL_WIDTH   64
#define PANEL_HEIGHT  32
#define HALL_PIN      35

MatrixPanel_I2S_DMA *display;
volatile unsigned long lastTrigger = 0;
unsigned long rotationPeriod = 1000000;
const unsigned long pulseWidth = 4000;

// cube center + velocities
int x = 10, y = 15, z = 0;
int vel_x = 1, vel_y = 2, vel_z = 3;

// track last on/off state so we only redraw on change
bool lastPulseState = false;

// draw color (gets set on bounce)
uint8_t r = 255, g = 255, b = 255;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("POV Starting");
  pinMode(HALL_PIN, INPUT_PULLUP);

  HUB75_I2S_CFG cfg;
  cfg.double_buff      = true;
  cfg.i2sspeed         = HUB75_I2S_CFG::HZ_20M;
  cfg.driver           = HUB75_I2S_CFG::FM6126A;
  cfg.min_refresh_rate = 120;

  display = new MatrixPanel_I2S_DMA(cfg);
  if (!display->begin()) {
    Serial.println("Display init failed");
    while (1);
  }
  display->setBrightness(255);
  lastTrigger = micros();
}

// return the normalized [0..1) delay for voxel at (xi, zi)
float computeDelayNorm(int xi, int zi) {
  float ang = atan2((float)xi, (float)zi);
  if (ang < 0) ang += 2 * M_PI;
  return ang / (2 * M_PI);
}

// test if a given voxel slice is in its pulse window (with wrap)
bool sliceOnAtPhase(int dx, int dz, float phase, float windowFrac) {
  float dly = computeDelayNorm(x + dx, z + dz);

  // front‐half pulse
  if (phase >= dly && phase < dly + windowFrac) return true;

  // back‐half pulse, wrapped
  float d2 = dly + 0.5f;
  if (d2 >= 1.0f) d2 -= 1.0f;
  float end = d2 + windowFrac;
  if (end < 1.0f) {
    return (phase >= d2 && phase < end);
  } else {
    // wraps past 1.0
    return (phase >= d2 && phase < 1.0f)
        || (phase >= 0.0f && phase < (end - 1.0f));
  }
}

void loop() {
  unsigned long now = micros();

  // on each hall‐sensor tick, measure period + move+bounce
  if (digitalRead(HALL_PIN) == LOW && (now - lastTrigger > 7500)) {
    rotationPeriod = now - lastTrigger;
    lastTrigger    = now;

    x += vel_x;  y += vel_y;  z += vel_z;

    // X‐bounce → green
    if (x <= 0 || x >= PANEL_WIDTH  - 3) {
      vel_x = -vel_x;
      x     = constrain(x, 0, PANEL_WIDTH  - 3);
      r = 0; g = 255; b = 0;
    }
    // Y‐bounce → red
    if (y <= 0 || y >= PANEL_HEIGHT - 3) {
      vel_y = -vel_y;
      y     = constrain(y, 0, PANEL_HEIGHT - 3);
      r = 255; g = 0; b = 0;
    }
    // Z‐bounce → blue
    int maxZ = (PANEL_WIDTH/2) - 3;
    if (z <= 0 || z >= maxZ) {
      vel_z = -vel_z;
      z     = constrain(z, 0, maxZ);
      r = 0; g = 0; b = 255;
    }
  }

  // nothing to do until we have a period
  if (rotationPeriod == 0) return;

  // current rotation phase [0..1)
  float phase      = fmod((float)(now - lastTrigger) / rotationPeriod, 1.0f);
  float windowFrac = (float)pulseWidth / (float)rotationPeriod;

  // do we have *any* voxel on right now?
  bool currentPulseState = false;
  for (int dz = 0; dz < 3 && !currentPulseState; dz++) {
    for (int dx = 0; dx < 3 && !currentPulseState; dx++) {
      if (sliceOnAtPhase(dx, dz, phase, windowFrac))
        currentPulseState = true;
    }
  }

  // only re-draw when the on/off actually flips
  if (currentPulseState != lastPulseState) {
    display->clearScreen();

    if (currentPulseState) {
      // draw every sub‐voxel in the 3×3×3 cube that's "on" now
      for (int dz = 0; dz < 3; dz++) {
        for (int dx = 0; dx < 3; dx++) {
          if (sliceOnAtPhase(dx, dz, phase, windowFrac)) {
            for (int dy = 0; dy < 3; dy++) {
              int drawX = x + dx;
              if (phase >= 0.5f)
                drawX = PANEL_WIDTH - 3 - (x + dx);
              display->drawPixel(
                drawX,
                y + dy,
                display->color565(r, g, b)
              );
            }
          }
        }
      }
    }

    display->flipDMABuff.pper();
    lastPulseState = currentPulseState;
  }
}

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <math.h>

#define PANEL_WIDTH  64
#define PANEL_HEIGHT 32

struct Color { uint8_t r, g, b; };
struct ColorPair { Color start, end; };

const int NUM_COLOR_PAIRS = 5;
ColorPair colorSets[NUM_COLOR_PAIRS] = {
  {{238,  34, 105}, {228,  30,  21}},
  {{ 20, 148, 209}, { 30,  40, 141}},
  {{ 81, 231,  84}, {229, 240,  55}},
  {{124,  12, 108}, { 94,   0, 148}},
  {{255, 225,   0}, {255, 136,   0}}
};

const float TOTAL_CYCLE_TIME = 120000.0; // ms to cycle through all pairs
const float PULSE_SPEED      = 0.0008;   // smaller = slower pulse
const int   BASE_RADIUS      = 3;
const int   MAX_RADIUS       = min(PANEL_WIDTH, PANEL_HEIGHT) / 3;

MatrixPanel_I2S_DMA *display = nullptr;

Color lerpColor(const Color &a, const Color &b, float t) {
  return {
    (uint8_t)(a.r + (b.r - a.r) * t),
    (uint8_t)(a.g + (b.g - a.g) * t),
    (uint8_t)(a.b + (b.b - a.b) * t)
  };
}

void setup() {
  Serial.begin(115200);
  delay(100);

  HUB75_I2S_CFG cfg;
  cfg.double_buff      = true;
  cfg.i2sspeed         = HUB75_I2S_CFG::HZ_20M;
  cfg.driver           = HUB75_I2S_CFG::FM6126A;
  cfg.min_refresh_rate = 120;

  display = new MatrixPanel_I2S_DMA(cfg);
  if (!display->begin()) {
    Serial.println("Display init failed!");
    while (true) delay(1000);
  }
  display->setBrightness(128);
}

void loop() {
  unsigned long t = millis();

  // 1) Cycle through your color pairs
  float prog     = fmod(t, TOTAL_CYCLE_TIME) / TOTAL_CYCLE_TIME;
  float pairProg = prog * NUM_COLOR_PAIRS;
  int   idx      = (int)floor(pairProg);
  float tp       = pairProg - idx;
  ColorPair cur  = colorSets[idx];
  ColorPair nxt  = colorSets[(idx + 1) % NUM_COLOR_PAIRS];
  Color startCol = lerpColor(cur.start, nxt.start, tp);
  Color endCol   = lerpColor(cur.end,   nxt.end,   tp);

  // 2) Compute pulse (0→1→0) for size and color
  float pulse  = (sin(t * PULSE_SPEED) + 1.0f) * 0.5f;
  int   radius = BASE_RADIUS + (int)(pulse * MAX_RADIUS);
  Color circC  = lerpColor(startCol, endCol, pulse);
  uint16_t col = display->color565(circC.r, circC.g, circC.b);

  // 3) Draw filled circle
  display->clearScreen();
  display->fillCircle(
    PANEL_WIDTH/2, PANEL_HEIGHT/2, radius,
    col
  );
  display->flipDMABuffer();

  delay(20);  // ~50 Hz refresh
}

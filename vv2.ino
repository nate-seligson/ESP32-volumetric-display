#include <WiFi.h>
#include <WiFiServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_struct.h"

// HUB75 Pins
#define R1_PIN  25
#define G1_PIN  26
#define B1_PIN  27
#define R2_PIN  14
#define G2_PIN  12
#define B2_PIN  13
#define A_PIN   23
#define B_PIN   19
#define C_PIN    5
#define D_PIN   17
#define LAT_PIN  4
#define OE_PIN  15
#define CLK_PIN 16

const uint8_t PANEL_W = 64;
const uint8_t PANEL_H = 32;
const uint8_t ROW_BYTES = PANEL_W / 8;
const size_t FRAME_SIZE = PANEL_H * ROW_BYTES;

const char* SSID = "IoT_IRL";
const char* PASSWORD = "DePaulIRL";
const uint16_t TCP_PORT = 5005;

// Double-buffered framebuffer
uint8_t framebuffer[2][PANEL_H][ROW_BYTES];

// Use portMUX_TYPE for fast lock (better than mutex here)
portMUX_TYPE bufferLock = portMUX_INITIALIZER_UNLOCKED;
volatile uint8_t activeBuffer = 0;

WiFiServer server(TCP_PORT);
WiFiClient client;

// Map pins to bit masks
const uint32_t R1_MASK = 1UL << R1_PIN;
const uint32_t G1_MASK = 1UL << G1_PIN;
const uint32_t B1_MASK = 1UL << B1_PIN;
const uint32_t R2_MASK = 1UL << R2_PIN;
const uint32_t G2_MASK = 1UL << G2_PIN;
const uint32_t B2_MASK = 1UL << B2_PIN;
const uint32_t A_MASK  = 1UL << A_PIN;
const uint32_t B_MASK  = 1UL << B_PIN;
const uint32_t C_MASK  = 1UL << C_PIN;
const uint32_t D_MASK  = 1UL << D_PIN;
const uint32_t LAT_MASK = 1UL << LAT_PIN;
const uint32_t OE_MASK  = 1UL << OE_PIN;
const uint32_t CLK_MASK = 1UL << CLK_PIN;

// Set row address via direct GPIO
inline void setRowAddress(uint8_t row) {
  uint32_t pins = 0;
  if (row & 0x01) pins |= A_MASK;
  if (row & 0x02) pins |= B_MASK;
  if (row & 0x04) pins |= C_MASK;
  if (row & 0x08) pins |= D_MASK;

  // Clear all row address pins first
  GPIO.out_w1tc = A_MASK | B_MASK | C_MASK | D_MASK;
  // Set needed pins
  GPIO.out_w1ts = pins;
}

// Display task on core 0
void displayTask(void *pvParameters) {
  while (true) {
    uint8_t currentBuffer;
    // Lock and copy activeBuffer
    portENTER_CRITICAL(&bufferLock);
    currentBuffer = activeBuffer;
    portEXIT_CRITICAL(&bufferLock);

    for (uint8_t row = 0; row < PANEL_H / 2; row++) {
      // Disable output while updating row data
      GPIO.out_w1ts = OE_MASK;  // OE HIGH (disable)
      setRowAddress(row);

      for (uint8_t col = 0; col < PANEL_W; col++) {
        // Extract pixel bits for top and bottom half
        bool top_pixel = framebuffer[currentBuffer][row][col / 8] & (0x80 >> (col & 7));
        bool bot_pixel = framebuffer[currentBuffer][row + PANEL_H / 2][col / 8] & (0x80 >> (col & 7));

        // Prepare color bits for top
        uint32_t top_bits = 0;
        if (top_pixel) top_bits |= R1_MASK | G1_MASK | B1_MASK;  // white

        // Prepare color bits for bottom
        uint32_t bot_bits = 0;
        if (bot_pixel) bot_bits |= R2_MASK | G2_MASK | B2_MASK;  // white

        // Clear RGB pins before setting
        GPIO.out_w1tc = R1_MASK | G1_MASK | B1_MASK | R2_MASK | G2_MASK | B2_MASK;

        // Set RGB pins
        GPIO.out_w1ts = top_bits | bot_bits;

        // Pulse clock
        GPIO.out_w1ts = CLK_MASK;
        GPIO.out_w1tc = CLK_MASK;
      }

      // Latch data
      GPIO.out_w1ts = LAT_MASK;
      GPIO.out_w1tc = LAT_MASK;

      // Enable output
      GPIO.out_w1tc = OE_MASK;

      // Small delay to allow visible refresh (adjust as needed)
      delayMicroseconds(100);
    }
    vTaskDelay(1);  // Yield for WiFi and networking
  }
}

void setup() {
  Serial.begin(115200);

  const uint8_t pins[] = {
    R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
    A_PIN, B_PIN, C_PIN, D_PIN,
    LAT_PIN, OE_PIN, CLK_PIN
  };
  for (uint8_t i = 0; i < sizeof(pins); i++) {
    esp_rom_gpio_pad_select_gpio(pins[i]);
    gpio_set_direction((gpio_num_t)pins[i], GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)pins[i], 0);
  }

  memset(framebuffer, 0, sizeof(framebuffer));

  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());

  server.begin();
  Serial.printf("TCP server started on port %d\n", TCP_PORT);

  // Create display task pinned to core 0
  xTaskCreatePinnedToCore(
    displayTask,
    "DisplayTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
  static unsigned long lastActive = millis();

  if (!client || !client.connected()) {
    if (client) {
      Serial.println("Cleaning up dead client");
      client.stop();
    }

    WiFiClient newClient = server.available();
    if (newClient) {
      client = newClient;
      client.setNoDelay(true);
      lastActive = millis();
      Serial.println("Client connected");
    }
    delay(1);
    return;
  }

  if (client.available() >= FRAME_SIZE) {
    uint8_t nextBuffer = 1 - activeBuffer;
    size_t readBytes = client.readBytes((uint8_t*)framebuffer[nextBuffer], FRAME_SIZE);

    if (readBytes == FRAME_SIZE) {
      portENTER_CRITICAL(&bufferLock);
      activeBuffer = nextBuffer;
      portEXIT_CRITICAL(&bufferLock);

      lastActive = millis();
    } else {
      Serial.println("Incomplete frame, dropping client");
      client.stop();
    }
  }

  if (millis() - lastActive > 3000) {
    Serial.println("Connection timeout, dropping client");
    client.stop();
  }

  delay(1);
}

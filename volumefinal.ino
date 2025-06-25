#include <Arduino.h>
#include <WiFi.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <cmath>
#include <math.h>
// Replace with your network credentials
const char* ssid = "IoT_IRL";
const char* password = "DePaulIRL";
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32
#define HALL_PIN 35

// Define a simple color struct FIRST to avoid declaration issues
struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};
int myArray[PANEL_WIDTH * PANEL_HEIGHT][5];
MatrixPanel_I2S_DMA *display = nullptr;
volatile unsigned long lastTrigger = 0;
volatile float rpm = 0;
unsigned long rotationPeriod = 1000000;  

int spin_delay = 0;
const int spin_slices = 100;
const unsigned long pulseWidth = 4000;

// Color Configuration
const int NUM_COLOR_PAIRS = 5;
struct ColorPair {
  Color start;
  Color end;
};

const float TOTAL_CYCLE_TIME = 120000.0; // 30 seconds for full cycle through all pairs
bool lastPulseState = false;

// Create a Wi‑Fi TCP server on port 8888
WiFiServer server(8888);

// Global variable to hold the current mode (default is 1)
volatile int currentMode = 0;

// 3D Bouncing Ball Variables (ball is now 3x3 pixels)
int x = 10;         // lateral
int y = 15;         // vertical
int z = 0;          // "depth"
int vel_x = 1;
int vel_y = 2;
int vel_z = 3;
unsigned long z_delay = 0;

int r = 255;
int g = 255;
int b = 255;
// --- Mode function prototypes ---
void mode1();
void mode2();

void drawFromArray(bool reversed = false) {
  for (int i = 0; i < PANEL_WIDTH * PANEL_HEIGHT; i++) {
    int x = myArray[i][0];
    if (reversed) {
      x = PANEL_WIDTH - 1 - x; // Reverse the X coordinate
    }

    // Draw the pixel at the reversed or original X and Y positions
    display->drawPixel(x, myArray[i][1], 
                       display->color565(myArray[i][2], myArray[i][3], myArray[i][4]));
  }
}
void processArrayWiFiClient() {
  WiFiClient client = server.available();
  if (client && client.connected()) {
    Serial.println("Client connected!");
    String buffer;

    // Read all data from the client as quickly as possible
    while (client.connected() || client.available()) {
      while (client.available()) {
        buffer += client.readString(); // Read ALL available bytes
      }

      // Process lines in the buffer
      int endIndex;
      while ((endIndex = buffer.indexOf('\n')) != -1) {
        String line = buffer.substring(0, endIndex);
        line.trim(); // Remove \r or whitespace
        buffer = buffer.substring(endIndex + 1); // Remove processed line

        int x, y, r, g, b;
        if (sscanf(line.c_str(), "%d,%d,%d,%d,%d", &x, &y, &r, &g, &b) == 5) {
          int index = y * PANEL_WIDTH + x;
          if (index >= 0 && index < PANEL_WIDTH * PANEL_HEIGHT) {
            myArray[index][0] = x;
            myArray[index][1] = y;
            myArray[index][2] = r;
            myArray[index][3] = g;
            myArray[index][4] = b;
          }
        }
      }
    }

    client.stop();
  }
}
// --- Function to process incoming client commands ---
void processClient() {
  WiFiClient client = server.available();
  if (client && client.connected()) {
    String input = client.readStringUntil('\n'); // Read until newline
    input.trim(); // Clean up whitespace/newlines

    // Quick and clever mode selection: if you send "1", "2", or "3", you switch!
    if (input == "1" || input == "2" || input == "3") {
      currentMode = input.toInt();
      Serial.print("Switching to Mode: ");
      Serial.println(currentMode);
    } else {
      Serial.print("Unknown command received: ");
      Serial.println(input);
    }
    client.stop();  // Close the connection
  }
}

// Recalculate the x-axis delay based on x-z plane projection
void alterPoints() {
  // Calculate the projected distance in the x-z plane (for alignment)
  float local_x = sqrt((float)x * x + (float)z * z);
  
  // Calculate the angle in the x-z plane.
  // atan2 returns [-π, π]; we normalize to [0, 2π]
  float angle = atan2((float)x, (float)z);
  if(angle < 0) {
    angle += 2 * M_PI;
  }
  
  // Ratio of the angle to a full circle; then map it to the rotation period.
  float ratio = angle / (2 * M_PI);
  z_delay = ratio * rotationPeriod;
}

void drawBall(int drawX, int drawY, uint16_t color) {
  // Draw a 3x3 block.
  for (int dx = 0; dx < 3; dx++) {
    for (int dy = 0; dy < 3; dy++) {
      display->drawPixel(drawX + dx, drawY + dy, color);
    }
  }
}
void setup() {
  // Initialize serial communications
  Serial.begin(115200);
  delay(2000); // Allow serial to initialize
  Serial.println("POV Display Started - ESP32 Mode Switcher Starting Up!");

  // Initialize the Hall sensor pin
  pinMode(HALL_PIN, INPUT_PULLUP);
  // Connect to Wi‑Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi‑Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start the TCP server
  server.begin();
  Serial.println("TCP Server Started!");

  // Configure HUB75 panel
  HUB75_I2S_CFG config;
  config.double_buff = true;
  config.i2sspeed = HUB75_I2S_CFG::HZ_20M;
  config.driver = HUB75_I2S_CFG::FM6126A;
  config.min_refresh_rate = 120;

  display = new MatrixPanel_I2S_DMA(config);
  if (!display->begin()) {
    Serial.println("Display Init Fail!");
    while (1); // Halt execution if the display fails to initialize
  } else {
    Serial.println("HUB75 Display Initialized Successfully!");
  }
  
  display->setBrightness(255);
  lastTrigger = micros();
}



void loop() {
  // Check for incoming client commands
  if(currentMode == 0){
  processClient();
  }

  // Run the active mode's script
  switch (currentMode) {
    case 1:
      mode1();
      break;
    case 2:
      mode2();
      break;
  }
}

// --- Mode 1 ---
// Paste your Mode 1 script code here. For now, it just prints a message.
void mode1() {
  processArrayWiFiClient();
  unsigned long now = micros();

  if (digitalRead(HALL_PIN) == LOW) {
    if (now - lastTrigger > 5000) { 
      rotationPeriod = now - lastTrigger;
      rpm = 60000000.0 / rotationPeriod;
      lastTrigger = now;
      spin_delay = (spin_delay + 1) % spin_slices;
    }
  }

  if (rotationPeriod == 0) return;

  float phase = (float)(now - lastTrigger) / rotationPeriod;
  phase = fmod(phase, 1.0);
  float offset = (float)spin_delay / spin_slices;
  float effectivePhase = fmod(phase + offset, 1.0);

  bool currentPulseState = (effectivePhase < (float)pulseWidth / rotationPeriod) ||
                           (effectivePhase >= 0.5 && effectivePhase < 0.5 + (float)pulseWidth / rotationPeriod);

  if (currentPulseState != lastPulseState) {
    display->clearScreen();  // Clear the screen before drawing the new frame
    
    if (currentPulseState) {
      // Reverse the X-axis if the effectivePhase is in the second half of the cycle (phase >= 0.5)
      if (effectivePhase >= 0.5 && effectivePhase < 0.5 + (float)pulseWidth / rotationPeriod) {
        drawFromArray(true);  // Draw reversed
      } else {
        drawFromArray();  // Draw normal
      }
    } else {
      display->clearScreen();  // Clear the screen if no pulse state
    }

    display->flipDMABuffer();
    lastPulseState = currentPulseState;
  }
}

// --- Mode 2 ---
// Paste your Mode 2 script code here.
void mode2() {
  unsigned long now = micros();

  // On Hall sensor trigger update the rotation period and bounce positions
  if (digitalRead(HALL_PIN) == LOW && (now - lastTrigger > 7500)) {
    rotationPeriod = now - lastTrigger;
    lastTrigger = now;
    
    // Update bouncing positions
    x += vel_x;
    y += vel_y;
    z += vel_z;

    // Bounce off panel boundaries (account for a 3x3 ball)
    if (x <= 0 || x >= PANEL_WIDTH - 3) {
      vel_x = -vel_x;
      r = 0;
      g = 255;
      b = 0;
    }
    if (y <= 0 || y >= PANEL_HEIGHT - 3){ 
      vel_y = -vel_y;
      r = 255;
      g = 0;
      b = 0;
    }
    // For depth, you might use PANEL_WIDTH as the limit, or adjust as needed.
    if (z <= 0 || z >= (PANEL_WIDTH/2) - 3){
      r = 0;
      g = 0;
      b = 255;
      vel_z = -vel_z;
    }
    
    alterPoints();
  }

  // Wait until we have a valid rotation period
  if (rotationPeriod == 0) return;

  // Determine the current phase (a fraction of the rotation)
  float phase = fmod((float)(now - lastTrigger) / rotationPeriod, 1.0);

  // Calculate pulse window start and end (twice per rotation)
  float startPulse = z_delay / (float)rotationPeriod;
  float endPulse = (z_delay + pulseWidth) / (float)rotationPeriod;
  bool currentPulseState = false;
  
  // Show the ball if we’re within the pulse window for either half of the rotation.
  if ((phase >= startPulse && phase < endPulse) ||
      (phase >= 0.5 + startPulse && phase < 0.5 + endPulse)) {
    currentPulseState = true;
  }

  // Only update the display if the pulse state has changed
  if (currentPulseState != lastPulseState) {
    display->clearScreen();
    if (currentPulseState) {
      int drawX = x;
      // Mirror the x-axis for the second half of the rotation
      if (phase >= 0.5) {
        drawX = PANEL_WIDTH - 3 - x;
      }
      drawBall(drawX, y, display->color565(r, g, b));
    }
    display->flipDMABuffer();
    lastPulseState = currentPulseState;
  }
}
// --- Mode 3 ---
// Paste your Mode 3 script code here.
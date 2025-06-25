#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>

// Panel Configuration
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32
#define HALL_PIN 35

int myArray[PANEL_WIDTH * PANEL_HEIGHT][5];
MatrixPanel_I2S_DMA *display = nullptr;
volatile unsigned long lastTrigger = 0;
volatile float rpm = 0;
unsigned long rotationPeriod = 1000000;  

int spin_delay = 0;
const int spin_slices = 100;
const unsigned long pulseWidth = 4000;

bool lastPulseState = false;

const char* ssid = "IoT_IRL"; // Replace with your Wi-Fi SSID
const char* password = "DePaulIRL"; // Replace with your Wi-Fi password
WiFiServer server(8888); // TCP server on port 8888

// Function to draw from myArray
// Function to draw from myArray
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

void setup() {
  Serial.begin(115200);
  delay(2000); // Give time for Serial to initialize
  Serial.println("Serial is working!"); // First debug print
  
  pinMode(HALL_PIN, INPUT_PULLUP);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to Wi-Fi...");
  }
  Serial.println("Connected to Wi-Fi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start TCP server
  server.begin();
  Serial.println("TCP Server Started!");

  // HUB75 Panel Setup
  HUB75_I2S_CFG config;
  config.double_buff = true;
  config.i2sspeed = HUB75_I2S_CFG::HZ_20M;
  config.driver = HUB75_I2S_CFG::FM6126A;
  config.min_refresh_rate = 120;

  display = new MatrixPanel_I2S_DMA(config);
  if (display->begin()) {
    Serial.println("HUB75 Display Initialized Successfully!");
  } else {
    Serial.println("HUB75 Display Initialization Failed!");
  }

  display->setBrightness(255);
  lastTrigger = micros();
}
void loop() {
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

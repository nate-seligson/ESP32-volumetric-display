#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32
#define BUFFER_SIZE 5000  // Reduced for safety
#define PORT 8888         // TCP port for communication

// WiFi credentials
const char* ssid = "IoT_IRL";
const char* password = "DePaulIRL";

const int hallpin = 35; // input hall pin

MatrixPanel_I2S_DMA *display = nullptr;
WiFiServer server(PORT);
WiFiClient client;
uint8_t serialBuffer[BUFFER_SIZE];
size_t bufferLength = 0;

// Mode selection enum
enum Mode { MODE_UNDEFINED, MODE_SETUP, MODE_LOOP };
Mode currentMode = MODE_UNDEFINED;

/**
 * Sends a reset command to the client.
 * In this implementation, a reset command is a single zero byte (0x00).
 */
void sendResetQueue() {
  if (client && client.connected()) {
    client.write((uint8_t)0x00);
    bufferLength = 0;
  } else {
    Serial.println("No client connected. Cannot send reset queue.");
  }
}

void processModeSelection() {
  // Wait for a mode command if not set yet.
  if (client && client.connected() && client.available()) {
    String modeCommand = client.readStringUntil('\n');
    modeCommand.trim();
    if (modeCommand.equalsIgnoreCase("setup")) {
      currentMode = MODE_SETUP;
      Serial.println("Client selected SETUP mode");
    } else if (modeCommand.equalsIgnoreCase("loop")) {
      currentMode = MODE_LOOP;
      Serial.println("Client selected LOOP mode");
    } else {
      Serial.println("Invalid mode command received. Expecting 'setup' or 'loop'.");
    }
  }
}
// Declare lastTime outside the function to preserve its value between calls.
unsigned long lastTime = 0;
bool active = false;
void processSetupMode() {
  if(digitalRead(hallpin) == HIGH){
    active = false;
  }
  else if (digitalRead(hallpin) == LOW && !active) {
    active = true;
    unsigned long currentTime = millis();
    // Ensure there's a previous timestamp to compute a delta
    if (lastTime > 0 && currentTime > lastTime) {
      float deltaSeconds = (currentTime - lastTime) / 1000.0;
      if (deltaSeconds > 0) {
        float frequency = 1.0 / deltaSeconds;  // in Hz (cycles per second)
        float rpm = frequency * 60;            // convert to rotations per minute
        Serial.println(rpm);
        // Check if client is connected, then send the RPM value as a string.
        if (client && client.connected()) {
          String output = String(rpm, 2); // Format to 2 decimal places.
          client.println(output);
        }
      }
    }
    lastTime = currentTime;
  }
  // Optionally, add more setup-related functionality here.
}



void processLoopMode() {
  // Handle incoming data from the TCP client.
  if (client && client.connected()) {
    int bytesAvailable = client.available();
    if (bytesAvailable > 0) {
      size_t bytesToRead = min((size_t)bytesAvailable, BUFFER_SIZE - bufferLength);
      client.readBytes(&serialBuffer[bufferLength], bytesToRead);
      bufferLength += bytesToRead;
    }
  }

  // Process frames in the buffer.
  size_t i = 0;
  while (i + 1 < bufferLength) {
    if (serialBuffer[i] == 0xFF && serialBuffer[i + 1] == 0xFF) {
      // Found frame end marker; assume the two bytes before the marker represent delay.
      if (i >= 2) {
        uint16_t frameDelay = (serialBuffer[i - 2] << 8) | serialBuffer[i - 1];
        size_t pixelDataLength = i - 2;
        if (pixelDataLength % 5 == 0) {
          display->clearScreen();
          for (size_t j = 0; j < pixelDataLength; j += 5) {
            if (j + 4 >= pixelDataLength) break;
            uint8_t x = serialBuffer[j];
            uint8_t y = serialBuffer[j + 1];
            uint8_t r = serialBuffer[j + 2];
            uint8_t g = serialBuffer[j + 3];
            uint8_t b = serialBuffer[j + 4];
            if (x < PANEL_WIDTH && y < PANEL_HEIGHT) {
              display->drawPixel(x, y, display->color565(r, g, b));
            }
          }
          uint32_t start = micros();
          while ((uint32_t)(micros() - start) < frameDelay) {
            yield(); // Let the ESP32 do background tasks
          }
          display->flipDMABuffer();

          // Remove processed data.
          size_t bytesToRemove = i + 2;
          memmove(serialBuffer, &serialBuffer[bytesToRemove], bufferLength - bytesToRemove);
          bufferLength -= bytesToRemove;
          i = 0;
          continue; // start processing remaining buffer
        }
      }
      break;
    }
    i++;
  }

  // Buffer overflow protection.
  if (bufferLength >= BUFFER_SIZE) {
    Serial.println("Buffer overflow; clearing buffer.");
    bufferLength = 0;
  }
  if (active && digitalRead(hallpin) == HIGH) {
    active = false;
  }
  // If the hall sensor is triggered (LOW), send a reset command.
  else if (digitalRead(hallpin) == LOW && !active) {
    active = true;

    sendResetQueue();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(hallpin, INPUT_PULLUP);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP address: ");
  Serial.println(WiFi.localIP());

  // Start TCP server.
  server.begin();
  Serial.println("TCP server started");

  // Initialize display.

  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, 1);
  mxconfig.double_buff = true;
  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();
  display->setBrightness(128);
  Serial.println("Ready");
}

void loop() {
  // Manage client connection.
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("New client connected");
      // Reset mode selection for each new client.
      currentMode = MODE_UNDEFINED;
      bufferLength = 0;
    }
  }
  
  // If we have a connected client, wait for mode selection.
  if (currentMode == MODE_UNDEFINED) {
    processModeSelection();
  } else if (currentMode == MODE_SETUP) {
    processSetupMode();
    display->fillScreen(display->color565(255, 0, 0));
    display->flipDMABuffer();
  } else if (currentMode == MODE_LOOP) {
    processLoopMode();
  }
}

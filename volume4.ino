  #include <Arduino.h>
  #include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
  #undef  LED_BUILTIN
  #define LED_BUILTIN  -1   // disable Wi-Fi LED on GPIO 2

  #include <WiFi.h>
  #include <math.h>

  // Define a simple color struct FIRST to avoid declaration issues
  struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };
  // Panel Configuration
  #define PANEL_WIDTH 64
  #define PANEL_HEIGHT 32

  // Global pixel array
  int myColorArray[PANEL_WIDTH * PANEL_HEIGHT][5];

  MatrixPanel_I2S_DMA *display = nullptr;

  // WiFi and Server
  const char* ssid = "IoT_IRL";
  const char* password = "DePaulIRL";
  WiFiServer server(8888);
  WiFiClient client;

  // Color Configuration
  const int NUM_COLOR_PAIRS = 5;
  struct ColorPair {
    Color start;
    Color end;
  };

  ColorPair colorSets[NUM_COLOR_PAIRS] = {
    {{238, 34, 105}, {228, 30, 21}},   // Pair 0
    {{20, 148, 209}, {30, 40, 141}},    // Pair 1
    {{81, 231, 84}, {229, 240, 55}},    // Pair 2
    {{124, 12, 108}, {94, 0, 148}},     // Pair 3
    {{255, 225, 0}, {255, 136, 0}}      // Pair 4
  };

  const float TOTAL_CYCLE_TIME = 120000.0; // 30 seconds for full cycle through all pairs

  // Improved lerp function
  Color lerpColor(const Color &a, const Color &b, float t) {
    return {
      (uint8_t)(a.r + (b.r - a.r) * t),
      (uint8_t)(a.g + (b.g - a.g) * t),
      (uint8_t)(a.b + (b.b - a.b) * t)
    };
  }

  void drawFromColorArray(bool reversed = false) {
    unsigned long time = millis();
    
    // Calculate smooth transition between all pairs
    float progress = fmod(time, TOTAL_CYCLE_TIME) / TOTAL_CYCLE_TIME;
    float pairProgress = progress * (NUM_COLOR_PAIRS - 1);
    int pairIndex = (int)pairProgress;
    float t = pairProgress - pairIndex;

    // Get current and next color pairs
    ColorPair current = colorSets[pairIndex];
    ColorPair next = colorSets[(pairIndex + 1) % NUM_COLOR_PAIRS];
    
    // Interpolate between pairs
    Color start = lerpColor(current.start, next.start, t);
    Color end = lerpColor(current.end, next.end, t);

    // Add pulsating effect
    float pulse = (sin(time * 0.002) + 1.0) / 2.0; // 0.002 ≈ 3.14 seconds per pulse

    display->clearScreen();
    for (int i = 0; i < PANEL_WIDTH * PANEL_HEIGHT; i++) {
      int x = reversed ? (PANEL_WIDTH - 1 - myColorArray[i][0]) : myColorArray[i][0];
      int y = myColorArray[i][1];

      Color col = {0, 0, 0};
      
      if (myColorArray[i][2] > 0) {
        col = lerpColor(start, end, pulse);
      } 
      else if (myColorArray[i][4] > 0) {
        col = lerpColor(end, start, pulse);
      }

      display->drawPixel(x, y, display->color565(col.r, col.g, col.b));
    }
  }

  void processArrayColorWiFiClient() {
    if (!client || !client.connected()) {
      // If no client is connected, check for a new connection
      client = server.available();
      if (client && client.connected()) {
        Serial.println("Client connected!");
      }
    } else {
      // If client is connected, read and process data incrementally
      while (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();

        int x, y, flag1, dummy, flag2;
        if (sscanf(line.c_str(), "%d,%d,%d,%d,%d", &x, &y, &flag1, &dummy, &flag2) == 5) {
          int index = y * PANEL_WIDTH + x;
          if (index >= 0 && index < PANEL_WIDTH * PANEL_HEIGHT) {
            myColorArray[index][0] = x;
            myColorArray[index][1] = y;
            myColorArray[index][2] = flag1;
            myColorArray[index][3] = dummy;
            myColorArray[index][4] = flag2;
          }
        }
      }
    }
  }
  void setup() {
    Serial.begin(115200);
    delay(2000); // Allow time for Serial to initialize

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

    // For demonstration, initialize pixels so that even columns have red flag and odd columns have blue flag.
    for (int y = 0; y < PANEL_HEIGHT; y++) {
      for (int x = 0; x < PANEL_WIDTH; x++) {
        int index = y * PANEL_WIDTH + x;
        myColorArray[index][0] = x;
        myColorArray[index][1] = y;
        if (x % 2 == 0) {
          myColorArray[index][2] = 1;  // red flag
          myColorArray[index][4] = 0;
        } else {
          myColorArray[index][2] = 0;
          myColorArray[index][4] = 1;  // blue flag
        }
        myColorArray[index][3] = 0;  // dummy (unused)
      }
    }
  }

  void loop() {
    processArrayColorWiFiClient();  // Continuously process incoming data
    drawFromColorArray();            // Update the display
    display->flipDMABuffer();   // Swap buffers for smooth rendering
  }
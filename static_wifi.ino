// ESP32S3 HY-P10-RGB LED Matrix Display with WiFi Color Control
// Combines direct matrix control with WiFi server for remote color updates
// Stores and loads colors from SPIFFS and displays last payload

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h> // For file system access
#include <Preferences.h>

// WiFi settings
const char* ssid = "Armtronix";
const char* password = "i0t_ARM_123$%";
WebServer server(80);

// Static IP configuration
IPAddress staticIP(192, 168, 1, 254);     // The static IP you want to assign to ESP32
IPAddress gateway(192, 168, 1, 1);        // Your router's IP address
IPAddress subnet(255, 255, 255, 0);       // Subnet mask
IPAddress dns(8, 8, 8, 8);             // DNS (Google's DNS in this example)

// LED Matrix pin definitions
#define R1_PIN  48 // Red 1
#define G1_PIN  47 // Green 1
#define B1_PIN  46 // Blue 1
#define R2_PIN  45 // Red 2
#define G2_PIN  42 // Green 2
#define B2_PIN  41 // Blue 2
#define A_PIN   40 // Row address A
#define B_PIN   39 // Row address B
#define C_PIN   38 // Row address C
#define CLK_PIN 36 // Clock
#define LAT_PIN 37 // Latch
#define OE_PIN  35 // Output Enable (active LOW)

// Display dimensions
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 16
#define SCAN_ROWS 8  // P10 panels typically have 1/8 scan rate (16/2 = 8 rows)

// Frame buffer (3 bits per pixel for RGB)
uint8_t frameBuffer[MATRIX_WIDTH * MATRIX_HEIGHT / 8 * 3];

// Character dimensions
#define CHAR_WIDTH 10
#define CHAR_HEIGHT 14

// Font data (S, P, Q, D, C, M, E)
const uint8_t largeFont_S[] = {
    0b00111111, 0b00000000,
    0b01111111, 0b10000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b00111111, 0b00000000,
    0b00011111, 0b10000000,
    0b00000001, 0b10000000,
    0b00000001, 0b10000000,
    0b00000001, 0b10000000,
    0b00000001, 0b10000000,
    0b01111111, 0b10000000,
    0b00111111, 0b00000000
};
const uint8_t largeFont_P[] = {
    0b01111110, 0b00000000,
    0b01111111, 0b00000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01111111, 0b00000000,
    0b01111110, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000
};
const uint8_t largeFont_Q[] = {
    0b00011110, 0b00000000,
    0b00111111, 0b00000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100101, 0b10000000,
    0b01100011, 0b10000000,
    0b01100001, 0b10000000,
    0b00111111, 0b10000000,
    0b00011110, 0b10000000
};
const uint8_t largeFont_D[] = {
    0b01111110, 0b00000000,
    0b01111111, 0b00000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01100001, 0b10000000,
    0b01111111, 0b00000000,
    0b01111110, 0b00000000
};
const uint8_t largeFont_C[] = {
    0b00011111, 0b00000000,
    0b00111111, 0b10000000,
    0b01100000, 0b10000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b10000000,
    0b01100000, 0b10000000,
    0b00111111, 0b10000000,
    0b00011111, 0b00000000
};
const uint8_t largeFont_M[] = {
    0b11000001, 0b10000000,
    0b11100011, 0b10000000,
    0b11110111, 0b10000000,
    0b11111111, 0b10000000,
    0b11101011, 0b10000000,
    0b11000001, 0b10000000,
    0b11000001, 0b10000000,
    0b11000001, 0b10000000,
    0b11000001, 0b10000000,
    0b11000001, 0b10000000,
    0b11000001, 0b10000000,
    0b11000001, 0b10000000,
    0b11000001, 0b10000000,
    0b11000001, 0b10000000
};
// New font data for E
const uint8_t largeFont_E[] = {
    0b01111111, 0b00000000,
    0b01111111, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01111110, 0b00000000,
    0b01111110, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b00000000,
    0b01111111, 0b00000000,
    0b01111111, 0b00000000
};


// Text to display
const char text[] = "SPQDCME";

// Color mapping structure
struct RGBColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// Function to convert color name to RGB values
RGBColor getColorFromName(String colorName) {
  // Convert to lowercase for case-insensitive comparison
  colorName.toLowerCase();

  if (colorName == "red") return {7, 0, 0};
  if (colorName == "green") return {0, 7, 0};
  if (colorName == "blue") return {0, 0, 7};
  if (colorName == "yellow") return {7, 7, 0};
  if (colorName == "cyan") return {0, 7, 7};
  if (colorName == "magenta") return {7, 0, 7};
  if (colorName == "white") return {7, 7, 7};
  if (colorName == "orange") return {7, 3, 0};
  if (colorName == "purple") return {4, 0, 7};

  // Default to white if color not recognized
  return {7, 7, 7};
}

// *** SPIFFS Configuration ***
#define COLOR_FILE "/colors.dat"
#define PAYLOAD_FILE "/payload.txt"

// Default colors for each letter (will be updated via WiFi)
RGBColor letterColors[7] = {
  {7, 0, 0},  // S - Red
  {0, 7, 0},  // P - Green
  {0, 0, 7},  // Q - Blue
  {7, 7, 0},  // D - Yellow
  {0, 7, 7},  // C - Cyan
  {7, 0, 7},  // M - Magenta
  {7, 3, 0}   // E - Orange
};

// WiFi connection status and retry parameters
bool wifiConnected = false;
unsigned long lastWifiAttempt = 0;
const unsigned long wifiRetryInterval = 60000; // 1 minute between connection attempts

int currentChar = 0;             // Current character index
unsigned long lastUpdate = 0;    // Time of last character update
const int updateInterval = 100;  // 100ms for smoother scrolling

// Vertical position of text (for scrolling)
int yPosition = MATRIX_HEIGHT;   // Start below the visible area

// Scroll speed - set to 1 for original speed
const int scrollSpeed = 1;       // Move this many pixels per frame

// Define center position and pause duration
const int centerY = (MATRIX_HEIGHT - CHAR_HEIGHT) / 2; // Center Y position
const unsigned long pauseDuration = 4000; // 4 seconds pause at center
unsigned long pauseStartTime = 0; // When did the pause begin

// Animation state
enum AnimState {
  MOVING_IN,    // Character is moving into the screen
  PAUSED,       // Character is paused at center
  MOVING_OUT,   // Character is moving out of the screen
};

AnimState animState = MOVING_IN;

// Function prototypes
void clearDisplay();
void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
void drawLargeChar(char ch, int x, int y, uint8_t r, uint8_t g, uint8_t b);
void refreshDisplay();
void setupWiFi();
void handleData(String jsonData); 
void checkWiFiConnection();
bool loadColors();
bool saveColors();
String loadPayload();
void savePayload(const String& payload);

String lastPayload = ""; // Store the last payload

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\nESP32 LED Matrix with WiFi Color Control Starting...");

  // Initialize pins
  pinMode(R1_PIN, OUTPUT);
  pinMode(G1_PIN, OUTPUT);
  pinMode(B1_PIN, OUTPUT);
  pinMode(R2_PIN, OUTPUT);
  pinMode(G2_PIN, OUTPUT);
  pinMode(B2_PIN, OUTPUT);
  pinMode(A_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  pinMode(C_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(LAT_PIN, OUTPUT);
  pinMode(OE_PIN, OUTPUT);

  // Set initial pin states
  digitalWrite(OE_PIN, HIGH);  // Display off during initialization
  digitalWrite(CLK_PIN, LOW);
  digitalWrite(LAT_PIN, LOW);

  // Clear frame buffer
  clearDisplay();

  // *** Initialize SPIFFS - Do this first ***
  if (!SPIFFS.begin(true)) { // Format on failure for more reliable operation
    Serial.println("SPIFFS Mount Failed");
    delay(1000);
    return;
  } else {
    Serial.println("SPIFFS Mount OK");
  }

  // *** Load Colors from SPIFFS first ***
  if (!loadColors()) {
    Serial.println("Failed to load colors from file, using defaults.");
  } else {
    Serial.println("Colors loaded successfully from file.");
    // Display the loaded colors for debugging
    for (int i = 0; i < strlen(text); i++) {
      Serial.printf("Letter %c: R=%d, G=%d, B=%d\n", 
                   text[i], 
                   letterColors[i].r, 
                   letterColors[i].g, 
                   letterColors[i].b);
    }
  }

  // Then load and apply the last payload
  lastPayload = loadPayload();
  if (!lastPayload.isEmpty()) {
    Serial.print("Loaded previous payload: ");
    Serial.println(lastPayload);
    handleData(lastPayload); // Apply the colors from the last payload
  } else {
    Serial.println("No previous payload found");
  }

  // Setup web server endpoints
  server.on("/data", HTTP_POST, []() { // inline handler function
    if (server.hasArg("plain")) {
      String jsonData = server.arg("plain");
      Serial.print("Received JSON data: ");
      Serial.println(jsonData);

      // Process the payload immediately
      savePayload(jsonData);
      handleData(jsonData);
      server.send(200, "application/json", "{\"status\":\"success\"}");

    } else {
      Serial.println("[ERROR] No JSON data received!");
      server.send(400, "application/json", "{\"error\":\"No data received\"}");
    }
  });

  // Initialize WiFi (non-blocking)
  setupWiFi();
  
  // Start the server
  server.begin();
  Serial.println("LED Matrix Initialized");
}

void loop() {
  // Handle WiFi and server requests
  checkWiFiConnection();
  if (wifiConnected) {
    server.handleClient();
  }

  // Refresh the display continuously
  refreshDisplay();

  // Update animation based on current time
  unsigned long currentTime = millis();

  // Check if it's time to update the position
  if (currentTime - lastUpdate >= updateInterval) {
    lastUpdate = currentTime;

    // Clear the display for the next frame
    clearDisplay();

    // Center horizontally
    int centerX = (MATRIX_WIDTH - CHAR_WIDTH) / 2;  // Center the character

    // State machine for animation
    switch (animState) {
      case MOVING_IN:
        // Moving from bottom to center
        if (yPosition <= centerY) {
          // Reached center, start pause
          yPosition = centerY;
          animState = PAUSED;
          pauseStartTime = currentTime;
        } else {
          // Continue moving up
          yPosition -= scrollSpeed;
        }
        break;

      case PAUSED:
        // Character pauses at center
        if (currentTime - pauseStartTime >= pauseDuration) {
          // Pause finished, start moving out
          animState = MOVING_OUT;
        }
        // During pause, yPosition remains at centerY
        break;

      case MOVING_OUT:
        // Moving from center to top
        yPosition -= scrollSpeed;

        // If character has moved off screen at the top
        if (yPosition < -CHAR_HEIGHT) {
          // Reset for next character
          yPosition = MATRIX_HEIGHT;
          currentChar = (currentChar + 1) % strlen(text);
          animState = MOVING_IN; // Start next character moving in
        }
        break;
    }

    // Draw the current character with its color
    char curChar = text[currentChar];
    drawLargeChar(curChar, centerX, yPosition,
                  letterColors[currentChar].r,
                  letterColors[currentChar].g,
                  letterColors[currentChar].b);
  }
}

// Set up WiFi connection (non-blocking)
// Set up WiFi connection (non-blocking)
void setupWiFi() {
  Serial.print("Connecting to WiFi Network: ");
  Serial.println(ssid);

  // Configure static IP
  if (!WiFi.config(staticIP, gateway, subnet, dns)) {
    Serial.println("Static IP Configuration Failed");
  } else {
    Serial.println("Static IP Configuration Successful");
    Serial.print("Static IP: ");
    Serial.println(staticIP.toString());
  }

  // Start WiFi connection process
  WiFi.begin(ssid, password);

  // Record the start time of connection attempt
  lastWifiAttempt = millis();

  // Initial connection attempt will continue in checkWiFiConnection()
}

// Check and maintain WiFi connection (non-blocking)
void checkWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      // We just connected
      wifiConnected = true;
      Serial.println("\nConnected to WiFi!");
      Serial.print("ESP32 IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.println("HTTP Server started");
      Serial.print("Endpoint: http://");
      Serial.println(WiFi.localIP().toString() + "/data");
    }
  } else {
    // Not connected
    if (wifiConnected) {
      // We just lost connection
      wifiConnected = false;
      Serial.println("WiFi connection lost!");
    }

    // Try to reconnect periodically instead of blocking in a loop
    unsigned long currentTime = millis();
    if (currentTime - lastWifiAttempt >= wifiRetryInterval) {
      Serial.println("\nAttempting to reconnect to WiFi...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      lastWifiAttempt = currentTime;
      Serial.print("WiFi Status:");
      Serial.println(WiFi.status());
    }
  }
}

// Handle incoming JSON data for color updates
void handleData(String jsonData) {
  // Use ArduinoJson to parse the data
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonData);

  if (error) {
    Serial.print("[ERROR] Failed to parse JSON: ");
    Serial.println(error.c_str());
    return; // Don't call server.send here - might be called during boot
  }

  // Update colors based on JSON data
  // Format: {"S": "Blue", "Q": "Green", "P": "Blue", "D": "Green", "C": "Blue", "M": "Green", "E": "Blue"}
  bool updated = false;

  for (int i = 0; i < strlen(text); i++) {
    String letterKey(text[i]);

    if (doc.containsKey(letterKey)) {
      String colorName = doc[letterKey].as<String>();
      RGBColor color = getColorFromName(colorName);

      // Update the color
      letterColors[i] = color;
      updated = true;

      Serial.print("Updated letter ");
      Serial.print(letterKey);
      Serial.print(" to color: ");
      Serial.println(colorName);
    }
  }

  if (updated) {
    Serial.println("Letter colors updated successfully!");
    // Always save colors to SPIFFS when they are updated
    if (!saveColors()) {
      Serial.println("Error: Failed to save colors to file!");
    } else {
      Serial.println("Colors saved successfully to file.");
    }
  } else {
    Serial.println("No matching letters found in the JSON data.");
  }
}

// Clear the display buffer
void clearDisplay() {
  memset(frameBuffer, 0, sizeof(frameBuffer));
}

// Set a pixel in the buffer (x, y, r, g, b)
void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) return;

  // Calculate buffer position
  int index_pixel = (y * MATRIX_WIDTH + x) / 8;
  int bit = 7 - (x % 8);

  // Set RGB values (simplifying to 1 bit per color)
  if (r > 0) frameBuffer[index_pixel] |= (1 << bit);
  else frameBuffer[index_pixel] &= ~(1 << bit);

  if (g > 0) frameBuffer[index_pixel + MATRIX_WIDTH * MATRIX_HEIGHT / 8] |= (1 << bit);
  else frameBuffer[index_pixel + MATRIX_WIDTH * MATRIX_HEIGHT / 8] &= ~(1 << bit);

  if (b > 0) frameBuffer[index_pixel + 2 * MATRIX_WIDTH * MATRIX_HEIGHT / 8] |= (1 << bit);
  else frameBuffer[index_pixel + 2 * MATRIX_WIDTH * MATRIX_HEIGHT / 8] &= ~(1 << bit);
}

// Draw a character from our large font
void drawLargeChar(char ch, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  // Get the appropriate character data based on the character
  const uint8_t* charData;

  if (ch == 'S' || ch == 's') charData = largeFont_S;
  else if (ch == 'P' || ch == 'p') charData = largeFont_P;
  else if (ch == 'Q' || ch == 'q') charData = largeFont_Q;
  else if (ch == 'D' || ch == 'd') charData = largeFont_D;
  else if (ch == 'C' || ch == 'c') charData = largeFont_C;
  else if (ch == 'M' || ch == 'm') charData = largeFont_M;
  else if (ch == 'E' || ch == 'e') charData = largeFont_E;
  else {
    return; // Unsupported character
  }

  // Draw each row of the character
  for (int row = 0; row < CHAR_HEIGHT; row++) {
    // Process each row of the font
    for (int col = 0; col < CHAR_WIDTH; col++) {
      // For each column in our 10-pixel wide character
      // Adjust bit position calculation to shift left
      int byteIndex = col < 8 ? 0 : 1;  // Which byte of the pair

      // Modified bit position calculation - shifting 2 bits left
      int bitPosition;
      if (col < 8) {
        bitPosition = 7 - col;
      } else {
        bitPosition = 15 - col;
      }

      // Check if this bit is set in our font data
      if (charData[row * 2 + byteIndex] & (1 << bitPosition)) {
        setPixel(x + col, y + row, r, g, b);
      }
    }
  }
}

// Refresh the display (bit-banging the HUB75 protocol)
// Refresh the display (bit-banging the HUB75 protocol)
void refreshDisplay() {
  digitalWrite(OE_PIN, HIGH); // Disable display

  for (int row = 0; row < SCAN_ROWS; row++) {
    // Set row address for the first matrix
    digitalWrite(A_PIN, row & 0x01);
    digitalWrite(B_PIN, row & 0x02);
    digitalWrite(C_PIN, row & 0x04);

    for (int x = 0; x < MATRIX_WIDTH; x++) {
      // Calculate the index for the upper and lower halves of the display
      int upperIndex = (row * MATRIX_WIDTH + x) / 8;
      int lowerIndex = ((row + SCAN_ROWS) * MATRIX_WIDTH + x) / 8;

      // Calculate the bit position within the byte
      int bit = 7 - (x % 8);

      // Get the color data from the frame buffer for the first matrix
      uint8_t r1 = (frameBuffer[upperIndex] >> bit) & 0x01;
      uint8_t g1 = (frameBuffer[upperIndex + MATRIX_WIDTH * MATRIX_HEIGHT / 8] >> bit) & 0x01;
      uint8_t b1 = (frameBuffer[upperIndex + 2 * MATRIX_WIDTH * MATRIX_HEIGHT / 8] >> bit) & 0x01;

      // Write the color data to the pins for the first matrix
      digitalWrite(R1_PIN, r1);
      digitalWrite(G1_PIN, g1);
      digitalWrite(B1_PIN, b1);

      // For the second matrix, use the lowerIndex to get the correct row data
      uint8_t r2 = (frameBuffer[lowerIndex] >> bit) & 0x01;
      uint8_t g2 = (frameBuffer[lowerIndex + MATRIX_WIDTH * MATRIX_HEIGHT / 8] >> bit) & 0x01;
      uint8_t b2 = (frameBuffer[lowerIndex + 2 * MATRIX_WIDTH * MATRIX_HEIGHT / 8] >> bit) & 0x01;

      // Write the color data to the pins for the second matrix
      digitalWrite(R2_PIN, r2);
      digitalWrite(G2_PIN, g2);
      digitalWrite(B2_PIN, b2);

      // Clock the data in
      digitalWrite(CLK_PIN, HIGH);
      digitalWrite(CLK_PIN, LOW);
    }
    // Latch the data
    digitalWrite(LAT_PIN, HIGH);
    digitalWrite(LAT_PIN, LOW);

    // Output Enable (active LOW) - turn on the display for this row
    digitalWrite(OE_PIN, LOW);
    delayMicroseconds(500); // Adjust this value for brightness
    digitalWrite(OE_PIN, HIGH); // Disable display
  }
}

// Load Colors from SPIFFS - Improved with error handling
bool loadColors() {
  if (!SPIFFS.exists(COLOR_FILE)) {
    Serial.println("Color file not found, will use defaults");
    return false;
  }

  File file = SPIFFS.open(COLOR_FILE, "r");
  if (!file) {
    Serial.println("Failed to open color file for reading");
    return false;
  }

  if (file.size() != sizeof(letterColors)) {
    Serial.println("Error: Color file has unexpected size. Using defaults.");
    Serial.printf("Expected size: %d, Actual size: %d\n", sizeof(letterColors), file.size());
    file.close();
    return false;
  }

  size_t bytesRead = file.readBytes((char*)letterColors, sizeof(letterColors));
  if (bytesRead != sizeof(letterColors)) {
    Serial.printf("Error: Could not read all color bytes. Read %d of %d\n", 
                 bytesRead, sizeof(letterColors));
    file.close();
    return false;
  }

  file.close();
  return true;
}

// Save Colors to SPIFFS - Improved with error handling
bool saveColors() {
  File file = SPIFFS.open(COLOR_FILE, "w");
  if (!file) {
    Serial.println("Error: Could not open color file for writing");
    return false;
  }

  size_t bytesWritten = file.write((uint8_t*)letterColors, sizeof(letterColors));
  if (bytesWritten != sizeof(letterColors)) {
    Serial.printf("Error: Could not write all color bytes. Wrote %d of %d\n", 
                 bytesWritten, sizeof(letterColors));
    file.close();
    return false;
  }

  file.close();
  return true;
}

// Load the last payload from SPIFFS
String loadPayload() {
  if (!SPIFFS.exists(PAYLOAD_FILE)) {
    Serial.println("Payload file not found");
    return "";
  }

  File file = SPIFFS.open(PAYLOAD_FILE, "r");
  if (!file) {
    Serial.println("Failed to open payload file for reading");
    return "";
  }

  String payload = file.readString();
  file.close();
  
  Serial.printf("Loaded payload (%d bytes): %s\n", payload.length(), payload.c_str());
  return payload;
}

// Save the current payload to SPIFFS
void savePayload(const String& payload) {
  File file = SPIFFS.open(PAYLOAD_FILE, "w");
  if (!file) {
    Serial.println("Error opening payload file for writing");
    return;
  }

  size_t bytesWritten = file.print(payload);
  if (bytesWritten != payload.length()) {
    Serial.printf("Warning: Only wrote %d of %d bytes to payload file\n", 
                 bytesWritten, payload.length());
  } else {
    Serial.println("Payload saved successfully");
  }
  
  file.close();

}

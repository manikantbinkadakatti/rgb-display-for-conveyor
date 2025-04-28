// ESP32S3 HY-P10-RGB LED Matrix Display with Ethernet Color Control
// Combines direct matrix control with Ethernet server for remote color updates
// Stores and loads colors from SPIFFS and displays last payload



#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <WiFi.h> // Needed for MAC address fallback

const char* WIFI_SSID = "Armtronix";      // Replace with your WiFi SSID
const char* WIFI_PASSWORD = "i0t_ARM_123$%";

// Ethernet settings with manual MAC address option
// Option 1: Use predefined MAC address (uncomment one)
// byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // Default
//byte mac[] = { 0xDE, 0x85, 0xE3, 0xE6, 0xD0, 0xE4 }; // Example custom MAC
// byte mac[] = { 0x06, 0x15, 0x24, 0x33, 0x42, 0x51 }; // Another example
byte mac[6] = {0xDE, 0x00, 0x00, 0x00, 0x00, 0x00}; // First byte will be DE, rest from WiFi MAC


// Option 2: Derive from WiFi MAC (automatically used if manual MAC is commented out)
//byte mac[6]; // Will be initialized in setupEthernet()

IPAddress ip(192, 168, 1, 191);
IPAddress dns(8, 8, 8, 8); // Google DNS
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

EthernetServer server(80);

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

// Ethernet specific pins for ESP32S3
#define ETH_MISO  12
#define ETH_MOSI  11
#define ETH_SCLK  13
#define ETH_CS    14
#define ETH_INT   10
#define ETH_RST   9
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
// Font data for E
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

// Default colors for each letter (will be updated via Ethernet)
RGBColor letterColors[7] = {
  {7, 0, 0},  // S - Red
  {0, 7, 0},  // P - Green
  {0, 0, 7},  // Q - Blue
  {7, 7, 0},  // D - Yellow
  {0, 7, 7},  // C - Cyan
  {7, 0, 7},  // M - Magenta
  {7, 3, 0}   // E - Orange
};

// Ethernet connection status and retry parameters
bool ethernetConnected = false;
unsigned long lastEthernetAttempt = 0;
const unsigned long ethernetRetryInterval = 60000; // 1 minute between connection attempts

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
void setupEthernet();
void handleData(String jsonData);
void checkEthernetConnection();
bool loadColors();
bool saveColors();
String loadPayload();
void savePayload(const String& payload);
void deriveEthernetMAC();
void connectToWiFi();

String lastPayload = ""; // Store the last payload
String currentRequestData = ""; // To store incoming request data

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\nESP32 LED Matrix with Ethernet Color Control Starting...");
  connectToWiFi();

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

  // Initialize Ethernet (non-blocking)
  setupEthernet();
}
void connectToWiFi() {
  Serial.println("Connecting to WiFi...");

  // Set WiFi to station mode
  WiFi.mode(WIFI_MODE_STA);

  // Begin WiFi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Wait for connection with timeout
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  // Check connection status
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Print WiFi MAC address
    uint8_t baseMac[6];
    WiFi.macAddress(baseMac);
    Serial.print("WiFi MAC address: ");
    for (int i = 0; i < 6; i++) {
      if (baseMac[i] < 0x10) Serial.print("0");
      Serial.print(baseMac[i], HEX);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }

  // Disconnect WiFi after getting MAC
  WiFi.disconnect(true);
  WiFi.mode(WIFI_MODE_NULL);
  Serial.println("WiFi disconnected");
}
// Replace the deriveEthernetMAC() function with:
// Replace the deriveEthernetMAC() function with this corrected version:
void deriveEthernetMAC() {
  // First ensure WiFi is initialized
  WiFi.mode(WIFI_MODE_STA);
  delay(100); // Give WiFi time to initialize
  
  // Get MAC address through WiFi interface
  String macStr = WiFi.macAddress();
  macStr.replace(":", "");
  
  // Convert MAC string to bytes
  for (int i = 0; i < 6; i++) {
    String byteStr = macStr.substring(i*2, i*2+2);
    mac[i] = strtol(byteStr.c_str(), NULL, 16);
  }
  
  // Force first byte to DE
  mac[0] = 0xDE;
  
  // Debug output
  Serial.print("Derived Ethernet MAC: ");
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}

void loop() {
  // Check for Ethernet connection
  checkEthernetConnection();

  // Check for any client connections
  EthernetClient client = server.available();
  if (client) {
    Serial.println("Client connected!");
    currentRequestData = "";

    // Read incoming data
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        currentRequestData += c;

        // Look for empty line signaling end of headers
        if (currentRequestData.endsWith("\r\n\r\n")) {
          break;
        }
      }
    }

    // If we have a POST request with data
    if (currentRequestData.indexOf("POST /data") >= 0) {
      // Wait for the request body data
      unsigned long startTime = millis();
      String jsonData = "";

      // Read remaining data which is the JSON payload
      while (client.connected() && (millis() - startTime < 5000)) {
        if (client.available()) {
          char c = client.read();
          jsonData += c;
        }
      }

      if (jsonData.length() > 0) {
        Serial.print("Received JSON data: ");
        Serial.println(jsonData);

        // Process the payload
        savePayload(jsonData);
        handleData(jsonData);

        // Send a response
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println("{\"status\":\"success\"}");
      } else {
        // No data received
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println("{\"error\":\"No data received\"}");
      }
    } else {
      // Not a POST to /data
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<html><body>");
      client.println("<h1>ESP32 LED Matrix Controller</h1>");
      client.println("<p>Send JSON data to /data endpoint using POST</p>");
      client.println("</body></html>");
    }

    // Close the connection
    delay(1);
    client.stop();
    Serial.println("Client disconnected.");
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

// Set up Ethernet connection
void setupEthernet() {
  Serial.println("Initializing Ethernet...");
  
  // Derive MAC first
  deriveEthernetMAC();
  
  // Initialize SPI bus
  SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI, ETH_CS);
  
  // Initialize Ethernet
  Ethernet.init(ETH_CS);
  
  // Start Ethernet connection
  Serial.println("Starting Ethernet connection...");
  
  unsigned long startTime = millis();
  if (Ethernet.begin(mac, 10000)) { // 10 second timeout
    Serial.println("Ethernet connected via DHCP");
    Serial.print("MAC Address: ");
    printMacAddress(mac);
    Serial.print("IP Address: ");
    Serial.println(Ethernet.localIP());
    ethernetConnected = true;
  } else {
    Serial.println("Failed to configure via DHCP, trying static IP...");
    Ethernet.begin(mac, ip, dns, gateway, subnet);
    
    // Check connection status
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet hardware not found!");
      ethernetConnected = false;
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable not connected!");
      ethernetConnected = false;
    } else {
      Serial.println("Ethernet connected with static IP");
      Serial.print("MAC Address: ");
      printMacAddress(mac);
      Serial.print("IP Address: ");
      Serial.println(Ethernet.localIP());
      ethernetConnected = true;
    }
  }
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  lastEthernetAttempt = millis();
}

void printMacAddress(byte mac[]) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}
// Check Ethernet connection status
void checkEthernetConnection() {
  static bool lastStatus = false;
  
  bool currentStatus = (Ethernet.linkStatus() == LinkON);
  
  if (currentStatus != lastStatus) {
    if (currentStatus) {
      Serial.println("Ethernet link is up");
      Serial.print("MAC: ");
      printMacAddress(mac);
      Serial.print("IP: ");
      Serial.println(Ethernet.localIP());
      ethernetConnected = true;
    } else {
      Serial.println("Ethernet link is down");
      ethernetConnected = false;
    }
    lastStatus = currentStatus;
  }
  
  // Maintain DHCP lease
  Ethernet.maintain();
}

// Handle incoming JSON data for color updates
void handleData(String jsonData) {
  // Use ArduinoJson to parse the data
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonData);

  if (error) {
    Serial.print("[ERROR] Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
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

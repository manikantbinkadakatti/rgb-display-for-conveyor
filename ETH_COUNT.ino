#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Ethernet settings
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC address
IPAddress ip(192, 168, 1, 177);                       // IP address
IPAddress gateway(192, 168, 1, 1);                    // Gateway
IPAddress subnet(255, 255, 255, 0);                   // Subnet mask
IPAddress dns(8, 8, 8, 8);                            // DNS server

// MQTT settings
const char* mqttServer = "192.168.1.100";             // MQTT broker IP
const int mqttPort = 1883;                            // MQTT port
const char* mqttTopic = "matrix/counter";             // MQTT topic

// Ethernet specific pins for ESP32S3
#define ETH_MISO  12
#define ETH_MOSI  11
#define ETH_SCLK  13
#define ETH_CS    14
#define ETH_INT   10
#define ETH_RST   9

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
#define OE_PIN  35 // Output Enable

// Matrix dimensions (32x16)
#define PANEL_WIDTH 32
#define PANEL_HEIGHT 16
#define PANEL_CHAIN 1     // Number of chained panels

// Create MatrixPanel_I2S_DMA object
MatrixPanel_I2S_DMA *matrix = nullptr;

// Create Ethernet and MQTT clients
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

// Counter variables
int counter = 0;
char counterText[5]; // enough for 4 digits + null terminator

// Font sizing
#define FONT_WIDTH 5       // Width of each character in pixels
#define FONT_HEIGHT 7      // Height of each character in pixels
#define SCALE_FACTOR 2     // Scaling factor for bold effect

// Error tracking
uint8_t connectionAttempts = 0;
bool ethernetConnected = false;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("ESP32-S3 RGB Matrix Counter with Ethernet and MQTT");
  
  // Initialize the Ethernet shield with specific pins
  SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI, ETH_CS);
  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);  // Reset Ethernet module
  delay(100);
  digitalWrite(ETH_RST, HIGH); // Release reset
  delay(200);
  
  // Configure Ethernet using DHCP
  Serial.println("Initializing Ethernet...");
  Ethernet.init(ETH_CS);
  
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Try to configure using static IP if DHCP failed
    Ethernet.begin(mac, ip, dns, gateway, subnet);
  }
  
  // Give the Ethernet shield time to initialize
  delay(1500);
  
  if (Ethernet.linkStatus() == LinkON) {
    Serial.print("Ethernet connected, IP address: ");
    Serial.println(Ethernet.localIP());
    ethernetConnected = true;
  } else {
    Serial.println("Ethernet connection failed");
  }
  
  // Set up MQTT client
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
  
  // Setup Matrix panel
  initMatrix();
  
  // Display initial counter
  updateCounter();
}

void initMatrix() {
  // Define matrix configuration
  HUB75_I2S_CFG matrixConfig(
    PANEL_WIDTH,   // Width
    PANEL_HEIGHT,  // Height
    PANEL_CHAIN    // Chain length
  );
  
  // Custom pin mapping for ESP32-S3
  matrixConfig.gpio.r1 = R1_PIN;
  matrixConfig.gpio.g1 = G1_PIN;
  matrixConfig.gpio.b1 = B1_PIN;
  matrixConfig.gpio.r2 = R2_PIN;
  matrixConfig.gpio.g2 = G2_PIN;
  matrixConfig.gpio.b2 = B2_PIN;
  matrixConfig.gpio.a = A_PIN;
  matrixConfig.gpio.b = B_PIN;
  matrixConfig.gpio.c = C_PIN;
  matrixConfig.gpio.clk = CLK_PIN;
  matrixConfig.gpio.lat = LAT_PIN;
  matrixConfig.gpio.oe = OE_PIN;
  
  // Display quality settings
  matrixConfig.clkphase = false;
  matrixConfig.driver = HUB75_I2S_CFG::FM6126A;
  
  // Initialize matrix
  matrix = new MatrixPanel_I2S_DMA(matrixConfig);
  matrix->begin();
  matrix->setBrightness(128); // Medium brightness (0-255)
  matrix->clearScreen();
  matrix->setTextWrap(false);
}

void loop() {
  // Handle MQTT connections
  if (ethernetConnected) {
    // Maintain Ethernet connection
    Ethernet.maintain();
    
    // Check MQTT connection and reconnect if needed
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    
    // Process MQTT messages
    mqttClient.loop();
  }
  
  // Check for incoming serial commands
  checkSerialCommands();
}

void reconnectMQTT() {
  // Try to reconnect to MQTT broker
  if (connectionAttempts >= 5) {
    // Retry only every 30 seconds after 5 failed attempts
    static unsigned long lastAttemptTime = 0;
    if (millis() - lastAttemptTime < 30000) return;
    lastAttemptTime = millis();
  }
  
  Serial.print("Attempting MQTT connection...");
  String clientId = "ESP32Matrix-";
  clientId += String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("connected");
    mqttClient.subscribe(mqttTopic);
    connectionAttempts = 0;
    
    // Send a status message upon reconnection
    mqttClient.publish("matrix/status", "Matrix display connected");
  } else {
    Serial.print("failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" try again later");
    connectionAttempts++;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  // Process the command
  processCommand(message);
}

void drawDigit(char c, int16_t x, int16_t y, uint16_t color) {
  // 5x7 font representation (each character is 5 columns x 7 rows)
  // Each digit is represented by 5 bytes (columns), each byte represents 7 rows (LSB = top row)
  const uint8_t font[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
  };

  if (c < '0' || c > '9') return; // Only handle digits
  
  int charIndex = c - '0';
  
  // Draw each pixel of the character with bold effect
  for (int col = 0; col < FONT_WIDTH; col++) {
    for (int row = 0; row < FONT_HEIGHT; row++) {
      if (font[charIndex][col] & (1 << row)) {
        // Draw the pixel with scaling for bold effect
        for (int dy = 0; dy < SCALE_FACTOR; dy++) {
          for (int dx = 0; dx < SCALE_FACTOR; dx++) {
            matrix->drawPixel(x + (col * SCALE_FACTOR) + dx, 
                             y + (row * SCALE_FACTOR) + dy, 
                             color);
          }
        }
      }
    }
  }
}

void updateCounter() {
  // Format counter with leading zeros (0000 format)
  sprintf(counterText, "%04d", counter);
  
  // Clear display
  matrix->clearScreen();
  
  // Define bright RED color
  uint16_t red_color = matrix->color565(255, 0, 0);
  
  // Recalculate dimensions to ensure all digits fit on the 32x16 screen
  // Each digit is FONT_WIDTH(5) * SCALE_FACTOR(2) = 10 pixels wide
  // Plus spacing between digits of 1 pixel
  // Total width needed for 4 digits: 10*4 + 3 = 43 pixels > 32 pixels screen width
  
  // Since 43 > 32, we need to reduce the scale factor or add less spacing
  // Let's reduce the scale factor for width slightly
  int scaleW = 1; // Reduced scale factor for width
  int scaleH = 2; // Keep original height scaling for bold effect
  
  int digitWidth = FONT_WIDTH * scaleW;
  int digitSpacing = 1;
  int totalWidth = (4 * digitWidth) + (3 * digitSpacing);
  
  // Center display horizontally
  int startX = (PANEL_WIDTH - totalWidth) / 2;
  
  // Center display vertically
  int startY = (PANEL_HEIGHT - (FONT_HEIGHT * scaleH)) / 2;
  
  // Draw each digit with adjusted scaling
  for (int i = 0; i < 4; i++) {
    int xPos = startX + i * (digitWidth + digitSpacing);
    
    // Draw digit with custom scaling
    char c = counterText[i];
    if (c < '0' || c > '9') continue; // Only handle digits
    
    int charIndex = c - '0';
    
    // Get font data
    const uint8_t font[10][5] = {
      {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
      {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
      {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
      {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
      {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
      {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
      {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
      {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
      {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
      {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
    };
    
    // Draw each pixel of the character with custom scaling
    for (int col = 0; col < FONT_WIDTH; col++) {
      for (int row = 0; row < FONT_HEIGHT; row++) {
        if (font[charIndex][col] & (1 << row)) {
          // Draw the pixel with different horizontal and vertical scaling
          for (int dy = 0; dy < scaleH; dy++) {
            for (int dx = 0; dx < scaleW; dx++) {
              matrix->drawPixel(xPos + (col * scaleW) + dx, 
                              startY + (row * scaleH) + dy, 
                              red_color);
            }
          }
        }
      }
    }
  }
  
  // Send update to debug
  Serial.print("Counter display updated: ");
  Serial.println(counterText);
  
  // Send the current counter value back to MQTT (optional)
  if (mqttClient.connected()) {
    mqttClient.publish("matrix/current", counterText);
  }
}
void incrementCounter() {
  counter++;
  if (counter > 9999) counter = 0;
  updateCounter();
}

void processCommand(const char* command) {
  Serial.print("Processing command: ");
  Serial.println(command);
  
  if (strcmp(command, "increment") == 0) {
    incrementCounter();
  }
  else if (strncmp(command, "set:", 4) == 0) {
    int newValue = atoi(command + 4);
    counter = constrain(newValue, 0, 9999);
    updateCounter();
  }
  else if (strcmp(command, "reset") == 0) {
    counter = 0;
    updateCounter();
  }
  else if (isdigit(command[0]) || (command[0] == '-' && isdigit(command[1]))) {
    int newValue = atoi(command);
    counter = constrain(newValue, 0, 9999);
    Serial.print("Setting counter to: ");
    Serial.println(newValue);
    updateCounter();
  }
}

void checkSerialCommands() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    processCommand(input.c_str());
  }
}

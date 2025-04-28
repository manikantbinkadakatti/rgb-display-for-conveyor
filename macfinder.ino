#include <Arduino.h>
#include <WiFi.h>

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32 MAC Address - Universal Method");
  
  // Reset WiFi to make sure we get a clean state
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Get and print MAC address using standard WiFi library
  Serial.print("WiFi STA MAC: ");
  Serial.println(WiFi.macAddress());
  
  // Try WiFi.softAPmacAddress() too
  WiFi.mode(WIFI_AP);
  delay(100);
  Serial.print("WiFi AP MAC: ");
  Serial.println(WiFi.softAPmacAddress());
  
  // Return to station mode
  WiFi.mode(WIFI_STA);
}

void loop() {
  // Nothing to do here
  delay(1000);
}

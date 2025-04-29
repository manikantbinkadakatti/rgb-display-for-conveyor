# RGB Display for Conveyor Belt


## Overview
A customizable scrolling RGB display system designed for industrial conveyor belts. Controlled using an ESP32 microcontroller, the display allows text messages to be updated dynamically via a web configuration page or Excel sheets over Wi-Fi or Ethernet.

## Features
- Customizable scrolling text messages
- Adjustable text color, speed, and size
- Web interface for Wi-Fi SSID, password, and IP configuration
- Excel sheet input for fast industrial updates
- ESP32-based control for wireless or wired communication

## Technologies Used
- ESP32 (Arduino framework)
- HTML/CSS (Webpage design)
- Ethernet/Wi-Fi Networking
- RGB LED Display Panels
- Excel Sheet Integration (for input)

## How to Use
1. Upload the Arduino code to the ESP32 microcontroller.
2. Connect the RGB display panel according to provided pin mappings.
3. Access the Web Interface to configure:
   - Wi-Fi SSID
   - Password
   - Static or Dynamic IP settings
4. Input text to be displayed either via the web interface or Excel file method.
5. Power the system and monitor the scrolling display.


## Future Improvements
- Add remote text updating using mobile apps
- Enhance font styles and add animation effects
- OTA (Over-The-Air) updates for firmware


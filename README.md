# ESP Utils ESP8266 
`arduino code for ESP Utils project, utilities for ESP8266 and ESP32.`

[![Codacy Badge](https://app.codacy.com/project/badge/Grade/9f2e4183d2a341398ac1fc88ed7a0788)](https://www.codacy.com/gh/meudayhegde/ESPUtils_ESP8266/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=meudayhegde/ESPUtils_ESP8266&amp;utm_campaign=Badge_Grade) 
[![version](https://img.shields.io/badge/version-1.1.0-blue.svg)](https://github.com/meudayhegde/ESPUtils_ESP8266)
[![status](https://img.shields.io/badge/status-alpha-gold.svg)](https://github.com/meudayhegde/ESPUtils_ESP8266)
[![IDE](https://img.shields.io/badge/developed_in-Arduino_IDE-cyan.svg)](https://www.arduino.cc/en/software)

### Libraries used
[![status](https://img.shields.io/badge/ArduinoJson-6.20.0-blue.svg)](https://arduinojson.org/v6/doc/)
[![status](https://img.shields.io/badge/IRremoteESP8266-2.8.4-blue.svg)](https://github.com/crankyoldgit/IRremoteESP8266)

### Releases
[![DOWNLOAD](https://img.shields.io/badge/Downloads-1.1.0_alpha-darkgreen.svg)](https://github.com/meudayhegde/ESPUtils_ESP8266/releases/tag/release-1.1.0-alpha)

#### Android interface: [ESP Utils Android application](https://github.com/meudayhegde/ESPUtils_android)

`This module is to be installed on ESP8266 / ESP32 Device.`
#### Features
- `Wireless switch: Can be used as wireless switch through android app connected by wifi network.`
- `Wireless Infrared Controller: Infrared controllers can be captured and remodeled on android application.`

## How to install
### Method 1: Through Arduino IDE
1. Download and install [**Arduino IDE**](https://www.arduino.cc/en/software) on your Windows/Mac/Linux Environment.
2. Setup Arduino IDE for ESP8266/ESP32 development. websearch for the steps, Here is a [**Sample Guide**](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
3. [**Download This Project**](https://github.com/meudayhegde/ESPUtils_ESP8266.git) (or using git clone). Extract and open the project on Arduino ide.
4. Follow the procedure and upload the sketch on to your ESP Device.
5. And Done. you can start configuring the device using [**ESP Utils Android application**](https://github.com/meudayhegde/ESPUtils_android)
### Method 2: Using esptool
1. Install python3 from <b>[python.org](https://www.python.org/downloads)</b>
2. Install module <b>esptool</b><br>```python3 -m pip install esptool```
3. Install USB Driver (CH340G/CP2102 or other appropriate driver depending on serial to usb converter used) if not installed.
4. Download system bin file from <b>[Releases](https://github.com/meudayhegde/ESPUtils_ESP8266/releases)</b>
5. Connect ESP Device via USB and execute command, <br>
```python3 -m esptool --port PORT write_flash 0x00000 path_to_ESPUtils_ESPxxxxx_xx.xx.bin``` <br> usual value for PORT on windows is COM8, for Linux /dev/ttyUSB0


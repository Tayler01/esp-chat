# esp-chat
A esp32 project for chat on airgapped PCs

## Required Hardware
* ESP32 development board (e.g. ESP32 DevKitC)
* USB cable for flashing
* Access to a Wi‑Fi network used to sync messages

## Arduino Libraries
The sketch relies on the ESP32 Arduino core and the following libraries which can be installed from the Arduino Library Manager:

* **ArduinoJson**
* **Preferences** (part of the ESP32 core)
* **WiFi** / **WiFiClientSecure** / **HTTPClient** (part of the ESP32 core)

## Building and Uploading
1. Install the [Arduino IDE](https://www.arduino.cc/en/software) or `arduino-cli`.
2. Add the ESP32 boards package using the Boards Manager with the URL:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Select your ESP32 board (e.g. **ESP32 Dev Module**).
4. Open `espChat.ino` and compile/upload in the IDE, or run:

   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32 espChat.ino
   arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 espChat.ino
   ```

## Serial Commands
After upload, open a serial monitor at **115200 baud**. Type `!help` to see commands:

* `!setname [name]` – set display name
* `!setid [id]` – set user ID
* `!setcolor [hex]` – set avatar color
* `!setssid [ssid]` – set SSID (requires `!reboot`)
* `!setpass [pass]` – set password (requires `!reboot`)
* `!show` – show current config values
* `!reset` – reset all settings to default
* `!reboot` – restart device
* `!ip` – show IP address

# 🎤 ESP32 INMP441 Audio Recorder (Mono)

This project demonstrates how to use the **ESP32** with an **INMP441 I2S microphone** to record audio, save it as a `.wav` file on an **SD card**, and timestamp the recordings using **NTP time synchronization** over Wi-Fi.

---

## Features
- Records audio at **16 kHz, 16-bit, mono** using INMP441 (I2S).
- Stores audio files on an **SD card** in `.wav` format.
- Filenames include **date & recording number** for organization.
- External LED status:
  - **Blink once**: System ready (WiFi + Time + SD initialized)
  - **On**: Recording active
  - **Off**: Idle (not recording)
- Push-button control (start/stop recording)

---

## 🛠️ Hardware Required
- ESP32 Development Board  
- INMP441 I2S microphone  
- microSD card module (SPI-based, 3.3V logic, FAT32)  
- Push button (active LOW to GND)  
- External LED + 220Ω resistor
  
---

## 🔌 Wiring Diagram
 ![Error Loading Image](https://github.com/MuhammadZia-ud-Din/ESP32_INMP441_Recorder/blob/3e39a0bba5d864682aab786129e2e539b9f7c213/Images/Schematic%20%20Design.png)
## 🔌 Pin Connections

| ESP32 Pin | INMP441 | SD Card | LED | Button |
|-----------|---------|---------|-----|--------|
| 26        | SCK     | -       | -   | -      |
| 33        | WS      | -       | -   | -      |
| 32        | SD      | -       | -   | -      |
| 5         | -       | CS      | -   | -      |
| 14        | -       | -       | -   | BTN → RES → 3V3 |
| 13        | -       | -       | LED+ | -      |
| GND       | L/R     | GND     | LED- → RES → GND | BTN-|
| 3.3V      | VCC     | VCC     | -   | -      |

**Other connections:**
- SD card module CS → GPIO 5  
- Button → GPIO 14 (active LOW, use internal pull-up)  
- Built-in LED → GPIO 2 (active LOW)  
- Status LED → GPIO 13 with resistor in series for protection.

---

## ⚙️ Setup Instructions

### 1. Install Dependencies
- Arduino IDE (or PlatformIO in VS Code)
- ESP32 board support:
  - Arduino IDE → *Preferences* → Additional Boards Manager URL:  
    ```
    https://espressif.github.io/arduino-esp32/package_esp32_index.json
    ```
- Libraries (pre-installed in ESP32 core):
  - `WiFi.h`
  - `time.h`
  - `FS.h`, `SD.h`, `SPI.h`
  - `driver/i2s.h`

### 2. Configure WiFi
Update in `main.cpp`:
```cpp
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

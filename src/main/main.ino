#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include "driver/i2s.h"

// ---------------- USER CONFIG ----------------
#define SD_CS       5        // SD card CS pin
#define BUTTON_PIN  14       // Push button pin (active LOW)
#define LED_SYS     13       // External LED: system readiness blink + Recording
#define SAMPLE_RATE 16000    // 16kHz
#define BITS_PER_SAMPLE 16
#define CHANNELS    1

// INMP441 connections
#define I2S_WS   33   // LRCL / WS
#define I2S_SD   32   // DOUT from mic
#define I2S_SCK  26   // BCLK

const char* ssid     = "SSID"; // Your SSID here
const char* password = "Password"; // Your Password here
const long gmtOffset_sec = 5 * 3600;  // UTC+5
const int daylightOffset_sec = 0;

// ---------------- GLOBALS ----------------
File audioFile;
bool recording = false;
int recordCount = 0;
uint8_t i2sBuffer[1024];
int16_t sampleBuffer[512];
int bufIndex = 0;


// ---------------- WAV HELPERS ----------------
void writeWavHeader(File &f, uint32_t sampleRate, uint16_t bitsPerSample, uint16_t channels) {
  byte header[44];
  int byteRate = sampleRate * channels * bitsPerSample / 8;
  int blockAlign = channels * bitsPerSample / 8;

  memcpy(header, "RIFF", 4);
  *(uint32_t*)(header + 4) = 0; // placeholder
  memcpy(header + 8, "WAVEfmt ", 8);
  *(uint32_t*)(header + 16) = 16;
  *(uint16_t*)(header + 20) = 1;
  *(uint16_t*)(header + 22) = channels;
  *(uint32_t*)(header + 24) = sampleRate;
  *(uint32_t*)(header + 28) = byteRate;
  *(uint16_t*)(header + 32) = blockAlign;
  *(uint16_t*)(header + 34) = bitsPerSample;
  memcpy(header + 36, "data", 4);
  *(uint32_t*)(header + 40) = 0; // placeholder

  f.write(header, 44);
}

void finalizeWav(File &f) {
  uint32_t fileSize = f.size();
  uint32_t dataSize = fileSize - 44;
  uint32_t chunkSize = fileSize - 8;

  f.seek(4);
  f.write((uint8_t*)&chunkSize, 4);
  f.seek(40);
  f.write((uint8_t*)&dataSize, 4);
}

// ---------------- FILE NAMING ----------------
String makeFilename(int count) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String("/INMP441_unknown_" + String(count) + ".wav");
  }
  char dateStr[16];
  strftime(dateStr, sizeof(dateStr), "%Y%m%d", &timeinfo);
  char fname[32];
  sprintf(fname, "/INMP441_%s_%02d.wav", dateStr, count);
  return String(fname);
}

// ---------------- I2S CONFIG ----------------
void i2s_init() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}


// ---------------- RECORDING ----------------
void startRecording() {
  recordCount++;
  String filename = makeFilename(recordCount);
  audioFile = SD.open(filename.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("File open failed!");
    return;
  }
  writeWavHeader(audioFile, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS);
  recording = true;

  digitalWrite(LED_SYS, HIGH);  // Built-in LED ON while recording

  Serial.print("Recording started: ");
  Serial.println(filename);
}

void stopRecording() {
  if (!recording) return;
  finalizeWav(audioFile);
  audioFile.close();
  recording = false;

  digitalWrite(LED_SYS, LOW);   // Built-in LED OFF when stopped

  Serial.println("Recording stopped and saved.");
}

// ---------------- BUTTON ----------------
void handleButton() {
  static bool lastState = HIGH;
  bool currentState = digitalRead(BUTTON_PIN);

  if (lastState == HIGH && currentState == LOW) {
    if (!recording) startRecording();
    else stopRecording();
    delay(200);
  }
  lastState = currentState;
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_SYS, OUTPUT);
  digitalWrite(LED_SYS, LOW);

  // WiFi for NTP
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  // NTP
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Time OK");

  // SD init
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed!");
    while (1);
  }
  Serial.println("SD OK");

  // System ready blink
  digitalWrite(LED_SYS, HIGH);
  delay(500);
  digitalWrite(LED_SYS, LOW);

  i2s_init();
}

// ---------------- LOOP ----------------
void loop() {
  handleButton();

  if (recording) {
    size_t bytesRead;
    i2s_read(I2S_NUM_0, (void*)i2sBuffer, sizeof(i2sBuffer), &bytesRead, portMAX_DELAY);

    int samples = bytesRead / 4; // 32-bit per sample from I2S
    int32_t *raw = (int32_t*)i2sBuffer;

    for (int i = 0; i < samples; i++) {
      int16_t s = (raw[i] >> 14); // convert 32-bit I2S to 16-bit
      sampleBuffer[bufIndex++] = s;
      if (bufIndex >= 512) {
        audioFile.write((uint8_t*)sampleBuffer, sizeof(sampleBuffer));
        bufIndex = 0;
      }
    }
  }
}

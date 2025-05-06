#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <Update.h>
#include "esp_rom_crc.h"

/* ---- gateway radio / HTTP parameters ---- */
#define GATEWAY_SSID "EventHorizonOTA"
#define GATEWAY_PASS "otapass"
#define GATEWAY_FW_URL "http://192.168.4.1/latest.bin"

struct __attribute__((packed)) OtaBeacon {
  uint32_t size;
  uint32_t crc;
};

/* =========== single‑header helper class =========== */
class OtaClient {
 public:
  void begin(uint8_t wifiChannel = 0)
  {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
      Serial.println("ESP-NOW init fail"); return;
    }
    esp_now_register_recv_cb(onBeaconStatic);
    instance = this;
  }

 private:
  static OtaClient *instance;

  /* → static shim because ESP‑NOW uses C callbacks */
  static void onBeaconStatic(const uint8_t *, const uint8_t *d, int len) {
    if (instance && len == sizeof(OtaBeacon))
      instance->handleBeacon(*(OtaBeacon *)d);
  }

  void handleBeacon(const OtaBeacon &b) {
    Serial.printf("\n⚡ OTA beacon  size=%u  crc=0x%08X\n", b.size, b.crc);
    performHttpUpdate(b.size, b.crc);
  }

  void performHttpUpdate(uint32_t expectedSize, uint32_t expectedCrc)
  {
    /* connect to gateway AP */
    Serial.print("Connecting to gateway … ");
    WiFi.begin(GATEWAY_SSID, GATEWAY_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
      delay(250); Serial.print('.');
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\n✗ Wi‑Fi connect failed"); return;
    }
    Serial.printf("\nIP %s\n", WiFi.localIP().toString().c_str());

    /* HTTP GET */
    HTTPClient http;
    if (!http.begin(GATEWAY_FW_URL)) { Serial.println("HTTP begin err"); return; }
    int code = http.GET();
    if (code != HTTP_CODE_OK) { Serial.printf("HTTP err %d\n", code); return; }

    int len = http.getSize();
    if (len != (int)expectedSize) Serial.println("⚠ Size mismatch - continuing");

    if (!Update.begin(len)) { Serial.println("Update begin fail"); return; }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[256]; size_t written = 0;
    uint32_t crc = 0xFFFFFFFF;

    while (http.connected() && written < (size_t)len) {
      size_t n = stream->readBytes(buf, sizeof buf);
      if (n) {
        Update.write(buf, n);
        crc = esp_rom_crc32_le(crc, buf, n);
        written += n;
      }
    }
    crc ^= 0xFFFFFFFF;

    if (expectedCrc && crc != expectedCrc) {
      Serial.println("✗ CRC mismatch - aborting");
      Update.abort(); return;
    }
    if (!Update.end(true)) {
      Serial.printf("✗ Update error: %s\n", Update.errorString()); return;
    }
    Serial.println("✓ OTA success - rebooting");
    ESP.restart();
  }
};
inline OtaClient *OtaClient::instance = nullptr;

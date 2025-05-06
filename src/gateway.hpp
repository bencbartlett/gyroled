#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "esp_rom_crc.h"

#ifndef AP_SSID        // can be overridden from build_flags
#define AP_SSID  "EventHorizonOTA2"
#define AP_PASS  "updateeventhorizon!1"
#endif

#define FW_PATH  "/latest.bin"      // where we store the upload

AsyncWebServer server(80);
File           uploadFile;

/* ------------  tiny 12‑byte beacon ------------- */
struct __attribute__((packed)) OtaBeacon {
  uint32_t size;                    // bytes
  uint32_t crc;                     // CRC‑32 of the file (little‑endian)
};

/* -------- CRC helper (SPIFFS → crc32) ---------- */
uint32_t crc32_file(const char *path)
{
  File f = SPIFFS.open(path, "r");
  uint32_t crc = 0xFFFFFFFF;
  uint8_t  buf[256];

  while (f && f.available()) {
    int n = f.read(buf, sizeof buf);
    crc = esp_rom_crc32_le(crc, buf, n);
  }
  if (f) f.close();
  return crc ^ 0xFFFFFFFF;
}

/* -------- broadcast to everyone on the channel -------- */
void sendBeacon(uint32_t size, uint32_t crc)
{
  OtaBeacon b{size, crc};
  esp_now_send(nullptr, (uint8_t *)&b, sizeof b);   // NULL ⇒ broadcast
}

/* ----------------- ESP‑NOW setup ---------------- */
void setupEspNow()
{
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED!");
    return;
  }
  esp_now_peer_info_t peer{};
  memset(&peer, 0, sizeof peer);
  for (int i = 0; i < 6; ++i) peer.peer_addr[i] = 0xFF;   // FF:FF:FF:FF:FF:FF
  peer.ifidx   = WIFI_IF_AP;                              // works on AP iface
  peer.channel = 0;                                       // "all"
  esp_now_add_peer(&peer);
}

/* ---------------- HTTP + upload ----------------- */
void setupWeb()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain",
            "ESP32 OTA gateway - POST your firmware to /upload");
  });

  /* stream the incoming POST straight into SPIFFS */
  server.on(
      "/upload", HTTP_POST,
      /* called when entire body received */
      [](AsyncWebServerRequest *req) {
        if (uploadFile) uploadFile.close();

        uint32_t sz  = SPIFFS.open(FW_PATH, "r").size();
        uint32_t crc = crc32_file(FW_PATH);
        Serial.printf("▲ Upload complete: %u bytes  CRC=0x%08X\n", sz, crc);

        sendBeacon(sz, crc);                       // alert the swarm ✈️
        req->send(200, "text/plain", "OK");
      },
      /* body handler – streamed in chunks */
      [](AsyncWebServerRequest *, String, size_t idx,
         uint8_t *data, size_t len, bool final) {
        if (idx == 0) {
          Serial.println("Receiving firmware …");
          uploadFile = SPIFFS.open(FW_PATH, "w");
        }
        if (uploadFile) uploadFile.write(data, len);
        if (final && uploadFile) uploadFile.close();
      });

  /* static GET so clients pull /latest.bin */
  server.serveStatic("/latest.bin", SPIFFS, FW_PATH);
  server.begin();
}

/* ---------------------- SETUP ------------------- */
void setupGateway()
{
  Serial.begin(115200);
  delay(5000);
  if (!SPIFFS.begin(true)) Serial.println("✗ SPIFFS mount failed");

  /* start the Soft‑AP */
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("Gateway up →  SSID:%s  IP:%s\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());

  setupWeb();
  setupEspNow();

  /* if a firmware is already present, beacon it at boot */
  if (SPIFFS.exists(FW_PATH)) {
    uint32_t sz  = SPIFFS.open(FW_PATH, "r").size();
    uint32_t crc = crc32_file(FW_PATH);
    sendBeacon(sz, crc);
    Serial.printf("Beaconed existing build  (%u bytes)\n", sz);
  }
}

/* ---------------------- LOOP -------------------- */
void loopGateway() {
  /* everything is event‑driven – nothing to do here */
}
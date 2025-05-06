#include <WiFi.h>
#include <ArduinoOTA.h>

// #ifndef OTA_SSID                // allow overrides from build flags
#define OTA_SSID      "YourWiFi"
#define OTA_PSK       "YourPass"
#define OTA_HOSTNAME  "esp32-%06llx"      // default: MAC address
#define OTA_PASS      "otapass"           // shared by update script
// #endif

void setupOTA()
{
  Serial.begin(115200);

  /* --- connect to Wi‑Fi -------------------------------------------------- */
  WiFi.mode(WIFI_STA);
  WiFi.begin(OTA_SSID, OTA_PSK);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    Serial.print('.');
  }
  Serial.printf("\nConnected: %s\nIP: %s\n",
                OTA_SSID, WiFi.localIP().toString().c_str());

  /* --- bring ArduinoOTA up ---------------------------------------------- */
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);

  ArduinoOTA
      .onStart([]() { Serial.println("\nOTA start"); })
      .onEnd([]()   { Serial.println("\nOTA end, rebooting…"); })
      .onProgress([](unsigned p, unsigned t)
                  { Serial.printf("Progress: %u%%\r", (p * 100) / t); })
      .onError([](ota_error_t e)
               { Serial.printf("Error[%u] => rebooting\n", e); });

  ArduinoOTA.begin();          // advertises _arduino._tcp on port 3232 
}

void loopOTA()
{
  ArduinoOTA.handle();         // keep this near the top
}
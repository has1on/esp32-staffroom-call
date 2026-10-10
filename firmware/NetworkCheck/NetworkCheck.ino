// Temporary network-only check: no TF, DAC or LCD access.
// Use only an approved 2.4 GHz Wi-Fi network. Replace with role firmware later.
#include <Arduino.h>
#include <WiFi.h>

const char *WIFI_SSID = "CHANGE_ME";
const char *WIFI_PASSWORD = "CHANGE_ME";
uint32_t lastTry = 0;
bool connectedLogged = false;

void setup() {
  Serial.begin(115200);
  delay(1200);
  WiFi.mode(WIFI_STA);
  Serial.printf("NETWORK CHECK MAC=%s\n", WiFi.macAddress().c_str());
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastTry = millis();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!connectedLogged) {
      connectedLogged = true;
      Serial.printf("CONNECTED IP=%s MAC=%s\n",
                    WiFi.localIP().toString().c_str(),
                    WiFi.macAddress().c_str());
    }
  } else {
    connectedLogged = false;
    if (millis() - lastTry > 10000) {
      lastTry = millis();
      Serial.println("Waiting for approved 2.4GHz Wi-Fi...");
      WiFi.reconnect();
    }
  }
  delay(50);
}
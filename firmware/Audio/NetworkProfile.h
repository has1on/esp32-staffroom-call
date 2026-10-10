#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include <esp_eap_client.h>
#include <ArduinoJson.h>
#include "config.h"
#include "NetworkDefaults.h"
#include "NetworkValidation.h"
struct NetworkProfile {
 String ssid,password,username,identity,audioHost,matrixHost;
 uint8_t mode=0;
};
inline bool networkValid(const NetworkProfile &p) {
 return staffroom::validNetworkFields(p.ssid.c_str(),p.password.c_str(),p.mode,p.username.c_str(),p.identity.c_str(),String(WIFI_EAP_CA).length()>0,ALLOW_INSECURE_EAP);
}
inline void networkToJson(const NetworkProfile&p,JsonDocument&d) {
 d["version"]=1;d["ssid"]=p.ssid;d["password"]=p.password;d["username"]=p.username;
 d["identity"]=p.identity;d["mode"]=p.mode;d["audioHost"]=p.audioHost;d["matrixHost"]=p.matrixHost;
}
inline bool networkFromJson(JsonDocument&d,NetworkProfile&p) {
 if(d["version"]!=1||!d["mode"].is<uint8_t>()||!d["ssid"].is<String>()||!d["password"].is<String>())return false;
 p.mode=d["mode"].as<uint8_t>();p.ssid=d["ssid"].as<String>();p.password=d["password"].as<String>();
 p.username=d["username"]|"";p.identity=d["identity"]|"";p.audioHost=d["audioHost"]|"";p.matrixHost=d["matrixHost"]|"";
 return networkValid(p);
}
inline NetworkProfile networkDefaults(const char*audio="",const char*matrix="") {
 NetworkProfile p;p.ssid=WIFI_SSID;p.password=WIFI_PASSWORD;p.mode=WIFI_ENTERPRISE?WIFI_EAP_METHOD:0;
 p.username=WIFI_EAP_USERNAME;p.identity=WIFI_EAP_IDENTITY;p.audioHost=audio;p.matrixHost=matrix;return p;
}
inline bool saveNetwork(const NetworkProfile&p) {
 if(!networkValid(p))return false;JsonDocument d;networkToJson(p,d);String text;serializeJson(d,text);
 Preferences n;if(!n.begin("staffnet",false))return false;bool ok=n.putString("profile",text)==text.length();n.end();return ok;
}
inline NetworkProfile loadNetwork(const char*audio="",const char*matrix="") {
 NetworkProfile p=networkDefaults(audio,matrix);Preferences n;
 if(!n.begin("staffnet",true))return p;String text=n.getString("profile","");n.end();
 JsonDocument d;NetworkProfile saved;if(text.length()<=2048&&!deserializeJson(d,text)&&networkFromJson(d,saved))p=saved;return p;
}
inline void eraseNetwork(){Preferences n;if(n.begin("staffnet",false)){n.remove("profile");n.end();}}
inline void joinNetwork(const NetworkProfile&p) {
 // mode is initialized by the caller BEFORE WebServer::begin().
 WiFi.disconnect(false,false);WiFi.setAutoReconnect(true);
 if(!networkValid(p))return;
 if(p.mode==0){esp_wifi_sta_enterprise_disable();WiFi.setMinSecurity(p.password.isEmpty()?WIFI_AUTH_OPEN:WIFI_AUTH_WPA2_PSK);WiFi.begin(p.ssid.c_str(),p.password.isEmpty()?nullptr:p.password.c_str());}
#if CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT
 else {WiFi.setMinSecurity(WIFI_AUTH_WPA2_ENTERPRISE);
  WiFi.begin(p.ssid.c_str(),p.mode==2?WPA2_AUTH_TTLS:WPA2_AUTH_PEAP,p.identity.c_str(),p.username.c_str(),p.password.c_str(),String(WIFI_EAP_CA).isEmpty()?nullptr:WIFI_EAP_CA,nullptr,nullptr,p.mode==2?ESP_EAP_TTLS_PHASE2_MSCHAPV2:-1);}
#endif
}
inline String setupSSID(){return String("Staffroom-")+SYSTEM_ID+"-Audio";}
inline String setupPassword(){return String(DEVICE_KEY).substring(0,12);}
inline bool adminPinReady(){String p=NETWORK_ADMIN_PIN;if(p.length()<6||p.length()>12)return false;for(size_t i=0;i<p.length();i++){char c=p[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')))return false;}return true;}

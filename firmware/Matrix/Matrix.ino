// 64x32 HUB75 display, rendered with generated UTF-8 bitmap glyphs.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "config.h"
#include "NetworkProfile.h"
#include "BitmapText.h"

WebServer server(80);MatrixPanel_I2S_DMA*panel=nullptr;
String line1,line2,currentId,bootId;uint32_t receivedAt=0,holdMs=20000,lastDraw=0,lastWifiTry=0;
struct Seen{String id;uint32_t hash=0,at=0;};Seen seen[128];int nextSeen=0;
uint32_t hashText(const String&s){uint32_t h=2166136261UL;for(size_t i=0;i<s.length();i++){h^=(uint8_t)s[i];h*=16777619;}return h;}
void response(int status,bool ok,const String&error=""){JsonDocument d;d["ok"]=ok;d["requestId"]=currentId;if(error.length())d["error"]=error;String s;serializeJson(d,s);server.sendHeader("Cache-Control","no-store");server.send(status,"application/json; charset=utf-8",s);}
bool authorized(){if(server.header("X-Device-Key")!=DEVICE_KEY||String(DEVICE_KEY).length()<32){response(403,false,"unauthorized");return false;}return true;}
void receive(){
 if(!authorized())return;if(!panel){response(503,false,"panel_not_confirmed");return;}
 JsonDocument d;if(server.arg("plain").length()>1024||deserializeJson(d,server.arg("plain"))){response(400,false,"invalid_json");return;}
 if(String(d["systemId"]|"")!=SYSTEM_ID){response(403,false,"wrong_system");return;}
 String id=d["requestId"]|"",a=d["line1"]|"",b=d["line2"]|"";
 // line2 may be empty: a single-line banner (the welcome message) then draws
 // only line1, vertically centred, and still auto-scrolls because width > 64.
 if(id.length()<1||id.length()>64||a.length()<1||a.length()>120||b.length()>90){response(400,false,"invalid_text");return;}
 uint32_t hash=hashText(a+"\n"+b);
 for(int i=0;i<128;i++)if(seen[i].id==id&&millis()-seen[i].at<600000){if(seen[i].hash!=hash){response(409,false,"request_conflict");return;}JsonDocument ack;ack["ok"]=true;ack["requestId"]=id;ack["duplicate"]=true;String out;serializeJson(ack,out);server.send(200,"application/json",out);return;}
 seen[nextSeen]={id,hash,millis()};nextSeen=(nextSeen+1)%128;currentId=id;line1=a;line2=b;receivedAt=millis();
 holdMs=max((uint32_t)20000,(uint32_t)(max(textWidth(a),textWidth(b))+64)*70+3000);response(200,true);
}
void drawLine(const String&s,int y,uint16_t color){int width=textWidth(s),x=(64-width)/2;
 if(width>64){int offset=(millis()-receivedAt)/70%(width+64);x=64-offset;}
 bitmapText(s,x,y,color,[](int x,int y,uint16_t c){if(x>=0&&x<64&&y>=0&&y<32)panel->drawPixel(x,y,c);});
}
void setup(){
 Serial.begin(115200);bootId=String(esp_random(),HEX);
 if(PANEL_CONFIRMED){HUB75_I2S_CFG c(64,32,1);c.gpio={4,5,6,7,15,16,18,8,3,42,-1,40,2,41};c.clkphase=PANEL_CLKPHASE;c.driver=PANEL_FM6126A?HUB75_I2S_CFG::FM6126A:HUB75_I2S_CFG::SHIFTREG;
  panel=new MatrixPanel_I2S_DMA(c);if(!panel->begin()){delete panel;panel=nullptr;}else {panel->setBrightness8(PANEL_BRIGHTNESS);panel->clearScreen();}}
 const char*headers[]={"X-Device-Key"};server.collectHeaders(headers,1);
 server.on("/health",HTTP_GET,[](){JsonDocument d;d["ok"]=true;d["role"]="MATRIX";d["ready"]=panel!=nullptr;d["systemId"]=SYSTEM_ID;d["bootId"]=bootId;String out;serializeJson(d,out);server.send(200,"application/json",out);});
 server.on("/display",HTTP_POST,receive);server.onNotFound([](){response(404,false,"not_found");});WiFi.mode(WIFI_STA);server.begin();WiFi.setAutoReconnect(true);joinNetwork(loadNetwork("", ""));
 Serial.printf("MATRIX panel ready=%d\n",panel!=nullptr);
}
void loop(){
 static bool logged=false;if(WiFi.status()==WL_CONNECTED&&!logged){logged=true;Serial.printf("MATRIX IP=%s MAC=%s\n",WiFi.localIP().toString().c_str(),WiFi.macAddress().c_str());}if(WiFi.status()!=WL_CONNECTED)logged=false;
 server.handleClient();if(WiFi.status()!=WL_CONNECTED&&millis()-lastWifiTry>10000){lastWifiTry=millis();WiFi.reconnect();}
 if(panel&&millis()-lastDraw>=50){lastDraw=millis();panel->clearScreen();
  if(WiFi.status()!=WL_CONNECTED){drawLine("網絡離線",7,panel->color565(255,60,30));}
  else if(currentId.length()&&millis()-receivedAt<holdMs){drawLine(line1,0,panel->color565(255,255,255));drawLine(line2,16,panel->color565(255,205,50));}
  else {drawLine("教員室",7,panel->color565(70,170,180));}
 }
 delay(2);
}

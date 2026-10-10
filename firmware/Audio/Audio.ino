// Staffroom v0.1 - local LAN prototype, not a production-certified release.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <SD.h>
#include <driver/i2s_std.h>
#include <esp_system.h>
#include "config.h"
#include "NetworkProfile.h"
#include <ESPmDNS.h>
#include "Roster.h"

WebServer server(80);
const int CAPACITY=8, HISTORY=128;
const uint32_t HISTORY_MS=10*60*1000UL, COOLDOWN_MS=30000;
struct CallRecord {
 String requestId, studentId, teacherId, studentFile, teacherFile, line1, line2, state, displayState, error, sourceIp;
 uint32_t acceptedAt=0; bool used=false;
};
CallRecord records[HISTORY];
int fifo[CAPACITY], qHead=0, qCount=0;bool playing=false, ready=false;
String bootId, startupError;uint32_t lastWifiTry=0,downSince=0;bool autoSetup=false;
NetworkProfile netProfile;
bool awaitingStaAfterApply=false, setupActive=false, pendingNetwork=false, ipPending=false, networkSpeaking=false, mdnsRunning=false;
uint32_t setupStarted=0, applyAt=0;
String lastConnectedIp, lastSpokenIp, serialCommand;
NetworkProfile nextNetwork;

// Local delta: one browser session token at a time. A teacher opens the page on
// a phone; a second login simply replaces the first, which is the behaviour a
// single-operator console wants.
String consoleToken;uint32_t consoleTokenAt=0,consoleLockUntil=0;uint8_t consoleFails=0;


void reply(int code,JsonDocument&d){String s;serializeJson(d,s);server.sendHeader("Cache-Control","no-store");server.send(code,"application/json; charset=utf-8",s);}
void fail(int code,const String&error){JsonDocument d;d["ok"]=false;d["error"]=error;reply(code,d);}
// Local delta: accepts either the device key (Door, PC tool) or a browser
// session token minted by /console-unlock.
bool consoleTokenValid(){return consoleToken.length()==32&&(uint32_t)(millis()-consoleTokenAt)<CONSOLE_SESSION_MS;}
bool authorized(){
 const String k=server.header("X-Device-Key");
 if(k.length()&&k==DEVICE_KEY&&String(DEVICE_KEY).length()>=32)return true;
 if(consoleTokenValid()&&k==consoleToken)return true;
 fail(403,"unauthorized");return false;}
bool parseRequest(JsonDocument&d){if(server.arg("plain").length()>1024||deserializeJson(d,server.arg("plain"))){fail(400,"invalid_json");return false;}
 if(String(d["systemId"]|"")!=SYSTEM_ID){fail(403,"wrong_system");return false;}return true;}
int findRequest(const String&id){for(int i=0;i<HISTORY;i++)if(records[i].used&&records[i].requestId==id&&(millis()-records[i].acceptedAt<=HISTORY_MS||records[i].state=="queued"||records[i].state=="playing"))return i;return -1;}
int queuePosition(int index){for(int i=0;i<qCount;i++)if(fifo[(qHead+i)%CAPACITY]==index)return i+1;return 0;}
void recordReply(int i,int code,bool duplicate=false){const CallRecord&r=records[i];JsonDocument d;d["ok"]=true;d["requestId"]=r.requestId;d["state"]=r.state;d["displayState"]=r.displayState;d["queuePosition"]=queuePosition(i);d["duplicate"]=duplicate;d["bootId"]=bootId;if(r.error.length())d["error"]=r.error;reply(code,d);}
bool validWav(const String&path){File f=SD.open(path,FILE_READ);staffroom::WavInfo w;std::string e;return f&&staffroom::wavInfo(f,w,e);}

void handleAnnounce(){
 if(!authorized())return;JsonDocument d;if(!parseRequest(d))return;
 String id=d["requestId"]|"",sid=d["studentId"]|"",tid=d["teacherId"]|"";
 if(!staffroom::identifier(std::string(id.c_str()),64)||!staffroom::studentId5(std::string(sid.c_str()))||!staffroom::identifier(std::string(tid.c_str()))){fail(400,"invalid_id");return;}
 if(String(d["expectedBootId"]|"")!=bootId){fail(409,"audio_restarted");return;}
 int old=findRequest(id);
 if(old>=0){if(records[old].studentId!=sid||records[old].teacherId!=tid){fail(409,"request_conflict");return;}recordReply(old,200,true);return;}
 if(!ready){fail(503,"audio_not_ready");return;}
 if(pendingNetwork||(!AUDIO_AP_ALWAYS&&setupActive)){fail(503,"network_configuration_active");return;}
 if(qCount+(playing?1:0)>=CAPACITY){fail(429,"queue_full");return;}
 for(int i=0;i<HISTORY;i++)if(records[i].used&&records[i].studentId==sid&&records[i].teacherId==tid&&millis()-records[i].acceptedAt<COOLDOWN_MS){fail(429,"cooldown_30s");return;}
 Person s,t;String error;
 if(!findPerson(SD,"/students.csv","studentId",sid,s,error)){fail(error=="id_not_found"?404:503,error);return;}
 if(!findPerson(SD,"/teachers.csv","teacherId",tid,t,error)){fail(error=="id_not_found"?404:503,error);return;}
 if(!validWav(s.audioFile)||!validWav(t.audioFile)||(USE_CHIME&&!validWav("/chime.wav"))){fail(422,"wav_missing_or_invalid");return;}
 int index=-1;
 for(int i=0;i<HISTORY;i++)if(!records[i].used||(millis()-records[i].acceptedAt>HISTORY_MS&&records[i].state!="queued"&&records[i].state!="playing")){index=i;break;}
 // Do not silently evict a live idempotency record inside the promised ten-minute window.
 if(index<0){fail(429,"history_full_retry_later");return;}
 CallRecord&r=records[index];r=CallRecord{};r.used=true;r.requestId=id;r.studentId=sid;r.teacherId=tid;r.studentFile=s.audioFile;r.teacherFile=t.audioFile;
 r.line1=t.displayName;r.line2=(s.displayName.length()?s.displayName:s.maskedName)+"同學找";r.state="queued";r.displayState="pending";r.acceptedAt=millis();
 r.sourceIp=server.client().remoteIP().toString();
 fifo[(qHead+qCount)%CAPACITY]=index;++qCount;recordReply(index,202);
}

// Intended topology: only Audio sends display content. Shared LAN key is not strong role authentication.
bool sendDisplay(CallRecord&r){
 if(netProfile.matrixHost.isEmpty())return false;
 JsonDocument d;d["systemId"]=SYSTEM_ID;d["requestId"]=r.requestId;d["sourceBootId"]=bootId;d["line1"]=r.line1;d["line2"]=r.line2;String body;serializeJson(d,body);
 for(int attempt=0;attempt<3;attempt++){
  WiFiClient c;HTTPClient h;h.setConnectTimeout(350);h.setTimeout(600);h.begin(c,String("http://")+netProfile.matrixHost+"/display");h.addHeader("Content-Type","application/json");h.addHeader("X-Device-Key",DEVICE_KEY);
  int code=h.POST(body);String response=h.getString();h.end();JsonDocument ack;
  if(code==200&&!deserializeJson(ack,response)&&ack["ok"]==true&&String(ack["requestId"]|"")==r.requestId)return true;
  server.handleClient();delay(30);
 }
 return false;
}

// Real RIFF parser: skips metadata chunks. Mono is duplicated to L/R for PCM5102A.
bool playWav(const String&path){
 File f=SD.open(path,FILE_READ);staffroom::WavInfo w;std::string error;
 if(!f||!staffroom::wavInfo(f,w,error))return false;
 i2s_chan_handle_t tx=nullptr;i2s_chan_config_t channel=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO,I2S_ROLE_MASTER);
 if(i2s_new_channel(&channel,&tx,nullptr)!=ESP_OK)return false;
 i2s_std_config_t config={};config.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(w.sampleRate);config.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
 config.gpio_cfg.mclk=I2S_GPIO_UNUSED;config.gpio_cfg.bclk=(gpio_num_t)PIN_I2S_BCLK;config.gpio_cfg.ws=(gpio_num_t)PIN_I2S_LRCK;config.gpio_cfg.dout=(gpio_num_t)PIN_I2S_DATA;config.gpio_cfg.din=I2S_GPIO_UNUSED;
 bool enabled=false,ok=i2s_channel_init_std_mode(tx,&config)==ESP_OK;
 if(ok){ok=i2s_channel_enable(tx)==ESP_OK;enabled=ok;}
 uint8_t input[1024];int16_t stereo[1024];uint32_t left=w.dataBytes;
 while(ok&&left){
  size_t want=left>sizeof(input)?sizeof(input):left;size_t got=f.read(input,want);if(got!=want){ok=false;break;}left-=got;
  uint8_t*out=input;size_t bytes=got;
  if(w.channels==1){for(size_t i=0;i<got/2;i++){int16_t sample=(int16_t)staffroom::le16(input+i*2);stereo[i*2]=sample;stereo[i*2+1]=sample;}out=(uint8_t*)stereo;bytes=got*2;}
  size_t written=0;ok=i2s_channel_write(tx,out,bytes,&written,1000)==ESP_OK&&written==bytes;
  if(AUDIO_TEST_STAGE==0)server.handleClient();delay(1);
 }
 // Flush real audio through DMA with enough trailing silence, rather than cutting off a last word.
 if(ok){memset(stereo,0,sizeof(stereo));uint32_t silence=w.sampleRate/8;while(silence&&ok){size_t frames=silence>512?512:silence;size_t n=0;ok=i2s_channel_write(tx,stereo,frames*4,&n,1000)==ESP_OK&&n==frames*4;silence-=frames;if(AUDIO_TEST_STAGE==0)server.handleClient();}delay(100);}
 if(enabled)i2s_channel_disable(tx);i2s_del_channel(tx);return ok;
}

bool validAudioPins(){
 const int pins[]={PIN_SD_CS,PIN_SD_SCK,PIN_SD_MISO,PIN_SD_MOSI,PIN_I2S_BCLK,PIN_I2S_LRCK,PIN_I2S_DATA};
 // Conservative N16R8 candidate GPIOs only. A real board pinout still must be checked.
 // Exclude flash/OPI PSRAM 26-37, USB 19/20, UART 43/44, and strapping pins 0/3/45/46.
 for(int i=0;i<7;i++){
  int p=pins[i];bool allowed=p==1||p==2||(p>=4&&p<=18)||p==21||(p>=38&&p<=42)||p==47||p==48;
  if(!allowed)return false;for(int j=0;j<i;j++)if(pins[j]==p)return false;
 }
 return AUDIO_PINS_CONFIRMED;
}

bool startAudioSetup(){
 if(playing||qCount||networkSpeaking||pendingNetwork||String(DEVICE_KEY).length()<32)return false;
 WiFi.mode(WIFI_AP_STA);
 IPAddress ip(192,168,4,1);WiFi.softAPConfig(ip,ip,IPAddress(255,255,255,0));
 setupActive=WiFi.softAP(setupSSID().c_str(),setupPassword().c_str(),1,false,AUDIO_AP_MAX_CLIENTS);
 setupStarted=millis();Serial.printf("AUDIO SETUP %s at 192.168.4.1 (10 min; password NOT logged)\n",setupSSID().c_str());
 return setupActive;
}
void stopAudioSetup(){if(setupActive){WiFi.softAPdisconnect(true);WiFi.mode(WIFI_STA);setupActive=false;}autoSetup=false;}
void handleNetwork(){
 if(!authorized())return;
 // Never allow network credential changes via the normal school LAN.
 if(!setupActive||server.client().localIP()!=WiFi.softAPIP()){fail(403,"setup_ap_required");return;}
 if(playing||qCount||networkSpeaking||pendingNetwork){fail(409,"audio_busy");return;}
 JsonDocument d;if(server.arg("plain").length()>2048||deserializeJson(d,server.arg("plain"))){fail(400,"invalid_json");return;}
 if(String(d["systemId"]|"")!=SYSTEM_ID){fail(403,"wrong_system");return;}
 NetworkProfile p;
 if(!networkFromJson(d,p)){fail(422,"invalid_network_or_ca_missing");return;}
 if(!p.matrixHost.isEmpty()&&!staffroom::unicastIPv4(p.matrixHost.c_str())){fail(422,"invalid_matrix_ip");return;}
 p.audioHost="";if(!saveNetwork(p)){fail(507,"nvs_save_failed");return;}
 nextNetwork=p;pendingNetwork=true;applyAt=millis()+1000;
 JsonDocument ack;ack["ok"]=true;ack["role"]="AUDIO";ack["systemId"]=SYSTEM_ID;
 ack["mac"]=WiFi.macAddress();ack["state"]="saved_reconnecting";reply(200,ack);
}
bool networkVoiceFilesReady(){
 if(!ready)return false;
 if(!validWav("/network/ip.wav")||!validWav("/network/dot.wav"))return false;
 for(int i=0;i<10;i++)if(!validWav(String("/network/")+i+".wav"))return false;
 return true;
}
// Local delta: status cache. The check above opens twelve files (about
// 250 ms of SPI SD time), and the PC console polls a status endpoint every
// few seconds. Serve a cached answer for the status routes only; the
// playback paths still call the live version, so nothing audible changes.
bool networkVoiceReadyCached(){
 static uint32_t checkedAt=0;static bool cached=false;
 if(!checkedAt||(uint32_t)(millis()-checkedAt)>=10000){
  cached=networkVoiceFilesReady();checkedAt=millis();if(!checkedAt)checkedAt=1;
 }
 return cached;
}
bool sayOwnIP(){
 if(!networkVoiceFilesReady()){Serial.println("IP VOICE SKIPPED: copy validated network WAVs to Audio SD; call audio unaffected");return false;}
 String ip=WiFi.localIP().toString();networkSpeaking=true;
 bool ok=playWav("/network/ip.wav");
 for(size_t i=0;ok&&i<ip.length();i++) {
  String path=ip[i]=='.'?"/network/dot.wav":String("/network/")+ip[i]+".wav";
  ok=playWav(path); // Existing RIFF parser and PCM5102A playback, no Internet TTS.
 }
 networkSpeaking=false;if(ok)lastSpokenIp=ip;Serial.println(ok?"IP VOICE DONE (listen to verify)":"IP VOICE PLAYBACK FAILED");return ok;
}
void handleSayIP(){
 if(!authorized())return;
 if(!ready||WiFi.status()!=WL_CONNECTED||!networkVoiceFilesReady()){fail(503,"network_voice_missing_or_not_ready");return;}
 if(playing||qCount||pendingNetwork||networkSpeaking){fail(409,"audio_busy");return;}
 ipPending=true;JsonDocument d;d["ok"]=true;d["systemId"]=SYSTEM_ID;d["state"]="ip_voice_pending";reply(202,d);
}
// Local delta: read-only remote console. The Door has no HTTP server, so the
// Audio reports only what it can observe itself: its own radio state, how many
// stations are attached to its hotspot (i.e. is the Door there), and the last
// calls together with the source IP the Door posted from. Never mutates the
// queue or the idempotency history.
void handleDiag(){
 if(!authorized())return;
 JsonDocument d;
 d["ok"]=true;d["role"]="AUDIO";d["systemId"]=SYSTEM_ID;d["bootId"]=bootId;
 d["ready"]=ready;d["error"]=startupError;d["uptimeMs"]=millis();d["freeHeap"]=ESP.getFreeHeap();
 d["staIp"]=WiFi.localIP().toString();d["staMac"]=WiFi.macAddress();d["staSsid"]=WiFi.SSID();
 d["rssi"]=WiFi.RSSI();d["channel"]=WiFi.channel();
 d["apOn"]=setupActive;d["apIp"]=setupActive?WiFi.softAPIP().toString():"";
 d["apSsid"]=setupActive?setupSSID():"";d["apClients"]=setupActive?WiFi.softAPgetStationNum():0;
 d["queued"]=qCount;d["playing"]=playing;d["networkVoiceReady"]=networkVoiceReadyCached();
 // Local delta: enough for the console to prefill the Wi-Fi form. The stored
 // password is deliberately NOT exposed; the form makes the user retype it.
 d["netSsid"]=netProfile.ssid;d["netMode"]=netProfile.mode;
 d["netMatrix"]=netProfile.matrixHost;d["netValid"]=networkValid(netProfile);
 int picked[16],n=0;
 // records[] is a flat ring; repeat "take the newest not yet taken" a few times
 // instead of allocating a sorted copy on a device with a small heap.
 for(int k=0;k<16;k++){
  int best=-1;
  for(int i=0;i<HISTORY;i++){
   if(!records[i].used)continue;
   bool seen=false;for(int j=0;j<n;j++)if(picked[j]==i){seen=true;break;}
   if(seen)continue;
   if(best<0||(int32_t)(records[i].acceptedAt-records[best].acceptedAt)>0)best=i;
  }
  if(best<0)break;
  picked[n++]=best;
 }
 int used=0,done=0;
 for(int i=0;i<HISTORY;i++)if(records[i].used){used++;if(records[i].state=="done")done++;}
 JsonArray log=d["log"].to<JsonArray>();
 for(int j=0;j<n;j++){
  const CallRecord&r=records[picked[j]];JsonObject o=log.add<JsonObject>();
  o["at"]=r.acceptedAt;o["requestId"]=r.requestId;o["studentId"]=r.studentId;o["teacherId"]=r.teacherId;
  o["line1"]=r.line1;o["line2"]=r.line2;o["state"]=r.state;o["displayState"]=r.displayState;o["sourceIp"]=r.sourceIp;
  if(r.error.length())o["error"]=r.error;
 }
 d["historyUsed"]=used;d["historyDone"]=done;
 reply(200,d);
}
#include "Console.h"   // browser console HTML; see the header for why it is not inline

// Local delta: browser console. See config.h (CONSOLE_PIN) and LOCAL_DELTAS.md.
void handleConsoleUnlock(){
 if(server.arg("plain").length()>256){fail(400,"invalid_json");return;}
 JsonDocument d;
 if(deserializeJson(d,server.arg("plain"))){fail(400,"invalid_json");return;}
 if(millis()<consoleLockUntil){
  JsonDocument o;o["ok"]=false;o["error"]="locked";o["retryMs"]=consoleLockUntil-millis();reply(429,o);return;
 }
 if(String(d["pin"]|"")!=CONSOLE_PIN){
  if(++consoleFails>=CONSOLE_MAX_FAILS){consoleFails=0;consoleLockUntil=millis()+CONSOLE_LOCK_MS;}
  fail(403,"bad_pin");return;
 }
 consoleFails=0;
 char t[33];snprintf(t,sizeof(t),"%08x%08x%08x%08x",esp_random(),esp_random(),esp_random(),esp_random());
 consoleToken=t;consoleTokenAt=millis();
 Serial.println("CONSOLE unlocked from browser");
 JsonDocument o;o["ok"]=true;o["token"]=consoleToken;o["expiresMs"]=CONSOLE_SESSION_MS;reply(200,o);
}
void handleConsolePage(){server.sendHeader("Cache-Control","no-store");server.send_P(200,"text/html; charset=utf-8",CONSOLE_HTML);}

void networkTick(){
 while(Serial.available()){
  char c=Serial.read();if(c=='\r')continue;
  if(c=='\n'){
   if(serialCommand=="SETUP") {if(!startAudioSetup())Serial.println("SETUP denied: busy/key missing");}
   else if(serialCommand=="SAYIP")ipPending=true;
   else if(serialCommand=="CLEARWIFI"&&!playing&&!qCount&&!networkSpeaking&&!pendingNetwork){eraseNetwork();netProfile=networkDefaults(AUDIO_HOST,MATRIX_HOST);joinNetwork(netProfile);startAudioSetup();}
   else if(serialCommand=="STATUS")Serial.printf("AUDIO IP=%s MAC=%s SETUP=%d READY=%d\n",WiFi.localIP().toString().c_str(),WiFi.macAddress().c_str(),setupActive,ready);
   serialCommand="";
  }else if(serialCommand.length()<20)serialCommand+=c;
 }
 if(pendingNetwork&&!playing&&!networkSpeaking&&(int32_t)(millis()-applyAt)>=0){
  netProfile=nextNetwork;pendingNetwork=false;awaitingStaAfterApply=true;joinNetwork(netProfile);lastConnectedIp="";
 }
 if(!AUDIO_AP_ALWAYS&&setupActive&&millis()-setupStarted>=NETWORK_SETUP_WINDOW_MS)stopAudioSetup();
 if(WiFi.status()==WL_CONNECTED){
  if(awaitingStaAfterApply&&WiFi.SSID()==netProfile.ssid){if(!AUDIO_AP_ALWAYS)stopAudioSetup();awaitingStaAfterApply=false;}
  String ip=WiFi.localIP().toString();
  if(ip!=lastConnectedIp){
   lastConnectedIp=ip;Serial.printf("AUDIO IP=%s MAC=%s\n",ip.c_str(),WiFi.macAddress().c_str());
   if(mdnsRunning)MDNS.end();String name=String("sr-audio-")+SYSTEM_ID;name.toLowerCase();
   mdnsRunning=MDNS.begin(name.c_str());if(mdnsRunning){MDNS.addService("sr-audio","tcp",80);MDNS.addServiceTxt("sr-audio","tcp","system",SYSTEM_ID);}
   if(ip!=lastSpokenIp)ipPending=true;
  }
 }else {lastConnectedIp="";if(mdnsRunning){MDNS.end();mdnsRunning=false;}}
 // Local delta: computer-free recovery. Never opens while connected, while a call
 // is in flight, or while a pushed profile is being applied.
 if(WiFi.status()==WL_CONNECTED){downSince=0;autoSetup=false;}
 else {
  if(!downSince)downSince=millis();
  if(AUTO_SETUP_DELAY_MS&&!autoSetup&&!setupActive&&!pendingNetwork&&!awaitingStaAfterApply&&(millis()-downSince)>=AUTO_SETUP_DELAY_MS){
   autoSetup=true;
   Serial.println(startAudioSetup()?"AUDIO AUTO SETUP: saved network unreachable; hotspot open for 10 min"
                                  :"AUDIO AUTO SETUP denied: busy/key missing");
  }
 }
}
void setup(){
 Serial.begin(115200);delay(2000);bootId=String(esp_random(),HEX)+String(esp_random(),HEX);
 if(!validAudioPins()){startupError="pins_not_confirmed";}
 else {
  SPI.begin(PIN_SD_SCK,PIN_SD_MISO,PIN_SD_MOSI,PIN_SD_CS);
  if(!SD.begin(PIN_SD_CS,SPI,4000000))startupError="sd_mount_failed";
  else {Person teachers[15];size_t n=0;String e;ready=SD.exists("/students.csv")&&loadTeachers(SD,teachers,n,e);if(!ready)startupError="roster_missing_or_invalid";}
 }
 // Explicit, offline bench modes; no queue, no HTTP server and no network.
 if(AUDIO_TEST_STAGE!=0){
  WiFi.mode(WIFI_OFF);
  Serial.printf("AUDIO TEST stage=%u ready=%d reason=%s\n",AUDIO_TEST_STAGE,ready,startupError.c_str());
  bool ok=ready&&(AUDIO_TEST_STAGE==1||AUDIO_TEST_STAGE==2);
  const char*paths[]={"/teachers/01.wav","/students/TEST001.wav","/chime.wav"};
  for(int i=0;ok&&i<(USE_CHIME?3:2);i++){
   bool valid=validWav(paths[i]);Serial.printf("WAV %s %s\n",paths[i],valid?"PASS":"FAIL");ok=valid;
  }
  if(ok&&AUDIO_TEST_STAGE==2){
   Serial.println("PLAY ONCE: chime(optional), teacher 01, then student TEST001. Keep speaker volume LOW.");delay(3000);
   ok=(!USE_CHIME||playWav("/chime.wav"))&&playWav(paths[0])&&playWav(paths[1]);
  }
  Serial.println(ok?"AUDIO TEST PASS (listen separately in stage 2)":"AUDIO TEST FAIL");
  return;
 }
 const char*headers[]={"X-Device-Key"};server.collectHeaders(headers,1);
 server.on("/health",HTTP_GET,[](){if(!authorized())return;JsonDocument d;d["ok"]=true;d["role"]="AUDIO";d["ready"]=ready;d["systemId"]=SYSTEM_ID;d["bootId"]=bootId;d["queued"]=qCount;d["playing"]=playing;d["error"]=startupError;d["ip"]=WiFi.localIP().toString();d["mac"]=WiFi.macAddress();d["setup"]=setupActive;d["networkVoiceReady"]=networkVoiceReadyCached();reply(200,d);});
 server.on("/announce",HTTP_POST,handleAnnounce);
 server.on("/network",HTTP_POST,handleNetwork);
 server.on("/say-ip",HTTP_POST,handleSayIP);
 server.on("/status",HTTP_GET,[](){if(!authorized())return;int i=findRequest(server.arg("requestId"));if(i<0){fail(404,"request_unknown_or_expired");return;}recordReply(i,200);});
 server.on("/diag",HTTP_GET,handleDiag);
 server.on("/",HTTP_GET,handleConsolePage);
 server.on("/console-unlock",HTTP_POST,handleConsoleUnlock);
 server.onNotFound([](){fail(404,"not_found");});// WiFi.mode() MUST precede server.begin(): it lazily runs esp_netif_init()
 // (NetworkManager::begin), which creates the lwIP core lock WebServer needs.
 // Reversed order panics: assert failed: xQueueSemaphoreTake queue.c:1709.
 WiFi.mode(WIFI_STA);server.begin();
 netProfile=loadNetwork(AUDIO_HOST,MATRIX_HOST);WiFi.setAutoReconnect(true);joinNetwork(netProfile);
 if(AUDIO_AP_ALWAYS||!networkValid(netProfile))startAudioSetup();
 Serial.printf("AUDIO ready=%d reason=%s\n",ready,startupError.c_str());
}
void loop(){
 if(AUDIO_TEST_STAGE!=0){delay(100);return;}
 networkTick();
 server.handleClient();
 if(WiFi.status()!=WL_CONNECTED&&millis()-lastWifiTry>10000){lastWifiTry=millis();WiFi.reconnect();}
 if(ipPending&&!playing&&!qCount&&!pendingNetwork&&WiFi.status()==WL_CONNECTED){ipPending=false;sayOwnIP();}
 if(!playing&&qCount){int i=fifo[qHead];qHead=(qHead+1)%CAPACITY;--qCount;playing=true;CallRecord&r=records[i];r.state="playing";r.displayState=sendDisplay(r)?"ok":"failed";
  // School request: call the teacher first, then the student looking for them.
  bool ok=(!USE_CHIME||playWav("/chime.wav"))&&playWav(r.teacherFile)&&playWav(r.studentFile);
  r.state=ok?"done":"failed";if(!ok)r.error="playback_failed";playing=false;
 }
 delay(2);
}

/*
 * 教员室门口交互端 — Arduino / ESP32-S3-Touch-LCD-7B (1024 x 600)
 * 只负责触控、查找遮蔽名册、向 Audio 发送召唤。不在门口播放录音。
 * 阅读同包 README.md 后再接入学校网络。此工程不是网页，也不是 Uno 程序。
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD_MMC.h>
#include <esp_system.h>
#include "rgb_lcd_port.h"
#include "gt911.h"
#include "config.h"
#include "NetworkProfile.h"
#include <ESPmDNS.h>
#include "ui_config.h"
#include "Roster.h"
#include "DeliveryPolicy.h"
#include "UiView.h"

UiView view;
uint16_t *canvas = nullptr;
Person student, teachers[15];
size_t teacherCount = 0;
int selected = -1;
UiPage page = UiPage::Input;
bool ready = false, dirty = true, hadTouch = false;
bool frozen = false, stopRetry = false, displayWarning = false;
String studentId, requestId, bootSeen, notice, blockingTitle, blockingHelp;
String completeState;
uint32_t lastActivity = 0, successTime = 0, lastWifiTry = 0, requestStarted = 0;
// Special welcome-banner feature. This is the only path that talks to the Matrix
// directly; announcing a teacher still goes only to the Audio board.
String passcode, bannerText;
uint32_t welcomeStart = 0, lastWelcomeDraw = 0;
int passcodeFails = 0;
int welcomePenX();
NetworkProfile netProfile, netDraft;
String netPin, netEditValue, netStatus, pairSsid, pairPassword, lockedAudioHost;
bool netCredentials=false, pairing=false, mdnsReady=false;
String doorSerialCommand;
int netField=-1, netKeyboard=0, netFails=0;
uint32_t netLockedUntil=0;


void resetSession() {
    studentId = ""; requestId = ""; bootSeen = ""; notice = "";
    student = Person{}; selected = -1; page = UiPage::Input;
    frozen = false; stopRetry = false; displayWarning = false; completeState = "";
    passcode = ""; bannerText = ""; passcodeFails = 0;
    if(pairing&&!UI_DEMO_MODE){pairing=false;joinNetwork(netProfile);}
    netPin="";netEditValue="";netDraft=NetworkProfile{};pairPassword="";
    netCredentials=false;lockedAudioHost="";
    lastActivity = millis(); dirty = true;
}

void drawScreen() {
    if (!canvas) return;
    ViewState s;
    s.page = page; s.demo = UI_DEMO_MODE; s.ready = ready;
    s.wifi = !UI_DEMO_MODE && WiFi.status() == WL_CONNECTED;
    s.frozen = frozen;
    s.canRetry = !stopRetry && door::retryWindowOpen(millis(), requestStarted);
    s.displayWarning = displayWarning;
    s.blockingTitle = blockingTitle; s.blockingHelp = blockingHelp; s.notice = notice;
    s.studentId = studentId; s.studentClass = student.className;
    s.maskedName = student.maskedName; s.completeState = completeState;
    if (selected >= 0 && size_t(selected) < teacherCount)
        s.selectedTeacher = teachers[selected].displayName;
    s.teacherCount = teacherCount;
    for (size_t i = 0; i < teacherCount; ++i) s.teacherNames[i] = teachers[i].displayName;
    s.passcode = passcode; s.bannerX = welcomePenX();
    // The passcode page previews the phrase, so fall back to the configured one.
    s.bannerText = bannerText.length() ? bannerText : String(BANNER_TEXT);
    s.ownIp=UI_DEMO_MODE?"DEMO":WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"未連線";
    s.ownMac=UI_DEMO_MODE?"DEMO":WiFi.macAddress();
    s.networkSsid=pairing?pairSsid:netProfile.ssid;
    s.apSsid=setupSSID();
    s.audioHost=netDraft.audioHost;s.matrixHost=netDraft.matrixHost;s.netStatus=netStatus;
    s.netPin=netPin;s.netEditValue=netEditValue;s.netKeyboard=netKeyboard;
    s.netCredentials=netCredentials;s.netPairing=pairing;
    s.netModeName=netDraft.mode==0?"普通 / 開放":netDraft.mode==1?"PEAP 帳號":"TTLS 帳號";
    s.netFields[0]=netDraft.ssid;s.netFields[1]=netDraft.password.isEmpty()?"(空白)":"********";
    s.netFields[2]=netDraft.username;s.netFields[3]=netDraft.identity;
    const char*captions[]={"Wi-Fi 名稱","Wi-Fi 密碼","登入帳號","外層身份","Audio IPv4","Matrix IPv4","Audio 熱點名稱","Audio 熱點密碼"};
    if(netField>=0&&netField<8)s.netEditCaption=captions[netField];
    s.netSecret=netField==1||netField==7;
    view.render(s);
    wavesahre_rgb_lcd_display(reinterpret_cast<uint8_t *>(canvas));
    dirty = false;
}

void showWorking(const String &message) {
    notice = message;
    drawScreen(); // Show progress before synchronous HTTP; don't hide a blocked request.
    delay(20);
}

bool configurationReady() {
    bool ok=String(DEVICE_KEY).length()>=32&&String(DEVICE_KEY)!="CHANGE_ME"&&doorIdentifier(SYSTEM_ID);
    if(!ok){blockingTitle="設備密鑰或套別未設定";blockingHelp="先用更新工具保留本套設定，再燒錄";}
    return ok;
}

void loadDemoRoster() {
    // Deliberately fictitious data embedded in the firmware. Never read the real TF card.
    teacherCount = 3;
    teachers[0].id = "01"; teachers[0].displayName = "李老師";
    teachers[1].id = "02"; teachers[1].displayName = "王老師";
    teachers[2].id = "03"; teachers[2].displayName = "陳老師";
}

bool lookupStudent() {
    if (!staffroom::studentId5(studentId.c_str())) {
        notice = "請輸入剛好 5 位數字學號"; return false;
    }
    if (UI_DEMO_MODE) {
        student = Person{};
        student.id = studentId;
        if (studentId == "00001") { student.className = "2A"; student.maskedName = "陳大文"; }
        else if (studentId == "99999") { student.className = "TEST"; student.maskedName = "測試學生"; }
        else if (studentId == "00002") { student.className = "2B"; student.maskedName = "林小明"; }
        else { notice = "演示學號：00001、99999、00002"; return false; }
        return true;
    }
    String error;
    if (findStudent(SD_MMC, studentId, student, error)) return true;
    if (error == "id_not_found") notice = "找不到編號，請檢查輸入";
    else if (error == "duplicate_student") notice = "此學號重複，請聯絡老師修正名冊";
    else notice = "名冊讀取失敗，請聯絡老師";
    return false;
}

int httpRequest(const String &path, bool post, JsonDocument &payload, JsonDocument &response,
                const char *host = nullptr) {
    // Defense in depth: even an accidental submit cannot emit network traffic in demo mode.
    if (UI_DEMO_MODE || WiFi.status() != WL_CONNECTED) return -1;
    String target=host?String(host):!lockedAudioHost.isEmpty()?lockedAudioHost:netProfile.audioHost;
    if(!staffroom::unicastIPv4(target.c_str()))return -1;
    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(2000);
    http.setTimeout(3000);
    if (!http.begin(client, String("http://") + target + path)) return -1;
    http.addHeader("X-Device-Key", DEVICE_KEY);
    int code;
    if (post) {
        http.addHeader("Content-Type", "application/json");
        String body;
        serializeJson(payload, body);
        code = http.POST(body);
    } else code = http.GET();
    // Our Audio responses are small. Refuse an obviously wrong HTTP endpoint.
    if (http.getSize() > 4096) { http.end(); return -2; }
    if (code > 0) {
        String body = http.getString();
        http.end();
        if (body.length() > 4096 || deserializeJson(response, body)) return -2;
    } else http.end();
    return code;
}

bool checkAudio(bool first) {
    JsonDocument unused, reply;
    int code = httpRequest("/health", false, unused, reply);
    if (code != 200 || reply["role"] != "AUDIO" ||
        String(reply["systemId"] | "") != SYSTEM_ID) {
        notice = "語音端離線或設定不符，請聯絡老師"; return false;
    }
    String boot = reply["bootId"] | "";
    if (boot.isEmpty()) { notice = "語音端回覆無效，請聯絡老師"; return false; }
    if (!first && boot != bootSeen) {
        notice = "語音端已重啟，請找老師確認";
        frozen = true; stopRetry = true; return false;
    }
    if (reply["ready"] != true) { notice = "語音端未就緒，請聯絡老師"; return false; }
    if (first){bootSeen = boot;lockedAudioHost=netProfile.audioHost;}
    return true;
}

String refusalMessage(const String &error) {
    if (error == "queue_full") return "隊列已滿，請稍候再試";
    if (error == "cooldown_30s") return "請等待 30 秒後再召喚";
    if (error == "wav_missing_or_invalid") return "錄音缺失或格式錯誤，請聯絡老師";
    if (error == "unauthorized" || error == "wrong_system") return "設備設定不一致，請聯絡老師";
    if (error == "id_not_found") return "室內名冊沒有此編號，請聯絡老師";
    if (error == "history_full_retry_later") return "系統紀錄已滿，請稍候";
    return "請聯絡老師檢查設備或名冊";
}

void showAccepted(JsonDocument &reply) {
    page = UiPage::Complete;
    completeState = String(reply["state"] | "");
    displayWarning = reply["displayState"] == "failed";
    notice = ""; frozen = false; successTime = millis();
    // This is an Audio acknowledgement, NOT a teacher response or proof of sound/display.
}

void submitCall() {
    if (selected < 0 || size_t(selected) >= teacherCount) return;
    if (UI_DEMO_MODE) {
        page = UiPage::Complete; completeState = "demo";
        notice = ""; successTime = millis(); dirty = true;
        return; // no HTTP and no fake "queued" acknowledgement
    }
    const bool first = requestId.isEmpty();
    if (stopRetry || (!first && !door::retryWindowOpen(millis(), requestStarted))) {
        stopRetry = true; frozen = true;
        notice = "不可再自動重試，請找老師確認"; dirty = true; return;
    }
    showWorking("正在確認室內設備");
    if (!checkAudio(first)) { dirty = true; return; }
    if (first) {
        char randomPart[17];
        snprintf(randomPart, sizeof(randomPart), "%08lx%08lx",
                 (unsigned long)esp_random(), (unsigned long)esp_random());
        requestId = String(SYSTEM_ID) + "-" + randomPart;
        requestStarted = millis();
    }
    // checkAudio may itself block; check the retry boundary again before POST.
    if (!door::retryWindowOpen(millis(), requestStarted)) {
        stopRetry = true; frozen = true;
        notice = "已超過安全重試時限，請找老師確認"; dirty = true; return;
    }
    showWorking("正在送出，請稍候");
    JsonDocument payload, reply;
    payload["systemId"] = SYSTEM_ID;
    payload["requestId"] = requestId;
    payload["studentId"] = studentId;
    payload["teacherId"] = teachers[selected].id;
    payload["expectedBootId"] = bootSeen;
    int code = httpRequest("/announce", true, payload, reply);
    bool correlated = reply["ok"] == true &&
                      String(reply["requestId"] | "") == requestId &&
                      String(reply["bootId"] | "") == bootSeen;
    String state = reply["state"] | "", error = reply["error"] | "";
    door::Outcome result = door::classifyReply(code, correlated, state.c_str(), error.c_str());
    if (result == door::Outcome::Accepted) showAccepted(reply);
    else if (result == door::Outcome::PlaybackFailed) {
        frozen = true; stopRetry = true; notice = "播放失敗，請找老師處理";
    } else if (result == door::Outcome::Rejected) {
        notice = refusalMessage(error);
        if (!frozen) { requestId = ""; bootSeen = ""; lockedAudioHost = ""; }
        // An earlier uncertain attempt may already have been accepted: a later
        // refusal cannot undo that uncertainty, so keep the original request ID.
    } else {
        frozen = true;
        if (error == "audio_restarted") stopRetry = true;
        notice = "尚未確認，請查詢原請求或找老師";
    }
    dirty = true;
}

void queryCall() {
    if (UI_DEMO_MODE || requestId.isEmpty()) return;
    showWorking("正在查詢原請求");
    JsonDocument unused, reply;
    int code = httpRequest("/status?requestId=" + requestId, false, unused, reply);
    bool correlated = reply["ok"] == true &&
                      String(reply["requestId"] | "") == requestId &&
                      String(reply["bootId"] | "") == bootSeen;
    String state = reply["state"] | "";
    auto result = door::classifyReply(code, correlated, state.c_str(), "");
    if (result == door::Outcome::Accepted) showAccepted(reply);
    else if (result == door::Outcome::PlaybackFailed) {
        stopRetry = true; notice = "播放失敗，請找老師處理";
    } else notice = "尚未確認，請勿重新叫號，請找老師";
    dirty = true;
}

// --- Special feature: welcome banner (Door -> Matrix, never through Audio) ---
int welcomePenX() {
    if (page != UiPage::Welcome) return UiView::WIDTH;
    int parked = (UiView::WIDTH - textWidth(bannerText, 4)) / 2;
    uint32_t elapsed = millis() - welcomeStart;
    if (elapsed >= WELCOME_SLIDE_MS) return parked;
    float t = float(elapsed) / float(WELCOME_SLIDE_MS);
    float eased = 1.0f - (1.0f - t) * (1.0f - t);       // ease-out
    return int(float(UiView::WIDTH) + float(parked - UiView::WIDTH) * eased);
}

void pushBannerToMatrix() {
    IPAddress target;
    if (!target.fromString(netProfile.matrixHost)) { notice = "未設定走廊點陣屏 IP"; return; }
    char randomPart[9];
    snprintf(randomPart, sizeof(randomPart), "%08lx", (unsigned long)esp_random());
    JsonDocument payload, reply;
    payload["systemId"] = SYSTEM_ID;
    payload["requestId"] = String(SYSTEM_ID) + "-banner-" + randomPart;
    payload["line1"] = BANNER_TEXT;
    payload["line2"] = "";   // single-line banner: Matrix draws line1 only
    int code = httpRequest("/display", true, payload, reply, netProfile.matrixHost.c_str());
    if (code == 200 && reply["ok"] == true) return;
    if (code == 503) notice = "點陣屏未啟用（PANEL_CONFIRMED 仍是 false）";
    else if (code == 403) notice = "點陣屏拒絕：密鑰或 SYSTEM_ID 不符";
    else notice = "點陣屏無回應，請檢查 IP 與電源";
}

void submitPasscode() {
    if (String(BANNER_PASSWORD) == "CHANGE_ME" || passcode != BANNER_PASSWORD) {
        passcode = "";
        if (++passcodeFails >= 3) {
            passcodeFails = 0; page = UiPage::Input; notice = "密碼連續錯誤，已返回首頁";
        } else notice = "密碼錯誤";
        dirty = true;
        return;
    }
    passcode = ""; passcodeFails = 0; notice = "";
    bannerText = BANNER_TEXT;
    welcomeStart = millis(); lastWelcomeDraw = 0;
    page = UiPage::Welcome;
    if (UI_DEMO_MODE) notice = "演示模式：沒有送到走廊點陣屏";
    else pushBannerToMatrix();
    dirty = true;
}


bool networkPage(){return page==UiPage::Network||page==UiPage::NetworkEdit||page==UiPage::NetUnlock;}
String getNetField(int field){
 switch(field){case 0:return netDraft.ssid;case 1:return netDraft.password;case 2:return netDraft.username;
 case 3:return netDraft.identity;case 4:return netDraft.audioHost;case 5:return netDraft.matrixHost;case 6:return pairSsid;case 7:return pairPassword;}
 return "";
}
void setNetField(int field,const String&value){
 switch(field){case 0:netDraft.ssid=value;break;case 1:netDraft.password=value;break;case 2:netDraft.username=value;break;
 case 3:netDraft.identity=value;break;case 4:netDraft.audioHost=value;break;case 5:netDraft.matrixHost=value;break;
 case 6:pairSsid=value;break;case 7:pairPassword=value;break;}
}
bool saveDoorDraft(bool connect){
 if(UI_DEMO_MODE){notice="演示模式：沒有保存或連網";return false;}
 if(!networkValid(netDraft)){notice="Wi-Fi 資料無效；企業網需帳號及 CA";return false;}
 if(!netDraft.audioHost.isEmpty()&&!staffroom::unicastIPv4(netDraft.audioHost.c_str())){notice="Audio IP 格式不正確";return false;}
 if(!netDraft.matrixHost.isEmpty()&&!staffroom::unicastIPv4(netDraft.matrixHost.c_str())){notice="Matrix IP 格式不正確";return false;}
 if(!saveNetwork(netDraft)){notice="保存失敗，未更改連線";return false;}
 netProfile=netDraft;
 if(connect){pairing=false;if(mdnsReady){MDNS.end();mdnsReady=false;}joinNetwork(netProfile);netStatus="等待取得 IP；失敗可再次修改";}
 notice="設定已保存；關機再開仍保留";return true;
}
bool verifyAudioAt(const String&host, bool requireReady, JsonDocument&reply){
 if(!staffroom::unicastIPv4(host.c_str()))return false;
 JsonDocument unused;
 int code=httpRequest("/health",false,unused,reply,host.c_str());
 return code==200&&reply["ok"]==true&&reply["role"]=="AUDIO"&&String(reply["systemId"]|"")==SYSTEM_ID&&
        String(reply["bootId"]|"").length()>0&&(!requireReady||reply["ready"]==true);
}
void discoverAudio(){
 if(UI_DEMO_MODE){notice="演示模式：沒有尋找設備";return;}
 if(WiFi.status()!=WL_CONNECTED||pairing){notice="先連正式 Wi-Fi；不能在配置熱點尋找";return;}
 showWorking("正在尋找並核對 Audio");
 if(!mdnsReady){String name=String("sr-door-")+SYSTEM_ID;name.toLowerCase();mdnsReady=MDNS.begin(name.c_str());}
 if(!mdnsReady){notice="尋找服務失敗；可手動輸入 Audio IP";return;}
 int n=MDNS.queryService("sr-audio","tcp");String found;
 for(int i=0;i<n&&i<8;i++){
  JsonDocument reply;String ip=MDNS.address(i).toString();
  if(verifyAudioAt(ip,false,reply)){
   if(found.length()&&found!=ip){notice="找到多塊同套 Audio，請手動指定 IP";return;}
   found=ip;
  }
 }
 if(found.isEmpty()){notice="找不到 Audio；請聽 IP 並手動填入";return;}
 netDraft.audioHost=found;
 if(saveDoorDraft(false)){netStatus="已核對 Audio 身份並保存 IP";notice=netStatus;}
}
void activateNetwork(const Hit&hit){
 lastActivity=millis();notice="";
 if(hit.action==UiAction::OpenNetwork){
  if(frozen||!studentId.isEmpty()||!requestId.isEmpty()||page!=UiPage::Input)return;
  if(!UI_DEMO_MODE&&!adminPinReady()){notice="先用工具設定 6 至 12 位 PIN";dirty=true;return;}
  netPin="";netKeyboard=1;page=UiPage::NetUnlock;dirty=true;return;
 }
 if(hit.action==UiAction::NetCancel){resetSession();return;}
 if(page==UiPage::NetUnlock){
  switch(hit.action){
   case UiAction::NetKey:case UiAction::NetEditKey:if(netPin.length()<12)netPin+=hit.value;break;
   case UiAction::NetEditShift:netKeyboard=(netKeyboard+1)%3;break;
   case UiAction::NetClear:netPin="";break;
   case UiAction::NetBackspace:if(netPin.length())netPin.remove(netPin.length()-1);break;
   case UiAction::NetUnlockSubmit:
    if(netLockedUntil&&(int32_t)(millis()-netLockedUntil)<0){notice="錯誤過多，請等待 60 秒";break;}
    if(!netPin.equalsIgnoreCase(UI_DEMO_MODE?"123456":NETWORK_ADMIN_PIN)){
     netPin="";notice="管理 PIN 錯誤";if(++netFails>=5){netLockedUntil=millis()+60000;netFails=0;}break;
    }
    netPin="";netFails=0;netLockedUntil=0;netDraft=netProfile;netCredentials=false;
    pairSsid=setupSSID();pairPassword=setupPassword();netStatus="輸入 Audio IP 或自動尋找";page=UiPage::Network;break;
   default:break;
  }
  dirty=true;return;
 }
 if(page==UiPage::NetworkEdit){
  switch(hit.action){
   case UiAction::NetEditKey:if(netEditValue.length()<128)netEditValue+=hit.value;else notice="此欄位最多 128 bytes";break;
   case UiAction::NetBackspace:if(netEditValue.length())netEditValue.remove(netEditValue.length()-1);break;
   case UiAction::NetClear:netEditValue="";break;
   case UiAction::NetEditShift:netKeyboard=(netKeyboard+1)%3;break;
   case UiAction::NetEditSave:
    if((netField==4||netField==5)&&!netEditValue.isEmpty()&&!staffroom::unicastIPv4(netEditValue.c_str())){notice="請填完整 IPv4，例如 192.168.1.21";break;}
    if((netField==0||netField==6)&&netEditValue.length()>32){notice="Wi-Fi 名稱最多 32 bytes";break;}
    setNetField(netField,netEditValue);netEditValue="";page=UiPage::Network;break;
   case UiAction::NetMenu:netEditValue="";page=UiPage::Network;break;
   default:break;
  }
  dirty=true;return;
 }
 if(page!=UiPage::Network)return;
 switch(hit.action){
  case UiAction::NetField:netField=hit.value.toInt();netEditValue=getNetField(netField);netKeyboard=0;page=UiPage::NetworkEdit;break;
  case UiAction::NetMenu:
   if(hit.value=="mode")netDraft.mode=(netDraft.mode+1)%3;
   else if(hit.value=="credentials")netCredentials=true;
   else if(hit.value=="status")netCredentials=false;
   else if(hit.value=="save"){if(pairing)notice="先離開配置熱點";else saveDoorDraft(false);}
   break;
  case UiAction::NetConnect:saveDoorDraft(true);break;
  case UiAction::NetTest:{
   if(UI_DEMO_MODE){notice="演示模式：沒有測試設備";break;}
   if(pairing){notice="先離開配置熱點再測試正式 IP";break;}
   showWorking("測試只查詢狀態，不叫號");JsonDocument reply;
   if(verifyAudioAt(netDraft.audioHost,false,reply))notice=reply["ready"]==true?"Audio 身份及就緒狀態通過；仍需試叫號":"連線成功，但 Audio 硬件或名冊未就緒";
   else notice="Audio 無回應：檢查 IP、密鑰、隔離及電源";
   break;
  }
  case UiAction::NetDiscover:discoverAudio();break;
  case UiAction::NetPairJoin:{
   if(UI_DEMO_MODE){notice="演示模式：沒有連接熱點";break;}
   if(!networkValid(netDraft)){notice="先填正式網絡資料及 CA 設定";break;}
   if(pairSsid.isEmpty()||pairPassword.length()<8||pairPassword.length()>63){notice="核對 Audio 熱點及其密碼";break;}
   NetworkProfile ap;ap.ssid=pairSsid;ap.password=pairPassword;pairing=true;if(mdnsReady){MDNS.end();mdnsReady=false;}joinNetwork(ap);
   netStatus="等待 Audio 熱點取得 192.168.4.x";notice=netStatus;break;
  }
  case UiAction::NetJoinAudioAp:{
   if(UI_DEMO_MODE){notice="演示模式：沒有連接熱點";break;}
   String apSsid=setupSSID(),apPass=setupPassword();
   // While pairing the same button promotes the temporary pairing state into the
   // normal callable profile, which is a useful thing to be able to do.
   if(!pairing&&netProfile.ssid==apSsid){notice="門口已在 Audio 熱點；要回正式 Wi-Fi 請按「Wi-Fi / 同步配置」";break;}
   if(apPass.length()<8||apPass.length()>63){notice="設備密鑰未設定，無法加入熱點";break;}
   // One press: the hotspot becomes the ordinary, callable network. Unlike
   // NetPairJoin this is not a temporary state, and the Audio answers at the
   // hotspot gateway address.
   netDraft=netProfile;netDraft.ssid=apSsid;netDraft.password=apPass;
   netDraft.username="";netDraft.identity="";netDraft.mode=0;netDraft.audioHost="192.168.4.1";
   if(saveDoorDraft(true)){netCredentials=false;netStatus="正在加入 Audio 熱點；取得 192.168.4.x 後即可叫號";notice=netStatus;}
   break;
  }
  case UiAction::NetPairSend:{
   if(UI_DEMO_MODE){notice="演示模式：沒有同步設定";break;}
   if(!pairing||WiFi.status()!=WL_CONNECTED||WiFi.localIP()[0]!=192||WiFi.localIP()[1]!=168||WiFi.localIP()[2]!=4){notice="先連 Audio 的配置熱點";break;}
   if(!networkValid(netDraft)){notice="正式網絡欄位無效或 CA 未設定";break;}
   if(!netDraft.matrixHost.isEmpty()&&!staffroom::unicastIPv4(netDraft.matrixHost.c_str())){notice="Matrix IP 無效";break;}
   showWorking("正在核對 Audio 並同步；請勿斷電");JsonDocument health;
   if(!verifyAudioAt("192.168.4.1",false,health)||health["setup"]!=true){notice="不是本套配置中的 Audio；沒有送出密碼";break;}
   JsonDocument payload,reply;networkToJson(netDraft,payload);payload["systemId"]=SYSTEM_ID;
   int code=httpRequest("/network",true,payload,reply,"192.168.4.1");
   if(code!=200||reply["ok"]!=true||reply["state"]!="saved_reconnecting"||String(reply["systemId"]|"")!=SYSTEM_ID){notice="未確認同步結果；保留此頁再查狀態，不要叫號";break;}
   // Old DHCP Audio address is no longer reliable after changing networks.
   netDraft.audioHost="";
   if(saveDoorDraft(true)){netCredentials=false;notice="兩板重連正式網絡；請按自動尋找或輸入新 IP";}
   else notice="Audio 已保存，但門口未保存；請重新設定門口";
   break;
  }
  case UiAction::NetSpeak:{
   if(UI_DEMO_MODE){notice="演示模式：沒有播報";break;}
   if(pairing){notice="請先離開配置熱點";break;}
   JsonDocument health;if(!verifyAudioAt(netDraft.audioHost,true,health)){notice="請先測試 Audio 並核對 IP";break;}
   JsonDocument payload,reply;int code=httpRequest("/say-ip",true,payload,reply,netDraft.audioHost.c_str());
   notice=code==202&&reply["ok"]==true?"Audio 已接收 IP 播報；請聽喇叭":"無法播報；補 network WAV 或稍候再試";break;
  }
  default:break;
 }
 dirty=true;
}

void activate(const Hit &hit) {
    if(hit.action==UiAction::OpenNetwork||networkPage()){activateNetwork(hit);return;}
    if (!ready) return;
    // The frozen page must never expose a new student/teacher/request operation.
    if (frozen && hit.action != UiAction::Submit && hit.action != UiAction::Query) return;
    lastActivity = millis(); notice = "";
    switch (hit.action) {
    case UiAction::Key:
        if (hit.value.length()==1&&hit.value[0]>='0'&&hit.value[0]<='9'&&studentId.length()<5)studentId+=hit.value;
        else notice="學號只接受 5 位數字";
        break;
    case UiAction::Clear: studentId = ""; break;
    case UiAction::Backspace:
        if (studentId.length()) studentId.remove(studentId.length() - 1);
        break;
    case UiAction::Next:
        showWorking("正在查找學號");
        if (lookupStudent()) { page = UiPage::Teachers; notice = ""; }
        break;
    case UiAction::SelectTeacher:
        selected = hit.value.toInt(); page = UiPage::Confirm; requestId = "";
        break;
    case UiAction::Back:
        if (page == UiPage::Teachers) resetSession();
        else { page = UiPage::Teachers; selected = -1; requestId = ""; }
        break;
    case UiAction::Submit: submitCall(); break;
    case UiAction::Query: queryCall(); break;
    case UiAction::NextStudent: resetSession(); break;
    case UiAction::OpenPasscode:
        passcode = ""; passcodeFails = 0; page = UiPage::Passcode; break;
    case UiAction::PasscodeKey:
        if (passcode.length() < 12) passcode += hit.value;
        else notice = "密碼最多 12 位";
        break;
    case UiAction::PasscodeClear: passcode = ""; break;
    case UiAction::PasscodeBack:
        if (passcode.length()) passcode.remove(passcode.length() - 1);
        break;
    case UiAction::PasscodeCancel:
        passcode = ""; page = UiPage::Input; notice = ""; break;
    case UiAction::PasscodeSubmit: submitPasscode(); break;
    case UiAction::Dismiss: passcode = ""; resetSession(); break;
    }
    dirty = true;
}

void setup() {
    Serial.begin(115200);
    // These are the manufacturer's 7B-specific RGB, GT911 and I/O expander drivers.
    touch_gt911_init();
    waveshare_esp32_s3_rgb_lcd_init();
    wavesahre_rgb_lcd_bl_on();
    canvas = static_cast<uint16_t *>(heap_caps_malloc(UiView::WIDTH * UiView::HEIGHT * 2,
                                                     MALLOC_CAP_SPIRAM));
    if (!canvas) {
        Serial.println("DOOR framebuffer allocation failed: check OPI PSRAM / board model");
        return;
    }
    view.begin(canvas);
    if (UI_DEMO_MODE) {
        WiFi.mode(WIFI_OFF);
        netProfile=networkDefaults(AUDIO_HOST,MATRIX_HOST);
        loadDemoRoster();
        ready = true;
        Serial.println("DOOR DEMO: no SD, no Wi-Fi, no announcements");
    } else if (configurationReady()) {
        netProfile=loadNetwork(AUDIO_HOST,MATRIX_HOST);
        IO_EXTENSION_Output(IO_EXTENSION_IO_4, 1);
        // Built-in slot, manufacturer's 1-bit SD_MMC: CLK=12, CMD=11, D0=13.
        // format_if_mount_failed=false: never format a user's card automatically.
        bool mounted = SD_MMC.setPins(12, 11, 13) && SD_MMC.begin("/sdcard", true, false);
        String error;
        ready = mounted && studentsReady(SD_MMC, error) &&
                loadTeachers(SD_MMC, teachers, teacherCount, error);
        if (!ready) {
            blockingTitle = "TF 卡或名冊未就緒";
            blockingHelp = "請檢查門口卡根目錄的兩份 CSV";
            notice = error.length() ? error : "SD mount failed";
        }
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        joinNetwork(netProfile);
        Serial.printf("DOOR LIVE ready=%d (no student or key logs)\n", ready);
    } else Serial.println("DOOR LIVE: configuration incomplete; see config.h");
    lastActivity = millis();
    drawScreen();
}

void loop() {
    if (!canvas) { delay(100); return; }
    if (!UI_DEMO_MODE) {
        // Local USB recovery only; never clear a pending/uncertain call.
        while(Serial.available()){
            char c=Serial.read();if(c=='\r')continue;
            if(c=='\n'){
                if(doorSerialCommand=="STATUS")Serial.printf("DOOR IP=%s MAC=%s\n",WiFi.localIP().toString().c_str(),WiFi.macAddress().c_str());
                else if(doorSerialCommand=="CLEARWIFI"){
                    if(frozen||!requestId.isEmpty())Serial.println("CLEARWIFI denied: pending request");
                    else {eraseNetwork();netProfile=networkDefaults(AUDIO_HOST,MATRIX_HOST);resetSession();joinNetwork(netProfile);Serial.println("DOOR saved network cleared; compile-time defaults restored");}
                }
                doorSerialCommand="";
            }else if(doorSerialCommand.length()<20)doorSerialCommand+=c;
        }
        static String previousIp;
        String currentIp=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():String();
        if(previousIp!=currentIp){previousIp=currentIp;dirty=true;}
        static wl_status_t oldStatus = WL_IDLE_STATUS;
        wl_status_t current = WiFi.status();
        if (current != oldStatus) {
            oldStatus = current; dirty = true;
            if(current!=WL_CONNECTED&&mdnsReady){MDNS.end();mdnsReady=false;}
            if (current == WL_CONNECTED)
                Serial.printf("DOOR IP=%s MAC=%s\n", WiFi.localIP().toString().c_str(),
                              WiFi.macAddress().c_str());
        }
        if (current != WL_CONNECTED && millis() - lastWifiTry > 10000) {
            lastWifiTry = millis(); WiFi.reconnect();
        }
    }
    touch_gt911_point_t point = touch_gt911_read_point(1);
    // Act only on a new press, not every loop while a finger is held down.
    if (point.cnt && !hadTouch) {
        const Hit *found = view.hitTest(point.x[0], point.y[0]);
        if (found) { Hit copy = *found; activate(copy); }
    }
    hadTouch = point.cnt > 0;
    if (page == UiPage::Complete && millis() - successTime >= SUCCESS_HOLD_MS)
        resetSession();
    if (page == UiPage::Welcome) {
        uint32_t elapsed = millis() - welcomeStart;
        if (elapsed >= WELCOME_TOTAL_MS) resetSession();
        else if (elapsed < WELCOME_SLIDE_MS &&
                 millis() - lastWelcomeDraw >= WELCOME_FRAME_MS) {
            lastWelcomeDraw = millis(); dirty = true;   // slide-in animation
        }
    } else if (page == UiPage::Passcode && millis() - lastActivity >= PASSCODE_IDLE_MS) {
        passcode = ""; page = UiPage::Input; notice = ""; dirty = true;
    }
    if (!frozen && (page != UiPage::Input || studentId.length()) &&
        millis() - lastActivity >= (networkPage()?180000:INPUT_IDLE_MS)) resetSession();
    if (frozen && !stopRetry && !door::retryWindowOpen(millis(), requestStarted)) {
        stopRetry = true; notice = "已超過安全重試時限，請找老師確認"; dirty = true;
    }
    if (dirty) drawScreen();
    delay(20);
}

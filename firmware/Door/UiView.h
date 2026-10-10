#pragma once
#include "UiTypes.h"
#include "BitmapText.h"

// Native RGB565 graphics; no HTML, LVGL, browser, or external TFT library.
class UiView {
public:
    static constexpr int WIDTH = 1024, HEIGHT = 600, MAX_HITS = 64;
    static constexpr uint16_t BG = 0xF77E, INK = 0x1148, BLUE = 0x1AF5;
    static constexpr uint16_t MUTED = 0x5B4E, WHITE = 0xFFFF;
    static constexpr uint16_t GREEN = 0x056C, RED = 0xB9E7, DEMO_BG = 0xFFE8;
    static constexpr uint16_t GOLD = 0xFE60, DIM = 0x8C71;
    // Management entry: the discreet dot at the far right of the header bar.
    static constexpr int SECRET_X = 960, SECRET_Y = 0, SECRET_W = 64, SECRET_H = 70;
    Hit hits[MAX_HITS];
    int hitCount = 0;

    void begin(uint16_t *framebuffer) { canvas = framebuffer; }
    const Hit *hitTest(int x, int y) const {
        for (int i = 0; i < hitCount; ++i) {
            const Hit &h = hits[i];
            if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h)
                return &h;
        }
        return nullptr;
    }
    void render(const ViewState &s) {
        if (!canvas) return;
        hitCount = 0;
        // The welcome banner owns the whole panel: no header, no status bar.
        if (s.page == UiPage::Welcome) { drawWelcome(s); return; }
        box(0, 0, WIDTH, HEIGHT, BG);
        box(0, 0, WIDTH, 70, INK);
        label("教員室召喚", 36, 20, WHITE);
        label("輸入編號  >  選老師  >  確認", 438, 24, WHITE, 1, 500);
        label("*", 992, 24, MUTED);
        if (s.ready && !s.frozen && s.page == UiPage::Input && s.studentId.isEmpty() && hitCount < MAX_HITS)
            hits[hitCount++] = {SECRET_X, SECRET_Y, SECRET_W, SECRET_H,
                                UiAction::OpenPasscode, ""};
        box(0, 70, WIDTH, 38, s.demo ? DEMO_BG : WHITE);
        label(s.demo ? "演示模式：不讀 TF、不連線、不發送" : "請輸入學號，再選擇你要找的老師",
              40, 75, s.demo ? RED : MUTED);
        label(!s.ready ? "請聯絡負責老師檢查設備" : s.frozen ? "結果待確認，暫停新召喚" :
              s.page == UiPage::Complete ? "即將自動返回首頁" :
              (s.page == UiPage::NetUnlock || s.page == UiPage::Network || s.page == UiPage::NetworkEdit) ?
              "管理頁 3 分鐘未操作會關閉" : "60 秒未操作會清空資料",
              40, 565, MUTED, 1, 540);
        label(s.demo ? "DEMO / OFFLINE" : s.wifi ? "Wi-Fi 已連線" : "Wi-Fi 離線",
              766, 565, s.demo || s.wifi ? MUTED : RED, 1, 235);

        if (s.page == UiPage::NetUnlock) { drawNetUnlock(s); drawNetNotice(s); return; }
        if (s.page == UiPage::Network) { drawNetwork(s); drawNetNotice(s); return; }
        if (s.page == UiPage::NetworkEdit) { drawNetworkEdit(s); drawNetNotice(s); return; }
        if (!s.frozen && s.page == UiPage::Input && s.studentId.isEmpty())
            button(594, 559, 144, 36, "網絡設定", UiAction::OpenNetwork);
        if (!s.ready) {
            label(s.blockingTitle, 64, 166, RED, 1, 896);
            label(s.blockingHelp, 64, 222, INK, 1, 896);
            label(s.notice, 64, 288, RED, 1, 896);
            label("改程式要重新燒錄；換卡請先斷電", 64, 360, MUTED);
            return; // no active buttons when configuration or SD is invalid
        }
        if (s.page == UiPage::Input) drawInput(s);
        else if (s.page == UiPage::Teachers) drawTeachers(s);
        else if (s.page == UiPage::Confirm) drawConfirm(s);
        else if (s.page == UiPage::Passcode) drawPasscode(s);
        else drawComplete(s);
        if (s.notice.length()) {
            int x = s.page == UiPage::Teachers ? 224 : 40;
            label(s.notice, x, 522, RED, 1, 984 - x);
        }
    }
private:
    uint16_t *canvas = nullptr;
    void pixel(int x, int y, uint16_t color) {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
            canvas[y * WIDTH + x] = color;
    }
    void box(int x, int y, int w, int h, uint16_t color) {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx) pixel(xx, yy, color);
    }
    String fit(const String &s, int maxWidth, int scale = 1) {
        if (textWidth(s, scale) <= maxWidth) return s;
        String result;
        const char *p = s.c_str();
        while (*p) {
            const char *start = p;
            utf8Next(p);
            String next = result;
            for (const char *q = start; q < p; ++q) next += *q;
            if (textWidth(next + "...", scale) > maxWidth) break;
            result = next;
        }
        return result + "...";
    }
    void label(const String &s, int x, int y, uint16_t color = INK,
               int scale = 1, int maxWidth = 944) {
        bitmapText(fit(s, maxWidth, scale), x, y, color,
                   [this](int px, int py, uint16_t c) { pixel(px, py, c); },
                   scale, maxWidth);
    }
    void button(int x, int y, int w, int h, const String &caption,
                UiAction action, const String &value = "", bool primary = false) {
        box(x, y, w, h, primary ? BLUE : WHITE);
        String visible = fit(caption, w - 16);
        label(visible, x + (w - textWidth(visible)) / 2,
              y + (h - GLYPH_SIZE) / 2, primary ? WHITE : INK, 1, w - 16);
        if (hitCount < MAX_HITS) hits[hitCount++] = {x, y, w, h, action, value};
    }
    void drawInput(const ViewState &s) {
        label("請輸入 5 位數字學號", 40, 129);
        box(40, 178, 944, 57, WHITE);
        label(s.studentId.length() ? s.studentId : "例如 12345；保留開頭的 0", 60, 191,
              s.studentId.length() ? INK : MUTED);
        const char *digits="123456789";
        for(int i=0;digits[i];++i)
            button(40+(i%3)*148,254+(i/3)*60,138,52,String(digits[i]),UiAction::Key,String(digits[i]));
        button(40,434,138,52,"清除",UiAction::Clear);
        button(188,434,138,52,"0",UiAction::Key,"0");
        button(336,434,138,52,"刪除",UiAction::Backspace);
        label("只輸入數字，不輸入 E", 540, 279, MUTED);
        label("輸入滿 5 位後按下一步", 540, 334, MUTED);
        button(654,434,330,52,"下一步",UiAction::Next,"",true);
    }
    void drawTeachers(const ViewState &s) {
        label("學號 " + s.studentId + "  " + s.studentClass + "  " + s.maskedName,
              40, 124);
        label("請選擇你要找的老師", 40, 164);
        for (size_t i = 0; i < s.teacherCount && i < 15; ++i)
            button(40 + (i % 3) * 320, 207 + (i / 3) * 60, 304, 52,
                   s.teacherNames[i], UiAction::SelectTeacher, String(i));
        button(40, 518, 164, 38, "返回", UiAction::Back);
    }
    void drawConfirm(const ViewState &s) {
        label("請確認召喚資料", 40, 132);
        box(40, 185, 944, 112, WHITE);
        label("學生 " + s.studentId + "  " + s.maskedName, 64, 201, INK, 1, 896);
        label("老師 " + s.selectedTeacher, 64, 252, INK, 1, 896);
        if (!s.frozen) {
            label(s.demo ? "這是演示，不會通知老師" : "確認後會向室內語音端送出通知",
                  64, 334, MUTED);
            button(40, 411, 244, 64, "重新選擇", UiAction::Back);
            button(656, 411, 328, 64, s.demo ? "演示確認" : "確認召喚",
                   UiAction::Submit, "", true);
        } else {
            label("結果未明或播放失敗，請勿重新叫號", 64, 330, RED, 1, 896);
            button(40, 411, 280, 64, "查詢結果", UiAction::Query);
            if (s.canRetry)
                button(344, 411, 328, 64, "同一請求重試", UiAction::Submit, "", true);
            else label("請找老師確認", 374, 429, RED);
        }
    }
    void drawComplete(const ViewState &s) {
        String title = s.demo ? "演示完成" : s.completeState == "done"
                         ? "語音端回報完成" : "通知已排隊";
        label(title, 104, 175, s.demo ? BLUE : GREEN, 2, 840);
        label(s.demo ? "沒有發送，也不會廣播" : "請在門口稍候，不要重複叫號",
              104, 267);
        label(s.demo ? "切換正式模式後，才會連接室內設備"
                     : "接收通知不代表老師已回應或顯示成功",
              104, 320, MUTED, 1, 840);
        if (s.displayWarning) label("室內顯示異常，請聯絡老師", 104, 371, RED);
        button(684, 431, 300, 64, "下一位同學", UiAction::NextStudent, "", true);
    }
    // Special feature: management passcode. Reached only from the header dot.
    void drawPasscode(const ViewState &s) {
        label("管理員密碼", 40, 118, INK);
        label("輸入正確密碼後，「" + s.bannerText + "」會送到走廊點陣屏", 40, 156,
              MUTED, 1, 944);
        box(40, 196, 944, 62, WHITE);
        String mask;
        for (size_t i = 0; i < s.passcode.length(); ++i) mask += "*";
        label(mask.length() ? mask : "請輸入密碼", 56, 213,
              mask.length() ? INK : MUTED, 1, 912);
        const char *digits = "123456789";
        for (int i = 0; digits[i]; ++i)
            button(40 + (i % 3) * 148, 278 + (i / 3) * 62, 138, 54,
                   String(digits[i]), UiAction::PasscodeKey, String(digits[i]));
        button(40, 464, 138, 54, "清除", UiAction::PasscodeClear);
        button(188, 464, 138, 54, "0", UiAction::PasscodeKey, "0");
        button(336, 464, 138, 54, "刪除", UiAction::PasscodeBack);
        button(540, 464, 210, 54, "取消", UiAction::PasscodeCancel);
        button(774, 464, 210, 54, "確定", UiAction::PasscodeSubmit, "", true);
    }
    // Full-panel welcome banner. bannerX slides in from the right, then parks.
    void drawWelcome(const ViewState &s) {
        box(0, 0, WIDTH, HEIGHT, INK);
        label(s.bannerText, s.bannerX, (HEIGHT - GLYPH_SIZE * 4) / 2, GOLD, 4, 100000);
        label(s.notice.length() ? s.notice
                                : (s.demo ? "演示模式：沒有送到走廊點陣屏"
                                          : "歡迎語已送到走廊點陣屏"),
              40, 556, s.notice.length() ? RED : DIM, 1, 944);
        if (hitCount < MAX_HITS)
            hits[hitCount++] = {0, 0, WIDTH, HEIGHT, UiAction::Dismiss, ""};
    }

    void drawNetNotice(const ViewState&s){if(s.notice.length())label(s.notice,40,522,RED,1,944);}
    String stars(const String &s){String masked;for(size_t i=0;i<s.length();i++)masked+='*';return masked;}
    // Local delta: QWERTY instead of alphabetical. 10 columns x 4 rows = 40
    // slots; a space marks an unused slot so the letter rows line up like a
    // real keyboard. Symbol page unchanged.
    static const char*netKeyMap(int mode){
        static const char*lower="1234567890qwertyuiopasdfghjkl zxcvbnm   ";
        static const char*upper="1234567890QWERTYUIOPASDFGHJKL ZXCVBNM   ";
        static const char*syms ="!@#$%^&*()-_=+[]{};:'\"\\|,.<>/?`~";
        return mode==0?lower:mode==1?upper:syms;
    }
    // Local delta: alphanumeric admin PIN. Reuses the same 3-mode keyboard as
    // drawNetworkEdit so the PIN can be typed on the touchscreen. No new glyphs:
    // every character here already appears in this file's other screens.
    void drawNetUnlock(const ViewState&s){
        label("老師網絡設定：輸入管理 PIN",40,112);
        box(40,154,944,52,WHITE);label(stars(s.netPin),60,166);
        const char*keys=netKeyMap(s.netKeyboard);
        for(int i=0;keys[i]&&i<40;i++){
            if(keys[i]==' ')continue;
            button(40+(i%10)*94,220+(i/10)*49,86,42,String(keys[i]),UiAction::NetEditKey,String(keys[i]));
        }
        button(40,428,200,52,"abc/ABC/符號",UiAction::NetEditShift);
        button(252,428,130,52,"清除",UiAction::NetClear);
        button(394,428,130,52,"刪除",UiAction::NetBackspace);
        button(536,428,200,52,"取消",UiAction::NetCancel);
        button(748,428,236,52,"解鎖",UiAction::NetUnlockSubmit,"",true);
        label("不會更改學生叫號資料",40,495,MUTED);
        label("演示 PIN：123456",540,495,s.demo?BLUE:BG);
    }
    void drawNetwork(const ViewState&s){
        label("本機 IP " + s.ownIp,40,126,INK,1,450);
        label("MAC " + s.ownMac,516,126,INK,1,468);
        label("Wi-Fi " + s.networkSsid,40,166,INK,1,650);
        label(s.netModeName,716,166,BLUE,1,268);
        if(s.netCredentials){
            const char*captions[]={"Wi-Fi 名稱","Wi-Fi 密碼","登入帳號","外層身份"};
            for(int i=0;i<4;i++)button(40+(i%2)*476,222+(i/2)*69,460,58,String(captions[i])+": "+s.netFields[i],UiAction::NetField,String(i));
            button(40,373,222,48,"切換認證方式",UiAction::NetMenu,"mode");
            button(278,373,222,48,"Audio 熱點",UiAction::NetField,"6");
            button(516,373,222,48,"熱點密碼",UiAction::NetField,"7");
            button(754,373,230,48,"返回狀態",UiAction::NetMenu,"status");
            button(40,442,300,54,"門口連接 Wi-Fi",UiAction::NetConnect,"",true);
            button(360,442,300,54,"連 Audio 配置熱點",UiAction::NetPairJoin);
            button(684,442,300,54,"同步給 Audio",UiAction::NetPairSend);
        } else {
            button(40,218,460,58,"Audio IP: "+s.audioHost,UiAction::NetField,"4");
            button(524,218,460,58,"Matrix IP: "+s.matrixHost,UiAction::NetField,"5");
            label(s.netPairing?"正在 Audio 臨時熱點；不可叫號":s.netStatus,40,297,MUTED,1,944);
            // Local delta: four 220px slots so the one-press hotspot switch fits on
            // the same row; the three original captions still fit in 204px.
            button(40,354,220,52,"測試 Audio",UiAction::NetTest);
            button(273,354,220,52,"自動尋找 Audio",UiAction::NetDiscover);
            button(506,354,220,52,"朗讀 Audio IP",UiAction::NetSpeak);
            bool onHotspot=s.networkSsid.length()&&s.networkSsid==s.apSsid&&!s.netPairing;
            button(739,354,220,52,onHotspot?"已在 Audio 熱點":"改用熱點叫號",
                   UiAction::NetJoinAudioAp,"",!onHotspot);
            button(40,435,300,56,"Wi-Fi / 同步配置",UiAction::NetMenu,"credentials",true);
            button(360,435,300,56,"保存 IP 設定",UiAction::NetMenu,"save");
            button(684,435,300,56,"返回叫號",UiAction::NetCancel);
        }
    }
    void drawNetworkEdit(const ViewState&s){
        label(s.netEditCaption,40,125,INK,1,944);
        box(40,171,944,53,WHITE);label(s.netSecret?stars(s.netEditValue):s.netEditValue,54,184,INK,1,916);
        const char*letters=netKeyMap(s.netKeyboard);
        for(int i=0;letters[i]&&i<40;i++){
            if(letters[i]==' ')continue;
            button(40+(i%10)*94,239+(i/10)*49,86,42,String(letters[i]),UiAction::NetEditKey,String(letters[i]));
        }
        button(40,452,200,52,"abc/ABC/符號",UiAction::NetEditShift);
        button(254,452,116,52,"空格",UiAction::NetEditKey," ");
        button(384,452,116,52,"刪除",UiAction::NetBackspace);
        button(514,452,116,52,"清除",UiAction::NetClear);
        button(644,452,140,52,"取消",UiAction::NetMenu,"cancelEdit");
        button(798,452,186,52,"確認欄位",UiAction::NetEditSave,"",true);
    }
};

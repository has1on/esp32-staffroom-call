#pragma once
// ===========================================================================
//   教員室叫號系統  ·  唯一設定檔
//   firmware/staffroom_config.h
// ---------------------------------------------------------------------------
//   整包只有這一個檔案需要你動手改。改完之後，在專案根目錄執行：
//
//        py -X utf8 tools/apply_config.py
//
//   它會先檢查內容，再自動產生下面 6 個檔案（舊的會備份成 .bak）：
//
//        firmware/Door/config.h        firmware/Door/NetworkDefaults.h
//        firmware/Audio/config.h       firmware/Audio/NetworkDefaults.h
//        firmware/Matrix/config.h      firmware/Matrix/NetworkDefaults.h
//
//   ⚠ 不要直接改那 6 個檔案 —— 下次執行 apply_config.py 會把它們蓋掉。
//
//   【最少只要改一行】就是第 1 區的 DEVICE_KEY。
//   其他每一項都有可以立刻跑起來的預設值。
//
//   格式規定：一行一個設定，寫成   constexpr <型別> <名稱> = <值>;
//             值不要跨行，字串用雙引號。工具就是用這個規則讀的。
//             這個檔案本身也是合法 C++，但 firmware 不會 include 它。
// ===========================================================================


// ═══════════════════════════════════════════════════════════════════════════
//  【1】身份　★ 必改　三塊板子必須完全一樣
// ═══════════════════════════════════════════════════════════════════════════

// SYSTEM_ID ── 幫這一套系統取的名字。
//   · 同一個空間只有一套 → 保持 ROOM_A 就好。
//   · 同一間學校有第二套 → 請改成 ROOM_B、ROOM_C……兩套不可以用一樣的。
//   · 只能用 大寫英文字母、數字、減號 -、底線 _，長度 1~16。
//
//   ⚠ 絕對不要用小寫。門口機的檢查是大小寫敏感的，用小寫會開不了機。
//   ⚠ 兩套系統如果同名，兩台的熱點會一模一樣，門口機會連到隔壁那一台。

constexpr const char *SYSTEM_ID = "ROOM_A";

// DEVICE_KEY ── 三塊板子互相認證用的密碼。至少 32 個字元，建議 64 個十六進位字元。
//
//   ★★ 一定要自己重新產生一組，不要沿用下面這個範例值 ★★
//
//   產生方法（任選一種，複製結果貼進下面的雙引號裡）：
//       PowerShell :  -join (1..64 | ForEach-Object { '{0:x}' -f (Get-Random -Maximum 16) })
//       Python     :  py -c "import secrets;print(secrets.token_hex(32))"
//       macOS/Linux:  openssl rand -hex 32
//       或用工具    :  py -X utf8 tools/apply_config.py --new-key
//
//   這組金鑰另外有兩個作用：
//     · 語音機熱點的密碼 = 這串字的前 12 碼
//     · 門口機連語音機熱點用的密碼也 = 前 12 碼
//   所以改金鑰＝熱點密碼也跟著改，三塊板子要一起重新燒錄。

constexpr const char *DEVICE_KEY = "CHANGE_ME";


// ═══════════════════════════════════════════════════════════════════════════
//  【2】語音機要連的 Wi-Fi　（留空 = 純熱點模式，推薦新手）
// ═══════════════════════════════════════════════════════════════════════════

// ★ 建議第一次安裝就讓它留空。
//
//   留空的時候：
//     · 語音機永遠開著自己的熱點，門口機直接連上去
//     · 整套系統不需要學校網路、不需要網管開權限、不會遇到 EAP 憑證問題
//     · 缺點：手機要進語音機的控制台，也得先連上那個熱點
//
//   想進階一點、讓語音機同時也上學校網路（例如想從辦公室電腦連控制台），
//   再把 SSID / 密碼填上，然後重跑 apply_config.py 並重燒 Audio 和 Matrix。
//   門口機不用重燒 —— 它永遠連語音機的熱點。
//
//   ⚠ 只支援 2.4 GHz。5 GHz 的 SSID 連不上。
//   ⚠ 觸控螢幕不能輸入中文 SSID，所以正式網路請在「純熱點模式」下用手機
//      連熱點、開控制台來填（見 docs/功能說明書.md 第 4 章）。
//   ⚠ 填錯不會永遠失聯：熱點是常開的，連錯了還是能連熱點改回來。

constexpr const char *AUDIO_WIFI_SSID = "";
constexpr const char *AUDIO_WIFI_PASSWORD = "";


// ═══════════════════════════════════════════════════════════════════════════
//  【3】矩陣屏要連的 Wi-Fi　（留空 = 跟語音機一樣；再留空 = 連語音機熱點）
// ═══════════════════════════════════════════════════════════════════════════
//
//   這三個條件依序判斷：
//     1. 這裡填了 → 用這裡的
//     2. 這裡留空，但第 2 區有填 → 跟語音機用同一個網路
//     3. 兩邊都留空 → 矩陣屏去連語音機的熱點
//
//   ※ 條件 3 的時候矩陣屏的 IP 由熱點隨機配發、無法預測，
//     所以下面第 4 區的 MATRIX_IP 只能留空（＝不使用矩陣屏）。
//     要真的用矩陣屏，建議用條件 1 或 2，給它一個固定 IP。

constexpr const char *MATRIX_WIFI_SSID = "";
constexpr const char *MATRIX_WIFI_PASSWORD = "";


// ═══════════════════════════════════════════════════════════════════════════
//  【4】三塊板子的 IP
// ═══════════════════════════════════════════════════════════════════════════

// AUDIO_IP ── 語音機的 IP。
//   純熱點模式下熱點閘道固定是 192.168.4.1，不用改。
//   如果語音機連上了學校 Wi-Fi，請填入它在該網路拿到的 IP
//   （語音機開機時串口會印 AUDIO IP=… ，或看它控制台的首頁）。

constexpr const char *AUDIO_IP = "192.168.4.1";

// MATRIX_IP ── 矩陣屏的 IP。留空 = 沒有裝矩陣屏。
//
//   ★ 留空是完全可以的，而且是預設值。
//     留空時叫號流程完全正常（門口機 → 語音機 → 播音），
//     只是沒有走廊點陣屏可看。語音機也不會浪費時間去重試連線。
//   要裝矩陣屏時再填，例如 "192.168.4.50"。

constexpr const char *MATRIX_IP = "";


// ═══════════════════════════════════════════════════════════════════════════
//  【5】語音機硬體接線　★ 每次換板子都要重新確認
// ═══════════════════════════════════════════════════════════════════════════

// AUDIO_PINS_CONFIRMED ── 安全鎖。確認下面 7 個腳位真的是你這塊板子的接線之後，
//   才改成 true。維持 false 的話語音機不會初始化 SD 卡和喇叭，等於播不出聲音。
//
//   為什麼要這道鎖：ESP32-S3 N16R8 有 OPI PSRAM，會佔用 GPIO 26~37；
//   亂填腳位可能直接讓板子開不起來。工具會擋掉明顯危險的腳位，
//   但只有你手上的實物能證明接線是對的。
//
//   本站已經實測可用的一組（N16R8 + SPI microSD + PCM5102A）：
//        SD  ：CS=10  SCK=12  MISO=13  MOSI=11
//        I2S ：BCLK=5  LRCK=6  DATA=7
//   你的板子不一定一樣。請照著 docs/硬體接線.md 逐項確認。

constexpr bool AUDIO_PINS_CONFIRMED = false;

constexpr int PIN_SD_CS = -1;    // microSD 模組的 CS
constexpr int PIN_SD_SCK = -1;   // microSD 模組的 SCK / CLK
constexpr int PIN_SD_MISO = -1;  // microSD 模組的 MISO / DO
constexpr int PIN_SD_MOSI = -1;  // microSD 模組的 MOSI / DI
constexpr int PIN_I2S_BCLK = -1; // PCM5102A 的 BCK
constexpr int PIN_I2S_LRCK = -1; // PCM5102A 的 LRCK
constexpr int PIN_I2S_DATA = -1; // PCM5102A 的 DIN

// USE_CHIME ── 叫號前要不要先播提示音。
//   true 需要 SD 卡根目錄有 chime.wav；沒有這個檔就設 false。

constexpr bool USE_CHIME = false;

// AUDIO_TEST_STAGE ── 語音機的自我測試模式。
//   0 = 正常運作（正式使用請保持 0）
//   1 = 只讀卡檢查（讀 students/TEST001.wav、teachers/01.wav，不出聲）
//   2 = 開機時播一次就停，且不連 Wi-Fi（第一次接喇叭時用，音量請先轉小）

constexpr uint8_t AUDIO_TEST_STAGE = 0;


// ═══════════════════════════════════════════════════════════════════════════
//  【6】語音機熱點行為
// ═══════════════════════════════════════════════════════════════════════════
//
//   熱點名稱固定是：Staffroom-<SYSTEM_ID>-Audio
//   熱點密碼固定是：DEVICE_KEY 的前 12 碼
//   熱點位址固定是：192.168.4.1（WPA2、通道 1）
//   以上三個都由程式推導，不需要（也不能）在這裡設定。

// AUDIO_AP_ALWAYS ── true = 熱點從開機到關機一直開著。
//
//   ★ 預設 true 是刻意的，請不要改成 false。
//     原版設計是「連不上網路 60 秒後才開 10 分鐘」，結果會卡死：
//     語音機連到了錯的網路 → 永遠不開熱點 → 門口機沒有熱點可連
//     → 也就沒有人能把語音機改回正確的網路。
//     常開熱點是唯一能讓「改錯 Wi-Fi」永遠救得回來的做法。

constexpr bool AUDIO_AP_ALWAYS = true;

// AUDIO_AP_MAX_CLIENTS ── 熱點最多讓幾台裝置同時連。
//   門口機一定要佔 1 個；有矩陣屏再 +1；留空間給手機看控制台。

constexpr uint8_t AUDIO_AP_MAX_CLIENTS = 4;

// AUTO_SETUP_DELAY_MS ── 「連不上就自動開熱點」的等待時間（毫秒）。
//   只有在 AUDIO_AP_ALWAYS = false 時才有作用。0 = 關閉這功能。

constexpr uint32_t AUTO_SETUP_DELAY_MS = 60000;


// ═══════════════════════════════════════════════════════════════════════════
//  【7】語音機網頁控制台
// ═══════════════════════════════════════════════════════════════════════════
//
//   手機或電腦打開 http://<語音機IP>/ 就會看到控制台：
//   即時狀態、叫號記錄、手動叫號、以及改 Wi-Fi 的表單。
//   ⚠ 「改 Wi-Fi」只能在連著熱點的時候用（ISP 端擋掉，這是故意的）。

// CONSOLE_PIN ── 控制台的登入碼。
//   ★ 請務必改掉。用於 LAN 上任何一台裝置，太容易被猜到。
//     故意不用 DEVICE_KEY 當密碼 —— 那串 64 字元的金鑰不該被
//     任何一台能連上網的瀏覽器打出來、也不該被塞進網頁裡。

constexpr const char *CONSOLE_PIN = "CHANGE_ME";

// 登入成功後幾毫秒過期（1800000 = 30 分鐘）。

constexpr uint32_t CONSOLE_SESSION_MS = 1800000;

// 連續錯幾次就鎖住登入端點。

constexpr uint8_t CONSOLE_MAX_FAILS = 5;

// 鎖住幾毫秒（60000 = 1 分鐘）。

constexpr uint32_t CONSOLE_LOCK_MS = 60000;


// ═══════════════════════════════════════════════════════════════════════════
//  【8】門口機：歡迎語（右上角那個不明顯的「*」）
// ═══════════════════════════════════════════════════════════════════════════
//
//   點門口機畫面右上角的「*」→ 輸入密碼 → 門口機自己顯示歡迎語，
//   同時直接 POST 給矩陣屏。這是唯一一條不經過語音機的路徑。

// BANNER_TEXT ── 歡迎語文字。
//   · 這些字會自動被 tools/generate_fonts.py 烘進字庫，不用另外處理。
//   · 但改了之後一定要重跑 generate_fonts.py 並重燒門口機。

constexpr const char *BANNER_TEXT = "歡迎光臨";

// BANNER_PASSWORD ── 歡迎語的密碼。保持 CHANGE_ME 時這個功能會被拒絕。

constexpr const char *BANNER_PASSWORD = "CHANGE_ME";


// ═══════════════════════════════════════════════════════════════════════════
//  【9】矩陣屏硬體　★ 沒確認過不要打開
// ═══════════════════════════════════════════════════════════════════════════

// PANEL_CONFIRMED ── 安全鎖。維持 false 時矩陣屏不會初始化，
//   也不會去搶 GPIO。等於整個矩陣屏功能關閉（但系統其他部分照常）。
//
//   為什麼要這道鎖：P5 面板的掃描方式（1/8、1/16）和驅動 IC
//   （SHIFTREG 還是 FM6126A）不同廠牌不一樣，亂開會顯示錯誤畫面
//   甚至過電流。請先跑原廠的 01_SimpleTestShapes 範例確認。

constexpr bool PANEL_CONFIRMED = false;

constexpr bool PANEL_FM6126A = false;   // 面板驅動 IC 是不是 FM6126A
constexpr bool PANEL_CLKPHASE = false;  // 畫面如果整片錯位，把這個改成 true 再試
constexpr uint8_t PANEL_BRIGHTNESS = 32; // 0~255。第一次測試請保持低亮度


// ═══════════════════════════════════════════════════════════════════════════
//  【10】門口機網路設定頁的 PIN 與企業級 Wi-Fi
// ═══════════════════════════════════════════════════════════════════════════

// NETWORK_ADMIN_PIN ── 門口機「網絡設定」頁的密碼：6~12 個英數字。
//   保持 CHANGE_ME 時（含底線，不合格）這個頁面會拒絕解鎖。請務必改掉。

constexpr const char *NETWORK_ADMIN_PIN = "CHANGE_ME";

// 網路設定頁閒置多久自動關閉（600000 = 10 分鐘）。

constexpr uint32_t NETWORK_SETUP_WINDOW_MS = 600000;

// ── 以下是「企業級 / 校園 Wi-Fi」（WPA2-Enterprise, PEAP/TTLS）才需要的 ──
//   一般的家用 Wi-Fi 或手機熱點走 WPA2-PSK，這幾項保持原狀即可。

// WIFI_ENTERPRISE ── 開機時要不要直接用下面的 EAP 設定連線。
//   一般情況保持 false；門口機的網路設定頁可以在現場輸入並存進 NVS。

constexpr bool WIFI_ENTERPRISE = false;

// WIFI_EAP_METHOD ── 1 = PEAP，2 = TTLS。

constexpr uint32_t WIFI_EAP_METHOD = 1;

constexpr const char *WIFI_EAP_IDENTITY = "";  // 通常留空
constexpr const char *WIFI_EAP_USERNAME = "";  // 例如學號或教職員編號

// WIFI_EAP_CA ── 學校的根憑證（PEM 全文）。留空 = 不驗證伺服器憑證。
//   ⚠ 留空就等於不做憑證驗證，只適合自己可控的網路。
//     學校網管通常拿得到 CA，能填就填。

constexpr const char *WIFI_EAP_CA = "";

// ALLOW_INSECURE_EAP ── 沒有填 CA 時，要不要還是允許連線。
//   false 比較安全但沒有 CA 就連不上；true 方便但少一層保護。

constexpr bool ALLOW_INSECURE_EAP = false;

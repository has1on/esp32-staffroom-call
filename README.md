# esp32-staffroom-call · 教員室叫號系統

> 一套自製的校園「教員室叫號」系統。學生在教員室門口的觸控螢幕上輸入學號，
> 室內的語音機就會依序廣播「〇〇〇同學找△△△老師」，走廊的點陣屏同步顯示。
>
> Three ESP32-S3 boards: a touch panel at the door, a speaker box inside the
> staff room, and an optional LED matrix in the corridor. No cloud, no server,
> no app — everything runs on a local Wi-Fi hotspot.

[繁體中文說明在下面](#這是什麼) · [English quick start](docs/English.md)

---

## 這是什麼

```
   ┌─────────────────┐        ┌──────────────────┐        ┌──────────────┐
   │  門口機 Door     │        │  語音機 Audio     │        │  矩陣屏 Matrix│
   │  ESP32-S3        │  HTTP  │  ESP32-S3 N16R8  │  HTTP  │  ESP32-S3    │
   │  Touch-LCD-7B    │ ─────► │  microSD + DAC   │ ─────► │  P5 64×32    │
   │  1024×600 觸控    │  80    │  音響 / 有源喇叭  │  80    │  （可選）     │
   └─────────────────┘        └──────────────────┘        └──────────────┘
          學生按 5 位學號            播「學號→老師」              顯示姓名
```

- **門口機**：一塊 7 吋觸控螢幕。學生輸入**正好 5 位數字**學號，按下確認就把請求送給語音機。
  上面另有隱藏的網路設定頁（PIN 保護），可以現場改 Wi-Fi、測語音機、朗讀語音機 IP。
- **語音機**：插一張 microSD 卡（裡面是每位學生 / 老師的錄音），照順序播音：
  提示音 → 「老師姓名」→「學生班級姓名 同學找」。同時有**網頁控制台**可以看即時狀態、
  叫號記錄、手動叫號、改 Wi-Fi。
- **矩陣屏**（可選）：走廊的 LED 點陣屏，顯示「找哪位老師 / 哪位同學」。

**設計上的三個關鍵決定**

| 決定 | 為什麼 |
|---|---|
| 語音機**永遠開著自己的 Wi-Fi 熱點** | 門口機直接連語音機，不依賴學校網路。就算語音機連錯 Wi-Fi，門口機還是找得到它，也就永遠救得回來。 |
| 錄音放 **SD 卡**，不是燒進固體 | 換名字、換聲音只要換檔案，不用重新編譯。老師的錄音甚至是「先播老師、再播學生」。 |
| **中文點陣字庫是編譯時產生的** | 名冊改了要重跑一次字庫工具，不然新名字會顯示成空白。工具在 `tools/generate_fonts.py`。 |

---

## 硬體需求

| 角色 | 板子 | 其他 | 必備 |
|---|---|---|---|
| 門口機 | Waveshare **ESP32-S3-Touch-LCD-7B**（1024×600、GT911 觸控） | USB-C 線、5 V 電源 | ✅ |
| 語音機 | **ESP32-S3 N16R8**（16 MB flash / 8 MB OPI PSRAM） | microSD 模組（SPI）、**PCM5102A** DAC、有源喇叭或音響、microSD 卡 | ✅ |
| 矩陣屏 | **ESP32-S3 N32R16** | P5 64×32 HUB75 面板、**5 V / ≥4 A** 獨立電源 | ⬜ 可選 |

> ⚠️ 矩陣屏**一定要獨立供電**。從開發板拉電給 P5 面板會造成重開機或燒毀穩壓器。

軟體：Arduino IDE 2.x + **esp32 core 3.3.7** + **ArduinoJson 7.4.3**
（+ Adafruit GFX / BusIO，只有矩陣屏需要）。Python 3.10+，套件見 `requirements.txt`。

---

## 5 分鐘快速開始

```bash
git clone https://github.com/<你的帳號>/esp32-staffroom-call.git
cd esp32-staffroom-call
py -m pip install -r requirements.txt
```

**① 產生一組自己的金鑰**（不要用範例值）

```bash
py -X utf8 tools/apply_config.py --new-key
```

**② 改 `firmware/staffroom_config.h`**

最少只要改這些，其他全部保持預設就能跑：

| 設定 | 說明 |
|---|---|
| `SYSTEM_ID` | 這一套的名字。同一間學校有兩套就要取不同名字（`ROOM_A` / `ROOM_B`）。**只能大寫。** |
| `AUDIO_PINS_CONFIRMED` + 7 個 `PIN_*` | 填你實際的接線，確認無誤後改成 `true`。不動它的話語音機不會播音。 |
| `CONSOLE_PIN` | 語音機網頁控制台的登入碼，請自己改掉。 |
| `NETWORK_ADMIN_PIN` | 門口機網路設定頁的 PIN，6~12 個英數字。 |

**③ 產生三端的設定檔**

```bash
py -X utf8 tools/apply_config.py
```

它會從上面那一個檔案產生 6 個檔案（`firmware/{Door,Audio,Matrix}/config.h` 與
`NetworkDefaults.h`），舊的會備份成 `.bak`。**那 6 個檔案不要手改。**

**④ 編譯並燒錄**

照 [`docs/燒錄與硬體.md`](docs/燒錄與硬體.md)。編譯後把 `firmware/Door`、
`firmware/Audio`、`firmware/Matrix` 三個資料夾各自當成 sketch 開進 Arduino IDE。

**⑤ 準備 SD 卡**

把 `sd_templates/AUDIO/` 的內容複製到語音機的卡上，再照你自己的名冊放錄音。
格式規定見 [`docs/功能說明書.md`](docs/功能說明書.md) 第 7 章。

第一次安裝建議**先不要連學校網路**：`AUDIO_WIFI_SSID` 留空，走「純熱點模式」，
整套系統自成一個小圈圈，最容易一次成功。之後要接學校網路再開控制台慢慢調。

---

## 目錄結構

```
esp32-staffroom-call/
├─ firmware/
│  ├─ staffroom_config.h     ★ 唯一需要你改的檔案
│  ├─ Door/                  門口機 sketch
│  ├─ Audio/                 語音機 sketch（含網頁控制台 Console.h）
│  ├─ Matrix/                矩陣屏 sketch
│  └─ NetworkCheck/          純網路診斷用的獨立 sketch
├─ tools/
│  ├─ apply_config.py        ★ 總設定 → 三端 config.h
│  ├─ generate_fonts.py      名冊 → 中文點陣字庫
│  ├─ validate_sd.py         離線檢查 SD 卡內容與 WAV 格式
│  ├─ make_rosters.py        名冊 CSV 產生 / 檢查
│  ├─ prepare_wav.py         WAV 格式轉換
│  ├─ probe_audio.py         從電腦直接測試語音機 API
│  └─ prepare_upgrade.py     把既有安裝搬進新版本（保留設定）
├─ sd_templates/             兩張 SD 卡的目錄範本
├─ master_templates/         虛構的名冊範本
├─ sd_first_test/            只放一筆資料的首次測試範本
├─ tests/                    軟體測試（含 Door 的原生主機測試）
├─ demo/                     電腦上的網頁模擬器，不接硬體也能看流程
├─ assets/                   Noto Sans CJK 字型（OFL）與預覽圖
└─ docs/                     功能說明書、燒錄指南、硬體接線
```

---

## 功能一覽

**門口機**
- 學生入口：**正好 5 位數字**，只有數字鍵盤，前導零保留（`00012` ≠ `12`）
- 相容舊的 `E` + 5 位數字別名，但拒絕與純數字 ID 碰撞
- 成功 / 失敗都有明確畫面；顯示查到的姓名（依名冊遮名規則）
- 隱藏網路設定頁（PIN 保護）：自身 IP / MAC、SSID、Audio 與 Matrix 目標 IP、
  語音機健康測試、mDNS 自動尋找、朗讀語音機 IP、Wi-Fi 表單
- 一鍵「改用熱點叫號」：自動把門口機切到語音機的熱點
- 歡迎語（右上角 `*` 進入，密碼保護，直接送給矩陣屏）

**語音機**
- SD 卡 WAV 播放（PCM 16-bit、16 或 22.05 kHz、單聲道或立體聲、≤30 秒）
- 呼叫佇列 8 筆、去重、30 秒冷卻、128 筆歷史（保留 10 分鐘）
- 老師先播、學生後播；可選提示音
- 網頁控制台：即時狀態、叫號記錄、手動叫號、Wi-Fi 設定表單
- 純熱點模式：連不上任何網路也能運作

**系統**
- 純 HTTP + 共用金鑰（`X-Device-Key`）與 `systemId` 雙重核對
- 開機 ID 比對（`expectedBootId`）避免誤判「按了沒播」
- mDNS：`sr-audio-<system_id 小寫>.local`

---

## 已知限制（請先讀）

- **本專案只有軟體層級的編譯與測試證據，沒有實機驗收。** 硬體、Wi-Fi、
  觸控、聲光、供電都要你自己照 `ACCEPTANCE_CHECKLIST.csv` 逐項確認。
- 通訊是**明碼 HTTP**，沒有 TLS。只適合放在自己控制的區網或熱點裡。
- 裝置金鑰一次只能有一組，**沒有金鑰輪替機制**。要換金鑰就是三塊板子一起重燒。
- 兩套系統不能用同一個 `SYSTEM_ID`：熱點名稱與密碼都會一模一樣，門口機會連錯台。
- 語音機的網頁控制台**只能在熱點上改 Wi-Fi**（`setup_ap_required`），這是刻意的。
- 矩陣屏的面板掃描方式與驅動 IC 各廠牌不同，**沒跑過原廠範例前不要開 `PANEL_CONFIRMED`**。
- `AUDIO_PINS_CONFIRMED` 與 `PANEL_CONFIRMED` 兩道安全鎖預設是 `false`，
  程式碼裡沒有任何地方可以繞過它們。

---

## 授權

本專案自行撰寫的部分採 **MIT**，見 [LICENSE](LICENSE)。
隨包的廠商驅動（Waveshare / Espressif，Apache-2.0）與字型
（Noto Sans CJK，SIL OFL 1.1）各有自己的授權，來源與固定提交見 [NOTICE.md](NOTICE.md)。

## 致謝

- [Waveshare ESP32-S3-Touch-LCD-7B](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-7B) — 門口機驅動
- [Waveshare ESP32-S3-RGB-Matrix](https://github.com/waveshareteam/ESP32-S3-RGB-Matrix) — 矩陣屏驅動
- [ESP32-HUB75-MatrixPanel-I2S-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA) — HUB75 面板驅動
- [Noto CJK](https://github.com/notofonts/noto-cjk) — 點陣字型來源

# 第三方來源與授權

本專案自行撰寫的部分採 MIT（見 [LICENSE](LICENSE)）。底下是隨包附上的
廠商驅動與字型的來源。硬體資料來自廠商官方頁面與公開倉庫，
**本專案不是廠商認證的產品**。請保留所有原始版權宣告。

## 門口機：Waveshare ESP32-S3-Touch-LCD-7B

- 倉庫：<https://github.com/waveshareteam/ESP32-S3-Touch-LCD-7B>
- 固定提交：`c652c902db607f7ffb376257393cfd7657aa6428`
- 隨包檔案：`firmware/Door/rgb_lcd_port.*`、`gt911.*`、`touch.*`、`i2c.*`、`io_extension.*`
- 部分廠商 `.c` 檔在本專案改用 `.cpp` 副檔名，以配合 Arduino C++ 工程；
  函式與腳位沿用 7B 範例。
- 授權：**Apache-2.0**，見 `firmware/Door/VENDOR-LICENSE.txt`。
  Espressif / Waveshare 的檔案抬頭一律保留。

## 矩陣屏：Waveshare ESP32-S3-RGB-Matrix

- 倉庫：<https://github.com/waveshareteam/ESP32-S3-RGB-Matrix>
- 固定提交：`4047e4e7e13b043ae9ddd4daaeff5582de044c80`
- 來源：`example/arduino_v3.3.7/01_SimpleTestShapes` 內隨附的驅動與 `platforms/`。
- 底層為 [ESP32-HUB75-MatrixPanel-I2S-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA)，
  原作者的衍生記錄保留在原始檔中。
- 授權：**Apache-2.0**，見 `firmware/Matrix/VENDOR-LICENSE.txt`。
- 本專案只使用**單塊 64×32**，並使用該板官方 HUB75 腳位；D 列接 GPIO42，
  64×32 不使用 E 列。
- **不保證**所有非標準掃描方式或驅動 IC 都能用。請照 `docs/燒錄與硬體.md`
  先用原廠範例確認過再開 `PANEL_CONFIRMED`。
- Adafruit GFX / BusIO 請自行透過官方 Library Manager 安裝，本包不轉散佈其安裝快取。

## 中文字型：Noto Sans CJK

- 來源：<https://github.com/notofonts/noto-cjk>
- 檔案：`assets/NotoSansCJKsc-Regular.otf`（Noto Sans CJK SC Regular）
- 授權：**SIL Open Font License 1.1**，見 `assets/FONT-LICENSE.txt`。
- `tools/generate_fonts.py` 只輸出實際需要的單色點陣子集，原始字型未被改名或修改。
- 字型置中方式採用**實際墨跡邊界**而非字體基線，避免漢字底部筆畫被裁掉。

## 本專案自行撰寫的部分

- `firmware/Door/`：`Door.ino`、`UiView.h`、`UiTypes.h`、`ui_config.h`、
  `Roster.h`、`DeliveryPolicy.h`、自製的 `glyphs.h`
- `firmware/Audio/`：`Audio.ino`、`Console.h`（網頁控制台）、`Roster.h`
- `firmware/Matrix/`：`Matrix.ino`
- `firmware/`（共用）：`CommonCore.h`、`NetworkValidation.h`、`NetworkProfile.h`、
  `NetworkDefaults.h`（產生物）
- `tools/`：所有 Python 工具
- `tests/`、`demo/`、`docs/`、`sd_templates/`、`master_templates/`、`sd_first_test/`

其中 CSV / RIFF 格式驗證、離線播放器、HTTP 佇列與去重、網路檢查、
電腦模擬器、資料工具與文件都是自行撰寫。

**本倉庫不包含任何真實學生名冊、真實錄音、真實部署密碼或已配置好的固體檔。**

---

## 版本沿革

### v0.3 網路強化版（2026-10）

在既有的優化安裝之上，保留歡迎語動畫、老師優先播報、佇列與安全重試。新增：

- 五位純數字學號入口
- 門口機 PIN 保護的網路設定頁：自身 IP/MAC、目標測試、按鍵 mDNS 尋找
- 語音機臨時熱點配網、有限的 EAP 方法、CA 安全停點、已錄 IP 片段播放
- 語音機網頁控制台（`/`）、`/diag` 診斷端點
- 熱點常開設計，移除「連錯網路就再也救不回來」的死結
- 保留設定的升級工具

### v0.4 開源整理版（本版）

- **導入唯一設定檔** `firmware/staffroom_config.h` 與 `tools/apply_config.py`。
  原本要分別改三份 `config.h` 與三份 `NetworkDefaults.h`，現在只改一個檔案。
- 移除 `tools/make_config.py`（與新流程衝突）。
- 熱點相關推導值（熱點名稱、熱點密碼）改由工具自動產生，不會打錯。
- 支援「純熱點模式」：`AUDIO_WIFI_SSID` 留空即可完全不依賴外部網路。
- `generate_fonts.py` 一併掃描 `staffroom_config.h`，
  歡迎語的字不會再漏掉。
- 移除所有實際安裝的識別資訊（學校名稱、真實 IP/MAC、金鑰、PIN、名冊、錄音）。

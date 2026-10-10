# 把既有安裝升級到新版本（保留原本設定）

## 這個工具在做什麼

`tools/prepare_upgrade.py` 會把**已經在跑的那一套**（網路、裝置金鑰、SYSTEM_ID、
GPIO、安全鎖、字庫）原封不動搬到新版本的程式碼上，產生**一個全新的目錄**。

它**絕對不會**修改你的原安裝。它**不會**複製名冊或 WAV 檔。

> ⚠️ 升級後的目錄含有你真實的 Wi-Fi 密碼與裝置金鑰。**不要上傳、不要分享、
> 不要丟進公開的 ZIP。**

## 什麼時候要用它

- 你已經有一套在用，只想換成新版程式，不想重新設定一次
- 你的名冊有很多中文字，不想重新產生字庫（工具會把新舊字庫合併）

如果你是要**從零開始裝一套新的**，不要用這個 —— 直接用
[README.md](README.md) 的 `apply_config.py` 流程。

## 步驟

### 0. 先備份

關掉 Arduino 的序列埠監看、關掉編輯器裡開著的原目錄。
把原安裝目錄、兩張 SD 卡、測試記錄都複製一份。

### 1. 決定校園網路的憑證

- **原本用一般密碼（WPA2-PSK）網路** → 下面的指令不要加 `--ca`
- **原本用校園帳號（PEAP / TTLS）** → 先向網管索取伺服器 **CA 憑證 PEM**

工具**不會**默默沿用「沒有 CA 就不驗證」的舊行為。缺 CA 而沒有明確同意的話，
它會在**寫入前**就拒絕，而且不碰任何原檔。

如果學校已經明確同意在隔離網路關閉憑證驗證，才另外加 `--allow-insecure-eap`，
並且要在提示時親自輸入 `ALLOW`。**這個風險要學校批准，不是工具或文件能替你決定的。**

### 2. 執行

```bash
# 一般密碼網路
py tools/prepare_upgrade.py <你的原安裝目錄> <新目錄>

# 校園帳號網路
py tools/prepare_upgrade.py <你的原安裝目錄> <新目錄> --ca 學校CA.pem
```

- 會請你輸入新的 **6~12 位數字** 網路管理 PIN（輸入時不顯示）。
- **輸出目錄必須是全新的，而且不能跟原目錄互相嵌套。**

### 3. 核對

打開新目錄裡的 `PRIVATE_UPGRADE_README.txt`，然後逐端核對：

- `SYSTEM_ID` / `DEVICE_KEY` 三端是否一致
- GPIO 與 `AUDIO_PINS_CONFIRMED` / `PANEL_CONFIRMED` 是否保留原本的值
- `MATRIX_HOST`、`isUseChime`、歡迎語設定是否還在

### 4. 燒錄

從新目錄的 `firmware/` 分別上傳 Door、Audio、Matrix
（細節見 [docs/燒錄與硬體.md](docs/燒錄與硬體.md)）。
已經接線合格、確認過的那一端不需要重接。

> ⚠️ **升級後的目錄不要再跑 `apply_config.py`。** 那個目錄是「已配置的私人目錄」，
> 裡面的 `firmware/*/config.h` 是真實設定，不是產生物。

## 常見狀況

| 訊息 | 意思 | 怎麼辦 |
|---|---|---|
| `Output already exists` | 輸出目錄已經有了 | 換一個新目錄名稱 |
| `source is not configured` | 原目錄的 `config.h` 裡金鑰太短或 SYSTEM_ID 不合規 | 先修好原目錄的那一份 |
| `Unsupported literal in ...` | 原設定用了工具看不懂的寫法 | 停止，請技術人員手動處理 |
| `Enterprise source has no server CA` | 缺 CA | 取得 CA，或經學校同意後用 `--allow-insecure-eap` |
| `SYSTEM_ID / DEVICE_KEY do not match across roles` | 三端身份不一致 | 先修原目錄，不要帶著不一致升級 |

## 回退

原安裝完全沒被改動，所以回退就是**把原目錄的程式碼重燒回去**、
換回原本那兩張卡。

⚠️ 注意：**已經存在裝置裡的 NVS 網路設定會覆蓋原始碼預設值。**
重燒不會自動清掉 NVS。要清掉的話看
[docs/功能說明書.md](docs/功能說明書.md) 的故障排除章節。

回退前請先停掉學生叫號，並人工確認有沒有「結果未明」的請求還沒播完。

# 從這裡開始

## 我只有 5 分鐘

```bash
py -m pip install -r requirements.txt
py -X utf8 tools/apply_config.py --new-key      # 產生一組自己的裝置金鑰
# 打開 firmware/staffroom_config.h，填 PIN、確認接線
py -X utf8 tools/apply_config.py                # 產生三端的 config.h
```

接著看 **[docs/燒錄與硬體.md](docs/燒錄與硬體.md)**，它有逐步的點亮順序。

## 我想先看看它是什麼樣子，不想碰硬體

```bash
py demo/server.py
```

打開 <http://127.0.0.1:8000/>。這是一個純記憶體的模擬器，
不會碰你的檔案、不會連任何裝置。輸入 `00001` 試試看。

## 我想知道裝好之後怎麼操作

**[docs/功能說明書.md](docs/功能說明書.md)**：熱點特性、語音機網頁控制台怎麼連怎麼改、
門口機的聯網功能、完整 API、WAV 格式規定、故障排除。

## 我已經有一套在跑，想把設定搬過來

看 **[UPGRADE.md](UPGRADE.md)**。它會把舊安裝的網路、身份、接線、字庫
原封不動複製到新目錄，**不會動到原本那一份**。

## 順序建議

1. 讀 [README.md](README.md) 的「已知限制」——先把期待放對地方
2. 做完設定（上面那三行）
3. 照 [docs/燒錄與硬體.md](docs/燒錄與硬體.md) 點亮語音機 → 門口機 → 矩陣屏
4. 逐項填 `ACCEPTANCE_CHECKLIST.csv`
5. 硬體都通了之後，再看 [docs/功能說明書.md](docs/功能說明書.md) 調細節

## 這個版本被驗證到什麼程度

看 [TEST_REPORT.md](TEST_REPORT.md) 與 `test_results/`。簡單說：

- ✅ 四個 sketch 都編得過（ESP32 core 3.3.7）
- ✅ 門口機的畫面邏輯在主機上跑過 77 項檢查
- ✅ Python 工具測試全數通過
- ❌ **沒有任何實機驗收。** Wi-Fi、觸控、聲音、面板、供電、接線都要你自己確認。

> ⚠️ 本專案只有**軟體層級**的編譯與測試證據。硬體、Wi-Fi、觸控、聲光、供電
> 都要你照 `ACCEPTANCE_CHECKLIST.csv` 自己確認。

# 測試與編譯紀錄

## 已驗證

| 項目 | 方法 | 結果 |
|---|---|---|
| Door 編譯 | arduino-cli，ESP32 core 3.3.7 | OK（1252271 bytes） |
| Audio 編譯 | arduino-cli，ESP32 core 3.3.7 | OK（1169067 bytes） |
| Matrix 編譯 | arduino-cli，ESP32 core 3.3.7 | OK（1081107 bytes） |
| NetworkCheck 編譯 | arduino-cli，ESP32 core 3.3.7 | OK（876215 bytes） |
| 門口機原生測試 | 用 g++ 編譯真實的 `UiView.h`/`Roster.h`/`DeliveryPolicy.h` | `ALL 77 checks passed.` |
| Python 測試 | `unittest discover -s tests` | 全數通過 |
| 設定產生器 | `tests/test_apply_config.py` | 產生、驗證、備份、回滾都涵蓋 |

## 沒有驗證（不要當成通過）

- **任何實機行為**：Wi-Fi 連線、觸控、喇叭出聲、LED 面板、供電、
  接線正確性，全部沒有在本倉庫驗證過。
- **矩陣屏**：`PANEL_CONFIRMED` 預設 `false`，面板掃描與驅動 IC 未確認。
- **語音機接線**：`AUDIO_PINS_CONFIRMED` 預設 `false`，GPIO 是空的（`-1`）。
- 實機驗收請照 `ACCEPTANCE_CHECKLIST.csv` 逐項填寫。

## 怎麼自己重跑

```bash
py -X utf8 tools/apply_config.py --new-key
py -X utf8 -m unittest discover -s tests -p "test_*.py"
g++ -std=c++17 -I tests/native/host -I firmware/Door \
    tests/native/native_tests.cpp -o native_tests && ./native_tests out
```

逐檔的完整輸出在 `test_results/` 底下。

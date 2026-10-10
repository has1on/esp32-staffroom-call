# 驗證摘要

由 `private/tools/verify_oss.py` 產生：把整棵樹複製到暫存目錄，
用唯一設定檔產生三端 `config.h`，然後編譯四個 sketch 並跑原生測試。

> ⚠️ 這只證明**軟體層級**可以編譯、可以通過測試。
> 不代表任何實機（Wi-Fi、觸控、聲光、供電、接線）已經驗收。

## 編譯

| Sketch | 結果 | 大小 (bytes) |
|---|---|---|
| Door | OK | 1252271 |
| Audio | OK | 1169067 |
| Matrix | OK | 1081107 |
| NetworkCheck | OK | 876215 |

## 測試

- Python 測試套件：`py -X utf8 -m unittest discover -s tests -p "test_*.py"`
- 門口機原生主機測試：`g++ -std=c++17 -I tests/native/host -I firmware/Door tests/native/native_tests.cpp -o native_tests && ./native_tests out`

**整體結果：全部通過**

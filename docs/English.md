# esp32-staffroom-call — English quick start

A three-board ESP32-S3 classroom call system. A student types a 5-digit ID on a
touch panel at the staff-room door; a speaker box inside plays
"*teacher name* … *student name is looking for you*"; an optional LED matrix in
the corridor shows the same. No cloud, no server, no phone app — the audio board
runs its own Wi-Fi hotspot and everything talks plain HTTP/80 over it.

The project's documentation is written in Traditional Chinese. This page is the
short English version.

## Hardware

| Role | Board | Notes |
|---|---|---|
| Door | Waveshare **ESP32-S3-Touch-LCD-7B** (1024×600, GT911 touch) | 16 MB flash, OPI PSRAM |
| Audio | **ESP32-S3 N16R8** | SPI microSD + **PCM5102A** DAC + powered speaker |
| Matrix (optional) | **ESP32-S3 N32R16** | P5 64×32 HUB75 panel, **separate 5 V / ≥4 A supply** |

## Configure

Everything lives in **one file**: `firmware/staffroom_config.h`.

```bash
py -m pip install -r requirements.txt
py -X utf8 tools/apply_config.py --new-key   # mints a fresh 64-hex DEVICE_KEY
#   ... edit firmware/staffroom_config.h ...
py -X utf8 tools/apply_config.py             # writes the 6 generated headers
```

`apply_config.py` generates `firmware/{Door,Audio,Matrix}/config.h` and
`NetworkDefaults.h` from that single file. **Do not edit those six** — they are
build artifacts. Old copies are kept as `.bak`.

The only field you *must* change is `DEVICE_KEY`. Two fields you really should:

- `CONSOLE_PIN` — login for the Audio's browser console
- `NETWORK_ADMIN_PIN` — 6–12 alphanumerics, for the Door's network settings page

Two safety interlocks ship as `false` and cannot be bypassed in code:

- `AUDIO_PINS_CONFIRMED` — audio won't touch SD/I2S until you confirm your wiring
- `PANEL_CONFIRMED` — the matrix panel won't initialise until you've verified it
  with the vendor's own example

## Pure hotspot mode (recommended)

Leave `AUDIO_WIFI_SSID` empty. The Audio never joins an external network; it just
keeps its own AP up (`Staffroom-<SYSTEM_ID>-Audio`, password = first 12 chars of
`DEVICE_KEY`, gateway `192.168.4.1`) and the Door joins it. No school network,
no network admin, no EAP certificate headaches.

## Build

Arduino IDE 2.x, **esp32 core 3.3.7**, **ArduinoJson 7.4.3**
(plus Adafruit GFX / BusIO for the matrix). Open the *folder*, not the `.ino`.

```
Door    esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=default
Audio   esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc
Matrix  esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_fat24M_32MB,CDCOnBoot=cdc
```

Full details — wiring, bring-up order, flashing a merged `.bin` — are in
[docs/燒錄與硬體.md](燒錄與硬體.md) (Chinese, with diagrams and command lines).

## Try it without hardware

```bash
py demo/server.py      # then open http://127.0.0.1:8000/
```

An in-memory simulator. Enter `00001`. It never touches your files or the network.

## Tests

```bash
py -X utf8 -m unittest discover -s tests -p "test_*.py" -v
g++ -std=c++17 -I tests/native/host tests/native/native_tests.cpp -o native_tests && ./native_tests
```

The second one compiles the Door's real `UiView.h` / `Roster.h` /
`DeliveryPolicy.h` on the host and renders 14 PPM frames.

## Limitations — read these

- **Software-tested only.** No hardware acceptance was performed. Verify Wi-Fi,
  touch, audio, power and wiring yourself against `ACCEPTANCE_CHECKLIST.csv`.
- Plain **HTTP, no TLS**. LAN or hotspot use only.
- One device key, **no rotation**. Changing it means reflashing all three boards.
- Two installations must **not** share a `SYSTEM_ID` — the hotspots would be
  identical and a Door could attach to the wrong Audio.
- The Audio's console can only change Wi-Fi **while you are on its hotspot**
  (`setup_ap_required`). That is deliberate: it is what makes a wrong Wi-Fi
  setting always recoverable.
- WAV files must be PCM 16-bit, 16000 or 22050 Hz, ≤30 s.

## License

MIT for the project's own code — see [LICENSE](../LICENSE).
Bundled vendor drivers are Apache-2.0 (Waveshare/Espressif) and the font is
SIL OFL 1.1 (Noto Sans CJK). See [NOTICE.md](../NOTICE.md) for pinned commits.

"""從 firmware/staffroom_config.h 產生三端的 config.h 與 NetworkDefaults.h。

用法（在專案根目錄）：
    py -X utf8 tools/apply_config.py              檢查後產生 6 個檔案
    py -X utf8 tools/apply_config.py --check      只檢查，什麼都不寫
    py -X utf8 tools/apply_config.py --new-key    先換一組新的 DEVICE_KEY 再產生
    py -X utf8 tools/apply_config.py --show       產生後順便印出來看

設計原則：
  · 唯一來源是 staffroom_config.h，那 6 個檔案永遠是產生物。
  · 先全部檢查完才開始寫；任何一項不合格就整批不寫。
  · 舊檔一律備份成 .bak，絕不覆蓋既有的 .bak。
"""
import argparse
import ipaddress
import json
import re
import secrets
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROLES = ("Door", "Audio", "Matrix")

# 允許 `const char *NAME`、`const char * NAME`、`bool NAME`、`int NAME` 等寫法。
LINE = re.compile(r"^\s*constexpr\s+(.+?)\s*\*?\s*([A-Za-z_]\w*)\s*=\s*(.+?);\s*(?://.*)?$")
KEY_LINE = re.compile(r'(\s*constexpr\s+const\s+char\s*\*\s*DEVICE_KEY\s*=\s*)"([^"]*)"(\s*;)')

# validAudioPins() 允許的 GPIO，兩份必須一致。
ALLOWED_GPIO = set([1, 2]) | set(range(4, 19)) | {21} | set(range(38, 43)) | {47, 48}

PIN_NAMES = ("PIN_SD_CS", "PIN_SD_SCK", "PIN_SD_MISO", "PIN_SD_MOSI",
             "PIN_I2S_BCLK", "PIN_I2S_LRCK", "PIN_I2S_DATA")


class ConfigError(Exception):
    pass


def master_path(root):
    return Path(root) / "firmware" / "staffroom_config.h"


def bak_path(path):
    """config.h → config.h.bak。不要用 Path.with_suffix，那個會把 .h 吃掉。"""
    return path.parent / (path.name + ".bak")


def parse_master(root):
    path = master_path(root)
    if not path.exists():
        raise ConfigError(f"找不到 {path}")
    values, dupes = {}, []
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        m = LINE.match(line)
        if not m:
            continue
        name, raw = m.group(2), m.group(3).strip()
        if name in values:
            dupes.append(f"{name}（第 {n} 行重複定義）")
        values[name] = raw
    if dupes:
        raise ConfigError("staffroom_config.h 有重複的設定：" + "、".join(dupes))
    return values


def need(v, name):
    if name not in v:
        raise ConfigError(f"staffroom_config.h 缺少設定 {name}")
    return v[name]


def as_str(v, name):
    raw = need(v, name)
    if not (raw.startswith('"') and raw.endswith('"')):
        raise ConfigError(f"{name} 必須是雙引號字串，現在是 {raw}")
    return raw[1:-1]


def as_int(v, name):
    raw = need(v, name)
    try:
        return int(raw)
    except ValueError:
        raise ConfigError(f"{name} 必須是整數，現在是 {raw}")


def as_bool(v, name):
    raw = need(v, name)
    if raw not in ("true", "false"):
        raise ConfigError(f"{name} 必須是 true 或 false，現在是 {raw}")
    return raw == "true"


def cq(s):
    return json.dumps(s, ensure_ascii=False)


def validate(v):
    """回傳 (推導值字典, 警告清單)。任何一項不合格就丟 ConfigError。"""
    warn = []

    system_id = as_str(v, "SYSTEM_ID")
    if not re.fullmatch(r"[0-9A-Z_-]{1,16}", system_id):
        if re.fullmatch(r"[0-9a-zA-Z_-]{1,16}", system_id):
            raise ConfigError(
                f"SYSTEM_ID 用了小寫：{system_id}\n"
                "  門口機的大小寫檢查是嚴格的，小寫會讓它開不了機。"
                f"請改成全大寫，例如 {system_id.upper()}")
        raise ConfigError(f"SYSTEM_ID 只能是大寫英數字、- 和 _，長度 1~16：{system_id}")

    device_key = as_str(v, "DEVICE_KEY")
    if "CHANGE_ME" in device_key:
        raise ConfigError(
            "DEVICE_KEY 還是範例值。請產生一組自己的：\n"
            "    py -X utf8 tools/apply_config.py --new-key\n"
            '  或  py -c "import secrets;print(secrets.token_hex(32))"')
    if len(device_key) < 32:
        raise ConfigError(f"DEVICE_KEY 太短（{len(device_key)} 字元），至少要 32。")
    if len(device_key) < 48:
        warn.append(f"DEVICE_KEY 只有 {len(device_key)} 字元，建議 64 個十六進位字元。")

    audio_ssid = as_str(v, "AUDIO_WIFI_SSID")
    audio_pass = as_str(v, "AUDIO_WIFI_PASSWORD")
    matrix_ssid = as_str(v, "MATRIX_WIFI_SSID")
    matrix_pass = as_str(v, "MATRIX_WIFI_PASSWORD")
    if "CHANGE_ME" in (audio_ssid, matrix_ssid):
        raise ConfigError('Wi-Fi 名稱還是 CHANGE_ME。不想連學校網路就填空白字串 ""。')
    if len(audio_ssid.encode()) > 32:
        raise ConfigError("AUDIO_WIFI_SSID 超過 32 位元組。")
    if len(matrix_ssid.encode()) > 32:
        raise ConfigError("MATRIX_WIFI_SSID 超過 32 位元組。")

    audio_ip = as_str(v, "AUDIO_IP")
    matrix_ip = as_str(v, "MATRIX_IP")
    try:
        ipaddress.IPv4Address(audio_ip)
    except ValueError:
        raise ConfigError(f"AUDIO_IP 不是合法的 IPv4：{audio_ip}")
    if matrix_ip:
        try:
            ipaddress.IPv4Address(matrix_ip)
        except ValueError:
            raise ConfigError(f"MATRIX_IP 不是合法的 IPv4：{matrix_ip}")
        if matrix_ip == audio_ip:
            raise ConfigError("AUDIO_IP 和 MATRIX_IP 不能一樣。")

    pins = {n: as_int(v, n) for n in PIN_NAMES}
    confirmed = as_bool(v, "AUDIO_PINS_CONFIRMED")
    if confirmed:
        bad = [n for n, p in pins.items() if p not in ALLOWED_GPIO]
        if bad:
            raise ConfigError(
                "AUDIO_PINS_CONFIRMED 是 true，但這些腳位不合法或仍是 -1："
                + "、".join(bad)
                + "\n  允許的 GPIO：1、2、4~18、21、38~42、47、48。")
        if len(set(pins.values())) != len(pins):
            raise ConfigError("有兩個以上的腳位填成同一個 GPIO。")
    else:
        warn.append("AUDIO_PINS_CONFIRMED = false → 語音機不會初始化 SD 卡和喇叭，"
                    "現在燒上去是沒有聲音的。確認接線後改成 true。")

    stage = as_int(v, "AUDIO_TEST_STAGE")
    if stage not in (0, 1, 2):
        raise ConfigError("AUDIO_TEST_STAGE 只能是 0、1 或 2。")
    if stage != 0:
        warn.append(f"AUDIO_TEST_STAGE = {stage} → 語音機處於測試模式，不是正式運作。")

    max_clients = as_int(v, "AUDIO_AP_MAX_CLIENTS")
    if not 1 <= max_clients <= 8:
        raise ConfigError("AUDIO_AP_MAX_CLIENTS 請填 1~8。")

    if as_str(v, "CONSOLE_PIN") == "CHANGE_ME":
        warn.append("CONSOLE_PIN 還是 CHANGE_ME → 網頁控制台等於是沒有鎖。請改掉。")
    if not re.fullmatch(r"[0-9A-Za-z]{6,12}", as_str(v, "NETWORK_ADMIN_PIN")):
        warn.append("NETWORK_ADMIN_PIN 不合格（要 6~12 個英數字）→ "
                    "門口機的「網絡設定」頁會拒絕解鎖。")
    if as_str(v, "BANNER_PASSWORD") == "CHANGE_ME":
        warn.append("BANNER_PASSWORD 還是 CHANGE_ME → 門口機的歡迎語功能會被拒絕。")
    if not as_str(v, "BANNER_TEXT").strip():
        raise ConfigError("BANNER_TEXT 不可以是空的。")

    if as_bool(v, "PANEL_CONFIRMED"):
        warn.append("PANEL_CONFIRMED = true → 矩陣屏會真的啟用。"
                    "請確定你已經用原廠範例確認過面板掃描與驅動 IC。")
    if not matrix_ip:
        warn.append("MATRIX_IP 是空的 → 沒有矩陣屏，系統照常運作。")

    method = as_int(v, "WIFI_EAP_METHOD")
    if method not in (1, 2):
        raise ConfigError("WIFI_EAP_METHOD 只能是 1 (PEAP) 或 2 (TTLS)。")
    if as_bool(v, "WIFI_ENTERPRISE"):
        if not as_str(v, "WIFI_EAP_USERNAME"):
            raise ConfigError("WIFI_ENTERPRISE = true 但 WIFI_EAP_USERNAME 是空的。")
        if not as_str(v, "WIFI_EAP_CA") and not as_bool(v, "ALLOW_INSECURE_EAP"):
            raise ConfigError(
                "WIFI_ENTERPRISE = true，但沒有 WIFI_EAP_CA 而且 "
                "ALLOW_INSECURE_EAP = false → 這台永遠連不上。\n"
                "  要嘛填學校的 CA，要嘛把 ALLOW_INSECURE_EAP 改成 true。")

    if as_int(v, "CONSOLE_SESSION_MS") <= 0:
        raise ConfigError("CONSOLE_SESSION_MS 必須大於 0。")
    if not audio_ssid:
        warn.append("AUDIO_WIFI_SSID 是空的 → 純熱點模式：語音機不連任何外部網路，"
                    "只開自己的熱點給門口機連。這是建議的預設值。")

    return {
        "system_id": system_id,
        "device_key": device_key,
        "ap_ssid": f"Staffroom-{system_id}-Audio",
        "ap_password": device_key[:12],
        "audio_ssid": audio_ssid,
        "audio_password": audio_pass,
        "matrix_ssid": matrix_ssid or audio_ssid or f"Staffroom-{system_id}-Audio",
        "matrix_password": matrix_pass or audio_pass or device_key[:12],
        "audio_ip": audio_ip,
        "matrix_ip": matrix_ip,
    }, warn


HEADER = ("#pragma once\n"
          "// ⚠ 這個檔案是 tools/apply_config.py 產生的，請勿手改。\n"
          "// 要改設定請改 firmware/staffroom_config.h，然後重跑 apply_config.py。\n"
          "// 產生時間：{ts}\n"
          "// 來源：SYSTEM_ID={sid}\n\n")


def emit_door(d):
    return HEADER.format(ts=d["ts"], sid=d["system_id"]) + f"""\
// 門口機只能連語音機的熱點，這兩個值是推導出來的，不需要使用者設定。
constexpr const char *WIFI_SSID = {cq(d['ap_ssid'])};
constexpr const char *WIFI_PASSWORD = {cq(d['ap_password'])};

constexpr const char *SYSTEM_ID = {cq(d['system_id'])};
constexpr const char *DEVICE_KEY = {cq(d['device_key'])};

// 門口機用這個位址找語音機。純熱點模式下固定是熱點閘道。
constexpr const char *AUDIO_HOST = {cq(d['audio_ip'])};

// 歡迎語功能會直接 POST /display 到這裡；留空時該功能會提示未設定。
constexpr const char *MATRIX_HOST = {cq(d['matrix_ip'])};
constexpr const char *BANNER_TEXT = {cq(d['banner_text'])};
constexpr const char *BANNER_PASSWORD = {cq(d['banner_password'])};
"""


def emit_audio(d):
    return HEADER.format(ts=d["ts"], sid=d["system_id"]) + f"""\
// 語音機平常要連的網路。空字串 = 純熱點模式。
constexpr const char *WIFI_SSID = {cq(d['audio_ssid'])};
constexpr const char *WIFI_PASSWORD = {cq(d['audio_password'])};

constexpr const char *SYSTEM_ID = {cq(d['system_id'])};
constexpr const char *DEVICE_KEY = {cq(d['device_key'])};

constexpr const char *AUDIO_HOST = {cq(d['audio_ip'])};
constexpr const char *MATRIX_HOST = {cq(d['matrix_ip'])};

// 硬體接線。AUDIO_PINS_CONFIRMED 為 false 時 SD 卡與喇叭都不會初始化。
constexpr bool AUDIO_PINS_CONFIRMED = {d['pins_confirmed']};
constexpr int PIN_SD_CS = {d['pin_sd_cs']};
constexpr int PIN_SD_SCK = {d['pin_sd_sck']};
constexpr int PIN_SD_MISO = {d['pin_sd_miso']};
constexpr int PIN_SD_MOSI = {d['pin_sd_mosi']};
constexpr int PIN_I2S_BCLK = {d['pin_i2s_bclk']};
constexpr int PIN_I2S_LRCK = {d['pin_i2s_lrck']};
constexpr int PIN_I2S_DATA = {d['pin_i2s_data']};
constexpr bool USE_CHIME = {d['use_chime']};

// 0=LIVE；1=只讀卡檢查；2=開機播一次且不連 Wi-Fi。
constexpr uint8_t AUDIO_TEST_STAGE = {d['test_stage']};

// 熱點行為。AUDIO_AP_ALWAYS=true 時熱點全程開著，這是刻意設計：
// 語音機連錯網路時，門口機仍然找得到它，也就永遠救得回來。
constexpr bool AUDIO_AP_ALWAYS = {d['ap_always']};
constexpr uint8_t AUDIO_AP_MAX_CLIENTS = {d['ap_max_clients']};
constexpr uint32_t AUTO_SETUP_DELAY_MS = {d['auto_setup_delay']};

// 網頁控制台。故意不用 DEVICE_KEY 當密碼。
constexpr const char *CONSOLE_PIN = {cq(d['console_pin'])};
constexpr uint32_t CONSOLE_SESSION_MS = {d['console_session']};
constexpr uint8_t CONSOLE_MAX_FAILS = {d['console_fails']};
constexpr uint32_t CONSOLE_LOCK_MS = {d['console_lock']};
"""


def emit_matrix(d):
    return HEADER.format(ts=d["ts"], sid=d["system_id"]) + f"""\
// 矩陣屏平常要連的網路。留空時會退回語音機的網路，再退回語音機的熱點。
constexpr const char *WIFI_SSID = {cq(d['matrix_ssid'])};
constexpr const char *WIFI_PASSWORD = {cq(d['matrix_password'])};

constexpr const char *SYSTEM_ID = {cq(d['system_id'])};
constexpr const char *DEVICE_KEY = {cq(d['device_key'])};

constexpr const char *AUDIO_HOST = {cq(d['audio_ip'])};
constexpr const char *MATRIX_HOST = {cq(d['matrix_ip'])};

// PANEL_CONFIRMED 為 false 時完全不初始化面板，也不會去搶 GPIO。
constexpr bool PANEL_CONFIRMED = {d['panel_confirmed']};
constexpr bool PANEL_FM6126A = {d['panel_fm6126a']};
constexpr bool PANEL_CLKPHASE = {d['panel_clkphase']};
constexpr uint8_t PANEL_BRIGHTNESS = {d['panel_brightness']};
"""


def emit_network_defaults(d):
    return HEADER.format(ts=d["ts"], sid=d["system_id"]) + f"""\
// 三端共用。NETWORK_ADMIN_PIN 是門口機網路設定頁的密碼。
constexpr bool WIFI_ENTERPRISE = {d['enterprise']};
constexpr uint32_t WIFI_EAP_METHOD = {d['eap_method']};
constexpr const char * WIFI_EAP_IDENTITY = {cq(d['eap_identity'])};
constexpr const char * WIFI_EAP_USERNAME = {cq(d['eap_username'])};
constexpr const char * WIFI_EAP_CA = {cq(d['eap_ca'])};
constexpr bool ALLOW_INSECURE_EAP = {d['allow_insecure']};
constexpr const char * NETWORK_ADMIN_PIN = {cq(d['admin_pin'])};
constexpr uint32_t NETWORK_SETUP_WINDOW_MS = {d['setup_window']};
"""


def derive(v):
    """把 staffroom_config.h 的值整理成產生檔案要用的完整字典。"""
    d, _ = validate(v)
    d.update({
        "ts": datetime.now().strftime("%Y-%m-%d %H:%M"),
        "banner_text": as_str(v, "BANNER_TEXT"),
        "banner_password": as_str(v, "BANNER_PASSWORD"),
        "pins_confirmed": str(as_bool(v, "AUDIO_PINS_CONFIRMED")).lower(),
        "use_chime": str(as_bool(v, "USE_CHIME")).lower(),
        "test_stage": as_int(v, "AUDIO_TEST_STAGE"),
        "ap_always": str(as_bool(v, "AUDIO_AP_ALWAYS")).lower(),
        "ap_max_clients": as_int(v, "AUDIO_AP_MAX_CLIENTS"),
        "auto_setup_delay": as_int(v, "AUTO_SETUP_DELAY_MS"),
        "console_pin": as_str(v, "CONSOLE_PIN"),
        "console_session": as_int(v, "CONSOLE_SESSION_MS"),
        "console_fails": as_int(v, "CONSOLE_MAX_FAILS"),
        "console_lock": as_int(v, "CONSOLE_LOCK_MS"),
        "panel_confirmed": str(as_bool(v, "PANEL_CONFIRMED")).lower(),
        "panel_fm6126a": str(as_bool(v, "PANEL_FM6126A")).lower(),
        "panel_clkphase": str(as_bool(v, "PANEL_CLKPHASE")).lower(),
        "panel_brightness": as_int(v, "PANEL_BRIGHTNESS"),
        "enterprise": str(as_bool(v, "WIFI_ENTERPRISE")).lower(),
        "eap_method": as_int(v, "WIFI_EAP_METHOD"),
        "eap_identity": as_str(v, "WIFI_EAP_IDENTITY"),
        "eap_username": as_str(v, "WIFI_EAP_USERNAME"),
        "eap_ca": as_str(v, "WIFI_EAP_CA"),
        "allow_insecure": str(as_bool(v, "ALLOW_INSECURE_EAP")).lower(),
        "admin_pin": as_str(v, "NETWORK_ADMIN_PIN"),
        "setup_window": as_int(v, "NETWORK_SETUP_WINDOW_MS"),
        "pin_sd_cs": as_int(v, "PIN_SD_CS"),
        "pin_sd_sck": as_int(v, "PIN_SD_SCK"),
        "pin_sd_miso": as_int(v, "PIN_SD_MISO"),
        "pin_sd_mosi": as_int(v, "PIN_SD_MOSI"),
        "pin_i2s_bclk": as_int(v, "PIN_I2S_BCLK"),
        "pin_i2s_lrck": as_int(v, "PIN_I2S_LRCK"),
        "pin_i2s_data": as_int(v, "PIN_I2S_DATA"),
    })
    return d


def build(root, d):
    """回傳 {路徑: 內容}。只算不寫。"""
    root = Path(root)
    defaults = emit_network_defaults(d)
    files = {
        root / "firmware" / "Door" / "config.h": emit_door(d),
        root / "firmware" / "Audio" / "config.h": emit_audio(d),
        root / "firmware" / "Matrix" / "config.h": emit_matrix(d),
    }
    for role in ROLES:
        files[root / "firmware" / role / "NetworkDefaults.h"] = defaults
    return files


def write_all(root, files):
    """先確認沒有任何 .bak 擋路，再全部寫入；任何一個出錯就整批回滾。"""
    root = Path(root)
    for path in files:
        bak = bak_path(path)
        if bak.exists():
            raise ConfigError(
                f"{path.relative_to(root)}.bak 已經存在。請先確認它是不是你要的版本，"
                "刪掉或改名後再執行。")
    done = []
    try:
        for path, text in files.items():
            old = path.read_bytes() if path.exists() else None
            if old is not None:
                bak_path(path).write_bytes(old)
            path.write_text(text, encoding="utf-8", newline="\n")
            done.append((path, old))
    except OSError:
        for path, old in reversed(done):
            if old is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(old)
        raise
    return done


def generate(root=ROOT):
    """讀 master → 檢查 → 產生 6 個檔案。回傳 (推導值, 警告, 檔案內容)。"""
    root = Path(root)
    v = parse_master(root)
    d = derive(v)
    _, warn = validate(v)
    files = build(root, d)
    write_all(root, files)
    return d, warn, files


def regenerate_key(root=ROOT):
    path = master_path(root)
    key = secrets.token_hex(32)
    text = path.read_text(encoding="utf-8")
    new_text, hits = KEY_LINE.subn(lambda m: m.group(1) + f'"{key}"' + m.group(3), text)
    if hits != 1:
        raise ConfigError("在 staffroom_config.h 找不到唯一的 DEVICE_KEY 那一行。")
    bak_path(path).write_bytes(path.read_bytes())
    path.write_text(new_text, encoding="utf-8", newline="\n")
    return key


def main():
    ap = argparse.ArgumentParser(description="從 staffroom_config.h 產生三端的 config.h。")
    ap.add_argument("--check", action="store_true", help="只檢查，不寫任何檔案")
    ap.add_argument("--show", action="store_true", help="產生後把內容印出來")
    ap.add_argument("--new-key", action="store_true", help="先換一組新的 DEVICE_KEY 再產生")
    a = ap.parse_args()

    if a.new_key:
        key = regenerate_key(ROOT)
        print(f"已換上新的 DEVICE_KEY（舊檔備份為 {master_path(ROOT).name}.bak）")
        print(f"  熱點密碼會跟著變成：{key[:12]}")

    v = parse_master(ROOT)
    d, warn = validate(v)
    d = derive(v)

    print("=" * 68)
    print(f"  這一套的身份      SYSTEM_ID = {d['system_id']}")
    print(f"  熱點名稱          {d['ap_ssid']}")
    print(f"  熱點密碼          {d['ap_password']}")
    print(f"  語音機要連的網路  {d['audio_ssid'] or '（無 → 純熱點模式）'}")
    print(f"  門口機找語音機用  {d['audio_ip']}")
    print(f"  矩陣屏            {d['matrix_ip'] or '（未安裝）'}")
    print("=" * 68)

    if warn:
        print(f"\n有 {len(warn)} 項提醒：")
        for i, w in enumerate(warn, 1):
            print(f"  {i}. {w}")

    if a.check:
        print("\n--check：只檢查，沒有寫入任何檔案。")
        return

    files = build(ROOT, d)
    write_all(ROOT, files)
    print("\n已產生 6 個檔案（舊的已備份成 .bak）：")
    for path in files:
        print(f"  {path.relative_to(ROOT)}")

    if a.show:
        for path, text in files.items():
            print(f"\n{'─' * 68}\n{path.relative_to(ROOT)}\n{'─' * 68}\n{text}")

    print("\n下一步：")
    print("  1. py -X utf8 tools/generate_fonts.py          # 名冊或歡迎語改了才需要")
    print("  2. 照 docs/燒錄與硬體.md 編譯並燒錄三塊板子")


if __name__ == "__main__":
    try:
        main()
    except (ConfigError, OSError) as e:
        print(f"\n停止：{e}", file=sys.stderr)
        sys.exit(1)

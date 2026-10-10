"""唯一設定檔 apply_config.py 的測試。

重點在「先檢查完才寫」與「出錯不留半套」：這支工具會一次改動 6 個檔案，
只要有一個沒檢查到就會編出四不像的固體。
"""
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import apply_config as ac  # noqa: E402

REAL_MASTER = ROOT / "firmware" / "staffroom_config.h"
GOOD_KEY = "0123456789abcdef" * 4


class ConfigBase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(ignore_cleanup_errors=True)
        self.root = Path(self.tmp.name)
        for role in ac.ROLES:
            (self.root / "firmware" / role).mkdir(parents=True)
        self.text = ac.master_path(ROOT).read_text(encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def write_master(self, *replacements):
        text = self.text
        for old, new in replacements:
            self.assertIn(old, text, f"測試用的母版找不到：{old}")
            text = text.replace(old, new)
        ac.master_path(self.root).write_text(text, encoding="utf-8")
        return text

    def with_key(self, *extra):
        return self.write_master(
            ('constexpr const char *DEVICE_KEY = "CHANGE_ME";',
             f'constexpr const char *DEVICE_KEY = "{GOOD_KEY}";'), *extra)


class Generation(ConfigBase):
    def test_generates_all_six_files(self):
        self.with_key()
        d, warn, files = ac.generate(self.root)
        self.assertEqual(len(files), 6)
        for path in files:
            self.assertTrue(path.exists(), path)
        for role in ac.ROLES:
            self.assertTrue((self.root / "firmware" / role / "config.h").exists())
            self.assertTrue((self.root / "firmware" / role / "NetworkDefaults.h").exists())

    def test_door_joined_to_the_audio_hotspot_not_the_school(self):
        self.with_key(('constexpr const char *AUDIO_WIFI_SSID = "";',
                       'constexpr const char *AUDIO_WIFI_SSID = "SCHOOL-2.4G";'))
        ac.generate(self.root)
        door = (self.root / "firmware/Door/config.h").read_text(encoding="utf-8")
        audio = (self.root / "firmware/Audio/config.h").read_text(encoding="utf-8")
        self.assertIn('WIFI_SSID = "Staffroom-ROOM_A-Audio"', door)
        self.assertIn(f'WIFI_PASSWORD = "{GOOD_KEY[:12]}"', door)
        self.assertIn('WIFI_SSID = "SCHOOL-2.4G"', audio)

    def test_matrix_falls_back_to_audio_then_hotspot(self):
        self.with_key()
        ac.generate(self.root)
        m = (self.root / "firmware/Matrix/config.h").read_text(encoding="utf-8")
        self.assertIn('WIFI_SSID = "Staffroom-ROOM_A-Audio"', m)
        self.assertIn(f'WIFI_PASSWORD = "{GOOD_KEY[:12]}"', m)

        self.with_key(('constexpr const char *AUDIO_WIFI_SSID = "";',
                       'constexpr const char *AUDIO_WIFI_SSID = "SCHOOL";'))
        ac.generate(self.root)
        m = (self.root / "firmware/Matrix/config.h").read_text(encoding="utf-8")
        self.assertIn('WIFI_SSID = "SCHOOL"', m)

    def test_network_defaults_identical_across_roles(self):
        self.with_key()
        ac.generate(self.root)
        bodies = {(self.root / "firmware" / r / "NetworkDefaults.h").read_text(encoding="utf-8")
                  for r in ac.ROLES}
        self.assertEqual(len(bodies), 1)

    def test_safety_locks_stay_false(self):
        self.with_key()
        ac.generate(self.root)
        audio = (self.root / "firmware/Audio/config.h").read_text(encoding="utf-8")
        matrix = (self.root / "firmware/Matrix/config.h").read_text(encoding="utf-8")
        self.assertIn("AUDIO_PINS_CONFIRMED = false", audio)
        self.assertIn("PANEL_CONFIRMED = false", matrix)

    def test_banner_characters_reach_the_font_generator(self):
        # generate_fonts.py 會掃 staffroom_config.h，否則歡迎語的字不會被烘進字庫。
        self.with_key()
        sys.path.insert(0, str(ROOT / "tools"))
        import generate_fonts as gf
        sources = {p.name for p in gf.collect_sources()}
        self.assertIn("staffroom_config.h", sources)


class Refusals(ConfigBase):
    def test_placeholder_key_rejected(self):
        self.write_master()
        with self.assertRaises(ac.ConfigError):
            ac.generate(self.root)

    def test_lowercase_system_id_rejected(self):
        self.with_key(('constexpr const char *SYSTEM_ID = "ROOM_A";',
                       'constexpr const char *SYSTEM_ID = "room_a";'))
        with self.assertRaises(ac.ConfigError) as cm:
            ac.generate(self.root)
        self.assertIn("小寫", str(cm.exception))

    def test_nothing_written_when_validation_fails(self):
        self.with_key(('constexpr const char *AUDIO_IP = "192.168.4.1";',
                       'constexpr const char *AUDIO_IP = "not-an-ip";'))
        with self.assertRaises(ac.ConfigError):
            ac.generate(self.root)
        self.assertFalse(list(self.root.rglob("config.h")))
        self.assertFalse(list(self.root.rglob("*.bak")))

    def test_confirmed_pins_must_be_real(self):
        self.with_key(("constexpr bool AUDIO_PINS_CONFIRMED = false;",
                       "constexpr bool AUDIO_PINS_CONFIRMED = true;"))
        with self.assertRaises(ac.ConfigError):
            ac.generate(self.root)

    def test_duplicate_gpio_rejected(self):
        self.with_key(("constexpr bool AUDIO_PINS_CONFIRMED = false;",
                       "constexpr bool AUDIO_PINS_CONFIRMED = true;"),
                      ("constexpr int PIN_SD_CS = -1;", "constexpr int PIN_SD_CS = 5;"),
                      ("constexpr int PIN_SD_SCK = -1;", "constexpr int PIN_SD_SCK = 12;"),
                      ("constexpr int PIN_SD_MISO = -1;", "constexpr int PIN_SD_MISO = 13;"),
                      ("constexpr int PIN_SD_MOSI = -1;", "constexpr int PIN_SD_MOSI = 11;"),
                      ("constexpr int PIN_I2S_BCLK = -1;", "constexpr int PIN_I2S_BCLK = 5;"),
                      ("constexpr int PIN_I2S_LRCK = -1;", "constexpr int PIN_I2S_LRCK = 6;"),
                      ("constexpr int PIN_I2S_DATA = -1;", "constexpr int PIN_I2S_DATA = 7;"))
        with self.assertRaises(ac.ConfigError) as cm:
            ac.generate(self.root)
        self.assertIn("同一個 GPIO", str(cm.exception))

    def test_forbidden_gpio_rejected(self):
        # GPIO 26~37 被 OPI PSRAM 佔用，填了會讓板子開不起來。
        self.with_key(("constexpr bool AUDIO_PINS_CONFIRMED = false;",
                       "constexpr bool AUDIO_PINS_CONFIRMED = true;"),
                      ("constexpr int PIN_SD_CS = -1;", "constexpr int PIN_SD_CS = 26;"))
        with self.assertRaises(ac.ConfigError):
            ac.generate(self.root)

    def test_identical_ips_rejected(self):
        self.with_key(('constexpr const char *MATRIX_IP = "";',
                       'constexpr const char *MATRIX_IP = "192.168.4.1";'))
        with self.assertRaises(ac.ConfigError):
            ac.generate(self.root)

    def test_enterprise_without_ca_or_override_rejected(self):
        self.with_key(("constexpr bool WIFI_ENTERPRISE = false;",
                       "constexpr bool WIFI_ENTERPRISE = true;"),
                      ('constexpr const char *WIFI_EAP_USERNAME = "";',
                       'constexpr const char *WIFI_EAP_USERNAME = "s1234567";'))
        with self.assertRaises(ac.ConfigError) as cm:
            ac.generate(self.root)
        self.assertIn("ALLOW_INSECURE_EAP", str(cm.exception))

    def test_enterprise_with_override_accepted(self):
        self.with_key(("constexpr bool WIFI_ENTERPRISE = false;",
                       "constexpr bool WIFI_ENTERPRISE = true;"),
                      ('constexpr const char *WIFI_EAP_USERNAME = "";',
                       'constexpr const char *WIFI_EAP_USERNAME = "s1234567";'),
                      ("constexpr bool ALLOW_INSECURE_EAP = false;",
                       "constexpr bool ALLOW_INSECURE_EAP = true;"))
        ac.generate(self.root)
        text = (self.root / "firmware/Audio/NetworkDefaults.h").read_text(encoding="utf-8")
        self.assertIn("ALLOW_INSECURE_EAP = true", text)

    def test_duplicate_setting_rejected(self):
        self.write_master()
        p = ac.master_path(self.root)
        p.write_text(p.read_text(encoding="utf-8") +
                     '\nconstexpr const char *SYSTEM_ID = "ROOM_A";\n', encoding="utf-8")
        with self.assertRaises(ac.ConfigError) as cm:
            ac.generate(self.root)
        self.assertIn("重複", str(cm.exception))


class WriteSafety(ConfigBase):
    def test_existing_backup_blocks_everything(self):
        self.with_key()
        bak = self.root / "firmware/Audio/config.h.bak"
        bak.write_bytes(b"keep me")
        with self.assertRaises(ac.ConfigError):
            ac.generate(self.root)
        self.assertEqual(bak.read_bytes(), b"keep me")
        self.assertFalse((self.root / "firmware/Door/config.h").exists())

    def test_previous_content_is_backed_up(self):
        self.with_key()
        first = ac.generate(self.root)[2]
        door = self.root / "firmware/Door/config.h"
        before = door.read_bytes()
        for p in first:
            ac.bak_path(p).unlink(missing_ok=True)
        second = ac.generate(self.root)[2]
        self.assertEqual(len(second), 6)
        self.assertTrue(ac.bak_path(door).exists())
        self.assertEqual(ac.bak_path(door).read_bytes(), before)

    def test_write_failure_rolls_back(self):
        self.with_key()
        v = ac.parse_master(self.root)
        files = ac.build(self.root, ac.derive(v))
        real = Path.write_text
        calls = []

        def faulty(self, *a, **k):
            calls.append(self)
            if len(calls) == 3:
                raise OSError("injected")
            return real(self, *a, **k)

        with patch.object(Path, "write_text", faulty):
            with self.assertRaises(OSError):
                ac.write_all(self.root, files)
        for p in files:
            self.assertFalse(p.exists(), p)
            self.assertFalse(ac.bak_path(p).exists(), p)

    def test_regenerate_key_rewrites_only_device_key(self):
        self.write_master()
        before = ac.master_path(self.root).read_text(encoding="utf-8")
        key = ac.regenerate_key(self.root)
        after = ac.master_path(self.root).read_text(encoding="utf-8")
        self.assertEqual(len(key), 64)
        self.assertIn(f'DEVICE_KEY = "{key}"', after)

        # 其他 CHANGE_ME 佔位（PIN 等）不可以被動到。
        def blank_device_key(text):
            return re.sub(r'(DEVICE_KEY = )"[^"]*"', r'\1"X"', text)

        self.assertEqual(blank_device_key(before), blank_device_key(after))
        self.assertEqual(
            ac.bak_path(ac.master_path(self.root)).read_text(encoding="utf-8"), before)


class ShippedMaster(ConfigBase):
    """直接檢查倉庫裡那一份 staffroom_config.h：欄位齊全、預設值安全。"""

    def test_every_field_the_generator_needs_exists(self):
        v = ac.parse_master(ROOT)
        needed = set(re.findall(r'as_(?:str|int|bool)\(v, "([A-Z_0-9]+)"\)',
                                (ROOT / "tools" / "apply_config.py").read_text(encoding="utf-8")))
        needed |= {n for n in ac.PIN_NAMES}
        missing = sorted(needed - set(v))
        self.assertEqual(missing, [], f"staffroom_config.h 少了：{missing}")

    def test_shipped_defaults_are_safe(self):
        v = ac.parse_master(ROOT)
        self.assertEqual(ac.as_str(v, "DEVICE_KEY"), "CHANGE_ME")
        self.assertEqual(ac.as_str(v, "AUDIO_WIFI_SSID"), "")
        self.assertEqual(ac.as_str(v, "MATRIX_IP"), "")
        self.assertFalse(ac.as_bool(v, "AUDIO_PINS_CONFIRMED"))
        self.assertFalse(ac.as_bool(v, "PANEL_CONFIRMED"))
        self.assertTrue(ac.as_bool(v, "AUDIO_AP_ALWAYS"))
        self.assertEqual(ac.as_int(v, "AUDIO_TEST_STAGE"), 0)

    def test_no_generated_config_is_committed(self):
        # config.h 是產生物，不進版控；進去了就代表有人手改過。
        for role in ac.ROLES:
            self.assertFalse((ROOT / "firmware" / role / "config.h").exists(),
                             f"firmware/{role}/config.h 不該被提交")
            self.assertFalse((ROOT / "firmware" / role / "NetworkDefaults.h").exists(),
                             f"firmware/{role}/NetworkDefaults.h 不該被提交")

    def test_gitignore_covers_generated_files(self):
        text = (ROOT / ".gitignore").read_text(encoding="utf-8")
        for role in ac.ROLES:
            self.assertIn(f"firmware/{role}/config.h", text)
            self.assertIn(f"firmware/{role}/NetworkDefaults.h", text)


if __name__ == "__main__":
    unittest.main(verbosity=2)

"""五位學號、E 別名、既有錄音路徑、以及 prepare_upgrade 的保留行為。"""
import csv
import io
import re
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import apply_config as ac  # noqa: E402
from validate_sd import validate, student_ui_id  # noqa: E402
from make_rosters import generate  # noqa: E402
from prepare_upgrade import prepare  # noqa: E402

GOOD_KEY = "0123456789abcdef" * 4


class NetworkRegression(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(ignore_cleanup_errors=True)
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def roster(self, ids):
        m = self.root / "m.csv"
        t = self.root / "t.csv"
        m.write_text("studentId,class,displayName,audioFile\n" +
                     "".join(i + ",2A,測試學生,/students/original.wav\n" for i in ids),
                     encoding="utf8")
        t.write_text("teacherId,displayName\n01,李老師\n", encoding="utf8")
        return m, t

    # ── 學號格式 ──────────────────────────────────────────────
    def test_e_alias_mapping(self):
        self.assertEqual(student_ui_id("E12345"), "12345")

    def test_zero_mapping(self):
        self.assertEqual(student_ui_id("00001"), "00001")

    def test_no_arbitrary_suffix_mapping(self):
        self.assertRaises(ValueError, student_ui_id, "A12345")

    def test_no_six_digit_truncation(self):
        self.assertRaises(ValueError, student_ui_id, "123456")

    def test_alias_collision_preflight(self):
        m, t = self.roster(["E12345", "12345"])
        self.assertRaises(ValueError, generate, m, t, self.root / "out")
        self.assertFalse((self.root / "out").exists())

    def test_existing_audio_path_unchanged(self):
        m, t = self.roster(["E12345"])
        out = generate(m, t, self.root / "out")
        self.assertIn("/students/original.wav",
                      (out / "AUDIO/students.csv").read_text(encoding="utf8"))

    def test_card_alias_collision(self):
        (self.root / "students.csv").write_text(
            "studentId,class,maskedName\nE12345,2A,測○生\n12345,2A,另○生\n", encoding="utf8")
        (self.root / "teachers.csv").write_text(
            "teacherId,displayName\n01,李老師\n", encoding="utf8")
        self.assertTrue(any("衝突" in e for e in validate(self.root, "door")["errors"]))

    # ── 升級 ──────────────────────────────────────────────────
    def fixture(self, enterprise=False):
        """造一個「已經設定好的既有安裝」，用來測 prepare_upgrade。"""
        base = self.root / "source"
        shutil.copytree(ROOT / "firmware", base / "firmware")
        master = ac.master_path(base)
        master.write_text(
            re.sub(r'(constexpr const char \*DEVICE_KEY = )"CHANGE_ME"',
                   r'\1"' + GOOD_KEY + '"', master.read_text(encoding="utf8")),
            encoding="utf8")
        ac.generate(base)
        for role in ac.ROLES:
            p = base / "firmware" / role / "NetworkDefaults.h"
            s = p.read_text(encoding="utf8")
            s = s.replace("WIFI_ENTERPRISE = false",
                          f"WIFI_ENTERPRISE = {str(enterprise).lower()}")
            s = s.replace('WIFI_EAP_USERNAME = ""', 'WIFI_EAP_USERNAME = "fixture"')
            p.write_text(s, encoding="utf8")
        return base

    def test_upgrade_no_ca_fails_before_write(self):
        base = self.fixture(True)
        out = self.root / "out"
        self.assertRaises(ValueError, prepare, base, out, "123456")
        self.assertFalse(out.exists())

    def test_upgrade_requires_pin(self):
        self.assertRaises(ValueError, prepare, self.root / "source", self.root / "out", "123")

    def test_upgrade_preserves_and_no_rosters(self):
        base = self.fixture()
        out = prepare(base, self.root / "out", "123456")
        self.assertEqual((base / "firmware/Audio/config.h").read_bytes(),
                         (out / "firmware/Audio/config.h").read_bytes())
        self.assertIn('NETWORK_ADMIN_PIN="123456"',
                      (out / "firmware/Door/NetworkDefaults.h").read_text(encoding="utf8"))
        self.assertFalse(list(out.rglob("*.wav")))
        self.assertFalse(list(out.rglob("*.csv")))

    def test_upgrade_copies_tools_and_font(self):
        base = self.fixture()
        out = prepare(base, self.root / "out", "123456")
        self.assertEqual((ROOT / "tools/generate_fonts.py").read_bytes(),
                         (out / "tools/generate_fonts.py").read_bytes())
        self.assertEqual((ROOT / "assets/NotoSansCJKsc-Regular.otf").stat().st_size,
                         (out / "assets/NotoSansCJKsc-Regular.otf").stat().st_size)
        self.assertTrue((out / "assets/FONT-LICENSE.txt").exists())

    def test_upgrade_insecure_explicit(self):
        base = self.fixture(True)
        out = prepare(base, self.root / "out", "123456", allow_insecure=True)
        self.assertIn("ALLOW_INSECURE_EAP=true",
                      (out / "firmware/Audio/NetworkDefaults.h").read_text(encoding="utf8"))

    def test_upgrade_refuses_existing_output(self):
        out = self.root / "out"
        out.mkdir()
        self.assertRaises(ValueError, prepare, self.root / "source", out, "123456")

    def test_admin_pin_must_be_digits(self):
        # prepare_upgrade 只收 6~12 位數字；門口機的網路設定頁則是英數字。
        for bad in ("12345", "1234567890123", "A123456"):
            self.assertRaises(ValueError, prepare, self.root, self.root / "o", bad)

    # ── IP 朗讀素材 ───────────────────────────────────────────
    def test_missing_ip_voice_is_optional(self):
        r = validate(self.root)
        v = validate(self.root, network_voice=True)
        self.assertFalse(any("network/" in e for e in r["errors"]))
        self.assertEqual(sum("network/" in e or "network\\" in e for e in v["errors"]), 12)


if __name__ == "__main__":
    unittest.main(verbosity=2)

"""首次兩張測試卡的內容檢查，以及門口機字庫是否涵蓋畫面用字。

（原本這裡還有一組測 make_config 的 ConfigTests，那個工具已被
 tools/apply_config.py 取代，改由 tests/test_apply_config.py 覆蓋。）
"""
import re
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from validate_sd import validate  # noqa: E402


class Cards(unittest.TestCase):
    def test_first_door(self):
        self.assertTrue(validate(ROOT / "sd_first_test/DOOR", "door")["ok"])

    def test_first_audio_honestly_requires_user_wavs(self):
        # 範本不附任何真實錄音，所以檢查一定要失敗，而且只因為缺 2 個 WAV。
        r = validate(ROOT / "sd_first_test/AUDIO")
        self.assertFalse(r["ok"])
        self.assertEqual(len(r["errors"]), 2)

    def test_door_rejects_extra_headers(self):
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as d:
            root = Path(d)
            (root / "students.csv").write_text(
                "studentId,class,maskedName,extra\n99999,TEST,masked,x\n", encoding="utf-8")
            (root / "teachers.csv").write_text(
                "teacherId,displayName\n01,Teacher\n", encoding="utf-8")
            self.assertFalse(validate(root, "door")["ok"])

    def test_new_ui_characters_present(self):
        # 畫面原始碼裡出現的每個非 ASCII 字元都必須在字庫裡，否則會顯示成空白。
        text = (ROOT / "firmware/Door/glyphs.h").read_text(encoding="utf-8")
        codes = {int(n) for n in re.findall(r"^\{(\d+),", text, re.M)}
        for source in ("UiView.h", "Door.ino"):
            words = "".join(re.findall(r'"([^"\n]*)"',
                                       (ROOT / "firmware/Door" / source).read_text(encoding="utf-8")))
            missing = {ord(c) for c in words if ord(c) > 127} - codes
            self.assertFalse(missing, f"{source} 有字不在字庫裡：{missing}")

    def test_banner_text_characters_present(self):
        # BANNER_TEXT 在 staffroom_config.h 裡，字庫產生器必須把它一起掃進去。
        import apply_config as ac
        banner = ac.as_str(ac.parse_master(ROOT), "BANNER_TEXT")
        text = (ROOT / "firmware/Door/glyphs.h").read_text(encoding="utf-8")
        codes = {int(n) for n in re.findall(r"^\{(\d+),", text, re.M)}
        missing = {c for c in banner if ord(c) not in codes}
        self.assertFalse(missing, f"歡迎語「{banner}」有字不在字庫裡：{missing}")


if __name__ == "__main__":
    unittest.main(verbosity=2)

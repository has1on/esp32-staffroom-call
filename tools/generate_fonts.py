"""為門口機與矩陣屏產生中文點陣字庫子集。需要 Pillow。

    py -X utf8 tools/generate_fonts.py
    py -X utf8 tools/generate_fonts.py --students 我的名冊.csv --teachers 我的老師.csv

會把名冊 CSV 裡出現的每個字、firmware 原始碼裡出現的每個字串常值、
以及 firmware/staffroom_config.h（含 BANNER_TEXT）全部烘成
firmware/Door/glyphs.h 與 firmware/Matrix/glyphs.h。

★ 只要改了名冊、改了班級、改了歡迎語，就一定要重跑這支程式並重燒，
  否則畫面上那些新字會變成空白（不是問號，是直接空掉）。
"""
import argparse
import csv
import json
import re
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("缺少 Pillow。請先安裝：\n    py -m pip install pillow", file=sys.stderr)
    raise SystemExit(1)

ROOT = Path(__file__).resolve().parents[1]

# 掃描時要跳過的檔名：glyphs.h 是自己上一次的產物，config.h 是工具產生的。
SKIP_NAMES = ("glyphs.h", "config.h")


def chars_from_csv(path):
    with Path(path).open(encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))
    return "".join(v for r in rows for v in r.values() if v)


def collect_sources():
    """回傳所有要掃字串常值的檔案。"""
    sources = []
    for role in ("Door", "Matrix"):
        for p in sorted((ROOT / "firmware" / role).glob("*")):
            if p.suffix in (".ino", ".h", ".cpp") and p.name not in SKIP_NAMES:
                sources.append(p)
    # 唯一設定檔不在角色資料夾裡，要特別加進來，否則 BANNER_TEXT 的字不會被烘進去。
    master = ROOT / "firmware" / "staffroom_config.h"
    if master.exists():
        sources.append(master)
    return sources


def render_glyph(font_path, size, character):
    # 依實際墨跡邊界置中，不要用字體的 ascent：CJK 字形的基線偏移很大，
    # 固定 -size//7 的位移會把漢字下方的筆畫切掉。
    font = ImageFont.truetype(str(font_path), size - 2)
    left, top, right, bottom = font.getbbox(character)
    if right - left > size or bottom - top > size:
        font = ImageFont.truetype(str(font_path), size - 3)
        left, top, right, bottom = font.getbbox(character)
    advance = min(size, max(1, round(font.getlength(character)), right - left))
    im = Image.new("L", (size, size), 0)
    ImageDraw.Draw(im).text(
        ((advance - (right - left)) // 2 - left, (size - (bottom - top)) // 2 - top),
        character, font=font, fill=255)
    return im, advance


def generate(font_path, students, teachers, dest=ROOT / "firmware"):
    # '○' 是名冊遮名用的符號，固定保留。
    words = "".join(chr(i) for i in range(32, 127)) + "○"
    words += chars_from_csv(students) + chars_from_csv(teachers)
    for source in collect_sources():
        words += "".join(re.findall(r'"([^"\n]*)"', source.read_text(encoding="utf-8")))
    cps = sorted({ord(c) for c in words if c not in "\n\r\t"})
    summary = {}
    for role, size in (("Door", 28), ("Matrix", 16)):
        glyphs = []
        for cp in cps:
            im, advance = render_glyph(font_path, size, chr(cp))
            raw = []
            for offset in range(0, size * size, 8):
                byte = 0
                for bit in range(8):
                    index = offset + bit
                    if index < size * size and im.getpixel((index % size, index // size)) >= 96:
                        byte |= 0x80 >> bit
                raw.append(byte)
            glyphs.append("{" + str(cp) + "," + str(advance) + ",{" + ",".join(str(b) for b in raw) + "}}")
        out = Path(dest) / role / "glyphs.h"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(
            "// Generated bitmap subset. Font: Noto Sans CJK / SIL OFL.\n"
            "// Regenerate with tools/generate_fonts.py after any roster or banner change.\n"
            "#pragma once\n#include <stdint.h>\n"
            f"constexpr int GLYPH_SIZE={size};\n"
            f"struct Glyph{{uint32_t code;uint8_t advance;uint8_t bits[{(size * size + 7) // 8}];}};\n"
            "static const Glyph GLYPHS[]={\n" + ",\n".join(glyphs) + "\n};\n"
            "constexpr int GLYPH_COUNT=sizeof(GLYPHS)/sizeof(GLYPHS[0]);\n",
            encoding="utf-8")
        summary[role] = {"glyphs": len(cps), "size": size, "sourceBytes": out.stat().st_size}
    return summary


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--students", default=str(ROOT / "sd_templates" / "AUDIO" / "students.csv"))
    p.add_argument("--teachers", default=str(ROOT / "sd_templates" / "AUDIO" / "teachers.csv"))
    p.add_argument("--font", default=str(ROOT / "assets" / "NotoSansCJKsc-Regular.otf"))
    a = p.parse_args()
    for label, path in (("students", a.students), ("teachers", a.teachers), ("font", a.font)):
        if not Path(path).exists():
            print(f"找不到 {label}：{path}", file=sys.stderr)
            raise SystemExit(1)
    result = generate(a.font, a.students, a.teachers)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    print("\n記得重新編譯並燒錄門口機（字庫是編進固體裡的，不是放 SD 卡）。")

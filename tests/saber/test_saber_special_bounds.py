#!/usr/bin/env python3
"""Keep special-move hitbox tables tied to the shipped FX art boxes."""

from __future__ import annotations

import json
from pathlib import Path
import re
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src" / "saber" / "mmx_saber_attack.c"
BOXES = ROOT / "tools" / "saber" / "saber_specials_boxes.json"
SPRITES = ROOT / "assets" / "saber-zero" / "sprites"

TABLES = {
    "Uppercut": "kUppercutBounds",
    "RollingSlash": "kRollingSlashBounds",
}

ENTRY = re.compile(
    r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*"
    r"(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}"
)


def fx_box(path: Path, cell: int, cell_index: int) -> tuple[int, int, int, int]:
    """Return the nontransparent FX box in player-canvas coordinates."""

    with Image.open(path) as opened:
        image = opened.convert("RGBA")
        frame = image.crop(
            (cell_index * cell, 0, (cell_index + 1) * cell, cell)
        )
        box = frame.getchannel("A").getbbox()
    if box is None:
        raise AssertionError(f"{path.name} frame {cell_index} has no FX pixels")
    x0, y0, x1, y1 = box
    # The renderer places every extracted cell at player-canvas (+1, +3).
    return x0 + 1, y0 + 3, x1 + 1, y1 + 3


def transformed(box: tuple[int, int, int, int], first: int, last: int) -> tuple[int, ...]:
    x0, y0, x1, y1 = box
    width = x1 - x0
    height = y1 - y0
    return (
        first,
        last,
        (x0 + x1) // 2 - 64,
        (y0 + y1) // 2 - 72,
        (width + 1) // 2,
        (height + 1) // 2,
    )


def c_table(source: str, name: str) -> list[tuple[int, ...]]:
    match = re.search(
        rf"static const MmxSaberBoundsSegment {name}\[\]\s*=\s*\{{(.*?)\}};",
        source,
        re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"C table {name} is missing")
    return [tuple(int(value) for value in entry) for entry in ENTRY.findall(match.group(1))]


class SaberSpecialBoundsTests(unittest.TestCase):
    def test_fx_boxes_match_c_tables(self) -> None:
        config = json.loads(BOXES.read_text(encoding="utf-8"))
        source = SOURCE.read_text(encoding="utf-8")
        cell = int(config["cell"])

        for move, table_name in TABLES.items():
            with self.subTest(move=move):
                fx_path = SPRITES / f"Special_{move}_FX.png"
                fx_count = len(config["moves"][move]["fx"])
                actual = c_table(source, table_name)
                expected = []
                for art_index in range(fx_count):
                    # RollingSlash_FX omits Zero step 0; its first cell is step 1.
                    step = art_index if move == "Uppercut" else art_index + 1
                    box = fx_box(fx_path, cell, art_index)
                    last = 255 if move == "Uppercut" and step == 6 else step * 2 + 1
                    expected.append(transformed(box, step * 2, last))

                self.assertEqual(
                    actual,
                    expected,
                    f"{table_name} no longer matches {fx_path.relative_to(ROOT)}",
                )


if __name__ == "__main__":
    unittest.main()

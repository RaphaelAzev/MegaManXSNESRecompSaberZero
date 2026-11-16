#!/usr/bin/env python3
"""Pin deterministic Storm Eagle tornado strip generation."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SHEET = Path(r"D:\Games\Mega Man\SaberZero\StormEagleSpecial.png")
OUTPUT = ROOT / "assets" / "saber-zero" / "sprites" / "Special_Tornado.png"
SCRIPT = ROOT / "tools" / "saber" / "build_storm_tornado.py"
KEY = (0, 73, 70)
CELL_WIDTH = 28
CELL_HEIGHT = 120
PIECE_INDICES = (9, 10, 14)

sys.path.insert(0, str(ROOT))
from tools.saber.build_storm_tornado import segments  # noqa: E402


@unittest.skipUnless(SHEET.is_file(), "Storm Eagle operator sheet is absent")
class StormTornadoTests(unittest.TestCase):
    def test_rebuild_is_byte_identical_and_preserves_piece_colours(self) -> None:
        expected = OUTPUT.read_bytes()
        with tempfile.TemporaryDirectory(dir=ROOT / ".foreman") as temp_dir:
            rebuilt = Path(temp_dir) / OUTPUT.name
            result = subprocess.run(
                [
                    sys.executable,
                    "-I",
                    str(SCRIPT),
                    "--input",
                    str(SHEET),
                    "--output",
                    str(rebuilt),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, msg=result.stdout + result.stderr)
            self.assertEqual(rebuilt.read_bytes(), expected)

        boxes = segments(SHEET)
        self.assertEqual(len(boxes), 15)
        with Image.open(SHEET) as opened:
            source = opened.convert("RGB")
        piece_colours = {}
        for index in PIECE_INDICES:
            x0, y0, x1, y1, _ = boxes[index]
            piece_colours[index] = {
                source.getpixel((x, y))
                for y in range(y0, y1)
                for x in range(x0, x1)
                if source.getpixel((x, y)) != KEY
            }

        with Image.open(OUTPUT) as opened:
            opened.load()
            image = opened.convert("RGBA")
        self.assertEqual(image.mode, "RGBA")
        self.assertEqual(image.size, (CELL_WIDTH * len(PIECE_INDICES), CELL_HEIGHT))

        for frame_index, piece_index in enumerate(PIECE_INDICES):
            cell = image.crop(
                (
                    frame_index * CELL_WIDTH,
                    0,
                    (frame_index + 1) * CELL_WIDTH,
                    CELL_HEIGHT,
                )
            )
            pixels = list(cell.getdata())
            opaque = {pixel[:3] for pixel in pixels if pixel[3]}
            self.assertTrue(opaque)
            self.assertTrue(opaque <= piece_colours[piece_index])
            self.assertTrue(all(pixel[3] in (0, 255) for pixel in pixels))
            self.assertTrue(all(pixel[:3] != KEY for pixel in pixels if pixel[3]))
            opaque_rows = [
                y
                for y in range(CELL_HEIGHT)
                if any(cell.getpixel((x, y))[3] for x in range(CELL_WIDTH))
            ]
            self.assertEqual(max(opaque_rows), CELL_HEIGHT - 1)


if __name__ == "__main__":
    unittest.main()

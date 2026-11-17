#!/usr/bin/env python3
"""Pin deterministic Chill Penguin ice clone strip generation."""

from __future__ import annotations

from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SHEET = Path(r"D:\Games\Mega Man\SaberZero\ChillPenguinSpecialProjectile1.png")
OUTPUT = ROOT / "assets" / "saber-zero" / "sprites" / "Special_IceClone.png"
SCRIPT = ROOT / "tools" / "saber" / "build_chill_clone.py"
KEY = (0, 73, 70)
EXPECTED_BOXES = (
    (4, 15, 83, 51),
    (96, 13, 189, 50),
    (196, 12, 282, 49),
)
CELL = (94, 38)
EXPECTED_SHEET_SHA256 = "6a394dcd9747fa2594e71ef54b56464e7fbd830d781f8c487517e7e84a07760e"

sys.path.insert(0, str(ROOT))
from tools.saber.build_chill_clone import segments  # noqa: E402


@unittest.skipUnless(SHEET.is_file(), "Chill Penguin operator sheet is absent")
class ChillCloneTests(unittest.TestCase):
    def test_rebuild_is_byte_identical_and_preserves_order_alignment_and_colours(self) -> None:
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

        self.assertEqual(
            hashlib.sha256(SHEET.read_bytes()).hexdigest(), EXPECTED_SHEET_SHA256
        )
        boxes = segments(SHEET)
        self.assertEqual([box[:4] for box in boxes], list(EXPECTED_BOXES))

        with Image.open(SHEET) as opened:
            source = opened.convert("RGB")
        frame_colours = []
        for x0, y0, x1, y1 in EXPECTED_BOXES:
            frame_colours.append(
                {
                    source.getpixel((x, y))
                    for y in range(y0, y1)
                    for x in range(x0, x1)
                    if source.getpixel((x, y)) != KEY
                }
            )

        with Image.open(OUTPUT) as opened:
            opened.load()
            image = opened.convert("RGBA")
        self.assertEqual(image.mode, "RGBA")
        self.assertEqual(image.size, (CELL[0] * 3, CELL[1]))

        frame_sizes = [(x1 - x0, y1 - y0) for x0, y0, x1, y1 in EXPECTED_BOXES]
        for frame_index, (frame_width, frame_height) in enumerate(frame_sizes):
            cell = image.crop(
                (
                    frame_index * CELL[0],
                    0,
                    (frame_index + 1) * CELL[0],
                    CELL[1],
                )
            )
            self._assert_cell(
                cell,
                frame_colours[frame_index],
                (CELL[0] - frame_width, CELL[1] - frame_height,
                 CELL[0], CELL[1]),
            )

    def _assert_cell(
        self,
        cell: Image.Image,
        source_colours: set[tuple[int, int, int]],
        expected_alpha_box: tuple[int, int, int, int],
    ) -> None:
        pixels = list(cell.getdata())
        opaque = {pixel[:3] for pixel in pixels if pixel[3]}
        self.assertEqual(opaque, source_colours)
        self.assertTrue(all(pixel[3] in (0, 255) for pixel in pixels))
        self.assertTrue(all(pixel[:3] != KEY for pixel in pixels if pixel[3]))
        self.assertEqual(cell.getchannel("A").getbbox(), expected_alpha_box)


if __name__ == "__main__":
    unittest.main()

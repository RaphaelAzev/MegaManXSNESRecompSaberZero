#!/usr/bin/env python3
"""Pin deterministic Boomer Kuwanger cutter strip generation."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SHEET = Path(r"D:\Games\Mega Man\SaberZero\BoomerKuwangerSpecial.png")
OUTPUT = ROOT / "assets" / "saber-zero" / "sprites" / "Special_Cutter.png"
SCRIPT = ROOT / "tools" / "saber" / "build_boomer_cutter.py"
KEY = (0, 73, 70)
EXPECTED_PIECE_COUNT = 29
FORWARD_FRAME_COUNT = 4
DIAGONAL_FRAME_COUNT = 4
FORWARD_CELL = (32, 12)
DIAGONAL_CELL = (26, 24)
FORWARD_PIECE_INDEX = 27
DIAGONAL_PIECE_INDICES = (24, 25, 26, 28)
EXPECTED_BOXES = {
    24: (154, 223, 179, 247),
    25: (185, 224, 207, 248),
    26: (213, 223, 238, 247),
    27: (13, 225, 142, 237),
    28: (246, 226, 268, 250),
}

sys.path.insert(0, str(ROOT))
from tools.saber.build_boomer_cutter import keyed, segments, split_columns  # noqa: E402


@unittest.skipUnless(SHEET.is_file(), "Boomer Kuwanger operator sheet is absent")
class BoomerCutterTests(unittest.TestCase):
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
        self.assertEqual(len(boxes), EXPECTED_PIECE_COUNT)
        for index, box in EXPECTED_BOXES.items():
            self.assertEqual(boxes[index][:4], box)

        with Image.open(SHEET) as opened:
            source = opened.convert("RGB")
        piece_colours = {}
        for index in (FORWARD_PIECE_INDEX,) + DIAGONAL_PIECE_INDICES:
            x0, y0, x1, y1, _ = boxes[index]
            piece_colours[index] = {
                source.getpixel((x, y))
                for y in range(y0, y1)
                for x in range(x0, x1)
                if source.getpixel((x, y)) != KEY
            }

        forward_source = keyed(
            source.crop(boxes[FORWARD_PIECE_INDEX][:4])
        )
        self.assertEqual(len(split_columns(forward_source)), FORWARD_FRAME_COUNT)
        self.assertEqual(len(DIAGONAL_PIECE_INDICES), DIAGONAL_FRAME_COUNT)

        with Image.open(OUTPUT) as opened:
            opened.load()
            image = opened.convert("RGBA")
        self.assertEqual(image.mode, "RGBA")
        self.assertEqual(
            image.size,
            (FORWARD_CELL[0] * FORWARD_FRAME_COUNT,
             FORWARD_CELL[1] + DIAGONAL_CELL[1]),
        )

        for frame_index in range(FORWARD_FRAME_COUNT):
            cell = image.crop(
                (
                    frame_index * FORWARD_CELL[0],
                    0,
                    (frame_index + 1) * FORWARD_CELL[0],
                    FORWARD_CELL[1],
                )
            )
            self._assert_cell_colours(cell, piece_colours[FORWARD_PIECE_INDEX])

        for frame_index, piece_index in enumerate(DIAGONAL_PIECE_INDICES):
            cell = image.crop(
                (
                    frame_index * DIAGONAL_CELL[0],
                    FORWARD_CELL[1],
                    (frame_index + 1) * DIAGONAL_CELL[0],
                    FORWARD_CELL[1] + DIAGONAL_CELL[1],
                )
            )
            self._assert_cell_colours(cell, piece_colours[piece_index])

    def _assert_cell_colours(
        self,
        cell: Image.Image,
        source_colours: set[tuple[int, int, int]],
    ) -> None:
        pixels = list(cell.getdata())
        opaque = {pixel[:3] for pixel in pixels if pixel[3]}
        self.assertTrue(opaque)
        self.assertTrue(opaque <= source_colours)
        self.assertTrue(all(pixel[3] in (0, 255) for pixel in pixels))
        self.assertTrue(all(pixel[:3] != KEY for pixel in pixels if pixel[3]))


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Pin deterministic Enkoujin thrust strip generation."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SHEET = Path(r"D:\Games\Mega Man\SaberZero\Enkoujin.png")
OUTPUT = ROOT / "assets" / "saber-zero" / "sprites" / "Special_Enkoujin.png"
SCRIPT = ROOT / "tools" / "saber" / "build_enkoujin_thrust.py"
CELL_SIZE = 128
INPUT_CELL_COUNT = 12
OUTPUT_CELL_COUNT = 6


@unittest.skipUnless(SHEET.is_file(), "Enkoujin operator sheet is absent")
class EnkoujinThrustTests(unittest.TestCase):
    def test_rebuild_is_byte_identical_and_copies_six_cells(self) -> None:
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

        with Image.open(SHEET) as opened:
            opened.load()
            source = opened.copy()
        self.assertEqual(source.mode, "RGBA")
        self.assertEqual(source.size, (CELL_SIZE * INPUT_CELL_COUNT, CELL_SIZE))

        with Image.open(OUTPUT) as opened:
            opened.load()
            output = opened.copy()
        self.assertEqual(output.mode, "RGBA")
        self.assertEqual(output.size, (CELL_SIZE * OUTPUT_CELL_COUNT, CELL_SIZE))

        for cell_index in range(OUTPUT_CELL_COUNT):
            source_cell = source.crop(
                (
                    cell_index * CELL_SIZE,
                    0,
                    (cell_index + 1) * CELL_SIZE,
                    CELL_SIZE,
                )
            )
            output_cell = output.crop(
                (
                    cell_index * CELL_SIZE,
                    0,
                    (cell_index + 1) * CELL_SIZE,
                    CELL_SIZE,
                )
            )
            self.assertEqual(output_cell.tobytes(), source_cell.tobytes())


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Pin deterministic Storm uppercut FX generation and manifest geometry."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.saber.build_storm_uppercut_fx import PINK  # noqa: E402


SCRIPT = ROOT / "tools" / "saber" / "build_storm_uppercut_fx.py"
OUTPUT = ROOT / "assets" / "saber-zero" / "sprites" / "Special_Uppercut_FX_Storm.png"
MANIFEST = ROOT / "tools" / "saber" / "saber_zero_manifest.json"


class StormUppercutFxTests(unittest.TestCase):
    def test_rebuild_is_byte_identical_and_only_uses_pink_ramp(self) -> None:
        expected = OUTPUT.read_bytes()
        with tempfile.TemporaryDirectory(dir=ROOT / ".foreman") as temp_dir:
            rebuilt = Path(temp_dir) / OUTPUT.name
            result = subprocess.run(
                [sys.executable, "-I", str(SCRIPT), "--output", str(rebuilt)],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                result.returncode,
                0,
                msg=result.stdout + result.stderr,
            )
            self.assertEqual(rebuilt.read_bytes(), expected)

        with Image.open(OUTPUT) as decoded:
            decoded.load()
            self.assertEqual(decoded.mode, "RGBA")
            self.assertEqual(decoded.size, (896, 128))
            pixels = list(decoded.getdata())

        opaque = {pixel for pixel in pixels if pixel[3]}
        self.assertTrue(opaque)
        self.assertTrue(opaque <= {color + (255,) for color in PINK})
        self.assertTrue(all(pixel[3] in (0, 255) for pixel in pixels))

        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        frames = {frame["id"]: frame for frame in manifest["frames"]}
        with Image.open(OUTPUT) as decoded:
            for step in range(7):
                cell = decoded.crop((step * 128, 0, (step + 1) * 128, 128))
                bbox = cell.getbbox()
                self.assertIsNotNone(bbox)
                if bbox is None:
                    continue
                frame = frames[1099 + step]
                self.assertEqual(
                    frame["body"]["rect"],
                    [
                        step * 128 + bbox[0],
                        bbox[1],
                        bbox[2] - bbox[0],
                        bbox[3] - bbox[1],
                    ],
                )
                self.assertEqual(
                    frame["body"]["origin"], [bbox[0] + 1, bbox[1] + 3]
                )


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Pin deterministic reduced thunder slam and orb strip generation."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SHEET = Path(r"D:\Games\Mega Man\SaberZero\SparkMandrillSpecial.png")
SLAM_OUTPUT = ROOT / "assets" / "saber-zero" / "sprites" / "Special_ThunderSlam.png"
ORB_OUTPUT = ROOT / "assets" / "saber-zero" / "sprites" / "Special_ThunderOrb.png"
SCRIPT = ROOT / "tools" / "saber" / "build_spark_slam.py"
SLAM_CELL = (128, 128)
ORB_CELL = (32, 32)
SLAM_FRAME_COUNT = 14
ORB_FRAME_COUNT = 2

sys.path.insert(0, str(ROOT))
from tools.saber.build_spark_slam import PALETTE_CAP  # noqa: E402


def opaque_colors(image: Image.Image) -> set[tuple[int, int, int]]:
    return {pixel[:3] for pixel in image.getdata() if pixel[3]}


@unittest.skipUnless(SHEET.is_file(), "Spark Mandrill operator sheet is absent")
class SparkSlamTests(unittest.TestCase):
    def test_rebuild_is_byte_identical_and_uses_14_body_and_2_orb_frames(self) -> None:
        expected_slam = SLAM_OUTPUT.read_bytes()
        expected_orb = ORB_OUTPUT.read_bytes()
        with tempfile.TemporaryDirectory(dir=ROOT / ".foreman") as temp_dir:
            temporary = Path(temp_dir)
            rebuilt_slam = temporary / SLAM_OUTPUT.name
            rebuilt_orb = temporary / ORB_OUTPUT.name
            result = subprocess.run(
                [
                    sys.executable,
                    "-I",
                    str(SCRIPT),
                    "--input",
                    str(SHEET),
                    "--slam-output",
                    str(rebuilt_slam),
                    "--orb-output",
                    str(rebuilt_orb),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, msg=result.stdout + result.stderr)
            self.assertEqual(rebuilt_slam.read_bytes(), expected_slam)
            self.assertEqual(rebuilt_orb.read_bytes(), expected_orb)

        with Image.open(SLAM_OUTPUT) as opened:
            opened.load()
            slam = opened.convert("RGBA")
        with Image.open(ORB_OUTPUT) as opened:
            opened.load()
            orb = opened.convert("RGBA")

        self.assertEqual(slam.size, (SLAM_CELL[0] * SLAM_FRAME_COUNT, SLAM_CELL[1]))
        self.assertEqual(orb.size, (ORB_CELL[0] * ORB_FRAME_COUNT, ORB_CELL[1]))
        for image in (slam, orb):
            self.assertTrue(all(pixel[3] in (0, 255) for pixel in image.getdata()))

        for frame_index in range(SLAM_FRAME_COUNT):
            cell = slam.crop(
                (
                    frame_index * SLAM_CELL[0],
                    0,
                    (frame_index + 1) * SLAM_CELL[0],
                    SLAM_CELL[1],
                )
            )
            self.assertTrue(opaque_colors(cell), frame_index)

        for frame_index in range(ORB_FRAME_COUNT):
            cell = orb.crop(
                (
                    frame_index * ORB_CELL[0],
                    0,
                    (frame_index + 1) * ORB_CELL[0],
                    ORB_CELL[1],
                )
            )
            self.assertTrue(opaque_colors(cell), frame_index)

        joint = opaque_colors(slam) | opaque_colors(orb)
        self.assertLessEqual(len(joint), PALETTE_CAP)


if __name__ == "__main__":
    unittest.main()

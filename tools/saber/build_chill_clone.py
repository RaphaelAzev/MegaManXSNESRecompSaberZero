#!/usr/bin/env python3
"""Build Chill Penguin's ice clone projectile strip from its operator sheet."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import tempfile

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT = r"D:\Games\Mega Man\SaberZero\ChillPenguinSpecialProjectile1.png"
DEFAULT_OUTPUT = "assets/saber-zero/sprites/Special_IceClone.png"
KEY = (0, 73, 70)
GAP = 2
EXPECTED_SIZE = (288, 59)
EXPECTED_SHEET_SHA256 = "6a394dcd9747fa2594e71ef54b56464e7fbd830d781f8c487517e7e84a07760e"
EXPECTED_BOXES = (
    (4, 15, 83, 51),
    (96, 13, 189, 50),
    (196, 12, 282, 49),
)
CELL_WIDTH = 94
CELL_HEIGHT = 38
BOARD_X = 26
BOARD_ZW = 34
BOARD_FRONT_OFFSET = 14
BOARD_FLOOR = 160
BOARD_PLACEMENTS = ((0, 1.3), (1, 2.2), (2, 3.0), (2, 3.6), (2, 3.9), (2, 4.1))


def _repo_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def segments(path: Path, gap: int = GAP) -> list[tuple[int, int, int, int, int]]:
    """Copy the operator sheet segmenter used by ``.foreman/mock/seg.py``."""
    with Image.open(path) as decoded:
        image = decoded.convert("RGB")
    width, height = image.size
    pixels = image.load()
    foreground = [
        [pixels[x, y] != KEY for x in range(width)] for y in range(height)
    ]
    seen = [[False] * width for _ in range(height)]
    boxes: list[tuple[int, int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            if foreground[y][x] and not seen[y][x]:
                stack = [(x, y)]
                seen[y][x] = True
                x0 = x1 = x
                y0 = y1 = y
                count = 0
                while stack:
                    current_x, current_y = stack.pop()
                    count += 1
                    x0 = min(x0, current_x)
                    x1 = max(x1, current_x)
                    y0 = min(y0, current_y)
                    y1 = max(y1, current_y)
                    for dx in range(-gap, gap + 1):
                        for dy in range(-gap, gap + 1):
                            next_x, next_y = current_x + dx, current_y + dy
                            if (
                                0 <= next_x < width
                                and 0 <= next_y < height
                                and foreground[next_y][next_x]
                                and not seen[next_y][next_x]
                            ):
                                seen[next_y][next_x] = True
                                stack.append((next_x, next_y))
                if count > 8:
                    boxes.append((x0, y0, x1 + 1, y1 + 1, count))
    boxes.sort(key=lambda box: (box[1] // 25, box[0]))
    return boxes


def keyed(image: Image.Image) -> Image.Image:
    result = Image.new("RGBA", image.size, (0, 0, 0, 0))
    source = image.convert("RGB").load()
    destination = result.load()
    for y in range(image.height):
        for x in range(image.width):
            pixel = source[x, y]
            if pixel != KEY:
                destination[x, y] = pixel + (255,)
    return result


def _load_frames(input_path: Path) -> tuple[list[tuple[int, int, int, int, int]], list[Image.Image]]:
    raw_sha256 = hashlib.sha256(input_path.read_bytes()).hexdigest()
    assert raw_sha256 == EXPECTED_SHEET_SHA256, (
        f"Chill Penguin sheet SHA-256 changed: {raw_sha256}"
    )
    with Image.open(input_path) as decoded:
        assert decoded.size == EXPECTED_SIZE, (
            f"Chill Penguin sheet size changed: {decoded.size}"
        )
        image = decoded.convert("RGB")
    boxes = segments(input_path)
    actual_boxes = [box[:4] for box in boxes]
    assert actual_boxes == list(EXPECTED_BOXES), (
        f"Chill Penguin sheet segmentation changed: {actual_boxes}"
    )
    frames = [keyed(image.crop(box)) for box in EXPECTED_BOXES]
    assert len(frames) == 3
    return boxes, frames


def _place_bottom_right(
    output: Image.Image,
    frame: Image.Image,
    cell_x: int,
) -> tuple[int, int]:
    x = cell_x + CELL_WIDTH - frame.width
    y = CELL_HEIGHT - frame.height
    output.alpha_composite(frame, (x, y))
    return x, y


def build(input_path: Path, output_path: Path) -> list[dict[str, object]]:
    boxes, frames = _load_frames(input_path)
    output = Image.new("RGBA", (CELL_WIDTH * len(frames), CELL_HEIGHT), (0, 0, 0, 0))
    geometry: list[dict[str, object]] = []
    for frame_index, frame in enumerate(frames):
        x, y = _place_bottom_right(output, frame, frame_index * CELL_WIDTH)
        geometry.append(
            {
                "frame": frame_index,
                "box": boxes[frame_index][:4],
                "size": frame.size,
                "cell": (CELL_WIDTH, CELL_HEIGHT),
                "offset": (x - frame_index * CELL_WIDTH, y),
                "origin": (CELL_WIDTH, CELL_HEIGHT),
            }
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=output_path.parent,
            prefix=f".{output_path.name}.",
            suffix=".tmp",
            delete=False,
        ) as temporary:
            temporary_name = temporary.name
        output.save(temporary_name, format="PNG", optimize=False, compress_level=9)
        os.replace(temporary_name, output_path)
    finally:
        if temporary_name:
            try:
                os.unlink(temporary_name)
            except FileNotFoundError:
                pass
    return geometry


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default=DEFAULT_INPUT, help="Chill Penguin operator sheet")
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="generated ice clone strip")
    arguments = parser.parse_args()
    input_path = _repo_path(arguments.input)
    output_path = _repo_path(arguments.output)
    boxes, frames = _load_frames(input_path)
    geometry = build(input_path, output_path)
    with Image.open(output_path) as decoded:
        decoded.load()
        image = decoded.convert("RGBA")
        opaque = {pixel[:3] for pixel in image.getdata() if pixel[3]}
    print(f"input: {input_path}")
    print(f"output: {output_path}")
    print(f"mode: {image.mode} size: {image.size}")
    print(f"key: {KEY} resample: none recolour: none")
    print(f"sheet_sha256: {hashlib.sha256(input_path.read_bytes()).hexdigest()}")
    print(f"boxes: {[box[:4] for box in boxes]}")
    for item in geometry:
        print(
            f"frame {item['frame'] + 1}: box={item['box']} size={item['size']} "
            f"cell={item['cell']} offset={item['offset']} origin={item['origin']}"
        )
    for frame_index, progress in BOARD_PLACEMENTS:
        right = int(BOARD_X + BOARD_FRONT_OFFSET + progress * BOARD_ZW)
        x = right - frames[frame_index].width
        y = BOARD_FLOOR - frames[frame_index].height
        print(
            f"board frame {frame_index + 1}: progress={progress} right={right} "
            f"x={x} y={y} bottom={BOARD_FLOOR}"
        )
    print(f"frame_count: {len(frames)}")
    print(f"opaque_colors: {len(opaque)}")
    print(f"sha256: {hashlib.sha256(output_path.read_bytes()).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

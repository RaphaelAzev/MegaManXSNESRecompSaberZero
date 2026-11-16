#!/usr/bin/env python3
"""Build Boomer Kuwanger cutter projectile strips from its operator sheet."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import tempfile

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT = r"D:\Games\Mega Man\SaberZero\BoomerKuwangerSpecial.png"
DEFAULT_OUTPUT = "assets/saber-zero/sprites/Special_Cutter.png"
KEY = (0, 73, 70)
GAP = 2
EXPECTED_PIECE_COUNT = 29
FORWARD_PIECE_INDEX = 27
FORWARD_FRAME_COUNT = 4
DIAGONAL_PIECE_INDICES = (24, 25, 26, 28)


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


def _split_column_spans(image: Image.Image) -> list[tuple[int, int]]:
    columns = [
        any(image.getpixel((x, y))[3] for y in range(image.height))
        for x in range(image.width)
    ]
    spans: list[tuple[int, int]] = []
    start: int | None = None
    for x, occupied in enumerate(columns + [False]):
        if occupied and start is None:
            start = x
        if not occupied and start is not None:
            spans.append((start, x))
            start = None
    return spans


def split_columns(image: Image.Image) -> list[Image.Image]:
    """Split a keyed strip at columns with no opaque pixels."""
    frames: list[Image.Image] = []
    for start, end in _split_column_spans(image):
        piece = image.crop((start, 0, end, image.height))
        bounds = piece.getbbox()
        if bounds:
            frames.append(piece.crop(bounds))
    return frames


def _even(value: int) -> int:
    return value if value % 2 == 0 else value + 1


def _cell_size(frames: list[Image.Image]) -> tuple[int, int]:
    if not frames:
        raise ValueError("cannot size an empty strip")
    return (
        _even(max(frame.width for frame in frames)),
        _even(max(frame.height for frame in frames)),
    )


def _load_frames(
    input_path: Path,
) -> tuple[
    list[tuple[int, int, int, int, int]],
    list[Image.Image],
    list[Image.Image],
    list[tuple[int, int]],
]:
    boxes = segments(input_path)
    assert len(boxes) == EXPECTED_PIECE_COUNT, (
        f"Boomer Kuwanger sheet yielded {len(boxes)} pieces, expected "
        f"{EXPECTED_PIECE_COUNT}"
    )
    required_indices = (FORWARD_PIECE_INDEX,) + DIAGONAL_PIECE_INDICES
    for index in required_indices:
        assert index < len(boxes), f"Boomer Kuwanger piece {index} is missing"

    with Image.open(input_path) as decoded:
        image = decoded.convert("RGB")
    source_piece = lambda index: keyed(
        image.crop(boxes[index][:4])
    )
    forward_source = source_piece(FORWARD_PIECE_INDEX)
    split_spans = _split_column_spans(forward_source)
    forward_frames = split_columns(forward_source)
    assert len(forward_frames) == FORWARD_FRAME_COUNT, (
        f"Boomer Kuwanger piece {FORWARD_PIECE_INDEX} yielded "
        f"{len(forward_frames)} forward frames, expected {FORWARD_FRAME_COUNT}"
    )
    diagonal_frames = [source_piece(index) for index in DIAGONAL_PIECE_INDICES]
    assert len(diagonal_frames) == len(DIAGONAL_PIECE_INDICES)
    return boxes, forward_frames, diagonal_frames, split_spans


def _place_centered(
    output: Image.Image,
    frame: Image.Image,
    cell_x: int,
    cell_y: int,
    cell_width: int,
    cell_height: int,
) -> None:
    x = cell_x + (cell_width - frame.width) // 2
    y = cell_y + (cell_height - frame.height) // 2
    output.alpha_composite(frame, (x, y))


def build(input_path: Path, output_path: Path) -> list[dict[str, object]]:
    boxes, forward_frames, diagonal_frames, split_spans = _load_frames(input_path)
    forward_cell = _cell_size(forward_frames)
    diagonal_cell = _cell_size(diagonal_frames)
    output_width = max(
        forward_cell[0] * len(forward_frames),
        diagonal_cell[0] * len(diagonal_frames),
    )
    output_height = forward_cell[1] + diagonal_cell[1]
    output = Image.new("RGBA", (output_width, output_height), (0, 0, 0, 0))
    geometry: list[dict[str, object]] = []

    for frame_index, frame in enumerate(forward_frames):
        _place_centered(
            output,
            frame,
            frame_index * forward_cell[0],
            0,
            forward_cell[0],
            forward_cell[1],
        )
        geometry.append(
            {
                "strip": "forward",
                "frame": frame_index,
                "piece": FORWARD_PIECE_INDEX,
                "box": boxes[FORWARD_PIECE_INDEX][:4],
                "split": split_spans[frame_index],
                "piece_size": frame.size,
                "cell": forward_cell,
                "origin": (forward_cell[0] // 2, forward_cell[1] // 2),
            }
        )

    for frame_index, (piece_index, frame) in enumerate(
        zip(DIAGONAL_PIECE_INDICES, diagonal_frames)
    ):
        _place_centered(
            output,
            frame,
            frame_index * diagonal_cell[0],
            forward_cell[1],
            diagonal_cell[0],
            diagonal_cell[1],
        )
        geometry.append(
            {
                "strip": "diagonal",
                "frame": frame_index,
                "piece": piece_index,
                "box": boxes[piece_index][:4],
                "piece_size": frame.size,
                "cell": diagonal_cell,
                "origin": (diagonal_cell[0] // 2, diagonal_cell[1] // 2),
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
    parser.add_argument("--input", default=DEFAULT_INPUT, help="Boomer Kuwanger operator sheet")
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="generated cutter strip")
    arguments = parser.parse_args()
    input_path = _repo_path(arguments.input)
    output_path = _repo_path(arguments.output)
    boxes, forward_frames, diagonal_frames, split_spans = _load_frames(input_path)
    geometry = build(input_path, output_path)
    with Image.open(output_path) as decoded:
        decoded.load()
        image = decoded.convert("RGBA")
        opaque = {pixel[:3] for pixel in image.getdata() if pixel[3]}
    print(f"input: {input_path}")
    print(f"output: {output_path}")
    print(f"mode: {image.mode} size: {image.size}")
    print(f"key: {KEY} resample: none recolour: none")
    print(f"piece_count: {len(boxes)} expected: {EXPECTED_PIECE_COUNT}")
    for index in DIAGONAL_PIECE_INDICES + (FORWARD_PIECE_INDEX,):
        print(f"piece {index}: box={boxes[index][:4]}")
    print(f"forward_split_columns: {split_spans}")
    for item in geometry:
        print(
            f"{item['strip']} frame {item['frame']}: piece={item['piece']} "
            f"box={item['box']} split={item.get('split')} "
            f"piece_size={item['piece_size']} cell={item['cell']} "
            f"origin={item['origin']}"
        )
    print(f"forward_frames: {len(forward_frames)}")
    print(f"diagonal_frames: {len(diagonal_frames)}")
    print(f"opaque_colors: {len(opaque)}")
    print(f"sha256: {hashlib.sha256(output_path.read_bytes()).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Build Spark Mandrill's thunder slam and orb strips from the operator sheet."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT = r"D:\Games\Mega Man\SaberZero\SparkMandrillSpecial.png"
DEFAULT_SLAM_OUTPUT = "assets/saber-zero/sprites/Special_ThunderSlam.png"
DEFAULT_ORB_OUTPUT = "assets/saber-zero/sprites/Special_ThunderOrb.png"
MANIFEST_PATH = "tools/saber/saber_zero_manifest.json"
SOURCE_DIR = "assets/saber-zero/sprites"
KEY = (0, 73, 70)
GAP = 2
EXPECTED_SIZE = (908, 113)
EXPECTED_SHEET_SHA256 = "b298564e6970aa241eea8432cd6838d7a3be4cd91ad8cb438038f8d7eddb8e48"
EXPECTED_PIECE_COUNT = 18
EXPECTED_ART_PIECE_COUNT = 16
EXPECTED_BOXES = (
    (15, 22, 52, 67),
    (65, 15, 101, 67),
    (117, 13, 152, 67),
    (165, 18, 208, 67),
    (220, 18, 261, 67),
    (282, 16, 323, 67),
    (350, 16, 401, 71),
    (422, 16, 488, 73),
    (502, 16, 566, 73),
    (585, 16, 636, 71),
    (658, 16, 699, 70),
    (719, 16, 760, 67),
    (783, 16, 824, 67),
    (842, 16, 883, 67),
    (49, 74, 81, 105),
    (10, 75, 38, 104),
    (372, 102, 385, 113),
    (386, 104, 394, 113),
)
ZERO_FRAME_COUNT = 14
ZERO_CELL_SIZE = 128
ZERO_ORIGIN = (64, 88)
BOARD_ZX = 89
BOARD_BODY_CENTER_OFFSET = 5
BOARD_GLOW = {
    5: 25.4,
    6: 25.0,
    7: 32.5,
    8: 31.1,
    9: 24.6,
    10: 24.3,
    11: 26.8,
    12: 26.8,
    13: 26.8,
}
ORB_PIECE_INDICES = (14, 15)
ORB_CELL_SIZE = 32
ORB_ORIGIN = (16, 16)
PALETTE_CAP = 150
SNAP_DISTANCE_SQUARED = 48
TARGET_SOURCE_FILES = frozenset(
    {"Special_ThunderSlam.png", "Special_ThunderOrb.png"}
)


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


def _load_sheet(
    input_path: Path,
) -> tuple[Image.Image, list[tuple[int, int, int, int, int]]]:
    raw_sha256 = hashlib.sha256(input_path.read_bytes()).hexdigest()
    assert raw_sha256 == EXPECTED_SHEET_SHA256, (
        f"Spark Mandrill sheet SHA-256 changed: {raw_sha256}"
    )
    with Image.open(input_path) as decoded:
        assert decoded.size == EXPECTED_SIZE, (
            f"Spark Mandrill sheet size changed: {decoded.size}"
        )
        decoded.load()
        image = decoded.convert("RGB")
    boxes = segments(input_path)
    assert len(boxes) == EXPECTED_PIECE_COUNT, (
        f"Spark Mandrill sheet yielded {len(boxes)} pieces, expected "
        f"{EXPECTED_PIECE_COUNT}"
    )
    actual_boxes = [box[:4] for box in boxes]
    assert actual_boxes == list(EXPECTED_BOXES), (
        f"Spark Mandrill sheet segmentation changed: {actual_boxes}"
    )
    assert EXPECTED_ART_PIECE_COUNT == ZERO_FRAME_COUNT + len(ORB_PIECE_INDICES)
    return image, boxes


def _board_left(frame_index: int, frame_width: int) -> int:
    if frame_index in BOARD_GLOW:
        left = BOARD_ZX - BOARD_GLOW[frame_index]
    else:
        left = BOARD_ZX - BOARD_BODY_CENTER_OFFSET - frame_width / 2
    return int(round(left))


def _zero_offset(frame_index: int, frame_size: tuple[int, int]) -> tuple[int, int]:
    frame_width, frame_height = frame_size
    board_left = _board_left(frame_index, frame_width)
    cell_x = ZERO_ORIGIN[0] + (
        board_left - (BOARD_ZX - BOARD_BODY_CENTER_OFFSET)
    )
    cell_bottom = ZERO_ORIGIN[1] + (2 if 6 <= frame_index <= 9 else 0)
    cell_y = cell_bottom - frame_height
    return cell_x, cell_y


def _place(output: Image.Image, piece: Image.Image, x: int, y: int) -> None:
    if x < 0 or y < 0 or x + piece.width > output.width or y + piece.height > output.height:
        raise ValueError(
            f"piece placement outside output: size={piece.size} offset={(x, y)} "
            f"output={output.size}"
        )
    output.alpha_composite(piece, (x, y))


def _bgr555(rgb: tuple[int, int, int]) -> int:
    red, green, blue = rgb
    return (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)


def _rgb_distance_squared(
    first: tuple[int, int, int], second: tuple[int, int, int]
) -> int:
    return sum((left - right) ** 2 for left, right in zip(first, second))


def _load_existing_palette() -> tuple[list[tuple[int, int, int]], dict[str, int]]:
    manifest_path = ROOT / MANIFEST_PATH
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    source_specs = {
        source["id"]: source
        for source in manifest["sources"]
        if source["file"] not in TARGET_SOURCE_FILES
    }
    source_images: dict[int, Image.Image] = {}
    for source_id, source in source_specs.items():
        source_path = ROOT / SOURCE_DIR / source["file"]
        raw = source_path.read_bytes()
        actual_sha256 = hashlib.sha256(raw).hexdigest()
        if actual_sha256 != source["sha256"]:
            raise ValueError(
                f"existing Saber source SHA-256 changed: {source['file']}"
            )
        with Image.open(source_path) as decoded:
            decoded.load()
            source_images[source_id] = decoded.convert("RGBA")

    alpha_threshold = manifest.get("alpha_threshold")
    colors: set[tuple[int, int, int]] = set()
    for frame in manifest["frames"]:
        source = source_specs.get(frame["source"])
        if source is None:
            continue
        image = source_images[frame["source"]]
        for plane in (frame["body"], frame.get("blade")):
            if plane is None:
                continue
            x, y, width, height = plane["rect"]
            for row in range(y, y + height):
                for column in range(x, x + width):
                    red, green, blue, alpha = image.getpixel((column, row))
                    if alpha_threshold is None:
                        if alpha == 0:
                            continue
                        if alpha != 255:
                            raise ValueError(
                                "existing Saber source has partial alpha without threshold"
                            )
                    elif alpha < alpha_threshold:
                        continue
                    colors.add((red, green, blue))

    candidates = sorted(colors)
    palette: list[tuple[int, int, int]] = []
    bgr_values: set[int] = set()
    for color in candidates:
        value = _bgr555(color)
        if value not in bgr_values:
            bgr_values.add(value)
            palette.append(color)
    return palette, {
        "rgb_candidates": len(candidates),
        "bgr555_colors": len(palette),
        "palette_entries": len(palette) + 1,
    }


def _opaque_colors(image: Image.Image) -> set[tuple[int, int, int]]:
    return {pixel[:3] for pixel in image.getdata() if pixel[3]}


def _nearest_color(
    color: tuple[int, int, int], palette: list[tuple[int, int, int]]
) -> tuple[int, int, int]:
    return min(
        palette,
        key=lambda candidate: (_rgb_distance_squared(color, candidate), candidate),
    )


def _snap_to_existing_palette(
    images: tuple[Image.Image, Image.Image],
    existing_palette: list[tuple[int, int, int]],
) -> tuple[tuple[Image.Image, Image.Image], int, int]:
    existing_colors = set(existing_palette)
    snapped_images: list[Image.Image] = []
    snapped_pixels = 0
    snapped_colors: set[tuple[int, int, int]] = set()
    for image in images:
        output = image.copy()
        pixels = output.load()
        for y in range(output.height):
            for x in range(output.width):
                red, green, blue, alpha = pixels[x, y]
                if not alpha:
                    continue
                color = (red, green, blue)
                if color not in existing_colors:
                    nearest = _nearest_color(color, existing_palette)
                    if _rgb_distance_squared(color, nearest) <= SNAP_DISTANCE_SQUARED:
                        pixels[x, y] = nearest + (alpha,)
                        snapped_pixels += 1
                        snapped_colors.add(color)
        snapped_images.append(output)
    return (snapped_images[0], snapped_images[1]), snapped_pixels, len(snapped_colors)


def _shared_palette(
    images: tuple[Image.Image, Image.Image], cap: int
) -> list[tuple[int, int, int]]:
    opaque_pixels = [
        pixel[:3]
        for image in images
        for pixel in image.getdata()
        if pixel[3]
    ]
    if not opaque_pixels:
        raise ValueError("thunder art has no opaque pixels")
    opaque_image = Image.new("RGB", (len(opaque_pixels), 1))
    opaque_image.putdata(opaque_pixels)
    indexed = opaque_image.quantize(
        colors=cap,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )
    used = indexed.getcolors(maxcolors=cap)
    if used is None:
        raise ValueError("shared thunder palette exceeded cap")
    palette_data = indexed.getpalette()
    return [
        tuple(palette_data[index * 3 : index * 3 + 3])
        for _, index in sorted(used, key=lambda item: item[1])
    ]


def _map_to_palette(
    images: tuple[Image.Image, Image.Image], palette: list[tuple[int, int, int]]
) -> tuple[Image.Image, Image.Image]:
    reduced: list[Image.Image] = []
    for image in images:
        output = image.copy()
        pixels = output.load()
        for y in range(output.height):
            for x in range(output.width):
                red, green, blue, alpha = pixels[x, y]
                if alpha:
                    pixels[x, y] = _nearest_color(
                        (red, green, blue), palette
                    ) + (alpha,)
        reduced.append(output)
    return reduced[0], reduced[1]


def _reduce_palette(
    images: tuple[Image.Image, Image.Image],
    existing_palette: list[tuple[int, int, int]],
) -> tuple[tuple[Image.Image, Image.Image], dict[str, object]]:
    before = tuple(len(_opaque_colors(image)) for image in images)
    before_joint = len(set().union(*(_opaque_colors(image) for image in images)))
    snapped, snapped_pixels, snapped_colors = _snap_to_existing_palette(
        images, existing_palette
    )
    shared_palette = _shared_palette(snapped, PALETTE_CAP)
    reduced = _map_to_palette(snapped, shared_palette)
    after = tuple(len(_opaque_colors(image)) for image in reduced)
    after_joint = len(set().union(*(_opaque_colors(image) for image in reduced)))
    if after_joint > PALETTE_CAP:
        raise ValueError(
            f"reduced thunder art has {after_joint} opaque colours, cap is {PALETTE_CAP}"
        )

    error_sum = 0.0
    error_max = 0.0
    opaque_pixels = 0
    for original, final in zip(images, reduced):
        for original_pixel, final_pixel in zip(
            original.getdata(), final.getdata()
        ):
            if original_pixel[3] != final_pixel[3]:
                raise ValueError("palette reduction changed alpha")
            if original_pixel[3]:
                error = math.sqrt(
                    _rgb_distance_squared(original_pixel[:3], final_pixel[:3])
                )
                error_sum += error
                error_max = max(error_max, error)
                opaque_pixels += 1
    return reduced, {
        "before": before,
        "before_joint": before_joint,
        "after": after,
        "after_joint": after_joint,
        "shared_palette": len(shared_palette),
        "snapped_pixels": snapped_pixels,
        "snapped_colors": snapped_colors,
        "rgb_error_max": error_max,
        "rgb_error_mean": error_sum / opaque_pixels,
    }


def _publish(image: Image.Image, output_path: Path) -> None:
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
        image.save(
            temporary_name,
            format="PNG",
            optimize=False,
            compress_level=9,
        )
        os.replace(temporary_name, output_path)
    finally:
        if temporary_name:
            try:
                os.unlink(temporary_name)
            except FileNotFoundError:
                pass


def build(
    input_path: Path,
    slam_output_path: Path,
    orb_output_path: Path,
) -> dict[str, object]:
    image, boxes = _load_sheet(input_path)
    pieces = [keyed(image.crop(box[:4])) for box in boxes]
    assert len(pieces) == EXPECTED_PIECE_COUNT

    slam = Image.new(
        "RGBA",
        (ZERO_CELL_SIZE * ZERO_FRAME_COUNT, ZERO_CELL_SIZE),
        (0, 0, 0, 0),
    )
    slam_geometry: list[dict[str, object]] = []
    for frame_index in range(ZERO_FRAME_COUNT):
        piece = pieces[frame_index]
        offset = _zero_offset(frame_index, piece.size)
        _place(slam, piece, frame_index * ZERO_CELL_SIZE + offset[0], offset[1])
        slam_geometry.append(
            {
                "frame": frame_index,
                "piece": frame_index,
                "box": boxes[frame_index][:4],
                "piece_size": piece.size,
                "cell": (ZERO_CELL_SIZE, ZERO_CELL_SIZE),
                "offset": offset,
                "cell_offset": (frame_index * ZERO_CELL_SIZE + offset[0], offset[1]),
                "origin": ZERO_ORIGIN,
                "bottom": ZERO_ORIGIN[1] + (2 if 6 <= frame_index <= 9 else 0),
            }
        )

    orb = Image.new(
        "RGBA",
        (ORB_CELL_SIZE * len(ORB_PIECE_INDICES), ORB_CELL_SIZE),
        (0, 0, 0, 0),
    )
    orb_geometry: list[dict[str, object]] = []
    for frame_index, piece_index in enumerate(ORB_PIECE_INDICES):
        piece = pieces[piece_index]
        offset = (
            (ORB_CELL_SIZE - piece.width) // 2,
            (ORB_CELL_SIZE - piece.height) // 2,
        )
        _place(orb, piece, frame_index * ORB_CELL_SIZE + offset[0], offset[1])
        orb_geometry.append(
            {
                "frame": frame_index,
                "piece": piece_index,
                "box": boxes[piece_index][:4],
                "piece_size": piece.size,
                "cell": (ORB_CELL_SIZE, ORB_CELL_SIZE),
                "offset": offset,
                "cell_offset": (frame_index * ORB_CELL_SIZE + offset[0], offset[1]),
                "origin": ORB_ORIGIN,
            }
        )

    existing_palette, existing_palette_report = _load_existing_palette()
    (slam, orb), reduction_report = _reduce_palette(
        (slam, orb), existing_palette
    )

    _publish(slam, slam_output_path)
    _publish(orb, orb_output_path)
    return {
        "input": input_path,
        "slam_output": slam_output_path,
        "orb_output": orb_output_path,
        "boxes": boxes,
        "slam": slam_geometry,
        "orb": orb_geometry,
        "existing_palette": existing_palette_report,
        "reduction": reduction_report,
    }


def _opaque_color_count(path: Path) -> int:
    with Image.open(path) as decoded:
        decoded.load()
        image = decoded.convert("RGBA")
        return len({pixel[:3] for pixel in image.getdata() if pixel[3]})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default=DEFAULT_INPUT, help="Spark Mandrill operator sheet")
    parser.add_argument(
        "--slam-output",
        default=DEFAULT_SLAM_OUTPUT,
        help="generated thunder slam body strip",
    )
    parser.add_argument(
        "--orb-output",
        default=DEFAULT_ORB_OUTPUT,
        help="generated thunder orb projectile strip",
    )
    arguments = parser.parse_args()
    input_path = _repo_path(arguments.input)
    slam_output_path = _repo_path(arguments.slam_output)
    orb_output_path = _repo_path(arguments.orb_output)
    report = build(input_path, slam_output_path, orb_output_path)

    print(f"input: {report['input']}")
    print(f"slam_output: {report['slam_output']}")
    print(f"orb_output: {report['orb_output']}")
    print(f"sheet_sha256: {hashlib.sha256(input_path.read_bytes()).hexdigest()}")
    print(
        f"key: {KEY} resample: none palette_cap: {PALETTE_CAP} "
        f"snap_distance_squared: {SNAP_DISTANCE_SQUARED}"
    )
    existing_palette = report["existing_palette"]
    print(
        f"existing_palette: rgb_candidates={existing_palette['rgb_candidates']} "
        f"bgr555_colors={existing_palette['bgr555_colors']} "
        f"palette_entries={existing_palette['palette_entries']}"
    )
    print(f"piece_count: {len(report['boxes'])} expected: {EXPECTED_PIECE_COUNT}")
    for index, box in enumerate(report["boxes"]):
        print(f"piece {index}: box={box[:4]} pixels={box[4]}")
    for item in report["slam"]:
        print(
            f"slam frame {item['frame']}: piece={item['piece']} box={item['box']} "
            f"piece_size={item['piece_size']} cell={item['cell']} "
            f"offset={item['offset']} cell_offset={item['cell_offset']} "
            f"bottom={item['bottom']} origin={item['origin']}"
        )
    for item in report["orb"]:
        print(
            f"orb frame {item['frame']}: piece={item['piece']} box={item['box']} "
            f"piece_size={item['piece_size']} cell={item['cell']} "
            f"offset={item['offset']} cell_offset={item['cell_offset']} "
            f"origin={item['origin']}"
        )
    with Image.open(slam_output_path) as decoded:
        decoded.load()
        slam = decoded.convert("RGBA")
    with Image.open(orb_output_path) as decoded:
        decoded.load()
        orb = decoded.convert("RGBA")
    print(f"slam_mode: {slam.mode} size: {slam.size}")
    print(f"orb_mode: {orb.mode} size: {orb.size}")
    reduction = report["reduction"]
    print(
        f"slam_opaque_colors: before={reduction['before'][0]} "
        f"after={reduction['after'][0]}"
    )
    print(
        f"orb_opaque_colors: before={reduction['before'][1]} "
        f"after={reduction['after'][1]}"
    )
    print(f"joint_opaque_colors: before={reduction['before_joint']} after={reduction['after_joint']}")
    print(f"shared_palette_colors: {reduction['shared_palette']}")
    print(f"snapped_opaque_pixels: {reduction['snapped_pixels']}")
    print(f"snapped_unique_colors: {reduction['snapped_colors']}")
    print(f"rgb_error_euclidean_max: {reduction['rgb_error_max']:.15f}")
    print(f"rgb_error_euclidean_mean: {reduction['rgb_error_mean']:.15f}")
    print(f"slam_sha256: {hashlib.sha256(slam_output_path.read_bytes()).hexdigest()}")
    print(f"orb_sha256: {hashlib.sha256(orb_output_path.read_bytes()).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Build Storm Eagle's pink, enlarged uppercut effect strip.

The transform matches ``.foreman/mock/scenes3.py:storm``:

* isolate the connected green-edge/white-fill blade;
* map its luminance through the mock's PINK ramp;
* scale the 128x128 canvas 1.4x with nearest-neighbour resampling;
* scale about the measured hand pivot for frames 0-2 and the contact pivot
  for frames 3-6.

The source strips use the extractor's cell convention. Their assembled
manifest planes are one pixel right and three pixels down from raw cell
coordinates. The generated cells therefore store the transformed canvas at
(-1, -3), while manifest origins remain ``(cell bbox + (1, 3))``.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
from typing import Any

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
CANVAS = 128
SCALE = 1.4
CELL_OFFSET = (-1, -3)
HAND = {0: (60, 76), 1: (82, 84), 2: (69, 47)}
PINK = [
    (104, 32, 96),
    (168, 64, 152),
    (216, 120, 200),
    (240, 184, 232),
    (248, 232, 248),
]


def _repo_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def lum(pixel: tuple[int, int, int, int]) -> float:
    return 0.299 * pixel[0] + 0.587 * pixel[1] + 0.114 * pixel[2]


def is_green(pixel: tuple[int, int, int, int]) -> bool:
    return pixel[1] > pixel[0] + 20 and pixel[1] > pixel[2] + 20


def is_light(pixel: tuple[int, int, int, int]) -> bool:
    return min(pixel[:3]) >= 200


def blade_mask(image: Image.Image) -> set[tuple[int, int]]:
    pixels = image.load()
    width, height = image.size
    seen: set[tuple[int, int]] = set()
    stack = [
        (x, y)
        for y in range(height)
        for x in range(width)
        if pixels[x, y][3] and is_green(pixels[x, y])
    ]
    seen.update(stack)
    while stack:
        x, y = stack.pop()
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                point = (x + dx, y + dy)
                if point in seen or not (
                    0 <= point[0] < width and 0 <= point[1] < height
                ):
                    continue
                pixel = pixels[point]
                if pixel[3] and (is_green(pixel) or is_light(pixel)):
                    seen.add(point)
                    stack.append(point)
    return seen


def blade_only(image: Image.Image) -> Image.Image:
    mask = blade_mask(image)
    result = Image.new("RGBA", image.size, (0, 0, 0, 0))
    source_pixels = image.load()
    result_pixels = result.load()
    for point in mask:
        result_pixels[point] = source_pixels[point]
    return result


def pink(image: Image.Image) -> Image.Image:
    result = image.copy()
    pixels = result.load()
    for y in range(result.height):
        for x in range(result.width):
            pixel = pixels[x, y]
            if not pixel[3]:
                continue
            index = min(len(PINK) - 1, int(lum(pixel) / 256 * len(PINK)))
            pixels[x, y] = PINK[index] + (255,)
    return result


def contact_pivot(body: Image.Image, effect: Image.Image) -> tuple[float, float] | None:
    body_pixels = body.load()
    effect_pixels = effect.load()
    width, height = body.size
    hits: list[tuple[int, int]] = []
    for y in range(height):
        for x in range(width):
            if not effect_pixels[x, y][3]:
                continue
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    point = (x + dx, y + dy)
                    if (
                        0 <= point[0] < width
                        and 0 <= point[1] < height
                        and body_pixels[point][3]
                        and not effect_pixels[point][3]
                    ):
                        hits.append((x, y))
                        break
                else:
                    continue
                break
    if not hits:
        return None
    return (
        sum(point[0] for point in hits) / len(hits),
        sum(point[1] for point in hits) / len(hits),
    )


def scale_about(image: Image.Image, factor: float, ox: float, oy: float) -> Image.Image:
    """Match the nearest-neighbour canvas transform in ``scenes2.py``."""
    enlarged = image.resize(
        (round(image.width * factor), round(image.height * factor)),
        Image.Resampling.NEAREST,
    )
    dx, dy = round(ox - ox * factor), round(oy - oy * factor)
    temporary = Image.new(
        "RGBA", (image.width * 3, image.height * 3), (0, 0, 0, 0)
    )
    temporary.alpha_composite(enlarged, (image.width + dx, image.height + dy))
    return temporary.crop(
        (image.width, image.height, image.width * 2, image.height * 2)
    )


def compose_manifest_frame(
    source: Image.Image, frame: dict[str, Any]
) -> Image.Image:
    x, y, width, height = frame["body"]["rect"]
    origin_x, origin_y = frame["body"]["origin"]
    canvas = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    canvas.alpha_composite(
        source.crop((x, y, x + width, y + height)),
        (origin_x, origin_y),
    )
    return canvas


def _load_body_frames(manifest: dict[str, Any]) -> list[Image.Image]:
    frames_by_id = {frame["id"]: frame for frame in manifest["frames"]}
    animation = next(animation for animation in manifest["animations"] if animation["id"] == 8)
    body_source_id = 8
    body_source_name = next(
        source["file"]
        for source in manifest["sources"]
        if source["id"] == body_source_id
    )
    body_path = ROOT / "assets" / "saber-zero" / "sprites" / body_source_name
    with Image.open(body_path) as decoded:
        decoded.load()
        body_source = decoded.convert("RGBA")
    return [
        compose_manifest_frame(body_source, frames_by_id[step["frame"]])
        for step in animation["steps"]
    ]


def build(input_path: Path, output_path: Path) -> list[dict[str, Any]]:
    with Image.open(input_path) as decoded:
        decoded.load()
        if decoded.mode != "RGBA":
            raise ValueError(f"input must be RGBA, got {decoded.mode}")
        source = decoded.copy()

    if source.height != CANVAS or source.width % CANVAS:
        raise ValueError(f"input must be a strip of 128-pixel cells, got {source.size}")
    cell_count = source.width // CANVAS
    if cell_count != 7:
        raise ValueError(f"uppercut FX strip must contain 7 cells, got {cell_count}")

    manifest_path = ROOT / "tools" / "saber" / "saber_zero_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    body_frames = _load_body_frames(manifest)
    output = Image.new("RGBA", source.size, (0, 0, 0, 0))
    geometry: list[dict[str, Any]] = []

    for step, body in enumerate(body_frames):
        cell = source.crop((step * CANVAS, 0, (step + 1) * CANVAS, CANVAS))

        # Recreate frame(9, step) from the old manifest. Extracted FX cells
        # assemble one pixel right and three pixels down on the 128 canvas.
        old_effect = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
        old_effect.alpha_composite(cell, (1, 3))
        effect = pink(blade_only(old_effect))
        bbox = effect.getbbox()
        if bbox is None:
            raise ValueError(f"uppercut FX frame {step} has no opaque pixels")

        pivot = HAND.get(step) or contact_pivot(body, old_effect)
        if pivot is None:
            body_bbox = body.getbbox()
            if body_bbox is None:
                raise ValueError(f"uppercut body frame {step} has no opaque pixels")
            hand_centre = (
                (body_bbox[0] + body_bbox[2]) / 2,
                (body_bbox[1] + body_bbox[3]) / 2,
            )
            points = [
                (x, y)
                for y in range(bbox[1], bbox[3])
                for x in range(bbox[0], bbox[2])
                if effect.getpixel((x, y))[3]
            ]
            pivot = min(
                points,
                key=lambda point: (point[0] - hand_centre[0]) ** 2
                + (point[1] - hand_centre[1]) ** 2,
            )

        transformed = scale_about(effect, SCALE, pivot[0], pivot[1])
        transformed_bbox = transformed.getbbox()
        if transformed_bbox is None:
            raise ValueError(f"scaled uppercut FX frame {step} has no opaque pixels")

        output.alpha_composite(
            transformed,
            (step * CANVAS + CELL_OFFSET[0], CELL_OFFSET[1]),
        )
        cell_bbox = (
            transformed_bbox[0] + CELL_OFFSET[0],
            transformed_bbox[1] + CELL_OFFSET[1],
            transformed_bbox[2] + CELL_OFFSET[0],
            transformed_bbox[3] + CELL_OFFSET[1],
        )
        geometry.append(
            {
                "step": step,
                "pivot": pivot,
                "canvas_bbox": transformed_bbox,
                "cell_bbox": cell_bbox,
                "rect": [
                    step * CANVAS + cell_bbox[0],
                    cell_bbox[1],
                    cell_bbox[2] - cell_bbox[0],
                    cell_bbox[3] - cell_bbox[1],
                ],
                "origin": [cell_bbox[0] + 1, cell_bbox[1] + 3],
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
    parser.add_argument(
        "--input",
        default="assets/saber-zero/sprites/Special_Uppercut_FX.png",
        help="source uppercut FX strip, relative to the repository root",
    )
    parser.add_argument(
        "--output",
        default="assets/saber-zero/sprites/Special_Uppercut_FX_Storm.png",
        help="generated Storm uppercut FX strip, relative to the repository root",
    )
    arguments = parser.parse_args()
    input_path = _repo_path(arguments.input)
    output_path = _repo_path(arguments.output)
    geometry = build(input_path, output_path)
    with Image.open(output_path) as decoded:
        decoded.load()
        image = decoded.convert("RGBA")
        opaque = {pixel for pixel in image.getdata() if pixel[3]}
    print(f"input: {input_path}")
    print(f"output: {output_path}")
    print(f"mode: RGBA size: {image.size}")
    print(f"scale: {SCALE} resample: NEAREST")
    for item in geometry:
        print(
            f"frame {item['step']}: pivot={item['pivot']} "
            f"canvas_bbox={item['canvas_bbox']} cell_bbox={item['cell_bbox']} "
            f"rect={item['rect']} origin={item['origin']}"
        )
    print(f"opaque_colors: {sorted(opaque)}")
    print(f"sha256: {hashlib.sha256(output_path.read_bytes()).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

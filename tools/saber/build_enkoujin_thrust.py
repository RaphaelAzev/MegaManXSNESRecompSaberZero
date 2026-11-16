#!/usr/bin/env python3
"""Build the six-cell Enkoujin thrust strip from the operator sheet."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import tempfile

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT = r"D:\Games\Mega Man\SaberZero\Enkoujin.png"
DEFAULT_OUTPUT = "assets/saber-zero/sprites/Special_Enkoujin.png"
CELL_SIZE = 128
EXPECTED_CELL_COUNT = 12
USED_CELL_COUNT = 6


def _repo_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def _load_sheet(input_path: Path) -> Image.Image:
    with Image.open(input_path) as decoded:
        if decoded.mode != "RGBA":
            raise ValueError(
                f"Enkoujin sheet must be RGBA, got {decoded.mode}"
            )
        expected_size = (CELL_SIZE * EXPECTED_CELL_COUNT, CELL_SIZE)
        if decoded.size != expected_size:
            raise ValueError(
                f"Enkoujin sheet must be {EXPECTED_CELL_COUNT} "
                f"{CELL_SIZE}x{CELL_SIZE} RGBA cells, got {decoded.size}"
            )
        decoded.load()
        return decoded.copy()


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


def build(input_path: Path, output_path: Path) -> dict[str, object]:
    sheet = _load_sheet(input_path)
    output = sheet.crop((0, 0, CELL_SIZE * USED_CELL_COUNT, CELL_SIZE))
    if output.mode != "RGBA" or output.size != (CELL_SIZE * USED_CELL_COUNT, CELL_SIZE):
        raise ValueError(
            f"Enkoujin output must be RGBA {CELL_SIZE * USED_CELL_COUNT}x{CELL_SIZE}, "
            f"got {output.mode} {output.size}"
        )
    _publish(output, output_path)
    return {
        "input": input_path,
        "output": output_path,
        "input_size": sheet.size,
        "output_size": output.size,
        "input_mode": sheet.mode,
        "output_mode": output.mode,
        "input_cells": EXPECTED_CELL_COUNT,
        "output_cells": USED_CELL_COUNT,
        "origin": (1, 3),
        "canvas_anchor": (64, 88),
        "sha256": hashlib.sha256(output_path.read_bytes()).hexdigest(),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        default=DEFAULT_INPUT,
        help="Enkoujin operator sheet",
    )
    parser.add_argument(
        "--output",
        default=DEFAULT_OUTPUT,
        help="generated Enkoujin thrust strip",
    )
    arguments = parser.parse_args()
    report = build(_repo_path(arguments.input), _repo_path(arguments.output))
    print(f"input: {report['input']}")
    print(f"output: {report['output']}")
    print(
        f"input_mode: {report['input_mode']} size: {report['input_size']} "
        f"cells: {report['input_cells']}"
    )
    print(
        f"output_mode: {report['output_mode']} size: {report['output_size']} "
        f"cells: {report['output_cells']}"
    )
    print(f"origin: {report['origin']} canvas_anchor: {report['canvas_anchor']}")
    print(f"sha256: {report['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

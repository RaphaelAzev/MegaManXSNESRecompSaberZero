#!/usr/bin/env python3
"""Extract an SNES-style crescent strip from a PlayStation crescent rip.

Shape traced from a rip of Zero's crescent slash in Capcom's PlayStation Mega
Man X games (provided by the project author); SNES-style redraw made for this
project; original art (c) Capcom.
"""
import argparse
import math
from collections import Counter
from pathlib import Path

from PIL import Image


CELL = 128
RAMP = (
    (200, 72, 0),
    (248, 144, 0),
    (248, 216, 32),
    (248, 248, 160),
    (248, 248, 248),
)
_RESAMPLING = getattr(Image, "Resampling", Image)
_BOX = _RESAMPLING.BOX
_NEAREST = _RESAMPLING.NEAREST


def _colour_distance(pixel, background):
    return math.sqrt(
        sum((pixel[channel] - background[channel]) ** 2 for channel in range(3))
    )


def _key_mask(sheet, background, tolerance):
    """Return the keyed binary mask for the complete source sheet."""
    width, height = sheet.size
    pixels = sheet.load()
    return [
        [
            _colour_distance(pixels[x, y], background) > tolerance
            for x in range(width)
        ]
        for y in range(height)
    ]


def _frame_boxes(mask, min_gap):
    """Find foreground column runs and return inclusive frame boxes."""
    height = len(mask)
    width = len(mask[0]) if height else 0
    columns = [
        any(mask[y][x] for y in range(height))
        for x in range(width)
    ]

    runs = []
    start = None
    for x, foreground in enumerate(columns + [False]):
        if foreground and start is None:
            start = x
        elif not foreground and start is not None:
            runs.append((start, x - 1))
            start = None

    merged = []
    for left, right in runs:
        gap = left - merged[-1][1] - 1 if merged else None
        if merged and gap < min_gap:
            merged[-1] = (merged[-1][0], right)
        else:
            merged.append((left, right))

    boxes = []
    for left, right in merged:
        rows = [
            y
            for y in range(height)
            if any(mask[y][x] for x in range(left, right + 1))
        ]
        if not rows:
            continue
        boxes.append((left, min(rows), right, max(rows)))
    if not boxes:
        raise ValueError("No keyed foreground found in the source rip")
    return boxes


def _fit_circle(mask, box):
    """Fit a Kasa least-squares circle to one frame's edge pixels."""
    height = len(mask)
    width = len(mask[0]) if height else 0
    x0, y0, x1, y1 = box
    points = []
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if mask[y][x] and any(
                not (
                    0 <= y + dy < height
                    and 0 <= x + dx < width
                    and mask[y + dy][x + dx]
                )
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
            ):
                points.append((x, y))

    if not points:
        raise ValueError(f"Frame {box} has no edge pixels")

    count = len(points)
    sx = sum(point[0] for point in points) / count
    sy = sum(point[1] for point in points) / count
    suu = svv = suv = suuu = svvv = suvv = svuu = 0.0
    for x, y in points:
        u, v = x - sx, y - sy
        suu += u * u
        svv += v * v
        suv += u * v
        suuu += u ** 3
        svvv += v ** 3
        suvv += u * v * v
        svuu += v * u * u

    a, b, c, d = suu, suv, suv, svv
    e = 0.5 * (suuu + suvv)
    f = 0.5 * (svvv + svuu)
    determinant = a * d - b * c
    if determinant == 0:
        raise ValueError(f"Frame {box} has a degenerate edge circle fit")

    uc = (e * d - b * f) / determinant
    vc = (a * f - e * c) / determinant
    cx, cy = uc + sx, vc + sy
    radius = math.sqrt(uc * uc + vc * vc + (suu + svv) / count)
    return (cx, cy), radius


def _shape(mask, box, scale, centre):
    """Resize one binary frame with BOX coverage and return its scaled centre."""
    x0, y0, x1, y1 = box
    width = x1 - x0 + 1
    height = y1 - y0 + 1
    coverage = Image.new("L", (width, height), 0)
    pixels = coverage.load()
    for y in range(height):
        for x in range(width):
            pixels[x, y] = 255 if mask[y0 + y][x0 + x] else 0

    target = (
        max(1, round(width / scale)),
        max(1, round(height / scale)),
    )
    small = coverage.resize(target, _BOX)
    binary = small.point(lambda value: 255 if value >= 128 else 0)
    scaled_centre = ((centre[0] - x0) / scale, (centre[1] - y0) / scale)
    return binary, scaled_centre


def _bfs(mask, width, height, seeds):
    """Return 4-neighbour distances from seeds inside a binary shape."""
    infinite = 1 << 30
    distances = [[infinite] * width for _ in range(height)]
    queue = list(seeds)
    for x, y in queue:
        distances[y][x] = 0

    index = 0
    while index < len(queue):
        x, y = queue[index]
        index += 1
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if (
                0 <= nx < width
                and 0 <= ny < height
                and mask[ny][nx]
                and distances[ny][nx] > distances[y][x] + 1
            ):
                distances[ny][nx] = distances[y][x] + 1
                queue.append((nx, ny))
    return distances


def _sym(outer_distance, inner_distance):
    """Apply the approved five-colour ramp from both shape edges."""
    distance = min(outer_distance, inner_distance)
    return RAMP[distance] if distance < 4 else RAMP[4]


def _redraw(mask_image, centre):
    """Colour a resized binary shape using symmetric edge-distance bands."""
    width, height = mask_image.size
    pixels = mask_image.load()
    mask = [
        [pixels[x, y] > 0 for x in range(width)]
        for y in range(height)
    ]
    points = [
        (x, y)
        for y in range(height)
        for x in range(width)
        if mask[y][x]
    ]
    if not points:
        return Image.new("RGBA", (width, height), (0, 0, 0, 0))

    centre_x, centre_y = centre
    mean_radius = sum(
        math.hypot(x - centre_x, y - centre_y)
        for x, y in points
    ) / len(points)
    outer = []
    inner = []
    for x, y in points:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if not (
                0 <= nx < width
                and 0 <= ny < height
                and mask[ny][nx]
            ):
                seeds = outer if math.hypot(
                    nx - centre_x, ny - centre_y
                ) > mean_radius else inner
                seeds.append((x, y))
                break

    outer_distance = _bfs(mask, width, height, outer)
    inner_distance = _bfs(mask, width, height, inner)
    output = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    output_pixels = output.load()
    for x, y in points:
        output_pixels[x, y] = _sym(
            outer_distance[y][x],
            inner_distance[y][x],
        ) + (255,)
    return output


def _load_rolling(path):
    with Image.open(path) as opened:
        rolling = opened.convert("RGBA")
    if rolling.width == 0 or rolling.width % CELL:
        raise ValueError("--rolling must contain whole 128x128 cells")
    if rolling.height < CELL:
        raise ValueError("--rolling must contain 128-pixel-tall cells")

    cells = []
    for index in range(rolling.width // CELL):
        cell = rolling.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        bbox = cell.getbbox()
        if bbox is None:
            raise ValueError(f"--rolling cell {index} has no opaque pixels")
        anchor = ((bbox[0] + bbox[2]) / 2, (bbox[1] + bbox[3]) / 2)
        cells.append((cell, anchor))
    return cells


def _place_crescents(shapes, rolling_cells):
    """Render crescent cells and reject placement that clips a 128x128 cell."""
    placed = []
    for index, (mask_image, centre) in enumerate(shapes):
        crescent = _redraw(mask_image, centre)
        _, (zero_x, zero_y) = rolling_cells[index]
        left = round(zero_x - centre[0])
        top = round(zero_y - centre[1])
        if (
            left < 0
            or top < 0
            or left + crescent.width > CELL
            or top + crescent.height > CELL
        ):
            raise ValueError(
                f"Crescent frame {index} clips its {CELL}x{CELL} cell "
                f"at ({left}, {top}) with size "
                f"{crescent.width}x{crescent.height}"
            )

        cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        cell.alpha_composite(crescent, (left, top))
        placed.append((cell, crescent.size, (left, top)))
    return placed


def _make_strip(placed):
    strip = Image.new("RGBA", (CELL * len(placed), CELL), (0, 0, 0, 0))
    for index, (cell, _, _) in enumerate(placed):
        strip.alpha_composite(cell, (index * CELL, 0))
    return strip


def _make_preview(placed, rolling_cells, rolling_path):
    fx_path = rolling_path.with_name(
        f"{rolling_path.stem}_FX{rolling_path.suffix}"
    )
    with Image.open(fx_path) as opened:
        fx_strip = opened.convert("RGBA")
    if fx_strip.height < CELL or fx_strip.width < CELL * (len(placed) - 1):
        raise ValueError(
            f"{fx_path} must contain at least "
            f"{len(placed) - 1} 128x128 FX cells"
        )

    row = Image.new("RGBA", (CELL * len(placed), CELL), (0, 0, 0, 0))
    for index, (crescent, _, _) in enumerate(placed):
        cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        cell.alpha_composite(rolling_cells[index][0], (0, 0))
        if index >= 1:
            fx = fx_strip.crop(
                ((index - 1) * CELL, 0, index * CELL, CELL)
            )
            cell.alpha_composite(fx, (0, 0))
        cell.alpha_composite(crescent, (0, 0))
        row.alpha_composite(cell, (index * CELL, 0))
    return row.resize((row.width * 2, row.height * 2), _NEAREST)


def _print_report(background, boxes, fits, placed, strip):
    print(f"Background: {background}")
    print(f"Frames: {len(boxes)}")
    for index, (box, (centre, radius), (_, size, offset)) in enumerate(
        zip(boxes, fits, placed)
    ):
        print(
            f"Frame {index}: box={box}, "
            f"arc centre=({centre[0]:.6f}, {centre[1]:.6f}), "
            f"radius={radius:.6f}, scaled size={size[0]}x{size[1]}, "
            f"offset={offset}"
        )
    colours = Counter(
        pixel[:3]
        for pixel in strip.getdata()
        if pixel[3] == 255
    )
    print(f"Colour count: {len(colours)}")
    for colour in sorted(colours):
        print(f"  {colour}: {colours[colour]}")


def extract_crescent(
    rip_path,
    rolling_path,
    out,
    preview=None,
    scale=1.4,
    key_tolerance=80.0,
    min_gap=3,
    report=False,
):
    """Extract, redraw, anchor, save, and optionally preview crescent frames."""
    if scale <= 0:
        raise ValueError("--scale must be greater than zero")
    if key_tolerance <= 0:
        raise ValueError("--key-tolerance must be greater than zero")
    if min_gap < 0:
        raise ValueError("--min-gap must not be negative")

    with Image.open(rip_path) as opened:
        rip = opened.convert("RGB")
    background = Counter(rip.getdata()).most_common(1)[0][0]
    mask = _key_mask(rip, background, key_tolerance)
    boxes = _frame_boxes(mask, min_gap)
    rolling_cells = _load_rolling(rolling_path)
    if len(boxes) != len(rolling_cells):
        raise ValueError(
            f"Rip produced {len(boxes)} frames, but --rolling has "
            f"{len(rolling_cells)} 128x128 cells"
        )
    fits = [_fit_circle(mask, box) for box in boxes]

    shapes = [
        _shape(mask, box, scale, centre)
        for box, (centre, _) in zip(boxes, fits)
    ]
    placed = _place_crescents(shapes, rolling_cells)
    strip = _make_strip(placed)

    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    strip.save(out)

    if preview is not None:
        preview = Path(preview)
        preview.parent.mkdir(parents=True, exist_ok=True)
        _make_preview(placed, rolling_cells, Path(rolling_path)).save(preview)

    if report:
        _print_report(background, boxes, fits, placed, strip)


def _positive_float(value):
    number = float(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return number


def _nonnegative_int(value):
    number = int(value)
    if number < 0:
        raise argparse.ArgumentTypeError("must not be negative")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rip", type=Path)
    parser.add_argument("--rolling", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--preview", type=Path)
    parser.add_argument("--scale", type=_positive_float, default=1.4)
    parser.add_argument("--key-tolerance", type=_positive_float, default=80.0)
    parser.add_argument("--min-gap", type=_nonnegative_int, default=3)
    parser.add_argument("--report", action="store_true")
    args = parser.parse_args()

    try:
        extract_crescent(
            args.rip,
            args.rolling,
            args.out,
            preview=args.preview,
            scale=args.scale,
            key_tolerance=args.key_tolerance,
            min_gap=args.min_gap,
            report=args.report,
        )
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()

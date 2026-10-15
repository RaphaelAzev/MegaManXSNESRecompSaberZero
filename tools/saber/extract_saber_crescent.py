#!/usr/bin/env python3
"""Extract crescent-slash frames from a noisy custom effect sheet.

Source credit: The Spriters Resource, Mega Man X Customs, asset 124142
(https://www.spriters-resource.com/custom_edited/megamanxcustoms/asset/124142/).
TODO(credit): add the asset author.
"""
import argparse
import math
import statistics
from pathlib import Path

from PIL import Image, ImageDraw


CELL = 128
ROLLING_FRAMES = 9
_LANCZOS = getattr(getattr(Image, "Resampling", Image), "LANCZOS")


def _clamp(value, low, high):
    return max(low, min(high, value))


def _border_median(image):
    """Return the per-channel median of the image border."""
    width, height = image.size
    if width < 1 or height < 1:
        raise ValueError("The source sheet is empty")

    border = []
    for x in range(width):
        border.append(image.getpixel((x, 0)))
        if height > 1:
            border.append(image.getpixel((x, height - 1)))
    for y in range(1, height - 1):
        border.append(image.getpixel((0, y)))
        if width > 1:
            border.append(image.getpixel((width - 1, y)))

    return tuple(
        int(round(statistics.median(pixel[channel] for pixel in border)))
        for channel in range(3)
    )


def _colour_distance(pixel, background):
    return math.sqrt(
        sum((int(channel) - int(bg_channel)) ** 2
            for channel, bg_channel in zip(pixel, background))
    )


def _soft_alpha(pixel, background, tolerance):
    """Return keyed foreground alpha as a float in the range 0..1."""
    distance = _colour_distance(pixel, background)
    if distance <= tolerance:
        return 0.0

    # Smooth blue changes can be glow or just the sheet background.  Judge
    # them by RGB value too, retaining only a clearly brighter glow.
    blue_tinted = pixel[2] >= pixel[0] and pixel[2] >= pixel[1]
    brightness_gain = max(pixel) - max(background)
    if (
        blue_tinted
        and distance < 2 * tolerance
        and brightness_gain < max(8.0, tolerance * 0.25)
    ):
        return 0.0

    return _clamp((distance - tolerance) / tolerance, 0.0, 1.0)


def _key_sheet(sheet, background, tolerance):
    """Build soft alpha and keyed column bounds for the whole source sheet."""
    width, height = sheet.size
    alpha = Image.new("L", sheet.size)
    alpha_values = bytearray(width * height)
    column_top = [height] * width
    column_bottom = [-1] * width
    pixels = sheet.load()

    for y in range(height):
        row = y * width
        for x in range(width):
            soft = _soft_alpha(pixels[x, y], background, tolerance)
            value = int(round(soft * 255))
            if soft > 0.0 and value == 0:
                value = 1
            alpha_values[row + x] = value
            if value:
                column_top[x] = min(column_top[x], y)
                column_bottom[x] = y

    alpha.putdata(alpha_values)
    return alpha, column_top, column_bottom


def _frame_boxes(column_top, column_bottom, min_gap):
    """Find and merge foreground column runs, returning inclusive source boxes."""
    foreground = [bottom >= 0 for bottom in column_bottom]
    runs = []
    run_start = None
    for x, is_foreground in enumerate(foreground):
        if is_foreground and run_start is None:
            run_start = x
        elif not is_foreground and run_start is not None:
            runs.append((run_start, x - 1))
            run_start = None
    if run_start is not None:
        runs.append((run_start, len(foreground) - 1))

    merged = []
    for left, right in runs:
        if merged and left - merged[-1][1] - 1 < min_gap:
            merged[-1] = (merged[-1][0], right)
        else:
            merged.append((left, right))

    boxes = []
    for left, right in merged:
        top = min(column_top[left:right + 1])
        bottom = max(column_bottom[left:right + 1])
        boxes.append((left, top, right, bottom))
    if not boxes:
        raise ValueError("No keyed foreground found in the source sheet")
    return boxes


def _rgba_crop(sheet, alpha, box):
    left, top, right, bottom = box
    rgb = sheet.crop((left, top, right + 1, bottom + 1))
    mask = alpha.crop((left, top, right + 1, bottom + 1))
    return Image.merge("RGBA", (*rgb.split(), mask))


def _downscale(frame, scale, frame_index, background, tolerance):
    width = max(1, int(round(frame.width / scale)))
    height = max(1, int(round(frame.height / scale)))
    if width > CELL or height > CELL:
        raise ValueError(
            f"Frame {frame_index} downscaled size {width}x{height} exceeds "
            f"{CELL}x{CELL}; use a larger --scale"
        )

    source_pixels = list(frame.getdata())
    premultiplied_pixels = [
        (
            int(round(red * alpha_value / 255)),
            int(round(green * alpha_value / 255)),
            int(round(blue * alpha_value / 255)),
        )
        for red, green, blue, alpha_value in source_pixels
    ]
    premultiplied = Image.new("RGB", frame.size)
    premultiplied.putdata(premultiplied_pixels)
    small_colour = premultiplied.resize((width, height), _LANCZOS)
    small_alpha = frame.getchannel("A").resize((width, height), _LANCZOS)

    output_pixels = []
    for (red, green, blue), alpha_value in zip(
        small_colour.getdata(), small_alpha.getdata()
    ):
        alpha_value = int(alpha_value)
        if alpha_value < 128:
            output_pixels.append((0, 0, 0, 0))
            continue

        red = _clamp(int(round(red * 255 / alpha_value)), 0, 255)
        green = _clamp(int(round(green * 255 / alpha_value)), 0, 255)
        blue = _clamp(int(round(blue * 255 / alpha_value)), 0, 255)
        colour = (red, green, blue)
        if _colour_distance(colour, background) <= tolerance:
            output_pixels.append((0, 0, 0, 0))
        else:
            output_pixels.append((*colour, 255))

    output = Image.new("RGBA", (width, height))
    output.putdata(output_pixels)
    return output


def _quantize_frames(frames, colors, background, tolerance):
    """Use one MEDIANCUT palette for every frame and return RGBA frames."""
    opaque_pixels = [
        pixel[:3]
        for frame in frames
        for pixel in frame.getdata()
        if pixel[3] == 255
    ]
    if not opaque_pixels:
        raise ValueError("No opaque foreground remains after downscaling")

    palette_source = Image.new("RGB", (len(opaque_pixels), 1))
    palette_source.putdata(opaque_pixels)
    quantized = palette_source.quantize(
        colors=colors,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )
    assignments = list(quantized.getdata())
    raw_palette = quantized.getpalette()
    used_indices = sorted(set(assignments))
    palette = {
        index: tuple(raw_palette[index * 3:index * 3 + 3])
        for index in used_indices
    }

    colour_to_index = {}
    for colour, index in zip(opaque_pixels, assignments):
        colour_to_index.setdefault(colour, index)
    valid_indices = [
        index
        for index in used_indices
        if _colour_distance(palette[index], background) > tolerance
    ]

    output_frames = []
    output_colours = set()
    for frame in frames:
        output_pixels = []
        for red, green, blue, alpha_value in frame.getdata():
            if alpha_value != 255:
                output_pixels.append((0, 0, 0, 0))
                continue

            source_colour = (red, green, blue)
            index = colour_to_index[source_colour]
            if index not in valid_indices:
                if not valid_indices:
                    output_pixels.append((0, 0, 0, 0))
                    continue
                index = min(
                    valid_indices,
                    key=lambda candidate: sum(
                        (palette[candidate][channel] - source_colour[channel]) ** 2
                        for channel in range(3)
                    ),
                )
            colour = palette[index]
            if _colour_distance(colour, background) <= tolerance:
                output_pixels.append((0, 0, 0, 0))
            else:
                output_pixels.append((*colour, 255))
                output_colours.add(colour)

        output = Image.new("RGBA", frame.size)
        output.putdata(output_pixels)
        output_frames.append(output)

    if not output_colours:
        raise ValueError("No opaque foreground remains after palette keying")
    return output_frames, sorted(output_colours)


def _place_frames(frames):
    strip = Image.new("RGBA", (CELL * len(frames), CELL), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        x = index * CELL + CELL // 2 - frame.width // 2
        y = CELL // 2 - frame.height // 2
        strip.alpha_composite(frame, (x, y))
    return strip


def _rolling_preview_row(strip, rolling_path):
    with Image.open(rolling_path) as opened:
        rolling = opened.convert("RGBA")
    if rolling.width < CELL * ROLLING_FRAMES or rolling.height < CELL:
        raise ValueError(
            f"--rolling must contain at least {ROLLING_FRAMES} 128x128 frames"
        )

    row = Image.new("RGBA", strip.size, (0, 0, 0, 0))
    for index in range(strip.width // CELL):
        base = rolling.crop(
            (index % ROLLING_FRAMES * CELL, 0,
             (index % ROLLING_FRAMES + 1) * CELL, CELL)
        )
        row.alpha_composite(base, (index * CELL, 0))
        crescent = strip.crop(
            (index * CELL, 0, (index + 1) * CELL, CELL)
        )
        row.alpha_composite(crescent, (index * CELL, 0))
    return row


def _make_preview(strip, rolling_path):
    scale = 2
    label_height = 18
    row_gap = 6
    rows = [("crescent frames", strip)]
    if rolling_path is not None:
        rows.append(("crescent over rolling slash", _rolling_preview_row(strip, rolling_path)))

    row_height = label_height + CELL * scale + row_gap
    preview = Image.new(
        "RGBA",
        (strip.width * scale, row_height * len(rows)),
        (40, 40, 48, 255),
    )
    draw = ImageDraw.Draw(preview)
    for row_index, (title, row) in enumerate(rows):
        top = row_index * row_height
        draw.text((4, top + 1), title, fill=(255, 255, 255, 255))
        image_top = top + label_height
        enlarged = row.resize((row.width * scale, row.height * scale), Image.NEAREST)
        preview.alpha_composite(enlarged, (0, image_top))
        for frame_index in range(strip.width // CELL):
            left = frame_index * CELL * scale
            draw.rectangle(
                (left, image_top, left + CELL * scale - 1,
                 image_top + CELL * scale - 1),
                outline=(100, 100, 120, 255),
            )
            draw.text(
                (left + 3, image_top + 2),
                str(frame_index),
                fill=(220, 220, 220, 255),
            )
    return preview


def _print_report(background, boxes, frames, palette):
    print(f"Background estimate: {background}")
    print(f"Frames: {len(boxes)}")
    for index, (box, frame) in enumerate(zip(boxes, frames)):
        print(
            f"Frame {index}: box={box}, "
            f"downscaled size={frame.width}x{frame.height}"
        )
    print(f"Palette colours: {palette}")


def extract_crescent(
    sheet_path,
    out,
    preview,
    scale=4,
    colors=8,
    key_tolerance=60.0,
    min_gap=6,
    rolling=None,
    report=False,
):
    """Extract, palette, place, save, and preview the crescent frames."""
    if scale <= 0:
        raise ValueError("--scale must be greater than zero")
    if colors < 1 or colors > 256:
        raise ValueError("--colors must be between 1 and 256")
    if key_tolerance <= 0:
        raise ValueError("--key-tolerance must be greater than zero")
    if min_gap < 0:
        raise ValueError("--min-gap must not be negative")

    with Image.open(sheet_path) as opened:
        sheet = opened.convert("RGB")
    background = _border_median(sheet)
    alpha, column_top, column_bottom = _key_sheet(
        sheet, background, key_tolerance
    )
    boxes = _frame_boxes(column_top, column_bottom, min_gap)
    downscaled = [
        _downscale(
            _rgba_crop(sheet, alpha, box),
            scale,
            index,
            background,
            key_tolerance,
        )
        for index, box in enumerate(boxes)
    ]
    frames, palette = _quantize_frames(
        downscaled, colors, background, key_tolerance
    )
    strip = _place_frames(frames)
    preview_image = _make_preview(strip, rolling)

    out = Path(out)
    preview = Path(preview)
    out.parent.mkdir(parents=True, exist_ok=True)
    preview.parent.mkdir(parents=True, exist_ok=True)
    strip.save(out)
    preview_image.save(preview)

    if report:
        _print_report(background, boxes, frames, palette)
    return {
        "background": background,
        "boxes": boxes,
        "sizes": [frame.size for frame in frames],
        "palette": palette,
    }


def _positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return number


def _nonnegative_int(value):
    number = int(value)
    if number < 0:
        raise argparse.ArgumentTypeError("must not be negative")
    return number


def _positive_float(value):
    number = float(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sheet", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--preview", type=Path, required=True)
    parser.add_argument("--rolling", type=Path)
    parser.add_argument("--scale", type=_positive_int, default=4)
    parser.add_argument("--colors", type=_positive_int, default=8)
    parser.add_argument("--key-tolerance", type=_positive_float, default=60.0)
    parser.add_argument("--min-gap", type=_nonnegative_int, default=6)
    parser.add_argument("--report", action="store_true")
    args = parser.parse_args()

    try:
        extract_crescent(
            args.sheet,
            args.out,
            args.preview,
            scale=args.scale,
            colors=args.colors,
            key_tolerance=args.key_tolerance,
            min_gap=args.min_gap,
            rolling=args.rolling,
            report=args.report,
        )
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()

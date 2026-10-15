#!/usr/bin/env python3
"""Extract frames from the "Zero (X3, Expanded)" custom sheet from The
Spriters Resource, Mega Man X Customs; sprite edit by HansungKee.

See assets/saber-zero/CREDITS.md for source credit.
"""
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw


def extract_specials(sheet_path, boxes_path, out, preview):
    with boxes_path.open(encoding="utf-8") as boxes_file:
        config = json.load(boxes_file)

    box_bg = {tuple(color) for color in config["box_background_colours"]}
    silhouette = tuple(config["silhouette_colour"])
    cell = config["cell"]
    foot_y = config["foot_y"]
    moves = config["moves"]
    sheet = Image.open(sheet_path).convert("RGB")

    def layer(box, keep_silhouette):
        """Box content as RGBA with box background (and effect silhouette) cleared."""
        x0, y0, x1, y1 = box
        src = sheet.crop((x0, y0, x1 + 1, y1 + 1))
        out_layer = Image.new("RGBA", src.size, (0, 0, 0, 0))
        sil = Image.new("1", src.size, 0)
        for y in range(src.height):
            for x in range(src.width):
                pixel = src.getpixel((x, y))
                if pixel == silhouette:
                    sil.putpixel((x, y), 1)
                    if keep_silhouette:
                        out_layer.putpixel((x, y), pixel + (255,))
                elif pixel not in box_bg:
                    out_layer.putpixel((x, y), pixel + (255,))
        return out_layer, sil

    def mask_of(img):
        return {(x, y) for y in range(img.height) for x in range(img.width) if img.getpixel((x, y))[3]}

    def best_offset(sil, zero_masks):
        """Offset (dx, dy) of the effect box relative to a Zero box that best
        overlays the effect's silhouette on one of the move's Zero frames."""
        silhouette_pixels = {
            (x, y)
            for y in range(sil.height)
            for x in range(sil.width)
            if sil.getpixel((x, y))
        }
        best = (-1.0, 0, 0, 0)
        for zi, zero_mask in enumerate(zero_masks):
            for dy in range(-16, 17):
                for dx in range(-16, 17):
                    moved = {(x + dx, y + dy) for x, y in silhouette_pixels}
                    inter = len(moved & zero_mask)
                    union = len(moved | zero_mask) or 1
                    score = inter / union
                    if score > best[0]:
                        best = (score, dx, dy, zi)
        return best

    report = []
    preview_rows = []
    for name, spec in moves.items():
        zero = [layer(box, False)[0] for box in spec["zero"]]
        fx_layers = [layer(box, False) for box in spec["fx"]]
        zero_masks = [mask_of(frame) for frame in zero]

        # Anchor the move on its first Zero frame: bbox centre -> x 64, feet -> foot_y.
        bx0, by0, bx1, by1 = zero[0].getbbox()
        ox = cell // 2 - (bx0 + bx1) // 2
        oy = foot_y - by1

        zero_strip = Image.new("RGBA", (cell * len(zero), cell), (0, 0, 0, 0))
        for i, frame in enumerate(zero):
            zero_strip.alpha_composite(frame, (i * cell + ox, oy))

        fx_strip = Image.new("RGBA", (cell * len(fx_layers), cell), (0, 0, 0, 0))
        fx_match = []
        for i, (fx, sil) in enumerate(fx_layers):
            score, dx, dy, zi = best_offset(sil, zero_masks)
            fx_match.append((zi, dx, dy, round(score, 2)))
            fx_strip.alpha_composite(fx, (i * cell + ox + dx, oy + dy))
            # Cells must not clip the effect.
        zero_strip.save(out / f"Special_{name}.png")
        fx_strip.save(out / f"Special_{name}_FX.png")
        report.append((name, len(zero), fx_match))
        preview_rows.append((name, zero_strip, fx_strip, fx_match))

    # Preview: per move, Zero row, FX row, and FX composited on its matched Zero frame.
    scale = 2
    label_h = 14
    cols = max(max(z.width, f.width) for _, z, f, _ in preview_rows) // cell
    pw = cols * cell * scale
    ph = sum((cell * 3 * scale) + label_h * 3 for _ in preview_rows)
    prev = Image.new("RGBA", (pw, ph), (40, 40, 48, 255))
    draw = ImageDraw.Draw(prev)
    y = 0
    for name, zero_strip, fx_strip, match in preview_rows:
        comp = Image.new("RGBA", fx_strip.size, (0, 0, 0, 0))
        for i, (zi, _, _, _) in enumerate(match):
            comp.alpha_composite(
                zero_strip.crop((zi * cell, 0, zi * cell + cell, cell)),
                (i * cell, 0),
            )
            comp.alpha_composite(
                fx_strip.crop((i * cell, 0, i * cell + cell, cell)),
                (i * cell, 0),
            )
        for title, strip in (
            (f"{name}: Zero frames", zero_strip),
            (f"{name}: effect frames", fx_strip),
            (f"{name}: effect over matched Zero frame", comp),
        ):
            draw.text((4, y + 1), title, fill=(255, 255, 255, 255))
            y += label_h
            for c in range(strip.width // cell):
                draw.rectangle(
                    (c * cell * scale, y, (c + 1) * cell * scale - 1, y + cell * scale - 1),
                    outline=(90, 90, 110, 255),
                )
                draw.text((c * cell * scale + 3, y + 2), str(c), fill=(200, 200, 200, 255))
            prev.alpha_composite(
                strip.resize((strip.width * scale, cell * scale), Image.NEAREST),
                (0, y),
            )
            y += cell * scale
    prev.save(preview)
    for result in report:
        print(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sheet", type=Path)
    parser.add_argument(
        "--boxes",
        type=Path,
        default=Path(__file__).with_name("saber_specials_boxes.json"),
    )
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--preview", type=Path, required=True)
    args = parser.parse_args()
    extract_specials(args.sheet, args.boxes, args.out, args.preview)


if __name__ == "__main__":
    main()

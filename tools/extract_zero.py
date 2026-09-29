#!/usr/bin/env python3
"""Extract vanilla X3 Zero's original-size poses from a user-supplied USA ROM.

No ROM data is embedded in this tool. Tables were verified against the USA ROM:
04:A63C (body selection), 04:BCA2 (graphics transfers), 01:805B (palettes).
The output is a local, ROM-derived cache; do not distribute it.
"""
import argparse
import hashlib
from pathlib import Path
import struct

SHA256 = '65b03268afac296330e8ff8d60dd0825879e13ed658b37713c034a3bd074f1d7'
WIDTH, HEIGHT, ORIGIN_X, ORIGIN_Y = 128, 128, 64, 64
GROUPS = ((0x4a, 117, 0x85d6a8), (0x4b, 21, 0x85db47), (0x50, 14, 0x85e6e0))


class Rom:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        if len(self.data) % 32768 == 512:
            self.data = self.data[512:]
        if hashlib.sha256(self.data).hexdigest() != SHA256:
            raise ValueError('Expected the original Mega Man X3 USA ROM')

    def read(self, address, length):
        if address & 0xffff < 0x8000:
            raise ValueError(f'Not a ROM address: {address:06x}')
        offset = ((address >> 16) & 127) * 32768 + (address & 32767)
        result = self.data[offset:offset + length]
        if len(result) != length:
            raise ValueError(f'ROM read out of bounds: {address:06x}')
        return result

    def integer(self, address, length=2):
        return int.from_bytes(self.read(address, length), 'little')


def palette(rom):
    colors = [0] * 256
    for key in (0xd0, 0xd2):
        address = 0x860000 | rom.integer(0x868180 + key)
        for _ in range(32):
            count, source, dest = struct.unpack('<BHB', rom.read(address, 4))
            if not count:
                break
            if dest + count > 256:
                raise ValueError('Palette transfer exceeds CGRAM')
            colors[dest:dest + count] = struct.unpack('<' + 'H' * count, rom.read(0x8c0000 | source, count * 2))
            address += 4
        else:
            raise ValueError('Unterminated palette list')
    # The common weapon palette used by the saber ($81:8D1E, OBJ palette 3).
    colors[176:192] = struct.unpack('<16H', rom.read(0x8cb5a0, 32))
    return colors[128:]


def common_tiles(rom):
    # X3 resource $0A ($86:F732), decompressed by $80:B730 into OBJ $6800.
    record = 0x86f732 + 0x0a * 5
    address, length = rom.integer(record, 3), rom.integer(record + 3)
    offset = ((address >> 16) & 127) * 32768 + (address & 32767)
    decoded = bytearray()
    while len(decoded) < length:
        control = rom.data[offset]
        offset += 1
        for bit in (128, 64, 32, 16, 8, 4, 2, 1):
            if len(decoded) == length:
                break
            if control & bit:
                a, b = rom.data[offset:offset + 2]
                offset += 2
                count, distance = a >> 2, ((a & 3) << 8) | b
                if not count or not 0 < distance <= len(decoded) or len(decoded) + count > length:
                    raise ValueError('Invalid common graphics backreference')
                for _ in range(count):
                    decoded.append(decoded[-distance])
            else:
                decoded.append(rom.data[offset])
                offset += 1
    if length != 4096:
        raise ValueError('Unexpected common graphics size')
    tiles = bytearray(8192)
    tiles[0x1000:0x2000] = decoded
    return tiles


def frame(rom, group, number, dma, tiles=None):
    # Several eye/hand poses have no transfer and inherit the previous CHR.
    if tiles is None:
        tiles = bytearray(8192)
    address = (dma & 0xff0000) | ((dma + rom.integer(dma + number * 2)) & 0xffff) if dma else 0
    for _ in range(32) if dma else ():
        count = rom.integer(address, 1)
        if not count:
            break
        source = rom.integer(address + 1, 3)
        target = rom.integer(address + 4)
        offset = ((target & 0x7fff) - 0x6000) * 2
        if not 0 <= offset <= len(tiles) - count * 16:
            raise ValueError(f'Invalid tile transfer {group:02x}/{number:02x}')
        tiles[offset:offset + count * 16] = rom.read(source, count * 16)
        if target & 0x8000:
            break
        address += 6
    else:
        if dma:
            raise ValueError('Unterminated tile transfer list')
    table = rom.integer(0x8d8000 + group * 3, 3)
    address = rom.integer(table + number * 3, 3)
    count = rom.integer(address, 1)
    if count > 64:
        raise ValueError('Invalid sprite piece count')
    pixels = bytearray(WIDTH * HEIGHT)
    pieces = rom.read(address + 1, count * 4)
    for piece in reversed(range(count)):
        flags, x, y, tile = struct.unpack_from('<BbbB', pieces, piece * 4)
        size = 16 if flags & 32 else 8
        for dy in range(size):
            for dx in range(size):
                tx = size - 1 - dx if flags & 64 else dx
                ty = size - 1 - dy if flags & 128 else dy
                t = (((tile >> 4) + ty // 8) & 15) * 16 + ((tile + tx // 8) & 15)
                bits = t * 32 + (ty & 7) * 2
                shift = 7 - (tx & 7)
                color = sum(((tiles[bits + (p // 2) * 16 + p % 2] >> shift) & 1) << p for p in range(4))
                if color:
                    px, py = ORIGIN_X + x + dx, ORIGIN_Y + y + dy
                    if not (0 <= px < WIDTH and 0 <= py < HEIGHT):
                        raise ValueError(f'Pose {group:02x}/{number:02x} exceeds extraction canvas at {px},{py}')
                    base = 0 if group >= 0x6f else 3 if group == 0x50 else 1
                    pixels[py * WIDTH + px] = color + (((flags >> 1) & 7) | base) * 16
    return pixels


def extract(path):
    rom = Rom(path)
    colors = palette(rom)
    # The stage's common OBJ palette (key $14). Zero's key $D2 is loaded at
    # palette 3 by $84:819F, not at palette 0 used by charge motes.
    colors[:16] = struct.unpack('<16H', rom.read(0x8cb100, 32))
    # Charge groups $70/$71 OR palette 2 until the stored saber becomes ready.
    colors[32:48] = struct.unpack('<16H', rom.read(0x8cb0e0, 32))
    poses = []
    for group, count, dma in GROUPS:
        tiles = bytearray(8192)
        poses.extend(frame(rom, group, n, dma, tiles) for n in range(count))
    bounds = bytearray(rom.read(0x86b837, 40))
    for i in range(1, 40, 4):
        bounds[i] = (bounds[i] - 8) & 255
    # Vanilla $84:DCE6 selects DMA list $5C at $86:9AB3. Its two 64-byte
    # transfers replace the four tiles of the 16x16 health badge at $6860/$6960.
    # $81:804A with X=$20/Y=$1C loads its palette from $8C:B0E0 into CGRAM $A0.
    hud = rom.read(0x2c8d20, 64) + rom.read(0x2c8de0, 64) + rom.read(0x8cb0e0, 32)
    # Group $4A's 136 sequence offsets and duration/flags/pose records, up to
    # group $4B. The host mirrors these independently of X1's gameplay flags.
    animation = rom.read(0x3fcc74, 0x474)
    # Original Zero firing-pose map and signed Y/X pairs ($81:8BA9).
    muzzle = rom.read(0x399161, 120) + rom.read(0x3991d9, 76)
    # Body palette pairs $86:B3B4: blue, purple, then green; each alternates
    # with the base palette every two frames. Green persists with stored saber.
    flash = b''.join(rom.read(a, 32) for a in (0x8caf60, 0x8caf80, 0x8ca5e0))
    # All three charge groups share the 22 one-frame poses at $3F:DA87.
    tiles = common_tiles(rom)
    particles = b''.join(frame(rom, group, n, 0, tiles) for group in (0x6f, 0x70, 0x71) for n in range(22))
    header = struct.pack('<8s6H', b'MMXZERO7', WIDTH, HEIGHT, ORIGIN_X, ORIGIN_Y, 117, 35)
    return header + struct.pack('<128H', *colors) + bounds + hud + animation + muzzle + b''.join(poses) + flash + particles, colors, poses


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--sheet', type=Path, help='Optional contact sheet (requires Pillow)')
    args = parser.parse_args()
    data, colors, poses = extract(args.rom)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    if args.sheet:
        from PIL import Image, ImageDraw
        sheet = Image.new('RGB', (8 * WIDTH, ((len(poses) + 7) // 8) * HEIGHT), '#243040')
        draw = ImageDraw.Draw(sheet)
        for i, pixels in enumerate(poses):
            x, y = i % 8 * WIDTH, i // 8 * HEIGHT
            for p, color in enumerate(pixels):
                if color:
                    c = colors[color]
                    sheet.putpixel((x + p % WIDTH, y + p // WIDTH), tuple(((c >> s) & 31) * 255 // 31 for s in (0, 5, 10)))
            group, number = (0x4a, i) if i < 117 else (0x4b, i - 117) if i < 138 else (0x50, i - 138)
            draw.text((x + 3, y + 3), f'{group:02X}:{number:02X}', fill='white')
        sheet.save(args.sheet)
    print(f'Extracted {len(poses)} original-size Zero poses to {args.output} ({len(data)} bytes)')


if __name__ == '__main__':
    main()

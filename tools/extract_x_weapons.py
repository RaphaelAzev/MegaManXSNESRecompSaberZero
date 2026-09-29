#!/usr/bin/env python3
"""Extract original X2/X3 weapon art from locally supplied USA ROMs.

The descriptor contains ROM addresses, never distributed game art. Static CHR
bindings and palette addresses were measured at native weapon selection; pose
layouts and per-pose DMA lists are decoded directly from each original ROM.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


class Rom:
    def __init__(self, path, digest):
        self.data = Path(path).read_bytes()
        if len(self.data) % 32768 == 512:
            self.data = self.data[512:]
        if hashlib.sha256(self.data).hexdigest() != digest:
            raise ValueError(f'Incorrect original USA ROM: {path}')

    def at(self, address, size):
        if address & 65535 < 32768:
            raise ValueError(f'Invalid ROM address {address:06x}')
        return self.raw(((address >> 16) & 127) * 32768 + (address & 32767), size)

    def raw(self, offset, size):
        result = self.data[offset:offset + size]
        if len(result) != size:
            raise ValueError('ROM read exceeds file')
        return result

    def integer(self, address, size=2):
        return int.from_bytes(self.at(address, size), 'little')


def transfers(rom, table, pose, tiles, known):
    address = (table & 0xff0000) | ((table + rom.integer(table + pose * 2)) & 65535)
    for _ in range(64):
        count = rom.integer(address, 1)
        if not count:
            return
        source = rom.integer(address + 1, 3)
        target = rom.integer(address + 4)
        start, length = ((target & 32767) - 0x6000) * 2, count * 16
        if start < 0 or start + length > len(tiles):
            raise ValueError(f'Invalid DMA {table:06x}/{pose}: {start}/{length}')
        tiles[start:start + length] = rom.at(source, length)
        known[start:start + length] = b'\1' * length
        if target & 32768:
            return
        address += 6
    raise ValueError('Unterminated DMA list')


def bulk_transfers(rom, address, tiles, known):
    for _ in range(64):
        length = rom.integer(address)
        if length & 1:
            return
        start = (rom.integer(address + 2) - 0x6000) * 2
        source = rom.integer(address + 4, 3)
        if start < 0 or start + length > len(tiles):
            raise ValueError('Invalid weapon-selection DMA')
        tiles[start:start + length] = rom.at(source, length)
        known[start:start + length] = b'\1' * length
        address += 7
    raise ValueError('Unterminated weapon-selection DMA list')


def pose_art(rom, group, pose, tiles, known):
    table = rom.integer(0x8d8000 + group * 3, 3)
    address = rom.integer(table + pose * 3, 3)
    count = rom.integer(address, 1)
    if count > 64:
        raise ValueError('Invalid sprite layout')
    pieces = list(struct.iter_unpack('<BbbB', rom.at(address + 1, count * 4)))
    if not pieces:
        return 0, 0, 0, 0, b''
    left, top = min(p[1] for p in pieces), min(p[2] for p in pieces)
    width = max(p[1] + (16 if p[0] & 32 else 8) for p in pieces) - left
    height = max(p[2] + (16 if p[0] & 32 else 8) for p in pieces) - top
    if width > 256 or height > 256:
        raise ValueError('Sprite exceeds extraction bounds')
    pixels = bytearray(width * height)
    for flags, x, y, tile in reversed(pieces):
        if flags & 14:
            raise ValueError(f'Unexpected extra palette in {group:02x}/{pose}: {flags:02x}')
        size = 16 if flags & 32 else 8
        for dy in range(size):
            for dx in range(size):
                tx = size - 1 - dx if flags & 64 else dx
                ty = size - 1 - dy if flags & 128 else dy
                number = (((tile >> 4) + ty // 8) & 15) * 16 + ((tile + tx // 8) & 15)
                bits, shift = number * 32 + (ty & 7) * 2, 7 - (tx & 7)
                offsets = [bits, bits + 1, bits + 16, bits + 17]
                if not all(known[o] for o in offsets):
                    raise ValueError(f'Unresolved inherited CHR {group:02x}/{pose:02x} tile {number:02x}')
                color = sum(((tiles[o] >> shift) & 1) << p for p, o in enumerate(offsets))
                if color:
                    pixels[(y - top + dy) * width + x - left + dx] = color
    return left, top, width, height, bytes(pixels)


def menu_graphics(rom, game, resource=0x4c):
    # Original resource $4C; X2 $80:B25E and X3 $80:B730 decode these LZ records.
    table = 0x86fa01 if game == 2 else 0x86f732
    record = table + resource * 5
    source, length = rom.integer(record, 3), rom.integer(record + 3)
    pos = ((source >> 16) & 127) * 32768 + (source & 32767)
    result = bytearray()
    while len(result) < length:
        control = rom.raw(pos, 1)[0]
        pos += 1
        for bit in (128, 64, 32, 16, 8, 4, 2, 1):
            if len(result) == length:
                break
            if control & bit:
                a, b = rom.raw(pos, 2)
                pos += 2
                count, distance = a >> 2, ((a & 3) << 8) | b
                if not count or not distance or distance > len(result) or len(result) + count > length:
                    raise ValueError('Invalid menu graphics backreference')
                for _ in range(count):
                    result.append(result[-distance])
            else:
                result.extend(rom.raw(pos, 1))
                pos += 1
    return result


def menu_icon(rom, game, weapon, graphics):
    # Both source menus follow native weapon IDs; X3 ID 2 is Parasitic Bomb
    # and ID 7 is Frost Shield, matching their projectile groups/palettes.
    tile = 0x30 + weapon * 2 if weapon < 8 else 0x50
    offset = -0x200 if game == 2 else 0
    pixels = bytearray()
    for y in range(16):
        for x in range(16):
            number = tile + x // 8 + (y // 8) * 16
            start, shift = offset + number * 32 + (y & 7) * 2, 7 - (x & 7)
            pixels.append(sum(((graphics[start + q] >> shift) & 1) << p for p, q in enumerate((0, 1, 16, 17))))
    colors = rom.raw(0x2cee0 if game == 2 else 0x62da0, 32)
    return colors + pixels


def hud_icon(game, tiles, known):
    # Native OAM slot 7: X2 $3628, X3 $36AC. These are dedicated 16x16
    # gameplay footers uploaded by the weapon-selection bulk DMA list.
    tile = 0x28 if game == 2 else 0xac
    pixels = bytearray()
    for y in range(16):
        for x in range(16):
            start = (tile + x // 8 + y // 8 * 16) * 32 + (y & 7) * 2
            offsets = (start, start + 1, start + 16, start + 17)
            if not all(known[o] for o in offsets):
                raise ValueError('Unresolved original gameplay HUD graphics')
            pixels.append(sum(((tiles[o] >> (7 - (x & 7))) & 1) << p
                              for p, o in enumerate(offsets)))
    return pixels


def animation_group(rom, game, group):
    root = 0x2fa000 if game == 2 else 0x3f8000
    base = rom.integer(root + group * 3, 3)
    header = rom.integer(base)
    if not header or header & 1 or header > 1024:
        raise ValueError('Invalid animation sequence directory')
    extent, visited = header, set()
    pending = [rom.integer(base + i) for i in range(0, header, 2)]
    while pending:
        offset = pending.pop()
        if offset in visited:
            continue
        if offset < header or offset > 8192:
            raise ValueError('Animation sequence exceeds group bounds')
        visited.add(offset)
        duration, flags, pose = rom.at(base + offset, 3)
        if not duration or pose >= 128:
            raise ValueError('Invalid animation record')
        end = offset + 3
        if flags & 128:
            displacement = int.from_bytes(rom.at(base + end, 2), 'little', signed=True)
            pending.append(end + displacement)
            extent = max(extent, end + 2)
        else:
            pending.append(end)
            extent = max(extent, end)
    return rom.at(base, extent)


def extract(x2, x3):
    entries = json.loads((Path(__file__).parent / 'data/x_weapon_assets.json').read_text())
    roms = {game: Rom(path, next(e['sha256'] for e in entries if e['game'] == game))
            for game, path in ((2, x2), (3, x3))}
    menus = {game: menu_graphics(rom, game) for game, rom in roms.items()}
    result = bytearray(struct.pack('<8sI', b'MMXWEAP5', len(entries)))
    sheets = []
    for entry in entries:
        rom = roms[entry['game']]
        colors = rom.raw(int(entry['weapon_palette'], 16), 32)
        result.extend(struct.pack('<4B', entry['game'], entry['weapon'], len(entry['groups']), 0))
        result.extend(rom.raw(int(entry['body_palette'], 16), 32) + colors)
        result.extend(menu_icon(rom, entry['game'], entry['weapon'], menus[entry['game']]))
        base, base_known = bytearray(8192), bytearray(8192)
        if 'static_resource' in entry:
            common = menu_graphics(rom, entry['game'], entry['static_resource'])
            offset = entry['static_offset']
            if offset + len(common) > len(base):
                raise ValueError('Static source graphics exceed tile bank')
            base[offset:offset+len(common)] = common
            base_known[offset:offset+len(common)] = b'\1' * len(common)
        bulk_root = 0x869664 if entry['game'] == 2 else 0x8697ad
        bulk = 0x860000 | rom.integer(bulk_root + 0x3e + entry['weapon'] * 2)
        bulk_transfers(rom, bulk, base, base_known)
        for address in entry.get('additional_dma', []):
            bulk_transfers(rom, int(address, 16), base, base_known)
        result.extend(hud_icon(entry['game'], base, base_known))
        for group in entry['groups']:
            tiles, known = bytearray(base), bytearray(base_known)
            for pose in group.get('setup_poses', []):
                transfers(rom, int(group['dma'], 16), pose, tiles, known)
            source_group = group.get('source_group', group['group'])
            animation = animation_group(rom, entry['game'], source_group)
            result.extend(struct.pack('<HHH', group['group'], group['frames'], len(animation)))
            group_colors = rom.raw(int(group.get('palette', entry['weapon_palette']), 16), 32)
            result.extend(group_colors)
            result.extend(animation)
            for pose in range(group['frames']):
                animated = 'animated_frame_start' in group and pose >= group['animated_frame_start']
                if not animated and 'poses' in group and pose not in group['poses']:
                    result.extend(bytes(8))
                    continue
                try:
                    layout = pose
                    if animated:
                        phase, orientation = divmod(pose-group['animated_frame_start'], group['animated_layout_count'])
                        tiles, known = bytearray(base), bytearray(base_known)
                        bulk_transfers(rom, 0x860000 | rom.integer(int(group['animated_dma_table'],16)+phase*2),tiles,known)
                        layout = group['animated_layout_start']+orientation
                    elif pose not in group.get('inherited_poses', []):
                        transfers(rom, int(group['dma'], 16), pose, tiles, known)
                    left, top, width, height, pixels = pose_art(rom, source_group, layout, tiles, known)
                except ValueError as error:
                    raise ValueError(f"X{entry['game']} {entry['name']} {group['group']:02x}/{pose:02x}: {error}") from error
                result.extend(struct.pack('<hhHH', left, top, width, height) + pixels)
                sheets.append((entry['game'], entry['weapon'], group['group'], pose,
                               left, top, width, height, pixels, group_colors))
    return bytes(result), sheets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('x2_rom', type=Path)
    parser.add_argument('x3_rom', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    data, poses = extract(args.x2_rom, args.x3_rom)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f'Extracted {len(poses)} original weapon poses ({len(data)} bytes) to {args.output}')


if __name__ == '__main__':
    main()

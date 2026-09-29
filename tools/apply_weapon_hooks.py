#!/usr/bin/env python3
"""Keep extended pause-page choices separate from the native X1 inventory."""
import argparse
from pathlib import Path
import re

MARKER = '/*MMX-WEAPONS*/'
INVENTORY = {0xc792, 0xc7b5, 0xc805, 0xc826, 0xc865, 0xc886}
SELECTION = {0xc484, 0xc767, 0xce28, 0xce33}
REQUIRED = INVENTORY | SELECTION | {0xc67f, 0x819d47}


def apply(text):
    output, found, pc = [], set(), 0
    for line in text.splitlines(keepends=True):
        if MARKER in line:
            continue
        block = re.search(r'cpu_trace_block\(cpu, 0x([0-9A-Fa-f]+)\);', line)
        if block:
            pc = int(block[1], 16)
        output.append(line)
        if pc == 0x819d47 and 'cpu->coprocessor_master_cycles = cpu->master_cycles;' in line:
            output.append(f'    {MARKER} {{ extern uint8_t g_ram[0x20000]; extern void MmxWeaponsMarkShot(uint8_t *, unsigned); MmxWeaponsMarkShot(g_ram, cpu->X); }}\n')
            found.add(pc)
        if pc == 0xc67f and 'cpu->coprocessor_master_cycles = cpu->master_cycles;' in line:
            output.append(f'    {MARKER} {{ extern uint8_t g_ram[0x20000]; extern void MmxWeaponsMenuTick(uint8_t *, unsigned); MmxWeaponsMenuTick(g_ram, cpu->D); }}\n')
            found.add(pc)
        load = re.search(r'uint8 (_v\d+) = cpu_read8\(', line)
        if not load:
            continue
        selected = (pc in INVENTORY and '0x1f88' in line) or (
            pc in SELECTION and ('0x0bdb' in line or (pc == 0xce33 and '0x000a' in line)))
        if selected:
            output.append(f'    {MARKER} {{ extern uint8_t g_ram[0x20000]; extern unsigned MmxWeaponsMenuRead(uint8_t *, unsigned, unsigned, unsigned, unsigned); {load[1]} = (uint8)MmxWeaponsMenuRead(g_ram, 0x{pc:06x}, cpu->D, cpu->X, {load[1]}); }}\n')
            found.add(pc)
    return ''.join(output), found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gen-dir', type=Path, required=True)
    args = parser.parse_args()
    found = set()
    for path in args.gen_dir.glob('bank*.c'):
        text = path.read_text(encoding='utf-8')
        if not any(f'0x{pc:06X}' in text for pc in REQUIRED):
            continue
        updated, sites = apply(text)
        found |= sites
        if updated != text:
            path.write_text(updated, encoding='utf-8', newline='\n')
    if found != REQUIRED:
        raise SystemExit(f'Missing extended weapon hooks: {REQUIRED - found}')
    print(f'MMX weapons: {len(found)} menu/combat sites verified')


if __name__ == '__main__':
    main()

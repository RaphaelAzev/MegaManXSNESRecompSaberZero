#!/usr/bin/env python3
"""Keep extended pause-page choices separate from the native X1 inventory."""
import argparse
from pathlib import Path
import re

MARKER = '/*MMX-WEAPONS*/'
INVENTORY = {0xc792, 0xc7b5, 0xc805, 0xc826, 0xc865, 0xc886}
SELECTION = {0xc484, 0xc767, 0xce28, 0xce33}
ENERGY_READ = {0xd907,0xd917,0xd940,0xd94a,0xd9a4,0x81e053,0x81e062,0x81e0a0}
ENERGY_STORE = {0xd917,0xd95c,0xd9a4,0x81e0cb}
REQUIRED = INVENTORY | SELECTION | ENERGY_READ | ENERGY_STORE | {0xc67f,0x819d47,0x9ef9,0x81e169}


def apply(text):
    output, found, pc = [], set(), 0
    for line in text.splitlines(keepends=True):
        if MARKER in line:
            continue
        block = re.search(r'cpu_trace_block\(cpu, 0x([0-9A-Fa-f]+)\);', line)
        if block:
            pc = int(block[1], 16)
        energy_store = pc in ENERGY_STORE and 'cpu_write' in line and ('0x1f86' in line or '0x1f85' in line)
        if energy_store:
            value = re.search(r', (_v\d+)\);',line)[1]
            output.append(f'    {MARKER} {{ extern bool MmxWeaponsEnergyStore(unsigned, bool); if (!MmxWeaponsEnergyStore({value}, {str(pc == 0x81e0cb).lower()}))\n')
        output.append(line)
        if energy_store:
            output.append(f'    {MARKER} }}\n')
            found.add(pc)
        if pc in (0x9ef9,0x81e169) and 'cpu->coprocessor_master_cycles = cpu->master_cycles;' in line:
            call = 'extern void MmxWeaponsRefill(void); MmxWeaponsRefill();' if pc == 0x9ef9 else 'extern uint8_t g_ram[0x20000]; extern void MmxWeaponsEnergyOverflow(uint8_t *, unsigned); MmxWeaponsEnergyOverflow(g_ram, cpu->X);'
            output.append(f'    {MARKER} {{ {call} }}\n')
            found.add(pc)
        energy_load = re.search(r'uint(8|16) (_v\d+) = cpu_read',line) if pc in ENERGY_READ else None
        if energy_load:
            address = next((a for a in ('0x0bdb','0x1f85','0x1f86') if a in line),None)
            if address:
                output.append(f'    {MARKER} {{ extern unsigned MmxWeaponsEnergyRead(unsigned, unsigned); {energy_load[2]} = (uint{energy_load[1]})MmxWeaponsEnergyRead({address}, {energy_load[2]}); }}\n')
                found.add(pc)
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

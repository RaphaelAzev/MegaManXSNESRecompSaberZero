#!/usr/bin/env python3
"""Insert bounded, disabled-by-default Zero hooks in regenerated X1 code."""
import argparse
from pathlib import Path
import re

MARKER = '/*MMX-ZERO*/'
PCS = {0x81971c, 0x819793, 0x8198fc}
MUZZLE_PCS = {0x81a566, 0x81a578, 0x838b6a, 0x838d82, 0x838eb4,
              0x839518, 0x83983c, 0x839974, 0x83a3a9}
REQUIRED = PCS | MUZZLE_PCS | {0x81815c, 0x00d3e5, 0x849e73, 0x849c16, 0x848f07, 0x848eea}


def apply(text):
    lines = [line for line in text.splitlines(keepends=True) if MARKER not in line]
    output, found, pc = [], set(), 0
    for line in lines:
        block = re.search(r'cpu_trace_block\(cpu, 0x([0-9A-Fa-f]+)\);', line)
        if block:
            pc = int(block[1], 16)
        output.append(line)
        if pc in MUZZLE_PCS:
            load = re.search(r'uint8 (_v\d+) = cpu_read8.*0xbe3([9a])', line)
            if load:
                axis = int(load[2] == 'a')
                output.append(f'    {MARKER} {{ extern uint8_t g_ram[0x20000]; extern unsigned MmxZeroMuzzle(const uint8_t *, unsigned, unsigned, unsigned, unsigned); {load[1]} = (uint8)MmxZeroMuzzle(g_ram, cpu->D, cpu->X, {axis}, {load[1]}); }}\n')
                found.add(pc)
        if pc in (0x848f07, 0x848eea) and 'cpu->coprocessor_master_cycles = cpu->master_cycles;' in line:
            if pc == 0x848f07:
                output.append(f'    {MARKER} {{ extern void MmxZeroAnimationStart(unsigned, unsigned); MmxZeroAnimationStart(cpu->D, cpu->A & 255); }}\n')
            else:
                output.append(f'    {MARKER} {{ extern void MmxZeroAnimationAdvance(unsigned); MmxZeroAnimationAdvance(cpu->D); }}\n')
            found.add(pc)
        if pc == 0x849c16:
            load = re.search(r'uint16 (_v\d+) = cpu_read16\(cpu,', line) if '0x0020' in line and 'cpu->X' in line else None
            if load:
                output.append(f'    {MARKER} {{ extern uint8_t g_ram[0x20000]; extern unsigned MmxZeroHitbox(const uint8_t *, unsigned, unsigned, unsigned); {load[1]} = (uint16)MmxZeroHitbox(g_ram, cpu->D, cpu->X, {load[1]}); }}\n')
                found.add(pc)
        if pc == 0x81815c and 'cpu->coprocessor_master_cycles = cpu->master_cycles;' in line:
            output.append(f'    {MARKER} {{ extern uint8_t g_ram[0x20000]; extern void MmxZeroPlayerTick(uint8_t *); MmxZeroPlayerTick(g_ram); }}\n')
            found.add(pc)
        if pc == 0x00d3e5:
            load = re.search(r'uint8 (_v\d+) = cpu_read8\(cpu, 0x00, \(uint16\)\(cpu->D \+ 0x0000\)\);', line)
            if load:
                output.append(f'    {MARKER} {{ extern uint8_t g_ram[0x20000]; extern unsigned MmxZeroWeaponTick(uint8_t *, unsigned, unsigned); {load[1]} = (uint8)MmxZeroWeaponTick(g_ram, cpu->D, {load[1]}); }}\n')
                found.add(pc)
        # The ordinary damage table still decides immunity and reflection.
        # Override only the final subtraction in the positive-damage path.
        if pc == 0x849e6e and 'uint8 ' in line and 'cpu_read8' in line and '0xef37' in line:
            variable = re.search(r'uint8 (_v\d+)', line)[1]
            output.append(f'    {MARKER} {{ extern uint8_t g_ram[0x20000]; extern unsigned MmxZeroDamage(uint8_t *, unsigned, unsigned, unsigned); {variable} = (uint8)MmxZeroDamage(g_ram, cpu->D, cpu->X, {variable}); }}\n')
            found.add(0x849e73)
        if pc in PCS:
            load = re.search(r'uint8 (_v\d+) = cpu_read8\(cpu, cpu->DB, \(uint16\)\(0x1f99\)\);', line)
            if load:
                output.append(f'    {MARKER} {{ extern unsigned MmxZeroUpgradeBits(unsigned, unsigned); {load[1]} = (uint8)MmxZeroUpgradeBits(0x{pc:06x}, {load[1]}); }}\n')
                found.add(pc)
    return ''.join(output), found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gen-dir', type=Path, required=True)
    args = parser.parse_args()
    found = set()
    for path in args.gen_dir.glob('*.c'):
        text = path.read_text(encoding='utf-8')
        if not any(f'0x{pc:06X}' in text for pc in REQUIRED | {0x849e6e}):
            continue
        updated, sites = apply(text)
        found |= sites
        if updated != text:
            path.write_text(updated, encoding='utf-8', newline='\n')
    if found != REQUIRED:
        raise SystemExit(f'Missing Zero hooks: {REQUIRED - found}')
    print(f'MMX Zero: {len(found)} capability/combat sites verified')


if __name__ == '__main__':
    main()

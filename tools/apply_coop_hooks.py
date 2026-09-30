#!/usr/bin/env python3
"""Keep the co-op controller boundary authoritative under generated dispatch."""
import argparse
from pathlib import Path
import re

MARKER = '/*MMX-COOP*/'
TARGETS = {0x01812e, 0x048fca, 0x0280b4, 0xd2bd, 0xd3dd, 0xd3fa, 0xd43a, 0xd457,
           0x049b03, 0x049b43, 0x019d67, 0xde9d, 0xdebc, 0xe543}


def apply(text):
    output, found = [], set()
    for line in text.splitlines(keepends=True):
        if MARKER in line:
            continue
        output.append(line)
        entry = re.match(r'RecompReturn bank_([0-9A-Fa-f]{2})_([0-9A-Fa-f]{4})_M[01]X[01]\(CpuState \*cpu\) \{', line)
        pc = int(entry[1] + entry[2],16) if entry else 0
        if (pc & 0x7fffff) in TARGETS:
            # Native JSL already owns a guest return frame. This uses the
            # bridge's existing paired/dispatch ABI, with no synthetic frame.
            output.append(f'  {MARKER} {{ extern bool MmxCoopEnabled(void); if (MmxCoopEnabled()) return interp_tier_dispatch_bank_miss(cpu, 0x{pc:06x}u, cpu->S, cpu->host_return_valid); }}\n')
            found.add(pc & 0x7fffff)
    return ''.join(output), found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gen-dir', type=Path, required=True)
    args = parser.parse_args()
    found = set()
    for path in args.gen_dir.glob('*.c'):
        text = path.read_text(encoding='utf-8')
        if not any(f'_{pc & 65535:04X}_' in text for pc in TARGETS):
            continue
        updated, count = apply(text)
        found |= count
        if updated != text:
            path.write_text(updated, encoding='utf-8', newline='\n')
    if TARGETS - found:
        raise SystemExit(f'Missing co-op native entries: {TARGETS - found}')
    print(f'MMX co-op: {len(found)} native routine boundaries verified')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Keep the co-op controller boundary authoritative under generated dispatch."""
import argparse
from pathlib import Path
import re

MARKER = '/*MMX-COOP*/'


def apply(text):
    output, found = [], 0
    for line in text.splitlines(keepends=True):
        if MARKER in line:
            continue
        output.append(line)
        if re.match(r'RecompReturn bank_81_812E_M[01]X[01]\(CpuState \*cpu\) \{', line):
            # Native JSL already owns a guest return frame. This uses the
            # bridge's existing paired/dispatch ABI, with no synthetic frame.
            output.append(f'  {MARKER} {{ extern bool MmxCoopEnabled(void); if (MmxCoopEnabled()) return interp_tier_dispatch_bank_miss(cpu, 0x81812eu, cpu->S, cpu->host_return_valid); }}\n')
            found += 1
    return ''.join(output), found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gen-dir', type=Path, required=True)
    args = parser.parse_args()
    found = 0
    for path in args.gen_dir.glob('*.c'):
        text = path.read_text(encoding='utf-8')
        if 'bank_81_812E_' not in text:
            continue
        updated, count = apply(text)
        found += count
        if updated != text:
            path.write_text(updated, encoding='utf-8', newline='\n')
    if not found:
        raise SystemExit('Missing co-op player controller entry')
    print(f'MMX co-op: {found} native controller entry variants verified')


if __name__ == '__main__':
    main()

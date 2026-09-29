"""Optional native/reference parity check; supply executable and your X2/X3 ROMs."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import extract_zero
import extract_x_weapons

exe, x2, x3 = (str(Path(p).resolve()) for p in sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='mmx-source-check-') as temporary:
    root = Path(temporary)
    def run(rom, game, zero, name):
        path = root / name
        subprocess.run([exe, str(rom), str(game), str(zero), str(path)], check=True)
        return path.read_bytes()

    reference_zero = extract_zero.extract(x3)[0]
    assert run(x3, 3, 1, 'zero.bin') == reference_zero
    a, b = run(x2, 2, 0, 'x2.bin'), run(x3, 3, 0, 'x3.bin')
    combined = a[:8] + struct.pack('<I', 16) + a[12:] + b[12:]
    assert combined == extract_x_weapons.extract(x2, x3)[0]
    source = Path(x3).read_bytes()
    if len(source) % 32768 == 512:
        source = source[512:]
    headered = root / 'header.smc'
    headered.write_bytes(bytes(512) + source)
    assert run(headered, 3, 1, 'header.bin') == reference_zero
    # Failed validation must not overwrite a previous valid cache.
    result = subprocess.run([exe, x2, '3', '1', str(root / 'zero.bin')])
    assert result.returncode != 0 and (root / 'zero.bin').read_bytes() == reference_zero
print('Native extraction parity, copier header, wrong ROM and cache preservation passed')

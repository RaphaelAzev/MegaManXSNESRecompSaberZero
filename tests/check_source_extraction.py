"""Optional Zero native/reference parity check; provide extractor EXE and X3 ROM."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import extract_zero

exe, rom = (str(Path(p).resolve()) for p in sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='mmx-zero-source-') as temporary:
    root = Path(temporary)
    output = root / 'zero.bin'
    def run(source):
        return subprocess.run([exe, str(source), '3', '1', str(output)])
    reference = extract_zero.extract(rom)[0]
    assert run(rom).returncode == 0 and output.read_bytes() == reference
    source = Path(rom).read_bytes()
    if len(source) % 32768 == 512:
        source = source[512:]
    headered = root / 'header.smc'
    headered.write_bytes(bytes(512) + source)
    assert run(headered).returncode == 0 and output.read_bytes() == reference
    wrong = root / 'wrong.sfc'
    wrong.write_bytes(bytes(len(source)))
    assert run(wrong).returncode != 0 and output.read_bytes() == reference
print('Zero extraction parity, copier header and invalid-ROM preservation passed')

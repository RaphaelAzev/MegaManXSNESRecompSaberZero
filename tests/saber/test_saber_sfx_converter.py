"""Exercise the converter with synthetic OGG and PCM WAV streams."""

from __future__ import annotations

import argparse
import struct
import subprocess
import tempfile
from pathlib import Path


class _LsbBitWriter:
    """Write the LSB-first fields used by the Vorbis I setup packet."""

    def __init__(self) -> None:
        self._data = bytearray()
        self._bits = 0

    def write(self, value: int, width: int) -> None:
        if width < 0 or value < 0 or value >= (1 << width):
            raise ValueError("bit field does not fit")
        for bit in range(width):
            if not self._bits % 8:
                self._data.append(0)
            self._data[-1] |= ((value >> bit) & 1) << (self._bits % 8)
            self._bits += 1

    def finish(self) -> bytes:
        return bytes(self._data)


def _authored_setup() -> bytes:
    """Build a tiny, silent Vorbis I setup packet from specification fields."""
    bits = _LsbBitWriter()
    bits.write(0, 8)  # one codebook
    bits.write(0x564342, 24)  # "VCB" sync, emitted LSB-first
    bits.write(1, 16)  # dimensions
    bits.write(1, 24)  # entries
    bits.write(1, 1)  # ordered codeword lengths
    bits.write(0, 5)  # one-bit codeword length minus one
    bits.write(1, 1)  # one entry at that length
    bits.write(0, 4)  # no lookup table; the book is used as a classbook

    bits.write(0, 6)  # one time-domain configuration
    bits.write(0, 16)

    bits.write(0, 6)  # one floor
    bits.write(1, 16)  # floor type 1
    bits.write(1, 5)  # one floor partition
    bits.write(0, 4)  # partition class 0
    bits.write(0, 3)  # one value in class 0
    bits.write(0, 2)  # no class subclasses
    bits.write(0, 8)  # no subclass book
    bits.write(0, 2)  # multiplier 1
    bits.write(4, 4)  # two floor posts in a 16-point range
    bits.write(8, 4)  # the partition's interior post

    bits.write(0, 6)  # one residue
    bits.write(0, 16)  # residue type 0
    bits.write(0, 24)  # begin
    bits.write(64, 24)  # end
    bits.write(0, 24)  # partition size 1 minus one
    bits.write(0, 6)  # one classification
    bits.write(0, 8)  # the tiny codebook is the classbook
    bits.write(0, 3)  # no residue books on the low passes
    bits.write(0, 1)  # no residue books on the high passes

    bits.write(0, 6)  # one mapping
    bits.write(0, 16)  # mapping type 0
    bits.write(0, 1)  # one submap
    bits.write(0, 1)  # no channel coupling
    bits.write(0, 2)  # reserved
    bits.write(0, 8)  # time configuration
    bits.write(0, 8)  # floor index
    bits.write(0, 8)  # residue index

    bits.write(0, 6)  # one mode
    bits.write(0, 1)  # short block
    bits.write(0, 16)  # window type
    bits.write(0, 16)  # transform type
    bits.write(0, 8)  # mapping index
    bits.write(1, 1)  # setup framing flag
    return b"\x05vorbis" + bits.finish()


def _ogg_crc(page: bytes) -> int:
    crc = 0
    for byte in page:
        crc ^= byte << 24
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else (crc << 1) & 0xFFFFFFFF
    return crc


def _page(flags: int, granule: int, sequence: int, packet: bytes) -> bytes:
    lacing = []
    remaining = len(packet)
    while remaining >= 255:
        lacing.append(255)
        remaining -= 255
    lacing.append(remaining)
    header = bytearray(
        b"OggS"
        + bytes([0, flags])
        + struct.pack("<QII", granule, 1, sequence)
        + b"\0\0\0\0"
        + bytes([len(lacing)])
        + bytes(lacing)
    )
    page = header + packet
    struct.pack_into("<I", page, 22, _ogg_crc(page))
    return bytes(page)


def _synthetic_ogg() -> bytes:
    ident = (
        b"\x01vorbis"
        + struct.pack("<I", 0)
        + bytes([2])
        + struct.pack("<I", 48000)
        + struct.pack("<iii", 0, 0, 0)
        + bytes([0xB8, 1])
    )
    comment = b"\x03vorbis" + struct.pack("<II", 0, 0)
    setup = _authored_setup()
    pages = [_page(2, 0, 0, ident), _page(0, 0, 1, comment), _page(0, 0, 2, setup)]
    for index in range(8):
        pages.append(_page(4 if index == 7 else 0, 256 * (index + 1), 3 + index, b"\0"))
    return b"".join(pages)


def _synthetic_wav() -> bytes:
    samples = (0, 1200, -1200, 2400, -2400, 600, -600, 0) * 8
    payload = struct.pack("<{}h".format(len(samples)), *samples)
    junk = b"JUNK" + struct.pack("<I", 3) + b"abc\0"
    fmt = struct.pack("<4sIHHIIHH", b"fmt ", 16, 1, 1, 16000,
                      16000 * 2, 2, 16)
    data = b"data" + struct.pack("<I", len(payload)) + payload
    body = b"WAVE" + junk + fmt + data
    return b"RIFF" + struct.pack("<I", len(body)) + body


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--converter", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(dir=Path.cwd()) as temporary:
        root = Path(temporary)
        source = root / "source"
        source.mkdir()
        fixture = _synthetic_ogg()
        for index in range(1, 4):
            (source / f"saber_{index}.ogg").write_bytes(fixture)
        (source / "crescent_slash.wav").write_bytes(_synthetic_wav())
        output_a = root / "a.bin"
        output_b = root / "b.bin"
        preview = root / "preview"
        preview.mkdir()
        command = [str(args.converter), "--source-dir", str(source), "--out"]
        first = subprocess.run(
            command + [str(output_a), "--preview-dir", str(preview)], cwd=root
        )
        assert first.returncode == 0 and output_a.is_file()
        second = subprocess.run(command + [str(output_b)], cwd=root)
        assert second.returncode == 0 and output_b.read_bytes() == output_a.read_bytes()
        sidecar = output_a.read_bytes()
        assert sidecar[:8] == b"MMXSFX2\0"
        assert struct.unpack_from("<H", sidecar, 8)[0] == 2
        # Mutation red: remove two entries from kClipSpecs or restore count 3.
        assert struct.unpack_from("<H", sidecar, 12)[0] == 5
        frames = []
        payloads = []
        cursor = 24 + 5 * 20
        for index in range(5):
            record = 24 + index * 20
            assert struct.unpack_from("<I", sidecar, record)[0] == 1
            assert struct.unpack_from("<I", sidecar, record + 4)[0] == 32000
            frame_count = struct.unpack_from("<I", sidecar, record + 8)[0]
            byte_length = struct.unpack_from("<I", sidecar, record + 12)[0]
            assert frame_count > 0 and byte_length == frame_count * 2
            frames.append(frame_count)
            payloads.append(sidecar[cursor : cursor + byte_length])
            cursor += byte_length
        # Mutation red: route OGG clips through WAV or alter the old OGG path.
        assert frames[:3] == [598, 598, 598]
        assert frames[3] > 0
        # Mutation red: remove UPPERCUT_PITCH from effective_sample_rate().
        assert frames[4] < frames[0]
        assert abs(frames[4] - round(frames[0] * 100 / 126)) <= 1
        preview_names = (
            "clip0_saber_1.wav",
            "clip1_saber_2.wav",
            "clip2_saber_3.wav",
            "clip3_crescent_slash.wav",
            "clip4_saber_1.wav",
        )
        # Mutation red: remove write_previews() or change its PCM source.
        for name, payload in zip(preview_names, payloads):
            preview_bytes = (preview / name).read_bytes()
            assert preview_bytes[:4] == b"RIFF"
            assert preview_bytes[8:12] == b"WAVE"
            assert struct.unpack_from("<H", preview_bytes, 20)[0] == 1
            assert struct.unpack_from("<H", preview_bytes, 22)[0] == 1
            assert struct.unpack_from("<I", preview_bytes, 24)[0] == 32000
            assert struct.unpack_from("<H", preview_bytes, 34)[0] == 16
            assert struct.unpack_from("<I", preview_bytes, 40)[0] == len(payload)
            assert preview_bytes[44:] == payload
        corrupt = root / "corrupt"
        corrupt.mkdir()
        (corrupt / "saber_1.ogg").write_bytes(fixture[:60])
        for index in (2, 3):
            (corrupt / f"saber_{index}.ogg").write_bytes(fixture)
        (corrupt / "crescent_slash.wav").write_bytes(_synthetic_wav())
        rejected = subprocess.run(
            [str(args.converter), "--source-dir", str(corrupt), "--out", str(root / "bad.bin")],
            cwd=root,
        )
        assert rejected.returncode != 0 and not (root / "bad.bin").exists()
        malformed_wav = root / "malformed-wav"
        malformed_wav.mkdir()
        for index in range(1, 4):
            (malformed_wav / f"saber_{index}.ogg").write_bytes(fixture)
        (malformed_wav / "crescent_slash.wav").write_bytes(_synthetic_wav()[:20])
        rejected_wav = subprocess.run(
            [str(args.converter), "--source-dir", str(malformed_wav), "--out", str(root / "bad-wav.bin")],
            cwd=root, capture_output=True, text=True,
        )
        # Mutation red: remove strict decode_wav() bounds and format checks.
        assert rejected_wav.returncode != 0 and not (root / "bad-wav.bin").exists()
        assert "WAV" in rejected_wav.stderr
    print("PASS Saber SFX converter decodes five clips, previews rendered PCM, pitches uppercut, and rejects corrupt OGG/WAV")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

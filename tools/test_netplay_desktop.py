"""Cold-boot two real desktop hosts; check pacing and offline-save isolation."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe", "rom", "x3", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--port", type=int, default=18010)
    args = parser.parse_args()
    for name in ("exe", "rom", "x3"):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    root = args.output.resolve()
    catalog = Path(__file__).resolve().parents[1] / "mods/preloaded/packages"
    processes = []
    try:
        for seat in range(2):
            peer = root / f"peer{seat}"
            peer.mkdir(parents=True, exist_ok=True)
            # Never overwrite an existing test/user installation or save.
            if (peer / args.exe.name).exists() or (peer / "saves").exists():
                raise RuntimeError(f"Use a fresh output directory: {peer}")
            exe = peer / args.exe.name
            shutil.copy2(args.exe, exe)
            mods = peer / "mods/preloaded"
            shutil.copytree(catalog, mods / "packages")
            (mods / "state.toml").write_text(
                'format_version = 1\n[[shared_resource]]\n'
                'id = "megaman-x.source.x3"\npath = ' +
                json.dumps(str(args.x3), ensure_ascii=False) + '\n', encoding="utf-8")
            (peer / "config.ini").write_text(
                '[General]\nAutosave=1\nDisableFrameDelay=0\n'
                '[Graphics]\nOutputMethod=SDL-Software\nWindowScale=1\n'
                '[Sound]\nEnableAudio=0\n', encoding="utf-8")
            env = {k: v for k, v in os.environ.items() if not k.startswith(
                ("MMX_", "SNES_NET", "SNES_RB_", "SNESRECOMP_", "RNET_", "LNG_"))}
            env.update(SNES_NETPLAY="1", SNES_NET_SLOT=str(seat),
                SNES_NET_BIND=f"127.0.0.1:{args.port + seat}",
                SNES_NET_PEER=f"127.0.0.1:{args.port + 1 - seat}",
                SNES_NET_INPUT_PLAYER="0", SNESRECOMP_RUN_FRAMES="180",
                SDL_VIDEODRIVER="dummy")
            log = (peer / "desktop.log").open("wb")
            proc = subprocess.Popen([str(exe), "--no-launcher", "--rom", str(args.rom)],
                cwd=peer, env=env, stdout=log, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            processes.append((proc, log, peer))
        digests = []
        for proc, log, peer in processes:
            result = proc.wait(timeout=40)
            log.close()
            text = (peer / "desktop.log").read_text(errors="replace")
            assert result == 0, text[-4000:]
            digest = re.search(r"RB boot digest agreed \(([0-9a-f]+)\)", text)
            assert digest, text[-4000:]
            digests.append(digest.group(1))
            timing = re.search(r"video totals: simulations=180 presentations=\d+ seconds=([0-9.]+)", text)
            assert timing, text[-4000:]
            elapsed = float(timing.group(1))
            assert elapsed >= 2.9, f"Guest outran the SNES frame rate: {elapsed}s"
            assert not (peer / "saves/save0.sav").exists(), "Online match wrote an offline autosave"
            assert "match refused" not in text and "INPUT desync" not in text, text[-4000:]
            print(f"{peer.name}: boot={digest.group(1)}, 180 frames in {elapsed:.3f}s, no autosave")
        assert digests[0] == digests[1], "Boot states differ"
    finally:
        for proc, log, _ in processes:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            log.close()


if __name__ == "__main__":
    main()

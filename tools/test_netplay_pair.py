"""Two real MMX lobby/runtime peers; all state lives in a private test directory."""
import argparse
import os
import random
from pathlib import Path
import shutil
import subprocess
import time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exe", type=Path, required=True)
    p.add_argument("--rom", type=Path, required=True)
    p.add_argument("--x2", type=Path, required=True)
    p.add_argument("--x3", type=Path, required=True)
    p.add_argument("--fixture", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--independent-views", action="store_true",
                   help="Separate the online actors and capture each peer's local view")
    p.add_argument("--unified-views", action="store_true",
                   help="Test host-selected Unified cameras and shared screen tether")
    args = p.parse_args()
    for key in ("exe", "rom", "x2", "x3", "fixture"):
        setattr(args, key, getattr(args, key).resolve(strict=True))
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    catalog = Path(__file__).resolve().parents[1] / "mods/preloaded/packages"
    processes = []
    logs = []
    port = random.randrange(20000, 50000)
    try:
        for seat in range(2):
            work = root / f"peer{seat}"
            work.mkdir(exist_ok=True)
            shutil.copytree(catalog, work / "catalog/packages", dirs_exist_ok=True)
            # Separate installations, as on two PCs: caches and LAN discovery
            # registry paths must not accidentally be shared by the harness.
            peer_exe = work / args.exe.name
            shutil.copy2(args.exe, peer_exe)
            env = {k: v for k, v in os.environ.items()
                   if not k.startswith(("MMX_", "SNES_NET", "SNES_RB_", "RNET_"))}
            env.update(MMX_NETPLAY_PAIR_ROOT=str(work / "catalog"),
                       MMX_NETPLAY_PAIR_PORT=str(port),
                       MMX_COOP_X2_ROM=str(args.x2), MMX_COOP_X3_ROM=str(args.x3),
                       MMX_ZERO_TEST_FIXTURE=str(args.fixture),
                       SNESRECOMP_LLE_BOUNCE="1")
            if seat:
                env["MMX_NETPLAY_PAIR_GUEST"] = "1"
            if args.independent_views or args.unified_views:
                env["MMX_NETPLAY_VIEWS_TEST"] = "1"
            if args.unified_views:
                env["MMX_NETPLAY_UNIFIED_TEST"] = "1"
            log = (work / "pair.log").open("wb")
            logs.append(log)
            processes.append(subprocess.Popen([str(peer_exe), str(args.rom)], cwd=work,
                env=env, stdout=log, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0))
            if not seat:
                time.sleep(0.4)
        for process in processes:
            process.wait(timeout=50)
    finally:
        for process in processes:
            if process.poll() is None:
                process.kill()
                process.wait()
        for log in logs:
            log.close()
    ok = True
    results = []
    for seat, process in enumerate(processes):
        log = (root / f"peer{seat}/pair.log").read_text(errors="replace")
        lines = [line for line in log.splitlines() if line.startswith("PAIR_RESULT")]
        print(f"Peer {seat}: exit={process.returncode}; log={root / f'peer{seat}/pair.log'}")
        print("\n".join(lines if lines else log.splitlines()[-18:]))
        ok &= process.returncode == 0 and "MMX NETPLAY PAIR CHECKS PASSED" in log
        if lines:
            results.append(dict(item.split("=", 1) for item in lines[0].split()[1:]))
    if len(results) == 2:
        ok &= all(results[0][key] == results[1][key] for key in ("frames", "x", "hp"))
        ok &= all(int(r["confirmed"]) >= 400 for r in results)
        ok &= any(int(r["replay"]) > 0 for r in results)
    else:
        ok = False
    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    main()

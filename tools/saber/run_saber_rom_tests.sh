#!/usr/bin/env bash
# Run the Saber-owned real-ROM characterization executable in a disposable
# executable/catalog/cache directory. Usage: tools/saber/run_saber_rom_tests.sh
# [X1_ROM]. The default X1 ROM and fixture are the repository's documented
# local assets; MMX_COOP_X3_ROM and MMX_ZERO_TEST_FIXTURE may override them.

set -o pipefail

export PATH="/c/msys64/mingw64/bin:$PATH"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
build_dir="$repo_root/build-mingw"
tmp_dir="$build_dir/tmp"
exe="$build_dir/mmx_saber_rom_tests.exe"
catalog_source="$build_dir/mods/preloaded"
cache_source="$build_dir/cache"
x1_rom="${1:-$repo_root/mmx.sfc}"
x3_rom="${MMX_COOP_X3_ROM:-$repo_root/Mega Man X3 (USA).sfc}"
fixture="${MMX_ZERO_TEST_FIXTURE:-$build_dir/saves/save0.sav}"

if [[ $# -gt 1 ]]; then
  printf 'usage: %s [X1_ROM]\n' "$0" >&2
  exit 2
fi
if [[ ! -x "$exe" ]]; then
  printf 'FAIL: test executable is missing: %s\n' "$exe" >&2
  exit 2
fi
if [[ ! -d "$catalog_source" ]]; then
  printf 'FAIL: staged catalog is missing: %s\n' "$catalog_source" >&2
  exit 2
fi
for required in "$x1_rom" "$x3_rom" "$fixture"; do
  if [[ ! -f "$required" ]]; then
    printf 'FAIL: required ROM/fixture is missing: %s\n' "$required" >&2
    exit 2
  fi
done

mkdir -p "$tmp_dir"
run_name="saber-rom-$(date +%Y%m%d-%H%M%S)-$$"
run_dir="$(mktemp -d "$tmp_dir/$run_name.XXXXXX")"
catalog_copy="$run_dir/catalog"
mkdir -p "$catalog_copy"
cp -a "$catalog_source/." "$catalog_copy/"
find "$catalog_copy" -type f -name 'state.toml' -delete
if [[ -d "$cache_source" ]]; then
  cp -a "$cache_source" "$run_dir/cache"
else
  mkdir -p "$run_dir/cache"
fi
if [[ -f "$build_dir/SDL3.dll" ]]; then
  cp "$build_dir/SDL3.dll" "$run_dir/SDL3.dll"
fi
cp "$exe" "$run_dir/mmx_saber_rom_tests.exe"

log_path="$tmp_dir/$run_name.log"
test_exe="$run_dir/mmx_saber_rom_tests.exe"
asset_path="$run_dir/cache/mmx-source/x3-zero-v7.bin"

set +e
(
  cd "$tmp_dir" || exit 125
  TMP="$run_dir" TEMP="$run_dir" TMPDIR="$run_dir" \
    MMX_COOP_LAUNCHER_ROOT="$catalog_copy" \
    MMX_COOP_X3_ROM="$x3_rom" \
    MMX_ZERO_TEST_FIXTURE="$fixture" \
    MMX_ZERO_TEST_ASSETS="$asset_path" \
    MMX_SABER_TEST_CACHE="$run_dir/cache" \
    "$test_exe" "$x1_rom"
) 2>&1 | tee "$log_path"
test_exit="${PIPESTATUS[0]}"
set -e

if [[ "$test_exit" == 0 ]] && grep -q 'SABER ROM CHECKS PASSED' "$log_path"; then
  printf 'PASS: Saber ROM checks\n'
  result_exit=0
else
  printf 'FAIL: Saber ROM checks (exit=%s)\n' "$test_exit"
  result_exit="$test_exit"
  if [[ "$result_exit" == 0 ]]; then
    result_exit=1
  fi
fi
printf 'log: %s\n' "$log_path"
rm -rf "$run_dir"
exit "$result_exit"

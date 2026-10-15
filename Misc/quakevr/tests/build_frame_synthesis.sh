#!/usr/bin/env bash
# Native ARM64 compile/CPU validation only. Never opens SDL, OpenXR or a game.
set -euo pipefail
if [[ $# != 1 ]]; then
  echo "usage: $0 ABSOLUTE_BUILD_ROOT (native ARM64 Linux; no runtime launch)" >&2
  exit 2
fi
case $(uname -m) in
  aarch64|arm64) ;;
  *) echo "Native ARM64 toolchain required; host builds are not Frame binaries" >&2; exit 2 ;;
esac
[[ $(uname -s) == Linux && $1 == /* ]] || { echo "Linux and an absolute build root are required" >&2; exit 2; }
source_root=$(cd -- "$(dirname -- "$0")/../../.." && pwd -P)
mkdir -p -- "$1"
build_root=$(cd -- "$1" && pwd -P)
[[ $build_root != "$source_root" ]] || { echo "Use a separate build root" >&2; exit 2; }
compiler_c=${CC:-clang}
compiler_cpp=${CXX:-clang++}
jobs=${QVR_BUILD_JOBS:-2}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || { echo "QVR_BUILD_JOBS must be positive" >&2; exit 2; }
command -v "$compiler_c" >/dev/null
command -v "$compiler_cpp" >/dev/null
{
  date -u +%Y-%m-%dT%H:%M:%SZ
  uname -sm
  "$compiler_c" --version
  "$compiler_cpp" --version
  cmake --version
  printf 'source=%s\nbuild=%s\njobs=%s\nruntime_launched=0\n' "$source_root" "$build_root" "$jobs"
} > "$build_root/toolchain.txt"
for mode in OFF ON; do
  output="$build_root/${mode,,}"
  # Preserve the production dependency set (including curl); portable paths
  # avoid user-directory reads/writes when these binaries are tested later.
  cmake -S "$source_root" -B "$output" \
    -DCMAKE_C_COMPILER="$compiler_c" -DCMAKE_CXX_COMPILER="$compiler_cpp" \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_USERDIRS=OFF \
    -DQVR_ENABLE_FRAME_SYNTHESIS_GL_VULKAN_BRIDGE="$mode" \
    -DQVR_ENABLE_NATIVE_VULKAN_BSP=OFF -DQVR_BUILD_OPENXR_VULKAN_FIXTURE=OFF \
    -DQVR_BUILD_CPU_TESTS=ON 2>&1 | tee "$build_root/${mode,,}-configure.log"
  cmake --build "$output" -j "$jobs" 2>&1 | tee "$build_root/${mode,,}-build.log"
  ctest --test-dir "$output" --output-on-failure 2>&1 | tee "$build_root/${mode,,}-ctest.log"
  readelf -h "$output/ironwail" > "$build_root/${mode,,}-elf.txt"
  grep -q 'Machine:.*AArch64' "$build_root/${mode,,}-elf.txt"
  sha256sum "$output/ironwail" > "$build_root/${mode,,}-binary.sha256"
done
# Compile the initialized paired handoff fixture too; never execute it here.
fixture=${QVR_BUILD_INTEROP_FIXTURE:-1}
[[ $fixture == 0 || $fixture == 1 ]] || { echo "QVR_BUILD_INTEROP_FIXTURE must be 0 or 1" >&2; exit 2; }
if [[ $fixture == 1 ]]; then
  cmake -S "$source_root/Misc/quakevr/tests/raster_depth_probe" -B "$build_root/paired-fixture" \
    -DCMAKE_CXX_COMPILER="$compiler_cpp" -DCMAKE_BUILD_TYPE=Debug \
    2>&1 | tee "$build_root/paired-fixture-configure.log"
  cmake --build "$build_root/paired-fixture" -j "$jobs" 2>&1 | tee "$build_root/paired-fixture-build.log"
  spirv-val --target-env vulkan1.1 "$build_root/paired-fixture/bridge_depth_readback.comp.spv" \
    2>&1 | tee "$build_root/paired-fixture-spirv.log"
  readelf -h "$build_root/paired-fixture/qvr_raster_depth_probe" > "$build_root/paired-fixture-elf.txt"
  grep -q 'Machine:.*AArch64' "$build_root/paired-fixture-elf.txt"
  sha256sum "$build_root/paired-fixture/qvr_raster_depth_probe" > "$build_root/paired-fixture-binary.sha256"
fi
printf 'PASS native ARM64 OFF/ON builds and CPU tests; fixture_compiled=%s runtime_launched=0\n' "$fixture"

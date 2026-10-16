#!/usr/bin/env bash
set -euo pipefail

source_root=${QVR_SOURCE:-/home/steamos/Games/QuakeVR-Unleashed-native/source-spacewarp-current-20261014}
results_root=${QVR_RESULTS_ROOT:-/home/steamos/Games/QuakeVR-Unleashed-native/performance-results}
out=${QVR_PROBE_OUT:-$results_root/spacewarp-gl-swapchain-probe-$(date +%Y%m%d-%H%M%S)}
runtime_json=${XR_RUNTIME_JSON:-/opt/steamvr/steamxr_linuxarm64.json}
compositor_log=${HOME}/.steam/steam/logs/vrcompositor.txt
mkdir -p "$out"

if pgrep -x ironwail >/dev/null; then
  echo "Quake is running; refusing to start a separate OpenXR session" >&2
  exit 3
fi

export SDL_VIDEODRIVER=x11
export DISPLAY=${DISPLAY:-:1}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/1000}
export XR_RUNTIME_JSON=$runtime_json

g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -I "$source_root/Quake/vr/external" -I /usr/include/SDL2 \
  "$source_root/Misc/quakevr/tests/vr_openxr_gl_swapchain_probe.cpp" \
  -lSDL2 -lGL -lX11 -L/opt/steamvr/bin/linuxarm64 \
  -Wl,-rpath,/opt/steamvr/bin/linuxarm64 -lopenxr_loader \
  -o /tmp/vr_openxr_gl_swapchain_probe

{
  date -u
  printf 'source_root=%s\nruntime_json=%s\n' "$source_root" "$runtime_json"
  sha256sum /tmp/vr_openxr_gl_swapchain_probe
  pgrep -af 'vrserver|vrcompositor' || true
} > "$out/environment.txt"

run_case() {
  local mode=$1 before after status
  before=$(wc -l < "$compositor_log")
  set +e
  timeout --signal=TERM --kill-after=3s 25s /tmp/vr_openxr_gl_swapchain_probe "$mode" \
    > "$out/$mode.stdout.txt" 2>&1
  status=$?
  set -e
  after=$(wc -l < "$compositor_log")
  printf 'mode=%s exit=%s compositor_lines_before=%s after=%s added=%s\n' \
    "$mode" "$status" "$before" "$after" "$((after - before))" \
    > "$out/$mode.status.txt"
  if (( after > before )); then
    sed -n "$((before + 1)),${after}p" "$compositor_log" > "$out/$mode.vrcompositor-delta.txt"
  else
    : > "$out/$mode.vrcompositor-delta.txt"
  fi
  cat "$out/$mode.status.txt"
  cat "$out/$mode.stdout.txt"
  grep -Ei 'shared image|depth stencil|fence|error|fail' "$out/$mode.vrcompositor-delta.txt" || true
}

for mode in color motion depth depth32_attach depth24 depth16 depth_noext; do
  run_case "$mode"
done

g++ -std=c++17 -O2 -Wall -Wextra -Wno-missing-field-initializers \
  -I "$source_root/Quake/vr/external" \
  "$source_root/Misc/quakevr/tests/vr_openxr_vulkan_format_probe.cpp" \
  -lvulkan -L/opt/steamvr/bin/linuxarm64 \
  -Wl,-rpath,/opt/steamvr/bin/linuxarm64 -lopenxr_loader \
  -o /tmp/vr_openxr_vulkan_format_probe
before=$(wc -l < "$compositor_log")
set +e
/tmp/vr_openxr_vulkan_format_probe > "$out/vulkan_control.stdout.txt" 2>&1
status=$?
set -e
after=$(wc -l < "$compositor_log")
printf 'mode=vulkan_control exit=%s compositor_lines_before=%s after=%s added=%s\n' \
  "$status" "$before" "$after" "$((after - before))" > "$out/vulkan_control.status.txt"
if (( after > before )); then
  sed -n "$((before + 1)),${after}p" "$compositor_log" > "$out/vulkan_control.vrcompositor-delta.txt"
else
  : > "$out/vulkan_control.vrcompositor-delta.txt"
fi
cat "$out/vulkan_control.status.txt"
cat "$out/vulkan_control.stdout.txt"
grep -Ei 'shared image|depth stencil|fence|error|fail' "$out/vulkan_control.vrcompositor-delta.txt" || true
echo "results=$out"

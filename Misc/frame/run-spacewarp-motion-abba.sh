#!/usr/bin/env bash
# App-side OFF/ON/ON/OFF profiling. This does not require a tracked HMD and
# cannot establish runtime receipt or synthesis.
set -euo pipefail
root=${QVR_ROOT:-/home/steamos/Games/QuakeVR-Unleashed-native}
game=${QVR_GAME:-$root/game}
source=${QVR_SOURCE:?Set QVR_SOURCE to the tested source tree on Frame}
binary=${QVR_BIN:?Set QVR_BIN to the matching ARM64 build}
smoke="$source/Misc/quakevr/tests/run_frame_synthesis_smoke.sh"
bench="$source/Misc/quakevr/bench/qvrbench.py"
frames=${QVR_FRAMES:-}
hz=${QVR_HZ:-72}
defer_wait=${QVR_VULKAN_DEFER_WAIT:-1}
demo=${QVR_DEMO:-}
demo_map=${QVR_DEMO_MAP:-e1m1}
if [[ -z "$frames" ]]; then
  if [[ -n "$demo" ]]; then frames=1080; else frames=180; fi
fi
scenario=${QVR_SCENARIO:-spacewarp_motion_fixture}
if [[ -n "$demo" && -z "${QVR_SCENARIO:-}" ]]; then scenario=demo_replay; fi
base=${QVR_OUT_BASE:-$root/performance-results/spacewarp-${scenario}-motion-abba-$(date -u +%Y%m%d-%H%M%S)}

for path in "$smoke" "$bench" "$binary"; do
  [[ -e "$path" ]] || { echo "Missing required path: $path" >&2; exit 2; }
done
[[ -x "$binary" ]] || { echo "Build is not executable: $binary" >&2; exit 2; }
[[ "$frames" =~ ^[0-9]+$ ]] && ((frames >= 60 && frames <= 1800)) || { echo "QVR_FRAMES must be 60..1800" >&2; exit 2; }
[[ "$scenario" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "QVR_SCENARIO must be a scenario basename" >&2; exit 2; }
if [[ -n "$demo" ]]; then
  [[ -f "$source/Misc/quakevr/bench/qvrdemo.py" ]] || { echo "Missing demo profile generator" >&2; exit 2; }
  [[ "$demo" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "QVR_DEMO must be a demo basename" >&2; exit 2; }
  [[ -f "$game/quakevr/$demo.dem" ]] || { echo "Missing demo: $game/quakevr/$demo.dem" >&2; exit 2; }
  [[ "$demo_map" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "QVR_DEMO_MAP must be a map basename" >&2; exit 2; }
fi
pgrep -x ironwail >/dev/null && { echo "Quake is already running; refusing a second session" >&2; exit 3; }
mkdir -p "$base"
printf 'app_side_only=true\nscenario=%s\ndemo=%s\nframes=%s\nhz_cap=%s\n' \
  "$scenario" "${demo:-none}" "$frames" "$hz" >"$base/README.txt"
sha256sum "$binary" >"$base/binary.sha256"

for spec in off-a1:0 on-b1:1 on-b2:1 off-a2:0; do
  tag=${spec%%:*}
  synthesis=${spec##*:}
  mode=off
  relaxed=0
  if [[ "$synthesis" == 1 ]]; then mode=on; relaxed=${QVR_RELAXED_INTERVAL_ON:-0}; fi
  leg="$base/raw/$tag"
  mkdir -p "$leg" "$base/$mode/$scenario"
  echo "Starting $tag: synthesis=$synthesis relaxed=$relaxed"
  QVR_ROOT="$root" QVR_GAME="$game" QVR_SOURCE="$source" QVR_BIN="$binary" \
    QVR_OUT="$leg" QVR_TAG="$tag" QVR_SCENARIO="$scenario" \
    QVR_DEMO="$demo" QVR_DEMO_MAP="$demo_map" \
    QVR_FRAMES="$frames" QVR_HZ="$hz" QVR_FRAME_SYNTHESIS="$synthesis" \
    QVR_RELAXED_INTERVAL="$relaxed" QVR_VULKAN_INTEROP=1 \
    QVR_VULKAN_DEFER_WAIT="$defer_wait" QVR_SRGB_MOTION=0 QVR_DETAIL_PROFILE=1 \
    bash "$smoke" >"$leg/runner.log" 2>&1 || {
      cat "$leg/runner.log" >&2
      echo "Failed $tag; stopping the paired run." >&2
      exit 1
    }
  repetition=1
  [[ "$tag" != *-a2 && "$tag" != *-b2 ]] || repetition=2
  cp -p "$leg/$tag.json" "$base/$mode/$scenario/r$repetition.json"
done

python3 "$bench" compare --threshold 5 "$base/off" "$base/on" | tee "$base/comparison.md"
echo "Completed app-side motion ABBA; raw runs are under $base/raw"

#!/usr/bin/env bash
set -euo pipefail
root=${QVR_ROOT:-/home/steamos/Games/QuakeVR-Unleashed-native}
game=${QVR_GAME:-$root/game}
binary=${QVR_BIN:-$root/build-spacewarp-current-20261014/ironwail}
source=${QVR_SOURCE:-$root/source-spacewarp-current-20261014}
progs_source=${QVR_PROGS_DAT:-$source/quakevr/progs.dat}
tag=${QVR_TAG:-frame_synthesis_smoke_$(date +%Y%m%d-%H%M%S)}
scenario=${QVR_SCENARIO:-idle_range}
demo=${QVR_DEMO:-}
record_demo=${QVR_RECORD_DEMO:-}
demo_map=${QVR_DEMO_MAP:-e1m1}
frames=${QVR_FRAMES:-450}
hz=${QVR_HZ:-72}
timeout_seconds=${QVR_TIMEOUT_SECONDS:-}
synthesis=${QVR_FRAME_SYNTHESIS:-1}
vulkan_interop=${QVR_VULKAN_INTEROP:-0}
defer_wait=${QVR_VULKAN_DEFER_WAIT:-0}
relaxed=${QVR_RELAXED_INTERVAL:-0}
unfocused_pause=${QVR_XR_UNFOCUSED_PAUSE:-0}
srgb_motion=${QVR_SRGB_MOTION:-1}
diagnostic_capture=${QVR_SYNTHESIS_CAPTURE:-0}
detail_profile=${QVR_DETAIL_PROFILE:-0}
profile_interval=${QVR_PROFILE_INTERVAL:-120}
out=${QVR_OUT:-$root/performance-results/${tag}-$(date +%Y%m%d-%H%M%S)}
cfg="$game/quakevr/quakevr.cfg"
usercfg="$game/quakevr/ironwail.cfg"
progs="$game/quakevr/progs.dat"
openxrlog="$game/quakevr/qvr_openxr.txt"
profile="$game/quakevr/profile/bench/$tag.json"
[[ -x "$binary" ]] || { echo "Missing executable: $binary" >&2; exit 2; }
if [[ -z "$timeout_seconds" ]]; then
  if [[ -n "$demo" ]]; then timeout_seconds=90; else timeout_seconds=45; fi
fi
[[ "$timeout_seconds" =~ ^[0-9]+$ ]] && ((timeout_seconds >= 30 && timeout_seconds <= 300)) || {
  echo "QVR_TIMEOUT_SECONDS must be 30..300" >&2; exit 2;
}
[[ "$diagnostic_capture" =~ ^[01]$ ]] || { echo "QVR_SYNTHESIS_CAPTURE must be 0 or 1" >&2; exit 2; }
if [[ -n "$record_demo" ]]; then
  [[ -z "$demo" ]] || { echo "QVR_RECORD_DEMO and QVR_DEMO are mutually exclusive" >&2; exit 2; }
  [[ "$record_demo" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "QVR_RECORD_DEMO must be a demo basename" >&2; exit 2; }
  [[ ! -e "$game/quakevr/$record_demo.dem" ]] || { echo "Refusing to overwrite existing demo: $record_demo.dem" >&2; exit 4; }
fi
[[ -f "$cfg" && -f "$usercfg" && -f "$progs" ]] || { echo "Missing game configuration or progs.dat" >&2; exit 2; }
[[ -f "$progs_source" ]] || { echo "Missing test QuakeC bytecode: $progs_source" >&2; exit 2; }
if pgrep -x ironwail >/dev/null; then echo "Quake is already running; refusing a second session" >&2; exit 3; fi
mkdir -p "$out"
backup_cfg="$out/quakevr.cfg.original"
backup_usercfg="$out/ironwail.cfg.original"
backup_progs="$out/progs.dat.original"
backup_console="$out/qconsole.log.original"
backup_openxr="$out/qvr_openxr.txt.original"
for f in "$backup_cfg" "$backup_usercfg" "$backup_progs" "$backup_console" "$backup_openxr"; do
  [[ ! -e "$f" ]] || { echo "Backup already exists: $f" >&2; exit 4; }
done
cp -p "$cfg" "$backup_cfg"
cp -p "$usercfg" "$backup_usercfg"
cp -p "$progs" "$backup_progs"
[[ ! -e "$game/qconsole.log" ]] || cp -p "$game/qconsole.log" "$backup_console"
[[ ! -e "$openxrlog" ]] || cp -p "$openxrlog" "$backup_openxr"
capture_artifacts() {
  [[ ! -e "$game/qconsole.log" ]] || cp -p "$game/qconsole.log" "$out/qconsole.log.test"
  [[ ! -e "$openxrlog" ]] || cp -p "$openxrlog" "$out/qvr_openxr.txt.test"
  [[ ! -e "$profile" ]] || cp -p "$profile" "$out/$tag.json"
  if [[ -e "$out/run-started" ]]; then
    find "$game/quakevr/profile" -maxdepth 1 -type f -name 'profile_*.csv' -newer "$out/run-started" \
      -exec cp -p -t "$out" {} + 2>/dev/null || true
    local capture_dir="$game/quakevr/profile/frame_synthesis_capture"
    if [[ -d "$capture_dir" ]]; then
      python3 - "$capture_dir" "$out/frame_synthesis_capture" "$out/run-started" <<'PY'
from pathlib import Path
import shutil
import sys
source, destination, started = map(Path, sys.argv[1:])
if not started.exists():
    raise SystemExit(0)
cutoff = started.stat().st_mtime
files = [p for p in source.iterdir() if p.is_file() and p.stat().st_mtime >= cutoff]
if files:
    destination.mkdir(parents=True, exist_ok=True)
    for path in files:
        shutil.copy2(path, destination / path.name)
PY
    fi
    if [[ -n "$record_demo" && -s "$game/quakevr/$record_demo.dem" ]]; then
      cp -p "$game/quakevr/$record_demo.dem" "$out/$record_demo.dem"
    fi
  fi
  if [[ -e "$out/run-started" ]]; then
    local criteria="$HOME/.local/share/Steam/logs/perfcriteria.txt"
    [[ ! -e "$criteria" || ! "$criteria" -nt "$out/run-started" ]] || cp -p "$criteria" "$out/perfcriteria.txt"
    if [[ "${QVR_DEFER_STEAMVR_CSV:-0}" != 1 ]]; then
      find "$HOME/.local/share/Steam/logs" -maxdepth 1 -type f -name 'perfrecording-*.csv' -newer "$out/run-started" \
        -exec cp -p -t "$out" {} + 2>/dev/null || true
    fi
  fi
}
restore() {
  capture_artifacts
  cp -p "$backup_cfg" "$cfg"
  cp -p "$backup_usercfg" "$usercfg"
  cp -p "$backup_progs" "$progs"
  if [[ -e "$backup_console" ]]; then cp -p "$backup_console" "$game/qconsole.log"; else rm -f "$game/qconsole.log"; fi
  if [[ -e "$backup_openxr" ]]; then cp -p "$backup_openxr" "$openxrlog"; else rm -f "$openxrlog"; fi
  rm -f "$game/quakevr/frame_synthesis_smoke.cfg"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
python3 - "$cfg" "$usercfg" "${QVR_MOTION_RANGE:-0.125}" "$synthesis" "$relaxed" "$unfocused_pause" "$vulkan_interop" "$srgb_motion" "$defer_wait" <<'PY'
from pathlib import Path
import sys
cfg, usercfg = Path(sys.argv[1]), Path(sys.argv[2])
motion_range, synthesis, relaxed, unfocused_pause, interop, srgb_motion, defer_wait = sys.argv[3:]
if "vr_enabled 1" not in cfg.read_text():
    raise SystemExit("could not find vr_enabled 1 in quakevr.cfg")
names = ("vr_backend", "vr_xr_vulkan_interop", "vr_xr_vulkan_defer_wait", "vr_xr_frame_synthesis",
         "vr_xr_frame_synthesis_srgb_motion", "vr_xr_frame_synthesis_motion_range",
         "vr_xr_frame_synthesis_relaxed", "vr_xr_unfocused", "vr_xr_unfocused_pause")
settings = (f"vr_backend openxr\nvr_xr_vulkan_interop {interop}\n"
            f"vr_xr_vulkan_defer_wait {defer_wait}\n"
            f"vr_xr_frame_synthesis {synthesis}\n"
            f"vr_xr_frame_synthesis_srgb_motion {srgb_motion}\n"
            f"vr_xr_frame_synthesis_motion_range {motion_range}\n"
            f"vr_xr_frame_synthesis_relaxed {relaxed}\n"
            f"vr_xr_unfocused 0\nvr_xr_unfocused_pause {unfocused_pause}\n")
for path in (cfg, usercfg):
    content = path.read_text()
    content = "\n".join(line for line in content.splitlines()
                         if not any(line.startswith(name + " ") for name in names))
    if "vr_enabled 1" in content:
        content = content.replace("vr_enabled 1", settings + "vr_enabled 1", 1)
    else:
        content = content.rstrip() + "\n" + settings
    path.write_text(content + "\n")
PY
if [[ -n "$demo" ]]; then
  [[ "$demo" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "QVR_DEMO must be a demo basename (letters, numbers, _ or -)" >&2; exit 2; }
  [[ -f "$game/quakevr/$demo.dem" ]] || { echo "Missing demo: $game/quakevr/$demo.dem" >&2; exit 2; }
  [[ "$demo_map" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "QVR_DEMO_MAP must be a map basename" >&2; exit 2; }
  python3 "$source/Misc/quakevr/bench/qvrdemo.py" \
    --out "$game/quakevr/frame_synthesis_smoke.cfg" --tag "$tag" \
    --frames "$frames" --hz "$hz" --demo "$demo" --map "$demo_map"
else
  python3 "$source/Misc/quakevr/bench/qvrbench.py" script "$scenario" --out "$game/quakevr/frame_synthesis_smoke.cfg" --tag "$tag" --frames "$frames" --hz "$hz" --realtime
fi
python3 - "$game/quakevr/frame_synthesis_smoke.cfg" "$unfocused_pause" "$scenario" "$diagnostic_capture" "$record_demo" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
pause = sys.argv[2]
scenario = sys.argv[3]
diagnostic_capture = sys.argv[4]
record_demo = sys.argv[5]
lines = p.read_text().splitlines()
# The generated commands execute after the persisted game/user configs. Set
# this here as well as in the temporary base config so an old user value cannot
# freeze a headless single-player benchmark. Enable kill telemetry for the one
# replay whose validation requires recorded deaths.
lines.insert(0, f"vr_xr_unfocused_pause {pause}")
if diagnostic_capture == "1":
    lines.insert(1, "vr_xr_frame_synthesis_capture 1")
if scenario == "gore_headpop_24":
    lines.insert(2, "vr_bench_kills 1")
if record_demo:
    start = next((i for i, line in enumerate(lines) if line.startswith("vr_bench_begin ")), None)
    end = next((i for i, line in enumerate(lines) if line == "vr_bench_end"), None)
    if start is None or end is None or end <= start:
        raise SystemExit("cannot place demo recording around benchmark window")
    lines.insert(start, f"record {record_demo}")
    end = next(i for i, line in enumerate(lines) if line == "vr_bench_end")
    lines.insert(end + 1, "stop")
p.write_text("\n".join(lines) + "\n")
PY
if [[ "$detail_profile" == 1 ]]; then
  python3 - "$game/quakevr/frame_synthesis_smoke.cfg" "$tag" "$profile_interval" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
tag = sys.argv[2]
profile_interval = sys.argv[3]
s = p.read_text()
needle = f"vr_bench_begin {tag} 450"
if needle not in s:
    # The frame count is selected by QVR_FRAMES, which is not passed as an
    # argument to this helper. Match the unique command prefix instead.
    lines = s.splitlines()
    i = next((i for i, line in enumerate(lines) if line.startswith(f"vr_bench_begin {tag} ")), None)
    if i is None:
        raise SystemExit("could not find benchmark start command")
    lines[i + 1:i + 1] = ["vr_profile 1", f"vr_profile_interval {profile_interval}"]
    s = "\n".join(lines) + "\n"
else:
    s = s.replace(needle, needle + f"\nvr_profile 1\nvr_profile_interval {profile_interval}", 1)
s = s.replace("vr_bench_end\n", "vr_bench_end\nvr_profile_dump\n", 1)
p.write_text(s)
PY
fi
touch "$out/run-started"
cp -p "$progs_source" "$progs"
cd "$game"
export SDL_VIDEODRIVER=x11 DISPLAY="${DISPLAY:-:1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"
export XR_RUNTIME_JSON="${XR_RUNTIME_JSON:-/opt/steamvr/steamxr_linuxarm64.json}"
set +e
if [[ -n "$demo" ]]; then
  demo_completion_signal=0
  timeout --signal=INT --kill-after=5s "${timeout_seconds}s" "$binary" -window -vr-hide-mirror -qvr-frame-diagnostics -basedir "$game" -game quakevr -nobindless -nosteam -noconfigwrite -condebug +vr_memstats_log 0 +vr_mirror 0 +exec frame_synthesis_smoke.cfg >"$out/launcher.log" 2>&1 &
  process_group=$!
  for _ in $(seq 1 "$((timeout_seconds * 10))"); do
    kill -0 "$process_group" 2>/dev/null || break
    if [[ -s "$profile" ]] && grep -Fq "Playing demo from $demo.dem." "$game/qconsole.log" 2>/dev/null && \
       python3 -c 'import json,sys; sys.exit(0 if json.load(open(sys.argv[1])).get("frames") == int(sys.argv[2]) else 1)' "$profile" "$frames" 2>/dev/null; then
      app_pid=$(pgrep -n -x ironwail || true)
      if [[ -n "$app_pid" ]]; then
        echo "Validated $frames replay frames; stopping the completed demo session." >"$out/demo-completion.txt"
        kill -INT "$app_pid" 2>/dev/null || true
        demo_completion_signal=1
      fi
      break
    fi
    sleep 0.1
  done
  wait "$process_group"
  status=$?
else
  timeout --signal=INT --kill-after=5s "${timeout_seconds}s" "$binary" -window -vr-hide-mirror -qvr-frame-diagnostics -basedir "$game" -game quakevr -nobindless -nosteam -noconfigwrite -condebug +vr_memstats_log 0 +vr_mirror 0 +exec frame_synthesis_smoke.cfg >"$out/launcher.log" 2>&1
  status=$?
fi
set -e
echo "$status" >"$out/exit-status.txt"
if [[ $status -ne 0 && "${demo_completion_signal:-0}" != 1 ]]; then
  echo "Smoke process ended with status $status" >&2
  exit "$status"
fi
capture_artifacts

# A timeout or launch failure must not look like a successful idle replay just
# because the EXIT trap restored the user's files. Every scenario emits the
# same frame benchmark JSON; require the requested number of measured frames.
if [[ ! -s "$profile" ]]; then
  echo "Missing benchmark JSON: $profile" >&2
  exit 7
fi
python3 - "$profile" "$frames" <<'PY'
import json
import sys
from pathlib import Path

profile = Path(sys.argv[1])
expected = int(sys.argv[2])
data = json.loads(profile.read_text(encoding="utf-8"))
actual = int(data.get("frames", -1))
if actual != expected:
    raise SystemExit(f"Benchmark recorded {actual} frames; expected {expected}")
PY

if [[ -z "$demo" && "$scenario" == gore_headpop_24* ]]; then
  validation_args=(--expected-kills "${QVR_EXPECT_KILLS:-23}" \
    --expected-deaths "${QVR_EXPECT_MONSTER_DEATHS:-22}" \
    --console "$out/qconsole.log.test" \
    --expected-heads-popped "${QVR_EXPECT_HEADS_POPPED:-23}")
  python3 "$source/Misc/quakevr/tests/validate_frame_synthesis_capture.py" \
    "$profile" "$out/workload-validation.json" --frames "$frames" \
    "${validation_args[@]}"
fi
if [[ -z "$demo" && "$scenario" == "spacewarp_motion_fixture" ]]; then
  python3 "$source/Misc/quakevr/tests/validate_spacewarp_motion_fixture.py" \
    "$profile" "$out/qconsole.log.test" "$out/qvr_openxr.txt.test" \
    --frames "$frames" --expect-synthesis "$synthesis" \
    --output "$out/motion-fixture-validation.json"
fi
if [[ -n "$demo" ]]; then
  python3 - "$out/qconsole.log.test" "$demo" <<'PY'
from pathlib import Path
import sys

console = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
demo = sys.argv[2]
if f"Playing demo from {demo}.dem." not in console:
    raise SystemExit(f"demo playback was not confirmed in qconsole.log.test: {demo}")
if f"ERROR: couldn't open {demo}.dem" in console:
    raise SystemExit(f"Quake could not open requested demo: {demo}")
PY
fi
if [[ -n "$record_demo" ]]; then
  if ! grep -q "Completed demo" "$out/qconsole.log.test"; then
    echo "Quake did not confirm that demo recording completed: $record_demo" >&2
    exit 8
  fi
  if [[ ! -s "$game/quakevr/$record_demo.dem" || ! -s "$out/$record_demo.dem" ]]; then
    echo "The requested demo recording is missing or empty: $record_demo" >&2
    exit 8
  fi
  cmp -s "$game/quakevr/$record_demo.dem" "$out/$record_demo.dem" || {
    echo "Copied demo does not match the recorded file: $record_demo" >&2; exit 8;
  }
  # The destination did not exist at preflight; retain the replay artifact in
  # the result folder and leave the installed game directory clean.
  rm -f -- "$game/quakevr/$record_demo.dem"
fi
if [[ "$diagnostic_capture" == 1 ]]; then
  python3 "$source/Misc/quakevr/tests/validate_frame_synthesis_input_capture.py" \
    "$out/frame_synthesis_capture" --output "$out/input-capture-validation.json"
fi

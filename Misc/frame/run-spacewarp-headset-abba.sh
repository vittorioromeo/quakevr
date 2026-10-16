#!/usr/bin/env bash
# Guarded wearer A-B-B-A run. Reuses the smoke runner, which restores every
# temporary game file; this wrapper adds HMD/focus gates and compositor timing.
set -euo pipefail
root=${QVR_ROOT:-/home/steamos/Games/QuakeVR-Unleashed-native}
game=${QVR_GAME:-$root/game}
source=${QVR_SOURCE:?Set QVR_SOURCE to the tested source tree on Frame}
binary=${QVR_BIN:?Set QVR_BIN to the matching ARM64 build}
collector=${QVR_COMPOSITOR_COLLECTOR:?Set QVR_COMPOSITOR_COLLECTOR to the collector binary}
state_probe=${QVR_FRAME_STATE_PROBE:-$root/frame-vr-state.py}
smoke="$source/Misc/quakevr/tests/run_frame_synthesis_smoke.sh"
settings_helper="$source/Misc/frame/steamvr-settings.py"
recording_collector="$source/Misc/frame/collect-spacewarp-recording.py"
headset_validator="$source/Misc/quakevr/tests/validate_frame_synthesis_headset.py"
settings_file=/home/steamos/.config/openvr/config/steamvr.vrsettings
# The collector starts once Quake owns scene focus and runs alongside the
# smoke process. Stop it when the actual Quake process exits; the smoke script
# still has artifact validation and cleanup to do after the game has closed.
seconds=${QVR_COLLECTOR_SECONDS:-60}
csv_wait=${QVR_CSV_WAIT_SECONDS:-20}
frames=${QVR_FRAMES:-450}
hz=${QVR_HZ:-72}
scenario=${QVR_SCENARIO:-spacewarp_motion_fixture}
demo=${QVR_DEMO:-}
demo_map=${QVR_DEMO_MAP:-e1m1}
# Turnip hangs the GPU (and resets SteamVR) on decal-heavy scenarios on the GMEM path; the known
# workaround is TU_DEBUG=sysmem (see docs/HANDOFF-frame-kill-crash-2026-10-15.md). Set QVR_TU_DEBUG= (empty)
# to run without it. Runs before 2026-10-16 00:00 were made without it (the fixture has no decals).
if [[ -n "${QVR_TU_DEBUG-sysmem}" ]]; then export TU_DEBUG="${QVR_TU_DEBUG-sysmem}"; fi
precheck=${QVR_PRECHECK:-0}
# QVR_AUTO=1: unattended run. No wearer is required: an absent/standby HMD pose,
# missing controllers and an open SteamVR dashboard are recorded in the state
# files but do not abort the run (scene focus on the game is still required).
auto=${QVR_AUTO:-0}
[[ "$auto" =~ ^[01]$ ]] || { echo "QVR_AUTO must be 0 or 1" >&2; exit 2; }
[[ "$precheck" =~ ^[01]$ ]] || { echo "QVR_PRECHECK must be 0 or 1" >&2; exit 2; }
if [[ "$precheck" == 1 && -z "${QVR_FRAMES:-}" ]]; then frames=180; fi
# Keep queue-wait scheduling identical in synthesis OFF/ON runs. The deferred
# path is still opt-in in the game; this wearer comparison uses the measured
# low-latency setting unless explicitly overridden for a separate comparison.
defer_wait=${QVR_VULKAN_DEFER_WAIT:-1}
out_base=${QVR_OUT_BASE:-$root/performance-results/spacewarp-headset-$(date -u +%Y%m%d-%H%M%S)}
mkdir -p "$out_base"
for p in "$game/quakevr/quakevr.cfg" "$game/quakevr/ironwail.cfg" "$game/quakevr/progs.dat" "$source/quakevr/progs.dat" "$smoke" "$settings_helper" "$recording_collector" "$headset_validator" "$settings_file" "$binary" "$collector" "$state_probe"; do
  [[ -e "$p" ]] || { echo "Missing required path: $p" >&2; exit 2; }
done
if [[ -n "$demo" ]]; then
  [[ "$demo" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "QVR_DEMO must be a demo basename (letters, numbers, _ or -)" >&2; exit 2; }
  [[ -f "$game/quakevr/$demo.dem" ]] || { echo "Missing demo: $game/quakevr/$demo.dem" >&2; exit 2; }
  [[ "$demo_map" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "QVR_DEMO_MAP must be a map basename" >&2; exit 2; }
fi
[[ -x "$binary" && -x "$collector" ]] || { echo "Binary and collector must be executable" >&2; exit 2; }
[[ "$seconds" =~ ^[0-9]+$ ]] && ((seconds >= 1 && seconds <= 60)) || { echo "QVR_COLLECTOR_SECONDS must be 1..60" >&2; exit 2; }
[[ "$csv_wait" =~ ^[0-9]+$ ]] && ((csv_wait >= 1 && csv_wait <= 60)) || { echo "QVR_CSV_WAIT_SECONDS must be 1..60" >&2; exit 2; }
pgrep -x vrserver >/dev/null || { echo "SteamVR is not running" >&2; exit 3; }
pgrep -x ironwail >/dev/null && { echo "Quake is already running" >&2; exit 3; }
steamvr_root=$(steamvr path)
vrcmd="$steamvr_root/bin/linuxarm64/vrcmd"
[[ -x "$vrcmd" ]] || { echo "SteamVR settings CLI not found: $vrcmd" >&2; exit 2; }
read_state() { python3 "$state_probe"; }
has_pose() { python3 -c 'import json,sys; s=json.load(sys.stdin); sys.exit(0 if any(d.get("class")=="HMD" and d.get("connected") and d.get("pose_valid") for d in s.get("devices",[])) else 1)'; }
focus_pid() { python3 -c 'import json,sys; print(int(json.load(sys.stdin).get("compositor_scene_focus_pid",0)))'; }
scene_ready() {
  local expected_pid=$1
  python3 -c 'import json,sys; s=json.load(sys.stdin); pid=int(sys.argv[1]); auto=sys.argv[2]=="1"; sys.exit(0 if int(s.get("compositor_scene_focus_pid",0)) == pid and (auto or not s.get("dashboard_visible",False)) else 1)' "$expected_pid" "$auto"
}
get_setting() {
  local kind=$1 key=$2 raw
  raw=$("$vrcmd" "--settings-$kind" "$key")
  if [[ "$raw" == "$key="* ]]; then printf '%s\n' "${raw#*=}"; fi
}
set_setting_checked() {
  local kind=$1 key=$2 value=$3 raw actual expected
  "$vrcmd" "--set-settings-$kind" "$key" "$value" >/dev/null
  raw=$(get_setting "$kind" "$key")
  actual=${raw#*=}
  expected=$value
  if [[ "$kind" == bool ]]; then
    case "$actual" in true|1) actual=true ;; false|0) actual=false ;; esac
    case "$expected" in true|1) expected=true ;; false|0) expected=false ;; esac
  fi
  [[ -n "$raw" && "$actual" == "$expected" ]] || {
    echo "SteamVR setting verification failed: $key expected=$expected actual=${actual:-unset}" >&2
    return 1
  }
}
state=$(read_state)
printf '%s\n' "$state" >"$out_base/preflight-state.json"
if ! printf '%s\n' "$state" | has_pose; then
  if [[ "$auto" == 1 ]]; then echo "NOTE: HMD pose is not valid (no wearer); continuing because QVR_AUTO=1." >&2
  else echo "HMD pose is not valid; wear the headset and retry. No game files were touched." >&2; exit 5; fi
fi
original_record_stats=$(get_setting bool perfcheck.recordStats)
original_texture_view=$(get_setting int perfcheck.drawPerfCriteriaGameTextures)
[[ "$original_record_stats" == true || "$original_record_stats" == false ]] || {
  echo "Could not read SteamVR Record VR Performance setting." >&2; exit 8;
}
[[ "$original_texture_view" =~ ^[0-9]+$ ]] || original_texture_view=0
# QVR_MOTION_SMOOTHING=0|1 forces SteamVR's own motion smoothing off/on for the run and restores
# the original value on exit; "keep" (default) leaves it alone. Smoothing off isolates what the
# compositor does with the app's XR_EXT_frame_synthesis inputs from its generic fallback.
smoothing=${QVR_MOTION_SMOOTHING:-keep}
[[ "$smoothing" =~ ^(keep|0|1)$ ]] || { echo "QVR_MOTION_SMOOTHING must be keep, 0 or 1" >&2; exit 2; }
original_smoothing=$(get_setting bool steamvr.motionSmoothing)
[[ "$original_smoothing" == true || "$original_smoothing" == false ]] || original_smoothing=true
# QVR_MOTION_OVERRIDE / QVR_THROTTLE write the per-application overrides that SteamVR's
# "Per-Application Video Settings" page uses (<app key>.motionSmoothingOverride and
# <app key>.framesToThrottle) for the scene app's key, and restore them on exit. An override that
# was unset before the run is restored to 0 because vrcmd cannot delete a key.
app_key=${QVR_APP_KEY:-system.generated.openxr.quake vr.ironwail}
motion_override=${QVR_MOTION_OVERRIDE:-keep}
throttle_override=${QVR_THROTTLE:-keep}
[[ "$motion_override" =~ ^(keep|-?[0-9]+)$ ]] || { echo "QVR_MOTION_OVERRIDE must be keep or an integer" >&2; exit 2; }
[[ "$throttle_override" =~ ^(keep|[0-9]+)$ ]] || { echo "QVR_THROTTLE must be keep or a non-negative integer" >&2; exit 2; }
original_motion_override=$(get_setting int "$app_key.motionSmoothingOverride")
original_throttle=$(get_setting int "$app_key.framesToThrottle")
if [[ -n "${QVR_TEXTURE_VIEW:-}" ]]; then
  texture_view=$QVR_TEXTURE_VIEW
elif [[ "$precheck" == 1 ]]; then
  texture_view=2
else
  # Debug texture views change the rendered image and can contaminate a
  # performance/quality comparison. Keep them for the short precheck only.
  texture_view=0
fi
[[ "$texture_view" =~ ^[012]$ ]] || { echo "QVR_TEXTURE_VIEW must be 0 (off), 1 (depth), or 2 (motion)." >&2; exit 2; }
settings_snapshot="$out_base/steamvr-settings-original.json"
python3 "$settings_helper" snapshot "$settings_file" "$settings_snapshot"
settings_changed=0
restore_steamvr_settings() {
  if [[ "$settings_changed" == 1 ]]; then
    local restore_failed=0
    set_setting_checked bool perfcheck.recordStats "$([[ "$original_record_stats" == true ]] && echo 1 || echo 0)" || restore_failed=1
    set_setting_checked int perfcheck.drawPerfCriteriaGameTextures "$original_texture_view" || restore_failed=1
    if [[ "$smoothing" != keep ]]; then
      if [[ "$original_smoothing" == true ]]; then set_setting_checked bool steamvr.motionSmoothing 1 || restore_failed=1
      else set_setting_checked bool steamvr.motionSmoothing 0 || restore_failed=1; fi
    fi
    if [[ "$motion_override" != keep ]]; then
      set_setting_checked int "$app_key.motionSmoothingOverride" "${original_motion_override:-0}" || restore_failed=1
    fi
    if [[ "$throttle_override" != keep ]]; then
      set_setting_checked int "$app_key.framesToThrottle" "${original_throttle:-0}" || restore_failed=1
    fi
    python3 "$settings_helper" restore "$settings_file" "$settings_snapshot" || restore_failed=1
    settings_changed=0
    if [[ "$restore_failed" == 1 ]]; then echo "WARNING: could not fully restore original SteamVR performance settings." >&2; fi
  fi
}
trap restore_steamvr_settings EXIT
settings_changed=1
set_setting_checked bool perfcheck.recordStats 1
set_setting_checked int perfcheck.drawPerfCriteriaGameTextures "$texture_view"
if [[ "$smoothing" != keep ]]; then
  # vrcmd --set-settings-bool reads the value as a number ("true" would set it to 0).
  set_setting_checked bool steamvr.motionSmoothing "$smoothing"
fi
if [[ "$motion_override" != keep ]]; then
  set_setting_checked int "$app_key.motionSmoothingOverride" "$motion_override"
fi
if [[ "$throttle_override" != keep ]]; then
  set_setting_checked int "$app_key.framesToThrottle" "$throttle_override"
fi
printf 'recordStats=true\ndrawPerfCriteriaGameTextures=%s\nmotionSmoothing=%s\nappKey=%s\nmotionSmoothingOverride=%s\nframesToThrottle=%s\n' "$texture_view" "$smoothing" "$app_key" "$motion_override" "$throttle_override" >"$out_base/steamvr-settings-during-test.txt"
echo "Ready. SteamVR performance recording is on; headset texture view is $texture_view (0=off, 1=depth, 2=motion). Original values will be restored on exit."
runner_pid=""
collector_pid=""
cleanup() {
  if [[ -n "$runner_pid" ]] && kill -0 "$runner_pid" 2>/dev/null; then kill -INT "$runner_pid" 2>/dev/null || true; wait "$runner_pid" 2>/dev/null || true; fi
  if [[ -n "$collector_pid" ]] && kill -0 "$collector_pid" 2>/dev/null; then kill -INT "$collector_pid" 2>/dev/null || true; wait "$collector_pid" 2>/dev/null || true; fi
}
trap 'cleanup; exit 130' INT
trap 'cleanup; exit 143' TERM
if [[ -n "${QVR_TEST_SPECS:-}" ]]; then
  IFS=',' read -r -a test_specs <<<"$QVR_TEST_SPECS"
  ((${#test_specs[@]} > 0)) || { echo "QVR_TEST_SPECS must name at least one test leg." >&2; exit 2; }
  for spec in "${test_specs[@]}"; do
    [[ "$spec" =~ ^[A-Za-z0-9_-]+:[01]$ ]] || {
      echo "Invalid QVR_TEST_SPECS entry '$spec'; expected tag:0 or tag:1." >&2; exit 2;
    }
  done
elif [[ "$precheck" == 1 ]]; then
  test_specs=(on-precheck:1)
else
  test_specs=(off-a1:0 on-b1:1 on-b2:1 off-a2:0)
fi
for spec in "${test_specs[@]}"; do
  # qvrbench tags are also filenames. Colons in the ON/OFF selector are
  # replaced by underscores by the engine, which used to make smoke validation
  # look for a JSON file that had already been written under another name.
  safe_spec=${spec//:/_}
  tag="spacewarp-$(date -u +%Y%m%d-%H%M%S)-$safe_spec"
  # Keep the optional cadence hint constant so this ABBA isolates the
  # synthesis path. Test the hint separately with ON/ON captures.
  relaxed=${QVR_RELAXED_INTERVAL_OFF:-0}
  if [[ "${spec##*:}" == 1 ]]; then relaxed=${QVR_RELAXED_INTERVAL_ON:-0}; fi
  out="$out_base/$tag"
  mkdir -p "$out"
  steam_logs="$HOME/.local/share/Steam/logs"
  [[ -d "$steam_logs" ]] || { echo "SteamVR log directory is missing: $steam_logs" >&2; exit 10; }
  find "$steam_logs" -maxdepth 1 -type f -name 'perfrecording-*.csv' -print | sort >"$out/perfrecording-before.txt"
  recording_started=$(date +%s.%N)
  sha256sum "$binary" >"$out/binary.sha256"
  printf '{"started_unix":%s,"binary":"%s"}\n' "$recording_started" "$binary" >"$out/run-identity-start.json"
  state=$(read_state)
  printf '%s\n' "$state" >"$out/preflight-state.json"
  if ! printf '%s\n' "$state" | has_pose && [[ "$auto" != 1 ]]; then echo "HMD tracking was lost before $tag" >&2; exit 5; fi
  # Isolate each A/B leg into its own SteamVR CSV; turning recording off after
  # the app exits also asks SteamVR to finish writing that run's summary.
  set_setting_checked bool perfcheck.recordStats 1
    QVR_ROOT="$root" QVR_GAME="$game" QVR_SOURCE="$source" QVR_BIN="$binary" \
    QVR_OUT="$out" QVR_TAG="$tag" QVR_SCENARIO="$scenario" QVR_FRAMES="$frames" QVR_HZ="$hz" \
    QVR_DEMO="$demo" QVR_DEMO_MAP="$demo_map" \
    QVR_RELAXED_INTERVAL="$relaxed" \
    QVR_FRAME_SYNTHESIS="${spec##*:}" QVR_VULKAN_INTEROP=1 QVR_VULKAN_DEFER_WAIT="$defer_wait" \
    QVR_SRGB_MOTION=0 QVR_XR_UNFOCUSED_PAUSE=0 QVR_DEFER_STEAMVR_CSV=1 \
    QVR_DETAIL_PROFILE=1 QVR_PROFILE_INTERVAL=120 bash "$smoke" >"$out/runner.log" 2>&1 &
  runner_pid=$!
  scene_pid=0
  for _ in $(seq 1 40); do
    kill -0 "$runner_pid" 2>/dev/null || break
    state=$(read_state) || state='{}'
    printf '%s\n' "$state" >"$out/focus-state.json"
    scene_pid=$(printf '%s\n' "$state" | focus_pid 2>/dev/null || echo 0)
    game_pid=$(pgrep -n -x ironwail || true)
    [[ -n "$game_pid" && "$scene_pid" != 0 && "$scene_pid" == "$game_pid" ]] && \
      scene_ready "$game_pid" <<<"$state" && break
    sleep 0.5
  done
  if [[ "$scene_pid" == 0 || -z "${game_pid:-}" || "$scene_pid" != "$game_pid" ]] || \
     ! scene_ready "$game_pid" <<<"$state"; then
    echo "Quake did not become the focused compositor scene with the SteamVR dashboard dismissed; aborting and restoring temporary files." >&2
    kill -INT "$runner_pid" 2>/dev/null || true
    wait "$runner_pid" 2>/dev/null || true
    runner_pid=""
    exit 6
  fi
  target_game_pid=$game_pid
  printf '%s\n' "$target_game_pid" >"$out/quake-pid.txt"
  echo "Collecting SteamVR compositor timings for focused Quake PID $scene_pid (bounded by app exit, max $seconds seconds)"
  "$collector" --run "$seconds" >"$out/compositor-timings.jsonl" 2>"$out/compositor-timings.stderr" &
  collector_pid=$!
  set +e
  # The smoke script remains alive while it checks and copies its output. Tie
  # the compositor trace to Quake's lifetime rather than that later work, so a
  # dashboard transition after the game closes cannot contaminate attribution.
  # The benchmark prints BENCH_DONE (live in the -condebug console log; the
  # launcher.log stdout is block-buffered until exit) before the engine tears the OpenXR session
  # down. Stop the trace there: the compositor's scene/renderer changes during
  # that teardown while the process is still alive, which the collector would
  # (correctly) report as a focus change and invalidate a complete run.
  while kill -0 "$collector_pid" 2>/dev/null && kill -0 "$target_game_pid" 2>/dev/null; do
    if [[ ! -r "/proc/$target_game_pid/comm" ]] || [[ "$(cat "/proc/$target_game_pid/comm" 2>/dev/null)" != ironwail ]]; then
      break
    fi
    if grep -Fq BENCH_DONE "$game/qconsole.log" 2>/dev/null; then
      break
    fi
    sleep 0.05
  done
  recording_ended=$(date +%s.%N)
  printf '{"ended_unix":%s}\n' "$recording_ended" >"$out/run-identity-end.json"
  if kill -0 "$collector_pid" 2>/dev/null; then kill -INT "$collector_pid" 2>/dev/null || true; fi
  wait "$collector_pid"
  collector_status=$?
  collector_pid=""
  wait "$runner_pid"
  runner_status=$?
  runner_pid=""
  set -e
  echo "$collector_status" >"$out/compositor-timings-exit.txt"
  echo "$runner_status" >"$out/runner-exit.txt"
  if (( runner_status != 0 )); then
    echo "Smoke process failed with status $runner_status; refusing to use compositor data." >&2
    exit "$runner_status"
  fi
  if (( collector_status != 0 )); then
    echo "Compositor collection failed or scene/focus changed while Quake was alive (status $collector_status)." >&2
    exit 9
  fi
  python3 - "$out/compositor-timings.jsonl" <<'PY'
import json
import sys
from pathlib import Path

summaries = []
for line in Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines():
    try:
        row = json.loads(line)
    except json.JSONDecodeError:
        continue
    if row.get("type") == "summary":
        summaries.append(row)
if len(summaries) != 1:
    raise SystemExit("INDETERMINATE: compositor collector did not write exactly one summary")
summary = summaries[0]
if "scene_changed_while_app_alive" not in summary:
    raise SystemExit("INDETERMINATE: collector binary predates in-app focus-loss attribution")
if summary["scene_changed_while_app_alive"]:
    raise SystemExit("INDETERMINATE: compositor focus/renderer changed while Quake was alive")
if summary.get("stop_reason") not in {"runner_finished", "scene_changed_after_app_exit"}:
    raise SystemExit(f"INDETERMINATE: unexpected compositor collection stop: {summary.get('stop_reason')}")
PY
  set_setting_checked bool perfcheck.recordStats 0
  python3 "$recording_collector" --logs "$steam_logs" --out "$out" \
    --baseline "$out/perfrecording-before.txt" --started-at "$recording_started" \
    --ended-at "$recording_ended" --wait-seconds "$csv_wait" --binary "$binary" \
    >"$out/steamvr-recording-collection.json" || {
      echo "INDETERMINATE: SteamVR recording is missing, ambiguous, incomplete, or not attributable to Quake." >&2
      exit 11
    }
  if [[ ! -s "$out/perfcriteria.txt" ]]; then
    echo "WARNING: SteamVR perfcriteria.txt was not captured for $tag; headset texture/assessment diagnostics may be unavailable."
  fi
  state=$(read_state)
  printf '%s\n' "$state" >"$out/postrun-state.json"
  if [[ "${spec##*:}" == 1 ]]; then
    recordings=("$out"/perfrecording-*.csv)
    python3 "$headset_validator" \
      "$out/qvr_openxr.txt.test" "${recordings[0]}" \
      --require-focused $([[ "$auto" == 1 ]] || echo --require-controllers) \
      --expected-app '(?i)(ironwail|quake|unleashed)' --output "$out/headset-validation.json" || {
        validator_status=$?
        # Unattended runs have no wearer, so the head-pose check always fails;
        # the verdict stays in headset-validation.json for the report.
        if [[ "$auto" == 1 ]]; then echo "NOTE: headset validation reported invalid (exit $validator_status); see $out/headset-validation.json" >&2
        else exit "$validator_status"; fi
      }
  fi
  echo "Completed $tag (runner exit $runner_status, collector exit $collector_status)"
done
if [[ "$precheck" == 1 ]]; then
  echo "Precheck data saved under $out_base; inspect the motion/depth texture view before running the full A-B-B-A sequence."
else
  echo "A-B-B-A data saved under $out_base"
fi
echo "Compare poses, scene PID, OpenXR freshness, 0x08 timings, Valve texture/CSV data and wearer-visible quality together."
echo "Generic motion-smoothing flags alone do not prove use of XR_EXT_frame_synthesis inputs."

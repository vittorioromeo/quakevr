#!/usr/bin/env bash
set -euo pipefail
root=/home/steamos/Games/QuakeVR-Unleashed-native
game="$root/game"
out="$root/performance-results/${QVR_PROBE_OUT:-spacewarp-srgb-render-live-20261013}"
cfg="$game/quakevr/quakevr.cfg"
usercfg="$game/quakevr/ironwail.cfg"
openxrlog="$game/quakevr/qvr_openxr.txt"
mkdir -p "$out"
backup_cfg="$out/quakevr.cfg.original"
backup_usercfg="$out/ironwail.cfg.original"
backup_console="$out/qconsole.log.original"
backup_openxr="$out/qvr_openxr.txt.original"
if [[ -e "$backup_cfg" || -e "$backup_usercfg" || -e "$backup_console" || -e "$backup_openxr" ]]; then echo "Backups already exist: $out" >&2; exit 2; fi
cp -p "$cfg" "$backup_cfg"
cp -p "$usercfg" "$backup_usercfg"
[[ ! -e "$game/qconsole.log" ]] || cp -p "$game/qconsole.log" "$backup_console"
[[ ! -e "$openxrlog" ]] || cp -p "$openxrlog" "$backup_openxr"
restore_cfg() {
  [[ ! -e "$game/qconsole.log" ]] || cp -p "$game/qconsole.log" "$out/qconsole.log.test"
  [[ ! -e "$openxrlog" ]] || cp -p "$openxrlog" "$out/qvr_openxr.txt.test"
  cp -p "$backup_cfg" "$cfg"
  cp -p "$backup_usercfg" "$usercfg"
  if [[ -e "$backup_console" ]]; then cp -p "$backup_console" "$game/qconsole.log"; else rm -f "$game/qconsole.log"; fi
  if [[ -e "$backup_openxr" ]]; then cp -p "$backup_openxr" "$openxrlog"; else rm -f "$openxrlog"; fi
}
trap restore_cfg EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
python3 - "$cfg" "${QVR_FRAME_SYNTHESIS:-1}" "${QVR_SRGB_MOTION:-1}" "${QVR_MOTION_RANGE:-0.125}" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
enabled, srgb, motion_range = sys.argv[2:]
s = p.read_text()
needle = "vr_enabled 1"
if needle not in s: raise SystemExit("could not find vr_enabled 1 in quakevr.cfg")
names = ("vr_xr_frame_synthesis", "vr_xr_frame_synthesis_srgb_motion", "vr_xr_frame_synthesis_motion_range")
names += ("vr_xr_unfocused",)
s = "\n".join(line for line in s.split("\n") if not any(line.startswith(name + " ") for name in names))
settings = f"vr_xr_frame_synthesis {enabled}\nvr_xr_frame_synthesis_srgb_motion {srgb}\nvr_xr_frame_synthesis_motion_range {motion_range}\nvr_xr_unfocused 0\n"
s = s.replace(needle, settings + needle, 1)
p.write_text(s)
PY
cd "$game"
export SDL_VIDEODRIVER=x11 DISPLAY="${DISPLAY:-:1}" XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"
export XR_RUNTIME_JSON="${XR_RUNTIME_JSON:-/opt/steamvr/steamxr_linuxarm64.json}"
./ironwail-spacewarp-srgb-test -window -vr-hide-mirror -qvr-frame-diagnostics -basedir "$game" -game quakevr -nobindless -condebug +vr_memstats_log 0 +vr_mirror 0 +exec spacewarp-live.cfg >"$out/launcher.log" 2>&1

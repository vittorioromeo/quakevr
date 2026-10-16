#!/usr/bin/env bash
# Steam Frame toggle test. Run from Git Bash on the PC, with the Frame reachable as `ssh frame`.
#
# Before running, start the dump watcher in a PC terminal and type your sudo password there:
#   ssh -t frame 'sudo sh -c "$(cat ~/qvr-devcore-watch.sh)"'
# This script never asks for or handles a password. It only warns if the watcher is not running.
#
# What it does: waits for Quake (start it from the Steam shortcut, headset on), toggles
# perfcheck.recordStats five times with vrcmd while Quake runs, restores the original value,
# waits for Quake to exit (20 minutes at most), then copies the logs and binary hashes into
# performance-results/frame-toggle-<time>/.
set -u
# Git Bash would rewrite arguments starting with / (such as /opt/...) before ssh sees them.
export MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'

VRCMD=/opt/steamvr/bin/linuxarm64/vrcmd
KEY=perfcheck.recordStats
REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="$REPO/performance-results/frame-toggle-$(date +%Y%m%d-%H%M%S)"
LOG="$OUT/toggle-log.txt"

rssh() { ssh frame "$@"; }
get_value() { rssh "$VRCMD --settings-bool $KEY" | sed -n "s/^$KEY=//p"; }
set_value() { rssh "$VRCMD --set-settings-bool $KEY $1" >/dev/null; }

rssh 'pgrep -x vrserver >/dev/null' || { echo "SteamVR is not running on the Frame. Stopping."; exit 1; }
if rssh 'systemctl is-active --quiet qvr-devcore-watch.service || pgrep -f "[/]sys/class/devcoredump" >/dev/null'; then
  echo "Dump watcher: running"
else
  echo "WARNING: the dump watcher is not running. Continuing without a GPU dump."
fi

echo "Start Quake from the Steam shortcut and put the headset on. Waiting up to 10 minutes."
for _ in $(seq 1 120); do
  rssh 'pgrep -x ironwail >/dev/null' && break
  sleep 5
done
rssh 'pgrep -x ironwail >/dev/null' || { echo "Quake did not start within 10 minutes. Stopping."; exit 1; }
start=$(rssh 'date "+%F %T"')
echo "Quake running since $start. Settling for 90 seconds before toggling."
sleep 90

mkdir -p "$OUT"
orig=$(get_value)
if [ -z "$orig" ]; then
  echo "Could not read $KEY with vrcmd. Stopping before any change." | tee "$LOG"
  exit 1
fi
echo "start $start original $KEY=$orig" | tee "$LOG"
for i in 1 2 3 4 5; do
  for v in 1 0; do
    set_value "$v"
    now=$(get_value)
    echo "$(date +%H:%M:%S) cycle $i set=$v read=$now" | tee -a "$LOG"
    sleep 20
  done
done
set_value "$orig"
echo "$(date +%H:%M:%S) restored $KEY=$orig" | tee -a "$LOG"

echo "Watching Quake until it exits (20 minutes at most)."
for _ in $(seq 1 240); do
  rssh 'pgrep -x ironwail >/dev/null' || break
  sleep 5
done
end=$(rssh 'date "+%F %T"')
echo "end $end" | tee -a "$LOG"

echo "Copying logs to $OUT"
rssh 'cat ~/Games/QuakeVR-Unleashed-native/game/quakevr/qvr_openxr.txt' > "$OUT/qvr_openxr.txt"
rssh 'cat ~/Games/QuakeVR-Unleashed-native/game/qconsole.log' > "$OUT/qconsole.log"
rssh 'cat ~/.local/share/Steam/logs/vrcompositor.txt' > "$OUT/vrcompositor.txt"
rssh "journalctl -k --no-pager --since '$start'" > "$OUT/kernel-journal.txt"
rssh 'sha256sum ~/Games/QuakeVR-Unleashed-native/game/ironwail ~/Games/QuakeVR-Unleashed-native/game/run-native.sh' > "$OUT/frame-binaries.txt"
scp -r frame:devcore-dumps "$OUT/" 2>/dev/null || echo "No GPU dumps copied (none in ~/devcore-dumps)."
(cd "$OUT" && sha256sum ./*.txt ./*.log > SHA256SUMS.txt 2>/dev/null)
echo "Done. Logs in $OUT"

#!/bin/bash
# Runs inside WSL archlinux: the self-contained linux-x64 Avalonia window, headless (no display used), every page to PNG.
W=${QVR_REPO:-/mnt/c/OHWorkspace/qvr-agents/testsplit}  # the repository, as WSL sees it
OUT=${QVR_OUT:-$W/scratch/xi2_shots_linux}
mkdir -p $OUT
echo "WSLg: $(ls -d /mnt/wslg 2>/dev/null || echo none); DISPLAY=${DISPLAY:-unset}"
# DISPLAY unset: the harness must never open a window on the desktop.
env -u DISPLAY -u WAYLAND_DISPLAY timeout 300 ${QVR_WINDOW:-$W/scratch/xi2_linux_gui/QuakeVR-Setup} --screenshots $OUT --offline > $OUT/run.log 2>&1
echo "exit $?"
tail -4 $OUT/run.log
grep -E "FAILED|host:|harness" $OUT/report.txt

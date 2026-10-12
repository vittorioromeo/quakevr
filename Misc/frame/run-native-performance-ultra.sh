#!/bin/bash
set -eu
cd -- "$(dirname -- "$0")"
export SDL_VIDEODRIVER=x11
export DISPLAY="${DISPLAY:-:1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"
export XR_RUNTIME_JSON="${XR_RUNTIME_JSON:-/opt/steamvr/steamxr_linuxarm64.json}"
exec ./ironwail -window -vr-hide-mirror -basedir "$PWD" -game quakevr \
  -nobindless -condebug "$@" +vr_memstats_log 0 +vr_mirror 0 \
  +exec frame-performance-preview-ultra.cfg

#!/bin/bash
set -eu
cd -- "$(dirname -- "$0")"
export SDL_VIDEODRIVER=x11
export DISPLAY="${DISPLAY:-:1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"
export XR_RUNTIME_JSON="${XR_RUNTIME_JSON:-/opt/steamvr/steamxr_linuxarm64.json}"
# Turnip's autotuner picks GMEM or sysmem for each render pass; mixing them with UBWC images made the decal-heavy world
# shader fault the GPU (killing a monster sometimes hung the headset). Sysmem every time never did, and is no slower.
export TU_AUTOTUNE_ALGO="${TU_AUTOTUNE_ALGO:-prefer_sysmem}"
first_run=()
if [ ! -f quakevr/ironwail.cfg ]; then
  first_run=(+vr_graphics_preset 0 +vr_render_scale 0.45 +vid_fsaa 0)
fi
exec ./ironwail -window -vr-hide-mirror -basedir "$PWD" -game quakevr \
  -nobindless -condebug +vr_memstats_log 0 +vr_mirror 0 "${first_run[@]}" "$@"

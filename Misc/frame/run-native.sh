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
# prefer_sysmem alone did NOT hold on the headset: the autotuner still ran the world pass in GMEM (a worn headset hits
# its fdm / preempt_latency_risk cases) and the first kill hung the GPU. TU_DEBUG=sysmem forces it; it never hung in any
# test and is about 3 % faster. The root cause is not found, see docs/STEAM-FRAME-DEVELOPMENT.md.
export TU_DEBUG="${TU_DEBUG:-sysmem}"
# On a lost GPU abort at once instead of drawing on; a hang also takes SteamVR down.
export MESA_VK_ABORT_ON_DEVICE_LOSS=1
first_run=()
if [ ! -f quakevr/ironwail.cfg ]; then
  first_run=(+vr_graphics_preset 0 +vr_render_scale 0.45 +vid_fsaa 0)
fi
# +vr_decals_world 0: the world's decals as meshes, so the world shader's decal loop (the hang's trigger) never runs.
# Later arguments win, so "+vr_decals_world 1" after the launcher name turns it back on.
exec ./ironwail -window -vr-hide-mirror -basedir "$PWD" -game quakevr \
  -nobindless -condebug +vr_memstats_log 0 +vr_mirror 0 +vr_decals_world 0 "${first_run[@]}" "$@"

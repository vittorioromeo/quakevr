#!/usr/bin/env bash
# Starts the Godot frame-synthesis test with the environment Godot needs for X11 + the SteamVR OpenXR runtime.
# Arguments after `--` are user arguments for main.gd (see its header), e.g.: run-godot-test.sh -- --fs on --seconds 18
export DISPLAY=${DISPLAY:-:0}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/1000}
export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/1000/bus}
export XR_RUNTIME_JSON=${XR_RUNTIME_JSON:-/opt/steamvr/steamxr_linuxarm64.json}
cd "$(dirname "$0")" || exit 4
exec ./spacewarp-godot.arm64 --display-driver x11 --rendering-method mobile --xr-mode on "$@"

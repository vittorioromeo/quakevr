#!/bin/bash
# xr_session_test.sh [sections] -- the OpenXR session's faults on the fake headset (Misc/quakevr/fakexr, built first),
# the game run hidden on Linux (docs/vr-port/TESTING.md, "OpenXR runtime choice": fakexr; its archived recipe). Prints
# `xr_session: PASS|FAIL <what>` per check and the verdict `xr_session: N passed, M failed`.
#   [sections]: comma-separated numbers (default all):
#   1. the runtime gone without a loss event (FAKEXR_LOST): XR_ERROR_SESSION_LOST / XR_ERROR_INSTANCE_LOST from
#      xrPollEvent, xrWaitFrame, xrBeginFrame or xrEndFrame end the session: flat, said why, the game runs on.
#   2. frames with no session running (FAKEXR_END restart with FAKEXR_IDLE_MS: no xrWaitFrame for 1.5 s) paced at about
#      a display period with host_maxfps 1000.
# Linux only (the game run directly, as LINUX.md's; SDL on X11 with GLX: fakexr offers XR_KHR_opengl_enable alone).
# QVR_ENGINE: the game (default build/ironwail); QUAKE_DIR: Quake's data (default Steam's).
SECTIONS="${1:-all}"
want() {
    local spec
    for spec in ${SECTIONS//,/ }; do
        [ "$spec" = all ] || [ "$spec" = "$1" ] && return 0
    done
    return 1
}
TREE=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
QT_NAME=xr_session
source "$TREE/Misc/quakevr/qvrtest.sh"
ENGINE="${QVR_ENGINE:-$TREE/build/ironwail}"
QUAKE_DIR="${QUAKE_DIR:-$HOME/.local/share/Steam/steamapps/common/Quake}"
OUT="$TREE/scratch/xrsession"
F="$TREE/scratch/fakexr"
mkdir -p "$OUT"
if [ -f "$F/fakexr_steam.so" ] && [ -z "$(find "$TREE/Misc/quakevr/fakexr" -newer "$F/fakexr_steam.so" -type f)" ]; then
    echo "fakexr up to date: $F"
else
    bash "$TREE/Misc/quakevr/fakexr/build.sh" "$TREE" | tail -1
fi
XRLOG="$TREE/quakevr/qvr_openxr.txt"
unset FAKEXR_LOST FAKEXR_END FAKEXR_IDLE_MS FAKEXR_UNFOCUS

# game <tag> <script> [VAR=value...]: one hidden run on fakexr_steam (waitN: N waits), its console in $OUT/<tag>.log,
# fakexr's log in <tag>_fake.log, qvr_openxr.txt copied to <tag>_openxr.txt.
game() {
    local tag="$1" script="$2" expanded="" c i
    shift 2
    IFS=';' read -ra parts <<<"$script"
    for c in "${parts[@]}"; do
        if [[ "$c" =~ ^wait([0-9]+)$ ]]; then
            for ((i = 0; i < BASH_REMATCH[1]; i++)); do expanded+="wait;"; done
        else
            expanded+="$c;"
        fi
    done
    : > "$OUT/${tag}_fake.log"
    (cd "$TREE" && env SDL_VIDEODRIVER=x11 SDL_VIDEO_X11_FORCE_EGL=0 QVR_TEST_HIDDEN=1 QVR_TEST_BACKGROUND=1 QVR_NO_ERROR_DIALOG=1 \
        XR_RUNTIME_JSON="$F/steam/steamxr_win64.json" FAKEXR_HEADSET=fakexr_steam FAKEXR_LOG="$OUT/${tag}_fake.log" "$@" \
        timeout 240 "$ENGINE" -nosound -noconfigwrite -window -width 640 -height 400 -basedir "$QUAKE_DIR" -basedir . -game quakevr \
        +"${expanded%;}" > "$OUT/$tag.log" 2>&1
    echo "exit=$?" >> "$OUT/$tag.log")
    cp -f "$XRLOG" "$OUT/${tag}_openxr.txt"
}
# inorder <file> <regex>...: 1 if each regex matches a line after the previous one's, else 0.
inorder() {
    local file="$1" from=1 n re
    shift
    for re in "$@"; do
        n=$(tail -n +$from "$file" | grep -n -a -E -m1 -- "$re" | cut -d: -f1)
        [ -z "$n" ] && { echo 0; return; }
        from=$((from + n))
    done
    echo 1
}
none() { grep -q -a -E -- "$2" "$1" && echo 0 || echo 1; } # none <file> <regex>: 1 if no line matches
count() { grep -c -a -E -- "$2" "$1"; }

if want 1; then
    # Lost at xrWaitFrame 60 (the menus' frames, the panel's), or 120 frames into a map (the eyes' frames).
    for c in poll:instance wait:session begin:session end:session end:instance:map; do
        call="${c%%:*}"; rest="${c#*:}"; code="${rest%%:*}"; inmap=0; [ "$rest" != "$code" ] && inmap=1
        tag="lost_${call}_$code"
        if [ $inmap = 1 ]; then
            game $tag "vr_backend openxr;map start;wait200;echo STILLRUNNING;quit" FAKEXR_LOST="120:$call:$code"
        else
            game $tag "vr_backend openxr;wait150;echo STILLRUNNING;quit" FAKEXR_LOST="60:$call:$code"
        fi
        CODE=XR_ERROR_SESSION_LOST; [ "$code" = instance ] && CODE=XR_ERROR_INSTANCE_LOST
        XRCALL=$(case $call in poll) echo xrPollEvent ;; wait) echo xrWaitFrame ;; begin) echo xrBeginFrame ;; *) echo xrEndFrame ;; esac)
        what="$XRCALL returns $code lost$([ $inmap = 1 ] && echo ' (in a map)')"
        check $(inorder "$OUT/$tag.log" "VR: openxr session lost: playing flat" "VR: the session's last states: .*FOCUSED > $CODE$" \
            "VR: stopping openxr backend" "STILLRUNNING" "^exit=0") "$what: flat, said why, the game runs on"
        check $(inorder "$OUT/${tag}_openxr.txt" "$XRCALL: $CODE: the (session|runtime) is lost, no loss event came first") \
            "$what: qvr_openxr.txt says which call"
        check $(inorder "$OUT/${tag}_fake.log" "lost from now \(FAKEXR_LOST\)" "xrDestroySession" "FAKEXR_LOST: [12] calls answered $CODE" \
            "xrDestroyInstance") "$what: taken at the first answer (one call, or two for xrEndFrame's: the stop's own)"
        layers="[1-9][0-9]* projection layers"; [ $inmap = 0 ] && layers="[0-9]+ projection layers .* [1-9][0-9]* quad layers"
        check $(grep -q -a -E "xrDestroySession: [0-9]+ frames: $layers" "$OUT/${tag}_fake.log" && echo 1 || echo 0) \
            "$what: the frames before submitted (the eyes in a map, the panel in the menus)"
    done
fi

if want 2; then
    # Frame 60: STOPPING, IDLE after xrEndSession, READY 1.5 s later; the frames between run no xrWaitFrame.
    game paced "host_maxfps 1000;vr_backend openxr;wait500;echo STILLRUNNING;quit" FAKEXR_END=60:restart FAKEXR_IDLE_MS=1500
    rate=$(grep -a -o -E "the idle over \(FAKEXR_IDLE_MS\): [0-9]+ ms, [0-9]+ frames polled \([0-9]+ a second\)" "$OUT/paced_fake.log" | grep -o -E "\([0-9]+ a second" | grep -o -E "[0-9]+")
    check $([ -n "$rate" ] && [ "$rate" -ge 30 ] && [ "$rate" -le 120 ] && echo 1 || echo 0) \
        "no session running: about a display period a frame (${rate:-no} frames a second at 90 Hz, host_maxfps 1000)"
    check $(inorder "$OUT/paced_openxr.txt" "session STOPPING" "session IDLE" "session READY" "session SYNCHRONIZED" "session FOCUSED") \
        "no session running: the session begun again after the idle"
    check $(inorder "$OUT/paced.log" "STILLRUNNING" "^exit=0") "no session running: the game runs on in VR"
    check $(none "$OUT/paced.log" "session lost|asked the game to exit") "no session running: no end said"
fi

verdict
exit $fail

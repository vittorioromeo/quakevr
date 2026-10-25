#!/bin/bash
# check_nopch.sh [tree] -- builds the engine without the VR module's precompiled header (Quake/vr/vr_pch.hpp), which the
# normal builds force into every Quake/vr .cpp: so a file that uses a header without including it (the PCH having it
# anyway) fails here. Windows: MSBuild Release x64 -p:QvrNoPch=1 -p:QvrNoLto=1, in its own folders
# (Build-ironwail/x64/Release/nopch); elsewhere: CMake -DQVR_PCH=OFF in <tree>/build/nopch (or $QVR_NOPCH_DIR).
# Incremental: after the first run (a full build, about a minute) only the files changed since are compiled again.
# Prints the errors and "no-PCH build: ok" or "no-PCH build: FAILED"; the log is <tree>/scratch/check_nopch.log.
TREE=${1:-$(cd "$(dirname "$0")/../.." && pwd)}
mkdir -p "$TREE/scratch"
LOG="$TREE/scratch/check_nopch.log"
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
    MSB="/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"
    # The engine's project alone (and Zancle, which it references; not the Doom DLL), linked into its own folder.
    "$MSB" "$TREE/Windows/VisualStudio/ironwail.vcxproj" -p:Configuration=Release -p:Platform=x64 \
        -p:QvrNoPch=1 -p:QvrNoLto=1 -p:QvrStableDate=1 -m -v:q -nologo > "$LOG" 2>&1
    rc=$?
    grep -E " error " "$LOG" | sed 's/ \[.*//' | sort -u | head -30
    ;;
*)
    B="${QVR_NOPCH_DIR:-$TREE/build/nopch}"
    if [ ! -f "$B/build.ninja" ] && [ ! -f "$B/Makefile" ]; then
        G=; command -v ninja > /dev/null && G="-G Ninja"
        cmake -S "$TREE" -B "$B" $G -DCMAKE_BUILD_TYPE=Release -DQVR_PCH=OFF ${CMAKE_EXTRA:-} > "$LOG" 2>&1 || { tail -20 "$LOG"; echo "no-PCH build: FAILED (configure)"; exit 1; }
    fi
    cmake --build "$B" -j "$(nproc)" >> "$LOG" 2>&1
    rc=$?
    grep -E "error:" "$LOG" | sort -u | head -30
    ;;
esac
[ $rc -eq 0 ] && echo "no-PCH build: ok" || echo "no-PCH build: FAILED (exit $rc; $LOG)"
exit $rc

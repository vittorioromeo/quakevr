#!/bin/bash
# qvrprof.sh -- one benchmark scenario (qvrbench.py) under a profiler, gameplay and loading kept apart
# (docs/vr-port/PROFILING_2026-10.md).
#   bash Misc/quakevr/bench/qvrprof.sh <agent> <tool> <scenario> <outdir> [frames]
#   tool: vtune-hotspots | vtune-threading | nsys | none
#   env:  MODE=1 (default) VTune collects the benchmark window only; MODE=2 the map loads only (command to first frame);
#         MODE=0 as started (vr_bench_profiler). SETTINGS=<cfg> a settings profile (as bench.sh --settings).
#         PRE=<cfg> / POST=<cfg> lines run just before the window / after it (e.g. vr_profile 1 ... vr_profile_dump).
#         NSYS_DELAY / NSYS_DUR (s): nsys has no ITT hook, so its capture is a delay and a duration from the launch:
#         check them against vr_walltime lines (PRE/POST) so the capture falls inside the window.
# Runs fast (unpaced) and hidden in the agent's kit game folder, like run.sh but without its slot lock: run it alone.
# Results in <outdir>: VTune's vt/, nsys's ns.nsys-rep, the console log, the window's JSON.
# Reports: vtune -report top-down -r <outdir>/vt -format csv -csv-delimiter tab > td.tsv; python vtune_attr.py td.tsv
AGENT=$1; shift
WT=C:/OHWorkspace/qvr-agents/$AGENT; [ "$AGENT" = "cleanup" ] && WT=C:/OHWorkspace/quakevr-iw-cleanup
KIT=C:/OHWorkspace/qvr-kit
TOOL=$1; SC=$2; D=$3; FRAMES=${4:-900}; MODE=${MODE:-1}
[ -n "$D" ] || { echo "usage: qvrprof.sh <agent> <tool> <scenario> <outdir> [frames]"; exit 2; }
PY=$WT/Misc/quakevr/bench/qvrbench.py
mkdir -p "$D"
INFO=$(python "$PY" script "$SC" --out "$D/script.cfg" --tag "prof_$SC" --frames "$FRAMES" --hz 90 --eye 2048 \
    $([ -n "$SETTINGS" ] && echo --settings qvrbench_settings.cfg) --info | tr -d '\r')
BASE=${INFO%% *}
GAME=$KIT/bases/$AGENT/$BASE
python - "$D/script.cfg" "$MODE" "$PRE" "$POST" <<'EOF'
import sys
p, mode, pre, post = sys.argv[1:5]
nl = chr(10)
s = open(p).read().split(nl)
s.insert(1 if s[0].startswith('exec') else 0, 'vr_bench_profiler ' + mode)
s = nl.join(s)
if pre: s = s.replace('echo BENCH_SETUP_DONE', open(pre).read().strip() + nl + 'echo BENCH_SETUP_DONE', 1)
if post: s = s.replace('echo BENCH_DONE', open(post).read().strip() + nl + 'echo BENCH_DONE', 1)
open(p, 'w').write(s)
EOF
cp "$D/script.cfg" "$GAME/id1/qvrbench.cfg"
[ -n "$SETTINGS" ] && cp "$SETTINGS" "$GAME/id1/qvrbench_settings.cfg"
printf 'vr_backend mock\nsv_autosave 0\nvr_mock_fast 1\nexec qvrbench.cfg\n' > "$GAME/id1/autoexec.cfg"
: > "$GAME/qconsole.log"
EXE=$WT/Windows/VisualStudio/Build-ironwail/bin/x64/Release/ironwail.exe
EXEW=$(cygpath -w "$EXE"); GAMEW=$(cygpath -w "$GAME"); DW=$(cygpath -w "$D")
export QVR_NO_ERROR_DIALOG=1 QVR_TEST_BACKGROUND=1 QVR_TEST_HIDDEN=1
ARGS=(-basedir "$GAMEW" -game hipnotic -game rogue -game quakevr -condebug -window -width 960 -height 540 -nosound)
VTUNE="C:/Program Files (x86)/Intel/oneAPI/vtune/latest/bin64/vtune.exe"
NSYS="C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.5.1/target-windows-x64/nsys.exe"
cd "$GAME"
case "$TOOL" in
    # (user-mode sampling: hardware sampling needs VTune's driver and an administrator)
    vtune-hotspots) "$VTUNE" -collect hotspots -start-paused -knob sampling-mode=sw -r "$DW\\vt" -- "$EXEW" "${ARGS[@]}" \
        > "$D/tool.log" 2>&1 ;;
    vtune-threading) "$VTUNE" -collect threading -start-paused -r "$DW\\vt" -- "$EXEW" "${ARGS[@]}" > "$D/tool.log" 2>&1 ;;
    nsys) "$NSYS" profile -t opengl,wddm -s none -y "${NSYS_DELAY:-16}" -d "${NSYS_DUR:-6}" -o "$DW\\ns" -f true \
        "$EXEW" "${ARGS[@]}" > "$D/tool.log" 2>&1 ;;
    none) "$EXE" "${ARGS[@]}" ;;
    *) echo "qvrprof.sh: unknown tool $TOOL"; exit 2 ;;
esac
echo "exit $?"
# The run's config changes thrown away, as run.sh does (the settings profile is archived into ironwail.cfg).
[ -f "$WT/quakevr/ironwail.cfg.baseline" ] && cp "$WT/quakevr/ironwail.cfg.baseline" "$WT/quakevr/ironwail.cfg"
cp "$GAME/qconsole.log" "$D/qconsole.log" 2>/dev/null
grep -E "vr_bench:" "$D/qconsole.log" | cut -c1-200 | head -12
cp "$GAME/quakevr/profile/bench/prof_$SC.json" "$D/bench.json" 2>/dev/null

#!/bin/bash
# scenes.sh <agent> <tag> [scene...] -- the spatial audio's benchmark scenes (vr_snd_bench, ROUND21.md "Spatial audio:
# optimised"), each its own run alone on the machine (run.sh --exclusive), the mock headset at 90 fps in real time with
# sound. Each scene's rows are appended to the game folder's quakevr/sound_tests/bench.csv, labelled <tag>_<scene>, and
# printed. Scenes (all by default): idle combat32 move32 combat64 slow025 slow025wav.
AGENT="$1"; TAG="$2"; shift 2
KIT="C:/OHWorkspace/qvr-kit"
CSV="$KIT/bases/$AGENT/qbase/quakevr/sound_tests/bench.csv"
SCENES="$*"; [ -z "$SCENES" ] && SCENES="idle combat32 move32 combat64 slow025 slow025wav"
COMMON="god;notarget;host_maxfps 90;vr_snd_voices 32${EXTRA:+;$EXTRA}" # (EXTRA: more commands for every scene)
SECONDS_RUN=12
for s in $SCENES; do
    L="${TAG}_$s"
    case $s in
        idle)       S="map e1m1;wait30;$COMMON;wait90;vr_snd_bench $SECONDS_RUN $L" ;;
        combat32)   S="map e1m1;wait30;$COMMON;vr_snd_bench_spawn 16;wait90;vr_snd_bench $SECONDS_RUN $L 8 0" ;;
        move32)     S="map e1m1;wait30;$COMMON;vr_snd_bench_spawn 32;wait90;vr_snd_bench $SECONDS_RUN $L 4 400" ;;
        combat64)   S="map e2m1;wait30;$COMMON;vr_snd_voices 64;vr_snd_bench_spawn 48;wait90;vr_snd_bench $SECONDS_RUN $L 16 400" ;;
        slow025)    S="map e1m1;wait30;$COMMON;vr_snd_bench_spawn 32;vr_timescale 0.25;wait180;vr_snd_bench $SECONDS_RUN $L 4 0" ;;
        slow025wav) S="map e1m1;wait30;$COMMON;vr_snd_bench_spawn 32;vr_timescale 0.25;vr_snd_capture_game 30 bench;wait180;vr_snd_bench $SECONDS_RUN $L 4 0" ;;
        *) echo "unknown scene $s"; continue ;;
    esac
    bash "$KIT/run.sh" --exclusive "$AGENT" -Sound -RealTime -Timeout 120 -Script "$S;wait1400;toggleconsole;quit" \
        -Filter "vr_snd_bench $L:|rror" | grep -v "^$"
    grep "^$L," "$CSV" | tail -n 30
done

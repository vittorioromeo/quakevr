#!/bin/bash
# spawn_census.sh <label> [coop]: the A/B spawn census (TESTING.md "The spawn census"). Loads every map of every
# campaign (id1, Hipnotic, Rogue, Dopa, MG1, MG3; then AD's, in their own run) with sv_random_seed 7 and skill 1, and
# saves each 3 frames after spawn; the saves land in <worktree>/scratch/census_<label>/. `coop` runs Machine Games'
# campaigns (Dopa, MG1, MG3) in coop 1 instead (their coop spawnflags). Compare two labels with
#     python Misc/quakevr/spawn_census.py compare scratch/census_<before> scratch/census_<after>
# Typical use: the census on the base build, then on the change, then compare (must say 0 different, 0 only in one).
# Needs the kit (run.sh) and the official data the kit's qbase and Steam's rerelease hold; a run takes a few minutes.
set -u
L=${1:?usage: spawn_census.sh <label> [coop]}
COOP=0
if [ "${2:-}" = "coop" ]; then COOP=1; fi
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
OUT=$W/scratch/census_$L
mkdir -p "$OUT"
python -I "$W/Misc/quakevr/spawn_census.py" maps "$OUT/maps"
FILTER="census:|Ruleset:|Host_Error|SV_Error|ENGINE|rror|exit=|TIMEOUT|Can't save"
run_group() {
    local log="$1" games="$2"; shift 2
    local S="developer 0;sv_random_seed 7;skill 1;coop $COOP;maxplayers 1"
    local camp m
    for camp in "$@"; do
        S="$S;vr_campaign_native $camp;wait30"
        for m in $(cat "$OUT/maps/maps_$camp.txt"); do
            S="$S;map $m;wait3;save cen_${camp}_$m;echo census: map $camp $m;vr_ruleset_info"
        done
    done
    S="$S;echo census: group done;toggleconsole;quit"
    bash "$K/run.sh" "$NAME" -Games "$games" -ExtraArgs "-nomapindex -noaddons" -Timeout 1500 \
        -QuitOn "census: group done" -Script "$S" -Filter "$FILTER" > "$OUT/run_$log.log" 2>&1
    echo "census: $log: $(grep -c 'census: map' "$OUT/run_$log.log") maps saved, $(grep -c 'census: group done' "$OUT/run_$log.log") done"
}
if [ "$COOP" = 1 ]; then
    run_group coop "-game quakevr" dopa mg1 mg3
else
    run_group official "-game hipnotic -game rogue -game quakevr" id1 hipnotic rogue dopa mg1 mg3
    run_group ad "-game ad -game quakevr" ad
fi
for f in "$W"/quakevr/cen_*.sav; do
    if [ -f "$f" ]; then mv "$f" "$OUT/"; fi
done
echo "census: $L: $(ls "$OUT" | grep -c '\.sav$') saves in $OUT"

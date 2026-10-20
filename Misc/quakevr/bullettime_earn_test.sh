# bullettime_earn_test.sh <agent> [case regex] -- earned bullet time (ROUND22.md, "Earned bullet time"): the meter
# full on a new game, never refilling idle; dealt damage refilling it (half a grunt: 15/180; overkill capped at the
# health left; six grunts from empty to full); damage while it runs at vr_bullettime_earn_active; kept by a save and
# a level change; the classic mode (0) refilling over time and ignoring damage. Each case is one game run.
name=${1:-earnedbt}
F=${2:-.*}
WT="C:/OHWorkspace/qvr-agents/$name"
P='vr_fixed_frames 1; developer 1; vr_debug_bullettime 1; vr_bullettime_earn_damage 180; vr_bullettime_earn_active 0.25; skill 1' # (the cases' numbers: 180 on Normal, the shipped 350 since 2026-10-20)
FIL='bullet time|test hurt|rror'
mkdir -p "$WT/scratch/btearn"
run() { # label, script, regexes that must all appear (;-separated), [regex that must not]
  [[ "$1" =~ $F ]] || return 0
  out="$WT/scratch/btearn/$1.txt"
  bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Script "$P; $2; wait5; toggleconsole; quit" -Filter "$FIL" -Timeout 300 > "$out" 2>&1
  ok=1
  IFS=';' read -ra need <<< "$3"
  for n in "${need[@]}"; do grep -Eq "$n" "$out" || { ok=0; echo "  missing: $n"; }; done
  if [ -n "$4" ] && grep -Eq "$4" "$out"; then ok=0; echo "  unwanted: $4"; fi
  [ $ok = 1 ] && echo "PASS $1" || echo "FAIL $1 ($out)"
}
G='vr_test_spawn 0; vr_test_spawn_dist 150; impulse 241; wait10; vr_test_hurt 100; wait120' # (the body settled before the next: a ragdoll falling on it hurts it, unearned)
run earned "vr_bullettime_meter_mode 1; map e1m1; wait60; god; notarget; vr_bullettime_meter; vr_bullettime_meter 0; wait300; vr_bullettime_meter; vr_test_spawn 0; vr_test_spawn_dist 150; impulse 241; wait10; vr_test_hurt 15; wait5; vr_bullettime_meter; vr_test_hurt 100; wait5; $G; $G; $G; $G; $G; vr_bullettime_meter; vr_bullettime_meter 0.4; wait3; save qvr_btearn_test; wait10; vr_bullettime_meter 0.9; wait3; load qvr_btearn_test; wait90; vr_bullettime_meter; changelevel e1m2; wait90; vr_bullettime_meter; vr_bullettime_meter 0.5; vr_bullettime; wait5; $G; vr_bullettime; wait5" \
  'meter: 1\.000 \(off, kept;meter: 0\.000 \(off, kept.*fills by 180;test hurt: monster_army 15 damage, health 30 -> 1[0-9];earned 1[0-9] damage, meter 0\.000 -> 0\.(0[89]|1[01]);earned [0-9]+ damage, meter 0\.(0[89]|1[01])[0-9]* -> 0\.167;earned 30 damage, meter 0\.833 -> 1\.000;meter: 1\.000 \(off;meter: 0\.400 \(off, kept;earned 30 damage \(running\), meter 0\.4[0-9]+ -> 0\.(4[4-9]|5)[0-9]*' 'meter: 0\.900'
run classic "vr_bullettime_meter_mode 0; map e1m1; wait60; god; notarget; vr_bullettime_meter 0; wait300; vr_bullettime_meter; $G; vr_bullettime_meter" \
  'meter: 0\.(0[5-9]|1|2)[0-9]* \(off, kept, cooldown 0\.0 s; fills over 20 s\)' 'earned'
# The skills' multipliers (vr_bullettime_earn_skill0..3): Nightmare needs 1.5 times (270: 15 damage is 0.056), Easy 0.75
# times (135: 0.111).
run nightmare "vr_bullettime_meter_mode 1; skill 3; map e1m1; wait60; god; notarget; vr_bullettime_meter 0; vr_bullettime_meter; vr_test_spawn 0; vr_test_spawn_dist 150; impulse 241; wait10; vr_test_hurt 15; wait5" \
  'fills by 270 damage dealt \(skill 3\);earned 15 damage, meter 0\.000 -> 0\.056'
run easy "vr_bullettime_meter_mode 1; skill 0; map e1m1; wait60; god; notarget; vr_bullettime_meter 0; vr_bullettime_meter; vr_test_spawn 0; vr_test_spawn_dist 150; impulse 241; wait10; vr_test_hurt 15; wait5" \
  'fills by 135 damage dealt \(skill 0\);earned 15 damage, meter 0\.000 -> 0\.111'

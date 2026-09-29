#!/usr/bin/env bash
# Shots push props (ROUND21.md, "Shots push props"): each weapon fired at a health box (1.6 kg) and at an explosive box
# that never blows up (40 kg, vr_test_spawn 104), on e1m1's start; prints how far each moved and the pushes it took.
# Usage: bash shotpush_test.sh <worktree name> [weapons...] (default: sg ssg ng sng lg)
# Weapon ids (impulse 150 + id, main hand): 4 shotgun, 5 double shotgun, 6 nailgun, 7 super nailgun, 11 lightning.
kit=C:/OHWorkspace/qvr-kit
name=${1:?worktree name}
shift
weapons=${*:-sg ssg ng sng lg}
out=${OUT:-$(mktemp -d)} # (OUT: where the logs go)
for w in $weapons; do
    case $w in
    sg) id=4; hold=3; pitch=45 ;;
    ssg) id=5; hold=3; pitch=45 ;;
    ng) id=6; hold=72; pitch=43 ;;  # 1 s of fire
    sng) id=7; hold=72; pitch=43 ;;
    lg) id=11; hold=72; pitch=43 ;;
    esac
    for box in 100 104; do
        # The spawned box is edict 175 on e1m1 (the same prelude every run). The hand aims at the health box's near top
        # edge (each weapon's muzzle is elsewhere), at the explosive box's middle.
        [ $box = 104 ] && pitch=56
        s="map e1m1;wait60;vr_weapon_grip_mode 1;impulse 9;wait5;impulse $((150 + id));wait5"
        s="$s;vr_mock_hand main -0.05 1.3 -0.3 $pitch 0 0;vr_test_spawn $box;vr_test_spawn_dist 128;impulse 241;wait120"
        s="$s;vr_physics_list 175;vr_debug_box3d 1;+attack;wait$hold;-attack;wait5;vr_debug_box3d 0;wait210"
        s="$s;vr_physics_list 175;toggleconsole;quit"
        bash $kit/run.sh "$name" -Script "$s" -Filter "^  175 |175 pushed|rror" > "$out/$w-$box.txt" 2>&1
        python - "$out/$w-$box.txt" "$w" "$box" <<'EOF'
import re, sys, math
lines = open(sys.argv[1]).read().splitlines()
pos = [tuple(map(float, l.split()[1:4] if l.split()[1][0] in '-0123456789' else l.split()[2:5]))
       for l in lines if l.startswith('  175 ')]
pushes = [float(m.group(1)) for l in lines for m in [re.search(r'pushed .*, ([0-9.]+) N s', l)] if m]
kind = {'100': 'health box', '104': 'explosive box'}[sys.argv[3]]
if len(pos) < 2:
    print(f'{sys.argv[2]:4} {kind:13}: no box found'); sys.exit()
d = math.dist(pos[0][:2], pos[-1][:2])
print(f'{sys.argv[2]:4} {kind:13}: moved {d:6.1f} units ({d * 0.0381:.2f} m), {len(pushes)} pushes, {sum(pushes):.1f} N s')
EOF
    done
done
echo "(logs: $out)"

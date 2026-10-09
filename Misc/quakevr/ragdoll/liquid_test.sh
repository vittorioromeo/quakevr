#!/bin/bash
# liquid_test.sh <agent> [cases] -- ragdolls and monsters falling into water, slime and lava (ROUND21.md, "Ragdolls in
# liquids" and "Bodies burn in lava"): each case drops a grunt from about 200 units over a liquid (the player hovering in
# noclip, 150 units back: his hands hold nothing up) and prints the pelvis's track in it (vr_debug_ragdoll 2: "in liquid
# <contents>: pelvis z, speed up, speed across"; every 40th line), its fall damage ("fall:"), its burning and gibbing.
#   water    vrtesthall's pool (120 deep): a dead grunt; braked as it goes in, floats up face down
#   knight   the same, a dead knight (armour): sinks slowly, lies on the bottom
#   kd       vrtesthall: a live grunt knocked down over the pool (vr_knockdown_test 0): no fall damage
#   fall     vrtesthall: a live grunt falling in standing (SV_Physics_Step): no fall damage
#   slime    e3m1's slime pit: a dead grunt (thicker: braked harder)
#   kdslime  e3m1: a live grunt knocked down into the slime: no fall damage (the slime burns it: vr_enemy_liquid_damage)
#   lava     e1m7's lava: a dead grunt floats, burns, bursts into gibs after vr_burn_lava_gib s
#   kdlava   e1m7: a live grunt knocked down into the lava: dies burning, then gibbed
# OLD=1 runs them as before (liquids' lift, drag, cushion and lava bodies off).
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-water knight kd fall slime kdslime lava kdlava}
X=""
[ -n "$OLD" ] && X="vr_liquid_fall_cushion 0;vr_burn_lava_bodies 0;vr_ragdoll_float_water 0;vr_ragdoll_float_slime 0;vr_ragdoll_float_lava 0;vr_ragdoll_drag_water 0;vr_ragdoll_drag_slime 0;vr_ragdoll_drag_lava 0"
FILTER="in liquid|gibbed|fall:|knockdown test: .* down|^ragdoll [0-9]+ monster|rror|burnt|in lava \(|liquid: .*health|bodycheck: .*(ragdoll=1|knocked=1|water=[1-3])"
# drop <map> <x y z of the player> <monster> <dead 1/0> <after: knock down or nothing>
drop() {
    local S="map $1;wait30;god;notarget;developer 1;vr_debug_shots 1;vr_ragdoll 1;vr_debug_ragdoll 2;vr_knockdown_time_min 20;vr_knockdown_time_max 20;$X;noclip;setpos $2 0 0 0;wait5;vr_test_spawn $3;vr_test_spawn_dist ${D:-150};vr_test_spawn_dead $4;impulse 241;wait2;vr_test_spawn_dead 0;$5;wait500;vr_ragdoll_list;vr_knockdown_test 6"
    bash $KIT/run.sh $AGENT -Script "$S;toggleconsole;quit" -Filter "$FILTER" -Timeout 300 2>&1 | grep -v "^$" | tr -d '\r' |
        awk '/in liquid/ { n++; if (n % 40 != 1) next } { print substr($0, 1, 150) }'
}
for c in $CASES; do
    echo "== $c"
    case $c in
    water)   drop vrtesthall "180 -256 200" 0 1 ;;
    knight)  drop vrtesthall "180 -256 200" 5 1 ;;
    kd)      D=100 drop vrtesthall "230 -256 200" 0 0 "vr_knockdown_test 0" ;;
    fall)    D=100 drop vrtesthall "230 -256 200" 0 0 ;;
    slime)   drop e3m1 "1098 16 0" 0 1 ;;
    kdslime) D=100 drop e3m1 "960 16 0" 0 0 "vr_knockdown_test 0" ;;
    lava)    drop e1m7 "710 160 150" 0 1 ;;
    kdlava)  D=100 drop e1m7 "300 160 150" 0 0 "vr_knockdown_test 0" ;;
    esac
done

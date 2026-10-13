# hotblade_test.sh -- incandescent blades' headless tests (QC vr_hotblade.qc; docs/vr-port/ROUND22.md, "Incandescent
# blades"; TESTING.md). Each case is a run of the kit's mock headset; each prints PASS or FAIL and its lines.
# Usage: bash Misc/quakevr/hotblade_test.sh [agent, default hotblade] [case regex]
name=${1:-hotblade}
F=${2:-.*}
WT="C:/OHWorkspace/qvr-agents/$name"
P='vr_fixed_frames 1; developer 1; vr_debug_shots 1; vr_weapon_grip_mode 1; vr_walltorch 1; vr_mock_hand main 0.15 1.2 -0.45 -20 0 0; vr_mock_hand off -0.15 1.2 -0.45 -20 0 0'
FIL='Dummy:|hotblade|throw hit|burning: [a-oq-z].*lit|damage: monster|rror'
mkdir -p "$WT/scratch/hotblade"
run() { # label, script, regexes that must all appear (|-separated alternatives inside each), [regex that must not]
  [[ "$1" =~ $F ]] || return 0
  out="$WT/scratch/hotblade/$1.txt"
  bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Script "$P; $2; wait5; toggleconsole; quit" -Filter "$FIL" -Timeout 240 > "$out" 2>&1
  ok=1
  IFS=';' read -ra need <<< "$3"
  for n in "${need[@]}"; do grep -Eq "$n" "$out" || { ok=0; echo "  missing: $n"; }; done
  if [ -n "$4" ] && grep -Eq "$4" "$out"; then ok=0; echo "  unwanted: $4"; fi
  [ $ok = 1 ] && echo "PASS $1" || echo "FAIL $1 ($out)"
}
# A map flame (e1m2's): the build-up, hot, out of it full then fading then cold; a flame again sets it back to full.
run flame "vr_hotblade_time 6; vr_hotblade_fade 3; map e1m2; wait60; god; notarget; noclip; impulse 152; wait30; vr_hotblade_test 7; wait72; vr_hotblade_test 3; wait90; vr_mock_hand main 0.15 1.2 0.35 -20 0 0; wait72; vr_hotblade_test 3; wait180; vr_hotblade_test 3; wait36; vr_mock_hand main 0.15 1.2 -0.45 -20 0 0; wait20; vr_hotblade_test 3; wait10; vr_mock_hand main 0.15 1.2 0.35 -20 0 0; wait500; vr_hotblade_test 3" \
  'heat 0\.[3-5][0-9] glow 0\.[0-9]+ hot 0;incandescent \(a flame\);glow 1\.00 hot 1 left 5\.0;glow 0\.8[0-9] hot 1 left 2\.5;full again .*reheating, a buzz;hot 0 left 0\.0; off none; hud 0 0'
# A torch on its wall (an entity), a static one (vr_walltorch 0: a fire spot).
run walltorch "map e1m2; wait60; god; notarget; noclip; impulse 152; setpos 1714 -170 312 0 243 0; wait30; vr_hotblade_test 7; wait200; vr_hotblade_test 3" 'incandescent \(a torch on its wall\)'
run walltorch_static "vr_walltorch 0; map e1m2; wait60; god; notarget; noclip; impulse 152; setpos 1714 -170 312 0 243 0; wait30; vr_hotblade_test 7; wait200; vr_hotblade_test 3" 'incandescent \(a burning torch\)'
# The off hand's lit torch (taken off its wall), its flame brought to the main hand's blade (the move measured once).
run torch "map e1m2; wait60; god; notarget; impulse 152; setpos 1714 -170 312 0 243 0; wait30; +grableft; vr_mock_button off grip 1; vr_hotblade_test 9; wait30; vr_hotblade_test 10; wait30; vr_mock_hand_to off by -7.378 4.411 -17.031; wait10; vr_hotblade_test 10; wait200; vr_hotblade_test 3" 'incandescent \(a burning torch\)'
# Lava (e1m7's).
run lava "map e1m7; wait60; god; notarget; noclip; impulse 152; setpos -50 48 40 0 0 0; wait30; vr_hotblade_test 8; wait200; vr_hotblade_test 3" 'incandescent \(lava\)'
# Slime quenches it held (e1m1's), and lying.
run quench "map e1m1; wait60; god; notarget; noclip; impulse 152; setpos 200 2820 0 0 0 0; wait30; vr_hotblade_test 1; wait10; vr_hotblade_test 11; wait30; vr_hotblade_test 3" 'quenched \(glow 1\.00\);hot 0 left 0\.0; off none; hud 0 0'
run quench_lying "vr_weapon_grip_mode 0; map e1m1; wait60; god; notarget; noclip; setpos 200 2820 -20 0 0 0; wait10; +grabright; vr_mock_button main grip 1; impulse 152; wait20; vr_hotblade_test 1; wait10; -grabright; vr_mock_button main grip 0; wait200" 'a lying weapon 2 quenched'
# Holstered (the right chest holster), it cools on and glows there (the second stat).
run holster "vr_weapon_grip_mode 0; vr_hotblade_time 5; vr_hotblade_fade 2; map e1m2; wait60; god; notarget; +grabright; vr_mock_button main grip 1; impulse 152; wait20; vr_hotblade_test 1; wait10; vr_mock_hand main 0.12 1.3 -0.15 0 0 0; wait30; -grabright; vr_mock_button main grip 0; wait30; vr_hotblade_test 3; vr_hotblade_report; wait180; vr_hotblade_test 3; wait200; vr_hotblade_test 3" \
  'client: holster v_axe.mdl glow 1.00 part 3.4/8.5;main none; off none; hud 0 1\.57286e\+07;hud 0 1\.25829e\+07;main none; off none; hud 0 0'
# Mjolnir never heats; the chainsaw does; off, nothing does.
run mjolnir "map e1m2; wait60; god; notarget; noclip; impulse 153; wait30; vr_hotblade_test 7; wait200; vr_hotblade_test 3" 'blade put in;wid 3 .* hot 0' 'heating|incandescent'
run chainsaw "map e1m2; wait60; god; notarget; noclip; impulse 164; wait30; vr_hotblade_test 7; wait200; vr_hotblade_test 3" 'weapon 14 incandescent'
run off "vr_hotblade 0; map e1m2; wait60; god; notarget; noclip; impulse 152; wait30; vr_hotblade_test 7; wait200; vr_hotblade_test 3; vr_hotblade 1" 'hot 0 left 0\.0' 'incandescent'
# Blows: a live grunt (lit, +10), its corpse (lit), corpses off (not lit), enemies off (not lit).
G="map e1m1; wait60; god; notarget; impulse 152; wait5; vr_test_spawn 0; vr_test_spawn_dist 64; impulse 241; wait30"
run strike_alive "$G; vr_hotblade_test 1; wait5; vr_hotblade_test 6; wait30" 'strike monster_army \(health 30, corpse 0\) for 30;struck monster_army \(alive\);monster_army lit \(a blow\)'
run strike_corpse "$G; vr_hotblade_test 6; wait10; vr_hotblade_test 6; wait10; vr_hotblade_test 6; wait10; vr_hotblade_test 6; wait200; vr_hotblade_test 1; wait5; vr_hotblade_test 6; wait30" 'corpse 1\) for 30|corpse 2\) for 30;struck monster_army \(a corpse\)'
run corpse_off "vr_hotblade_ignite_corpses 0; $G; vr_hotblade_test 6; wait10; vr_hotblade_test 6; wait10; vr_hotblade_test 6; wait10; vr_hotblade_test 6; wait200; vr_hotblade_test 1; wait5; vr_hotblade_test 6; wait30; vr_hotblade_ignite_corpses 1" 'for 30' 'struck'
run alive_off "vr_hotblade_ignite 0; $G; vr_hotblade_test 1; wait5; vr_hotblade_test 6; wait30; vr_hotblade_ignite 1" 'for 30' 'struck'
# Saved hot, loaded hot.
run saveload "map e1m1; wait60; god; impulse 152; wait10; vr_hotblade_test 1; wait72; vr_hotblade_test 3; save hbtest; wait10; map e1m2; wait30; load hbtest; wait30; vr_hotblade_test 3" 'hot 1 left 29\.0;hot 1 left 28\.[0-9]'
# His tweaks (ROUND22.md, "Incandescent blades: his tweaks"). Only the heated part glows: each weapon's part of its
# grip-to-tip line and the vertices in it (vr_hotblade_report), its ends the hand's own; the light follows the glow.
B="map e1m1; wait60; god; notarget"
for imp in 152 163 167 164; do B="$B; impulse $imp; wait60; vr_hotblade_test 1; wait40; vr_hotblade_report; vr_hotblade_test 2; wait5"; done
run blade_parts "$B" 'main v_axe\.mdl glow 1\.00 part 3\.4/8\.5 verts 10/223 light 0\.9[0-9] reach 72;main v_ksword\.mdl glow 1\.00 part 22\.4/28\.0 verts 8/230;main v_crowbar\.mdl glow 1\.00 part 10\.5/11\.0 verts 280/464;main v_chainsaw\.mdl glow 1\.00 part 13\.9/25\.3 verts 18/285;grip off the hand.s 0\.0[0-9], tip off its muzzle 0\.0[0-9]'
run superaxe "map e1m1; wait60; god; notarget; impulse 168; wait60; vr_hotblade_test 1; wait40; vr_hotblade_report" 'main v_hammer\.mdl glow 1\.00 part 2\.3/5\.9 verts 295/795'
# Embers off the heated part held still (4 a puff, 12 puffs a second: 48), none with Embers per Puff 0; no light at 0.
E="map e1m1; wait60; god; notarget; impulse 152; wait20; vr_hotblade_test 1; wait150; vr_hotblade_report"
run embers "$E" 'embers (4[0-9]|5[0-6]), trail 0 in'
run embers_off "vr_hotblade_embers 0; vr_hotblade_light 0; $E; vr_hotblade_embers 4; vr_hotblade_light 1" 'embers 0, trail 0 in;light 0\.00'
# A swing (the mock hand swept across, 10 cm and 5 degrees a frame): fire and sparks behind it; none with Swing Trail 0.
W="map e1m1; wait60; god; notarget; impulse 152; vr_mock_hand main 0.6 1.6 -0.55 0 30 0; wait20; vr_hotblade_test 1; wait30"
i=0
for x in 0.6 0.5 0.4 0.3 0.2 0.1 0 -0.1 -0.2 -0.3 -0.4 -0.5; do W="$W; vr_mock_hand main $x 1.6 -0.55 0 $(( 30 - i * 5 )) 0; wait1"; i=$(( i + 1 )); done
run trail "$W; wait10; vr_hotblade_report" 'trail ([5-9][0-9]|[1-9][0-9][0-9]) in|[0-9]+, ([5-9][0-9]|[1-9][0-9][0-9]) since'
run trail_off "vr_hotblade_trail 0; $W; wait10; vr_hotblade_report; vr_hotblade_trail 1" 'trail 0 in the last second; [0-9]+, 0 since'
# Dropped (let go of where the hand is), it glows on the ground as in a hand (the client: lying), its heat ticking down
# there (6 s, the last 3 fading) until it is cold and the client draws none.
run drop_cools "vr_hotblade_time 6; vr_hotblade_fade 3; map e1m2; wait60; god; notarget; impulse 152; wait20; vr_hotblade_test 1; wait10; vr_hotblade_test 14; wait100; vr_hotblade_test 12; vr_hotblade_report; wait144; vr_hotblade_test 12; wait200; vr_hotblade_test 12; vr_hotblade_report" \
  'test: weapon 2 dropped;lying: wid 2 #[0-9]+ heat 0\.00 glow 1\.00 hot 1 left 4\.[0-9]; sent 1\.00;client: lying v_axe\.mdl glow 1\.00 part 3\.4/8\.5 verts 10/223 light 0\.9;lying: wid 2 #[0-9]+ heat 0\.00 glow 0\.8[0-9] hot 1 left 2\.[0-9]; sent 0\.8;glow 0\.00 hot 0 left 0\.0; sent 0\.00;client: 0 glowing'
# Thrown at a grunt (e1m2: no weapons lying near): cold, then red-hot (base 35 + 10: x1.29, and it is set on fire).
T="map e1m2; wait60; god; notarget; impulse 152; wait5; vr_test_spawn 0; vr_test_spawn_dist 64; impulse 241; wait30"
run throw_cold "$T; vr_hotblade_test 13; wait60" 'throw hit: Axe into monster_army at 276 u/s .*: (19\.[5-9]|20\.[0-1]) damage' 'set on fire'
run throw_hot "$T; vr_hotblade_test 1; wait5; vr_hotblade_test 13; wait60" 'throw hit: Axe into monster_army at 276 u/s .*: 25\.[2-6] damage;a thrown weapon 2 struck monster_army \(alive\): set on fire;monster_army lit \(a blow\)'
# A real swing (a synthetic overhead chop with the axe at the firing range's dummy): cold and hot, the hot one 1.5x
# (base 20 -> 30) and the dummy lit.
if [[ "swing" =~ $F ]]; then
  python "$WT/Misc/quakevr/motion_synth.py" chop_overhead --weapon axe --out "$WT/scratch/hotblade/motions" > /dev/null
  TAKE=$(ls -t "$WT"/scratch/hotblade/motions/*.csv | head -1)
  R=''
  for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do R="$R; wait4; vr_hotblade_test 1"; done
  run swing_cold "map vrfiringrange; wait60; vr_motion_play $TAKE; wait300" 'Dummy: 29\.6 damage'
  run swing_hot "map vrfiringrange; wait60; vr_motion_play $TAKE$R; wait200" 'Dummy: 44\.4 damage;monster_army lit \(a blow\)'
fi
# Screenshots, checked by eye (scratch/hotblade/shots_*.png; not PASS/FAIL): each blade held red-hot seen from the side
# (the axe, the sword, the crowbar, the Super Axe, the chainsaw: only the heated part glows, its light on the hand), the
# axe dropped on e1m2's floor (glowing there), a swing's trail of fire and sparks (just after it, and 10 frames on).
if [[ "shots" =~ $F ]]; then
  S="map e1m1; wait60; god; notarget; vr_mock_hand main 0.1 1.4 -0.4 0 0 0; vr_mock_camera 0.8 1.5 -0.7 10 90"
  for imp in 152 163 167 168; do S="$S; impulse $imp; wait60; vr_hotblade_test 1; wait40; screenshot; vr_hotblade_test 2; wait5"; done
  S="$S; impulse 164; wait60; vr_hotblade_test 1; vr_mock_hand main 0.1 1.4 -0.4 60 0 0; wait40; screenshot"
  bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Script "$P; $S; wait5; toggleconsole; quit" -Filter "composed" -Out hotblade/shots_blades.png | grep composed
  S="map e1m2; wait60; god; notarget; impulse 152; vr_mock_hand main 0.0 1.2 -0.9 0 90 0; wait40; vr_hotblade_test 1; wait10; vr_hotblade_test 14; wait5; vr_mock_hand main 0.3 1.0 0.1 -80 0 0; wait100; vr_mock_camera 0.3 0.7 -0.4 50 60; wait3; screenshot"
  bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Script "$P; $S; wait5; toggleconsole; quit" -Filter "composed" -Out hotblade/shots_dropped.png | grep composed
  bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Script "$P; vr_hotblade_embers 0; $W; screenshot; wait10; screenshot; vr_hotblade_embers 4; wait5; toggleconsole; quit" -Filter "composed" -Out hotblade/shots_trail.png | grep composed
fi

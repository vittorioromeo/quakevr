# hotblade_test.sh -- incandescent blades' headless tests (QC vr_hotblade.qc; docs/vr-port/ROUND22.md, "Incandescent
# blades"; TESTING.md). Each case is a run of the kit's mock headset; each prints PASS or FAIL and its lines.
# Usage: bash Misc/quakevr/hotblade_test.sh [agent, default hotblade] [case regex]
name=${1:-hotblade}
F=${2:-.*}
WT="C:/OHWorkspace/qvr-agents/$name"
P='vr_fixed_frames 1; developer 1; vr_debug_shots 1; vr_weapon_grip_mode 1; vr_walltorch 1; vr_mock_hand main 0.15 1.2 -0.45 -20 0 0; vr_mock_hand off -0.15 1.2 -0.45 -20 0 0'
FIL='Dummy:|hotblade|burning: [a-oq-z].*lit|damage: monster|rror'
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
  'heat 0\.[3-5][0-9] glow 0\.[0-9]+ hot 0;incandescent \(a flame\);glow 1\.00 hot 1 left 5\.0;glow 0\.8[0-9] hot 1 left 2\.5;full again;hot 0 left 0\.0; off none; hud 0 0'
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
run holster "vr_weapon_grip_mode 0; vr_hotblade_time 5; vr_hotblade_fade 2; map e1m2; wait60; god; notarget; +grabright; vr_mock_button main grip 1; impulse 152; wait20; vr_hotblade_test 1; wait10; vr_mock_hand main 0.12 1.3 -0.15 0 0 0; wait30; -grabright; vr_mock_button main grip 0; wait30; vr_hotblade_test 3; wait180; vr_hotblade_test 3; wait200; vr_hotblade_test 3" \
  'main none; off none; hud 0 1\.57286e\+07;hud 0 1\.25829e\+07;main none; off none; hud 0 0'
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

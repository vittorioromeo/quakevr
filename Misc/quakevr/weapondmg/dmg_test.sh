#!/usr/bin/env bash
# Weapon Damage (ROUND21.md, "Weapon Damage menu"): an ogre put ahead in the firing range, shot once with the shotgun
# (its 6 pellets' damage summed) and, in a second run, once with a rocket (its direct hit: the setting and up to a fifth
# more), at the settings given; vr_debug_shots prints each damage dealt ("damage: monster_ogre <n> by <inflictor>").
# Positional damage off: every pellet the same.
# Usage: bash dmg_test.sh <worktree name> ["<cvar> <value>;..."]   e.g. "vr_dmg_shotgun 8;vr_dmg_rocket 50"
kit=C:/OHWorkspace/qvr-kit
name=${1:?worktree name}
set_cvars=${2:-}
pre="map vrfiringrange;wait60;developer 1;vr_debug_shots 1;god;notarget;vr_positional_damage 0;vr_weapon_grip_mode 1"
pre="$pre;$set_cvars;impulse 9;wait5;vr_mock_hand main 0.25 1.20 -0.40 70 0 0;wait5"
pre="$pre;vr_test_spawn 1;vr_test_spawn_dist 128;impulse 241;wait20"
fire="wait20;+attack;wait3;-attack;wait90;toggleconsole;quit"
sg=$(bash $kit/run.sh "$name" -Script "$pre;impulse 154;$fire" -Filter "^damage: monster_ogre" 2>&1)  # the shotgun
rk=$(bash $kit/run.sh "$name" -Script "$pre;impulse 160;$fire" -Filter "^damage: monster_ogre" 2>&1)  # the rocket launcher
python - "$sg" "$rk" <<'EOF'
import re, sys
def total(text):
    return sum(float(m.group(1)) for l in text.splitlines() for m in [re.match(r'damage: monster_ogre ([0-9.]+)', l)] if m)
print(f'shotgun (6 pellets on the ogre): {total(sys.argv[1]):g}   rocket (direct hit on the ogre): {total(sys.argv[2]):.1f}')
EOF

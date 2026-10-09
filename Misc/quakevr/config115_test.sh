#!/bin/bash
# config115_test.sh <agent> -- config version 115 (ROUND21.md, "Crouched pose": the author's crouched offsets of
# 2026-10-09 are the defaults), headless: a config of version 114 holding the old defaults (0.05, 0 ...) takes the new
# ones; a setting the player changed (vr_hip_holster_crouch_x 4) keeps its value. PASS: every NEW line at its new default
# (vr_cfg_version 115), the KEPT block 4 and the rest new.
AGENT=${1:-tgcrash}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
OLD="vr_body_crouch_torso_back 0.05;vr_body_crouch_shoulders_out 0;vr_hip_holster_crouch_x 0;vr_hip_holster_crouch_y 0"
OLD="$OLD;vr_hip_holster_crouch_roll 0;vr_ammo_pouch_crouch_x 0;vr_ammo_pouch_crouch_z 0;vr_ammo_pouch_crouch_pitch 0"
P="${OLD//;/
}"; P=$(echo "$P" | sed 's/ .*//' | tr '\n' ';')
S="wait5;echo === NEW;vr_cfg_version 114;$OLD;vr_migrate_config;${P}vr_cfg_version"
S="$S;echo === KEPT;vr_cfg_version 114;$OLD;vr_hip_holster_crouch_x 4;vr_migrate_config"
S="$S;vr_hip_holster_crouch_x;vr_hip_holster_crouch_y;echo === END;toggleconsole;quit"
out=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|\" is \"" "$@")
echo "$out"
echo "$out" | python -c "
import sys, re
t = sys.stdin.read()
new = t.split('=== NEW')[1].split('=== KEPT')[0]
kept = t.split('=== KEPT')[1]
want = {'vr_body_crouch_torso_back': '0.2', 'vr_body_crouch_shoulders_out': '0.02', 'vr_hip_holster_crouch_x': '6',
        'vr_hip_holster_crouch_y': '3', 'vr_hip_holster_crouch_roll': '40', 'vr_ammo_pouch_crouch_x': '2',
        'vr_ammo_pouch_crouch_z': '7', 'vr_ammo_pouch_crouch_pitch': '29', 'vr_cfg_version': '115'}
got = dict(re.findall(r'\"(\w+)\" is \"([^\"]*)\"', new))
ok = all(got.get(k) == v for k, v in want.items())
k2 = dict(re.findall(r'\"(\w+)\" is \"([^\"]*)\"', kept))
ok = ok and k2.get('vr_hip_holster_crouch_x') == '4' and k2.get('vr_hip_holster_crouch_y') == '3'
print('PASS' if ok else 'FAIL')"

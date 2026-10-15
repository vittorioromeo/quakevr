#!/bin/bash
# config129_test.sh <agent> -- config version 129 (ROUND22.md, "His parry and bash tweaks are the defaults"): a 128
# config at the old shipped bash speed 2 takes 3; parry stamina 0 (it shipped at 1: vr_defaults.cfg) was hand-set and
# stays; a hand-set bash speed (2.5) stays. PASS: NEW 3 0, KEPT 2.5, and vr_cfg_version 129 or later.
AGENT=${1:-shovefix}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="wait5;echo === NEW;vr_cfg_version 128;vr_bash_speed 2;vr_parry_stamina 0;vr_migrate_config;vr_bash_speed;vr_parry_stamina;vr_cfg_version"
S="$S;echo === KEPT;vr_cfg_version 128;vr_bash_speed 2.5;vr_migrate_config;vr_bash_speed;echo === END;toggleconsole;quit"
out=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|\" is \"" "$@")
echo "$out"
echo "$out" | python -c "
import sys, re
t = sys.stdin.read()
def val(block, var):
    m = re.search(r'\"' + var + r'\" is \"([^\"]*)\"', t.split('=== ' + block)[1].split('===')[0])
    return m.group(1) if m else None
ok = val('NEW', 'vr_bash_speed') == '3' and val('NEW', 'vr_parry_stamina') == '0' and \
     int(float(val('NEW', 'vr_cfg_version') or 0)) >= 129 and val('KEPT', 'vr_bash_speed') == '2.5'
print('PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
"

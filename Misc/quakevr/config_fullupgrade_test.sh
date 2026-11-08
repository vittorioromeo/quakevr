#!/bin/bash
# config_fullupgrade_test.sh <agent>: a full config (1.0.x or a 1.1 beta: vr_cfg_version 217 and before) takes the
# defaults changed after the sparse switch (vr_cvars.cpp fullConfigChanges; release pass 2026-11-08): armed replacements
# off at beta 4's default 0 end on, the relight's ambient 0 ends at today's, a value the player changed (a dead zone of
# 7) stays; a sparse config's own "vr_agrunt_repl 0" (219) is his choice and stays. Old names that are gone print no
# "Unknown command" (vr_carry_two_hands_window, vr_walltorch_pull, vr_revolver_eject_tilt: retired). Writes
# quakevr/cfgtest_full217.cfg (git-ignored). Ends on "config_fullupgrade_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
printf 'vr_cfg_version "207"\nvr_agrunt_repl "0"\nvr_agrunt_repl_common "0"\nvr_relight_minlight "0"\nvr_deadzone_outer "7"\nvr_carry_two_hands_window "0.1"\nvr_walltorch_pull "1"\nvr_revolver_eject_tilt "30"\n' > "$TREE/quakevr/cfgtest_full217.cfg"
SHOW="vr_agrunt_repl;vr_agrunt_repl_common;vr_relight_minlight;vr_deadzone_outer"
S="wait5;echo === FULL;exec cfgtest_full217.cfg;vr_migrate_config;$SHOW"
S="$S;echo === SPARSE;vr_cfg_version 219;vr_agrunt_repl 0;vr_migrate_config;vr_agrunt_repl"
S="$S;vr_agrunt_repl 1;vr_agrunt_repl_common 0.5;vr_relight_minlight 32;vr_deadzone_outer 20;echo === END"
out=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|\" is \"|Unknown command" -QuitOn "=== END" -Timeout 60)
echo "$out" | python -c "
import re, sys
t = sys.stdin.read()
def val(block, var):
    m = re.search(r'\"' + var + r'\" is \"([^\"]*)\"', t.split('=== ' + block)[1].split('===')[0])
    return float(m.group(1)) if m else None
checks = [('full: replacements on', val('FULL', 'vr_agrunt_repl') == 1),
          ('full: common tier', val('FULL', 'vr_agrunt_repl_common') == 0.5),
          ('full: relight ambient', val('FULL', 'vr_relight_minlight') == 32),
          ('full: his dead zone kept', val('FULL', 'vr_deadzone_outer') == 7),
          ('sparse: his off kept', val('SPARSE', 'vr_agrunt_repl') == 0),
          ('no unknown lines', 'Unknown command' not in t)]
for name, ok in checks:
    if not ok:
        print('config_fullupgrade_test: FAIL ' + name)
failed = sum(1 for c in checks if not c[1])
print('config_fullupgrade_test: %d passed, %d failed' % (len(checks) - failed, failed))
sys.exit(1 if failed else 0)
"

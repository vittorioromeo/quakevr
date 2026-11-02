#!/bin/bash
# config220_test.sh <agent>: configVersion 220, vr_body_step_rate's new meaning (ROUND23.md, "The player's walk"). An old
# full config (a beta 4's: version 217, 2.2, a Running Cadence of 1.5) ends at the default 4.9, cadence 1, and the written
# config has no vr_body_step_rate line. A sparse 219 config's own 3 goes too, its cadence 0.5 stays. A 3 set at this
# version stays. Writes quakevr/cfgtest_old220.cfg and cfgtest_out220.cfg (git-ignored). Ends on the verdict line
# "config220_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
printf 'vr_cfg_version "217"\nvr_body_step_rate "2.2"\nvr_body_step_run "1.5"\n' > "$TREE/quakevr/cfgtest_old220.cfg"
SHOW="vr_body_step_rate;vr_body_step_run;vr_cfg_version"
S="wait5;echo === OLD;exec cfgtest_old220.cfg;vr_migrate_config;$SHOW;writeconfig cfgtest_out220"
S="$S;echo === OWN;vr_cfg_version 219;vr_body_step_rate 3;vr_body_step_run 0.5;vr_migrate_config;$SHOW"
S="$S;echo === KEPT;vr_body_step_rate 3;vr_migrate_config;$SHOW"
S="$S;vr_body_step_rate 4.9;vr_body_step_run 1;echo === END"
out=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|\" is \"" -QuitOn "=== END" -Timeout 60)
echo "$out" | python -c "
import re, sys
t = sys.stdin.read()
def val(block, var):
    m = re.search(r'\"' + var + r'\" is \"([^\"]*)\"', t.split('=== ' + block)[1].split('===')[0])
    return float(m.group(1)) if m else None
written = open(sys.argv[1]).read()
checks = [('old rate', val('OLD', 'vr_body_step_rate') == 4.9), ('old cadence', val('OLD', 'vr_body_step_run') == 1),
          ('old version', (val('OLD', 'vr_cfg_version') or 0) >= 220), ('no line written', 'vr_body_step_rate' not in written),
          ('own rate', val('OWN', 'vr_body_step_rate') == 4.9), ('own cadence', val('OWN', 'vr_body_step_run') == 0.5),
          ('kept rate', val('KEPT', 'vr_body_step_rate') == 3)]
for name, ok in checks:
    if not ok:
        print('config220_test: FAIL ' + name)
failed = sum(1 for c in checks if not c[1])
print('config220_test: %d passed, %d failed' % (len(checks) - failed, failed))
sys.exit(1 if failed else 0)
" "$TREE/quakevr/cfgtest_out220.cfg"

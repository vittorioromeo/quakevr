#!/bin/bash
# config123_test.sh <agent> -- config version 123 (ROUND22.md, "The Loading... crash, root cause"): the "Loading..."
# notice back on. A 1.0.0 config (122) at its 0 takes 1; an older config at 0 (turned off by hand) keeps 0; a 1.0.0
# config at 1 keeps 1. PASS: NEW 1, KEPT 0, ON 1, and vr_cfg_version 123.
AGENT=${1:-fix101}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="wait5;echo === NEW;vr_cfg_version 122;vr_loading_notice 0;vr_migrate_config;vr_loading_notice;vr_cfg_version"
S="$S;echo === KEPT;vr_cfg_version 121;vr_loading_notice 0;vr_migrate_config;vr_loading_notice"
S="$S;echo === ON;vr_cfg_version 122;vr_loading_notice 1;vr_migrate_config;vr_loading_notice;echo === END;toggleconsole;quit"
out=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|\" is \"" "$@")
echo "$out"
echo "$out" | python -c "
import sys, re
t = sys.stdin.read()
def val(block, var):
    m = re.search(r'\"' + var + r'\" is \"([^\"]*)\"', t.split('=== ' + block)[1].split('===')[0])
    return m.group(1) if m else None
ok = val('NEW', 'vr_loading_notice') == '1' and val('NEW', 'vr_cfg_version') == '123' and \
     val('KEPT', 'vr_loading_notice') == '0' and val('ON', 'vr_loading_notice') == '1'
print('PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
"

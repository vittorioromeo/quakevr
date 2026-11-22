#!/usr/bin/env bash
# humstudio2_test.sh: the Texture Studio's pick (docs/vr-port/ARMED_MONSTERS.md, "Recolours by weapon"; ROUND25.md
# "studio2"). In vrtesthall, held still and wearing their packs: first the shipped humlook.txt and humlook_masks.txt (the
# author's saved edits) read and written again under other names (scratch: studio2_rt_*.txt in quakevr/), which must match
# them line for line; then vr_humstudio_picktest: each type lined up in turn and, once drawn, rays at each one's hands,
# forearm, the back of its head and its back beside its pack must paint the texel under that place (a pack in the way
# passed through), and a stroke on a hand must take one undo step that puts the paint back. The engine's run ends on the
# test's verdict line; this prints "humstudio2_test: PASS" or "FAIL".
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
S="sv_random_seed 7;map vrtesthall;wait60;god;notarget;vr_agrunt_freeze 1;vr_humstudio_nopack 0;vr_humstudio_packstand 1"
S="$S;vr_humlook_reload;vr_humlook_file studio2_rt_table.txt;vr_humlook_masks_file studio2_rt_masks.txt;vr_humstudio_save"
S="$S;vr_humlook_file humlook.txt;vr_humlook_masks_file humlook_masks.txt;vr_humstudio_picktest;toggleconsole"
OUT=$(bash "$K/run.sh" "$NAME" -Script "$S" -Filter "humstudio|humlineup: row|Host_Error|SV_Error" -QuitOn "humstudio_picktest: [0-9]+ passed" -Timeout 240 2>&1)
echo "$OUT" | grep -v "picktest: ok "
V=PASS
echo "$OUT" | grep -q "humstudio_picktest: [1-9][0-9]* passed, 0 failed" || V=FAIL
for F in humlook humlook_masks; do
    case $F in humlook) R=studio2_rt_table.txt ;; *) R=studio2_rt_masks.txt ;; esac
    if ! diff -q --strip-trailing-cr "$W/quakevr/$F.txt" "$W/quakevr/$R" > /dev/null 2>&1; then
        echo "humstudio2_test: $F.txt not written back the same (quakevr/$R)"
        V=FAIL
    fi
done
echo "humstudio2_test: $V"

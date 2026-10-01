#!/bin/bash
# The "Standing on props 2" tests (ROUND21.md; "Props regressions" for ps2_two's new hand places).
#   bash Misc/quakevr/propstand2/run.sh <agent> <two|trap|walk> [cvars after the author's]
# Needs quakevr/ps2_author.cfg (the author's ironwail.cfg), not committed. Prints the console lines the tests read:
# two: "carry: both hands" once, every vr_physics_inlevel 0.0 (held); trap: AFTER, the player 150 units on (y -849.6);
# walk: the 25 kg box ~54 units on, the 40 kg ~63 upright (45 with vr_box3d_hand_push 0), the 150 kg ~30 (0.1 without hands).
K=C:/OHWorkspace/qvr-kit
D=$(cd "$(dirname "$0")" && pwd); Q="$D/../../../quakevr"
cp "$D/ps2_w.cfg" "$D/ps2_$2.cfg" "$Q/"
echo "$3" > "$Q/ps2_pre.cfg"
printf 'exec ps2_w.cfg\nexec ps2_author.cfg\nexec ps2_pre.cfg\nmap vrfiringrange\nw300\nw300\nexec ps2_%s.cfg\nw30\ntoggleconsole; quit\n' "$2" > "$Q/ps2_go.cfg"
bash $K/run.sh "$1" -Script "exec ps2_go.cfg" \
    -Filter "WALK|TRAPPED|AFTER|WALL|PUSH|carry: |misc_explobox|vr_physics_player:.*at|rror|ENGINE" 2>&1 |
    grep -v "moved by QC\|pushed by QC\|asleep at\|a prop body\|a mover body"

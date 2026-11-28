#!/bin/bash
# visorboot_test.sh <agent> -- the flatscreen visor HUD's boot sequence (vr_flat_visor_boot_seq; Quake/vr/vr_visor.cpp
# startBoot, traceBoot; docs/vr-port/ROUND26.md, "visorboot"), headless, in fast mode (the visor's clock is the frames':
# 1/72 s each, so the phases' frame counts are exact). Each boot is traced (vr_flat_visor_fx trace 1: a "visorboot: boot"
# verdict line as it ends) and vr_flat_visor_report read through the first one:
#   start      map e1m1 (the first start): dark (no panel drawn), fading in (drawn, glitching), sweeping, booted;
#              its trace: dark 0.3 s with nothing shown, the fade in 0.5 s rising every frame and glitching, the sweep,
#              booted at about 1.6 s, the power-up sound started once
#   change     changelevel e1m2: the same, signed on again
#   death      vr_hurt 500, then restart (single player's respawn: QC respawn's own "restart"; the death menu's hold
#              left out): the same
#   coop       coop 1, map e1m1 (a boot, signed on), vr_hurt 500, fire (vr_death_menu 0: a lone coop player's death
#              menu holds the respawn too): respawned in place (no new map): the same, its cause the respawn
#   off        vr_flat_visor_boot_seq 0, vr_flat_visor_fx boot: the sweep alone (no dark, no fade, no sound)
# The screenshots (scratch/visorboot_strip.png): the first start dark, fading in (two), sweeping, booted.
# Prints PASS/FAIL lines and "visorboot_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
mkdir -p "$S"
LOG=$S/visorboot_test.log
R="vr_flat_visor_report"
E="echo visorboot_test:"
START="vr_flat_visor_fx trace 1; vr_flat_visor_autohide_log 0; map e1m1; wait8; $E start dark; $R; screenshot; wait24; $E start fading; $R; screenshot; wait10; screenshot; wait20; $E start sweeping; $R; screenshot; wait60; $E start booted; $R; screenshot"
CHANGE="notarget; changelevel e1m2; wait140; $E change done"
DEATH="notarget; vr_hurt 500; wait36; restart; wait150; $E death done"
COOP="vr_death_menu 0; coop 1; map e1m1; wait140; notarget; vr_hurt 500; wait110; +attack; wait3; -attack; wait140; $E coop done; coop 0; reset vr_death_menu"
OFF="vr_flat_visor_boot_seq 0; vr_flat_visor_fx boot; wait90; $E off done; reset vr_flat_visor_boot_seq"
SCRIPT="$START; $CHANGE; $DEATH; $COOP; $OFF; $E end"
timeout 600 bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Clean -Out "$S/visorboot_strip.png" -QuitOn "visorboot_test: end" \
    -Timeout 400 -Filter "^visor" -Script "$SCRIPT; toggleconsole; quit" > "$LOG" 2>&1
python "$WT/Misc/quakevr/visorboot_check.py" "$LOG"

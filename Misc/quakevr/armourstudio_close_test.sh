#!/bin/bash
# armourstudio_close_test.sh <agent> -- the Armour Studio closed lets the armed humanoids go (his note 2026-11-10: "After
# closing the armour studio it seems that armed humanoids do not recover their AI/damageability"). The studio holds every
# armed humanoid still (vr_agrunt_freeze) while it runs; its window's X / Escape and leaving the menu used to leave it
# running. Flat screen (the window is flat-only), the scene flat_parry (a sword grunt facing you, god mode), you unseen
# while the studio runs (notarget), then seen again. Four runs, the first three each closing it one way:
#   cmd     vr_armourstudio_close (as before);
#   window  the studio's window opened and closed (vr_armourstudio_window_close: its X's own path);
#   menu    started with the VR menu open (menu_vr), the menu closed (togglemenu twice: the main menu, then the game).
# Each: "armourstudio: closed", vr_agrunt_freeze back to 0, then the grunt strikes at you (a dirparry/parry line: its AI
# back) and a test shot hurts it (vr_test_hurt 5: its health down by 5: damageable). Each run ends on its own "AS done";
# with vr_armourstudio_autoclose 0 the window's close leaves the studio running (the fourth run, the setting kept).
# Prints "armourstudio_close: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/asclose
mkdir -p "$S"
for how in cmd window menu keep; do
  L="$S/$how.log"
  SC="developer 1;vr_stealth_meter 0;vr_scenario flat_parry;wait120;notarget"
  case $how in
    cmd) SC="$SC;vr_armourstudio;wait120;vr_armourstudio_close;wait10" ;;
    window|keep)
      [ "$how" = keep ] && SC="$SC;vr_armourstudio_autoclose 0"
      SC="$SC;vr_armourstudio;wait120;vr_armourstudio_window open;wait10;vr_armourstudio_window_close;wait10" ;;
    menu) SC="$SC;menu_vr;wait10;vr_armourstudio;wait120;togglemenu;wait2;togglemenu;wait10" ;;
  esac
  SC="$SC;echo AS freeze;vr_agrunt_freeze;notarget;wait400;echo AS hurt;vr_test_hurt 5;wait5"
  [ "$how" = keep ] && SC="$SC;vr_armourstudio_close;vr_armourstudio_autoclose 1;wait5"
  SC="$SC;echo AS done;toggleconsole;quit"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SC" -QuitOn "^AS done" -Timeout 200 \
    -Filter '^AS |armourstudio: |"vr_agrunt_freeze" is|^dirparry|^parry|^test hurt|ERROR|CRASH' > "$L" 2>&1
  after() { awk -v m="AS $1" 'index($0,m)==1{on=1;next} on' "$L"; }
  if [ "$how" = keep ]; then
    check $(grep -aq "armourstudio: closed" <<<"$(awk '/^AS freeze/{exit} {print}' "$L")" && echo 0 || echo 1) "keep: vr_armourstudio_autoclose 0, the window closed: the studio still runs"
    check $(after freeze | grep -aq '"vr_agrunt_freeze" is "1"' && echo 1 || echo 0) "keep: the armed humanoids still held ($(after freeze | grep -a -m1 'vr_agrunt_freeze' | tr -d '\r'))"
    continue
  fi
  check $(grep -aq "^armourstudio: closed" "$L" && echo 1 || echo 0) "$how: the studio closed ($(grep -a -m1 '^armourstudio: closed' "$L" | tr -d '\r'))"
  check $(after freeze | grep -aq '"vr_agrunt_freeze" is "0"' && echo 1 || echo 0) "$how: vr_agrunt_freeze back to 0"
  check $(after freeze | awk '/^AS hurt/{exit} {print}' | grep -aqE "^dirparry|^parry" && echo 1 || echo 0) "$how: the grunt strikes at you again ($(after freeze | grep -aE -m1 '^dirparry|^parry' | cut -c1-80))"
  h=$(after hurt | grep -a -m1 '^test hurt')
  check $(awk -v l="$h" 'BEGIN{n=split(l,a," "); for(i=1;i<=n;i++) if(a[i]=="->"){b=a[i-1]+0; c=a[i+1]+0}; exit !(l ~ /monster_army/ && b - c >= 4.9)}' && echo 1 || echo 0) "$how: a test shot hurts it (${h:-no test hurt line})"
done
verdict

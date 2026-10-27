#!/bin/bash
# ad_hub_test.sh <agent>: Arcane Dimensions' hub (start), AD the running campaign (vr_campaign_native ad):
#  - a new game's loadout is AD's own (start's worldspawn: give_weapons 7 = shotgun, super shotgun, nailgun;
#    reset_health 75; green armour 25), on the hands and holsters (printed 200 frames in: his hub autosave, 2.6 s in,
#    had them all);
#  - the original Quake slipgates on its lower level (trigger_changelevel to e1m1, e2m1, e3m1, e4m1 and end, id1's
#    maps under AD's folder) change the map when walked into (round 22: refused as another campaign's, "use the
#    campaign selector", so nothing happened).
# One game per gate: the player put inside it (setpos, then noclip off: he falls and touches it) as the script's last
# command (the QC's changelevel is queued behind whatever the script still has); the run ends when the gate's map
# prints its title (-QuitOn), or fails on its timeout. Verdict: ad_hub_check.py ("adhub: N passed, M failed").
set -eu
AGENT=${1:?worker name}
K=C:/OHWorkspace/qvr-kit
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/ad_hub_test.log
GAMES='-game ad -game quakevr'
FILTER='^ADHUB|^"mapname"|^items |^health |^armorvalue |^holsterweaponmodel|campaign|Host_Error|SV_Error|ENGINE ERROR|CRASH|exit=|TIMEOUT'
SAV=$TREE/quakevr/adhubtest.sav
if [ -f "$SAV" ]; then mkdir -p "$TREE/scratch"; mv "$SAV" "$TREE/scratch/old_adhubtest_$$.sav"; fi # (an old one never checked)
bash "$K/run.sh" "$AGENT" -Games "$GAMES" -ExtraArgs '-nomapindex -noaddons' -Filter "$FILTER" -QuitOn '^ADHUB done' -Timeout 120 \
  -Script 'vr_campaign_native ad;wait30;map start;wait200;save adhubtest;echo ADHUB done;toggleconsole;quit' > "$OUT"
# each gate's map, a spot inside its trigger (the brush's middle, start.bsp) and the title the map prints
set -- e1m1 "-476 -576 -200" "the Slipgate Complex" e2m1 "-1216 -1060 -176" "the Installation" \
  e3m1 "-1956 1024 -96" "Termination Central" e4m1 "-576 1508 -560" "the Sewage System" end "-608 -608 -1032" "Shub-Niggurath"
while [ $# -ge 3 ]; do
  echo "ADHUB gate $1" >> "$OUT"
  bash "$K/run.sh" "$AGENT" -Games "$GAMES" -ExtraArgs '-nomapindex -noaddons' -Filter "$FILTER|$3" -QuitOn "$3" -Timeout 120 \
    -Script "vr_campaign_native ad;wait30;map start;wait30;sv_cheats 1;setpos $2;noclip" >> "$OUT"
  shift 3
done
python "$TREE/Misc/quakevr/ad_hub_check.py" "$OUT" "$TREE/quakevr/adhubtest.sav"

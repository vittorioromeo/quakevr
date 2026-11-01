#!/bin/bash
# maps_panel_switch_test.sh <agent> -- the Levels (Maps menu) on a second menu panel across a campaign switch (ROUND23.md,
# "Crash switching campaign with the Levels on a panel"). The switch frees and rebuilds the level list the Levels' items
# point into; the panel, hidden while the map loads, is drawn again when the menu reopens. Before the fix that draw read
# the freed items (access violation in M_Maps_Draw); now the Levels rebuild from the new list (extralevels_generation).
# PASS: two switches ("game" changed), each followed by one "Levels rebuilt" with levels in it (the kit's campaigns all
# mount the same folders, so the counts match; a mod folder such as Arcane Dimensions' changes them), panel 2 still on the
# Levels each time, the run reaching its end line.
AGENT=${1:-mapscrash}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="developer 1;vr_loading_hold 0;map vrstart;wait30;togglemenu;wait5;vr_menu_panels open;wait5;menu_maps;wait10"
S="$S;vr_menu_panels;vr_menu_panels focus 1;wait2;vr_campaign_select hipnotic;wait10;togglemenu;wait10;vr_menu_panels;wait5"
S="$S;vr_menu_panels focus 1;wait2;vr_campaign_select id1;wait10;togglemenu;wait10;vr_menu_panels;echo mapspanel: end;toggleconsole;quit"
out=$(bash $KIT/run.sh $AGENT -Timeout 240 -Script "$S" -QuitOn "mapspanel: end" \
    -Filter "Levels rebuilt|MENUPANEL|changed to|VR: .*(missing|not ready|incomplete)|mapspanel|ENGINE|crash|exit=" "$@")
echo "$out" | grep -v "^  0000" | head -20
echo "$out" | python -c "
import re, sys
t = sys.stdin.read()
counts = [int(m) for m in re.findall(r'Levels rebuilt: (\d+) levels', t)]
panels = re.findall(r'MENUPANEL 2 menu (\d+) .* drawn (-?\d+)', t)
switches = t.split('MENUPANELS', 1)[-1].count('\"game\" changed to')
ok = (switches == 2 and len(counts) == 2 and min(counts) > 0 and len(panels) == 3
      and all(p[0] == panels[0][0] for p in panels)
      and 'mapspanel: end' in t and 'exit=quit-on' in t)
print('mapspanel: switches', switches, 'levels', counts, 'panel 2', panels, '->', 'PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
"

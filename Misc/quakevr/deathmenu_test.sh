# deathmenu_test.sh <agent> [case regex] -- the death menu in the VR mock (ROUND22.md, "Author's notes of 2026-10-20"):
# each case prints its lines (Death menu: opening / not opened, MENUPANEL, the restart) to read.
NAME=${1:-release}
F=${2:-.*}
WT=C:/OHWorkspace/qvr-agents/$NAME
mkdir -p "$WT/scratch"
DIE='give h 1; vr_test_spawn 0; vr_test_spawn_dist 150; impulse 241; vr_waitprint 1500 Death menu: the player died'
FIL='Death menu|MENUPANEL|Autoload|restart|SpawnServer|Loading|timed out|exit=|ENGINE|deadtime'
run() {
  [[ "$1" =~ $F ]] || return 0
  echo "== $1"
  bash C:/OHWorkspace/qvr-kit/run.sh "$NAME" -Script "developer 1; sv_autosave 0; $2; wait5; toggleconsole; quit" -Filter "$FIL" -Timeout 150 > "$WT/scratch/dm_$1.txt" 2>&1
  grep -E "$FIL" "$WT/scratch/dm_$1.txt" | grep -v "^  video" | cut -c1-200 | head -20
}
# A: a map started from a menu (m_state left at m_main): the menu opens after 2 s, in front of the head
run stale "map e1m1; wait30; menu_main; wait5; map e1m1; wait60; $DIE; wait60; vr_menu_panel; echo deadtime early; wait200; vr_menu_panel"
# B: Restart the Level (row 2) from the open menu
run restart "map e1m1; wait60; menu_main; wait5; map e1m1; wait60; $DIE; wait220; vr_mock_key downarrow; wait3; vr_mock_key enter; wait120; echo deadtime after"
# C: a press before the delay opens the menu at once, and never respawns (author's note 2026-10-22: only its choices do)
run early "map e1m1; wait60; save qvr_dm_test; wait10; $DIE; wait90; +attack; wait3; -attack; wait3; +attack; wait3; -attack; wait200; echo deadtime after"
# D: Respawn (row 1) from the open menu: QC's respawn (Death menu: Respawn), the restart loading the last save
run respawn "map e1m1; wait60; save qvr_dm_test; wait10; $DIE; wait220; vr_mock_key enter; wait120; echo deadtime after"
# E: on Quake VR's own maps too (vrragtriggers: the author's 2026-10-22 death there had no menu)
run vrmap "map vrragtriggers; wait60; $DIE; wait220; vr_menu_panel; echo deadtime after"
# F: Back closes it; a press opens it again (no respawn)
run back "map e1m1; wait60; $DIE; wait220; vr_mock_key escape; wait30; +attack; wait3; -attack; wait60; vr_menu_panel; echo deadtime after"

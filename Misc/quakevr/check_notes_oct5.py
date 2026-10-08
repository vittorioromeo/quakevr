"""Validate the October 5 voice-note mock review."""
from pathlib import Path
import re
import sys
text=Path(sys.argv[1]).read_text(errors='replace')
assert 'REVIEW_DONE' in text
assert 'Host_Error' not in text and 'Sys_Error' not in text
before=text.split('CASE_room_before',1)[1].split('CASE_room_after',1)[0]
after=text.split('CASE_room_after',1)[1].split('CASE_portal_pull',1)[0]
assert 'player at (544 1376 24)' in before
assert 'carried edict 1 through side 1' in before
assert re.search(r'player at \(544 15\d\d 28\)',after)
assert 'portalpulltest: selected=1' in text
assert 'portalpulltest: gate=2' in text
assert 'force grab: crossed teleporter 2' in text and 'force grab: caught' in text
assert 'force grab: portal pull cancelled, object dropped in its room' in text
assert 'buttongibtest: small_rejected=1 large_allowed=1' in text
for test in (23,24,27):
 assert re.search(rf'decaptest: {test} .*monster_army: beheaded.*headless 1.*popped 1, gibbed 0',text)
for test in (25,26):
 assert re.search(rf'decaptest: {test} .*monster_army: not beheaded.*headless 0.*popped 0',text)
assert 'gremlingrabtest: held=1' in text
assert re.search(r'decaptest: 23 .*monster_gremlin: beheaded.*popped 1, gibbed 0',text)
assert re.search(r'corpse: monster_gremlin hit by player \(shots x1.00\) for 56.0, 44.0 left',text)
print('PASS: room-scale crossing, portal selection/flight/catch, blunt head/body outcomes, fragment buttons, gremlin pickup/melee/gun damage')

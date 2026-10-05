"""Check the rendered mock review against the gameplay QC classifier, not a duplicate Python implementation."""
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(errors='replace')
assert 'REVIEW_DONE' in text and 'Host_Error:' not in text
rows = {}
label = None
for line in text.splitlines():
    if line.startswith('CASE_'):
        label = line.strip()[5:]
    elif line.startswith('vr_hitzones_check:') and label:
        rows[label] = dict(re.findall(r'(\w+)=([\da-f]+)', line))
for label in ('stand', 'animated', 'pain', 'priority_off', 'xray', 'both', 'corpse'):
    row = rows[label]
    assert int(row['models']) > 0 and int(row['fallback']) == 0, (label, row)
    assert int(row['checked']) == int(row['samples']) > 100, (label, row)
    assert int(row['mismatch']) == 0 and int(row['classifier']) == 1, (label, row)
    assert all(int(row[key]) > 0 for key in ('head', 'body', 'limbs', 'legs')), (label, row)
assert len({rows[label]['pose'] for label in ('stand', 'animated', 'pain')}) == 3
assert rows['priority_off']['priority'] == '0' and rows['stand']['priority'] == '1'
assert rows['priority_off']['head'] != rows['stand']['head']
assert rows['xray']['xray'] == '1'
assert int(rows['corpse']['models']) > int(rows['stand']['models'])
assert int(rows['fallback']['fallback']) > 0 and int(rows['fallback']['samples']) == 0
for label in ('decap_only', 'off', 'map_reset'):
    assert rows[label]['models'] == rows[label]['samples'] == '0', (label, rows[label])
print('PASS: animated regions match QC; animation and pain poses change; priority, xray, corpse, fallback, off and map reset work')

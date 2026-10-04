"""Verify the scripted mock-VR effects review logs, including real lifetimes and collision outcomes."""
import re
import sys
from pathlib import Path

def cases(path, prefix):
    text = Path(path).read_text(errors='replace')
    assert 'REVIEW_DONE' in text, f'{path}: review did not finish'
    assert 'Host_Error:' not in text and 'ENGINE CRASH' not in text, path
    result = {}
    label = None
    for line in text.splitlines():
        if line.startswith('CASE_'):
            label = line.strip()[5:]
        elif line.startswith(prefix) and label:
            values = dict(re.findall(r'(\w+)=(-?[\d.]+(?:\.\.[\d.]+)?)', line))
            result.setdefault(label, []).append(values)
    return text, result

def main():
    dtext, debris = cases(sys.argv[1], 'explosiondebris:')
    for i, label in enumerate(('normal', 'colored', 'tar', 'preset'), 1):
        row = debris[label][0]
        assert int(row['made']) == i * 8, (label, row)
        assert int(row['fireballs']) == i * 3, (label, row)
        assert int(row['live']) == min(i * 8, 10), (label, row)
        assert int(row['health']) == 100, row
        lo, hi = map(float, row['speed'].split('..'))
        assert 3 <= lo <= hi <= 9, row
        lo, hi = map(float, row['life'].split('..'))
        assert 2 <= lo <= hi <= 3, row
        assert 0 < int(row['lights']) <= 8 and int(row['rendered']) > 0, row
    assert any(int(row['bounces']) > 0 and int(row['trails']) > 0 for row in debris['preset'])
    assert int(debris['expired'][0]['live']) == 0 and int(debris['expired'][0]['expired']) == 10
    assert int(debris['particles_off'][0]['made']) == 8 and int(debris['particles_off'][0]['live']) == 8
    assert int(debris['reduce_cap'][0]['live']) == 4
    for label in ('disabled', 'zero_cap', 'map_reset'):
        assert int(debris[label][0]['live']) == 0 and int(debris[label][0]['lights']) == 0, label
    assert int(debris['map_reset'][0]['made']) == 0
    ftext, fire = cases(sys.argv[2], 'fireparticles:')
    on = fire['fire_on'][0]
    assert int(on['static']) > 0 and int(on['torches']) > 0 and int(on['made']) > 0, on
    assert int(fire['fire_off'][0]['clocks']) == 0
    assert fire['fire_off'][0]['made'] == on['made'] == fire['zero_frequency'][0]['made']
    assert int(fire['fire_resumed'][0]['made']) > int(on['made'])
    tilt = {int(a): (float(h), float(z), int(n)) for a,h,z,n in re.findall(r'tilttest: angle=(\d+) height=([\d.]+) axisZ=(-?[\d.]+).*?flames=(\d+)', ftext)}
    assert tilt[0][2] == tilt[90][2] == tilt[180][2] == 1
    assert abs(tilt[0][0] - tilt[90][0]) < .001
    assert abs(tilt[180][0] / tilt[0][0] - .15) < .001 and tilt[180][1] == -1
    if len(sys.argv) > 3:
        _, sources = cases(sys.argv[3], 'fireparticles:')
        assert int(sources['held_torch'][0]['torches']) > 0
        assert int(sources['burning_monster'][0]['dynamic']) > 0
    print('PASS: explosion coverage, exact spawn counts, caps, bounds, bounces, trails, light budget, expiry, no player damage, reset; fire sources, toggles, frequency and single inverted flame')

if __name__ == '__main__':
    main()

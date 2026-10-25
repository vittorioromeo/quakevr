#!/usr/bin/env python3
"""body_detail_test.py <agent>: body-only mask detail, routing changes and foreign-blood isolation."""
import os
from pathlib import Path
import re
import subprocess
import sys


def main():
    agent = sys.argv[1]
    kit = Path(os.environ.get('KIT', 'C:/OHWorkspace/qvr-kit'))
    bash = os.environ.get('BASH', 'C:/Program Files/Git/bin/bash.exe' if os.name == 'nt' else 'bash')
    script = ('vr_fixed_frames 1;map vrfiringrange;wait60;god;notarget;vr_body_mode 3;'
              'vr_hand_rig 1;impulse 150;vr_body_bracers 0;vr_wounds 1;vr_body_blood 0')
    cases = [(0, 0), (0, 1024), (512, 1024), (0, 1024), (0, 0)]
    for hands, body in cases:
        script += (f';vr_wounds_own_res {hands};vr_wounds_body_res {body};wait10;'
                   'vr_gore_hands_test main 0.8;vr_gore_hands_test off 0.8;'
                   'vr_wounds_test self 1 20 4 14;wait10;vr_wounds_info')
    script += ';toggleconsole;quit'
    result = subprocess.run([bash, str(kit / 'run.sh'), agent, '-Script', script,
                             '-Filter', 'wounds:|foreign blood slot|ERROR|Host_Error|Sys_Error|CRASH'],
                            capture_output=True, text=True)
    text = result.stdout + result.stderr
    assert result.returncode == 0, text
    assert not re.search(r'ERROR|Host_Error|Sys_Error|CRASH', text), text
    reports = re.split(r'wounds: own resolution hands ', text)[1:]
    assert len(reports) == len(cases), text
    for report, (hands, body) in zip(reports, cases):
        assert report.startswith(f'{hands} body {body} '), report
        models = re.findall(r'progs/(hand_rig|vrbody[^ .]*)\.mdl.*?'
                            r'(\d+)x(\d+), foreign blood slot (\d+)', report)
        assert len(models) == 3, report
        slots = [int(m[3]) for m in models]
        assert len(set(slots)) == 3, ('foreign blood shared between your models', models)
        for name, width, height, _ in models:
            expected = (256, 1024) if body else (64, 256)
            if name == 'hand_rig':
                expected = (hands or 256, hands or 256)
            assert (int(width), int(height)) == expected, (models, expected)
        print(f'PASS hands {hands}, body {body}: dimensions and three independent foreign-blood slots')


if __name__ == '__main__':
    main()

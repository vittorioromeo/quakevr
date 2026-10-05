"""Reproduce second-hand torch acquisition, movement and hand transfers on portal maps.

Use a disposable base under build-cmake. Requires the existing benchmark runtime
assets; it spawns a wall torch through the normal QC spawn and immediate-grip test.
"""
import argparse
import json
from pathlib import Path
import re

import perf_suite


def main(args):
    original = perf_suite.script
    results = []
    for map_name in args.maps.split(','):
        def configured(name, frames, eye, gpu, screenshot):
            config = original(name, frames, eye, gpu, screenshot).replace('map vrfiringrange', f'map {map_name}')
            setup = ['developer 1', 'vr_debug_carry 1', 'vr_prop_two_hands_17 1',
                'vr_physics_spawn light_torch_small_walltorch 64 0',
                'vr_mock_hand main 0.1 1.25 -0.25 70 0 0',
                'vr_mock_hand off -0.3 1.25 -0.25 70 0 0'] + perf_suite.waits(20)
            setup += ['+grabright', 'vr_mock_button main grip 1', 'vr_test_walltorch_shot 24'] + perf_suite.waits(30)
            # Broad reach isolates acquisition from small differences in model/finger settings.
            # Server and client still use their real carry/clearance/portal paths.
            setup += ['vr_carry_two_hands_reach 100', 'vr_carry_two_hands_detach 100',
                'vr_mock_hand off 0.1 1.25 -0.25 70 0 0'] + perf_suite.waits(6)
            setup += ['+graboff', 'vr_mock_button off grip 1'] + perf_suite.waits(30)
            setup += ['vr_mock_hand main 0.12 1.3 -0.2 90 15 0',
                'vr_mock_hand off 0.08 1.27 -0.2 90 15 0'] + perf_suite.waits(30)
            for command, button in [('-grabright', 'main grip 0'), ('+grabright', 'main grip 1'),
                    ('-graboff', 'off grip 0'), ('+graboff', 'off grip 1')]:
                setup += [command, 'vr_mock_button ' + button] + perf_suite.waits(30)
            setup += ['-grabright', 'vr_mock_button main grip 0', '-graboff', 'vr_mock_button off grip 0'] + perf_suite.waits(30)
            return config.replace('echo BENCH_DONE', '\n'.join(setup) + '\necho BENCH_DONE')
        perf_suite.script = configured
        output = Path(args.output) / map_name
        run_args = argparse.Namespace(base=args.base, exe=args.exe, output=str(output),
            scenes='idle', frames=180, reps=1, eye=1024, gpu=0, cvar=[])
        perf_suite.run(run_args)
        for path in output.glob('*/qconsole.log'):
            log = path.read_text(errors='replace')
            both = len(re.findall(r'^carry: both hands$', log, re.M))
            kept = len(re.findall(r'^carry: kept by hand', log, re.M))
            if 'walltorch grip result: detached=1 held=1' not in log or both != 3 or kept != 2:
                raise RuntimeError(f'Torch acquisition/transfers were not exercised: {path}, both={both}, kept={kept}')
            results.append(dict(map=map_name, both_grabs=both, transfers=kept, log=str(path)))
    perf_suite.script = original
    destination = Path(args.output) / 'summary.json'
    destination.write_text(json.dumps(results, indent=2) + '\n')
    print(f'PASS: torch two-hand acquisition, movement and transfers; {destination}')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base', required=True)
    p.add_argument('--exe', required=True)
    p.add_argument('--output', required=True)
    p.add_argument('--maps', default='start,e1m1,vrfiringrange')
    main(p.parse_args())

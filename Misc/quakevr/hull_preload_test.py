"""Check loaded brush hull completeness and capture native-map combat after loading.

Use a disposable benchmark base. With --baseline-exe, compare against the previous
executable; stack capture measures allocation traffic, not CPU performance.
"""
import argparse
import json
from pathlib import Path
import re

import allocation_test
import perf_suite


def main(args):
    original = perf_suite.script
    results = []
    variants = [('preload', args.exe)]
    if args.baseline_exe:
        variants.insert(0, ('baseline', args.baseline_exe))
    for variant, exe in variants:
        for map_name in args.maps.split(','):
            def configured(name, frames, eye, gpu, screenshot):
                config = original(name, frames, eye, gpu, screenshot)
                config = config.replace('map vrfiringrange', f'map {map_name}')
                config = config.replace('god 1', 'vr_startup_times\ngod 1')
                config = config.replace('notarget 1', 'notarget 0')
                if args.traverse:
                    return config.replace('echo BENCH_MEASURE_BEGIN',
                        'vr_hull_walktest 12 7\necho BENCH_MEASURE_BEGIN')
                # Check completeness before any measured combat, then capture stats again
                # so the control cannot hide an automatic-preparation miss.
                config = config.replace('echo BENCH_WARM_END',
                    'echo HULL_CHECK_BEGIN\n' + ('vr_hull_preloadtest\n' if variant == 'preload' else '') +
                    'vr_hull_warmcache\nvr_hull_stats\necho HULL_CHECK_END\necho BENCH_WARM_END')
                return config
            perf_suite.script = configured
            output = Path(args.output) / variant / map_name
            run_args = argparse.Namespace(base=args.base, exe=exe, output=str(output),
                scenes='idle', frames=args.frames, reps=args.reps, eye=1024, gpu=0,
                extra_warm=0, self_test=True, peak_sites=True, hull_audit=True,
                prewarm_hulls=False)
            allocation_test.main(run_args)
            for path in output.glob('*/qconsole.log'):
                log = path.read_text(errors='replace')
                warm = re.search(r'hull_audit: warmcache built=(\d+)', log)
                prepared = re.findall(r'hull_audit: prepared models=(\d+) sizes=(\d+) builds=(\d+) '
                    r'bytes_before=(\d+) bytes_after=(\d+) ms=([\d.]+)', log)
                peak = re.search(r'peak (\d+) requests, (\d+) bytes at trace frame', log)
                check = '' if args.traverse else log.split('HULL_CHECK_BEGIN', 1)[1].split('HULL_CHECK_END', 1)[0]
                hashes = re.findall(r'hull: hash tree ([\dx.]+) ([0-9a-f]+)', check)
                if not args.traverse and variant == 'preload' and 'vr_hull_preloadtest: PASS' not in check:
                    raise RuntimeError(f'Prepared traces differ from fresh node builds with the same planes: {path}')
                if not args.traverse and (not warm or (variant == 'preload' and (int(warm[1]) != 0 or not prepared))):
                    raise RuntimeError(f'Loaded brush hulls were not fully prepared: {path}')
                measured = log.split('BENCH_MEASURE_BEGIN', 1)[1].split('BENCH_MEASURE_END', 1)[0]
                if args.traverse and variant == 'preload' and ('hull_audit: build ' in measured or 'hull_audit: brushes ' in measured):
                    raise RuntimeError(f'Unexpected hull work for loaded sizes during traversal: {path}')
                results.append(dict(variant=variant, map=map_name, log=str(path),
                    remaining_builds=int(warm[1]) if warm else None, prepared=prepared, tree_hashes=hashes,
                    trace_check=re.findall(r'vr_hull_preloadtest: PASS traces=(\d+) missing=0 mismatches=0', check),
                    peak_requests=int(peak[1]), peak_bytes=int(peak[2]),
                    walk=re.findall(r'hullwalk[^\n]*', log),
                    measured_hull_builds=measured.count('hull_audit: build ')))
    perf_suite.script = original
    destination = Path(args.output) / 'summary.json'
    destination.write_text(json.dumps(results, indent=2) + '\n')
    print(f'PASS: hull preparation checks; {destination}', flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base', required=True)
    p.add_argument('--exe', required=True)
    p.add_argument('--baseline-exe')
    p.add_argument('--output', required=True)
    p.add_argument('--maps', default='vrfiringrange,e1m1,e2m2,start')
    p.add_argument('--frames', type=int, default=900)
    p.add_argument('--reps', type=int, default=2)
    p.add_argument('--traverse', action='store_true', help='Walk for 12 seconds during capture without manual prewarming')
    main(p.parse_args())

"""Allocation-site regression runs against a disposable Quake VR benchmark base.
Tracing changes CPU timing. These runs measure traffic, not throughput or live heap size.
"""
import argparse
import re
from pathlib import Path
import perf_suite


def main(args):
    original = perf_suite.script

    def traced(name, frames, eye, gpu, screenshot):
        config = original(name, frames, eye, gpu, screenshot)
        if args.extra_warm:
            config = config.replace('echo BENCH_WARM_END', '\n'.join(perf_suite.waits(args.extra_warm)) + '\necho BENCH_WARM_END')
        checks = 'vr_alloc_test\n' if args.self_test else ''
        if args.hull_audit:
            # Enable before combat setup as well as during the measured window.
            config = 'vr_hull_audit 1\n' + config
            config = config.replace('echo BENCH_MEASURE_BEGIN', 'vr_hull_stats\necho BENCH_MEASURE_BEGIN')
            config = config.replace('echo BENCH_MEASURE_END', 'vr_hull_stats\necho BENCH_MEASURE_END')
        if args.prewarm_hulls:
            config = config.replace('echo BENCH_WARM_END', 'vr_hull_warmcache\necho BENCH_WARM_END')
        peak = ' 1' if args.peak_sites else ''
        return config.replace('echo BENCH_MEASURE_BEGIN', checks + f'vr_alloc_sites {frames} 200{peak}\necho BENCH_MEASURE_BEGIN')

    perf_suite.script = traced
    perf_suite.run(args)
    for log in Path(args.output).glob('*/qconsole.log'):
        text = log.read_text(errors='replace')
        if args.self_test and 'vr_alloc_test: PASS' not in text:
            raise RuntimeError(f'Heap wrapper self-test failed: {log}')
        if 'vr_alloc_sites:' not in text or 'heap events in' not in text:
            raise RuntimeError(f'Allocation trace missing: {log}')
        if 'not placed: the table was full' in text:
            raise RuntimeError(f'Allocation trace overflowed: {log}')
        if args.peak_sites:
            peak = re.search(r'peak (\d+) requests, (\d+) bytes at trace frame', text)
            sites = re.findall(r'^peak_site (\w+)\s+(\d+) bytes=(\d+)', text, re.M)
            requests = sum(int(n) for kind, n, _ in sites if kind not in ('delete', 'free'))
            requested_bytes = sum(int(n) for _, _, n in sites)
            if not peak or requests != int(peak[1]) or requested_bytes != int(peak[2]):
                raise RuntimeError(f'Peak stack totals do not match the frame counters: {log}')



if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base', required=True)
    p.add_argument('--exe', required=True)
    p.add_argument('--output', required=True)
    p.add_argument('--scenes', default='idle,particles_retro_fast,portal,enemies_fine,props_active_1000')
    p.add_argument('--frames', type=int, default=360)
    p.add_argument('--reps', type=int, default=1)
    p.add_argument('--eye', type=int, default=1024)
    p.add_argument('--gpu', type=int, default=0)
    p.add_argument('--extra-warm', type=int, default=0)
    p.add_argument('--self-test', action='store_true')
    p.add_argument('--peak-sites', action='store_true', help='Report stacks from the busiest individual frame')
    p.add_argument('--hull-audit', action='store_true', help='Log hull builds and cache clears with host frame numbers')
    p.add_argument('--prewarm-hulls', action='store_true', help='Prebuild loaded brush hulls before the measured window')
    main(p.parse_args())

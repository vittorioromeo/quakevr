"""Alternate one numeric VR cvar in one process, resetting the scene for each sample.

Use a disposable game base. Concurrent workloads still affect these measurements;
compare paired scope timings, not historical absolute frame times.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from perf_suite import script, waits


def run(args):
    base, exe, output = [Path(p).resolve() for p in (args.base, args.exe, args.output)]
    if 'build-cmake' not in base.parts:
        raise ValueError('Use a disposable base under build-cmake')
    output.mkdir(parents=True, exist_ok=False)
    phases, commands = [], []
    for rep in range(args.reps):
        for value in ((0, 1) if rep % 2 == 0 else (1, 0)):
            tag = f'{args.scene}_{value}_r{rep + 1}'
            phases.append((tag, value, rep + 1))
            phase = script(args.scene, args.frames, args.eye, args.gpu, False).splitlines()
            phase = phase[:phase.index('echo BENCH_DONE')]
            phase.insert(phase.index('vr_profile 1'), f'{args.cvar} {value}')
            commands += [f'echo PAIR_BEGIN {tag}'] + phase
            commands += [f'echo PAIR_END {tag}', 'disconnect'] + waits(10)
    commands += ['echo PAIR_DONE', 'quit']
    config = '\n'.join(commands) + '\n'
    (base / 'quakevr/autoexec.cfg').write_text(config)
    (output / 'autoexec.cfg').write_text(config)
    env = dict(os.environ, QVR_TEST_HIDDEN='1', QVR_TEST_BACKGROUND='1', QVR_NO_ERROR_DIALOG='1')
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(exe), '-basedir', str(base), '-game', 'hipnotic', '-game', 'rogue',
                                '-game', 'quakevr', '-window', '-width', '960', '-height', '540',
                                '-nosound', '-noconfigwrite', '-noaddons', '-condebug'],
                               cwd=base, env=env, startupinfo=startup)
    try:
        code = process.wait(timeout=600)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait()
        raise
    log = (base / 'qconsole.log').read_text(errors='replace')
    (output / 'qconsole.log').write_text(log)
    if code or 'PAIR_DONE' not in log or re.search(r'(?m)^(?:Sys_Error|Host_Error):', log):
        raise RuntimeError('Paired fixture failed; see qconsole.log')
    exe_hash = hashlib.sha256(exe.read_bytes()).hexdigest()
    qc_hash = hashlib.sha256((base / 'quakevr/progs.dat').read_bytes()).hexdigest()
    used = set()
    for tag, value, rep in phases:
        block = log.split(f'PAIR_BEGIN {tag}', 1)[1].split(f'PAIR_END {tag}', 1)[0]
        captures = set(re.findall(r'profile[/\\](profile_[^\r\n]+\.csv)', block))
        if len(captures) != 1 or captures & used:
            raise RuntimeError('Scope capture names collided; increase --frames')
        used |= captures
        destination = output / tag
        destination.mkdir()
        (destination / 'qconsole.log').write_text(block)
        for capture in captures:
            shutil.copy2(base / 'quakevr/profile' / capture, destination / capture)
        metadata = dict(scene=f'{args.scene}_{args.cvar}_{value}', eye=args.eye,
                        gpu_every=args.gpu, rep=rep, frames_requested=args.frames,
                        cvars=[f'{args.cvar} {value}'], exe_sha256=exe_hash, progs_sha256=qc_hash)
        (destination / 'metadata.json').write_text(json.dumps(metadata, indent=2))
    print(f'Completed {len(phases)} alternating phases: {output}', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True)
    parser.add_argument('--exe', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--scene', required=True)
    parser.add_argument('--cvar', required=True)
    parser.add_argument('--reps', type=int, default=3)
    parser.add_argument('--frames', type=int, default=1800)
    parser.add_argument('--eye', type=int, default=2048)
    parser.add_argument('--gpu', type=int, default=16)
    args = parser.parse_args()
    if not re.fullmatch(r'vr_[a-zA-Z0-9_]+', args.cvar) or args.reps < 1 or args.frames < 900:
        parser.error('Use a numeric VR cvar, positive repeats and at least 900 frames')
    run(args)

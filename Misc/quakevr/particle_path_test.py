"""Same-state stereo screenshots of reference/fast/half retro particle paths in a disposable base."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess

import numpy as np
from PIL import Image
from perf_suite import waits


def fixtures():
    # Smoke growth, blood rotation, additive/streaked sparks, explosion tint and general setting branches.
    return [
        ('smoke_near', [4], 12, []),
        ('smoke_grown', [11], 100, []),
        ('blood', [1], 12, []),
        ('sparks', [5, 3, 10], 6, []),
        ('explosion', [2], 10, []),
        ('heavy', [1, 4, 11], 60, []),
        ('mixed', [1, 4, 11, 5, 2], 12, []),
        ('hard_average', [1, 11], 50, ['vr_retro_particles_soft 0', 'vr_retro_particles_average 1']),
        ('palette_off', [1, 4, 2], 10, ['vr_retro_particles_palette 0']),
        ('snap_off', [1, 4, 2], 10, ['vr_retro_particles_snap 0']),
        ('far_fade', [1, 11, 5], 50, ['vr_retro_particles_block 0.0625', 'vr_retro_particles_fade 1']),
        ('wide_blocks', [1, 4, 2], 10, ['vr_retro_particles_block 4', 'vr_retro_particles_dither 2']),
        ('angled_streaks', [3, 5, 10, 4], 8, []),
        ('soft_off', [1, 11, 5], 20, ['vr_soft_particles 0']),
    ]


def main(args):
    base, output, exe = [Path(p).resolve() for p in (args.base, args.output, args.exe)]
    if 'build-cmake' not in base.parts:
        raise ValueError('Use a disposable base under build-cmake')
    output.mkdir(parents=True, exist_ok=True)
    commands = ['vr_backend mock', 'vr_enabled 1', 'vr_mock_fast 1', 'vr_fixed_frames 1',
                'vr_fixed_frames_rate 72', f'vr_mock_eye_size {args.eye}', 'vr_render_scale 1',
                'vr_window_view 0', 'vr_mirror 2', 'vid_vsync 0', 'host_maxfps 0',
                'sv_autosave 0', 'con_notifytime 0', 'vr_tips 0', 'showpause 0',
                'vr_fire_particles 0', 'vr_explosion_debris 0', 'vr_torch_lights 0',
                'vr_roomscale_move_mult 0', 'vr_profile 0', 'vr_profile_gpu 0'] + waits(80)
    for name, presets, age, settings in fixtures():
        commands += ['vr_particle_freeze 0', 'map vrfiringrange'] + waits(150)
        commands += ['god 1', 'notarget 1', 'vr_particles 1', 'vr_particle_seed 7',
                     'vr_retro 1', 'vr_retro_particles 1', 'vr_retro_particles_snap 1',
                     'vr_retro_particles_block 0.25', 'vr_retro_particles_soft 1',
                     'vr_retro_particles_average 0', 'vr_retro_particles_palette 1',
                     'vr_retro_particles_dither 0.5', 'vr_retro_particles_fade -1',
                     'vr_soft_particles 1', 'vr_particle_retro_halfres 0', 'vr_mock_look 0 0'] + settings
        commands += [f'vr_particle_test {p} {1 if p == 2 else (256 if name == 'heavy' else 32)}' for p in presets] + waits(age)
        commands += ['vr_particle_freeze 1', 'pause']
        if name == 'angled_streaks':
            commands += ['vr_mock_look 12 18']
        commands += waits(10)
        for path, fast, half in [('reference', 0, 0), ('fast', 1, 0), ('reference_again', 0, 0),
                                 ('half', 1, 1), ('half_all', 1, 1)]:
            commands += [f'vr_particle_retro_fast {fast}', f'vr_particle_retro_halfres {half}',
                         f'vr_particle_retro_halfres_pixels {0 if path == "half_all" else 64}',
                         f'cl_screenshotname screenshots/particle_{name}_{path}'] + waits(5) + ['screenshot'] + waits(3)
    commands += ['echo PARTICLE_PATH_TEST_DONE', 'disconnect'] + waits(10) + ['quit']
    config = '\n'.join(commands) + '\n'
    (base / 'quakevr/autoexec.cfg').write_text(config)
    (output / 'autoexec.cfg').write_text(config)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    env = dict(os.environ, QVR_TEST_HIDDEN='1', QVR_TEST_BACKGROUND='1', QVR_NO_ERROR_DIALOG='1')
    process = subprocess.Popen([str(exe), '-basedir', str(base), '-game', 'hipnotic', '-game', 'rogue',
                                '-game', 'quakevr', '-window', '-width', str(args.eye * 2), '-height', str(args.eye),
                                '-nosound', '-noconfigwrite', '-noaddons', '-condebug'], cwd=base, env=env, startupinfo=startup)
    try:
        code = process.wait(timeout=240)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait()
        raise
    log = (base / 'qconsole.log').read_text(errors='replace')
    (output / 'qconsole.log').write_text(log)
    assert code == 0 and 'PARTICLE_PATH_TEST_DONE' in log
    assert not any(x in log for x in ('failed to compile', 'failed to link', 'Host_Error', 'Sys_Error'))
    rows = []
    for name, *_ in fixtures():
        images = {}
        for path in ('reference', 'fast', 'reference_again', 'half', 'half_all'):
            filename = f'particle_{name}_{path}.png'
            matches = re.findall(r'Wrote screenshots/(' + re.escape(filename[:-4]) + r'\d*\.png)', log)
            assert len(matches) == 1, (filename, matches)
            shutil.copy2(base / 'quakevr/screenshots' / matches[0], output / filename)
            images[path] = np.asarray(Image.open(output / filename).convert('RGB')).astype(np.int16)
        reference = images['reference']
        row = {'fixture': name}
        for path in ('fast', 'reference_again', 'half', 'half_all'):
            error = np.abs(images[path] - reference)
            row[path] = {'mae_rgb8': float(error.mean()), 'max_rgb8': int(error.max()),
                         'changed_pixels_pct': float(np.any(error > 0, axis=2).mean() * 100),
                         'over_8_pixels_pct': float(np.any(error > 8, axis=2).mean() * 100)}
        assert row['fast']['mae_rgb8'] < max(0.1, 2 * row['reference_again']['mae_rgb8']), row
        assert row['fast']['over_8_pixels_pct'] < max(0.2, 2 * row['reference_again']['over_8_pixels_pct']), row
        rows.append(row)
        print(name, json.dumps(row['fast']), 'control', row['reference_again']['mae_rgb8'], flush=True)
    (output / 'results.json').write_text(json.dumps({'eye': args.eye, 'exe_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
                                                   'comparisons': rows}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True)
    parser.add_argument('--exe', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--eye', type=int, default=1024)
    main(parser.parse_args())

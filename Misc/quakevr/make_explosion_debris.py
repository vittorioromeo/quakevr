"""Make a molten fullbright skin on an existing irregular debris mesh (no external images)."""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Misc/quakevr/blender/addons/quakevr_models'))
from qpal import PALETTE

def build(src, out_dir, guarded=True):
    """vr_explosion_debris.mdl made from `src` (vr_rock1.mdl) into out_dir (source.py's export calls this: vr_rock1.blend's
    export writes it too)."""
    make(Path(src), Path(out_dir) / 'vr_explosion_debris.mdl')


def main():
    # Since 2026-11 vr_rock1.blend's export makes it (MODELS_IN_BLENDER.md, "Every other model"): this stops for it.
    sys.path.insert(0, str(ROOT / 'Misc/quakevr'))
    import genguard
    out = ROOT / 'quakevr/progs/vr_explosion_debris.mdl'
    genguard.retired('make_explosion_debris.py', [str(out)])
    make(ROOT / 'quakevr/progs/vr_rock1.mdl', out)


def make(source, out):
    data = bytearray(source.read_bytes())
    assert data[:4] == b'IDPO' and struct.unpack_from('<i', data, 4)[0] == 6
    skins, width, height = struct.unpack_from('<3i', data, 48)
    offset = 84
    # Orange/red/yellow fullbright indices, sorted by luminance; preserve the rock's relief as molten shading.
    ramp = sorted(range(229, 237), key=lambda i: sum(PALETTE[i]))
    for _ in range(skins):
        assert struct.unpack_from('<i', data, offset)[0] == 0, 'Expected a single skin, not a group'
        offset += 4
        pixels = data[offset:offset + width * height]
        luminance = [sum(PALETTE[i]) / 3 for i in pixels]
        lo, hi = min(luminance), max(luminance)
        for i, light in enumerate(luminance):
            data[offset + i] = ramp[min(7, int((light - lo) / max(1, hi - lo) * 7))]
        offset += width * height
    out.write_bytes(data)
    print(f'{out}: {skins} molten skins, {width}x{height}, {len(data)} bytes')

if __name__ == '__main__':
    main()

# gpu_settings.py <his ironwail.cfg> <out dir>: the GPU benchmark's settings files (BENCHMARKS.md, "GPU benchmark"): his
# config (read only) with the VR emulation's overrides appended:
#   gpu_fixed.cfg   dynamic resolution off: the eyes at the full size every frame (the worst case)
#   gpu_dynres.cfg  dynamic resolution on (as he plays: his vr_dynres_target / _min)
# Then: bash kit/bench.sh <agent> --scenarios gpu --hz 120 --eye 2688x2784 --settings <out dir>/gpu_fixed.cfg
import sys
from pathlib import Path

src = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
size = {"vid_width": "1920", "vid_height": "1080"}
for line in src.splitlines():
    parts = line.split()
    if len(parts) >= 2 and parts[0] in size:
        size[parts[0]] = parts[1].strip('"')
common = [
    "// --- the GPU benchmark's overrides (Misc/quakevr/bench/gpu_settings.py)",
    "vr_mock_hidden_area 1",  # the runtime's visibility mask (VDXR gives one): the mock's round one, about 17% of the image
    # his desktop window's size (the spectator camera's and the mirror's), not the test runs' 960x540 (the command line
    # locks the size: vid_unlock first)
    "vid_unlock", f"vid_width {size['vid_width']}", f"vid_height {size['vid_height']}", "vid_restart",
]
for name, extra in (("gpu_fixed.cfg", ["vr_dynres 0"]), ("gpu_dynres.cfg", ["vr_dynres 1"])):
    (out / name).write_text(src.rstrip("\n") + "\n" + "\n".join(common + extra) + "\n", encoding="utf-8")
    print(out / name)

"""Validate exports from matching Tracy CLI tools; no third-party Python dependencies.

tracy-csvexport capture.tracy > cpu.csv
tracy-csvexport -g capture.tracy > gpu.csv
python Misc/quakevr/check_tracy_capture.py cpu.csv gpu.csv [--box3d]
"""
import argparse
import csv
from pathlib import Path


def load(path):
    # Windows PowerShell 5 redirection produces UTF-16; newer shells produce UTF-8.
    data = Path(path).read_bytes()
    text = data.decode("utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig")
    return list(csv.DictReader(text.splitlines()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cpu")
    parser.add_argument("gpu")
    parser.add_argument("--box3d", action="store_true")
    args = parser.parse_args()
    cpu, gpu = load(args.cpu), load(args.gpu)
    assert cpu and gpu, "Both CPU and GPU exports must contain zones"
    names = {r["name"] for r in cpu}
    assert {"server", "eye L", "eye R"} <= names, f"Missing engine scopes: {names}"
    assert {"eye L", "eye R"} <= {r["name"] for r in gpu}, "Missing GPU eye zones"
    durations = [int(r["GPU execution time"]) for r in gpu]
    assert min(durations) >= 0 and max(durations) > 0, "Invalid GPU timestamps"
    box = [r for r in cpu if "box3d" in r["src_file"]]
    if args.box3d:
        assert box, "No Box3D instrumentation captured"
    print(f"PASS CPU {len(cpu)} scope names / {sum(int(r['counts']) for r in cpu)} events; "
          f"GPU {len(gpu)} events / {len({r['name'] for r in gpu})} names; "
          f"Box3D {len(box)} names / {sum(int(r['counts']) for r in box)} events")


if __name__ == "__main__":
    main()

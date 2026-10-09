"""gadget_sync_summary.py -- gadget_sync_test.sh's output (stdin) summed up per phase: the frames checked, the largest
distance of the tap zone from the drawn gadget's screen (expected 0) and of the old frame-start test (the lag), how far
the player moved and turned; other lines (taps, presses) passed through."""
import math
import re
import sys

LINE = re.compile(r"gadget sync: (\d+) yaw ([-\d.]+) zone ([-\d.]+) cm, start ([-\d.]+) cm; at ([-\d.]+) ([-\d.]+) ([-\d.]+)")


def flush(phase, rows):
    if not rows:
        return
    zone = max(r[2] for r in rows)
    start = [r[3] for r in rows if r[3] >= 0.0]
    moved = math.dist(rows[0][4:7], rows[-1][4:7])
    turned = sum(abs((b[1] - a[1] + 180.0) % 360.0 - 180.0) for a, b in zip(rows, rows[1:]))
    print(f"{phase}: {len(rows)} frames; zone off the drawn gadget max {zone:.4f} cm; the old frame-start test off "
          f"max {max(start) if start else -1:.2f} cm, mean {sum(start) / len(start) if start else -1:.2f}; moved "
          f"{moved:.0f} units, turned {turned:.0f} deg")


phase, rows = "START", []
for raw in sys.stdin:
    line = raw.strip()
    m = LINE.search(line)
    if m:
        rows.append([float(g) for g in m.groups()])
        continue
    if line.startswith("==="):
        flush(phase, rows)
        phase, rows = line.strip("= ").strip(), []
        print(line)
        continue
    if line and not line.startswith("exit="):
        print(line)
flush(phase, rows)

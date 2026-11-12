#!/usr/bin/env python3
"""taaparticles_check.py <screenshots dir> -- the share of the view's lower half that changed between the folder's
two newest screenshots taken by taaparticles_test.sh (before the blood, then after it): `changed <fraction>`.
A pixel changed: its R+G+B moved by more than 60 (of 765). The lower half: the sky above animates."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixdiff_ab import read_png  # noqa: E402  (the standard-library PNG reader)


def main():
    folder = sys.argv[1]
    shots = sorted((f for f in os.listdir(folder) if f.lower().endswith(".png")),
                   key=lambda f: os.path.getmtime(os.path.join(folder, f)))[-2:]
    if len(shots) < 2:
        print("changed -1 (no two screenshots)")
        return
    (w, h, a), (_, _, b) = (read_png(os.path.join(folder, f)) for f in shots)
    changed = total = 0
    for p in range(w * (h // 2) * 3, w * h * 3, 3):  # (RGB, row after row)
        total += 1
        if abs(a[p] - b[p]) + abs(a[p + 1] - b[p + 1]) + abs(a[p + 2] - b[p + 2]) > 60:
            changed += 1
    print(f"changed {changed / max(total, 1):.4f}")


if __name__ == "__main__":
    main()

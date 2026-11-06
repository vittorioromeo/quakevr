# plates_model_pixels.py <run log> <agent> [folder] -- plates_model_test.sh's pixel half. For each build, the pixels the
# plates change (screenshots from a camera in front of the player, the plates on, then off) in e1m1 and, after the
# campaign switch, in r2m6: at least MIN_CHANGED of the picture in each, and the same silhouette (intersection over
# union at least MIN_IOU: the camera and the pose are the player's; the wrong model in their place, a brush model or a
# gib, draws another shape). Colour-free: r2m6's light is orange, the body's vest green. [folder]: where the log's
# screenshots/ are (default the agent's quakevr folder). Prints each build's numbers, then
# `plates_model_test: N passed, M failed` (with the log's ppmodel tally).
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import quakeimage  # noqa: E402

MIN_CHANGED = 0.04  # of the picture (the plates: 0.08-0.14 of the 960 x 540 screenshot, e1m1 the darker)
MIN_IOU = 0.4       # e1m1's silhouette against r2m6's (the plates: 0.68-0.70, the bug 0.23-0.26)
DIFF = 4            # per channel, the least change counted (e1m1 is dark; the test steadies the light)


def mask(a, b):
    wa, ha, pa = a
    wb, hb, pb = b
    if (wa, ha) != (wb, hb):
        return None
    return bytes(1 if abs(pa[i] - pb[i]) > DIFF or abs(pa[i + 1] - pb[i + 1]) > DIFF or abs(pa[i + 2] - pb[i + 2]) > DIFF
                 else 0 for i in range(0, len(pa), 3))


def main():
    log, agent = sys.argv[1], sys.argv[2]
    shots = sys.argv[3] if len(sys.argv) > 3 else os.path.join("C:/OHWorkspace/qvr-agents", agent, "quakevr")
    text = open(log, encoding="utf-8", errors="replace").read()
    passed = failed = 0
    m = re.search(r"ppmodel: (\d+) passed, (\d+) failed", text)
    if m:
        passed, failed = int(m.group(1)), int(m.group(2))
    else:
        failed += 1
        print("plates_model_test: FAIL no ppmodel verdict")
    paths = {}
    for b, where, state, path in re.findall(r"ppshot: build (\d) (\w+) (on|off)\s*\n\s*Wrote (\S+\.png)", text):
        paths[(b, where, state)] = os.path.join(shots, path)
    for b in ("1", "2", "3"):
        masks = {}
        for where in ("e1m1", "r2m6"):
            imgs = []
            for state in ("on", "off"):
                p = paths.get((b, where, state), "")
                imgs.append(quakeimage.read_rgb(open(p, "rb").read(), "png") if p and os.path.exists(p) else None)
            masks[where] = mask(imgs[0], imgs[1]) if all(imgs) else None
        a, c = masks["e1m1"], masks["r2m6"]
        if a is None or c is None or len(a) != len(c):
            failed += 1
            print(f"plates_model_test: FAIL build {b}: screenshots missing")
            continue
        share_a, share_c = sum(a) / len(a), sum(c) / len(c)
        both = sum(x & y for x, y in zip(a, c))
        either = sum(x | y for x, y in zip(a, c))
        iou = both / max(either, 1)
        ok = share_a >= MIN_CHANGED and share_c >= MIN_CHANGED and iou >= MIN_IOU
        passed, failed = passed + ok, failed + (not ok)
        print(f"plates_model_test: {'ok' if ok else 'FAIL'} build {b}: the plates change {share_a:.3f} of e1m1's picture, "
              f"{share_c:.3f} of r2m6's, the same pixels {iou:.3f}")
    print(f"plates_model_test: {passed} passed, {failed} failed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

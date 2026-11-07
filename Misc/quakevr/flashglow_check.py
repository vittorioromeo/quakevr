import re, sys
# Expected "torchprobe <tag> lens drawn D model M" per tag.
want = {"flatwall": (0, 0), "flatopen": (0, 0), "flatshown": (1, 1), "vrhead": (1, 1)}
got = {}
for l in open(sys.argv[1], errors="replace"):
    m = re.search(r"torchprobe (\w+) lens drawn (\d) model (\d)", l)
    if m:
        got[m.group(1)] = (int(m.group(2)), int(m.group(3)))
ok = bad = 0
for tag, w in want.items():
    g = got.get(tag)
    if g == w:
        ok += 1
    else:
        bad += 1
        print("flashglow: FAIL %s: lens/model %s, want %s" % (tag, g, w))
print("flashglow: %d passed, %d failed" % (ok, bad))

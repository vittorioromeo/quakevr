# compare.py <bench.csv> <before tag> <after tag> [stage...] -- vr_snd_bench's rows (scenes.sh) side by side: for each
# scene and stage, the median, p95 and p99 (ms) and the load (ms per second of sound), before -> after, as a Markdown
# table.
import sys
from collections import defaultdict

path, before, after = sys.argv[1:4]
stages = sys.argv[4:] or ["paint", "spatial", "voices", "reverb", "antialias", "select", "listener", "shadow",
                          "cpu_read", "cpu_direct", "cpu_binaural", "sim_direct", "sim_reflections"]
rows = defaultdict(dict)  # (tag, scene) -> stage -> numbers (the last run of the label)
for line in open(path, encoding="utf-8"):
    f = line.strip().split(",")
    if len(f) < 8:
        continue
    label, stage = f[0], f[1]
    tag, _, scene = label.partition("_")
    rows[(tag, scene)][stage] = [float(x) for x in f[2:8]]
scenes = []
for (tag, scene) in rows:
    if tag == before and (after, scene) in rows and scene not in scenes:
        scenes.append(scene)
print("| scene | stage | median | p95 | p99 | ms/s |")
print("|---|---|---|---|---|---|")
for scene in scenes:
    b, a = rows[(before, scene)], rows[(after, scene)]
    for st in stages:
        if st not in b or st not in a:
            continue
        nb, na = b[st], a[st]
        if max(nb[1:]) == 0 and max(na[1:]) == 0:
            continue
        cells = [f"{nb[i]:.3f} -> {na[i]:.3f}" for i in (1, 2, 3)]
        load = f"{nb[5]:.2f} -> {na[5]:.2f}" if not st.startswith("sim_") else ""
        print(f"| {scene} | {st} | {' | '.join(cells)} | {load} |")

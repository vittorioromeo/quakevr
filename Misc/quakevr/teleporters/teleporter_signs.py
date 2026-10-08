# teleporter_signs.py <png>...: the green sign panels stacked in the middle column of an eye image facing vrteleporters'
# loop gate (teleporter_edges_test.sh recursion; one more each gate deeper): runs of rows with green pixels, centre +-80 px.
import sys
from PIL import Image
for f in sys.argv[1:]:
    im = Image.open(f).convert('RGB'); w, h = im.size; px = im.load()
    rows = []
    for y in range(h // 2 - 300, h // 2 + 100):
        n = sum(1 for x in range(w // 2 - 80, w // 2 + 80) if (lambda r, g, b: g > 50 and g > 1.6 * r and g > 1.3 * b)(*px[x, y]))
        rows.append((y, n))
    runs, cur = [], None
    for y, n in rows:
        if n >= 2:
            cur = [y, y, n] if cur is None else [cur[0], y, cur[2] + n]
        elif cur is not None:
            runs.append(cur); cur = None
    if cur: runs.append(cur)
    runs = [r for r in runs if r[1] - r[0] >= 2]  # a sign is rows tall (the deepest, 9); a lone row of 2 pixels is a speck
    print(f, len(runs), [(a, b, c) for a, b, c in runs])

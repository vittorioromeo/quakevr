import re, sys
# neck_loose_sum.py <label> < run output: NECKANGLE lines summarised (swing and twist: min, max, range, mean frame-to-frame change)
sw, tw = [], []
for line in sys.stdin:
    m = re.search(r"NECKANGLE \d+ swing (-?[\d.]+) twist (-?[\d.]+)", line)
    if m:
        sw.append(float(m.group(1)))
        tw.append(float(m.group(2)))
    elif "necksnap: " in line and "loose" in line:
        print("  " + line.strip())
if not sw:
    print(sys.argv[1], "no samples")
    sys.exit(0)
def d(v):
    return sum(abs(a - b) for a, b in zip(v[1:], v)) / max(len(v) - 1, 1)
print(f"{sys.argv[1]}: {len(sw)} samples; swing {min(sw):.0f}..{max(sw):.0f} (range {max(sw)-min(sw):.0f}, moves {d(sw):.1f}/sample), "
      f"twist {min(tw):.0f}..{max(tw):.0f} (range {max(tw)-min(tw):.0f}, moves {d(tw):.1f}/sample)")

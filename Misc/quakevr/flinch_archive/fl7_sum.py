import re, sys
n = int(sys.argv[2]) if len(sys.argv) > 2 else 40
# join console-wrapped lines
logical = []
for line in open(sys.argv[1], errors="replace"):
    line = line.rstrip("\n")
    if re.match(r"(legs|step|flinch|swing):", line) or not logical:
        logical.append(line)
    else:
        logical[-1] += line
k = 0
for line in logical:
    if line.startswith("step:") or line.startswith("flinch: ") and ("result" in line or "struck with" in line):
        print(line.strip()[:200]); continue
    m = re.match(r"legs: \d+ ([0-9.]+) s, (\d) bearing, push ([0-9.]+), middle ([-0-9.]+) along ([-0-9.]+) up, speed ([-0-9.]+) along, capture ([-0-9.]+) along \(([0-9.]+) past\), pressure ([-0-9.]+).*step (-?\d) .*off upright (-?\d+) deg; at ([-0-9.]+), com ([-0-9.]+), foot ([-0-9.]+) ([-0-9.]+)", line)
    if m and k < n:
        k += 1
        t, b, push, mid, up, sp, cap, past, cop, st, offu, at, com, f2, f3 = m.groups()
        print(f"{t} b{b} push {push} mid {mid} sp {sp} cap {cap} ({past}) cop {cop} st {st} offup {offu} pelv {at} com {com} feet {f2} {f3}")
    elif line.startswith("legs:") and "in the air" in line and k < n:
        k += 1
        print(line.strip()[:120])

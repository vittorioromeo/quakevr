import re, sys
# gp_table.py <gibprops output> : one row per monster: class | head | prop (grabbable, Box3D, taken, held, loose again) |
# destroyable | burst (small gibs) | gibs grabbable/destroyable of n | AD's left
rows = []
cur = None
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    line = line.strip()
    m = re.match(r"== (\d+)", line)
    if m:
        cur = {"n": m.group(1), "cls": "-", "head": "-", "grab": "?", "box": "?", "taken": "?", "held": "?", "loose": "?",
               "destroy": "?", "burst": "?", "sg": "?", "gibs": "?", "adleft": "?", "armed": "?"}
        rows.append(cur)
        continue
    if not cur or not line.startswith("gibprops:"):
        continue
    m = re.match(r"gibprops: (\S+) (gibbed alive|corpse gibbed)", line)
    if m:
        cur["cls"] = m.group(1)
    m = re.search(r"heads (\d+) head (\S+) grabbable (\d) destroyable (\d);", line)
    if m:
        cur.update(head=m.group(2) if m.group(1) != "0" else "none", grab=m.group(3), destroy=m.group(4))
    # (At the blow; then "since": the gibs thrown since, at its death's end too, which replaces it.)
    m = re.search(r"gibs (\d+) grabbable (\d+) destroyable (\d+); AD's (?:left )?(\d+)", line)
    if m:
        cur["gibs"] = "%s/%s d%s" % (m.group(2), m.group(1), m.group(3))
        cur["adleft"] = m.group(4)
    m = re.search(r"box3d (\d) taken (\d)", line)
    if m:
        cur.update(box=m.group(1), taken=m.group(2))
    m = re.search(r"gripped: carried (\d) in hand (\d) held body (\d)", line)
    if m:
        cur["held"] = "1" if m.group(1) == m.group(2) == m.group(3) == "1" else "0"
    m = re.search(r"let go: carried (\d) in hand \d held body \d box3d (\d)", line)
    if m:
        cur["loose"] = "1" if m.group(1) == "0" and m.group(2) == "1" else "0"
    m = re.search(r"armed (\d) burst (\d) small gibs (\d+)", line)
    if m:
        cur.update(armed=m.group(1), burst=m.group(2), sg=m.group(3))
print("| # | class | head model | grabbable | Box3D (landed) | taken / held / loose | destroyable | burst (small gibs) | gibs grab/n, destroyable | AD's left |")
print("|---|---|---|---|---|---|---|---|---|---|")
for r in rows:
    print("| %s | %s | %s | %s | %s | %s/%s/%s | %s | %s (%s) | %s | %s |" % (r["n"], r["cls"], r["head"], r["grab"], r["box"],
          r["taken"], r["held"], r["loose"], r["destroy"], r["burst"], r["sg"], r["gibs"], r["adleft"]))

# Toolgun spawn catalogue dump vs the QC item classnames. Usage: python catalogue.py <worktree> [index]
import re, sys, os
root = sys.argv[1]
vr = os.path.join(root, "Quake", "vr")
src = open(os.path.join(vr, "vr_toolgun.cpp"), encoding="utf-8").read()
body = src[src.index("constexpr Spawnable spawnables[] = {"):]
body = body[:body.index("\n};")]
entries = []
def inc(name):
    return open(os.path.join(vr, name), encoding="utf-8").read()
for line in body.split("\n"):
    s = line.strip()
    m = re.match(r'#include "(.*)"', s)
    if m:
        for l2 in inc(m.group(1)).split("\n"):
            l2 = l2.strip()
            mm = re.match(r'(QVR_AD_SPAWN|QVR_AD_SPAWN_MAP|QVR_AD_GUN|QVR_PACK)\((.*)\)', l2)
            if not mm or l2.startswith("#"):
                continue
            args = [a.strip().strip('"') for a in re.findall(r'"[^"]*"|[^,]+', mm.group(2))]
            if mm.group(1) == "QVR_PACK":
                entries.append((args[0], args[2], args[3], args[6] + "=" + args[7] if args[6] != "nullptr" else ""))
            elif mm.group(1) == "QVR_AD_GUN":
                entries.append(("Weapons", args[0], "func_weapon_grabbable", ""))
            else:
                entries.append((args[0], args[1], args[2], ""))
        continue
    m = re.match(r'(?:\{|QVR_FIRE\()"([^"]*)", "([^"]*)", "([^"]*)"(?:, "[^"]*", [^,]*, ("[^"]*"|nullptr), ([^,]*))?', s)
    if m and not s.startswith("#define"):
        if s.startswith("QVR_FIRE"):
            entries.append(("Fire", m.group(1), m.group(2), ""))
        else:
            key = m.group(4).strip('"') if m.group(4) and m.group(4) != "nullptr" else ""
            entries.append((m.group(1), m.group(2), m.group(3), key + "=" + m.group(5) if key else ""))
if len(sys.argv) > 2:
    i = int(sys.argv[2]); print("entry", i, entries[i]); sys.exit()
items = [e for e in entries if e[0] == "Items"]
print("entries %d, items %d" % (len(entries), len(items)))
for e in items:
    print("  %-28s %-32s %s" % (e[1], e[2], e[3]))
listed = set(e[2] for e in entries) | set(e[2][3:] for e in entries if e[2].startswith("ad_"))
id1 = ["item_health", "item_armor1", "item_armor2", "item_armorInv", "item_shells", "item_spikes", "item_rockets",
       "item_cells", "item_key1", "item_key2", "item_sigil", "item_artifact_super_damage",
       "item_artifact_invulnerability", "item_artifact_invisibility", "item_artifact_envirosuit"]
print("id1 item classnames missing:", [c for c in id1 if c not in listed] or "none")
qc = set()
for f in os.listdir(os.path.join(root, "QC")):
    if f.endswith(".qc"):
        qc |= set(re.findall(r'^void\(\) (item_[A-Za-z0-9_]+)\s*=', open(os.path.join(root, "QC", f), encoding="utf-8", errors="replace").read(), re.M))
print("QC item_ spawn functions not listed:", sorted(c for c in qc if c not in listed))

#!/usr/bin/env python3
# attach_guns_check.py <log | -> -- bayomounts: reads vr_embed_test 96's client reports (QC vr_attach_test.qc VR_ATG_Frame):
# after each `attachguns_mark <where> <model ...>` the next vr_attach_report's `attach_drawn:` lines. Prints one
# "<1|0> <what>" line a check (attach_test.sh's `check`):
#   hand <model>:    the gun in the main hand has its bayonet (kind 1) and the bayonet's mount drawn on it;
#   holster <model>: the holstered gun has its mount drawn; world <model>: the dropped gun has;
#   pickup <models>: the map pickups (drawn as the guns' view models in Quake VR) have one each (as many rail_world).
import re
import sys

text = (sys.stdin if sys.argv[1:] in ([], ["-"]) else open(sys.argv[1], encoding="utf-8", errors="replace")).read()
lines = text.splitlines()
i = 0
while i < len(lines):
    m = re.match(r"attachguns_mark (\w+) (.*)", lines[i].strip())
    if not m:
        i += 1
        continue
    where, models = m.group(1), m.group(2).split()
    drawn = []
    j = i + 1
    while j < len(lines) and not lines[j].startswith("attach_report:"):
        d = re.match(r"attach_drawn: where=(\S+) ent=\S+ kind=(\d+) gun=(\S+)", lines[j])
        if d:
            drawn.append((d.group(1), int(d.group(2)), d.group(3)))
        j += 1
    if where == "hand":
        g = models[0]
        ok = ("rail_hand_main", 0, g) in drawn and ("hand_main", 1, g) in drawn
        print("%d %s in the hand: its bayonet and the bayonet's mount drawn" % (ok, g))
    elif where in ("holster", "world"):
        g = models[0]
        ok = any(w == "rail_" + where and gun == g for w, _, gun in drawn)
        print("%d %s %s: the bayonet's mount drawn" % (ok, g, "holstered" if where == "holster" else "dropped, lying"))
    elif where == "pickup":
        guns = {gun for w, _, gun in drawn if w == "rail_world"}
        for g in models:
            ok = g in guns or g.replace("/g_", "/v_") in guns
            print("%d %s: the map pickup has the bayonet's mount drawn (as itself or its view model)" % (ok, g))
    i = j

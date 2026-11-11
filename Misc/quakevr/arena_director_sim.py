#!/usr/bin/env python3
"""Offline simulation of the Survival Arena budget director (docs/vr-port/ARENA_DIRECTOR.md) against the current
director (QC/vr_arena.qc VR_ARENA_PlanFoes / PlanTrickle / PlanArmour, ported at Normal skill, impact 1, every game's
data present). A design tool: it reads the draft cost table (Misc/quakevr/arena_director_draft.txt) and prints per-wave
stats for both directors over many seeds.

    python Misc/quakevr/arena_director_sim.py [--seeds 400] [--waves 30] [--skill 1] [--wave-size 1] [--data FILE]
"""
import argparse
import collections
import os
import random
import re

HERE = os.path.dirname(os.path.abspath(__file__))


# ---------------------------------------------------------------------------------------------------------------
# The data file: `<kind> <name> { key "value" ... }`
# ---------------------------------------------------------------------------------------------------------------
def load(path):
    text = re.sub(r"//[^\n]*", "", open(path, encoding="utf-8").read())
    out = collections.defaultdict(dict)
    for kind, name, body in re.findall(r"(\w+)\s+(\w+)\s*\{([^}]*)\}", text):
        out[kind][name] = dict(re.findall(r'(\w+)\s+"([^"]*)"', body))
    return out


def num(d, k, default=0.0):
    return float(d[k]) if k in d else default


def tags(d):
    return set(d.get("tags", "").split())


# ---------------------------------------------------------------------------------------------------------------
# Costs shared by both directors
# ---------------------------------------------------------------------------------------------------------------
class Table:
    def __init__(self, data, skill, wave_size):
        self.d = data
        self.bodies = data["body"]
        self.weapons = data["weapon"]
        self.extras = data["extra"]
        self.skill = skill
        self.factor = [0.75, 1.0, 1.25, 1.5][skill]
        self.adj = [3, 0, -2, -4][skill]
        self.wave_size = wave_size
        self.row = {int(w["row"]): n for n, w in self.weapons.items()}

    def wave_adj(self, w):  # VR_ARENA_WaveAdj
        return w if w <= 1 else max(2, w + self.adj)

    def unlocked(self, item, w):
        f = num(item, "from")
        if f <= 0:
            return False
        u = num(item, "until")
        # (the first rungs, from wave 3 or sooner, are not moved by the difficulty: as VR_ARENA_GunEligible's floor)
        return w >= (f if f <= 3 else self.wave_adj(f)) and (u <= 0 or w <= self.wave_adj(u))

    def armour_best(self, w):
        best = 0
        for i, n in enumerate(("armour_green", "armour_yellow", "armour_red")):
            if self.unlocked(self.extras[n], w):
                best = i + 1
        return best

    def armour_pieces(self, w):
        past = max(0, w - self.wave_adj(num(self.extras["armour_green"], "from")))
        return int(min(10, 2 + past * self.factor // 3))

    def armour_cost(self, t, pieces):
        if t <= 0:
            return 0.0
        e = self.extras[("armour_green", "armour_yellow", "armour_red")[t - 1]]
        return num(e, "cost") + num(e, "plate") * max(0, pieces - 2)

    def foe_cost(self, f):
        c = num(self.bodies[f["body"]], "cost") + (num(self.weapons[f["gun"]], "cost") if f["gun"] else 0)
        if f.get("gun2"):
            c += num(self.weapons[f["gun2"]], "cost") * num(self.extras["second_weapon"], "mult")
        if f.get("boss"):
            c *= num(self.extras["boss"], "mult") * (1 if f["boss"] == 1 else num(self.extras["boss"], "second"))
        c += self.armour_cost(f.get("armour", 0), f.get("pieces", 2))
        if f.get("infected"):
            c += num(self.extras["infected"], "cost")
        return c

    def role(self, cost):
        r = self.d["curve"]["roles"]
        return 0 if cost <= num(r, "fodder_max") else 1 if cost <= num(r, "mid_max") else 2


def foe(body, gun=None, **kw):
    f = {"body": body, "gun": gun}
    f.update(kw)
    return f


# ---------------------------------------------------------------------------------------------------------------
# The current director (vr_arena.qc), Normal-skill structure; its gun schedule's rows: (from, tier, share, peak)
# ---------------------------------------------------------------------------------------------------------------
CUR_ROWS = [(0, 1, .6, 2), (0, 1, .3, 2), (0, 1, .1, 2), (0, 1, .1, 2), (3, 2, .5, 2), (5, 2, .5, 2), (6, 2, .5, 0),
            (7, 2, 1, 2), (8, 2, .6, 0), (9, 1, .3, 0), (10, 3, 1, 2), (11, 2, .6, 0), (11, 2, .5, 0), (12, 3, .5, 2),
            (13, 3, 1, 2), (13, 2, .5, 0), (16, 3, 1, 2), (16, 2, .3, 0), (16, 2, .3, 0), (17, 2, .3, 0),
            (17, 2, .5, 0), (17, 3, .5, 0), (18, 3, .5, 0), (19, 3, 1, 2), (19, 2, .3, 0), (19, 2, .3, 0),
            (20, 2, .3, 0), (20, 3, .5, 0), (21, 3, .5, 0), (23, 3, .2, 0), (24, 3, .3, 0), (24, 3, .5, 0),
            (25, 3, .5, 0), (26, 3, .5, 0)]


class Current:
    def __init__(self, t, rng):
        self.t, self.r = t, rng

    def count(self, n):  # VR_ARENA_WaveCount
        return max(1, int(self.t.factor * int(n * self.t.wave_size + 0.5)))

    def eligible(self, i):
        f = CUR_ROWS[i][0]
        return 3 if f <= 0 else max(3, self.t.wave_adj(f))

    def weight(self, i, w):
        f, _, share, peak = CUR_ROWS[i]
        pool = 3 if f <= 0 else self.eligible(i) + (2 if peak > 0 else 1) * 2
        if w < pool:
            return 0
        a = w - pool
        ramp = 1 if f <= 0 else min(1, .35 + .65 * a / 4)
        return share * ramp * max(.2, 1 - .08 * max(0, a - 6))

    def pick_gun(self, w):
        ws = [self.weight(i, w) for i in range(len(CUR_ROWS))]
        return self.t.row[self.r.choices(range(len(CUR_ROWS)), ws)[0]]

    def early_gun(self, w):
        unarmed = [.3, .2, .15, .1][self.t.skill]
        if w <= self.t.wave_adj(4) and self.r.random() < unarmed:
            return "fists"
        return self.pick_gun(w)

    def tier_gun(self, tier, w):
        rows = [i for i in range(len(CUR_ROWS)) if CUR_ROWS[i][1] == tier and w >= self.eligible(i)]
        return self.t.row[self.r.choices(rows, [CUR_ROWS[i][2] for i in rows])[0]] if rows else "shotgun"

    def classic(self, w, i):
        a, k = self.t.wave_adj, i % 4
        if k == 0:
            return "zombie"
        if k == 1:
            return "dog" if w >= a(6) else "zombie"
        if k == 2:
            return "ogre" if w >= a(8) else "knight" if w >= a(4) else "zombie"
        return "hknight" if w >= a(16) else "fiend" if w >= a(12) else "knight" if w >= a(4) else "zombie"

    def plan(self, w):
        t, r, c, P = self.t, self.r, self.count, []
        boss = w % (5 + (1 if t.skill == 0 else 0)) == 0
        nb = 2 if t.skill == 3 else 1
        if boss:
            bk = "enforcer" if w < t.wave_adj(10) else "ogre" if w < t.wave_adj(16) else "hknight"
            for i in range(nb):
                g = None
                if bk == "enforcer":
                    x = r.random()
                    g = self.tier_gun(3 if w >= t.wave_adj(18) and x < .25 else 2 if w >= t.wave_adj(10) and x < .6
                                      else 1, w)
                P.append(foe(bk, g, boss=1 + i))
        G = lambda n, fn: [P.append(foe("grunt", fn(w))) for _ in range(n)]
        E = lambda n: [P.append(foe("enforcer", self.pick_gun(w))) for _ in range(n)]
        if w == 1:
            G(c(1 + (r.random() < .5)), lambda _: "fists")
        elif w == 2:
            n = c(2)
            P += [foe("grunt", "nail_pistol")] if n < 2 else [foe("grunt", "fists")] + [foe("grunt", "nail_pistol")
                                                                                        for _ in range(n - 1)]
        elif w == 3:
            G(c(3 + (r.random() < .5)), self.early_gun); P.append(foe("zombie"))
        elif w == 4:
            G(c(4), self.early_gun); P.append(foe("knight"))
        elif w == 5:
            G(c(3 + (r.random() < .5)), self.pick_gun)
        elif w in (6, 7):
            G(c(3 + (r.random() < (.5 if w == 6 else .4)) if w == 7 else 3), self.pick_gun); E(1); P.append(foe("dog"))
        elif w == 8:
            G(c(3), self.pick_gun); E(1); P += [foe("knight"), foe("ogre")]
        elif w == 9:
            G(c(4), self.pick_gun); P += [foe("zombie"), foe("ogre")]
        elif w == 10:
            E(c(2)); G(c(2), self.pick_gun); P.append(foe("dog"))
        elif w == 11:
            G(c(4), self.pick_gun); E(1); P += [foe("ogre") for _ in range(c(2))]
        elif w == 12:
            G(c(4), self.pick_gun); E(1); P += [foe("fiend"), foe("knight")]
        else:
            n = c(min(7, max(1, 4 + (w - 13) // 4)))
            if boss:
                n = max(1, n - nb)
            G(n, self.pick_gun)
            E(c(min(3, 1 + (w - 13) // 8)))
            P += [foe(self.classic(w, i)) for i in range(c(min(3, 1 + (w - 13) // 8)))]
        P = P[:40]
        # The trickle: each row due this wave dealt to its carriers among the non-boss humanoids, newest row first.
        free = [f for f in P if f["body"] in ("grunt", "enforcer") and not f.get("boss")]
        r.shuffle(free)
        for i in range(len(CUR_ROWS) - 1, -1, -1):
            if CUR_ROWS[i][0] <= 0 or w < 3:
                continue
            d = w - self.eligible(i)
            n = 0 if d < 0 else 1 if d < 2 else CUR_ROWS[i][3] if d < 4 else 0
            for _ in range(n):
                if free:
                    free.pop()["gun"] = t.row[i]
        # The armour (VR_ARENA_PlanArmour).
        frm = t.wave_adj(4)
        if w >= frm:
            ch = min(.85, (.10 + .03 * (w - frm)) * t.factor)
            best, pcs = t.armour_best(w), t.armour_pieces(w)
            for f in P:
                if f["body"] not in ("grunt", "enforcer"):
                    continue
                if f.get("boss"):
                    f["armour"], f["pieces"] = min(3, best + 1), min(10, pcs + 4)
                elif r.random() < min(.85, ch + (.15 if boss else 0)):
                    f["armour"], f["pieces"] = r.randint(1, best), pcs
        return P, {"theme": "-", "forced": False, "boss": boss}

    def supply(self, w, threat, hp, dom):
        t = self.t
        kits = max(1, min(4, int(2 / t.factor + .5)))
        every = max(1, min(12, int(4 * t.factor + .5)))
        pts = kits * 5 + self.r.randint(1, 2)
        if w % every == 0:
            pts += 20 if (w >= 12 and (w // every) % 2 == 1) else 12
        return pts, {"heal": kits * 5}


# ---------------------------------------------------------------------------------------------------------------
# The budget director
# ---------------------------------------------------------------------------------------------------------------
class Budget:
    def __init__(self, t, rng, reactive=True):
        self.t, self.r, self.reactive = t, rng, reactive
        self.seen = {}           # item -> last wave it appeared in
        self.owned = {"fists"}   # the player's weapons (he takes every gun his foes dropped)
        self.last_new = 0        # the last wave a foe carried a weapon the player did not have
        self.themes = []
        self.carry = 0.0
        self.last_power = -99
        self.bank = 0.0          # the power-ups' share, saved from clear to clear
        c = t.d["curve"]
        self.cv, self.roles, self.nov, self.prog, self.sup = (c["threat"], c["roles"], c["novelty"], c["progress"],
                                                              c["supply"])
        self.split_keys = []
        for part in self.roles["split"].split():
            k, v = part.split(":")
            self.split_keys.append((int(k), [float(x) for x in v.split(",")]))
        self.saw = [float(x) for x in self.cv["saw"].split()]
        self.recent = [float(x) for x in self.nov["recent"].split()]

    # -- curves
    def threat(self, w):
        cv = self.cv
        b = num(cv, "base") + num(cv, "slope") * (w - 1) ** num(cv, "power")
        every = 5 + (1 if self.t.skill == 0 else 0)
        pos = (w - 1) % every
        saw = self.saw[min(len(self.saw) - 1, round(pos * (len(self.saw) - 1) / max(1, every - 1)))]
        return b * saw * self.t.factor * self.t.wave_size

    def split(self, w, theme):
        keys = self.split_keys
        s = keys[-1][1]
        for (w0, a), (w1, b) in zip(keys, keys[1:]):
            if w0 <= w <= w1:
                k = (w - w0) / (w1 - w0)
                s = [x + (y - x) * k for x, y in zip(a, b)]
                break
        if w < keys[0][0]:
            s = keys[0][1]
        if "split" in theme and w >= 4:
            s = [max(0.0, x + float(y)) for x, y in zip(s, theme["split"].split(","))]
        tot = sum(s)
        return [x / tot for x in s]

    def novelty(self, item, w):
        if item not in self.seen:
            return num(self.nov, "cap")
        d = w - self.seen[item]
        if d <= 0:
            return 1.0
        if d <= len(self.recent):
            return self.recent[d - 1]
        return min(num(self.nov, "cap"), 1 + num(self.nov, "bonus") * (d - len(self.recent) - 1))

    def age(self, item, w):
        a = w - self.t.wave_adj(num(item, "from")) - num(self.nov, "fade_after")
        return max(num(self.nov, "floor"), 1 - num(self.nov, "fade") * max(0, a))

    # -- the theme
    def roll_theme(self, w):
        th = self.t.d["theme"]
        if w <= 3:
            return "mixed"
        names, ws = [], []
        for n, d in th.items():
            if not self.t.unlocked(d, w) or (self.themes and self.themes[-1] == n and n != "mixed"):
                continue
            if not all(self.t.unlocked(self.t.bodies[b], w) for b in d.get("needs", "").split()):
                continue
            k = num(d, "weight") * (0.5 if n in self.themes[-3:] and n != "mixed" else 1)
            names.append(n); ws.append(k)
        return self.r.choices(names, ws)[0]

    # -- the combos: (body, weapon) a foe may be, with its cost and its tags
    def combos(self, w):
        t, out = self.t, []
        for bn, b in t.bodies.items():
            if not t.unlocked(b, w):
                continue
            arms = b.get("arms", "none")
            if arms == "none":
                out.append((bn, None, num(b, "cost"), tags(b)))
                continue
            for wn, g in t.weapons.items():
                if not t.unlocked(g, w) or (wn == "fists" and bn != "grunt"):
                    continue
                if arms == "melee" and g.get("family") != "blades":
                    continue
                if arms == "guns" and (g.get("family") == "blades" or wn == "fists"):
                    continue
                out.append((bn, wn, num(b, "cost") + num(g, "cost"), tags(b) | tags(g)))
        return out

    def weight(self, combo, w, theme, fam, spent_items, counts):
        bn, wn, cost, tg = combo
        t = self.t
        b = t.bodies[bn]
        k = num(b, "weight", 1) * self.novelty(bn, w) ** 0.5
        again = float(theme.get("again", self.nov["again"]))
        k *= num(self.nov, "again") ** (0.5 * counts.get(bn, 0))
        if wn:
            g = t.weapons[wn]
            k *= num(g, "weight", 1) * self.novelty(wn, w) * self.age(g, w)
            if fam and g.get("family") == fam:
                k *= 5
                again = 1.0
            k *= again ** counts.get(wn, 0)
        for part in theme.get("bias", "").split():
            tag, m = part.split(":")
            if tag in tg:
                k *= float(m)
        return k

    def fits(self, combo, budget_total, spent_items):
        bn, wn, cost, _ = combo
        t = self.t
        b = t.bodies[bn]
        if spent_items.get(bn, 0) + num(b, "cost") > num(b, "share") * budget_total + 1e-6 and spent_items.get(bn):
            return False
        if wn:
            g = t.weapons[wn]
            if spent_items.get(wn, 0) + num(g, "cost") > num(g, "share") * budget_total + 1e-6 and spent_items.get(wn):
                return False
        return True

    def add(self, P, combo, spent_items, counts, **kw):
        bn, wn, cost, _ = combo
        P.append(foe(bn, wn, **kw))
        spent_items[bn] = spent_items.get(bn, 0) + num(self.t.bodies[bn], "cost")
        counts[bn] = counts.get(bn, 0) + 1
        if wn:
            spent_items[wn] = spent_items.get(wn, 0) + num(self.t.weapons[wn], "cost")
            counts[wn] = counts.get(wn, 0) + 1

    def spend(self, P, budget, band, w, theme, fam, total, spent_items, counts, combos, **kw):
        lo, hi = band
        over = num(self.cv, "overshoot") if kw.pop("last", False) else 0
        while len(P) < self.cap:
            cand = [c for c in combos if lo <= c[2] <= hi and c[2] <= budget + over and self.fits(c, total, spent_items)]
            if not cand:
                # A share cap may be all that stops it: relax it before giving up (a thin wave's backbone).
                cand = [c for c in combos if lo <= c[2] <= hi and c[2] <= budget + over]
                cand = [c for c in cand if c[0] == "grunt"]
                if not cand:
                    break
            hum = [c for c in cand if c[1]]
            if hum and self.r.random() < float(theme.get("humanoid", self.roles["humanoid"])):
                cand = hum
            ws = [self.weight(c, w, theme, fam, spent_items, counts) for c in cand]
            c = self.r.choices(cand, ws)[0]
            self.add(P, c, spent_items, counts, **kw)
            budget -= c[2]
        return budget

    def boss_foes(self, w, d):
        """A boss candidate's foes (one; two at Nightmare), its gun the best tier that keeps it within its share."""
        t = self.t
        out = []
        gun = None
        if d.get("weapon") == "tierpick":
            arms = t.bodies[d["body"]].get("arms")
            gs = [wn for wn, g in t.weapons.items() if t.unlocked(g, w) and wn != "fists"
                  and (arms != "melee" or g.get("family") == "blades")
                  and (arms != "guns" or g.get("family") != "blades")]
            top = max(int(num(t.weapons[g], "tier")) for g in gs)
            gun = self.r.choice([g for g in gs if int(num(t.weapons[g], "tier")) == top])
        for i in range(2 if t.skill == 3 else 1):
            f = foe(d["body"], gun, boss=1 + i)
            if t.bodies[d["body"]].get("arms") != "none" and t.armour_best(w):
                f["armour"] = int(d["armour"]) if "armour" in d else min(3, t.armour_best(w) + 1)
                f["pieces"] = min(10, t.armour_pieces(w) + 4)
            out.append(f)
        return out

    def boss(self, w, P, total):
        t = self.t
        bs = [(n, d) for n, d in t.d["boss"].items() if t.unlocked(d, w)]
        if not bs:
            return 0.0
        cand = [(n, d, self.boss_foes(w, d)) for n, d in bs]
        cand = [(n, d, fs, sum(t.foe_cost(f) for f in fs)) for n, d, fs in cand]
        fit = [c for c in cand if c[3] <= num(self.cv, "boss_share") * total]
        if not fit:
            fit = [min(cand, key=lambda c: c[3])]
        n, d, fs, cost = self.r.choices(fit, [num(c[1], "weight") * self.novelty("boss_" + c[0], w) for c in fit])[0]
        self.seen["boss_" + n] = w
        P.extend(fs)
        return cost

    def progression(self, w, P, budget, spent_items, counts, combos):
        every = num(self.prog, "every")
        if w < self.t.wave_adj(num(self.prog, "from")) or w - self.last_new < every:
            return budget, False
        if any(f["gun"] and f["gun"] not in self.owned for f in P):
            return budget, False
        new = [c for c in combos if c[1] and c[1] not in self.owned and c[0] in ("grunt", "enforcer")]
        if not new:
            # Nothing unlocked is new to him: a gun up to `peek` waves before its own wave, on a grunt.
            t, peek = self.t, num(self.prog, "peek")
            new = [("grunt", wn, num(t.bodies["grunt"], "cost") + num(g, "cost"), tags(t.bodies["grunt"]) | tags(g))
                   for wn, g in t.weapons.items() if wn not in self.owned and num(g, "from") > 0
                   and t.wave_adj(num(g, "from")) <= w + peek]
        if not new:
            return budget, False
        low = min(num(self.t.weapons[c[1]], "tier") for c in new)
        new = [c for c in new if num(self.t.weapons[c[1]], "tier") == low]
        c = min(new, key=lambda c: (c[2], self.r.random()))
        self.add(P, c, spent_items, counts)
        return budget - c[2], True

    def upgrades(self, P, budget, w, total):
        t, r = self.t, self.r
        best = t.armour_best(w)
        hum = [f for f in P if t.bodies[f["body"]].get("arms") != "none" and not f.get("boss")]
        r.shuffle(hum)
        hum.sort(key=lambda f: -t.foe_cost(f))
        spent = {"armour": 0.0, "gun2": 0.0, "infected": 0.0}
        for f in hum:
            opts = []
            if best and not f.get("armour"):
                opts.append(("armour", .75))
            if t.unlocked(t.extras["second_weapon"], w) and f["gun"] and f["gun"] != "fists" and not f.get("gun2"):
                opts.append(("gun2", .15))
            if t.unlocked(t.extras["infected"], w) and not f.get("infected"):
                opts.append(("infected", .10))
            if not opts:
                continue
            kind = r.choices([o[0] for o in opts], [o[1] for o in opts])[0]
            if kind == "armour":
                ty = r.randint(1, best)
                c = t.armour_cost(ty, t.armour_pieces(w))
                if c <= budget and spent["armour"] + c <= num(t.extras["armour_green"], "share") * total:
                    f["armour"], f["pieces"] = ty, t.armour_pieces(w)
                    budget -= c; spent["armour"] += c
            elif kind == "gun2":
                g = t.weapons[f["gun"]]
                c = num(g, "cost") * num(t.extras["second_weapon"], "mult")
                if c <= budget and spent["gun2"] + c <= num(t.extras["second_weapon"], "share") * total:
                    f["gun2"] = f["gun"]
                    budget -= c; spent["gun2"] += c
            else:
                c = num(t.extras["infected"], "cost")
                if c <= budget and spent["infected"] + c <= num(t.extras["infected"], "share") * total:
                    f["infected"] = 1
                    budget -= c; spent["infected"] += c
        return budget

    def plan(self, w):
        t = self.t
        total = self.threat(w) + self.carry
        theme_name = self.roll_theme(w)
        self.themes.append(theme_name)
        theme = t.d["theme"][theme_name]
        combos = self.combos(w)
        fam = None
        if theme.get("family"):
            fams = sorted({t.weapons[c[1]]["family"] for c in combos if c[1] and t.weapons[c[1]]["family"] != "none"})
            fam = self.r.choice(fams)
        P, spent_items, counts = [], {}, {}
        budget = total
        a, b = (float(x) for x in self.cv["count"].split())
        self.cap = min(40, max(1, int((a + b * w) * t.wave_size * t.factor + 0.5)))
        boss = w % (5 + (1 if t.skill == 0 else 0)) == 0
        if boss:
            budget -= self.boss(w, P, total)
        budget, forced = self.progression(w, P, budget, spent_items, counts, combos)
        cad = t.d["curve"].get("cadence", {})
        sv = [float(x) for x in theme.get("surges", cad.get("surges", "1 0.05 3")).split()]
        self.surges = int(min(sv[2], max(0, sv[0] + sv[1] * w))) if w >= t.wave_adj(num(self.cv, "reserve_from")) else 0
        reserve = float(theme.get("reserve", self.cv.get("reserve", 0.2))) * budget if self.surges else 0
        main = max(0.0, budget - reserve)
        s = self.split(w, theme)
        r = self.roles
        fm, mm = num(r, "fodder_max"), num(r, "mid_max")
        bands = [(0, fm), (fm + 1e-6, mm), (mm + 1e-6, 1e9)]
        extras = main * s[3]
        left = 0.0
        for role in (2, 1, 0):
            left = self.spend(P, main * s[role] + left, bands[role], w, theme, fam, total, spent_items, counts, combos)
        left = self.upgrades(P, left + extras, w, total)
        n_main = len(P)
        left = self.spend(P, left, bands[0], w, theme, fam, total, spent_items, counts, combos,
                          last=not self.surges)
        for k in range(1, self.surges + 1):
            left = self.spend(P, reserve / self.surges + left, (0, mm), w, theme, fam, total, spent_items, counts,
                              combos, reinf=k, last=k == self.surges)
        if left > 0:
            left = self.upgrades(P, left, w, total)  # (a full wave: its budget left turned into upgrades)
        self.carry = min(left, num(self.cv, "carry_max") * self.threat(w + 1))  # (negative: the overshoot's debt)
        for f in P:
            self.seen[f["body"]] = w
            if f["gun"]:
                self.seen[f["gun"]] = w
                if f["gun"] not in self.owned:
                    self.last_new = w
                self.owned.add(f["gun"])
        return P, {"theme": theme_name, "forced": forced, "boss": boss, "reinf": len(P) - n_main, "fam": fam}

    def supply(self, w, threat, hp, dom):
        t, s, r = self.t, self.sup, self.r
        pts = min(num(s, "max"), (num(s, "base") + num(s, "per_threat") * threat) / t.factor)
        if not (self.reactive and num(s, "reactive")):
            hp, dom = 0.5, 0.5
        sh = {"heal": num(s, "heal") + 0.5 * (0.5 - hp) - 0.2 * (dom - 0.5),
              "armour": num(s, "armour"), "ammo": num(s, "ammo"),
              "power": num(s, "power") + 0.25 * max(0, dom - 0.6)}
        sh = {k: max(0.0, v) for k, v in sh.items()}
        tot = sum(sh.values())
        items = t.d["supply"]
        got = collections.Counter()
        spent = collections.Counter()
        left = 0.0
        for kind in ("power", "armour", "heal", "ammo"):
            b = pts * sh[kind] / tot + left
            if kind == "power":
                b, left = b + self.bank, 0.0
                self.bank = 0.0
            cand = sorted([(n, d) for n, d in items.items() if d["kind"] == kind and t.unlocked(d, w)],
                          key=lambda x: -num(x[1], "cost"))
            if kind == "power" and w - self.last_power < 3:
                cand = []
            for n, d in cand:
                while num(d, "cost") <= b and got[n] < num(d, "max", 99):
                    if kind == "heal" and n == "megahealth" and hp > 0.35:
                        break
                    got[n] += 1; b -= num(d, "cost"); spent[kind] += num(d, "cost")
                    if kind == "power":
                        self.last_power = w
                        break
                if kind == "power" and got[n]:
                    break
            if kind == "power" and not sum(got[n] for n, d in items.items() if d["kind"] == "power"):
                self.bank, b = b, 0.0  # (saved for a later clear)
            left = b
        shards = int(left)
        got["shard"] += shards
        spent["armour"] += shards
        return pts, spent


# ---------------------------------------------------------------------------------------------------------------
# The run
# ---------------------------------------------------------------------------------------------------------------
def run(director_cls, t, seeds, waves, **kw):
    per = [collections.defaultdict(list) for _ in range(waves + 1)]
    sigs = [collections.Counter() for _ in range(waves + 1)]
    windows = collections.defaultdict(list)
    item_waves = collections.Counter()
    theme_n = collections.Counter()
    for seed in range(seeds):
        rng = random.Random(seed * 7919 + 1)
        d = director_cls(t, rng, **kw)
        prev = set()
        owned = {"fists"}
        last_new = 0
        for w in range(1, waves + 1):
            P, info = d.plan(w)
            cost = [t.foe_cost(f) for f in P]
            T = sum(cost)
            combos = {(f["body"], f["gun"]) for f in P}
            m = per[w]
            m["threat"].append(T)
            m["foes"].append(len(P))
            roles = [0.0, 0.0, 0.0]
            for f, c in zip(P, cost):
                roles[2 if f.get("boss") else t.role(c)] += c
            for i in range(3):
                m["role%d" % i].append(roles[i] / max(1, T))
            m["bodies"].append(len({f["body"] for f in P}))
            m["guns"].append(len({f["gun"] for f in P if f["gun"]}))
            m["combos"].append(len(combos))
            m["repeat"].append(len(combos & prev) / max(1, len(combos)) if w > 1 else 0)
            m["armoured"].append(sum(1 for f in P if f.get("armour")) / max(1, len(P)))
            m["human"].append(sum(1 for f in P if t.bodies[f["body"]].get("arms") != "none") / max(1, len(P)))
            m["extras"].append(sum(1 for f in P if f.get("gun2") or f.get("infected")))
            m["forced"].append(1 if info.get("forced") else 0)
            m["reinf"].append(info.get("reinf", 0))
            newgun = any(f["gun"] and f["gun"] not in owned for f in P)
            if newgun:
                last_new = w
            m["drought"].append(1 if w >= 3 and w - last_new >= 3 else 0)
            for f in P:
                if f["gun"]:
                    owned.add(f["gun"])
            prev = combos
            sigs[w][tuple(sorted((f["body"], f["gun"] or "") for f in P))] += 1
            windows[((w - 1) // 5, "c", seed)] = windows.get(((w - 1) // 5, "c", seed), set()) | combos
            windows[((w - 1) // 5, "g", seed)] = windows.get(((w - 1) // 5, "g", seed), set()) | {
                f["gun"] for f in P if f["gun"]}
            windows[((w - 1) // 5, "b", seed)] = windows.get(((w - 1) // 5, "b", seed), set()) | {
                f["body"] for f in P}
            theme_n[info["theme"]] += 1
            for it in {f["body"] for f in P} | {f["gun"] for f in P if f["gun"]}:
                item_waves[it] += 1
            hp, dom = rng.uniform(0.1, 1.0), rng.random()
            pts, spent = d.supply(w, T, hp, dom)
            m["supply"].append(pts)
            m["sheal"].append(spent.get("heal", 0) / max(1, pts))
            m["spower"].append(spent.get("power", 0) / max(1, pts))
    return per, sigs, windows, item_waves, theme_n


def mean(xs):
    return sum(xs) / max(1, len(xs))



# ---------------------------------------------------------------------------------------------------------------
# The cadence: the alive count over a wave, for three player models (rough: one foe at a time, each taking the time
# to find it and the time to kill it, its cost / his damage rate). Classic: its burst and alive pool; budget: its
# spawn groups, gaps stretched by the threat up, surges.
# ---------------------------------------------------------------------------------------------------------------
PLAYERS = {"fast": (1.0, 4.0), "average": (2.0, 2.5), "slow": (3.5, 1.5)}  # (s to find a foe, threat points / s)
CAP = 20       # vr_arena_alive_cap
DT = 0.1


def timeline(t, P, info, player, rng, budget, cad_theme):
    find, dps = PLAYERS[player]
    queue = list(P)
    alive = []           # [cost, work left]
    time_, last, gap, warn_at = 0.0, -1e9, 0.0, None
    peak, series, groups, surges = 0, [], [], []
    wt = max(1.0, sum(t.foe_cost(f) for f in P))
    pool = min(6 + (info["w"] - 1) // 4, 8) * t.factor if not budget else CAP
    burst = 3
    work = None
    while (queue or alive) and time_ < 600:
        up = sum(a[0] for a in alive)
        if budget:
            cad = cad_theme
            if queue and warn_at is None and time_ >= last + gap * (1 + cad["stretch"] * up / wt) * (2 if up / wt > cad["soft"] else 1):
                f = queue[0]
                if f.get("boss") or f.get("reinf"):
                    warn_at = time_ + (3 if f.get("boss") else cad["warn"])
                else:
                    g = rng.randint(int(cad["gmin"]), int(cad["gmax"]))
                    n, gc = 0, 0.0
                    while queue and n < g and len(alive) < CAP and not queue[0].get("boss") and not queue[0].get("reinf"):
                        c = t.foe_cost(queue.pop(0)); alive.append([c, find + c / dps]); n += 1; gc += c
                    last, gap = time_, cad["gap"] / t.factor * rng.uniform(0.8, 1.2) * max(1.0, gc / cad["ref"])
                    groups.append(n)
            elif warn_at is not None and time_ >= warn_at:
                k = queue[0].get("reinf", 0) if not queue[0].get("boss") else -1
                n, gc = 0, 0.0
                while queue and len(alive) < CAP and ((k == -1 and queue[0].get("boss")) or (k > 0 and queue[0].get("reinf") == k)):
                    c = t.foe_cost(queue.pop(0)); alive.append([c, find + c / dps]); n += 1; gc += c
                if k > 0:
                    surges.append(n)
                warn_at, last = None, time_
                gap = cad["gap"] / t.factor * rng.uniform(0.8, 1.2) * max(1.0, gc / cad["ref"])
        else:
            while queue and burst > 0 and len(alive) < pool:
                c = t.foe_cost(queue.pop(0)); alive.append([c, find + c / dps]); burst -= 1; last = time_
            if burst <= 0 and queue and len(alive) < pool:
                if gap <= 0:
                    gap = time_ + 1.5 / t.factor * (1 + rng.random())
                elif time_ >= gap:
                    c = t.foe_cost(queue.pop(0)); alive.append([c, find + c / dps]); gap = 0
        if alive:  # (he works on the oldest)
            alive[0][1] -= DT
            if alive[0][1] <= 0:
                alive.pop(0)
        peak = max(peak, len(alive))
        if int(time_ / DT) % 50 == 0:
            series.append(len(alive))
        time_ += DT
    return peak, time_, series, groups, surges


def run_cadence(t, seeds, waves):
    cad = t.d["curve"]["cadence"]
    print("cadence (wave size %g, skill %d): per player model, CURRENT / BUDGET" % (t.wave_size, t.skill))
    for player in PLAYERS:
        rows = collections.defaultdict(list)
        theme_peak = collections.defaultdict(list)
        theme_surge = collections.defaultdict(list)
        theme_group = collections.defaultdict(list)
        sample = {}
        for seed in range(seeds):
            for cls, budget in ((Current, False), (Budget, True)):
                rng = random.Random(seed * 7919 + 1)
                d = cls(t, rng) if cls is Current else cls(t, rng, reactive=True)
                for w in range(1, waves + 1):
                    P, info = d.plan(w)
                    info["w"] = w
                    th = t.d["theme"].get(info["theme"], {})
                    g = th.get("group", cad.get("group", "1 2")).split()
                    ct = {"gap": float(th.get("gap", cad["gap"])), "gmin": float(g[0]), "gmax": float(g[-1]),
                          "stretch": float(th.get("stretch", cad["stretch"])), "soft": float(th.get("soft", cad["soft"])),
                          "warn": float(cad["surge_warn"]), "ref": float(th.get("gap_ref", cad.get("gap_ref", 12)))}
                    peak, dur, series, groups, surges = timeline(t, P, info, player, random.Random(seed * 31 + w), budget, ct)
                    rows[(budget, w)].append((peak, dur, len(P), sum(series) / max(1, len(series))))
                    if budget:
                        theme_peak[info["theme"]].append(peak)
                        theme_surge[info["theme"]] += surges
                        theme_group[info["theme"]] += groups
                    if seed == 0 and w == 15:
                        sample[budget] = series
        print("  %s player (finds a foe in %.1f s, %.1f threat/s):" % (player, *PLAYERS[player]))
        for w in (3, 6, 10, 15, 20, 25, 30):
            if w > waves:
                continue
            c = rows[(False, w)]
            n = rows[(True, w)]
            m = lambda r, i: sum(x[i] for x in r) / len(r)
            print("    wave %2d: peak alive %4.1f / %4.1f, mean alive %4.1f / %4.1f, wave %4.0f / %4.0f s" % (
                w, m(c, 0), m(n, 0), m(c, 3), m(n, 3), m(c, 1), m(n, 1)))
        print("    by theme (budget): " + "; ".join(
            "%s peak %.1f, groups %.1f, surges %.1f" % (k, sum(v) / len(v), sum(theme_group[k]) / max(1, len(theme_group[k])),
                                                      sum(theme_surge[k]) / max(1, len(theme_surge[k])))
            for k, v in sorted(theme_peak.items())))
        print("    wave 15 seed 0, alive every 5 s: current %s | budget %s" % (sample.get(False), sample.get(True)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=400)
    ap.add_argument("--waves", type=int, default=30)
    ap.add_argument("--skill", type=int, default=1)
    ap.add_argument("--wave-size", type=float, default=1.0)
    ap.add_argument("--no-reactive", action="store_true")
    ap.add_argument("--data", default=os.path.join(HERE, "..", "..", "quakevr", "data", "arena_director.txt"))
    ap.add_argument("--cadence", action="store_true", help="the alive count over waves for three player models")
    a = ap.parse_args()
    t = Table(load(a.data), a.skill, a.wave_size)
    if a.cadence:
        run_cadence(t, a.seeds, a.waves)
        return
    W = a.waves
    cur = run(Current, t, a.seeds, W)
    new = run(Budget, t, a.seeds, W, reactive=not a.no_reactive)
    print("skill %d, wave_size %g, %d seeds. Each cell: CURRENT / BUDGET" % (a.skill, a.wave_size, a.seeds))
    print("wave | threat      | foes      | fod/mid/hvy %% (budget) | bodies  | guns    | combos  | repeat%%  "
          "| armd%% | hum%%  | unique plans%% | supply   | theme")
    for w in range(1, W + 1):
        c, n = cur[0][w], new[0][w]
        f = lambda k, m=1: "%5.1f/%5.1f" % (mean(c[k]) * m, mean(n[k]) * m)
        g = lambda k, m=1: "%4.1f/%4.1f" % (mean(c[k]) * m, mean(n[k]) * m)
        roles = "%3.0f/%3.0f/%3.0f" % tuple(100 * mean(n["role%d" % i]) for i in range(3))
        uniq = "%3.0f/%3.0f" % (100 * len(cur[1][w]) / a.seeds, 100 * len(new[1][w]) / a.seeds)
        print("%4d | %s | %s | %22s | %s | %s | %s | %s | %s | %s | %s | %s | %s" % (
            w, f("threat"), g("foes"), roles, g("bodies"), g("guns"), g("combos"), "%3.0f/%3.0f" % (
                100 * mean(c["repeat"]), 100 * mean(n["repeat"])), "%2.0f/%2.0f" % (
                100 * mean(c["armoured"]), 100 * mean(n["armoured"])), "%3.0f/%3.0f" % (
                100 * mean(c["human"]), 100 * mean(n["human"])), uniq, g("supply"),
            "boss" if w % 5 == 0 else ""))
    print("\n5-wave windows: distinct combos / guns / bodies per window (mean over seeds), CURRENT / BUDGET")
    for k in range((W + 4) // 5):
        vals = []
        for kind in ("c", "g", "b"):
            cv = mean([len(cur[2][(k, kind, s)]) for s in range(a.seeds)])
            nv = mean([len(new[2][(k, kind, s)]) for s in range(a.seeds)])
            vals.append("%5.1f/%5.1f" % (cv, nv))
        print("  waves %2d-%2d: combos %s  guns %s  bodies %s" % (k * 5 + 1, k * 5 + 5, *vals))
    tot = lambda r, k: sum(sum(r[0][w][k]) for w in range(1, W + 1)) / a.seeds
    print("\nper run: progression forced (budget) %.2f; drought waves (3+ waves without a new gun) cur %.2f budget %.2f;"
          " reinforcement foes (budget) %.1f; extras (2nd gun/infected) %.1f" % (
              tot(new, "forced"), tot(cur, "drought"), tot(new, "drought"), tot(new, "reinf"), tot(new, "extras")))
    print("mean over waves 1-%d: combos/wave cur %.2f budget %.2f; repeat cur %.0f%% budget %.0f%%; supply heal "
          "share %.0f%% power share %.1f%%" % (
              W, mean([mean(cur[0][w]["combos"]) for w in range(1, W + 1)]),
              mean([mean(new[0][w]["combos"]) for w in range(1, W + 1)]),
              100 * mean([mean(cur[0][w]["repeat"]) for w in range(2, W + 1)]),
              100 * mean([mean(new[0][w]["repeat"]) for w in range(2, W + 1)]),
              100 * mean([mean(new[0][w]["sheal"]) for w in range(1, W + 1)]),
              100 * mean([mean(new[0][w]["spower"]) for w in range(1, W + 1)])))
    print("per run (waves 1-%d): threat cur %.0f budget %.0f; foes cur %.0f budget %.0f; supply points cur %.0f budget %.0f"
          % (W, tot(cur, "threat"), tot(new, "threat"), tot(cur, "foes"), tot(new, "foes"), tot(cur, "supply"),
             tot(new, "supply")))
    print("themes (share of waves):", ", ".join("%s %.0f%%" % (k, 100 * v / (a.seeds * W))
                                                for k, v in new[4].most_common()))
    for name, r in (("CURRENT", cur), ("BUDGET", new)):
        items = r[3]
        print("%s items, share of all waves they appear in:" % name,
              ", ".join("%s %.0f%%" % (k, 100 * v / (a.seeds * W)) for k, v in items.most_common()))


if __name__ == "__main__":
    main()

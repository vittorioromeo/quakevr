#!/usr/bin/env python3
"""Imports Arcane Dimensions' QuakeC into Quake VR's progs, namespaced (docs/vr-port/ARCANE_DIMENSIONS.md, 5.2).

    python Misc/quakevr/ad_import.py [--release DIR]

DIR holds the unpacked official release ad_v1_80p1final.zip (pak0.pak, pak1.pak, pak2.pak, ad_1_8.fgd); default
scratch/ad/ad. The QC is read from the paks (my_progs/, patch 1's pak2 first), which are checked against the pinned
SHA-256 below, so the output is reproducible. AD's QC is GPL-2.0-or-later (its readme): the generated files keep
AD's headers and credits; AD's assets are never copied.

    python Misc/quakevr/ad_import.py --verify-frames   (after a build: AD's frame numbers vs AD's own progs.dat)

What it writes (QC/ad/, regenerated whole; never edit by hand, change this script):
  ad_builtins.qc   the extension builtins AD declares that Quake VR's QC does not: "#0", bound by name by the
                   engine when it has them (pr_edict.c PR_InitBuiltins), else an error only if called; standard
                   builtins AD declares under its own names or signatures keep their numbers (ad_strcat = #115)
  ad_<file>.qc     each AD source file, every identifier AD defines renamed with the prefix "ad_"
  AD_IMPORT.txt    the source hashes, what was kept, renamed, dropped (read it when re-importing)
and the block between the "ad_import.py" markers at the end of QC/progs.src: one entry per file, in AD's order
(fteqcc numbers $frame macros per progs.src entry; an #include would shift AD's frames).

Renaming: every identifier token outside strings, comments and $frame directives becomes ad_<name>, except
  - QC keywords and types, compiler intrinsics (__*), vector components (x, y, z);
  - the engine's system globals and fields (Quake VR's defs.qc before end_sys_fields) and their _x/_y/_z parts;
  - builtins Quake VR's QC already declares (AD's own declarations of them are dropped);
  - AD fields that are map keys (AD's FGD or AD's maps use them) and that Quake VR already has with the same type
    (target, wait, delay, alpha...): one field, as the map sets it.
So AD's spawn functions are ad_<classname> too: nothing of AD spawns, runs or is called by the engine until the
bridge (QC/vr_ad_bridge.qc) or the engine's spawn hook asks for it. AD's own system section, its builtin
declarations and its extension headers (dpextensions.qc: only what AD's code uses is kept) are not copied; the
CSQC HUD (csqc_*) is left out.
"""
import argparse
import hashlib
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
QC_DIR = os.path.join(ROOT, "QC")
OUT_DIR = os.path.join(QC_DIR, "ad")

# ad_v1_80p1final.zip (SHA-256 7ad993da6c760c446ca31fad71e9f5b6c9eee99b6354f161aa746b572fe70a7d), unpacked.
PINNED = {
    "pak0.pak": "117caa759bf9f161599ff693f561c68954519eda9cefb719a97cd5c1cd863ef3",
    "pak2.pak": "4c82e0347952b38b0265fd34e0c3c3a6b6b4bea17642d18bf4189a97d93dd0f1",
    "ad_1_8.fgd": "2fdaef42f02c37370b36bd973e9ebfe08827f5847e26b4c250dd9b521a6b8431",
}
# Files of AD's progs.src not imported: the CSQC HUD (Quake VR draws its own) and extension headers whose needed
# parts are filtered in (HEADERS) rather than copied whole.
SKIP = {"csqc_hudvanilla.qc"}
# Declarations only: copied statement by statement, only what the imported code uses (transitively).
HEADERS = {"defs.qc", "dpextensions.qc", "csqc_defs.qc", "csqc_hudad.qc"}
PREFIX = "ad_"
BRIDGE = "VR_AD_"  # the bridge's functions the patches below call (QC/vr_ad_bridge.qc): never renamed
# Reviewed edits to AD's source, applied before renaming: (file, exact text, replacement, why). Each must match once.
#
# The precedence patches (Misc/quakevr/check_qc_precedence.py): expressions that read differently under C priorities,
# parenthesised as fteqcc's QC priorities (AD's own compile, and ours) read them, so behaviour stays AD's as shipped
# (some look like AD bugs: kept, flagged "AD as shipped"). Each was verified to compile to the same instructions.
P = "parenthesised as QC priorities read it (check_qc_precedence)"
B = "bridge"
PATCHES = [
    ("client_debuff.qc", "if (targ.flags & FL_CLIENT == FALSE) return;", "if ((targ.flags & FL_CLIENT) == FALSE) return;", 3, P),
    ("client_debuff.qc", "if (self.items & ALL_ITARTIFACTS == 0) {", "if ((self.items & ALL_ITARTIFACTS) == 0) {", 1, P),
    ("client_debuff.qc", "if (!self.bleeding && !self.poisoned) {", "if (!(self.bleeding && !self.poisoned)) {", 1,
     P + "; AD as shipped"),
    ("misc_textbook.qc", "if (!self.enemy.cshift_upd && self.enemy.items & ALL_ITARTIFACTS == 0) {",
     "if (!(self.enemy.cshift_upd && (self.enemy.items & ALL_ITARTIFACTS) == 0)) {", 2, P + "; AD as shipped"),
    ("mon_bossmammoth.qc", "if (self.walkframe >= 4  && self.walkframe <= 8 ||\n\t\t\tself.walkframe >= 11 && self.walkframe <= 15)",
     "if (((self.walkframe >= 4  && self.walkframe <= 8) ||\n\t\t\tself.walkframe >= 11) && self.walkframe <= 15)", 1,
     P + "; AD as shipped"),
    ("monsters.qc", "if (self.deadflag == DEAD_REMOVE || self.deadflag == DEAD_EXPLODE\n",
     "if ((self.deadflag == DEAD_REMOVE || self.deadflag == DEAD_EXPLODE)\n", 1, P),
    ("monsters.qc", "if (self.spawnflags & MON_ONFLOOR && self.classgroup == CG_ZOMBIE) {",
     "if ((self.spawnflags & MON_ONFLOOR) && self.classgroup == CG_ZOMBIE) {", 1, P),
    ("monsters.qc", "if (self.classtype == CT_MONGARGOYLE && self.spawnflags & MON_GARGOYLE_PERCH ||\n"
     "\t\tself.classtype == CT_MONGAUNT && self.spawnflags & MON_GAUNT_PERCH ) \n",
     "if (((self.classtype == CT_MONGARGOYLE && (self.spawnflags & MON_GARGOYLE_PERCH)) ||\n"
     "\t\tself.classtype == CT_MONGAUNT) && (self.spawnflags & MON_GAUNT_PERCH) ) \n", 1, P + "; AD as shipped"),
    ("monsters.qc", "if (self.classtype == CT_MONSPIDER && self.spawnflags & MON_SPIDER_CEILING ||\n"
     "\t\tself.classtype == CT_MONSWAMPLING && self.spawnflags & MON_SWAMPLING_CEILING ||\n"
     "\t\tself.classtype == CT_MONVORELING && self.spawnflags & MON_VORELING_CEILING) {",
     "if (((((self.classtype == CT_MONSPIDER && (self.spawnflags & MON_SPIDER_CEILING)) ||\n"
     "\t\tself.classtype == CT_MONSWAMPLING) && (self.spawnflags & MON_SWAMPLING_CEILING)) ||\n"
     "\t\tself.classtype == CT_MONVORELING) && (self.spawnflags & MON_VORELING_CEILING)) {", 2, P + "; AD as shipped"),
    ("triggers.qc", "if (query_configflag(SVR_RUNE_ALL) & self.customkey == self.customkey) {",
     "if ((query_configflag(SVR_RUNE_ALL) & self.customkey) == self.customkey) {", 1, P),
    ("misc.qc", "self.health = self.gibhealth = -1;", "self.gibhealth = -1; self.health = -1;", 1, P),
    ("misc.qc", "if (self.count == TE_LIGHTNING1 || self.count == TE_LIGHTNING2 || \n\t\tself.count == TE_LIGHTNING3 && !self.movetarget) {",
     "if ((self.count == TE_LIGHTNING1 || self.count == TE_LIGHTNING2 || \n\t\tself.count == TE_LIGHTNING3) && !self.movetarget) {",
     1, P + "; AD as shipped"),
] + [
    # The bridge's hooks (P1b): AD's code calls Quake VR's where the two meet. The VR_AD_ functions are QC/vr_ad_bridge.qc's
    # (the renaming keeps every VR_AD_ name). Keep this list short: everything else stays AD's as shipped.
    ("world.qc", "ext_active = cvar(\"pr_checkextension\");\t// Check for extra extensions",
     "ext_active = VR_AD_Extensions();\t// Check for extra extensions", 1,
     B + ": AD's Fitz path (sprite particles, no CSQC stats); our engine's extensions are not AD's"),
    ("world.qc", "cvar_set (\"r_wateralpha\", ftos(liquid_alpha));", "VR_AD_WaterAlpha(liquid_alpha);", 1,
     B + ": never the player's r_wateralpha; the engine reads the water_alpha key (gl_rmisc.c)"),
    ("globalfog.qc", "\tstuffcmd(client, \"\\n\\n\\nfog \");",
     "\tif (VR_AD_Fog(client, fog_control.fog_currden, fog_control.fog_currcol)) return;\n\tstuffcmd(client, \"\\n\\n\\nfog \");",
     1, B + ": fog by svc_fog (stuffed commands are filtered), as MG's"),
    ("ai_combat.qc", "\tlocal float save, take, flinch, noarmordmg;\n",
     "\tlocal float save, take, flinch, noarmordmg;\n\tif (VR_AD_DamageOurs(targ, inflictor, attacker, damage, checkarmor)) return;\n",
     1, B + ": Quake VR's own entities (the player, our monsters) take damage by our T_Damage"),
    ("ai_combat.qc", "\ttarg.health = targ.health - take;\n",
     "\ttarg.health = targ.health - take;\n\tVR_AD_Hit(targ, inflictor, attacker, take);\n",
     1, B + ": our hit hooks on AD's monsters (damage numbers, gore, small gibs, wounds)"),
    ("ai_pathcorner.qc", "void() path_corner_touch =\n{\n",
     "void() path_corner_touch =\n{\n\tif (VR_AD_PathCornerTouch()) return;\n",
     1, B + ": our monsters follow AD's path_corners as ours (t_movetarget)"),
    ("items.qc", "void() weapon_touch =\n{\n\tlocal float pickupweapon, pickupmodweapon, pickupammo, moditem;\n",
     "void() weapon_touch =\n{\n\tlocal float pickupweapon, pickupmodweapon, pickupammo, moditem;\n\tif (VR_AD_WeaponTouch()) return;\n",
     1, B + ": never AD's weapon pickup on a Quake VR player (it sets .weapon, our hands' weapon id); ours are Quake VR's"),
    ("ai_projectiles.qc", "\t\telse self.dmg = DAMAGE_PSHELL;",
     "\t\telse self.dmg = DAMAGE_PSHELL * VR_AD_PelletDamage(self);",
     1, B + ": the pellet damage settings (vr_ad_pellet_damage, Weapon Damage)"),
]
# The monsters' hooks (P2, ARCANE_DIMENSIONS.md section 16): AD's monsters get Quake VR's treatment (QC/vr_ad_monsters.qc;
# their declarations in QC/vr_ad_mondefs.qc). Applied after the bridge's (the T_Damage line is the bridge's patched one).
MON_PATCHES = [
    ("monsters.qc", "void() monster_start =\n{\n", "void() monster_start =\n{\n\tVR_AD_MonsterSetup();\n",
     1, B + ": the monster's Quake VR data by its class (head zone, knockdown, corpse, parries)"),
    ("ai_combat.qc", "\ttarg.health = targ.health - take;\n",
     "\tVR_AD_KdSave(targ);\n\ttarg.health = targ.health - take;\n",
     1, B + ": a knocked-down monster's frame and think kept from AD's reaction (put back by VR_AD_PainDown)"),
    ("ai_combat.qc", "\t//----------------------------------------------------------------------\n"
     "\t// Check for ammo type resistance and immunity to pain\n",
     "\tif (VR_AD_PainDown(targ)) return;\n\t//----------------------------------------------------------------------\n"
     "\t// Check for ammo type resistance and immunity to pain\n",
     1, B + ": knocked down: no pain animation (its ragdoll), longer down"),
    ("ai_combat.qc", "\t\t\tif (self.flags & FL_MONSTER) monster_death_use();\n\t\t\tself.th_die ();\n",
     "\t\t\tif (self.flags & FL_MONSTER) monster_death_use();\n\t\t\tVR_AD_Dying(attacker);\n\t\t\tself.th_die ();\n"
     "\t\t\tVR_AD_Died();\n",
     1, B + ": Quake VR's dying around AD's death function (beheading, limbs, knockdown's death, corpse, gore)"),
    ("monsters.qc", "\tself.solid = SOLID_TRIGGER;\n\tself.touch = SUB_Null;\n",
     "\tself.solid = SOLID_TRIGGER;\n\tVR_AD_BodySolid();\n\tself.touch = SUB_Null;\n",
     1, B + ": the body touchable, not a trigger (its ragdoll and corpse are Quake VR's)"),
]
PATCHES = PATCHES + MON_PATCHES
# (Mechanical, not patches: a global array's length named by a constant, `float a[MAX];`, is written as the number,
# as our fteqcc flags (autoproto) take no named constant there.)

KEYWORDS = set("""void float vector entity string int integer local const var nosave shared noref static inline
if else while do for return break continue switch case default goto typedef enum enumflags struct union class
virtual nonvirtual optional autocvar x y z""".split())
DIRECTIVES = ("frame", "cd", "origin", "base", "skin", "flags", "scale", "modelname", "framesave", "framerestore",
              "framevalue", "spritename", "type", "load", "flush")

TOKEN = re.compile(r"""
 (?P<nl>[ \t\r]*\n)
|(?P<ws>[ \t\r]+)
|(?P<lc>//[^\n]*)
|(?P<bc>/\*.*?(?:\*/|\Z))
|(?P<str>"(?:\\.|[^"\\\n])*"?)
|(?P<vec>'[^'\n]*'?)
|(?P<fm>\$[A-Za-z_0-9]+)
|(?P<num>0[xX][0-9a-fA-F]+|\d+\.?\d*(?:[eE][-+]?\d+)?|\.\d+)
|(?P<id>[A-Za-z_][A-Za-z_0-9]*)
|(?P<op>.)
""", re.S | re.X)


def tokenize(text):
    """[(kind, text)]; a $directive line and a #preprocessor line are one 'raw' token."""
    out = []
    i, n = 0, len(text)
    line_start = True
    while i < n:
        if line_start:
            m = re.compile(r"[ \t]*(\$(?:%s)\b|#)[^\n]*" % "|".join(DIRECTIVES)).match(text, i)
            if m:
                out.append(("raw", m.group(0)))
                i = m.end()
                line_start = False
                continue
        m = TOKEN.match(text, i)
        kind = m.lastgroup
        out.append((kind, m.group(0)))
        i = m.end()
        line_start = kind == "nl" or (kind == "ws" and line_start) or (kind == "bc" and line_start and "\n" in m.group(0))
    return out


def code(tokens):
    """Indices of the tokens that matter to the parser (no whitespace, comments, raw lines)."""
    return [k for k, (kind, _) in enumerate(tokens) if kind not in ("nl", "ws", "lc", "bc", "raw")]


def statements(tokens):
    """Top-level statements: lists of code-token indices (a function body ends its statement)."""
    idx = code(tokens)
    out, cur, depth, body = [], [], 0, False
    for k in idx:
        t = tokens[k][1]
        cur.append(k)
        if t in "([{":
            depth += 1
            body = body or (t == "{" and depth == 1)
        elif t in ")]}":
            depth -= 1
            if t == "}" and depth == 0 and body:
                out.append(cur)
                cur, body = [], False
                continue
        elif t == ";" and depth == 0:
            out.append(cur)
            cur, body = [], False
    if cur:
        out.append(cur)
    return [s for s in out if not (len(s) == 1 and tokens[s[0]][1] == ";")]


MODIFIERS = {"local", "const", "var", "nosave", "shared", "noref", "static", "inline"}
TYPES = {"void", "float", "vector", "entity", "string", "int", "integer", "__variant"}


def declared(tokens, stmt):
    """(names, is_field, type text, is_builtin) of one top-level statement."""
    toks = [tokens[k][1] for k in stmt]
    while toks and toks[0] in MODIFIERS:
        toks = toks[1:]
    is_field = bool(toks) and toks[0] == "."
    is_builtin = any(toks[j] == "=" and j + 1 < len(toks) and toks[j + 1] == "#" for j in range(len(toks)))
    names, segs, seg, depth = [], [], [], 0
    for t in toks:
        if t in "([{":
            depth += 1
        elif t in ")]}":
            depth -= 1
        if t == "," and depth == 0:
            segs.append(seg)
            seg = []
        else:
            seg.append((t, depth))
    segs.append(seg)
    typ = None
    for s in segs:
        ids = []
        for t, d in s:
            if t == "=" and d == 0:
                break
            if d == 0 and re.match(r"[A-Za-z_]\w*$", t) and t not in TYPES and t not in MODIFIERS:
                ids.append(t)
        if ids:
            names.append(ids[-1])
            if typ is None:
                cut = [t for t, d in s]
                typ = "".join(cut[:cut.index(ids[-1])]).lstrip(".")
    return names, is_field, typ or "", is_builtin


def ids_in(tokens, stmt):
    """Identifiers a statement uses; a vector's component (v_x) uses the vector (v)."""
    out = set()
    for k in stmt:
        if tokens[k][0] == "id":
            t = tokens[k][1]
            out.add(t)
            m = re.match(r"(\w+)_[xyz]$", t)
            if m:
                out.add(m.group(1))
    return out


def read_pak(path):
    with open(path, "rb") as f:
        hdr = f.read(12)
        off, ln = struct.unpack("<ii", hdr[4:])
        f.seek(off)
        dirs = [f.read(64) for _ in range(ln // 64)]
        out = {}
        for e in dirs:
            name = e[:56].split(b"\0")[0].decode("latin1")
            o, s = struct.unpack("<ii", e[56:])
            out[name] = (o, s)
        return out


def pak_files(path, want):
    files = {}
    with open(path, "rb") as f:
        for name, (o, s) in read_pak(path).items():
            if want(name):
                f.seek(o)
                files[name] = f.read(s)
    return files


ENGINE_DATA = os.path.join(ROOT, "Quake", "vr", "vr_ad_data.inc")


def engine_lists(rel, official):
    """Writes Quake/vr/vr_ad_data.inc (vr_ad.cpp): the release's pin the engine checks (pak2.pak's size and the SHA-256
    of its header and directory: cheap at every start, and only patch 1 has it) and the files of AD's paks that an
    official pak (id1, hipnotic, rogue: their names only, read from `official`) also has. The engine skips those in AD's
    folder, so AD supplies only its own files (ARCANE_DIMENSIONS.md 5.5); maps/start.bsp excepted (AD's hub)."""
    with open(os.path.join(rel, "pak2.pak"), "rb") as f:
        hdr = f.read(12)
        off, ln = struct.unpack("<ii", hdr[4:])
        f.seek(off)
        directory = f.read(ln)
        f.seek(0, 2)
        pak2_size = f.tell()
    dirsha = hashlib.sha256(hdr + directory).hexdigest()
    ad = set()
    for p in ("pak0.pak", "pak1.pak", "pak2.pak"):
        ad |= {n.lower() for n in read_pak(os.path.join(rel, p))}
    owner = {}
    for game, tag in (("id1", 0), ("hipnotic", 1), ("rogue", 2)):
        folder = os.path.join(official, game)
        paks = sorted(fn for fn in os.listdir(folder) if re.match(r"pak\d+\.pak$", fn, re.I)) if os.path.isdir(folder) else []
        if game == "id1" and not paks:
            sys.exit("ad_import: no id1 paks in %s (--official)" % official)
        for fn in paks:
            for n in read_pak(os.path.join(folder, fn)):
                owner.setdefault(n.lower(), tag)
    shadowed = sorted(n for n in ad if n in owner and n != "maps/start.bsp")
    lines = ["// Generated by Misc/quakevr/ad_import.py --engine-lists from Arcane Dimensions 1.80 patch 1 and the official",
             "// paks' file names (no content); never edit. vr_ad.cpp: the release pin and AD's files an official pak has too",
             "// (skipped in AD's folder; the second argument is the official owner: 0 id1, 1 hipnotic, 2 rogue).",
             "#define VR_AD_PAK2_SIZE %d" % pak2_size,
             "#define VR_AD_PAK2_DIRSHA \"%s\"" % dirsha]
    lines += ["VR_AD_SHADOWED(\"%s\", %d)" % (n, owner[n]) for n in shadowed]
    with open(ENGINE_DATA, "w", encoding="latin1", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("ad_import: %s: %d shadowed files (id1 %d, hipnotic %d, rogue %d); pak2 %d bytes" % (
        os.path.relpath(ENGINE_DATA, ROOT), len(shadowed), sum(owner[n] == 0 for n in shadowed),
        sum(owner[n] == 1 for n in shadowed), sum(owner[n] == 2 for n in shadowed), pak2_size))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def map_keys(release):
    """Every key AD's FGD documents or AD's maps set."""
    keys = set()
    with open(os.path.join(release, "ad_1_8.fgd"), encoding="latin1") as f:
        for line in f:
            m = re.match(r"\s*([A-Za-z_]\w*)\s*\(", line)
            if m:
                keys.add(m.group(1))
    for pak in ("pak1.pak", "pak2.pak"):
        path = os.path.join(release, pak)
        if not os.path.isfile(path):
            continue
        with open(path, "rb") as f:
            for name, (o, s) in read_pak(path).items():
                if not name.endswith(".bsp"):
                    continue
                f.seek(o)
                head = f.read(12)
                eo, el = struct.unpack("<ii", head[4:12])
                f.seek(o + eo)
                lump = f.read(el).decode("latin1")
                keys.update(re.findall(r'"([A-Za-z_]\w*)"\s*"', lump))
    return keys


def ours():
    """Quake VR's QC (QC/ and its subfolders but QC/ad): system names, fields with types, every top-level name,
    builtin names, #define names."""
    system, fields, names, builtins, macros = set(), {}, set(), {}, set()
    funcs = set()
    for root, dirs, files in os.walk(QC_DIR):
        if os.path.normpath(root).startswith(os.path.normpath(OUT_DIR)):
            continue
        for fn in sorted(files):
            if not fn.endswith(".qc"):
                continue
            with open(os.path.join(root, fn), encoding="latin1") as f:
                text = f.read()
            macros.update(re.findall(r"^\s*#define\s+(\w+)", text, re.M))
            toks = tokenize(text)
            in_sys = fn == "defs.qc" and os.path.normpath(root) == os.path.normpath(QC_DIR)
            for st in statements(toks):
                ns, is_field, typ, is_builtin = declared(toks, st)
                if in_sys:
                    # The engine's entry points (main, StartFrame, PlayerPreThink...) are Quake VR's: AD's are
                    # renamed with the rest, so the engine never calls them.
                    if is_field or "(" not in typ:
                        system.update(ns)
                    if "end_sys_fields" in ns:
                        in_sys = False
                names.update(ns)
                if is_builtin:
                    for n in ns:
                        builtins[n] = builtin_number(toks, st)
                        SIGS.setdefault(n, signature(toks, st))
                elif is_field:
                    for n in ns:
                        fields.setdefault(n, typ)
                elif "(" in typ:
                    funcs.update(ns)
                    for n in ns:
                        SIGS.setdefault(n, signature(toks, st))
    return system, fields, names, builtins, macros, funcs


SIGS = {}  # Quake VR's function and builtin signatures, by name (ours())


def signature(tokens, stmt):
    """A function declaration's type without parameter names: 'string(string,string,...)'."""
    out, depth = [], 0
    toks = [tokens[k] for k in stmt]
    name = declared(tokens, stmt)[0]
    for kind, t in toks:
        if depth == 0 and name and t == name[0]:
            break
        if t == "(":
            depth += 1
        elif t == ")":
            depth -= 1
        if depth > 0 and kind == "id" and t not in TYPES and t != "optional":
            continue
        if t not in MODIFIERS:
            out.append(t)
    return "".join(out)


def builtin_number(tokens, stmt):
    toks = [tokens[k][1] for k in stmt]
    for j in range(len(toks) - 1):
        if toks[j] == "#" and re.match(r"\d+$", toks[j + 1]):
            return int(toks[j + 1])
    return 0


def verify_frames(rel):
    """Every AD frame function's frame number (its OP_STATE operand) in our compile equals AD's own progs.dat's
    (patch 1's, in pak2.pak). Reads the asm listing the precedence check writes (build/qcprec/qc/qc.asm): run the
    build first. fteqcc numbers $frame macros per progs.src entry, so a file folded into another would shift them."""
    d = pak_files(os.path.join(rel, "pak2.pak"), lambda n: n == "progs.dat")["progs.dat"]
    h = struct.unpack("<15i", d[:60])
    ofs_st, nst, ofs_fn, nfn, ofs_str, ofs_gl, ngl = h[2], h[3], h[8], h[9], h[10], h[12], h[13]
    gl = struct.unpack("<%df" % ngl, d[ofs_gl:ofs_gl + ngl * 4])
    starts = []
    for i in range(nfn):
        first, _, _, _, s_name = struct.unpack("<5i", d[ofs_fn + i * 36:ofs_fn + i * 36 + 20])
        if first > 0:
            e = d.index(b"\0", ofs_str + s_name)
            starts.append((first, d[ofs_str + s_name:e].decode("latin1")))
    starts.sort()
    theirs = {}
    for k, (first, name) in enumerate(starts):
        end = starts[k + 1][0] if k + 1 < len(starts) else nst
        for j in range(first, end):
            op, a = struct.unpack("<HH", d[ofs_st + j * 8:ofs_st + j * 8 + 4])
            if op == 60:  # OP_STATE
                theirs[name] = gl[a]
                break
    ours, cur = {}, None
    with open(os.path.join(ROOT, "build", "qcprec", "qc", "qc.asm"), errors="replace") as f:
        for line in f:
            if line.endswith(" = asm\n"):
                nm = line[:-7].split(")")[-1].strip().split()[-1]
                cur = nm[len(PREFIX):] if nm.startswith(PREFIX) else None
            elif cur and line.startswith("\tSTATE"):
                m = re.match(r"\tSTATE\s+(-?[\d.]+)f", line)
                if m:
                    ours[cur] = float(m.group(1))
                cur = None
            elif line.startswith("}"):
                cur = None
    bad = sorted(n for n in theirs if n in ours and abs(theirs[n] - ours[n]) > 1e-6)
    missing = sorted(n for n in theirs if n not in ours)
    print("ad_import --verify-frames: %d frame functions, %d compared, %d different, %d missing%s" % (
        len(theirs), len(theirs) - len(missing), len(bad), len(missing),
        (": " + ", ".join(bad[:10] + missing[:10])) if bad or missing else ""))
    return 1 if bad or missing else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--release", default=os.path.join(ROOT, "scratch", "ad", "ad"))
    ap.add_argument("--verify-frames", action="store_true",
                    help="after a build: compare AD's frame numbers with AD's own progs.dat; writes nothing")
    ap.add_argument("--engine-lists", action="store_true",
                    help="write Quake/vr/vr_ad_data.inc (the engine's release pin and shadowed-file list) and stop")
    ap.add_argument("--official", default="C:/OHWorkspace/qvr-kit/qbase",
                    help="a Quake folder with id1 (and hipnotic, rogue) paks, for --engine-lists")
    args = ap.parse_args()
    rel = args.release
    for fn, want in PINNED.items():
        got = sha256(os.path.join(rel, fn))
        if got != want:
            sys.exit("ad_import: %s is not AD 1.80 patch 1's (sha256 %s)" % (fn, got))
    if args.verify_frames:
        sys.exit(verify_frames(rel))
    if args.engine_lists:
        engine_lists(rel, args.official)
        return

    src = pak_files(os.path.join(rel, "pak0.pak"), lambda n: n.startswith("my_progs/"))
    src.update(pak_files(os.path.join(rel, "pak2.pak"), lambda n: n.startswith("my_progs/")))  # patch 1 wins
    src = {k[len("my_progs/"):]: v.decode("latin1").replace("\r\n", "\n") for k, v in src.items()}
    for fn, old, new, count, why in PATCHES:
        if src[fn].count(old) != count:
            sys.exit("ad_import: patch for %s (%s) matches %d times, not %d: %r" % (fn, why, src[fn].count(old), count, old))
        src[fn] = src[fn].replace(old, new)
    # Mechanical: Quake VR's beam temp entities (TE_LIGHTNING1/2/3, TE_BEAM) carry a beam id byte after the entity
    # (the client's VR_ParseBeamEntity; ours write 0 for a monster's). Every WriteEntity in AD's code (not commented out)
    # is one of those beams' entity: the byte follows it, else the client reads the next message one byte off.
    beam_rx = re.compile(r"^([ \t]*)WriteEntity[ \t]*\([ \t]*(MSG_\w+)[ \t]*,([^;\n]*)\);", re.M)
    beam_ids = 0
    for fn in src:
        src[fn], n = beam_rx.subn(lambda m: "%sWriteEntity (%s,%s); WriteByte (%s, 0);" % (
            m.group(1), m.group(2), m.group(3), m.group(2)), src[fn])
        beam_ids += n
    order = []
    for line in src["progs.src"].splitlines()[1:]:
        line = line.split("//")[0].strip()
        if line.endswith(".qc") and line not in SKIP:
            order.append(line)

    system, our_fields, our_names, our_builtins, our_macros, our_funcs = ours()
    our_numbers = {num: n for n, num in our_builtins.items() if num}
    keys = map_keys(rel)

    toks = {fn: tokenize(src[fn]) for fn in order}
    stmts = {fn: statements(toks[fn]) for fn in order}

    # fteqcc's autocvar(name, default): `name` is a cvar's name, not an identifier.
    for fn in order:
        t = toks[fn]
        ci = code(t)
        for a in range(len(ci) - 2):
            if t[ci[a]][1] == "autocvar" and t[ci[a + 1]][1] == "(" and t[ci[a + 2]][0] == "id":
                t[ci[a + 2]] = ("cvarname", t[ci[a + 2]][1])

    # Global arrays sized by a named constant: the number instead.
    consts = {}
    for fn in order:
        for st in stmts[fn]:
            t = [toks[fn][k][1] for k in st]
            if len(t) == 5 and t[0] == "float" and t[2] == "=" and re.match(r"\d+$", t[3]) and t[4] == ";":
                consts[t[1]] = t[3]
    for fn in order:
        for st in stmts[fn]:
            t = [toks[fn][k] for k in st]
            if any(x[1] == "{" for x in t):
                continue
            for a in range(len(st) - 2):
                if t[a][1] == "[" and t[a + 1][0] == "id" and t[a + 2][1] == "]" and t[a + 1][1] in consts:
                    toks[fn][st[a + 1]] = ("num", consts[t[a + 1][1]])

    # AD's own system section (defs.qc up to end_sys_fields) is the engine's: Quake VR's defines it.
    ad_sys, ad_fields, ad_builtin_stmts = set(), {}, {}
    skip_stmt = {fn: set() for fn in order}
    in_sys = True
    for k, st in enumerate(stmts["defs.qc"]):
        ns, is_field, typ, is_builtin = declared(toks["defs.qc"], st)
        if in_sys:
            ad_sys.update(ns)
            skip_stmt["defs.qc"].add(k)
            if "end_sys_fields" in ns:
                in_sys = False
    for fn in order:
        for k, st in enumerate(stmts[fn]):
            ns, is_field, typ, is_builtin = declared(toks[fn], st)
            if is_builtin:
                skip_stmt[fn].add(k)
                for n in ns:
                    ad_builtin_stmts.setdefault(n, (fn, st))
            elif is_field and k not in skip_stmt[fn]:
                for n in ns:
                    ad_fields.setdefault(n, (typ, fn))

    # What stays as written.
    keep = set(KEYWORDS) | system
    # A builtin Quake VR declares, or wraps in a QC function of the same name (frikbot's sound, stuffcmd...).
    # Only with the same signature: AD's strcat takes up to 8 strings, Quake VR's declaration 2 (the builtin is the
    # same): AD's declaration is kept as ad_strcat with the builtin's number.
    keep_builtins = {n for n in ad_builtin_stmts if (n in our_builtins or n in our_funcs)
                     and SIGS.get(n) == signature(toks[ad_builtin_stmts[n][0]], ad_builtin_stmts[n][1])}
    keep |= keep_builtins
    shared_fields = sorted(n for n, (typ, fn) in ad_fields.items()
                           if n in keys and n in our_fields and our_fields[n] == typ and n not in our_macros)
    keep |= set(shared_fields)

    def rename(name):
        if name in keep or name.startswith("__") or name.startswith(BRIDGE):
            return name
        m = re.match(r"(\w+)_[xyz]$", name)
        if m and m.group(1) in keep:
            return name
        return PREFIX + name

    # Headers: only the statements defining what the imported code uses (to a fixed point).
    body_ids = set()
    for fn in order:
        if fn in HEADERS:
            continue
        for st in stmts[fn]:
            body_ids |= ids_in(toks[fn], st)
    needed = set(body_ids)
    kept = {fn: set() for fn in order if fn in HEADERS}
    changed = True
    while changed:
        changed = False
        for fn in kept:
            for k, st in enumerate(stmts[fn]):
                if k in kept[fn] or k in skip_stmt[fn]:
                    continue
                ns = declared(toks[fn], st)[0]
                if set(ns) & needed:
                    kept[fn].add(k)
                    needed |= ids_in(toks[fn], st)
                    changed = True

    # Builtins AD uses that Quake VR's QC does not declare: by name (#0).
    # A standard builtin under another name (AD's walkmove_builtin = #32) keeps its number, as ad_<name>.
    bi_out, bi_renamed, bi_numbered, bi_byname = [], [], [], []
    for n in sorted(ad_builtin_stmts):
        if n in keep_builtins or n not in needed:
            continue
        fn, st = ad_builtin_stmts[n]
        num = builtin_number(toks[fn], st)
        if n in our_builtins and our_builtins[n]:
            num = our_builtins[n]
            bi_numbered.append("%s=#%d (another signature than Quake VR's: %s / %s)" % (
                n, num, signature(toks[fn], st), SIGS.get(n)))
            bi_out.append(emit(toks[fn], st, rename, builtin_name=PREFIX + n, number=num))
            continue
        if num in our_numbers:
            bi_numbered.append("%s=#%d (Quake VR's %s)" % (n, num, our_numbers[num]))
            bi_out.append(emit(toks[fn], st, rename, builtin_name=PREFIX + n, number=num))
            continue
        clash = n in our_names or n in our_macros
        if clash:
            bi_renamed.append(n)
        else:
            keep.add(n)
            bi_byname.append(n)
        text = emit(toks[fn], st, rename, builtin_name=(PREFIX + n if clash else n))
        bi_out.append(text)

    os.makedirs(OUT_DIR, exist_ok=True)
    banner = ("// Arcane Dimensions 1.80 patch 1 (Simon \"sock\" O'Callaghan and team; GPL-2.0-or-later, see\n"
              "// AD's readme and docs/vr-port/CREDITS.md), %s, generated by Misc/quakevr/ad_import.py: every name\n"
              "// AD defines is prefixed \"ad_\" (ARCANE_DIMENSIONS.md 5.2). Do not edit; change the importer.\n\n")
    written = []
    for fn in order:
        t = toks[fn]
        if fn in HEADERS:
            body = "\n".join(emit(t, st, rename) for k, st in enumerate(stmts[fn]) if k in kept[fn]) + "\n"
        else:
            drop = set()
            for k in skip_stmt[fn]:
                drop.update(range(stmts[fn][k][0], stmts[fn][k][-1] + 1))
            body = "".join(render(t, j, rename) for j in range(len(t)) if j not in drop)
        out = "ad_" + fn
        with open(os.path.join(OUT_DIR, out), "w", encoding="latin1", newline="\n") as f:
            f.write(banner % ("my_progs/" + fn) + body)
        written.append(out)
    with open(os.path.join(OUT_DIR, "ad_builtins.qc"), "w", encoding="latin1", newline="\n") as f:
        # AD as shipped: `if (string)` tests null (F322; the same in AD's own compile) and locals fteqcc can't prove
        # set on every path (F302, reported after the last file is read). Quiet from here on: AD's files are the
        # last in progs.src.
        f.write(banner % "extension builtins" +
                "#pragma warning disable F302\n#pragma warning disable F322\n\n"
                "// Not declared by Quake VR's QC: bound by name (#0) when the engine has them.\n\n" +
                "\n".join(bi_out) + "\n")
    # progs.src lists each file (fteqcc numbers $frame macros from 0 in each progs.src file, not in an #include).
    files = ["ad/" + fn for fn in ["ad_builtins.qc"] + written]
    update_progs_src(files)

    renamed_keys = sorted(n for n, (typ, fn) in ad_fields.items() if n in keys and n not in shared_fields)
    clash_type = sorted("%s (AD %s, Quake VR %s)" % (n, ad_fields[n][0], our_fields[n]) for n in renamed_keys
                        if n in our_fields)
    sys_missing = sorted(ad_sys - system - {"end_sys_globals", "end_sys_fields"})
    report = [
        "Arcane Dimensions import (Misc/quakevr/ad_import.py); regenerate, never edit.",
        "Source: ad_v1_80p1final.zip, " + ", ".join("%s %s" % kv for kv in sorted(PINNED.items())),
        "Files (AD's progs.src order): %d imported, skipped %s; headers filtered: %s" % (
            len(written), ", ".join(sorted(SKIP)), ", ".join(sorted(HEADERS))),
        "Header statements kept: " + ", ".join("%s %d/%d" % (fn, len(kept[fn]), len(stmts[fn])) for fn in sorted(kept)),
        "AD system names not kept (the engine's entry points: AD's are renamed, the engine never calls them): " +
        (", ".join(sys_missing) or "none"),
        "Builtins kept (Quake VR declares them): %d" % len(keep_builtins),
        "Builtins by name (#0, ad_builtins.qc): %d: %s" % (len(bi_byname), ", ".join(bi_byname)),
        "Builtins renamed (the name is Quake VR's for something else): " + (", ".join(bi_renamed) or "none"),
        "Standard builtins under AD's own names (number kept, ad_<name>): " + (", ".join(bi_numbered) or "none"),
        "Map-key fields shared with Quake VR (same name and type, bare): %d: %s" % (len(shared_fields), ", ".join(shared_fields)),
        "Map-key fields renamed ad_<key> (no Quake VR field, or another type; in an AD map the engine fills them from the key, pr_edict.c): %d" % len(renamed_keys),
        "  of them a type clash: " + (", ".join(clash_type) or "none"),
        "  " + ", ".join(renamed_keys),
        "Beam temp entities given Quake VR's beam id byte after WriteEntity (mechanical): %d" % beam_ids,
        "Reviewed patches: %d precedence, %d bridge hooks (VR_AD_*, QC/vr_ad_bridge.qc):" % (
            sum(1 for p in PATCHES if p[4].startswith(P)), sum(1 for p in PATCHES if p[4].startswith(B))),
    ] + ["  %s: %s" % (p[0], p[4]) for p in PATCHES if p[4].startswith(B)]
    with open(os.path.join(OUT_DIR, "AD_IMPORT.txt"), "w", encoding="latin1", newline="\n") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report[:12]))


BEGIN = "// ad_import.py: begin (generated list; Misc/quakevr/ad_import.py rewrites it)"
END = "// ad_import.py: end"


def update_progs_src(files):
    """Writes AD's files between the markers at the end of QC/progs.src (adds the markers the first time)."""
    path = os.path.join(QC_DIR, "progs.src")
    with open(path, encoding="latin1", newline="") as f:
        text = f.read()
    nl = "\r\n" if "\r\n" in text else "\n"
    block = nl.join([BEGIN] + files + [END])
    if BEGIN in text:
        a = text.index(BEGIN)
        b = text.index(END, a) + len(END)
        text = text[:a] + block + text[b:]
    else:
        text = text.rstrip("\r\n") + nl + block + nl
    with open(path, "w", encoding="latin1", newline="") as f:
        f.write(text)


def render(tokens, j, rename):
    kind, text = tokens[j]
    if kind == "id":
        return rename(text)
    if kind == "bc" and text.startswith("/*QUAKED"):
        return "/*ad:QUAKED" + text[len("/*QUAKED"):]  # (TrenchBroom's generator must not take AD's yet)
    return text


def emit(tokens, stmt, rename, builtin_name=None, number=0):
    """One statement's text (its own comments kept), renamed; a builtin declaration made '#<number>' and named."""
    parts = []
    first, last = stmt[0], stmt[-1]
    j = first
    while j <= last:
        kind, text = tokens[j]
        if builtin_name and text == "#":
            parts.append("#%d" % number)
            j += 1
            while j <= last and tokens[j][0] in ("ws", "num"):
                j += 1
            continue
        if builtin_name and kind == "id" and declared(tokens, stmt)[0] == [text]:
            parts.append(builtin_name)
        else:
            parts.append(render(tokens, j, rename))
        j += 1
    text = "".join(parts)
    is_function = "(" in declared(tokens, stmt)[2]
    if tokens[last][1] != ";" and (tokens[last][1] != "}" or not is_function):
        text += ";"  # (an array's initializer, `= { ... }`, ends with the brace: its semicolon was its own statement)
    return text


if __name__ == "__main__":
    main()

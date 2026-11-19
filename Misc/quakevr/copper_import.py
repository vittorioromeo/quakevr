#!/usr/bin/env python3
"""Imports Copper's QuakeC into Quake VR's progs, namespaced "cu_" and inert (docs/vr-port/DWELL.md, P1a).

    python -I Misc/quakevr/copper_import.py [--source DIR]
    python -I Misc/quakevr/copper_import.py --check [DIR]   (writes to DIR, default scratch/copper_check, not
                                                            QC/copper; diffs it against QC/copper and progs.src;
                                                            exit 1 on any difference)

DIR is a checkout of github.com/itslunaranyo/copper at master 379dd5d (GPL-2.0; default the kit's
downloads/copper_src); its progs.src and .qc files are checked against the pinned SHA-256 below, so the output is
reproducible. Nothing in DIR is executed (its qc2fgd.py and qc2def.py are not used).

The import is qc_import.py's (the same as Arcane Dimensions'), with Copper's configuration:
  - every name Copper defines becomes cu_<name>, so its spawn functions are cu_<classname>: they spawn only through
    the COPPER ruleset row (Quake/vr/vr_rules.cpp VR_Rules_SpawnName, which detects nothing until P1b) and the
    toolgun's explicit cu_ names under that ruleset;
  - its system section, builtin declarations and the rest of defs.qc filtered to what the code uses (defs.qc is a
    header); the CSQC HUD (csprogs.src: hud.qc, csdefs.qc) is not in progs.src, and NO_CSQC is taken as defined
    (Copper's csqc stats code is left out); test.qc (Copper's test entities) is skipped;
  - the engine's entry points (StartFrame, PlayerPreThink, ClientConnect, PutClientInServer...) are left out unless
    Copper's other code calls them: Quake VR's player and frame code run everywhere (DWELL.md 11.1);
  - map keys: a Copper field a map sets (its /*QUAKED documentation's keys, and the keys Dwell's 24 maps set:
    DWELL_KEYS) that Quake VR has with the same type stays bare (one field, as the map sets it); the others are
    cu_<key>, filled by the engine from the key in a Copper map (P1b, as AD's ad_<key>);
  - fx.qc's #define lines name Copper's fields: renamed with them.

Before committing a QC/copper change: make it here (PATCHES, a hook in main), regenerate, and run --check (0 files
differ). Re-running ad_import.py must still say 0 files differ for QC/ad (an import sees Quake VR's QC and the imports
before it in progs.src, never a later one).
"""
import argparse
import hashlib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # (run with python -I: the source is untrusted data)
import qc_import  # noqa: E402

ROOT = qc_import.ROOT
DEFAULT_SOURCE = "C:/OHWorkspace/qvr-kit/downloads/copper_src"
# SHA-256 over the sorted progs.src and *.qc files (name, a NUL, the bytes) of copper master 379dd5d.
PINNED = "91777a761b7acc5cc715cc3f29722dff834ee6636582d139501f21f8ca7f30c1"
SKIP = {"test.qc"}
HEADERS = {"defs.qc"}
# The keys Dwell's 24 maps set (Copper's map-makers' keys) that Copper's /*QUAKED documentation does not name; the
# compilers' underscore keys left out. Measured from dwellv2p2/maps with check_copper_entities.py's reader.
DWELL_KEYS = set("""ammo_nails ammo_rockets classname color corpsetime effects fixangle fog fog_color2 fog_density
fog_density2 locksounds mapversion minlight netname nextthink noise1 notrace origin phong rmovedir shadow shadows
skin_override sky skyfog skyfog_density skyfog_density2 solid spawnflags state sun surface switchshadstyle target2
target3 target4 think touch track_intermission use wateralpha yaw_speed""".split())
# Reviewed edits to Copper's source, applied before renaming: (file, exact text, replacement, count, why).
P = "parenthesised as QC priorities read it (check_qc_precedence)"
M = "mechanical"
PATCHES = [
    # NO_CSQC drops the HUD stats' registration and computation, but t_level.qc and t_void.qc still set these fields
    # (Copper's own NO_CSQC build does not compile either): the fields stay, out of the #ifndef.
    ("defs.qc", "#ifndef NO_CSQC\n// fields for csqc HUD stats\n.float\t\tclientdata;\n.float\t\ttint;\n.vector\t\ttint_color;\n#endif\n",
     "// fields for csqc HUD stats (QVR: kept with NO_CSQC: t_level.qc and t_void.qc set them)\n.float\t\tclientdata;\n"
     ".float\t\ttint;\n.vector\t\ttint_color;\n", 1, M + ": the csqc stat fields kept with NO_CSQC"),
] + [
    # fteqcc (autoproto) leaves an omitted argument with a default undefined (Q105); Copper means its default.
    (f, "T_RadiusDamage (self, self.trueowner, 40, world);",
     "T_RadiusDamage (self, self.trueowner, 40, world, DMGTYPE_EXPLOSION); // QVR: the parameter's default, explicit",
     1, M + ": T_RadiusDamage's default damage type passed (fteqcc Q105: undefined otherwise)")
    for f in ("m_ogre.qc", "m_shalrath.qc")
] + [
    # The precedence patches (qvr_checks qc_precedence), as ad_import.py's: parenthesised the way fteqcc's QC priorities
    # read them (Copper's own compile and ours), so behaviour stays Copper's as shipped; the ones that look like Copper
    # bugs are marked "Copper as shipped" (P1b decides, with AD's).
    (f, old, new, n, P) for f, old, new, n in (
        ("weapons.qc", "if (self.items & weap == 0)", "if ((self.items & weap) == 0)", 1),
        ("ai.qc", "if (self.flags & (FL_FLY|FL_SWIM) == 0)", "if ((self.flags & (FL_FLY|FL_SWIM)) == 0)", 2),
        ("dev.qc", "if (serverflags & SVFL_ALLEPISODES == SVFL_ALLEPISODES)",
         "if ((serverflags & SVFL_ALLEPISODES) == SVFL_ALLEPISODES)", 1),
        ("client.qc", "if (next.flags & FL_CLIENT == 0)", "if ((next.flags & FL_CLIENT) == 0)", 1),
        ("item_keys_runes.qc", "if (cl.worldtype & 240 == 240)", "if ((cl.worldtype & 240) == 240)", 1),
        ("item_keys_runes.qc", "if (cl.worldtype & 15 == 15)", "if ((cl.worldtype & 15) == 15)", 1),
        ("item_keys_runes.qc", "if (cl.worldtype & 240 == 0)", "if ((cl.worldtype & 240) == 0)", 1),
        ("item_keys_runes.qc", "if (cl.worldtype & 15 == 0)", "if ((cl.worldtype & 15) == 0)", 1),
        ("fx.qc", "if (self.spawnflags & (PFIELD_INWARD | PFIELD_OUTWARD) == (PFIELD_INWARD | PFIELD_OUTWARD))",
         "if ((self.spawnflags & (PFIELD_INWARD | PFIELD_OUTWARD)) == (PFIELD_INWARD | PFIELD_OUTWARD))", 1),
        ("items.qc", "if (sf & serverflags != sf)", "if ((sf & serverflags) != sf)", 1),
        ("items.qc", "if (it & e.items != it)", "if ((it & e.items) != it)", 1),
        ("projectiles.qc", "if (traceflags & (TRACE_NOMONSTERS|TRACE_WATER) == TRACE_WATER)",
         "if ((traceflags & (TRACE_NOMONSTERS|TRACE_WATER)) == TRACE_WATER)", 1),
        ("triggers.qc", "if ( other.flags & (FL_MONSTER | FL_FLY | FL_SWIM) != FL_MONSTER )",
         "if ( (other.flags & (FL_MONSTER | FL_FLY | FL_SWIM)) != FL_MONSTER )", 1),
        ("item_weap_ammo.qc", "if (other.items & self.items == self.items)", "if ((other.items & self.items) == self.items)", 1),
        ("client.qc", "if (time > self.air_finished ||", "if ((time > self.air_finished ||", 1),
        ("client.qc", "time > self.air_finished - 10 && self.dmg)",
         "time > self.air_finished - 10) && self.dmg) // QVR: Copper as shipped (QC reads `a || b && c` as `(a || b) && c`)", 1),
        # `x = a && b && c` is `(x = a) && b && c` (the other terms' cvar reads have no effect): Copper as shipped.
        ("t_void.qc", "noTint = (cvar(\"gl_cshiftpercent\") < CSHIFT_CUTOFF) && cvar(\"gl_farclip\") && !coop;",
         "noTint = (cvar(\"gl_cshiftpercent\") < CSHIFT_CUTOFF); // QVR: Copper as shipped: QC reads `x = a && b` as "
         "`(x = a) && b` (&& cvar(\"gl_farclip\") && !coop)", 1),
    )
]


def source_files(src_dir):
    files, h = {}, hashlib.sha256()
    for fn in sorted(os.listdir(src_dir)):
        if fn.endswith(".qc") or fn == "progs.src":
            data = open(os.path.join(src_dir, fn), "rb").read()
            h.update(fn.encode() + b"\0" + data)
            files[fn] = data.decode("latin1").replace("\r\n", "\n")
    if h.hexdigest() != PINNED:
        sys.exit("copper_import: %s is not copper master 379dd5d (sha256 %s)" % (src_dir, h.hexdigest()))
    return files


def quaked_keys(src):
    """The keys Copper's /*QUAKED comments document ("key" at a line's start)."""
    keys = set()
    for text in src.values():
        for block in re.findall(r"/\*QUAKED.*?\*/", text, re.S):
            keys.update(re.findall(r'(?m)^\s*"([A-Za-z_]\w*)"', block))
    return keys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", default=DEFAULT_SOURCE)
    ap.add_argument("--check", metavar="DIR", nargs="?", const=os.path.join(ROOT, "scratch", "copper_check"),
                    help="write to DIR (default scratch/copper_check) instead of QC/copper, then diff against "
                         "QC/copper and progs.src; exit 1 on any difference")
    args = ap.parse_args()
    src = source_files(args.source)
    for fn, old, new, count, why in PATCHES:
        if src[fn].count(old) != count:
            sys.exit("copper_import: patch for %s (%s) matches %d times, not %d: %r" % (fn, why, src[fn].count(old), count, old))
        src[fn] = src[fn].replace(old, new)
    keys = quaked_keys(src) | DWELL_KEYS
    mod = qc_import.Mod(
        name="Copper", short="Copper", script="copper_import.py", folder="copper", prefix="cu_", skip=SKIP,
        headers=HEADERS, keys=keys, defined={"NO_CSQC"}, drop_entry_points=True, rename_defines=True,
        key_fill="in a Copper map the engine fills them from the key (P1b, as AD's)",
        banner=("// Copper master 379dd5d (Lunaran; GPL-2.0-or-later, see docs/vr-port/CREDITS.md), %s, generated by\n"
                "// Misc/quakevr/copper_import.py: every name Copper defines is prefixed \"cu_\" (DWELL.md P1a).\n"
                "// Do not edit; change the importer.\n\n"),
        # Copper as shipped: its models are precached through its own helpers (precache_model3...), not where setmodel
        # names them (F210); fx.qc's target_debris_use has code after its loop that fteqcc calls unreachable (F323).
        # Quiet from here on: its files follow every other in progs.src.
        builtins_text=("#pragma warning disable F210\n#pragma warning disable F323\n\n"
                       "// Not declared by Quake VR's QC: bound by name (#0) when the engine has them.\n\n"),
        report_name="COPPER_IMPORT.txt",
        report_head=["Copper import (Misc/quakevr/copper_import.py); regenerate, never edit.",
                     "Source: github.com/itslunaranyo/copper master 379dd5d, sha256 (progs.src and *.qc) " + PINNED,
                     "Map keys: %d (its /*QUAKED keys and Dwell's maps' keys)" % len(keys)])
    if args.check:
        mod.write_dir = qc_import.check_dir(mod, args.check)
    report, _ = qc_import.run(mod, src)
    report.append("Reviewed patches: %d" % len(PATCHES))
    report += ["  %s: %s" % (p[0], p[4]) for p in PATCHES]
    qc_import.write_report(mod, report)
    print("\n".join(report[:14]))


if __name__ == "__main__":
    main()

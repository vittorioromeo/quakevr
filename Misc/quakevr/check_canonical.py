#!/usr/bin/env python3
"""check_canonical.py -- no NEW copies of the helper families the refactor made canonical.

docs/vr-port/CODE_STYLE.md ("Canonical helpers") and docs/vr-port/ARCHITECTURE.md ("Where to put new code") name one
implementation per helper. This check counts, per file, the code that copies or bypasses one (a local `vec(const
float*)` or `wrap180`, a new `rnd` wrapper, a direct `->haptic(` call, `PR_PushQCVM` outside `progs::ServerVm`, a QC
`find(world, classname, "player")`, ...) and fails when a file has more than Misc/quakevr/canonical_allowlist.txt
allows. The allowlist holds today's sites: the canonical implementations themselves and the ones the refactor left on
purpose (ROUND23, "left" lines). Comments are ignored; string literals are kept.

  python Misc/quakevr/check_canonical.py              check (the kit's build.sh runs it; exit 1 on a new copy)
  python Misc/quakevr/check_canonical.py --list FAM   every current site of one family (file:line: text)
  python Misc/quakevr/check_canonical.py --tighten    lower the allowlist to today's counts (after migrating sites)

A new site that really is meant (a different rule, a test of the engine itself): add or raise its allowlist line by
hand, with the reason after `#`. Never raise a count just to pass the build. Run from the repository's root.
"""
import glob
import os
import re
import sys

ALLOWLIST = "Misc/quakevr/canonical_allowlist.txt"

# A C++ definition (or declaration) of NAME: a return type then the name and `(`, not a call after return/else/...
_CPP_DEF = (r"(?m)^[ \t]*(?:\[\[\w+\]\][ \t]*)*(?:(?:static|inline|constexpr|const|extern|friend)\s+)*"
            r"(?!return\b|else\b|case\b|new\b|delete\b|throw\b|co_return\b|goto\b)\w[\w:<>]*[ \t*&]+(?:%s)\s*\(")
# The same as a lambda: `auto NAME = [...](`.
_CPP_LAMBDA = r"\bauto\s+(?:%s)\s*=\s*\[[^\]\n]*\]\s*\("


def cpp_def(names, params=r""):
    alt = "|".join(names)
    return [_CPP_DEF % ("(?:%s)" % alt) + params, _CPP_LAMBDA % ("(?:%s)" % alt) + params]


# family id -> (language, [regexes], what to use instead)
FAMILIES = {
    "geom-vec": ("cpp", cpp_def(["vec", "toGlm", "toVec"], r"\s*const\s+(?:float\s*\*|vec3_t)"),
                 "geom::vec / geom::store (vr_geom.hpp)"),
    "geom-math": ("cpp", cpp_def(["safeNormalize", "wrap180", "wrapYaw", "wrapDegrees", "wrapAngle", "smooth01",
                                  "smoothstep", "smoothStep", "approachExp", "yawDelta", "yawOf", "closestOnSegment",
                                  "segmentDistance", "closestOnTriangle", "nearestOnTri", "quakeFromTracking",
                                  "angleBetween"]),
                  "geom:: (vr_geom.hpp)"),
    "rnd-wrapper": ("cpp", cpp_def(["rnd", "rnd01", "random01", "randomFloat", "frand", "randf", "rand01"]),
                    "vrand::range / vrand::xorshift* / vrand::lcg* (vr_random.hpp)"),
    "strings-copy": ("cpp", cpp_def(["startsWith", "endsWith", "lower", "toLower", "trim", "appendf",
                                     "nearestNormal", "quantile", "percentile", "quantileSorted"]),
                     "strings:: (vr_strings.hpp), mdlbuild::nearestNormal, vrstats::quantileSorted"),
    "haptic-direct": ("cpp", [r"(?:->|\.)\s*haptic\s*\("],
                      "haptics::pulse(hand, seconds, amplitude, hz) (vr_backend.hpp)"),
    "qcvm-push": ("cpp", [r"\bPR_PushQCVM\s*\("], "progs::ServerVm vm; (vr_progs.hpp)"),
    "player-edict": ("cpp", [r"\bEDICT_NUM\s*\(\s*1\s*\)", r"\bsvs\.clients\s*\[\s*0\s*\]\s*\.edict\b"],
                     "progs::localPlayer() / progs::playerEdict() (vr_progs.hpp)"),
    "cvar-by-name": ("cpp", [r"\bCvar_VariableValue\s*\(\s*\"vr_"], "the cvar_t directly: vr_x.value"),
    "field-by-name": ("cpp", [r"\bED_FindFieldOffset\s*\(\s*\""], "progs::fields().x (add the field to vr_fields.inc)"),
    "qc-find-player": ("qc", [r"\bfind\s*\(\s*world\s*,\s*classname\s*,\s*\"player\"\s*\)"],
                       "VR_Player() / VR_NextPlayer(p) (QC/vr_util.qc)"),
    "qc-math-copy": ("qc", [r"(?m)^[ \t]*\w+\s*\([^)]*\)\s*(?:\w+_(?:Cross|Wrap180|Wrap|AngleNorm|SegDist|SegmentDist|"
                            r"Lerp|Nlerp|Clamp|Smooth|Ease)|clamp)\s*="],
                     "VR_Cross, VR_Wrap180, clamp, VR_Lerp, VR_Smooth, VR_SegmentDistance(Eps) (QC/vr_util.qc)"),
    "qc-test-check": ("qc", [r"(?m)^[ \t]*void\s*\(\s*(?:float\s+\w+\s*,\s*string\s+\w+|string\s+\w+\s*,\s*float\s+\w+)"
                             r"\s*\)\s*\w+_Check\s*="],
                      "VR_Test_Begin/Check/Verdict (QC/vr_testutil.qc) or the VR_AGT_* harness"),
}


# A cheap test before the family's regexes: a file without any of these words has no site.
NEEDLES = {
    "geom-vec": r"\b(?:vec|toGlm|toVec)\b",
    "geom-math": r"\b(?:safeNormalize|wrap180|wrapYaw|wrapDegrees|wrapAngle|smooth01|smoothstep|smoothStep|approachExp|"
                 r"yawDelta|yawOf|closestOnSegment|segmentDistance|closestOnTriangle|nearestOnTri|quakeFromTracking|"
                 r"angleBetween)\b",
    "rnd-wrapper": r"\b(?:rnd|rnd01|random01|randomFloat|frand|randf|rand01)\b",
    "strings-copy": r"\b(?:startsWith|endsWith|lower|toLower|trim|appendf|nearestNormal|quantile|percentile|"
                    r"quantileSorted)\b",
    "haptic-direct": r"haptic",
    "qcvm-push": r"PR_PushQCVM",
    "player-edict": r"EDICT_NUM|svs\.clients",
    "cvar-by-name": r"Cvar_VariableValue",
    "field-by-name": r"ED_FindFieldOffset",
    "qc-find-player": r"\"player\"",
    "qc-math-copy": r"(?:Cross|Wrap180|Wrap|AngleNorm|SegDist|SegmentDist|Lerp|Nlerp|Clamp|Smooth|Ease|clamp)\s*=",
    "qc-test-check": r"_Check\s*=",
}


def read(path):
    with open(path, encoding="latin-1") as f:
        return f.read()


_TOKENS = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|//[^\n]*|/\*.*?\*/', re.S)


def strip_comments(s):
    """Comments become spaces (newlines kept, so line numbers hold); string and char literals stay."""
    def repl(m):
        t = m.group(0)
        if t[0] in "\"'":
            return t
        return re.sub(r"[^\n]", " ", t)
    return _TOKENS.sub(repl, s)


def sources():
    cpp = []
    for ext in ("cpp", "hpp", "h", "inc", "c"):
        cpp += glob.glob("Quake/vr/**/*." + ext, recursive=True)
    cpp = sorted(p.replace("\\", "/") for p in cpp if "/external/" not in p.replace("\\", "/"))
    qc = sorted(p.replace("\\", "/") for p in glob.glob("QC/*.qc"))
    return {"cpp": cpp, "qc": qc}


def scan():
    """{family: {path: [(line, text)]}}"""
    files = sources()
    texts = {}
    found = {f: {} for f in FAMILIES}
    raw = {}
    for fam, (lang, regexes, _) in FAMILIES.items():
        needle = re.compile(NEEDLES[fam])
        regexes = [re.compile(rx) for rx in regexes]
        for path in files[lang]:
            if path not in raw:
                raw[path] = read(path)
            if not needle.search(raw[path]):
                continue
            if path not in texts:
                texts[path] = strip_comments(raw[path])
            text = texts[path]
            hits = set()
            for rx in regexes:
                for m in rx.finditer(text):
                    hits.add(m.start())
            for pos in sorted(hits):
                line = text.count("\n", 0, pos) + 1
                lstart = text.rfind("\n", 0, pos) + 1
                lend = text.find("\n", pos)
                found[fam].setdefault(path, []).append((line, text[lstart:lend if lend >= 0 else None].strip()))
    return found


def load_allowlist():
    allowed = {}
    lines = []
    if os.path.exists(ALLOWLIST):
        for raw in read(ALLOWLIST).splitlines():
            lines.append(raw)
            body = raw.split("#", 1)[0].split()
            if len(body) == 3 and body[2].isdigit():
                allowed[(body[0], body[1])] = int(body[2])
    return allowed, lines


def tighten(found, allowed, lines):
    out = []
    changed = 0
    for raw in lines:
        body = raw.split("#", 1)[0].split()
        if len(body) == 3 and body[2].isdigit():
            fam, path, n = body[0], body[1], int(body[2])
            now = len(found.get(fam, {}).get(path, []))
            if now < n:
                changed += 1
                if now == 0:
                    continue
                raw = raw.replace("%s %s %d" % (fam, path, n), "%s %s %d" % (fam, path, now), 1)
        out.append(raw)
    with open(ALLOWLIST, "w", encoding="latin-1", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print("check_canonical: %d allowlist line(s) lowered or dropped" % changed)
    return 0


def main(argv):
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    found = scan()
    allowed, lines = load_allowlist()
    if len(argv) >= 2 and argv[0] == "--list":
        fam = argv[1]
        if fam not in FAMILIES:
            print("check_canonical: no family %s (%s)" % (fam, ", ".join(FAMILIES)))
            return 1
        for path, hits in sorted(found[fam].items()):
            for line, text in hits:
                print("%s:%d: %s" % (path, line, text[:140]))
        return 0
    if argv[:1] == ["--tighten"]:
        return tighten(found, allowed, lines)

    problems = []
    stale = 0
    total = 0
    for fam, per in found.items():
        for path, hits in sorted(per.items()):
            total += len(hits)
            n = allowed.get((fam, path), 0)
            if len(hits) > n:
                where = ", ".join("%d" % l for l, _ in hits)
                problems.append("%s: %d %s site(s), %d allowed (lines %s): use %s"
                                % (path, len(hits), fam, n, where, FAMILIES[fam][2]))
    for (fam, path), n in allowed.items():
        if len(found.get(fam, {}).get(path, [])) < n:
            stale += 1
    if problems:
        for p in problems:
            print("check_canonical: " + p)
        print("check_canonical: %d new copy/copies of a canonical helper (docs/vr-port/CODE_STYLE.md, "
              "'Canonical helpers'; a meant one: a line in %s with its reason)" % (len(problems), ALLOWLIST))
        return 1
    note = ""
    if stale:
        note = "; %d allowlist line(s) above today's count: run --tighten" % stale
    print("check_canonical: %d families, %d allowed sites, no new copies%s" % (len(FAMILIES), total, note))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

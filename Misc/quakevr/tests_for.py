"""tests_for.py [--tree DIR] [--top N] [--why] [file ...] -- the targeted tests for these changed files.

With no files: the files this branch changed against origin/next, plus uncommitted ones. For each file it prints a
fixed rule (tables, headers, generated files) or the best few tests, each with the command that runs it:
  - a Misc/quakevr *_test.sh/.py/.ps1 script (with its sibling *_check.py),
  - an mp_test.py scenario (`--only mp_x`),
  - a numbered in-engine test (QC/test_numbers.txt: `vr_embed_test N`, ...) by the scripts that run its numbers,
  - an engine test command (*_test, *_selftest).
A test scores by the names it shares with the file (IDF-weighted, the rarest count most): the console commands the
file registers, the vr_* cvars it reads, and for QC the functions it defines (a numbered test's QC file calling them).
--why prints the shared names. Read only; a starting point, not a proof: run what it lists plus a smoke run, and add
what you know it misses (docs/vr-port/TESTING.md, "Which tests to run").
"""
import collections, math, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.normpath(os.path.join(HERE, "..", ".."))
TOP, WHY, files = 6, False, []
args = sys.argv[1:]
while args:
    a = args.pop(0)
    if a == "--tree": TREE = args.pop(0)
    elif a == "--top": TOP = int(args.pop(0))
    elif a == "--why": WHY = True
    elif a in ("-h", "--help"): print(__doc__); sys.exit(0)
    else: files.append(a)
TREE = TREE.replace("\\", "/").rstrip("/")
MQ = TREE + "/Misc/quakevr"
SRC_EXT = (".c", ".cpp", ".h", ".hpp", ".inc", ".qc")

# Files every change touches or that only declare: no test of their own picks them; the rule says what to run.
RULES = {
    "Quake/vr/vr_cvars.inc": "the test(s) of the cvar's own feature (tests_for.py the file that reads it); build + qvr_checks",
    "Quake/vr/vr_api.h": "build only (a declaration); the tests of the .cpp that defines the function",
    "QC/test_numbers.txt": "build (qvr_checks: duplicate numbers)",
    "QC/progs.src": "build (QC 0 warnings) + a smoke run (map e1m1)",
    "QC/vr_cvars_gen.qc": "build (generated from vr_cvars.inc)",
    "Quake/vr/vr_tunables.inc": "tunables_test.sh + the tuned feature's test",
    "Quake/vr/vr_tunables_rows.inc": "tunables_test.sh; settings_page (release suite)",
    "Quake/vr/vr_menu_rows.inc": "settings_page (release suite), vr_menu_path_check maps/vrcalibration.map, menu_flat_panel_test.py",
    "Quake/vr/vr_menu_pages.inc": "settings_page, vr_menu_path_check, menu_flat_panel_test.py",
    "Quake/vr/vr_menu_debug.inc": "vr_menu_path_check; the debug tool's own test",
    "Quake/vr/vr_menu_debug_tests.inc": "the test the row starts (its command); vr_menu_path_check",
    "Quake/vr/vr_menu_instances.inc": "settings_page, vr_menu_path_check",
    "Quake/vr/vr_fields.inc": "build + a smoke run (edict fields: every mode)",
    "Quake/vr/vr_protocol.hpp": "mp_test.py --only mp_version,mp_coop,mp_bandwidth (the wire format) + changelevel_signon",
    "Misc/quakevr/test_baseline.cfg": "a smoke run; every test reads it (the targeted ones of what the setting changes)",
}


def read(p):
    try:
        with open(p, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def rel_of(p):
    p = p.replace("\\", "/")
    return p[len(TREE) + 1:] if p.lower().startswith(TREE.lower() + "/") else p


# Known names: cvars (vr_cvars.inc), commands (Cmd_AddCommand anywhere in Quake/), QC functions (their defining file).
known = set(re.findall(r"\b(vr_[a-z0-9_]+)\b", read(TREE + "/Quake/vr/vr_cvars.inc")))
cmd_owner = collections.defaultdict(set)
for dp, dn, fn in os.walk(TREE + "/Quake"):
    for f in fn:
        if f.endswith(SRC_EXT):
            p = os.path.join(dp, f)
            for c in re.findall(r'Cmd_AddCommand\w*\s*\(\s*"([A-Za-z0-9_]+)"', read(p)):
                known.add(c); cmd_owner[c].add(rel_of(p))
QC_TYPES = r"(?:void|float|entity|vector|string|int)"
QC_DEF = re.compile(r"^%s\s*\([^)]*\)\s*(\w+)\s*=\s*(?!#)|^%s\s+(\w+)\s*\([^;]*$" % (QC_TYPES, QC_TYPES), re.M)
qc_defs = collections.defaultdict(set)  # file -> functions it defines
qc_text = {}
for f in os.listdir(TREE + "/QC"):
    if f.endswith(".qc"):
        t = qc_text["QC/" + f] = read(TREE + "/QC/" + f)
        for m in QC_DEF.finditer(t):
            qc_defs["QC/" + f].add(m.group(1) or m.group(2))
qc_funcs = set().union(*qc_defs.values()) if qc_defs else set()


def tokens_of_source(rel):
    t = read(TREE + "/" + rel)
    toks = set(re.findall(r'Cmd_AddCommand\w*\s*\(\s*"([A-Za-z0-9_]+)"', t))
    toks |= {n for n in re.findall(r"\b(vr_[a-z0-9_]+)\b", t) if n in known}
    if rel.endswith(".qc"):
        toks |= set(re.findall(r'"(vr_[a-z0-9_]+)', t)) | qc_defs.get(rel, set())
    return toks


# The tests: name -> (body, how to run it).
tests = {}
for dp, dn, fn in os.walk(MQ):
    if "__pycache__" in dp or "testbatch" in dp:
        continue
    for f in fn:
        if re.search(r"_test\.(sh|py|ps1)$", f) and f != "mp_test.py":
            p = os.path.join(dp, f)
            r = rel_of(p)
            run = {"sh": "bash ", "py": "python ", "ps1": "powershell -File "}[f.rsplit(".", 1)[1]] + r
            tests[r] = (read(p) + read(re.sub(r"_test\.(sh|py|ps1)$", "_check.py", p)), run)
mp = read(MQ + "/mp/mp_test.py")
defs = list(re.finditer(r"^def (\w+)\(", mp, re.M))
suite = set(re.findall(r'\("(mp_\w+)"', mp[mp.find("\nSUITE = ["):][:8000]))
for i, m in enumerate(defs):
    name = "mp_" + m.group(1)
    if name in suite or name + "_listen" in suite:
        end = defs[i + 1].start() if i + 1 < len(defs) else len(mp)
        tests["mp " + name] = (mp[m.start():end], "python Misc/quakevr/mp/mp_test.py suite --only " + name)
for c, owners in cmd_owner.items():
    if c.endswith("_test") or c.endswith("_selftest"):
        tests["engine " + c] = (c + " " + " ".join(owners), "the command %s (in %s)" % (c, ", ".join(sorted(owners))))

# Numbered in-engine tests: each QC test file of test_numbers.txt, its cvar and numbers, run by the scripts that set
# that cvar to one of the file's numbers.
claims = collections.defaultdict(list)  # (cvar, QC/file) -> [(lo, hi)]
for line in read(TREE + "/QC/test_numbers.txt").splitlines():
    m = re.match(r"^(vr_\w+)\s+(\d+)(?:-(\d+)|(\+))?\s+(\S+\.qc)\s", line)
    if m:
        lo = int(m.group(2)); hi = int(m.group(3)) if m.group(3) else (10 ** 9 if m.group(4) else lo)
        claims[(m.group(1), "QC/" + os.path.basename(m.group(5)))].append((lo, hi))
script_texts = {k: v[0] for k, v in tests.items() if not k.startswith(("mp ", "engine "))}


def runners(cvar, ranges):
    out = []
    for name, body in script_texts.items():
        nums = sorted({int(n) for n in re.findall(r"\b%s\s+(\d+)" % cvar, body)
                       if any(lo <= int(n) <= hi for lo, hi in ranges)})
        if nums:
            out.append((len(nums), name, nums))
    out.sort(reverse=True)
    return ["%s [%s]" % (n, ",".join(map(str, nums[:8])) + ("..." if len(nums) > 8 else "")) for _, n, nums in out[:3]]


def span_text(ranges):
    """[(1,1),(2,2),(3,3),(7,9),(20,10**9)] -> '1-3,7-9,20+'"""
    out = []
    for lo, hi in sorted(ranges):
        if out and lo <= out[-1][1] + 1:
            out[-1][1] = max(out[-1][1], hi)
        else:
            out.append([lo, hi])
    return ",".join(str(lo) if lo == hi else ("%d+" % lo if hi >= 10 ** 9 else "%d-%d" % (lo, hi)) for lo, hi in out)


for (cvar, qf), ranges in claims.items():
    if qf in qc_text:
        nums = span_text(ranges)
        r = runners(cvar, ranges)
        how = "%s %s (%s): %s" % (cvar, nums if len(nums) < 40 else nums[:37] + "...", qf, "; ".join(r) if r else
                                  "no script: run.sh <agent> -Script \"map <its map>;%s N\" -QuitOn <its verdict>" % cvar)
        tests["numbered %s (%s)" % (cvar, qf)] = (qc_text[qf], how)

ident = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")
names = known | qc_funcs
test_toks = {k: set(t for t in ident.findall(v[0]) if t in names or t.startswith("vr_")) for k, v in tests.items()}
df = collections.Counter()
for s in test_toks.values():
    df.update(s)
N = len(tests)


def rank(rel):
    toks = tokens_of_source(rel)
    scores = []
    for name, s in test_toks.items():
        shared = toks & s
        if not shared:
            continue
        sc = sum(math.log(N / df[t]) for t in shared)
        if name.startswith("engine ") and rel in tests[name][0]: sc += 20
        if name.startswith("numbered ") and name.endswith("(%s)" % rel): sc += 20  # (the test file itself)
        if tests[name][1].endswith(" " + rel): sc += 20  # (a test script itself)
        scores.append((sc, name, sorted(shared, key=lambda t: df[t])[:4]))
    scores.sort(reverse=True)
    return toks, scores[:TOP]


if not files:
    git = lambda *a: subprocess.run(["git", "-C", TREE] + list(a), capture_output=True, text=True).stdout.split()
    files = sorted(set(git("diff", "--name-only", "origin/next...HEAD") + git("diff", "--name-only", "HEAD")))
    if not files:
        print("tests_for: no changed files (against origin/next); pass files")
for f in files:
    rel = rel_of(os.path.abspath(f) if os.path.exists(f) and os.path.isabs(f) else f)
    if rel in RULES:
        print("%s\n    RULE: %s" % (rel, RULES[rel]))
        continue
    if not rel.endswith(SRC_EXT) and not re.search(r"_test\.(sh|py|ps1)$", rel):
        if rel.startswith("Misc/quakevr/mp/"):
            print("%s\n    python Misc/quakevr/mp/mp_test.py suite (the harness; or --only the scenarios it touches)" % rel)
        elif not rel.endswith((".md", ".txt")):
            print("%s\n    (not source: no mapping; a smoke run if the game reads it)" % rel)
        continue
    toks, best = rank(rel)
    print("%s  (%d names)" % (rel, len(toks)))
    if not best:
        print("    no test names its commands, cvars or functions: a smoke run (map e1m1), and a test of your own")
    for sc, n, sh in best:
        print("    %5.1f  %s" % (sc, tests[n][1]) + ("   [%s]" % ", ".join(sh) if WHY else ""))

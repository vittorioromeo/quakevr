#!/usr/bin/env python3
"""check_test_numbers.py -- the numbered test dispatchers against their registry (QC/test_numbers.txt).

The numbered test cvars (`vr_agrunt_test N`, `vr_ad_test N`, ...) are dispatched by QC functions as chains of
`if(t == N)` / `if(t >= A && t <= B)` branches. Parallel workers picking "the next free number" have clashed (the
second branch on a number is dead code). This check fails when:
  - two registry lines claim the same number of the same cvar;
  - two dispatch branches (not nested in one another) take the same number;
  - a dispatched number is not in the registry, or a registry line is dispatched by nothing;
  - a Debug menu / scenario command (Quake/vr: `command("...", "... vr_x_test N ...")`) runs a number that is not
    registered.
The registry's format is in QC/test_numbers.txt's header. Run from the repository's root (the kit's build.sh does);
exit 1 on a problem. `--list` prints each cvar's dispatched numbers (for a new registry line).
"""
import glob
import os
import re
import sys

REGISTRY = "QC/test_numbers.txt"
MENU_GLOBS = ["Quake/vr/*.inc", "Quake/vr/*.cpp", "Quake/vr/*.hpp"]
INF = 1 << 30


def fmt(lo, hi):
    if hi >= INF:
        return "%d+" % lo
    return "%d" % lo if lo == hi else "%d-%d" % (lo, hi)


def parse_numbers(spec):
    # "48", "20-39", "100+"
    m = re.fullmatch(r"(\d+)(?:-(\d+)|(\+))?", spec)
    if not m:
        return None
    lo = int(m.group(1))
    hi = INF if m.group(3) else int(m.group(2) or lo)
    return (lo, hi) if hi >= lo else None


def read_registry(errors):
    dispatchers = []  # (cvar, file, function, var, within)
    entries = {}  # cvar -> [(lo, hi, lineno, text)]
    with open(REGISTRY, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            words = line.split()
            if words[0] == "dispatch":
                opts = words[5:]
                loose = "loose" in opts
                if loose:
                    opts.remove("loose")
                if len(words) < 5 or len(opts) not in (0, 2) or (opts and opts[0] != "within"):
                    errors.append("%s:%d: dispatch <cvar> <file> <function> <variable> [within A-B] [loose]" % (REGISTRY, n))
                    continue
                within = parse_numbers(opts[1]) if opts else None
                dispatchers.append((words[1], words[2], words[3], words[4], within, n, loose))
                continue
            if len(words) < 3:
                errors.append("%s:%d: <cvar> <number|A-B|A+> <file> <what it tests>" % (REGISTRY, n))
                continue
            # (N*: the dispatcher's fall-through, e.g. `if(req) { ... }` past its numbered branches: no branch of its own)
            fallthrough = words[1].endswith("*")
            r = parse_numbers(words[1].rstrip("*"))
            if not r:
                errors.append("%s:%d: bad number %r" % (REGISTRY, n, words[1]))
                continue
            entries.setdefault(words[0], []).append((r[0], r[1], n, fallthrough))
    return dispatchers, entries


def strip_comments(text):
    out = []
    i = 0
    while i < len(text):
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = len(text) if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            k = len(text) if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:k]))
            i = k
        elif text[i] == '"':
            j = i + 1
            while j < len(text) and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append('"' + " " * (j - i - 1) + '"')
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def match_close(text, i, open_c, close_c):
    depth = 0
    for j in range(i, len(text)):
        if text[j] == open_c:
            depth += 1
        elif text[j] == close_c:
            depth -= 1
            if depth == 0:
                return j
    return -1


def function_body(path, name, errors):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = strip_comments(f.read())
    except OSError:
        errors.append("%s: dispatcher file %s not found" % (REGISTRY, path))
        return None
    m = re.search(r"\b" + re.escape(name) + r"\s*=\s*\{", text)
    if not m:
        errors.append("%s: dispatcher %s not found in %s" % (REGISTRY, name, path))
        return None
    start = m.end() - 1
    end = match_close(text, start, "{", "}")
    return text[start:end + 1], text[:start].count("\n") + 1


def condition_numbers(cond, var):
    """The numbers a condition takes if it tests only `var` against numbers (an || of ==, >=, <=, ranges);
    None if it is anything else (another variable in it: a refinement, not a dispatch)."""
    v = re.escape(var)
    rest = re.sub(r"\b" + v + r"\b", "", cond)
    if re.search(r"[A-Za-z_]", rest):
        return None
    out = []
    for alt in cond.split("||"):
        alt = alt.strip().strip("()").strip()
        lo, hi = -INF, INF
        ok = False
        for term in alt.split("&&"):
            term = term.strip().strip("()").strip()
            m = re.fullmatch(v + r"\s*(==|>=|<=|>|<)\s*(\d+)", term)
            if not m:
                return None
            op, k = m.group(1), int(m.group(2))
            ok = True
            if op == "==":
                lo, hi = max(lo, k), min(hi, k)
            elif op == ">=":
                lo = max(lo, k)
            elif op == ">":
                lo = max(lo, k + 1)
            elif op == "<=":
                hi = min(hi, k)
            else:
                hi = min(hi, k - 1)
        if not ok or lo > hi:
            return None
        out.append((max(lo, 0), hi))
    return out


def branches(body, var):
    """Each `if(...)` testing only `var`: (ranges, start, end of its block, line offset)."""
    found = []
    for m in re.finditer(r"\bif\s*\(", body):
        c0 = m.end() - 1
        c1 = match_close(body, c0, "(", ")")
        if c1 < 0:
            continue
        nums = condition_numbers(body[c0 + 1:c1], var)
        if nums is None:
            continue
        j = c1 + 1
        while j < len(body) and body[j].isspace():
            j += 1
        end = match_close(body, j, "{", "}") if j < len(body) and body[j] == "{" else body.find(";", j)
        found.append((nums, m.start(), end, body[:m.start()].count("\n")))
    # (a branch inside another's block refines it: not a dispatch of its own)
    top = [b for b in found if not any(o is not b and o[1] < b[1] and b[2] <= o[2] for o in found)]
    return top


def calls_in(body, start, end):
    return set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", body[start:end + 1]))


def overlap(a, b):
    return a[0] <= b[1] and b[0] <= a[1]


def main():
    os.chdir(os.environ.get("QVR_TREE", "."))
    errors = []
    dispatchers, entries = read_registry(errors)
    subs_of = {}  # cvar -> its dispatch functions
    for d in dispatchers:
        subs_of.setdefault(d[0], set()).add(d[2])
    leaves = {}  # cvar -> [(lo, hi, where)]
    for cvar, path, func, var, within, n, loose in dispatchers:
        got = function_body(path, func, errors)
        if not got:
            continue
        body, line0 = got
        for nums, start, end, dl in branches(body, var):
            where = "%s:%d (%s)" % (path, line0 + dl, func)
            if calls_in(body, start, end) & (subs_of[cvar] - {func}):
                continue  # (it hands these numbers to a sub-dispatcher of its own registry line)
            for lo, hi in nums:
                if lo == 0 and hi == 0:
                    continue  # (0: no test)
                if within and not (within[0] <= lo and hi <= within[1]):
                    errors.append("%s: %s %s outside its dispatcher's range %s" % (where, cvar, fmt(lo, hi), fmt(*within)))
                leaves.setdefault(cvar, []).append((lo, hi, where, func if loose else None))
    if "--list" in sys.argv:
        for cvar in sorted(leaves):
            print(cvar + ": " + " ".join(sorted({fmt(lo, hi) for lo, hi, _, _ in leaves[cvar]}, key=lambda s: int(re.match(r"\d+", s).group()))))
        return 0
    # Dispatch clashes (a `loose` dispatcher's own branches may share numbers: set-up branches, then the cases).
    for cvar, ls in leaves.items():
        for i in range(len(ls)):
            for j in range(i + 1, len(ls)):
                if ls[i][3] and ls[i][3] == ls[j][3]:
                    continue
                if overlap(ls[i], ls[j]):
                    errors.append("%s test number clash: %s takes %s and %s takes %s" % (
                        cvar, ls[i][2], fmt(ls[i][0], ls[i][1]), ls[j][2], fmt(ls[j][0], ls[j][1])))
    # Registry clashes, coverage both ways.
    for cvar, es in entries.items():
        for i in range(len(es)):
            for j in range(i + 1, len(es)):
                if overlap(es[i], es[j]):
                    errors.append("%s:%d and :%d both claim %s %s" % (REGISTRY, es[i][2], es[j][2], cvar,
                                                                     fmt(max(es[i][0], es[j][0]), min(es[i][1], es[j][1]))))
        for lo, hi, n, fallthrough in es:
            if not fallthrough and not any(overlap((lo, hi), l) for l in leaves.get(cvar, [])):
                errors.append("%s:%d: %s %s is registered but no dispatcher runs it" % (REGISTRY, n, cvar, fmt(lo, hi)))
    def covered(cvar, lo, hi):
        # (lo..hi within the union of the cvar's registry lines)
        pos = lo
        while pos <= hi:
            e = next((e for e in entries.get(cvar, []) if e[0] <= pos <= e[1]), None)
            if not e:
                return False
            if e[1] >= INF:
                return True
            pos = e[1] + 1
        return True

    for cvar, ls in leaves.items():
        for lo, hi, where, _ in ls:
            if not covered(cvar, lo, hi):
                errors.append("%s: %s %s is not in %s (claim it there)" % (where, cvar, fmt(lo, hi), REGISTRY))
    # The menus' commands.
    cvars = set(entries) | set(leaves)
    for pattern in MENU_GLOBS:
        for path in sorted(glob.glob(pattern)):
            with open(path, encoding="utf-8", errors="replace") as f:
                for n, line in enumerate(f, 1):
                    for cmd in re.findall(r'\bcommand\(\s*"(?:[^"\\]|\\.)*"\s*,\s*"((?:[^"\\]|\\.)*)"', line):
                        for cvar, num in re.findall(r"\b(vr_\w+_test)\s+(\d+)", cmd):
                            k = int(num)
                            if cvar in cvars and k != 0 and not any(e[0] <= k <= e[1] for e in entries.get(cvar, [])):
                                errors.append("%s:%d: the menu runs %s %d: not in %s" % (path.replace("\\", "/"), n, cvar, k, REGISTRY))
    for e in errors:
        print("test numbers: " + e)
    total = sum(len(v) for v in entries.values())
    print("test numbers: %d registered, %d dispatch branches, %d problems" % (total, sum(len(v) for v in leaves.values()), len(errors)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

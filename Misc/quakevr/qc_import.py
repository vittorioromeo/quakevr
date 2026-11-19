#!/usr/bin/env python3
"""The QuakeC importer's common part: a mod's QuakeC into Quake VR's progs, every name it defines prefixed, inert
until a ruleset row (Quake/vr/vr_rules.cpp) or a bridge calls it. Used by ad_import.py (Arcane Dimensions, "ad_",
QC/ad/) and copper_import.py (Copper, "cu_", QC/copper/); each supplies its sources (patched) and a Mod (below).
Not run directly.

Renaming: every identifier token outside strings, comments and $frame directives becomes <prefix><name>, except
  - QC keywords and types, compiler intrinsics (__*), vector components (x, y, z);
  - the engine's system globals and fields (Quake VR's defs.qc before end_sys_fields) and their _x/_y/_z parts;
  - builtins Quake VR's QC already declares with the same signature (the mod's own declarations are dropped);
  - the mod's fields that are map keys (Mod.keys) and that Quake VR already has with the same type: one field, as the
    map sets it;
  - the names the mod's reviewed patches call (Mod.keep_prefixes, Mod.qvr_names).
The mod's system section (its defs file up to end_sys_fields) and its builtin declarations are not copied; its header
files (Mod.headers) are filtered to what the imported code uses; with Mod.drop_entry_points, the definitions of the
engine's entry points its system section declares (StartFrame, PlayerPreThink, ClientConnect...) are left out unless
other imported code calls them. Preprocessor symbols in Mod.defined are resolved (#ifdef/#ifndef blocks on them).
"""
import difflib
import hashlib
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
QC_DIR = os.path.join(ROOT, "QC")
# Every import's folder under QC/, in progs.src order (each import's block follows the previous one's): a mod's
# import sees Quake VR's QC and the imports before it, never its own folder or a later one's.
IMPORT_ORDER = ["ad", "copper"]


class Mod:
    """One imported mod's configuration."""

    def __init__(self, **kw):
        self.name = ""             # "Arcane Dimensions"
        self.short = ""            # "AD", in the report
        self.script = ""           # "ad_import.py": its progs.src markers and messages
        self.folder = ""           # QC/<folder>
        self.prefix = ""           # every name it defines: <prefix><name>; also its files' prefix
        self.skip = set()          # progs.src files not imported
        self.headers = set()       # declarations only, filtered to what the code uses
        self.defs = "defs.qc"      # the file whose first section (to end_sys_fields) is the engine's
        self.keep_prefixes = ()    # names the reviewed patches call (the bridge's): never renamed
        self.qvr_names = set()     # Quake VR's names the patches call: never renamed
        self.own_names = set()     # Quake VR's names the patches define in the mod's code: never renamed
        self.keys = set()          # the map keys its fields take (a map-key field Quake VR has with its type stays bare)
        self.defined = set()       # preprocessor symbols taken as defined (#ifdef/#ifndef resolved)
        self.drop_entry_points = False
        self.rename_defines = False  # a #define/#undef line's names are the mod's too (renamed)
        self.key_fill = ""         # the report's note on who fills its renamed map-key fields
        self.source_name = lambda fn: fn  # a file's source path, in its banner
        self.banner = ""           # the generated files' header, with %s for the source file
        self.builtins_text = ""    # what follows the banner in <prefix>builtins.qc, before the declarations
        self.report_name = ""      # "AD_IMPORT.txt"
        self.report_head = []      # its first lines
        self.write_dir = None      # where the files go (QC/<folder>, or --check's folder)
        self.__dict__.update(kw)

    @property
    def out_dir(self):
        return os.path.join(QC_DIR, self.folder)

    @property
    def begin(self):
        return "// %s: begin (generated list; Misc/quakevr/%s rewrites it)" % (self.script, self.script)

    @property
    def end(self):
        return "// %s: end" % self.script

    def die(self, msg):
        sys.exit("%s: %s" % (self.script[:-3], msg))


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
RAW = re.compile(r"[ \t]*(\$(?:%s)\b|#)[^\n]*" % "|".join(DIRECTIVES))


def tokenize(text):
    """[(kind, text)]; a $directive line and a #preprocessor line are one 'raw' token."""
    out = []
    i, n = 0, len(text)
    line_start = True
    while i < n:
        if line_start:
            m = RAW.match(text, i)
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


def preprocess(text, defined):
    """`text` with the #ifdef/#ifndef blocks on `defined` symbols resolved (other conditionals left as written);
    returns (text, number of blocks resolved)."""
    if not defined:
        return text, 0
    out, stack, resolved = [], [], 0  # stack: None (another conditional) or [taken, in_else]
    for line in text.split("\n"):
        m = re.match(r"\s*#\s*(ifdef|ifndef|if|else|endif)\b\s*(\w*)", line)
        live = all(s is None or s[0] != s[1] for s in stack)
        if m and m.group(1) in ("ifdef", "ifndef") and m.group(2) in defined:
            stack.append([(m.group(1) == "ifdef"), False])
            resolved += 1
            continue
        if m and m.group(1) in ("ifdef", "ifndef", "if"):
            stack.append(None)
        elif m and m.group(1) == "else" and stack and stack[-1] is not None:
            stack[-1][1] = True
            continue
        elif m and m.group(1) == "endif" and stack:
            if stack.pop() is not None:
                continue
        if live:
            out.append(line)
    return "\n".join(out), resolved


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


def bsp_keys(data):
    """The keys a BSP's entity lump sets."""
    eo, el = struct.unpack("<ii", data[4:12])
    return set(re.findall(r'"([A-Za-z_]\w*)"\s*"', data[eo:eo + el].decode("latin1")))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


SIGS = {}  # Quake VR's function and builtin signatures, by name (ours())


def ours(exclude):
    """Quake VR's QC (QC/ and its subfolders but the `exclude` folders): system names, fields with types, every
    top-level name, builtin names, #define names, function names."""
    system, fields, names, builtins, macros = set(), {}, set(), {}, set()
    funcs = set()
    SIGS.clear()
    skip = [os.path.normpath(os.path.join(QC_DIR, d)) for d in exclude]
    for root, dirs, files in os.walk(QC_DIR):
        if any(os.path.normpath(root).startswith(s) for s in skip):
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
                    # The engine's entry points (main, StartFrame, PlayerPreThink...) are Quake VR's: a mod's are
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


def run(mod, src):
    """Imports `src` ({file name: text}, patched; its progs.src included) as `mod`: writes mod.write_dir's files and
    the progs.src block; returns (report lines, facts) for the caller's report and summary."""
    order = []
    for line in src["progs.src"].splitlines()[1:]:
        line = line.split("//")[0].strip()
        if line.endswith(".qc") and line not in mod.skip:
            order.append(line)
    resolved = 0
    for fn in order:
        src[fn], n = preprocess(src[fn], mod.defined)
        resolved += n

    later = IMPORT_ORDER[IMPORT_ORDER.index(mod.folder):]
    system, our_fields, our_names, our_builtins, our_macros, our_funcs = ours(later)
    if mod.qvr_names - our_names:
        mod.die("the patches call names Quake VR's QC does not define: " + ", ".join(sorted(mod.qvr_names - our_names)))
    our_numbers = {num: n for n, num in our_builtins.items() if num}
    keys = mod.keys

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

    # The mod's own system section (its defs file up to end_sys_fields) is the engine's: Quake VR's defines it.
    mod_sys, entry_points, mod_fields, mod_builtin_stmts, aliases = set(), set(), {}, {}, set()
    skip_stmt = {fn: set() for fn in order}
    in_sys = True
    for k, st in enumerate(stmts[mod.defs]):
        ns, is_field, typ, is_builtin = declared(toks[mod.defs], st)
        if in_sys:
            mod_sys.update(ns)
            if "(" in typ and not is_field and not is_builtin:
                entry_points.update(ns)
            skip_stmt[mod.defs].add(k)
            if "end_sys_fields" in ns:
                in_sys = False
    for fn in order:
        for k, st in enumerate(stmts[fn]):
            ns, is_field, typ, is_builtin = declared(toks[fn], st)
            if is_builtin:
                skip_stmt[fn].add(k)
                for n in ns:
                    mod_builtin_stmts.setdefault(n, (fn, st))
            elif is_field and k not in skip_stmt[fn]:
                for n in ns:
                    mod_fields.setdefault(n, (typ, fn))
                if "=" in (toks[fn][j][1] for j in st):
                    aliases.update(ns)  # (`.float t_length = length;`: another field's name, never Quake VR's field)

    # What stays as written.
    keep = set(KEYWORDS) | system
    # A builtin Quake VR declares, or wraps in a QC function of the same name (frikbot's sound, stuffcmd...).
    # Only with the same signature: AD's strcat takes up to 8 strings, Quake VR's declaration 2 (the builtin is the
    # same): such a declaration is kept as <prefix>strcat with the builtin's number.
    keep_builtins = {n for n in mod_builtin_stmts if (n in our_builtins or n in our_funcs)
                     and SIGS.get(n) == signature(toks[mod_builtin_stmts[n][0]], mod_builtin_stmts[n][1])}
    keep |= keep_builtins
    shared_fields = sorted(n for n, (typ, fn) in mod_fields.items()
                           if n in keys and n in our_fields and our_fields[n] == typ and n not in our_macros and
                           n not in aliases)
    keep |= set(shared_fields)

    def rename(name):
        if (name in keep or name.startswith("__") or name.startswith(mod.keep_prefixes) or name in mod.qvr_names or
                name in mod.own_names):
            return name
        m = re.match(r"(\w+)_[xyz]$", name)
        if m and m.group(1) in keep:
            return name
        return mod.prefix + name

    # The engine's entry points' definitions (drop_entry_points): left out unless the rest calls them.
    prunable = {fn: set() for fn in order}
    if mod.drop_entry_points:
        for fn in order:
            if fn in mod.headers:
                continue
            for k, st in enumerate(stmts[fn]):
                ns, is_field, typ, _ = declared(toks[fn], st)
                if not is_field and set(ns) & entry_points and k not in skip_stmt[fn]:
                    prunable[fn].add(k)

    # Headers: only the statements defining what the imported code uses (to a fixed point); prunable statements the
    # same way.
    body_ids = set()
    for fn in order:
        if fn in mod.headers:
            continue
        for k, st in enumerate(stmts[fn]):
            if k not in prunable[fn]:
                body_ids |= ids_in(toks[fn], st)
    needed = set(body_ids)
    kept = {fn: set() for fn in order if fn in mod.headers}
    kept_pruned = {fn: set() for fn in order}
    changed = True
    while changed:
        changed = False
        for fn in order:
            if fn in kept:
                cands, have = (k for k in range(len(stmts[fn])) if k not in skip_stmt[fn]), kept[fn]
            else:
                cands, have = iter(sorted(prunable[fn])), kept_pruned[fn]
            for k in cands:
                if k in have:
                    continue
                ns = declared(toks[fn], stmts[fn][k])[0]
                if set(ns) & needed:
                    have.add(k)
                    needed |= ids_in(toks[fn], stmts[fn][k])
                    changed = True
    dropped_entries = sorted({n for fn in order for k in prunable[fn] - kept_pruned[fn]
                              for n in declared(toks[fn], stmts[fn][k])[0]})

    # Builtins the mod uses that Quake VR's QC does not declare: by name (#0).
    # A standard builtin under another name (AD's walkmove_builtin = #32) keeps its number, as <prefix><name>.
    bi_out, bi_renamed, bi_numbered, bi_byname = [], [], [], []
    for n in sorted(mod_builtin_stmts):
        if n in keep_builtins or n not in needed:
            continue
        fn, st = mod_builtin_stmts[n]
        num = builtin_number(toks[fn], st)
        if n in our_builtins and our_builtins[n]:
            num = our_builtins[n]
            bi_numbered.append("%s=#%d (another signature than Quake VR's: %s / %s)" % (
                n, num, signature(toks[fn], st), SIGS.get(n)))
            bi_out.append(emit(toks[fn], st, rename, mod, builtin_name=mod.prefix + n, number=num))
            continue
        if num in our_numbers:
            bi_numbered.append("%s=#%d (Quake VR's %s)" % (n, num, our_numbers[num]))
            bi_out.append(emit(toks[fn], st, rename, mod, builtin_name=mod.prefix + n, number=num))
            continue
        clash = n in our_names or n in our_macros
        if clash:
            bi_renamed.append(n)
        else:
            keep.add(n)
            bi_byname.append(n)
        bi_out.append(emit(toks[fn], st, rename, mod, builtin_name=(mod.prefix + n if clash else n)))

    out_dir = mod.write_dir or mod.out_dir
    os.makedirs(out_dir, exist_ok=True)
    written = []
    for fn in order:
        t = toks[fn]
        if fn in mod.headers:
            body = "\n".join(emit(t, st, rename, mod) for k, st in enumerate(stmts[fn]) if k in kept[fn]) + "\n"
        else:
            drop = set()
            for k in skip_stmt[fn] | (prunable[fn] - kept_pruned[fn]):
                drop.update(range(stmts[fn][k][0], stmts[fn][k][-1] + 1))
            body = "".join(render(t, j, rename, mod) for j in range(len(t)) if j not in drop)
        out = mod.prefix + fn
        with open(os.path.join(out_dir, out), "w", encoding="latin1", newline="\n") as f:
            f.write(mod.banner % (mod.source_name(fn)) + body)
        written.append(out)
    builtins_file = mod.prefix + "builtins.qc"
    with open(os.path.join(out_dir, builtins_file), "w", encoding="latin1", newline="\n") as f:
        f.write(mod.banner % "extension builtins" + mod.builtins_text + "\n".join(bi_out) + "\n")
    # progs.src lists each file (fteqcc numbers $frame macros from 0 in each progs.src file, not in an #include).
    update_progs_src(mod, [mod.folder + "/" + fn for fn in [builtins_file] + written])

    renamed_keys = sorted(n for n, (typ, fn) in mod_fields.items() if n in keys and n not in shared_fields)
    clash_type = sorted("%s (%s %s, Quake VR %s)" % (n, mod.short, mod_fields[n][0], our_fields[n]) for n in renamed_keys
                        if n in our_fields)
    sys_missing = sorted(mod_sys - system - {"end_sys_globals", "end_sys_fields"})
    S = mod.short
    report = mod.report_head + [
        "Files (%s's progs.src order): %d imported, skipped %s; headers filtered: %s" % (
            S, len(written), ", ".join(sorted(mod.skip)), ", ".join(sorted(mod.headers))),
        "Header statements kept: " + ", ".join("%s %d/%d" % (fn, len(kept[fn]), len(stmts[fn])) for fn in sorted(kept)),
        "%s system names not kept (the engine's entry points: %s's are renamed, the engine never calls them): " % (S, S) +
        (", ".join(sys_missing) or "none"),
    ]
    if mod.drop_entry_points:
        report.append("Entry points left out (their definitions; no imported code calls them): " +
                      (", ".join(dropped_entries) or "none"))
    if mod.defined:
        report.append("Preprocessor symbols taken as defined: %s (%d blocks resolved)" % (
            ", ".join(sorted(mod.defined)), resolved))
    report += [
        "Builtins kept (Quake VR declares them): %d" % len(keep_builtins),
        "Builtins by name (#0, %s): %d: %s" % (builtins_file, len(bi_byname), ", ".join(bi_byname)),
        "Builtins renamed (the name is Quake VR's for something else): " + (", ".join(bi_renamed) or "none"),
        "Standard builtins under %s's own names (number kept, %s<name>): " % (S, mod.prefix) + (", ".join(bi_numbered) or "none"),
        "Map-key fields shared with Quake VR (same name and type, bare): %d: %s" % (len(shared_fields), ", ".join(shared_fields)),
        "Map-key fields renamed %s<key> (no Quake VR field, or another type; %s): %d" % (
            mod.prefix, mod.key_fill, len(renamed_keys)),
        "  of them a type clash: " + (", ".join(clash_type) or "none"),
        "  " + ", ".join(renamed_keys),
    ]
    return report, {"written": written, "kept": kept, "stmts": stmts}


def write_report(mod, report):
    out_dir = mod.write_dir or mod.out_dir
    with open(os.path.join(out_dir, mod.report_name), "w", encoding="latin1", newline="\n") as f:
        f.write("\n".join(report) + "\n")
    if mod.write_dir and os.path.normpath(mod.write_dir) != os.path.normpath(mod.out_dir):
        sys.exit(check(mod))


def update_progs_src(mod, files):
    """Writes the mod's files between its markers in QC/progs.src (the markers are added at the end the first time)."""
    path = os.path.join(QC_DIR, "progs.src")
    with open(path, encoding="latin1", newline="") as f:
        text = f.read()
    nl = "\r\n" if "\r\n" in text else "\n"
    block = nl.join([mod.begin] + files + [mod.end])
    if mod.begin in text:
        a = text.index(mod.begin)
        b = text.index(mod.end, a) + len(mod.end)
        text = text[:a] + block + text[b:]
    else:
        text = text.rstrip("\r\n") + nl + block + nl
    if mod.write_dir and os.path.normpath(mod.write_dir) != os.path.normpath(mod.out_dir):
        path = os.path.join(mod.write_dir, "progs.src")  # --check: the list next to the files, compared by check()
    with open(path, "w", encoding="latin1", newline="") as f:
        f.write(text)


def check(mod):
    """--check: the files just written to mod.write_dir against QC/<folder> (and progs.src); prints a unified diff of
    each difference, returns the exit code (0 when the tree is exactly what the importer makes)."""
    names = sorted(set(os.listdir(mod.write_dir)) | set(os.listdir(mod.out_dir)))
    bad = 0
    for fn in names:
        ours_path = os.path.join(QC_DIR, fn) if fn == "progs.src" else os.path.join(mod.out_dir, fn)
        gen_path = os.path.join(mod.write_dir, fn)
        a = open(ours_path, encoding="latin1").read().splitlines() if os.path.exists(ours_path) else []
        b = open(gen_path, encoding="latin1").read().splitlines() if os.path.exists(gen_path) else []
        if a != b:
            bad += 1
            sys.stdout.writelines(l + "\n" for l in difflib.unified_diff(
                a, b, "tree/" + fn, "importer/" + fn, lineterm="", n=1))
    print("%s --check: %d of %d files differ from the importer's output (%s)" % (
        mod.script[:-3], bad, len(names), mod.write_dir))
    return 1 if bad else 0


def check_dir(mod, folder):
    """--check's folder: outside the mod's own."""
    folder = os.path.abspath(folder)
    if os.path.normpath(folder).startswith(os.path.normpath(mod.out_dir)):
        mod.die("--check's folder must be outside QC/" + mod.folder)
    return folder


def render(tokens, j, rename, mod):
    kind, text = tokens[j]
    if kind == "id":
        return rename(text)
    if kind == "bc" and text.startswith("/*QUAKED"):
        # (TrenchBroom's generator reads these as the mod's, not Quake VR's: fgdgen.py)
        return "/*%s:QUAKED" % mod.prefix[:-1] + text[len("/*QUAKED"):]
    if kind == "raw" and mod.rename_defines:
        m = re.match(r"([ \t]*#[ \t]*(?:define|undef)\b)(.*)$", text, re.S)
        if m:  # a macro's name and body are the mod's names too
            return m.group(1) + re.sub(r"[A-Za-z_]\w*", lambda i: rename(i.group(0)), m.group(2))
    return text


def emit(tokens, stmt, rename, mod, builtin_name=None, number=0):
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
            parts.append(render(tokens, j, rename, mod))
        j += 1
    text = "".join(parts)
    is_function = "(" in declared(tokens, stmt)[2]
    if tokens[last][1] != ";" and (tokens[last][1] != "}" or not is_function):
        text += ";"  # (an array's initializer, `= { ... }`, ends with the brace: its semicolon was its own statement)
    return text

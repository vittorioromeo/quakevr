#!/usr/bin/env python3
"""Generates Quake VR's TrenchBroom entity definitions (Misc/trenchbroom/QuakeVR/quakevr.fgd) from the QuakeC.

    python Misc/trenchbroom/fgdgen.py                  write quakevr.fgd
    python Misc/trenchbroom/fgdgen.py --check          fail (exit 1) if a QC spawn function has no entry, or an
                                                       entry has no QC spawn function; warn if the file is stale
    python Misc/trenchbroom/fgdgen.py --report         per entity: the keys it reads that no definition documents
    python Misc/trenchbroom/fgdgen.py --verify-models <Quake folder>
                                                       every model() path must exist in id1, hipnotic, rogue or
                                                       quakevr (loose files or pak files)

Three sources, merged in this order (a later one wins, key by key):

1. The standard definitions: vendor/Quake.fgd (TrenchBroom's Quake.fgd, unchanged) for the id entities and its
   base classes (Appearflags, Targetname, Target, Item, Monster, Light...).
2. The QuakeC (QC/progs.src, in its order): every map entity's spawn function, with
   - its /*QUAKED name (r g b) (mins) (maxs) FLAG1 FLAG2 ... help */ comment when it has one (colour, size, point or
     brush, spawnflag names, help text);
   - the spawnflag bits its body tests (self.spawnflags & NAME, NAME resolved from the QC's constants);
   - the keys it reads (self.field), for the fields that the glossary (_Fields in entities.fgd) documents;
   - the model it sets (setmodel(self, "...") or the first model it precaches), as the model() preview.
3. entities.fgd, hand written: help text, key types and choices, sizes, colours and model() previews, the new
   Quake VR entities, the ericw-tools keys. A class there is merged into the generated one: its header items
   (base, color, size, model) and its description replace the generated ones, its properties replace those of the
   same name (the others are kept), its spawnflags replace the bits it lists. Directives in comments:
     //! internal <function> <reason>     a QC function the heuristics take for a spawn function but is not one
     //! drop <class> <property>           remove a generated property
     //! editor <class>                    a class with no spawn function (compiler or editor entity: func_group...)
     //! nomodel <class> <reason>          no model() preview (the model the QC sets is not shipped)

A spawn function is a `void()` function, not a frame function, that has a QUAKED comment, or whose name has a map
entity's prefix (info_, item_, weapon_, monster_, func_, trigger_, light, misc_, path_, ambient_, trap_, vr_...), is
never used as a value (think/touch/use...) and has no helper suffix (_think, _touch, _use...).
"""

import argparse
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
QC_DIR = os.path.join(ROOT, "QC")
VENDOR_FGD = os.path.join(HERE, "vendor", "Quake.fgd")
SOURCE_FGD = os.path.join(HERE, "entities.fgd")
OUT_FGD = os.path.join(HERE, "QuakeVR", "quakevr.fgd")

ENTITY_PREFIX = re.compile(
    r"^(info_|item_|weapon_|monster_|func_|trigger_|light$|light_|misc_|path_|ambient_|air_|viewthing$|event_|"
    r"target_|trap_|worldspawn$|test|noclass$|effect_|vr_|random_|dmatch_|play_|wallsprite$|endcam_)")
HELPER_SUFFIX = re.compile(
    r"_(think|think\d*|touch|use|die|pain|precache|start|done|blocked|fire|do|enable|disable|toggle|kill|firstthink|"
    r"go|spawn|find|linkgroups|real|scan|first|think_first|muzak_start)$")

# Where an entity without a title comes from (its QC file's prefix).
ORIGINS = {
    "hip": "From the Scourge of Armagon (hipnotic): its models and sounds come from the hipnotic folder.",
    "rogue": "From the Dissolution of Eternity (rogue): its models and sounds come from the rogue folder.",
    "honey": "From Honey (czg): Quake VR does not ship Honey's own models and sounds (honey/...).",
}

# Where an entity without a title comes from (its QC file's prefix).
ORIGINS = {
    "hip": "From the Scourge of Armagon (hipnotic): its models and sounds come from the hipnotic folder.",
    "rogue": "From the Dissolution of Eternity (rogue): its models and sounds come from the rogue folder.",
    "honey": "From Honey (czg): Quake VR does not ship Honey's own models and sounds (honey/...).",
}

# Where each group goes in the file, and its title.
CATEGORIES = [
    ("worldspawn", "World"),
    ("info_", "Player starts and markers"),
    ("weapon_", "Weapons"),
    ("item_", "Items"),
    ("monster_", "Monsters"),
    ("light", "Lights"),
    ("func_", "Brush entities"),
    ("trigger_", "Triggers"),
    ("vr_", "Quake VR"),
    ("", "Other point entities"),
]


# ------------------------------------------------------------------------------------------------------------------
# FGD model, parser and writer
# ------------------------------------------------------------------------------------------------------------------

class Prop:
    def __init__(self, name, ptype, short="", default=None, long="", items=None):
        self.name = name
        self.ptype = ptype            # as written (string, integer, choices, flags, target_source...)
        self.short = short
        self.default = default        # raw token text (a number, or a quoted string) or None
        self.long = long
        self.items = items            # choices: [(value raw, label)]; flags: [(bit, label, default, long)]

    def kind(self):
        return self.ptype.lower()


class FgdClass:
    def __init__(self, kind, name):
        self.kind = kind              # PointClass, SolidClass, BaseClass
        self.name = name
        self.desc = ""
        self.header = {}              # base, color, size, model -> raw text inside the parentheses
        self.props = []
        self.origin = ""              # where it came from, for messages

    def prop(self, name):
        for p in self.props:
            if p.name.lower() == name.lower():
                return p
        return None

    def bases(self):
        b = self.header.get("base")
        return [x.strip() for x in b.split(",")] if b else []


class FgdError(Exception):
    pass


TOKEN = re.compile(r'\s+|//[^\n]*|"(?:[^"\\]|\\.)*"|-?\d+(?:\.\d+)?|[A-Za-z_][A-Za-z0-9_]*|@|[=:\[\](),+{}]|.', re.S)


def tokenize(text):
    toks = []
    for m in TOKEN.finditer(text):
        s = m.group(0)
        if s.isspace() or s.startswith("//"):
            continue
        toks.append((s, m.start()))
    return toks


def unquote(s):
    return s[1:-1]


class _Parser:
    def __init__(self, text, path):
        self.text, self.path = text, path
        self.toks = tokenize(text)
        self.i = 0

    def err(self, msg):
        pos = self.toks[self.i][1] if self.i < len(self.toks) else len(self.text)
        raise FgdError("%s:%d: %s" % (self.path, self.text.count("\n", 0, pos) + 1, msg))

    def peek(self, k=0):
        j = self.i + k
        return self.toks[j][0] if j < len(self.toks) else None

    def next(self):
        t = self.peek()
        if t is None:
            self.err("unexpected end of file")
        self.i += 1
        return t

    def expect(self, s):
        t = self.next()
        if t.lower() != s.lower():
            self.i -= 1
            self.err("expected %r, got %r" % (s, t))

    def string(self):
        t = self.next()
        if not t.startswith('"'):
            self.i -= 1
            self.err("expected a string, got %r" % t)
        s = unquote(t)
        while self.peek() == "+" and (self.peek(1) or "").startswith('"'):
            self.i += 1
            s += unquote(self.next())
        return s

    def raw_parens(self):
        start = self.toks[self.i][1]
        depth, j, text = 0, start, self.text
        while j < len(text):
            c = text[j]
            if c == '"':
                j = text.index('"', j + 1)
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        while self.i < len(self.toks) and self.toks[self.i][1] <= j:
            self.i += 1
        return " ".join(text[start + 1:j].split()) if "\n" not in text[start + 1:j] else text[start + 1:j].strip()

    def value(self):
        """A default or a choice's value: a number or a string (kept raw), or None if absent."""
        t = self.peek()
        if t is None:
            return None
        if t.startswith('"') or re.match(r"-?\d", t):
            self.i += 1
            return t
        return None

    def prop(self):
        name = self.next()
        self.expect("(")
        ptype = self.next()
        self.expect(")")
        p = Prop(name, ptype)
        # Some files write "spawnflags(flags) = [...]" without the descriptions.
        if self.peek() == ":":
            self.i += 1
            if (self.peek() or "").startswith('"'):
                p.short = self.string()
            if self.peek() == ":":
                self.i += 1
                p.default = self.value()
                if self.peek() == ":":
                    self.i += 1
                    p.long = self.string()
        if self.peek() == "=":
            self.i += 1
            self.expect("[")
            p.items = []
            while self.peek() != "]":
                v = self.next()
                self.expect(":")
                label = self.string()
                if p.kind() == "flags":
                    d, long = "0", ""
                    if self.peek() == ":":
                        self.i += 1
                        d = self.value() or "0"
                        if self.peek() == ":":
                            self.i += 1
                            long = self.string()
                    p.items.append((int(v), label, d, long))
                else:
                    long = ""
                    if self.peek() == ":":
                        self.i += 1
                        long = self.string()
                    p.items.append((v, label, long))
            self.i += 1
        return p

    def classes(self):
        out = []
        while self.peek() is not None:
            self.expect("@")
            kt = self.next()
            kind = {"pointclass": "PointClass", "solidclass": "SolidClass", "baseclass": "BaseClass"}.get(kt.lower())
            if kind is None:
                self.i -= 1
                self.err("unknown @%s" % kt)
            header = {}
            while self.peek() != "=":
                key = self.next().lower()
                if self.peek() != "(":
                    self.err("expected '(' after %s" % key)
                header[key] = self.raw_parens()
            self.expect("=")
            c = FgdClass(kind, self.next())
            c.header, c.origin = header, self.path
            if self.peek() == ":":
                self.i += 1
                c.desc = self.string()
            if self.peek() == "[":
                self.i += 1
                while self.peek() != "]":
                    c.props.append(self.prop())
                self.i += 1
            out.append(c)
        return out


def read_fgd(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    return _Parser(text, os.path.relpath(path, ROOT)).classes(), text


def q(s):
    """A string literal. FGD has no escapes: a double quote becomes a single one."""
    return '"%s"' % s.replace('"', "'")


def write_prop(p, out):
    line = "\t%s(%s)" % (p.name, p.ptype)
    if p.short or p.default is not None or p.long:
        line += " : " + q(p.short)
        if p.default is not None or p.long:
            line += " : " + (p.default if p.default is not None else "")
            if p.long:
                line += " : " + q(p.long)
    if p.items is not None:
        out.append(line + " =")
        out.append("\t[")
        for it in p.items:
            if p.kind() == "flags":
                bit, label, d, long = it
                out.append("\t\t%d : %s : %s%s" % (bit, q(label), d, (" : " + q(long)) if long else ""))
            else:
                v, label, long = it
                out.append("\t\t%s : %s%s" % (v, q(label), (" : " + q(long)) if long else ""))
        out.append("\t]")
    else:
        out.append(line)


def write_class(c, out):
    head = "@%s" % c.kind
    for key in ("base", "color", "size", "model", "decal", "sprite"):
        if key in c.header:
            v = c.header[key]
            if key == "model" and "\n" in v:
                v = "\n\t" + v.replace("\n", "\n\t") + "\n"
            head += " %s(%s)" % (key, v)
    out.append(head + " =")
    out.append("\t%s%s" % (c.name, (" : " + q(c.desc)) if c.desc else ""))
    out.append("[")
    for p in c.props:
        write_prop(p, out)
    out.append("]")
    out.append("")


# ------------------------------------------------------------------------------------------------------------------
# QuakeC
# ------------------------------------------------------------------------------------------------------------------

class QcEntity:
    def __init__(self, name, file):
        self.name = name
        self.file = file
        self.body = ""
        self.quaked = None            # dict: color, size (None: brush), flags [names], text
        self.flag_bits = {}           # bit -> constant name (or "") the body tests
        self.reads = []               # fields read, in order
        self.model = None             # model path it sets


def strip_comments(text):
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            out.append("\n" * text.count("\n", i, j))
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def qc_files():
    with open(os.path.join(QC_DIR, "progs.src"), encoding="latin1") as f:
        src = f.read()
    files = []
    for line in src.splitlines()[1:]:
        line = line.split("//")[0].strip()
        if line.endswith(".qc"):
            files.append(line)
    return files


FUNC_RE = re.compile(r"\b(?:void|float|string|entity|vector)\s*\(([^()]*)\)\s*([A-Za-z_]\w*)\s*(=\s*)?(\[[^\]]*\]\s*)?\{")
QUAKED_RE = re.compile(r"/\*QUAKED\s+([A-Za-z_]\w*)\s*(.*?)\*/", re.S)
CONST_RE = re.compile(r"\bfloat\s+([A-Za-z_]\w*)\s*=\s*(-?\d+(?:\.\d+)?|VRUTIL_POWER_OF_TWO\s*\(\s*(\d+)\s*\))\s*;")


def parse_quaked(rest):
    """(color, size or None for a brush, [flag names], text) from what follows 'QUAKED name'."""
    first, _, text = rest.partition("\n")
    m = re.match(r"\s*\(([^)]*)\)\s*(\?|\(([^)]*)\)\s*\(([^)]*)\))?\s*(.*)$", first)
    color = size = None
    flags = []
    if m:
        color = [float(x) for x in m.group(1).split()] if m.group(1).strip() else None
        brush = m.group(2) == "?" or bool(m.group(2) and "?" in m.group(2))
        if m.group(2) and not brush:
            size = (m.group(3).split(), m.group(4).split())
        flags = m.group(5).split()
    else:
        brush = False
    return {"color": color, "size": size, "brush": brush, "flags": flags, "text": text}


def load_qc():
    consts = {}
    funcs = {}                        # name -> (file, args, body, is_frame)
    assigned = set()
    quaked = {}
    for f in qc_files():
        with open(os.path.join(QC_DIR, f), encoding="latin1") as fh:
            raw = fh.read()
        for m in QUAKED_RE.finditer(raw):
            quaked.setdefault(m.group(1), (f, parse_quaked(m.group(2))))
        code = strip_comments(raw)
        for m in CONST_RE.finditer(code):
            v = int(m.group(3)) if m.group(3) else m.group(2)
            consts[m.group(1)] = (1 << v) if m.group(3) else float(v)
        for m in re.finditer(r"=\s*([A-Za-z_]\w*)\s*;", code):
            assigned.add(m.group(1))
        for m in re.finditer(r"[,(]\s*([A-Za-z_]\w*)\s*[,)]", code):
            assigned.add(m.group(1))          # passed as a function value (SUB_CalcMove's callback...)
        for m in FUNC_RE.finditer(code):
            start = m.end() - 1
            depth, j = 0, start
            while j < len(code):
                if code[j] == '"':
                    j = code.index('"', j + 1)
                elif code[j] == "{":
                    depth += 1
                elif code[j] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            name = m.group(2)
            if name not in funcs:
                funcs[name] = (f, m.group(1).strip(), code[start:j + 1], bool(m.group(4)))
    return consts, funcs, assigned, quaked


def model_of(body, funcs):
    m = re.search(r'setmodel\s*\(\s*self\s*,\s*"([^"]+\.(?:mdl|bsp|spr))"', body)
    if m:
        return m.group(1)
    m = re.search(r'\b(impl_\w+|\w*_spawn\w*)\s*\(\s*"([^"]+\.(?:mdl|bsp|spr))"', body)
    if m:
        return m.group(2)
    for m in re.finditer(r'precache_model2?\s*\(\s*"([^"]+\.(?:mdl|bsp|spr))"', body):
        base = os.path.basename(m.group(1))
        if not re.match(r"(h_|gib|zom_gib|lavaball|s_|k_spike|spike|laser|missile|grenade|bolt|v_|w_spike)", base):
            return m.group(1)
    return None


def qc_entities(internal):
    consts, funcs, assigned, quaked = load_qc()
    names = []
    for name, (f, args, body, frame) in funcs.items():
        if args or frame or name in internal:
            continue
        if name in quaked:
            names.append(name)
        elif ENTITY_PREFIX.match(name) and name not in assigned and not HELPER_SUFFIX.search(name) \
                and not name.startswith("player_"):
            names.append(name)
    ents = {}
    for name in names:
        f, args, body, frame = funcs[name]
        e = QcEntity(name, f)
        e.body = body
        if name in quaked:
            e.quaked = quaked[name][1]
        for m in re.finditer(r"self\.spawnflags\s*&\s*\(?\s*([A-Za-z_]\w*|\d+)", body):
            tok = m.group(1)
            v = int(tok) if tok.isdigit() else consts.get(tok)
            if isinstance(v, float) and v.is_integer():
                v = int(v)
            if isinstance(v, int) and v > 0 and (v & (v - 1)) == 0:
                e.flag_bits.setdefault(v, "" if tok.isdigit() else tok)
        for m in re.finditer(r"\bself\.([A-Za-z_]\w*)\b(?!\s*=[^=])", body):
            if m.group(1) not in e.reads:
                e.reads.append(m.group(1))
        e.model = model_of(body, funcs)
        ents[name] = e
    return ents, quaked, funcs


# ------------------------------------------------------------------------------------------------------------------
# Merging
# ------------------------------------------------------------------------------------------------------------------

def reflow(text):
    """A QUAKED comment's text as paragraphs: hard-wrapped lines joined, lists and key lines kept."""
    lines = [l.rstrip() for l in text.replace("\t", "    ").splitlines()]
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    out = []
    for l in lines:
        s = l.strip()
        if not s:
            if out and out[-1] != "":
                out.append("")
            continue
        starts_item = re.match(r'^("|-|\*|\d+\)|\d+\s*[:=-]|[A-Z_]{3,}\b\s*[:=-]|[a-z_]+\s*[:=]\s)', s)
        s = re.sub(r"(?<=\S)  +", " ", s)
        if out and out[-1] != "" and not starts_item and not out[-1].endswith(":"):
            out[-1] += s if re.search(r"[a-z]-$", out[-1]) else " " + s
        else:
            out.append(s)
    return "\n".join(out).strip()


def color255(c):
    return " ".join(str(int(round(max(0.0, min(1.0, x)) * 255))) for x in c)


def resolve_props(name, classes, seen=None):
    """All properties of a class with its bases' (a class's own win), by lower-case name."""
    seen = seen or set()
    c = classes.get(name)
    if c is None or name in seen:
        return {}
    seen.add(name)
    props = {}
    for b in c.bases():
        props.update(resolve_props(b, classes, seen))
    for p in c.props:
        props[p.name.lower()] = p
    return props


def merge_into(dst, src):
    for key, v in src.header.items():
        dst.header[key] = v
    if src.desc:
        dst.desc = src.desc
    for p in src.props:
        old = dst.prop(p.name)
        if old is None:
            dst.props.append(p)
        elif p.kind() == "flags" and old.kind() == "flags" and p.items is not None:
            bits = {it[0]: it for it in (old.items or [])}
            for it in p.items:
                bits[it[0]] = it
            old.items = [bits[b] for b in sorted(bits)]
            if p.short:
                old.short = p.short
        else:
            dst.props[dst.props.index(old)] = p
    if src.kind != "BaseClass":
        dst.kind = src.kind


def directives(text):
    internal, drops, editor = {}, [], []
    for m in re.finditer(r"^//!\s*(\w+)\s+(.*)$", text, re.M):
        d, rest = m.group(1), m.group(2).split(None, 1)
        if d == "internal":
            internal[rest[0]] = rest[1] if len(rest) > 1 else ""
        elif d == "drop":
            drops.append(tuple(rest[0:1] + rest[1].split()[:1]))
        elif d == "editor":
            editor.append(rest[0])
        elif d == "nomodel":
            drops.append((rest[0], "model()"))
        else:
            raise FgdError("entities.fgd: unknown directive //! %s" % d)
    return internal, drops, editor


def category(name):
    for i, (prefix, _) in enumerate(CATEGORIES):
        if prefix == "light" and (name == "light" or name.startswith("light_")):
            return i
        if prefix != "light" and name.startswith(prefix):
            return i
    return len(CATEGORIES) - 1


def build():
    """(fgd text, problems [errors], notes [per-entity undocumented keys], entity names)."""
    vendor, _ = read_fgd(VENDOR_FGD)
    source, source_text = read_fgd(SOURCE_FGD)
    internal, drops, editor = directives(source_text)
    ents, quaked, funcs = qc_entities(internal)

    errors, notes = [], []
    vendor_by = {c.name: c for c in vendor}
    source_by = {}
    for c in source:
        if c.name in source_by:
            errors.append("entities.fgd: %s is defined twice" % c.name)
        source_by[c.name] = c

    glossary = source_by.get("_Fields")
    if glossary is None:
        errors.append("entities.fgd: no _Fields glossary")
        glossary = FgdClass("BaseClass", "_Fields")

    for name in internal:
        if name not in funcs:
            errors.append("entities.fgd: //! internal %s: no such QC function (stale)" % name)

    # The classes to write: every spawn function, and the editor-only ones.
    result = {}
    for name in sorted(ents, key=lambda n: (category(n), n)):
        e = ents[name]
        c = FgdClass("PointClass", name)
        if name in vendor_by:
            v = vendor_by[name]
            c.kind, c.header, c.desc = v.kind, dict(v.header), v.desc
            c.props = [Prop(p.name, p.ptype, p.short, p.default, p.long,
                            list(p.items) if p.items is not None else None) for p in v.props]
            for p in c.props:
                if p.kind() == "flags" and p.items:
                    # Quake.fgd lists some defaults as a flag 0 ("Spike": no flag set), which is not a flag.
                    p.items = [it for it in p.items if it[0] > 0]
        qk = e.quaked
        if qk:
            if qk["brush"]:
                c.kind = "SolidClass"
            if qk["color"] and "color" not in c.header:
                c.header["color"] = color255(qk["color"])
            if qk["size"] and "size" not in c.header and not qk["brush"]:
                c.header["size"] = "%s, %s" % (" ".join(qk["size"][0]), " ".join(qk["size"][1]))
            text = reflow(qk["text"])
            if text:
                if c.desc:
                    c.desc = c.desc.split("\n")[0].strip() + "\n\n" + text
                elif e.file.split("_")[0] in ORIGINS:
                    c.desc = ORIGINS[e.file.split("_")[0]] + "\n\n" + text
                else:
                    c.desc = text
        if e.model and "model" not in c.header and c.kind == "PointClass":
            c.header["model"] = '{ "path": "%s" }' % e.model
        # Spawnflags: the QUAKED names, then the bits the body tests.
        flags = {}
        if qk:
            for i, fl in enumerate(qk["flags"]):
                if fl.lower() in ("x", "-", "?", "unused") or i > 23:
                    continue
                flags[1 << i] = fl.replace("_", " ").strip()
        for bit, const in e.flag_bits.items():
            if bit not in flags:
                label = const
                for pre in ("HONEY_SPAWNFLAG_", "SPAWNFLAG_", "SF_"):
                    if label.startswith(pre):
                        label = label[len(pre):]
                flags[bit] = label.replace("_", " ").lower().capitalize() if label else "Flag %d" % bit
        if flags:
            fp = c.prop("spawnflags")
            existing = {it[0] for it in (fp.items or [])} if fp else set()
            inherited = resolve_props(name, dict(vendor_by, **{name: c})).get("spawnflags")
            inherited_bits = {it[0] for it in (inherited.items or [])} if inherited else set()
            new = [(b, l, "0", "") for b, l in sorted(flags.items()) if b not in existing and b not in inherited_bits]
            if new:
                if fp is None:
                    fp = Prop("spawnflags", "flags", items=[])
                    c.props.append(fp)
                fp.items = sorted((fp.items or []) + new)
        c.origin = e.file
        result[name] = c

    for name in editor:
        if name in result:
            errors.append("entities.fgd: //! editor %s has a QC spawn function" % name)
            continue
        if name in vendor_by:
            v = vendor_by[name]
            c = FgdClass(v.kind, name)
            c.header, c.desc, c.props = dict(v.header), v.desc, list(v.props)
        else:
            c = FgdClass("PointClass", name)
        result[name] = c

    # The hand-written definitions.
    for name, s in source_by.items():
        if s.kind == "BaseClass":
            continue
        if name not in result:
            if name in funcs:
                errors.append("entities.fgd: %s is not taken for a spawn function (frame function, helper, or "
                              "//! internal)" % name)
            else:
                errors.append("entities.fgd: %s has no QC spawn function (stale entry, or add //! editor %s)"
                              % (name, name))
            continue
        merge_into(result[name], s)
    for cls, prop in drops:
        if prop == "model()":
            if cls not in result or "model" not in result[cls].header:
                errors.append("entities.fgd: //! nomodel %s: it has no model()" % cls)
            else:
                del result[cls].header["model"]
            continue
        c = result.get(cls)
        if c is None or c.prop(prop) is None:
            errors.append("entities.fgd: //! drop %s %s: nothing to drop" % (cls, prop))
        else:
            c.props.remove(c.prop(prop))

    # Base classes used (vendor's, overridden by the hand-written ones).
    bases = {c.name: c for c in vendor if c.kind == "BaseClass"}
    for c in source:
        if c.kind == "BaseClass":
            if c.name in bases and c.name != "_Fields":
                merge_into(bases[c.name], c)
            else:
                bases[c.name] = c
    all_classes = dict(bases)
    all_classes.update(result)

    # Keys the spawn functions read: from the glossary if no definition has them.
    for name, c in result.items():
        e = ents.get(name)
        if not e:
            continue
        have = resolve_props(name, all_classes)
        missing = []
        for f in e.reads:
            if f.lower() in have or f in ("spawnflags", "classname", "origin", "angles", "model"):
                continue
            g = glossary.prop(f)
            if g is not None:
                c.props.append(g)
                have[f.lower()] = g
            else:
                missing.append(f)
        if missing:
            notes.append("%s (%s): reads %s" % (name, e.file, " ".join(missing)))

    # Every entity needs help and a known shape.
    for name, c in result.items():
        if not c.desc.strip():
            errors.append("%s (%s): no help text: add it to entities.fgd" % (name, c.origin or "?"))
        e = ents.get(name)
        if e and not e.quaked and name not in source_by and name not in vendor_by:
            errors.append("%s (%s): no FGD entry: a spawn function without a QUAKED comment needs a definition in "
                          "Misc/trenchbroom/entities.fgd (or //! internal %s <why>)" % (name, e.file, name))
        for b in c.bases():
            if b not in all_classes:
                errors.append("%s: unknown base class %s" % (name, b))

    # Write: the base classes used, then the entities by category.
    used = []

    def use(bname):
        if bname in used or bname not in bases:
            return
        for b in bases[bname].bases():
            use(b)
        used.append(bname)

    for c in result.values():
        for b in c.bases():
            use(b)

    out = [
        "// Quake VR entity definitions for TrenchBroom.",
        "//",
        "// GENERATED by Misc/trenchbroom/fgdgen.py from the QuakeC (QC/), Misc/trenchbroom/entities.fgd and",
        "// TrenchBroom's Quake.fgd (Misc/trenchbroom/vendor). Do not edit: edit entities.fgd, then run",
        "//     python Misc/trenchbroom/fgdgen.py",
        "",
    ]
    out.append("//")
    out.append("// Base classes")
    out.append("//")
    out.append("")
    for b in used:
        write_class(bases[b], out)
    cat = None
    for name, c in result.items():
        k = category(name)
        if k != cat:
            cat = k
            out.append("//")
            out.append("// " + CATEGORIES[k][1])
            out.append("//")
            out.append("")
        write_class(c, out)
    return "\n".join(out), errors, notes, sorted(result)


# ------------------------------------------------------------------------------------------------------------------
# Model check
# ------------------------------------------------------------------------------------------------------------------

# ------------------------------------------------------------------------------------------------------------------
# TrenchBroom's expression language, as it evaluates model() (the manual's "Expression Language"), strictly:
# - an entity's key is a String when it is set, Null when it is not (Null converts to 0, "" and false);
# - && || ! take Booleans only ('weapon == "6" && flags & 1' is Boolean && Number: TrenchBroom's error);
# - & | ^ << >> convert both sides to Number (a String must be a number: "3" yes, "abc" no);
# - == != compare values of the same type (or Null with anything: equal only to Null); < <= > >= Numbers;
# - a case's premise converts to Boolean (Quake.fgd's 'spawnflags & 1 -> ...'); a switch {{ }} gives its first
#   case that applies; precedence, lowest first: ->, ||, &&, |, ^, &, == !=, < <= > >=, << >>, + -, * / %, unary.
# ------------------------------------------------------------------------------------------------------------------

class ElError(Exception):
    pass


EL_TOKEN = re.compile(r'\s+|"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'|\d+(?:\.\d+)?|[A-Za-z_]\w*|->|==|!=|<=|>=|<<|>>|'
                      r'&&|\|\||\.\.|[-+*/%&|^~!<>(){}\[\],:]')
EL_BINARY = [["||"], ["&&"], ["|"], ["^"], ["&"], ["==", "!="], ["<", "<=", ">", ">="], ["<<", ">>"], ["+", "-"],
             ["*", "/", "%"]]


def el_parse(src):
    toks, i = [], 0
    while i < len(src):
        m = EL_TOKEN.match(src, i)
        if not m:
            raise ElError("unexpected character %r" % src[i])
        if not m.group(0).isspace():
            toks.append(m.group(0))
        i = m.end()
    pos = [0]

    def peek(k=0):
        return toks[pos[0] + k] if pos[0] + k < len(toks) else None

    def take(expected=None):
        t = peek()
        if t is None or (expected is not None and t != expected):
            raise ElError("expected %r, got %r" % (expected, t))
        pos[0] += 1
        return t

    def expression():
        left = binary(0)
        if peek() == "->":
            take()
            return ("case", left, binary(0))
        return left

    def binary(level):
        if level == len(EL_BINARY):
            return unary()
        left = binary(level + 1)
        while peek() in EL_BINARY[level]:
            op = take()
            left = ("bin", op, left, binary(level + 1))
        return left

    def unary():
        if peek() in ("!", "~", "-", "+"):
            op = take()
            return ("un", op, unary())
        return primary()

    def primary():
        t = take()
        if t == "(":
            e = expression()
            take(")")
            return e
        if t == "{" and peek() == "{":
            take("{")
            items = [expression()]
            while peek() == ",":
                take()
                items.append(expression())
            take("}")
            take("}")
            return ("switch", items)
        if t == "{":
            entries = []
            while peek() != "}":
                k = take()
                if k[0] not in "\"'":
                    raise ElError("a map key must be a string, got %r" % k)
                take(":")
                entries.append((k[1:-1], expression()))
                if peek() == ",":
                    take()
            take("}")
            return ("map", entries)
        if t[0] in "\"'":
            return ("lit", ("S", t[1:-1]))
        if re.match(r"\d", t):
            return ("lit", ("N", float(t)))
        if t in ("true", "false"):
            return ("lit", ("B", t == "true"))
        if t == "null":
            return ("lit", ("Z", None))
        if re.match(r"[A-Za-z_]", t):
            return ("var", t)
        raise ElError("unexpected %r" % t)

    tree = expression()
    if peek() is not None:
        raise ElError("unexpected %r after the expression" % peek())
    return tree


def el_convert(v, to):
    t, x = v
    if t == to:
        return x
    if t == "U":
        raise ElError("undefined value")
    if to == "B":
        return {"S": lambda: x not in ("", "false"), "N": lambda: x != 0, "Z": lambda: False}.get(t, lambda: None)()
    if to == "N":
        if t == "B":
            return 1.0 if x else 0.0
        if t == "Z":
            return 0.0
        if t == "S":
            if x.strip() == "":
                return 0.0
            if re.fullmatch(r"-?\d+(\.\d+)?", x.strip()):
                return float(x)
            raise ElError("%r is not a number" % x)
    if to == "S":
        if t == "B":
            return "true" if x else "false"
        if t == "N":
            return "%d" % x if x == int(x) else repr(x)
        if t == "Z":
            return ""
    raise ElError("cannot convert %s to %s" % (t, to))


def el_eval(node, env):
    kind = node[0]
    if kind == "lit":
        return node[1]
    if kind == "var":
        v = env.get(node[1])
        return ("Z", None) if v is None else ("S", v)
    if kind == "map":
        return ("M", {k: el_eval(e, env) for k, e in node[1]})
    if kind == "switch":
        for item in node[1]:
            v = el_eval(item, env)
            if v[0] != "U":
                return v
        return ("U", None)
    if kind == "case":
        premise = el_eval(node[1], env)
        if premise[0] in ("M", "A"):
            raise ElError("a case's premise is a %s" % premise[0])
        return el_eval(node[2], env) if el_convert(premise, "B") else ("U", None)
    if kind == "un":
        op, v = node[1], el_eval(node[2], env)
        if op == "!":
            if v[0] != "B":
                raise ElError("! of a %s" % v[0])
            return ("B", not v[1])
        n = el_convert(v, "N")
        return ("N", float(~int(n)) if op == "~" else (-n if op == "-" else n))
    op, a, b = node[1], el_eval(node[2], env), el_eval(node[3], env)
    if op in ("&&", "||"):
        if a[0] != "B" or b[0] != "B":
            raise ElError("invalid operand types %s and %s for %s" % (a[0], b[0], op))
        return ("B", (a[1] and b[1]) if op == "&&" else (a[1] or b[1]))
    if op in ("&", "|", "^", "<<", ">>"):
        x, y = int(el_convert(a, "N")), int(el_convert(b, "N"))
        return ("N", float({"&": x & y, "|": x | y, "^": x ^ y, "<<": x << y, ">>": x >> y}[op]))
    if op in ("==", "!="):
        if a[0] == "Z" or b[0] == "Z":
            eq = a[0] == b[0]
        elif a[0] != b[0]:
            raise ElError("comparing a %s with a %s (%r %s %r)" % (a[0], b[0], a[1], op, b[1]))
        else:
            eq = a[1] == b[1]
        return ("B", eq if op == "==" else not eq)
    x, y = el_convert(a, "N"), el_convert(b, "N")
    if a[0] != "N" and b[0] != "N":
        raise ElError("%s on a %s and a %s" % (op, a[0], b[0]))
    if op in ("<", "<=", ">", ">="):
        return ("B", {"<": x < y, "<=": x <= y, ">": x > y, ">=": x >= y}[op])
    if op in ("/", "%") and y == 0:
        raise ElError("division by zero")
    return ("N", {"+": x + y, "-": x - y, "*": x * y, "/": x / y if y else 0, "%": x % y if y else 0}[op])


def el_vars(node, out):
    kind = node[0]
    if kind == "var":
        out.add(node[1])
    elif kind == "map":
        for _, e in node[1]:
            el_vars(e, out)
    elif kind == "switch":
        for e in node[1]:
            el_vars(e, out)
    elif kind == "case":
        el_vars(node[1], out)
        el_vars(node[2], out)
    elif kind == "un":
        el_vars(node[2], out)
    elif kind == "bin":
        el_vars(node[2], out)
        el_vars(node[3], out)
    return out


def model_cases(c, classes):
    """Every value combination worth trying for the keys a class's model() reads: unset, each choice, the
    default, each spawnflag alone, pairs of them, all of them."""
    import itertools
    tree = el_parse(c.header["model"])
    props = resolve_props(c.name, classes)
    names = sorted(el_vars(tree, set()))
    options = []
    for n in names:
        p = props.get(n.lower())
        vals = [None]
        if p is not None and p.kind() == "choices":
            vals += [unquote(v) if v.startswith('"') else v for v, _, _ in p.items or []]
        elif p is not None and p.kind() == "flags":
            bits = [it[0] for it in p.items or []]
            combos = {0, sum(bits)} | set(bits) | {a | b for a, b in itertools.combinations(bits, 2)}
            vals += [str(v) for v in sorted(combos)]
        else:
            vals += ["1", "0"]
        if p is not None and p.default is not None:
            vals.append(unquote(p.default) if p.default.startswith('"') else p.default)
        options.append(sorted(set(vals), key=lambda v: (v is not None, str(v))))
    for combo in itertools.product(*options):
        yield tree, dict(zip(names, combo))


def model_paths(c, classes, problems):
    """The model paths a class's model() gives over model_cases; its failures go to problems."""
    paths = set()
    try:
        cases = list(model_cases(c, classes))
    except ElError as e:
        problems.append("%s: model() does not parse: %s" % (c.name, e))
        return paths
    for tree, env in cases:
        try:
            v = el_eval(tree, env)
        except ElError as e:
            set_keys = {k: val for k, val in env.items() if val is not None}
            problems.append("%s: model() fails with %s: %s" % (c.name, set_keys or "no keys set", e))
            return paths
        if v[0] == "S":
            path = v[1]
        elif v[0] == "M":
            p = v[1].get("path")
            if not p or p[0] != "S":
                problems.append("%s: model() gives a map without a path" % c.name)
                return paths
            path = p[1]
            for k in ("skin", "frame"):
                if k in v[1] and v[1][k][0] != "N":
                    problems.append("%s: model()'s %s is not a number" % (c.name, k))
        elif v[0] == "U":
            continue
        else:
            problems.append("%s: model() gives a %s" % (c.name, v[0]))
            return paths
        paths.add(path)
    return paths


def lint(text):
    """What TrenchBroom would reject or show badly, in a written FGD: classes defined twice, unknown base classes,
    point or brush classes without help, repeated keys, spawnflags that are not single bits, unbalanced model()
    expressions."""
    problems = []
    classes = _Parser(text, "quakevr.fgd").classes()
    seen = {}
    for c in classes:
        if c.name in seen:
            problems.append("%s defined twice" % c.name)
        seen[c.name] = c
    for c in classes:
        for b in c.bases():
            if b not in seen or seen[b].kind != "BaseClass":
                problems.append("%s: base class %s is not defined before use" % (c.name, b))
        if c.kind != "BaseClass" and not c.desc.strip():
            problems.append("%s: no help text" % c.name)
        names = [p.name.lower() for p in c.props]
        for n in set(names):
            if names.count(n) > 1:
                problems.append("%s: key %s defined %d times" % (c.name, n, names.count(n)))
        for p in c.props:
            if p.kind() == "flags":
                bits = [it[0] for it in p.items or []]
                for b in bits:
                    if b <= 0 or b & (b - 1):
                        problems.append("%s: spawnflag %d is not a single bit" % (c.name, b))
                if len(set(bits)) != len(bits):
                    problems.append("%s: a spawnflag bit is listed twice" % c.name)
        if "model" in c.header:
            model_paths(c, seen, problems)
    # Base classes must come before the classes that use them (TrenchBroom resolves them in order).
    order = [c.name for c in classes]
    for c in classes:
        for b in c.bases():
            if b in order and order.index(b) > order.index(c.name):
                problems.append("%s: its base %s comes after it" % (c.name, b))
    return problems


def check_config():
    """The game configuration and the profile templates: they must parse, and name files that exist."""
    import json
    problems = []
    cfg_dir = os.path.dirname(OUT_FGD)

    def load(name):
        with open(os.path.join(cfg_dir, name), encoding="utf-8") as f:
            text = f.read()
        # TrenchBroom's configs allow // comments (outside strings).
        text = re.sub(r'"(?:[^"\\]|\\.)*"|//[^\n]*', lambda m: m.group(0) if m.group(0)[0] == '"' else "", text)
        # and their own string escapes, such as the Liquid tag's "\**" (a literal *): not JSON's.
        text = re.sub(r'\\([^"\\/bfnrtu])', r'\\\\\1', text)
        return json.loads(text)

    try:
        cfg = load("GameConfig.cfg")
        if cfg.get("version") != 9:
            problems.append("GameConfig.cfg: version %r (TrenchBroom 2026.2 reads 9)" % cfg.get("version"))
        for key in ("name", "fileformats", "filesystem", "materials", "entities"):
            if key not in cfg:
                problems.append("GameConfig.cfg: no %s" % key)
        files = [cfg.get("icon", "")] + cfg.get("entities", {}).get("definitions", []) + \
            [f.get("initialmap", "") for f in cfg.get("fileformats", [])]
        for f in files:
            if f and not os.path.exists(os.path.join(cfg_dir, f)):
                problems.append("GameConfig.cfg: %s does not exist" % f)
    except (OSError, ValueError) as e:
        problems.append("GameConfig.cfg: %s" % e)
    for name in ("CompilationProfiles.cfg", "GameEngineProfiles.cfg"):
        try:
            if not load(name).get("profiles"):
                problems.append("%s: no profiles" % name)
        except (OSError, ValueError) as e:
            problems.append("%s: %s" % (name, e))
    return problems


def pak_names(path):
    with open(path, "rb") as f:
        magic, off, size = struct.unpack("<4sii", f.read(12))
        if magic != b"PACK":
            return set()
        f.seek(off)
        d = f.read(size)
    return {d[i:i + 56].split(b"\0")[0].decode("latin1").lower() for i in range(0, size, 64)}


def game_files(quake):
    files = {}
    for game in ("id1", "hipnotic", "rogue", "quakevr"):
        gdir = os.path.join(quake, game)
        if not os.path.isdir(gdir):
            continue
        for fn in os.listdir(gdir):
            if fn.lower().endswith(".pak"):
                for n in pak_names(os.path.join(gdir, fn)):
                    files.setdefault(n, "%s/%s" % (game, fn))
        for sub in ("progs", "maps"):
            sdir = os.path.join(gdir, sub)
            if os.path.isdir(sdir):
                for fn in os.listdir(sdir):
                    files.setdefault(("%s/%s" % (sub, fn)).lower(), "%s (loose)" % game)
    return files


def verify_models(fgd_text, quake):
    files = game_files(quake)
    if not files:
        print("no game files under %s" % quake)
        return 1
    bad = 0
    count = 0
    classes = _Parser(fgd_text, "quakevr.fgd").classes()
    by_name = {c.name: c for c in classes}
    for c in classes:
        if "model" not in c.header:
            continue
        problems = []
        found = model_paths(c, by_name, problems)   # every path the expression gives, over its keys' values
        for p in problems:
            bad += 1
            print("BAD %s" % p)
        literal = set(re.findall(r'"(:?[\w/.-]+\.(?:mdl|bsp|spr))"', c.header["model"]))
        for p in sorted(found | literal):
            count += 1
            where = files.get(p.lstrip(":").lower())
            if where is None:
                bad += 1
                print("MISSING %s: %s" % (c.name, p))
    print("%d model paths checked, %d missing" % (count, bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--verify-models", metavar="QUAKE_DIR")
    ap.add_argument("--qc", metavar="DIR", help="read the QuakeC from DIR instead of QC/ (to test the check)")
    args = ap.parse_args()
    if args.qc:
        global QC_DIR
        QC_DIR = os.path.abspath(args.qc)
    try:
        text, errors, notes, names = build()
    except FgdError as e:
        print("fgdgen: %s" % e)
        return 1
    if args.report:
        for e in errors:
            print("fgdgen: ERROR %s" % e)
        for n in notes:
            print(n)
        return 1 if errors else 0
    if args.verify_models:
        return verify_models(text, args.verify_models)
    for e in errors:
        print("fgdgen: ERROR %s" % e)
    if args.check:
        try:
            with open(OUT_FGD, encoding="utf-8") as f:
                current = f.read()
        except OSError:
            current = ""
        try:
            written = {c.name for c in _Parser(current, "quakevr.fgd").classes() if c.kind != "BaseClass"}
        except FgdError as e:
            print("fgdgen: ERROR quakevr.fgd: %s" % e)
            return 1
        for n in sorted(set(names) - written):
            errors.append(n)
            print("fgdgen: ERROR %s has no entry in quakevr.fgd: run python Misc/trenchbroom/fgdgen.py" % n)
        for n in sorted(written - set(names)):
            errors.append(n)
            print("fgdgen: ERROR quakevr.fgd has %s, which has no QC spawn function: run python "
                  "Misc/trenchbroom/fgdgen.py" % n)
        for p in lint(current) + check_config():
            errors.append(p)
            print("fgdgen: ERROR quakevr.fgd: %s" % p)
        if not errors and current != text:
            print("fgdgen: warning: quakevr.fgd is out of date (the QC or entities.fgd changed): run python "
                  "Misc/trenchbroom/fgdgen.py")
        if errors:
            return 1
        print("fgdgen: %d entities, quakevr.fgd covers every spawn function" % len(names))
        return 0
    if errors:
        return 1
    with open(OUT_FGD, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("fgdgen: wrote %s (%d entities)" % (os.path.relpath(OUT_FGD, ROOT), len(names)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

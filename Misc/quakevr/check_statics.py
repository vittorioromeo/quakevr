#!/usr/bin/env python3
"""Fails on any function-local `static` or `thread_local` variable in Quake/vr that is not `constexpr`
(docs/vr-port/CODE_STYLE.md, "Scratch buffers and caches").

The rule: inside a function body (a lambda's too) a `static` variable must be `static constexpr`. `constexpr` is the
compiler's own proof that the variable is constant-initialised and read-only: no hidden state, no first-call
initialisation and no thread-safe guard (MSVC's `_Init_thread_header`/`$TSS` guard is emitted for every local static
that is dynamically initialised or has a destructor). A `static const` is rejected too, even when it happens to be
constant-initialised (`static const int n = 3`): from the source alone `static const T x = f();` (guarded, built on
the first call) looks the same, so the rule is spelled `constexpr`. Everything else is state and goes where it is
visible: the file's `mem::Scratch`/`mem::Cache` set, a named struct at file scope, or a namespace-scope `const`.

A function-local `thread_local` is rejected the same way (a file-scope one is a worker's state by design:
`vr_decals.cpp`'s RNG). Class members (`static` member functions and data) and namespace-scope `static`s are not
function-local and are not checked.

A line that must stay ends with the comment `// statics-ok: <why>`.

Usage: python Misc/quakevr/check_statics.py [--legacy] [files or folders...]
  (from the repository's root; exit 1 and the sites when any is found. --legacy applies the old line rule, mutable
  `static std::`/`za::`/`ankerl::` and indented `thread_local` only, to compare counts.)
"""
import bisect
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2] / "Quake" / "vr"
EXTS = {".cpp", ".hpp", ".h", ".inc", ".c"}

# Words that, between a `)` and a `{`, still make the `{` a function's body: `f() const {`, `[]() mutable {`,
# `g() noexcept {`, `h() override {`.
FN_TAIL = {"const", "noexcept", "override", "final", "mutable", "volatile", "&", "&&"}
CLASS_KEYS = {"class", "struct", "union", "enum"}


def strip(text):
    """The source with comments, strings and character literals blanked (newlines kept, so line numbers hold)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c == "R" and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            m = re.match(r'R"([^ ()\\\t\n]{0,16})\(', text[i:])
            if not m:
                out.append(c)
                i += 1
                continue
            end = text.find(")" + m.group(1) + '"', i)
            j = n if end < 0 else end + len(m.group(1)) + 2
            out.append('""' + re.sub(r"[^\n]", " ", text[i + 2:j]))
            i = j
        elif c in "\"'":
            if c == "'" and i > 0 and text[i - 1].isalnum():  # digit separator: 1'000
                out.append(" ")
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + " " * (j - i - 2) + c if j - i >= 2 else c)
            i = j
        elif c == "#" and (i == 0 or text[i - 1] == "\n" or text[:i].rsplit("\n", 1)[-1].strip() == ""):
            j = i  # a preprocessor line (and its continuations): not code for this purpose
            while True:
                k = text.find("\n", j)
                if k < 0:
                    k = n
                    break
                if text[k - 1] != "\\":
                    break
                j = k + 1
            out.append(re.sub(r"[^\n]", " ", text[i:k]))
            i = k
        else:
            out.append(c)
            i += 1
    return "".join(out)


TOKEN = re.compile(r"[A-Za-z_]\w*|\d[\w.]*|::|->|&&|\S")


def scan(path):
    raw = path.read_text(encoding="utf-8", errors="replace")
    lines = raw.splitlines()
    code = strip(raw)
    breaks = [k for k, c in enumerate(code) if c == "\n"]
    toks = [(m.group(0), bisect.bisect_left(breaks, m.start()) + 1) for m in TOKEN.finditer(code)]
    # The scope stack: "ns" (namespace or file), "class", "fn" (a function's or lambda's body, and every block in one),
    # "init" (a braced initialiser: holds no declarations; a lambda in it is "fn"). Each entry keeps the statement and
    # the parenthesis depth it interrupted: an initialiser or a lambda is part of its enclosing statement.
    stack = [("ns", 0, 0, False)]
    found = []
    start = 0  # index of the first token of the current statement or declaration
    paren = 0
    for i, (t, ln) in enumerate(toks):
        if t in "([":
            paren += 1
        elif t in ")]":
            paren -= 1
        elif t == "{":
            head = [x for x, _ in toks[start:i]]
            kind, expr = classify(head, stack[-1][0])
            stack.append((kind, start, paren, expr))
            start, paren = i + 1, 0
        elif t == "}":
            if len(stack) > 1:
                kind, saved_start, saved_paren, expr = stack.pop()
                if kind == "init" or expr:
                    start, paren = saved_start, saved_paren  # the statement goes on: `T x{...};`, `f([] {...});`
                else:
                    start, paren = i + 1, 0
        elif t == ";" and paren == 0:
            start = i + 1
        elif t == ":" and i > 0 and toks[i - 1][0] in ("public", "private", "protected", "default"):
            start = i + 1
        elif t in ("static", "thread_local") and in_function(stack):
            # Anywhere in a function's body, parentheses included: `if(static bool once = false; !once)`. The
            # declaration's specifiers are the run of them around this one (counted once, at its first).
            j = i
            while j > start and toks[j - 1][0] in SPECIFIERS:
                j -= 1
            if any(x in ("static", "thread_local") for x, _ in toks[j:i]):
                continue  # the same declaration's second specifier
            words = []
            while j < len(toks) and toks[j][0] in SPECIFIERS:
                words.append(toks[j][0])
                j += 1
            if "constexpr" in words and "thread_local" not in words:
                continue
            text = lines[ln - 1] if ln - 1 < len(lines) else ""
            if "statics-ok" in text:
                continue
            found.append((ln, text.strip()))
    return found


SPECIFIERS = {"static", "thread_local", "constexpr", "constinit", "const", "inline", "volatile", "mutable", "extern"}


def in_function(stack):
    for kind, *_ in reversed(stack):
        if kind != "init":
            return kind == "fn"
    return False


def classify(head, parent):
    """What a `{` opens, from the tokens of its statement before it: (kind, is it inside an expression)."""
    if not head:
        return ("init" if parent == "init" else parent), False  # a block (in a function) or a nested initialiser
    prev = head[-1]
    if head == ["extern", '"', '"'] or ("namespace" in head and "(" not in head):  # `extern "C" {`, a namespace
        return "ns", False
    # a class definition: `struct X {`, `class X : public Y<T> {`, `enum class E : int {`, `static const struct {`
    depth = 0
    for k, x in enumerate(head):
        if x in "([<":
            depth += 1
        elif x in ")]>":
            depth -= 1
        elif x in CLASS_KEYS and depth == 0 and "=" not in head[k + 1:] and "(" not in head[k + 1:]:
            return "class", False
    if prev in ("=", ",", "(", "return", "?", "[", "{"):
        return "init", False
    # a function's body: `) {`, `) const noexcept {`, `-> T {`, `] {` (a lambda without parameters)
    j = len(head) - 1
    if "->" in head and head.index("->") > (len(head) - 1 - head[::-1].index(")") if ")" in head else -1):
        j = head.index("->") - 1
    while j >= 0 and head[j] in FN_TAIL:
        j -= 1
    if j >= 0 and head[j] == "]":
        return "fn", True
    if j >= 0 and head[j] == ")":
        depth, k = 0, j
        while k >= 0:
            depth += {")": 1, "(": -1}.get(head[k], 0)
            if depth == 0:
                break
            k -= 1
        lam = k > 0 and head[k - 1] == "]"
        return "fn", lam
    if prev in ("else", "do", "try", "}"):
        return "fn", False  # `} {`: a constructor's body after a braced member initialiser
    # `T name{...}`, `return T{...}`: a braced initialiser
    return "init", False


LEGACY_STD = re.compile(
    r"^\s+(?:static\s+thread_local|thread_local\s+static|static)\s+(?!const\b|constexpr\b|inline\b)"
    r"(?:std::|::std::|za::|::za::|ankerl::).*?\s[\*&]*\w+\s*(?:\[[^\]]*\]\s*)?(?:;|=|\{)"
)
LEGACY_TL = re.compile(r"^\s+(?:static\s+)?thread_local\s+(?!const\b|constexpr\b)")


def legacy(path):
    out = []
    for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if "statics-ok" in line:
            continue
        code = line.split("//", 1)[0]
        if LEGACY_STD.match(code) or LEGACY_TL.match(code):
            out.append((n, line.strip()))
    return out


def tree(root):
    return [p for p in sorted(root.rglob("*"))
            if p.suffix in EXTS and "external" not in p.relative_to(root).parts]


def files(args):
    if not args:
        return tree(ROOT)
    out = []
    for a in args:
        p = pathlib.Path(a).resolve()
        out += tree(p) if p.is_dir() else [p]
    return out


def main():
    args = sys.argv[1:]
    use_legacy = "--legacy" in args
    args = [a for a in args if a != "--legacy"]
    found = []
    for path in files(args):
        for ln, text in (legacy(path) if use_legacy else scan(path)):
            try:
                name = path.relative_to(ROOT.parents[1]).as_posix()
            except ValueError:
                name = path.as_posix()
            found.append(f"{name}:{ln}: {text}")
    for f in found:
        print(f)
    if found:
        print(f"{len(found)} function-local static/thread_local that is not constexpr (see docs/vr-port/CODE_STYLE.md, "
              f"'Scratch buffers and caches')")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

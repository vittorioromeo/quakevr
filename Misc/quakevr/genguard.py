# genguard.py -- keeps the generators from overwriting models edited by hand (in Blender:
# docs/vr-port/HANDS_IN_BLENDER.md, docs/vr-port/MODELS_IN_BLENDER.md).
#
# Misc/quakevr/generated.json holds, for every file a guarded generator wrote into the repository, the SHA-256 of what
# it wrote. Before a generator writes, Guard looks at the files it is about to write: one whose content is no longer
# what a generator last wrote has been edited since, and the generator stops before writing anything, naming it,
# unless it was run with
#   --keep-edited  write everything else, and leave the edited files as they are;
#   --force        overwrite them too (the edits are lost: keep your .blend; git has the last committed files).
# Files the manifest doesn't know (a new output, or one written outside the repository) are simply written.
#
# In a generator (importing this takes --force and --keep-edited out of sys.argv, so the generator's own arguments
# read as before):
#   import genguard
#   ...
#   guard = genguard.Guard("make_vrbody.py", [every file it will write])   # stops here if one was edited
#   ... write them ...
#   guard.finish()                                                         # records what was written

import atexit
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
MANIFEST = os.path.join(HERE, "generated.json")

FORCE = "--force" in sys.argv
KEEP = "--keep-edited" in sys.argv
sys.argv[:] = [a for a in sys.argv if a not in ("--force", "--keep-edited")]


def rel(path):
    """The path relative to the repository, or None outside it."""
    r = os.path.relpath(os.path.abspath(path), ROOT)
    return None if r.startswith("..") or os.path.isabs(r) else r.replace("\\", "/")


TEXT = (".md5mesh", ".md5anim")


def sha256(path):
    """The file's SHA-256 (a text file's with its line ends as LF, as git may check it out with CRLF)."""
    with open(path, "rb") as f:
        data = f.read()
    if path.lower().endswith(TEXT):
        data = data.replace(b"\r\n", b"\n")
    return hashlib.sha256(data).hexdigest()


def load():
    if not os.path.exists(MANIFEST):
        return {}
    with open(MANIFEST) as f:
        return json.load(f)


def save(manifest):
    with open(MANIFEST, "w", newline="\n") as f:
        json.dump(dict(sorted(manifest.items())), f, indent=1)
        f.write("\n")


def edited(paths, manifest=None):
    """The files among `paths` whose content isn't what the manifest says a generator wrote."""
    manifest = load() if manifest is None else manifest
    out = []
    for p in paths:
        r = rel(p)
        e = manifest.get(r) if r else None
        if e is not None and os.path.exists(p) and sha256(p) != e["sha256"]:
            out.append(p)
    return out


class Guard:
    def __init__(self, script, paths):
        self.script = script
        self.paths = [os.path.abspath(p) for p in paths]
        self.kept = {}
        changed = edited(self.paths)
        if not changed:
            return
        names = "\n".join("  %s (last written by %s)" % (rel(p), load()[rel(p)].get("by", "?")) for p in changed)
        if FORCE:
            print("%s: --force: overwriting %d files edited since a generator wrote them:\n%s" % (
                script, len(changed), names))
        elif KEEP:
            print("%s: --keep-edited: leaving %d files edited since a generator wrote them as they are:\n%s" % (
                script, len(changed), names))
            for p in changed:
                with open(p, "rb") as f:
                    self.kept[p] = f.read()
            atexit.register(self.restore)  # even if the generator fails half way
        else:
            sys.exit("%s: these files were edited since a generator wrote them (in Blender?), and this would overwrite "
                     "them:\n%s\nNothing was written. Run it again with --keep-edited to write the rest and leave "
                     "them, or with --force to overwrite them (git has the last committed ones)." % (script, names))

    def restore(self):
        for p, data in self.kept.items():
            with open(p, "wb") as f:
                f.write(data)

    def finish(self):
        """The edited files put back (--keep-edited), and what was written recorded."""
        self.restore()
        manifest = load()
        for p in self.paths:
            r = rel(p)
            if r is None or p in self.kept or not os.path.exists(p):
                continue
            manifest[r] = {"sha256": sha256(p), "by": self.script}
        save(manifest)

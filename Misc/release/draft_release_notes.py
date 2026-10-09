#!/usr/bin/env python3
"""Drafts a release's notes from the commits since the last v* tag, grouped by area (docs/vr-port/RELEASING.md,
"Release notes"): each commit's subject cut to a short bullet, filed under the area its words and files point at.

    python Misc/release/draft_release_notes.py --version 1.0.1 --out out/release/1.0.1/release-notes.md
    python Misc/release/draft_release_notes.py --since v1.0.0 --stdout

The draft starts with a DRAFT marker line: make_release.ps1 -Publish refuses notes that still have it (edit them, then
delete the line; -AutoNotes publishes the draft as it is). Commits that change only tests, docs or the checklist, and
version bumps, are counted but not listed. Nothing is written but --out.
"""

import argparse
import datetime
import os
import re
import subprocess
import sys

MARKER = "<!-- DRAFT"

# (key, heading, subject words, path prefixes). The order breaks ties; "fixes" only wins when nothing else matches
# better (a fix in the reloading is filed under weapons).
AREAS = [
    ("weapons", "Weapons and reloading",
     r"weapon|gun\b|guns\b|shotgun|nailgun|rifle|reload|magazine|\bmags?\b|pump|ammo|holster|pouch|grenade|rocket|"
     r"\baxe\b|crowbar|chainsaw|sword|melee|parry|throw|shell|muzzle|laser|lightning|sight|trigger|recoil|slide|flick",
     ["Misc/quakevr/reload/", "Misc/quakevr/weapon", "QC/vr_reload", "QC/vr_weapon"]),
    ("ai", "AI and stealth",
     r"monster|enem(y|ies)|\bai\b|stealth|grunt|knight|ogre|fiend|shambler|\bdogs?\b|zombie|dragon|scrag|vore|"
     r"chase|notic|alert|foegrab",
     ["QC/vr_stealth", "QC/ai.qc", "QC/monsters/"]),
    ("teleporters", "Teleporters and slipgates",
     r"teleport|slipgate|portal|\bgates?\b",
     ["Quake/vr/vr_portal", "Misc/quakevr/teleporters/"]),
    ("menus", "Menus and interface",
     r"menu|\bhud\b|\bui\b|settings? page|label|checklist|\btips?\b|wrist|gadget|subtitle|font|notify|"
     r"debug >|vr settings|calibration",
     ["Quake/vr/vr_menu", "Quake/vr/vr_hud"]),
    ("physics", "Physics, props and ragdolls",
     r"physic|ragdoll|\bprops?\b|box3d|\bgibs?\b|debris|collision|rigid|carry|\bgrab|climb|swim|crate|decap|barrel|"
     r"zancle|mantle",
     ["Quake/vr/vr_ragdoll", "Quake/vr/vr_box3d", "Quake/vr/vr_physics", "Misc/quakevr/ragdoll/"]),
    ("rendering", "Rendering and performance",
     r"render|\bperf|\bfps\b|frame time|\bgpu\b|shader|relight|\blit\b|shadow|texture|\bfog\b|particle|decal|"
     r"openxr|steamvr|\bxr\b|vdxr|headset|\bglow|bloom|haze|msaa|anisotrop|draw|resolution|eye image",
     ["Quake/gl_", "Quake/r_", "Quake/vr/vr_xr", "Shaders/"]),
    ("installer", "Installer and releases",
     r"installer|release|\bsetup\b|latest\.json|\bfeed\b|package|update notice|qvr-setup|smartscreen",
     ["Installer/", "Misc/release/", "Windows/package-quakevr"]),
    ("expansions", "Expansions and maps",
     r"hipnotic|rogue|\bdopa\b|\bmg1\b|\bmg3\b|machine games|expansion|campaign|map library|arcane|horde|scourge|"
     r"dissolution|dimension of|e\dm\d|\bmaps?\b",
     ["quakevr/maps/", "Misc/quakevr/maps/"]),
    ("fixes", "Fixes",
     r"\bfix|no longer|crash|\bbug|broke|wrong|regression|stuck|instead of|\bnever\b",
     []),
]
OTHER = ("other", "Other changes")

# Not listed: commits that are not about the game a player gets.
SKIP_SUBJECT = re.compile(r"^(checklist:|tests?:|version \d|merge |wip\b|round\d+\.md:|docs?:)", re.I)
SKIP_PATHS = re.compile(r"^(docs/|Misc/quakevr/.*(_test\.(sh|py)|/test\.py|_tests\.sh)$|Misc/quakevr/scratch/|"
                        r"quakevr/checklist|\.claude/|VERSION$)")

MAX_BULLET = 150


def git(root, *args):
    return subprocess.run(["git", "-C", root] + list(args), capture_output=True, text=True, encoding="utf-8",
                          errors="replace").stdout


def last_tag(root, ref):
    out = subprocess.run(["git", "-C", root, "describe", "--tags", "--abbrev=0", "--match", "v[0-9]*", ref],
                         capture_output=True, text=True)
    return out.stdout.strip() if out.returncode == 0 else ""


def commits(root, rng, limit):
    args = ["log", "--no-merges", "--format=%x1e%h%x1f%s", "--name-only"]
    args += [rng] if rng else ["-n", str(limit)]
    out = []
    for chunk in git(root, *args).split("\x1e"):
        if not chunk.strip():
            continue
        head, _, rest = chunk.partition("\n")
        h, _, subject = head.partition("\x1f")
        out.append((h.strip(), subject.strip(), [p for p in rest.splitlines() if p.strip()]))
    return out


def short(subject):
    """The subject cut to a short bullet: its lead clause (before ': ', ' (' or '; ') when it is long, at a word."""
    s = subject.strip().rstrip(".")
    if len(s) > MAX_BULLET:
        for sep in (": ", "; ", " ("):
            i = s.find(sep)
            if 25 <= i <= MAX_BULLET:
                s = s[:i]
                break
    if len(s) > MAX_BULLET:
        s = s[:MAX_BULLET].rsplit(" ", 1)[0].rstrip(",;:") + "..."
    if s.count("(") > s.count(")"):   # cut inside a parenthesis: drop it
        s = s[:s.rfind(" (")] if " (" in s else s.replace("(", "")
    first = s.split(" ", 1)[0]
    return s[:1].upper() + s[1:] if first.isalpha() else s   # (not a cvar's or a file's name)


def area_of(subject, paths):
    low = subject.lower()
    best, best_score = None, 0
    for key, _, words, prefixes in AREAS:
        score = 2 * len(re.findall(words, low))
        score += sum(1 for p in paths if any(p.startswith(x) for x in prefixes))
        if key == "fixes":
            score = min(score, 1)   # a fix-like word only decides when no area says more
        if score > best_score:
            best, best_score = key, score
    return best or OTHER[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
    ap.add_argument("--version", default="", help="the version the notes are for (the heading's comment)")
    ap.add_argument("--since", default="", help="the tag to start from (default: the last v* tag before HEAD)")
    ap.add_argument("--ref", default="HEAD")
    ap.add_argument("--limit", type=int, default=60, help="commits when there is no earlier v* tag")
    ap.add_argument("--per-area", type=int, default=30, help="bullets listed per area (the rest counted)")
    ap.add_argument("--out", default="")
    ap.add_argument("--stdout", action="store_true")
    a = ap.parse_args()

    since = a.since or last_tag(a.root, a.ref)
    rng = f"{since}..{a.ref}" if since else ""
    found = commits(a.root, rng, a.limit)
    groups = {k: [] for k, *_ in AREAS}
    groups[OTHER[0]] = []
    skipped = 0
    seen = set()
    for h, subject, paths in found:
        if SKIP_SUBJECT.match(subject) or (paths and all(SKIP_PATHS.match(p) for p in paths)):
            skipped += 1
            continue
        bullet = short(subject)
        if bullet.lower() in seen:
            skipped += 1
            continue
        seen.add(bullet.lower())
        groups[area_of(subject, paths)].append((h, bullet))

    listed = sum(len(v) for v in groups.values())
    what = f"since {since}" if since else f"(the last {a.limit} commits: no earlier v* tag)"
    lines = [
        f"{MARKER}: drafted {datetime.date.today().isoformat()} by Misc/release/draft_release_notes.py from "
        f"{len(found)} commits {what}{' for ' + a.version if a.version else ''}; {listed} listed, {skipped} not "
        f"(tests, docs, checklist, version bumps, repeats). Edit the bullets, then DELETE THIS LINE: "
        f"make_release.ps1 -Publish refuses notes that still have it. -->",
        "",
    ]
    for key, heading, *_ in AREAS + [OTHER + ("", [])]:
        items = groups[key]
        if not items:
            continue
        lines += [f"## {heading}", ""]
        lines += [f"- {b}" for _, b in items[:a.per_area]]
        if len(items) > a.per_area:
            rest = [h for h, _ in items[a.per_area:]]
            lines.append(f"- ...and {len(rest)} more ({' '.join(rest[:8])}{' ...' if len(rest) > 8 else ''})")
        lines.append("")
    text = "\n".join(lines).rstrip() + "\n"

    if a.stdout or not a.out:
        sys.stdout.write(text)
    if a.out:
        os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
        with open(a.out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print(f"{a.out}: {listed} bullets from {len(found)} commits {what}", file=sys.stderr if a.stdout else sys.stdout)


if __name__ == "__main__":
    main()

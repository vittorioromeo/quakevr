#!/usr/bin/env python3
"""make_shipped_cache.py -- the shipped cache: derived data for Quake VR's own maps, made at release time.

docs/vr-port/CACHES.md, "The shipped cache". The game reads `quakevr/cache_shipped/<product>/<file>` where its own
cache (vr_cache_dir, else <base>/cache) has no file of the same name: index.txt names each shipped file by the cache's
own name, `<product>/<build>/<key>.<ext>` (a hash of everything its maker reads, so a shipped file is read only for
exactly what it was made from), then its file in the folder (`hulls/vrstart_8_8_28.hul`: one name per map and box, so
the next release replaces it in place; Setup's Quest copy never deletes a file).

Each map is loaded alone by the game's preparation run (`-prepare <progress> -preparemaps <map>`: the installer's run,
limited to that map; the mock headset, hidden, no config read from the player's nor written) with an empty
vr_cache_dir and vr_cache_shipped 0, in a copy of the game folder (a package's quakevr, or with --git-root the files
git tracks under it plus progs.dat: package-quakevr.ps1's allowlist), so every file it makes is that map's and made
with the shipped defaults. That run writes every product however quickly this PC made it (the trees' 25 ms bar, the light
grid's 0.1 s) and finishes every one still on the pool (the monsters' deferred trees, the navmesh's size classes) before
the map ends, so any exe on any PC gives the same files, byte for byte (shipcachefix, 2026-11-26: a ThinLTO exe had left
out vrtutorial's light grid and a tree near the bar). The products copied (--products):
  hulls     the world's compiled hull trees (vr_hull.cpp, .hul): the hub's are 42-56 s of its first load on a Quest 3;
            and the map's brushes (.hbr: the walk of hull 0, the clip brushes, the holes; ~10 s of the hub's load on
            a Quest 3, loadprof), listed as "brushes" below
  navmesh   the monsters' navmeshes (vr_nav.cpp), small
  lightgrid the big maps' model light grids (vr_ambient.cpp)
Only Quake VR's own maps (their geometry, lightmaps and entities are the project's; no product of a model, skin or
texture, which may be id Software's: occlusion, normal maps, poses, rigs, gun pieces are never shipped).

  python Misc/release/make_shipped_cache.py --exe <ironwail.exe> --quake <Quake folder with id1> \
      --game <package>/quakevr --work <empty folder> --out <package>/quakevr/cache_shipped
  python Misc/release/make_shipped_cache.py ... --game <checkout>/quakevr --git-root <checkout>   (a development build)

Its last line: "make_shipped_cache: <n> files, <MB> MB (...)" and exit 0, or "make_shipped_cache: FAILED ..." and 1.
Nothing is deleted: --work and --out must be new or empty folders.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import time

DEFAULT_MAPS = ["vrcalibration", "vrtutorial", "vrstart"]
DEFAULT_PRODUCTS = ["hulls", "navmesh", "lightgrid"]
DEV_ONLY = (".gitignore", "motions/")  # package-quakevr.ps1's $devOnly
KEY = re.compile(r"^[0-9a-f]{16}")


def fail(text):
    print(f"make_shipped_cache: FAILED {text}", flush=True)
    sys.exit(1)


def empty_or_new(path, what):
    if os.path.isdir(path) and os.listdir(path):
        fail(f"{what} {path} is not empty (nothing is deleted here: give a new folder)")
    os.makedirs(path, exist_ok=True)


def copy_game(game, git_root, dest):
    """The game folder copied into dest: all of it, or (git_root) what git tracks under quakevr plus progs.dat."""
    if not git_root:
        shutil.copytree(game, dest, ignore=shutil.ignore_patterns("cache_shipped"))
        return
    out = subprocess.run(["git", "-C", git_root, "-c", "core.quotepath=off", "ls-files", "-z", "--", "quakevr"],
                         capture_output=True, check=True).stdout.decode("utf-8")
    rels = [p[len("quakevr/"):] for p in out.split("\0") if p.startswith("quakevr/")]
    rels = [r for r in rels if not any(r == d or (d.endswith("/") and r.startswith(d)) for d in DEV_ONLY)]
    rels.append("progs.dat")
    for rel in rels:
        src = os.path.join(game, rel)
        if not os.path.isfile(src):
            fail(f"{src} is missing (a tracked file, or progs.dat: build first)")
        dst = os.path.join(dest, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)


def run_map(args, base, name, work):
    """One map loaded by the preparation run into its own empty cache; its progress lines."""
    cache = os.path.join(work, "cache")
    os.makedirs(cache, exist_ok=True)
    progress = os.path.join(work, "progress.txt")
    command = [args.exe, "-basedir", args.quake, "-basedir", base, "-game", "quakevr",
               "-prepare", progress, "-preparemaps", name,
               "-noconfigwrite", "-noautoexec", "-nosound", "-nomapindex", "-noaddons", "-condebug",
               "-window", "-width", "640", "-height", "360",
               "+vr_backend", "mock", "+vr_mock_fast", "1", "+vr_cache_shipped", "0", "+vr_cache_dir", cache,
               # (no config: a first start, whose VR Calibration would replace any map but its own, vr_setup.cpp frame)
               "+vr_setup_pending", "0"]
    if args.skill is not None:
        command += ["+skill", str(args.skill)]
    env = dict(os.environ, QVR_TEST_HIDDEN="1", QVR_TEST_BACKGROUND="1", QVR_NO_ERROR_DIALOG="1")
    flags = 0
    if args.idle and os.name == "nt":
        flags = 0x00000040  # IDLE_PRIORITY_CLASS
    t0 = time.monotonic()
    try:
        code = subprocess.run(command, cwd=base, env=env, creationflags=flags, timeout=args.timeout,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
    except subprocess.TimeoutExpired:
        fail(f"{name}: the game did not end within {args.timeout} s")
    lines = open(progress, encoding="utf-8", errors="replace").read().splitlines() if os.path.isfile(progress) else []
    ok = [l for l in lines if l.startswith(f"map {name} ok")]
    if code != 0 or not ok or "done" not in lines:
        fail(f"{name}: exit {code}, progress: {' | '.join(lines[-4:]) or 'none'} (log: {base}/quakevr/qconsole.log)")
    print(f"  {name}: loaded in {ok[0].split()[-1]} s, run {time.monotonic() - t0:.1f} s", flush=True)
    return cache


def collect(cache, name, products, out, index, sizes):
    """The map's files of the products copied to out (`<product>/<map><rest of the name>`) and indexed."""
    for product in products:
        root = os.path.join(cache, product)
        if not os.path.isdir(root):
            continue
        for build in sorted(os.listdir(root)):
            folder = os.path.join(root, build)
            if not os.path.isdir(folder):
                continue
            for file in sorted(os.listdir(folder)):
                if file.endswith(".tmp") or not KEY.match(file):
                    continue
                cache_name = f"{product}/{build}/{file}"
                if cache_name in index:
                    continue  # (the same key from another map: one file)
                rest = file[16:]
                rel = f"{product}/{name}{rest}"
                n = 1
                while rel in index.values():
                    n += 1
                    rel = f"{product}/{name}_{n}{rest}"
                os.makedirs(os.path.join(out, product), exist_ok=True)
                shutil.copyfile(os.path.join(folder, file), os.path.join(out, rel))
                index[cache_name] = rel
                size = os.path.getsize(os.path.join(out, rel))
                kind = "brushes" if product == "hulls" and rest == ".hbr" else product
                count, total = sizes.get((name, kind), (0, 0))
                sizes[(name, kind)] = (count + 1, total + size)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", required=True, help="the engine (a release's ironwail.exe)")
    p.add_argument("--quake", required=True, help="a Quake folder (its id1 with pak0.pak): only read")
    p.add_argument("--game", required=True, help="the quakevr game folder to load (a package's, or a checkout's with --git-root)")
    p.add_argument("--git-root", default="", help="a checkout: only the files git tracks under quakevr (and progs.dat) are copied")
    p.add_argument("--work", required=True, help="an empty or new folder for the runs (the game's copy, each map's cache)")
    p.add_argument("--out", required=True, help="the shipped cache's folder to fill (empty or new)")
    p.add_argument("--maps", default=",".join(DEFAULT_MAPS), help="the maps, comma-separated (default %(default)s)")
    p.add_argument("--products", default=",".join(DEFAULT_PRODUCTS), help="the products (default %(default)s)")
    p.add_argument("--skill", type=int, default=None, help="the skill the maps load at (default: the game's)")
    p.add_argument("--timeout", type=int, default=600, help="a map's run at most (seconds)")
    p.add_argument("--idle", action="store_true", help="the game at idle priority")
    args = p.parse_args()

    maps = [m.strip().lower() for m in args.maps.split(",") if m.strip()]
    products = [x.strip() for x in args.products.split(",") if x.strip()]
    if not os.path.isfile(args.exe):
        fail(f"no engine at {args.exe}")
    if not os.path.isfile(os.path.join(args.quake, "id1", "pak0.pak")):
        fail(f"{args.quake} has no id1/pak0.pak")
    for m in maps:
        if not os.path.isfile(os.path.join(args.game, "maps", m + ".bsp")):
            fail(f"{args.game} has no maps/{m}.bsp (only Quake VR's own maps are shipped)")
    args.exe = os.path.abspath(args.exe)
    args.quake = os.path.abspath(args.quake)
    work = os.path.abspath(args.work)
    out = os.path.abspath(args.out)
    empty_or_new(work, "--work")
    empty_or_new(out, "--out")

    base = os.path.join(work, "base")
    t0 = time.monotonic()
    copy_game(os.path.abspath(args.game), args.git_root, os.path.join(base, "quakevr"))
    print(f"make_shipped_cache: the game folder copied into {base} ({time.monotonic() - t0:.1f} s); maps {', '.join(maps)}",
          flush=True)

    index = {}
    sizes = {}
    for m in maps:
        cache = run_map(args, base, m, os.path.join(work, m))
        collect(cache, m, products, out, index, sizes)

    with open(os.path.join(out, "index.txt"), "w", encoding="ascii", newline="\n") as f:
        f.write("# Quake VR's shipped cache (Misc/release/make_shipped_cache.py; docs/vr-port/CACHES.md): <the cache's name> <the file>\n")
        for cache_name in sorted(index):
            f.write(f"{cache_name} {index[cache_name]}\n")

    total = 0
    for (m, product), (count, size) in sorted(sizes.items()):
        print(f"  {m:14} {product:10} {count:3} files {size / 1048576:8.1f} MB", flush=True)
        total += size
    missing = [m for m in maps if not any(k[0] == m and k[1] == "hulls" for k in sizes)]
    if "hulls" in products and missing:
        # (a tree compiled in under vr_hull.cpp's diskMinMs, 25 ms, is not written: nothing to ship)
        print(f"  no hull trees for {', '.join(missing)} (compiled quickly enough not to be cached)", flush=True)
    if "hulls" in products and "vrstart" in maps and "vrstart" in missing:
        fail("no hull trees made for vrstart (the hub's are the ones that matter)")
    print(f"make_shipped_cache: {len(index)} files, {total / 1048576:.1f} MB ({', '.join(maps)}: {', '.join(products)}) in {out}",
          flush=True)


if __name__ == "__main__":
    main()

"""maplibrary_large_test.py <agent> -- the Map Library's installer on large and hostile packages, from a local server.

No package is downloaded from the internet: test zips are generated in <worktree>/scratch/maplib/, served by a local
HTTP server (127.0.0.1), and named by a test index written over the agent's cached one (put back afterwards). One game
run (run.sh, real time) then installs them through the same code path the Map Library page uses (maps_install):

  big    300 MB of incompressible data and a tiny BSP: downloads, its sha256 checks, installs (the 200 MB limit gone);
         the cache's cap lowered to 1 MB in the middle of it: its zip is never trimmed under it
  low    the same with vr_maps_debug_free_mb 100: refused before the download, saying what is needed and free
  cap    the same with vr_maps_max_download_mb 100: refused ("the package is ...")
  wide   40 MB that unpack to 200 MB with vr_maps_debug_free_mb 200: downloads, refused before unpacking (disk)
  bomb   1 GB of zeros in a 1 MB zip: refused as a zip bomb
  badsha a small zip under the wrong sha256: refused ("its sha256 is ...")

Then the test package is uninstalled and the cache trimmed (in game). Prints PASS/FAIL per case.
Usage: python Misc/quakevr/maplibrary_large_test.py magfix
"""

import hashlib
import http.server
import os
import re
import shutil
import socketserver
import struct
import subprocess
import sys
import threading
import time
import zipfile

KIT = "C:/OHWorkspace/qvr-kit"
PORT = 8766
INDEX_URL = "https://www.quaddicted.com/api/v1/"  # the default vr_maps_index_url: the cache is read, nothing fetched
MB = 1024 * 1024
# Git's bash (a plain "bash" from Python on Windows can be WSL's, which cannot run the kit).
BASH = "C:/Program Files/Git/bin/bash.exe" if os.path.isfile("C:/Program Files/Git/bin/bash.exe") else "bash"


def bsp29() -> bytes:
    # A BSP 29 header (the version is what the installer checks) and a little padding: installed, never loaded.
    return struct.pack("<i", 29) + bytes(15 * 8) + bytes(64)


def make_zips(out: str) -> dict:
    os.makedirs(out, exist_ok=True)
    zips = {}

    def build(name, writer):
        path = os.path.join(out, name + ".zip")
        if not os.path.isfile(path):
            tmp = path + ".tmp"
            with zipfile.ZipFile(tmp, "w", allowZip64=True) as z:
                writer(z)
            os.replace(tmp, path)
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(MB), b""):
                h.update(chunk)
        zips[name] = (h.hexdigest(), os.path.getsize(path))

    def big(z):
        z.writestr("maps/qvrbig.bsp", bsp29())
        with z.open(zipfile.ZipInfo("qvrbig/noise.bin"), "w") as f:  # (stored: incompressible)
            for _ in range(300):
                f.write(os.urandom(MB))

    def wide(z):
        z.writestr("maps/qvrwide.bsp", bsp29())
        info = zipfile.ZipInfo("qvrwide/runs.bin")
        info.compress_type = zipfile.ZIP_DEFLATED
        with z.open(info, "w") as f:
            for _ in range(200):
                seed = os.urandom(MB // 16)
                block = bytearray(MB)
                for i in range(16):
                    block[i::16] = seed  # (each byte 16 times: about 5 to 1)
                f.write(block)

    def bomb(z):
        z.writestr("maps/qvrbomb.bsp", bsp29())
        info = zipfile.ZipInfo("qvrbomb/zeros.bin")
        info.compress_type = zipfile.ZIP_DEFLATED
        with z.open(info, "w") as f:
            zero = bytes(MB)
            for _ in range(1024):
                f.write(zero)

    def small(z):
        z.writestr("maps/qvrsmall.bsp", bsp29())

    build("big", big)
    build("wide", wide)
    build("bomb", bomb)
    build("small", small)
    return zips


def index_line(sha, title, nbytes, startmap, url):
    # sha title author date types modes sizes themes bytes startmap extract progs urls description files rating userRating
    return "\t".join([sha, title, "Quake VR test", "2026-10-07", "map", "singleplayer", "small", "", str(nbytes),
                      startmap, "{base}/", "0", url, "a generated test package", "2", "0", "0"])


class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "magfix"
    tree = f"C:/OHWorkspace/qvr-agents/{name}"
    serve = os.path.join(tree, "scratch", "maplib")
    t0 = time.time()
    zips = make_zips(serve)
    print(f"packages made in {time.time() - t0:.1f} s: " +
          ", ".join(f"{k} {v[1] / MB:.1f} MB" for k, v in zips.items()))

    base = f"{KIT}/bases/{name}/qbase"
    index = os.path.join(base, "cache", "maps_index.txt")
    backup = os.path.join(tree, "scratch", "maps_index.txt.saved")
    if os.path.isfile(index) and not os.path.isfile(backup):
        shutil.copyfile(index, backup)
    url = f"http://127.0.0.1:{PORT}/"
    wrong = "ab" * 32
    lines = ["#quakevr-mapindex-3", "#" + INDEX_URL, "#" + str(int(time.time())),
             index_line(zips["big"][0], "QVR Test Big", zips["big"][1], "qvrbig", url + "big.zip"),
             index_line(zips["wide"][0], "QVR Test Wide", zips["wide"][1], "qvrwide", url + "wide.zip"),
             index_line(zips["bomb"][0], "QVR Test Bomb", zips["bomb"][1], "qvrbomb", url + "bomb.zip"),
             index_line(wrong, "QVR Test BadSha", zips["small"][1], "qvrsmall", url + "small.zip")]
    os.makedirs(os.path.dirname(index), exist_ok=True)
    with open(index, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    handler = lambda *a, **k: Quiet(*a, directory=serve, **k)
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    server = socketserver.ThreadingTCPServer(("127.0.0.1", PORT), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()

    big, wide, bomb = zips["big"][0][:12], zips["wide"][0][:12], zips["bomb"][0][:12]
    script = ";".join([
        "map e1m1", "wait72",
        "vr_maps_debug_free_mb 100", f"maps_install {big}", "wait72",
        "vr_maps_debug_free_mb 0", "vr_maps_max_download_mb 100", f"maps_install {big}", "wait72",
        "vr_maps_max_download_mb 0", f"maps_install {wrong[:12]}", "wait144",
        f"maps_install {bomb}", "wait360",
        "vr_maps_debug_free_mb 200", f"maps_install {wide}", "wait720",
        "vr_maps_debug_free_mb 0", "vr_maps_cache_mb 1", f"maps_install {big}", "wait144", "vr_maps_cache_mb 2",
        "wait1440", "vr_maps_cache_mb 512", "maps_status", "maps_installed", "maps_cache",
        f"maps_uninstall {big}", "vr_maps_cache_mb 0", "maps_cache trim", "maps_cache", "vr_maps_cache_mb 512",
        "toggleconsole", "quit"])
    try:
        run = subprocess.run([BASH, f"{KIT}/run.sh", name, "-RealTime", "-Timeout", "300",
                              "-Filter", r"^maps|^QVR Test|map index|ENGINE", "-Script", script],
                             capture_output=True, text=True, timeout=900)
        out = run.stdout
        if run.returncode or not out.strip():
            print(f"run.sh exit {run.returncode}: {run.stderr.strip()[-2000:]}")
    finally:
        server.shutdown()
        if os.path.isfile(backup):
            shutil.copyfile(backup, index)  # (the agent's own cached index back)
    print(out.strip())
    out = re.sub(r"[ \t]*\r?\n[ \t]*", " ", out)  # (the console's lines wrapped at its width, joined)

    checks = [
        ("low disk refused before the download", r"QVR Test Big - not enough disk space: its zip and its files needs 600\.\d MB"),
        ("vr_maps_max_download_mb refused", r"QVR Test Big - the package is 300\.\d MB \(vr_maps_max_download_mb 100\.0 MB"),
        ("wrong sha256 refused", r"QVR Test BadSha - mirror 1: its sha256 is [0-9a-f]{16}\.\.\., not the index's abababab"),
        ("zip bomb refused", r"QVR Test Bomb - its files would unpack to 1024\.\d MB from a zip of .* refused as a zip bomb"),
        ("low disk refused before unpacking", r"QVR Test Wide - not enough disk space: unpacking its files needs 200\.\d MB"),
        ("300 MB package installed", r"QVR Test Big - 2 file\(s\) written"),
        ("its zip trimmed only after the job (cache cap 1 MB, then 2 MB in the middle)",
         r"download cache \(after the job\): 1 old zip\(s\) removed, 300\.\d MB freed"),
    ]
    failed = 0
    for what, pattern in checks:
        ok = re.search(pattern, out) is not None
        failed += not ok
        print(f"{'PASS' if ok else 'FAIL'}: {what}")
    print("maplibrary_large_test: " + ("all passed" if not failed else f"{failed} FAILED"))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()

"""slipseer_test.py <agent> -- the Map Library's Slipseer source, offline (vr_slipseer.cpp, maps_slipseer_test).

No request reaches slipseer.com: the parser reads the saved pages in Misc/quakevr/slipseer_fixtures/ (trimmed copies,
make_fixtures.py), and the test map's zip (a root-level BSP 29 and a readme, as Slipseer's zips often are) is served
by a local HTTP server (127.0.0.1). One game run (run.sh) then checks, in maps_slipseer_test:

  - the listing page's maps (page, title, author, date, category, what it needs, rating, icon, tag line, next page);
  - two map pages (download link, description with entities decoded, screenshot from og:image or the icon, needs);
  - the entries in the shared index: the source filter (All / Quaddicted / Slipseer), the source tag, the credit,
    what Violacea is made for (Arcane Dimensions);
  - the test map installed through mapinstall::begin (the page's Install), its BSP found (Play's start map),
    uninstalled; no live request made.

The run ends on the test's verdict line ("slipseer_test: N passed, M failed").
Usage: python Misc/quakevr/slipseer_test.py <agent>
"""

import http.server
import os
import re
import socketserver
import struct
import subprocess
import sys
import threading
import zipfile

import qvrtest  # noqa: E402  (the tests' library: Misc/quakevr/qvrtest.py)

KIT = qvrtest.KIT
BASH = qvrtest.BASH
HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")


class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def bsp29() -> bytes:
    # A BSP 29 header (the version is what the installer checks) and a little padding: installed, never loaded.
    return struct.pack("<i", 29) + bytes(15 * 8) + bytes(64)


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "posebake"
    tree = f"C:/OHWorkspace/qvr-agents/{name}"
    serve = os.path.join(tree, "scratch", "slipseer_test")
    os.makedirs(serve, exist_ok=True)
    with zipfile.ZipFile(os.path.join(serve, "qvrslip.zip"), "w") as z:
        z.writestr("qvrslip.bsp", bsp29())
        z.writestr("qvrslip_readme.txt", "Quake VR's Slipseer install test.\n")

    socketserver.ThreadingTCPServer.allow_reuse_address = True
    server = socketserver.ThreadingTCPServer(("127.0.0.1", 0), lambda *a, **k: Quiet(*a, directory=serve, **k))
    port = server.server_address[1]
    threading.Thread(target=server.serve_forever, daemon=True).start()
    fixtures = f"{tree}/Misc/quakevr/slipseer_fixtures"
    script = f'maps_slipseer_test "{fixtures}" http://127.0.0.1:{port}/qvrslip.zip'
    try:
        run = subprocess.run([BASH, f"{KIT}/run.sh", name, "-Timeout", "300", "-Filter", r"slipseer_test",
                              "-QuitOn", r"slipseer_test: [0-9]+ passed", "-Script", script],
                             capture_output=True, text=True, timeout=600)
    finally:
        server.shutdown()
    out = run.stdout
    for line in out.splitlines():
        if "FAIL" in line or "passed" in line:
            print(line.strip())
    m = re.search(r"slipseer_test: (\d+) passed, (\d+) failed", out)
    ok = bool(m) and m.group(2) == "0" and int(m.group(1)) > 0
    print("slipseer_test.py: " + ("PASS" if ok else "FAIL (no verdict)" if not m else "FAIL"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()

"""The in-game update notice (Quake/vr/vr_update.cpp, vr_menubrand.cpp) against a local release feed.

    python Misc/quakevr/update_notice_test.py [agent-name]

A small server on 127.0.0.1 serves latest.json files of a newer, the same and an older version (and a 404), counting
the requests; the kit's run.sh runs the game (real time: the check runs on a thread of its own) with vr_update_url
pointed at them. Checked: a newer feed's notice is drawn and a press opens its page once (dry run: printed); the same
or an older version gives no notice; a 404 is silent (no notice, nothing printed without developer) and the next feed
is tried; the cache (<base>/cache/update_check.txt) is honoured within the hour, across a restart, and not after it;
vr_update_check 0 asks for nothing and shows nothing; the kit's test runs never check at start-up.
The agent's own cache file is put back afterwards.
"""

import http.server
import json
import os
import re
import shutil
import socketserver
import subprocess
import sys
import threading
import time

KIT = "C:/OHWorkspace/qvr-kit"
PORT = 8767
BASH = "C:/Program Files/Git/bin/bash.exe" if os.path.isfile("C:/Program Files/Git/bin/bash.exe") else "bash"
PAGE = "https://github.com/vittorioromeo/quakevr/releases/tag/v0.9.1"

hits = {}
hits_lock = threading.Lock()


class Counting(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        with hits_lock:
            hits[self.path] = hits.get(self.path, 0) + 1
        super().do_GET()

    def log_message(self, *a):
        pass


def feed(version, page=None):
    f = {"schema": 1, "version": version,
         "package": {"file": "QuakeVR.zip", "size": 1, "sha256": "0" * 64, "urls": [f"http://127.0.0.1:{PORT}/QuakeVR.zip"]},
         "components": {}}
    if page:
        f["page"] = page
    return f


def hit(path):
    with hits_lock:
        return hits.get(path, 0)


def run(name, script, out=None):
    args = [BASH, f"{KIT}/run.sh", name, "-RealTime", "-Timeout", "180",
            "-Filter", r"^update check|^menu link|update notice|^vr_update|ENGINE", "-Script", ";".join(script)]
    if out:
        args[3:3] = ["-Clean", "-Out", out]
    r = subprocess.run(args, capture_output=True, text=True, timeout=600)
    if r.returncode or not r.stdout.strip():
        print(f"run.sh exit {r.returncode}: {r.stderr.strip()[-2000:]}")
    return re.sub(r"[ \t]*\r?\n[ \t]*", " ", r.stdout)  # (the console's wrapped lines joined)


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "updnotice"
    tree = f"C:/OHWorkspace/qvr-agents/{name}"
    serve = os.path.join(tree, "scratch", "updfeed")
    os.makedirs(serve, exist_ok=True)
    for file, f in {"newer.json": feed("0.9.1 (2026-10-08 abcdef12)", PAGE), "same.json": feed("0.9.0 (2026-10-01 12345678)"),
                    "older.json": feed("0.8.2 (2026-09-01 12345678)"), "cached.json": feed("0.9.2 (2026-10-09 0badf00d)")}.items():
        with open(os.path.join(serve, file), "w", newline="\n") as fh:
            json.dump(f, fh)

    cache = f"{KIT}/bases/{name}/qbase/cache/update_check.txt"
    backup = os.path.join(tree, "scratch", "update_check.txt.saved")
    had_cache = os.path.isfile(cache)
    if had_cache:
        shutil.copyfile(cache, backup)

    handler = lambda *a, **k: Counting(*a, directory=serve, **k)
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    server = socketserver.ThreadingTCPServer(("127.0.0.1", PORT), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{PORT}"
    checks = []

    def check(what, ok):
        checks.append((what, bool(ok)))

    try:
        # 1. A newer version: the notice, one press (and a second within the repeat guard) opens its page once.
        out = run(name, ["developer 0", f"vr_update_url {base}/newer.json", "wait5", "vr_update_status", "vr_update_check_now", "wait240",
                         "vr_update_status", "menu_main", "wait30", "vr_mock_laser update", "wait10",
                         "vr_mock_button main trigger 1", "wait3", "vr_mock_button main trigger 0", "wait3",
                         "vr_mock_button main trigger 1", "wait3", "vr_mock_button main trigger 0", "wait3",
                         "menu_vr pos", "toggleconsole", "quit"])
        print("--- newer:", out.strip()[:1500])
        check("test run: no start-up check", "the start-up check: not run (a test run" in out)
        check("newer: answered", "update check: latest 0.9.1 (2026-10-08 abcdef12) from " + base + "/newer.json (asked): newer" in out)
        check("newer: notice with the feed's page", f'notice "Update available: Quake VR: Unleashed 0.9.1" -> {PAGE}' in out)
        check("newer: notice drawn", 'update notice "Update available: Quake VR: Unleashed 0.9.1"' in out)
        check("newer: a press opens the page once", out.count("menu link: opening") == 1 and f"menu link: opening {PAGE} (1) (dry run" in out)
        check("newer: one request", hit("/newer.json") == 1)

        # 2. The same version, then an older one: no notice. 3. A 404: silent, then the next feed answers.
        out = run(name, ["developer 0", f"vr_update_url {base}/same.json", "vr_update_check_now", "wait180", "vr_update_status",
                         f"vr_update_url {base}/older.json", "vr_update_check_now", "wait180", "vr_update_status",
                         "menu_main", "wait20", "menu_vr pos",
                         f"vr_update_url {base}/missing.json", "vr_update_check_now startup", "wait180", "echo vr_update_404_done",
                         "vr_update_status",
                         f'vr_update_url "{base}/missing.json {base}/newer.json"', "vr_update_check_now", "wait240", "vr_update_status",
                         "toggleconsole", "quit"])
        print("--- same/older/404:", out.strip()[:2500])
        parts = out.split("vr_update_404_done")
        check("same: not newer, no notice", re.search(r"latest 0\.9\.0 \(2026-10-01 12345678\) from \S+same\.json \(asked\): not newer .*?update check: no notice", out))
        check("older: not newer, no notice", re.search(r"latest 0\.8\.2 \(2026-09-01 12345678\) from \S+older\.json \(asked\): not newer .*?update check: no notice", out))
        check("older: no notice drawn", "update notice not drawn (no newer version known)" in out)
        between = parts[0].split("missing.json (a fresh cached answer is taken)")[-1] if len(parts) > 1 else "?"
        check("404: silent (nothing printed without developer)", len(parts) > 1 and "update check:" not in between)
        check("404: status says why", re.search(r"failed \(\S+missing\.json: HTTP 404\).*?no notice", parts[-1]))
        check("404 then the next feed answers", hit("/missing.json") == 2
              and re.search(r"latest 0\.9\.1 \(2026-10-08 abcdef12\) from \S+newer\.json \(asked\): newer", parts[-1]))

        # 4. The cache: the start-up check asks once, then takes the cached answer (and again after a restart); an
        # answer over an hour old is asked for again. 5. vr_update_check 0: nothing asked for, no notice.
        n0 = hit("/cached.json")
        out = run(name, ["developer 1", f"vr_update_url {base}/cached.json", "vr_update_check_now startup", "wait180",
                         "vr_update_check_now startup", "wait180", "vr_update_status", "toggleconsole", "quit"])
        print("--- cache 1:", out.strip()[:1500])
        check("cache: asked once in a run of two start-up checks", hit("/cached.json") - n0 == 1)
        check("cache: the second from the cache", "from " + base + "/cached.json (cached): newer" in out)
        out = run(name, ["developer 1", f"vr_update_url {base}/cached.json", "vr_update_check_now startup", "wait180",
                         "vr_update_status", "toggleconsole", "quit"])
        print("--- cache 2:", out.strip()[:1200])
        check("cache: honoured after a restart", hit("/cached.json") - n0 == 1 and "(cached): newer" in out)
        text = open(cache).read().split("\n")
        text[2] = "#" + str(int(time.time()) - 3700)
        with open(cache, "w", newline="\n") as fh:
            fh.write("\n".join(text))
        out = run(name, ["developer 1", f"vr_update_url {base}/cached.json", "vr_update_check_now startup", "wait180",
                         "vr_update_status", "vr_update_check 0", "vr_update_check_now startup", "wait60", "vr_update_status",
                         "menu_main", "wait20", "menu_vr pos", "toggleconsole", "quit"])
        print("--- cache 3 / off:", out.strip()[:2000])
        check("cache: over an hour old: asked again", hit("/cached.json") - n0 == 2 and "(asked): newer" in out)
        check("off: nothing asked for", hit("/cached.json") - n0 == 2 and "startup: off (vr_update_check 0): nothing asked for" in out)
        check("off: no notice", "update notice not drawn (no newer version known)" in out)
    finally:
        server.shutdown()
        if had_cache:
            shutil.copyfile(backup, cache)
    print(f"requests: {dict(sorted(hits.items()))}")
    failed = 0
    for what, ok in checks:
        failed += not ok
        print(f"{'PASS' if ok else 'FAIL'}: {what}")
    print("update_notice_test: " + ("all passed" if not failed else f"{failed} FAILED"))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()

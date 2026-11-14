"""mp_harness.py -- runs several headless Quake VR games on this machine as one multiplayer game, for tests.

A Session starts a server (a dedicated one, or a listen host that also plays) and clients (VR with the mock headset,
or flatscreen), each a separate process with its own console log and command inbox (-condebugfile, -cmdinbox:
Quake/vr/vr_mptest.cpp); a client may talk to the server through an mp_proxy.Proxy that adds lag, jitter and loss.
The scenario drives them by appending console commands to their inboxes (Instance.cmd) and waits on their logs for
lines (Instance.wait: a regex and a safety timeout); the clients print, wall-clock stamped (vr_mp_watch 1), when
they join a map, first see each player, see one start moving from rest. Checks add up to a verdict line,
"<test>: N passed, M failed". Docs: docs/vr-port/MP_PLAN.md, "Test harness".

The games are hidden (QVR_TEST_HIDDEN), run at below-normal priority (idle while the author's own game runs), use
small mock eyes, write no config (-noconfigwrite) and keep real time: several processes talking over UDP share the
wall clock, so these tests are real-time by nature (vr_fixed_frames 0); every wait is still on a condition.
"""

import json
import os
import re
import subprocess
import time

from mp_proxy import Proxy

GAMES = ["-game", "hipnotic", "-game", "rogue", "-game", "quakevr"]
BELOW_NORMAL = 0x00004000
IDLE = 0x00000040
NO_WINDOW = 0x08000000

FIELD = re.compile(r'(\w+)=("[^"]*"|\S+)')


def fields(line):
    """The key=value pairs of a harness line (mp_seen: t=... ent=2 ...) as a dict of strings."""
    return {k: v.strip('"') for k, v in FIELD.findall(line)}


# Each finished scenario's client links through a proxy: (test, Proxy.window over the whole run) (mp_test.py suite's
# byte table, MP_PLAN B7).
LINKS = []


def author_game_running():
    """The author's own game (a build from his checkout, quakevr-iw) is running: the tests keep out of its way."""
    try:
        out = subprocess.run(
            ["powershell", "-NoProfile", "-Command",
             "Get-Process ironwail -ErrorAction SilentlyContinue | Where-Object { $_.Path -match 'quakevr-iw\\\\Windows' } "
             "| Measure-Object | Select-Object -ExpandProperty Count"],
            capture_output=True, text=True, timeout=30).stdout.strip()
        return out not in ("", "0")
    except (OSError, subprocess.SubprocessError):
        return False


class StepFailed(Exception):
    pass


class Instance:
    def __init__(self, session, name, port, dedicated=0, flat=False, extra=()):
        self.s = session
        self.name = name
        self.port = port
        self.dedicated = dedicated
        self.flat = flat
        self.extra = list(extra)
        self.log = os.path.join(session.out, name + ".log")
        self.inbox = os.path.join(session.out, name + ".inbox")
        self.proc = None
        self.lines = []
        self.pos = 0
        self.part = ""

    def args(self):
        a = [self.s.exe, "-basedir", self.s.base] + GAMES + [
            "-condebug", "-condebugfile", self.log, "-cmdinbox", self.inbox,
            "-foreignconfigs", "-noconfigwrite", "-noaddons", "-nosound",
            "-ip", "127.0.0.1", "-port", str(self.port)]
        if self.dedicated:
            a += ["-dedicated", str(self.dedicated)]
        else:
            a += ["-window", "-width", "480", "-height", "270"]
            if self.flat:
                a += ["-flatscreenmode"]
        # (run before the first frame, after the configs: "+cmd args" on the command line)
        a += ["+vr_backend", "mock", "+sv_autosave", "0", "+vr_log_flush_frames", "1", "+vr_fixed_frames", "0",
              "+vr_mock_eye_size", "256", "+vr_mp_watch", "1"]
        return a + self.extra

    def start(self):
        with open(self.inbox, "w", encoding="ascii"):
            pass
        env = dict(os.environ, QVR_TEST_HIDDEN="1", QVR_NO_ERROR_DIALOG="1", QVR_TEST_BACKGROUND="1",
                   SDL_ASSERT="abort")
        flags = (IDLE if self.s.low_impact else BELOW_NORMAL) | NO_WINDOW
        self.proc = subprocess.Popen(self.args(), cwd=self.s.base, env=env, creationflags=flags,
                                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.s.note("%s: started (pid %d, port %d%s)" % (
            self.name, self.proc.pid, self.port,
            ", dedicated" if self.dedicated else ", flat" if self.flat else ", VR mock"))
        return self

    def cmd(self, text):
        """Console commands (';' separates them), run at this game's next frame."""
        with open(self.inbox, "a", encoding="ascii") as f:
            f.write(text + "\n")

    def read(self):
        if not os.path.exists(self.log):
            return
        with open(self.log, "rb") as f:
            f.seek(self.pos)
            data = f.read()
        self.pos += len(data)
        text = self.part + data.decode("utf-8", "replace")
        cut = text.rfind("\n")
        if cut < 0:
            self.part = text
            return
        self.lines += text[:cut].split("\n")
        self.part = text[cut + 1:]

    def mark(self):
        """The log's line count now: wait(after=mark()) sees only lines printed from here on."""
        self.read()
        return len(self.lines)

    def find(self, pattern, after=0):
        rx = re.compile(pattern)
        for i in range(after, len(self.lines)):
            if rx.search(self.lines[i]):
                return i, self.lines[i]
        return None

    def wait(self, pattern, timeout, after=0, what=None):
        """The first line from `after` on matching `pattern` -> (index, line). Fails the step after `timeout` seconds
        (a safety net: a test ends on its conditions) or when the game has exited."""
        deadline = time.monotonic() + timeout * self.s.time_scale
        while True:
            self.read()
            hit = self.find(pattern, after)
            if hit:
                return hit
            if self.proc and self.proc.poll() is not None:
                raise StepFailed("%s exited (code %s) waiting for %s" % (self.name, self.proc.returncode, what or pattern))
            if time.monotonic() > deadline:
                raise StepFailed("%s: timed out after %g s waiting for %s" % (self.name, timeout, what or pattern))
            time.sleep(0.02)

    def quiet(self, pattern, seconds, timeout, what=None):
        """Waits until no line matching `pattern` has been printed for `seconds` (e.g. a player at rest)."""
        deadline = time.monotonic() + timeout * self.s.time_scale
        rx = re.compile(pattern)
        last_n = self.mark()
        since = time.monotonic()
        while True:
            self.read()
            if any(rx.search(l) for l in self.lines[last_n:]):
                last_n = len(self.lines)
                since = time.monotonic()
            if time.monotonic() - since >= seconds:
                return
            if self.proc and self.proc.poll() is not None:
                raise StepFailed("%s exited waiting for quiet: %s" % (self.name, what or pattern))
            if time.monotonic() > deadline:
                raise StepFailed("%s: never quiet (%s) within %g s" % (self.name, what or pattern, timeout))
            time.sleep(0.02)

    def screenshot(self, label, timeout=30):
        """A screenshot (the window: the mock headset's left eye) moved into the session's folder as <name>_<label>.png
        -> its path. The game writes it on a thread under another name and renames it when complete: the wait is on the
        file (a safety timeout)."""
        since = self.mark()
        self.cmd('cl_screenshotname "screenshots/mp_%s_%s"; screenshot' % (self.name, label))
        _, line = self.wait(r"Wrote .*mp_%s_%s" % (self.name, label), timeout, since, "%s's screenshot" % self.name)
        rel = line.split("Wrote ", 1)[1].strip()
        found = None
        deadline = time.monotonic() + timeout * self.s.time_scale
        while not found:
            for game in [g for g in GAMES if g != "-game"][::-1] + ["id1"]:
                p = os.path.join(self.s.base, game, rel)
                if os.path.exists(p):
                    found = p
                    break
            if not found:
                if time.monotonic() > deadline:
                    raise StepFailed("%s: the screenshot %s never appeared" % (self.name, rel))
                time.sleep(0.05)
        out = os.path.join(self.s.out, "%s_%s.png" % (self.name, label))
        os.replace(found, out)  # (moved out of the shared base)
        return out

    def stop(self, timeout=20):
        """quit, then the exit code (None: had to be killed)."""
        if not self.proc:
            return None
        if self.proc.poll() is None:
            self.cmd("disconnect; quit" if not self.dedicated else "quit")
            try:
                self.proc.wait(timeout)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
                self.s.note("%s: did not quit within %d s: killed" % (self.name, timeout))
                return None
        self.read()
        return self.proc.returncode


class Session:
    def __init__(self, exe, base, out, test, port=27500, time_scale=1.0):
        self.exe = os.path.abspath(exe)
        self.base = os.path.abspath(base)
        self.test = test
        self.out = os.path.abspath(os.path.join(out, "%s_%s" % (test, time.strftime("%Y%m%d_%H%M%S"))))
        os.makedirs(self.out, exist_ok=True)
        self.port = port
        self.next_port = port + 1
        self.time_scale = time_scale
        self.instances = []
        self.proxies = []
        self.checks = []
        self.metrics = {}
        self.notes = []
        self.low_impact = author_game_running()
        if self.low_impact:
            self.note("the author's game is running: idle priority (timings are not representative)")

    def note(self, text):
        self.notes.append(text)
        print("  " + text, flush=True)

    def server(self, dedicated=True, maxplayers=4, flat=False, name="server"):
        inst = Instance(self, name, self.port, dedicated=maxplayers if dedicated else 0, flat=flat)
        self.instances.append(inst)
        return inst

    def client(self, name, flat=False, lag_ms=0.0, jitter_ms=0.0, loss=0.0, seed=1, proxy=False, extra=()):
        """A client; with lag, jitter or loss (or proxy=True: to count its traffic) it talks to the server through a
        proxy of its own. connect_cmd() is the console command that joins the server."""
        inst = Instance(self, name, self.take_port(), flat=flat, extra=extra)
        inst.target = self.port
        if proxy or lag_ms or jitter_ms or loss:
            p = Proxy(self.take_port(), self.port, lag_ms, jitter_ms, loss, seed).start()
            self.proxies.append(p)
            inst.target = p.listen_port
            inst.proxy = p
            self.note("%s: through a proxy (lag %g ms each way, jitter %g ms, loss %g%%)" % (
                name, lag_ms, jitter_ms, loss * 100))
        self.instances.append(inst)
        return inst

    def take_port(self):
        self.next_port += 1
        return self.next_port

    def connect_cmd(self, inst):
        return "connect 127.0.0.1:%d" % inst.target

    def check(self, name, ok, detail=""):
        self.checks.append((name, bool(ok), detail))
        print("  %s %s%s" % ("PASS" if ok else "FAIL", name, (": " + detail) if detail else ""), flush=True)
        return ok

    def metric(self, name, value):
        self.metrics[name] = value

    def finish(self, failure=None):
        if failure:
            self.check("scenario", False, str(failure))
        # Clients first, the server last (its exit code: a clean quit, MULTIPLAYER.md's old crash on quit).
        for inst in sorted(self.instances, key=lambda i: 1 if i.name == "server" else 0):
            code = inst.stop()
            crashed = any("ENGINE CRASH" in l for l in inst.lines)
            self.check("%s quits cleanly" % inst.name, code == 0 and not crashed,
                       "exit %s%s" % (code, ", ENGINE CRASH in its log" if crashed else ""))
        for p in self.proxies:
            p.stop()
            self.note(p.summary())
            if p.first is not None and p.last is not None and p.last > p.first:
                w = p.window((p.first, {d: [0, 0, 0, 0] for d in ("up", "down")}), (p.last, p.stats))
                self.metric("link_%d" % p.listen_port, w)
                LINKS.append((self.test, w))
                print("mp_bytes: %s up %d B/s (mean %d B) down %d B/s (mean %d B)" % (
                    self.test, w["up"]["bytes_s"], w["up"]["mean_B"], w["down"]["bytes_s"], w["down"]["mean_B"]),
                    flush=True)
        passed = sum(1 for c in self.checks if c[1])
        failed = len(self.checks) - passed
        summary = {"test": self.test, "passed": passed, "failed": failed,
                   "checks": [{"name": n, "ok": o, "detail": d} for n, o, d in self.checks],
                   "metrics": self.metrics, "notes": self.notes,
                   "logs": [i.log for i in self.instances]}
        with open(os.path.join(self.out, "summary.json"), "w", encoding="utf-8") as f:
            json.dump(summary, f, indent=1)
        print("%s: %d passed, %d failed (logs: %s)" % (self.test, passed, failed, self.out), flush=True)
        return failed == 0

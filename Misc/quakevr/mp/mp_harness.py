"""mp_harness.py -- runs several headless Quake VR games on this machine as one multiplayer game, for tests.

A Session starts a server (a dedicated one, or a listen host that also plays) and clients (VR with the mock headset,
or flatscreen), each a separate process with its own console log and command inbox (-condebugfile, -cmdinbox:
Quake/vr/vr_mptest.cpp); a client may talk to the server through an mp_proxy.Proxy that adds lag, jitter and loss.
The scenario drives them by appending console commands to their inboxes (Instance.cmd) and waits on their logs for
lines (Instance.wait: a regex and a safety timeout); the clients print, wall-clock stamped (vr_mp_watch 1), when
they join a map, first see each player, see one start moving from rest. Checks add up to a verdict line,
"<test>: N passed, M failed". Docs: docs/vr-port/MP_PLAN.md, "Test harness".

The games are hidden (QVR_TEST_HIDDEN), run at normal priority (B25; idle while the author's own game runs), use
small mock eyes, write no config (-noconfigwrite) and keep real time: several processes talking over UDP share the
wall clock, so these tests are real-time by nature (vr_fixed_frames 0); every wait is still on a condition.
"""

import json
import os
import re
import shutil
import subprocess
import time

from mp_proxy import Proxy

GAMES = ["-game", "hipnotic", "-game", "rogue", "-game", "quakevr"]
NORMAL = 0x00000020
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

# The kinds of B12's byte table (vr_net_bytes's fields): the snapshot's entries (the player state, entities, avatars, the
# walk's state, the armed humanoids' poses, its header) and what goes outside it.
KINDS = ("snap_cd", "snap_ent", "snap_av", "snap_pred", "snap_pose", "snap_hdr", "pose", "avatar", "ent", "cd", "sound",
         "tent", "rest", "reliable")


def kind_rates(kinds, down_bytes_s):
    """vr_net_bytes's fields -> each kind's share of the proxy's down rate, B/s ({} none)."""
    try:
        total = int(kinds["reliable"]) + int(kinds["unreliable"])
    except (KeyError, ValueError):
        return {}
    if total <= 0:
        return {}
    return {k: int(round(down_bytes_s * int(kinds.get(k, 0)) / total)) for k in KINDS}


def format_kinds(rates):
    return " ".join("%s=%d" % (k, rates[k]) for k in KINDS if rates.get(k))


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


# --- (B24) No test talks to the internet --------------------------------------------------------------------------
# A B23 run's dedicated server looked up the shipped room service (vittorioromeo.com) and sent it datagrams. Three
# fences: every game starts with -loopbackonly (net_main.c: the engine resolves and sends to 127.0.0.0/8 only, prints
# "NET SAFETY:" for each refusal); the harness refuses to start a game, or send it a command, that would point it at
# another host (net_safety_problem: vr_mp_room on with vr_mp_rendezvous elsewhere, a "connect" elsewhere, a room-code
# join through a service elsewhere); and each scenario's end asks every game its vr_mp_room and vr_mp_rendezvous and
# reads every log for "NET SAFETY:" (a failed check: "every game kept to loopback"). Names are never looked up here: only
# a literal 127.x.y.z or "localhost" counts as loopback.

ROOM_SERVICE_DEFAULT = "vittorioromeo.com:26100"  # (vr_mp_rendezvous's default: a game that never set it)
LOOPBACK_HOST = re.compile(r"^(127\.\d{1,3}\.\d{1,3}\.\d{1,3}|localhost)\.?$", re.I)
NET_SAFETY_LINE = "NET SAFETY:"


def loopback_endpoint(value):
    """A "host[:port]" (as vr_mp_rendezvous or connect take it) on this machine -> True; "" (none) also True."""
    host = value.strip().strip('"').strip()
    if not host:
        return True
    if host.count(":") == 1:
        host = host.split(":", 1)[0]
    return bool(LOOPBACK_HOST.match(host))


def room_on(value):
    try:
        return float(value.strip().strip('"') or "0") != 0
    except ValueError:
        return True  # (unreadable: assume on)


def net_safety_problem(room, rendezvous):
    """The reason a game with these values would contact a host off this machine (None: it would not)."""
    if room_on(room) and not loopback_endpoint(rendezvous):
        return "vr_mp_room %s with vr_mp_rendezvous %s (not loopback)" % (room, rendezvous)
    return None


def launch_net_values(args):
    """A command line's effective vr_mp_room and vr_mp_rendezvous ("+cvar value", the last one wins)."""
    room, rendezvous = "1", ROOM_SERVICE_DEFAULT
    for i, a in enumerate(args[:-1]):
        if a.lower() == "+vr_mp_room":
            room = args[i + 1]
        elif a.lower() == "+vr_mp_rendezvous":
            rendezvous = args[i + 1]
    return room, rendezvous


COMMAND_SPLIT = re.compile(r';|\n')


def command_net_problem(text, state):
    """Console text (';' separated) a game will run -> the reason it would contact a host off this machine (None: it
    would not). `state` ({"room", "rendezvous"}) follows its vr_mp_room / vr_mp_rendezvous settings."""
    for part in COMMAND_SPLIT.split(text):
        words = part.strip().split(None, 1)
        if not words:
            continue
        name, rest = words[0].lower(), (words[1].strip() if len(words) > 1 else "")
        if name == "vr_mp_room" and rest:
            state["room"] = rest.split()[0]
        elif name == "vr_mp_rendezvous" and rest:
            state["rendezvous"] = rest.split()[0].strip('"')
        elif name == "connect" and rest:
            target = rest.split()[0].strip('"')
            if target.startswith("@"):
                if not loopback_endpoint(state["rendezvous"]):
                    return "connect %s through the room service %s (not loopback)" % (target, state["rendezvous"])
            elif not loopback_endpoint(target):
                return "connect %s (not loopback)" % target
        else:
            continue
        problem = net_safety_problem(state["room"], state["rendezvous"])
        if problem:
            return problem
    return None


PORT_BLOCK = 32  # (mpopen) the ports one scenario may use from its base port: the server's, then each client's and proxy's


def busy_ports(port, count=PORT_BLOCK):
    """The ports of [port, port + count) another game or process holds on 127.0.0.1 (UDP: the games' sockets)."""
    import socket
    busy = []
    for p in range(port, port + count):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.bind(("127.0.0.1", p))
        except OSError:
            busy.append(p)
        finally:
            sock.close()
    return busy


def default_base(repo):
    """(backlog1122b) Each checkout's own game folder: the kit's bases/<checkout name>/qbase (new_agent.sh's, the one
    run.sh's runs use), so two workers' suites no longer share bases/mp/qbase (its autoexec.cfg, saves, crash folder).
    None if the checkout has none (the author's: --base is then required)."""
    import sys
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    import qvrtest  # (Misc/quakevr: the kit's folder, qvrtest.KIT)
    path = os.path.join(qvrtest.KIT, "bases", os.path.basename(os.path.normpath(repo)), "qbase")
    return path if os.path.isdir(os.path.join(path, "id1")) else None


def default_base_port(repo):
    """(mpopen) Each checkout its own block of ports (27600 on; never the shared 27500): two workers' suites at once
    no longer took each other's server port ("server exited waiting for the server on e1m1")."""
    import zlib
    return 27600 + (zlib.crc32(os.path.abspath(repo).lower().encode("utf-8")) % 60) * PORT_BLOCK


def free_base_port(port, tries=40):
    """`port` if its block is free, else the next free block -> (base port, the busy ports found at `port`)."""
    first = busy_ports(port)
    if not first:
        return port, []
    for k in range(1, tries):
        p = port + k * PORT_BLOCK
        if p + PORT_BLOCK < 65536 and not busy_ports(p):
            return p, first
    return port, first


class Instance:
    def __init__(self, session, name, port, dedicated=0, flat=False, extra=(), net_guard_test=False):
        self.s = session
        self.name = name
        self.port = port
        self.dedicated = dedicated
        self.flat = flat
        self.extra = list(extra)
        # (B24) mp_netsafe's own game: pointed off this machine on purpose to see -loopbackonly refuse it (its launch and
        # commands are not checked here, its NET SAFETY lines expected; the engine's fence is still on)
        self.net_guard_test = net_guard_test
        self.net_state = None
        self.size = (480, 270)  # (the window: a readable screenshot sets more before start(); mp_test.py roomui)
        # (the sync audit, 2026-11-21: sound on, through SDL's dummy driver -- nothing reaches a speaker -- so vr_mp_near
        # lists the sounds this game started; set before start())
        self.sound = False
        # (B25: each game its own vr_mp_clientkey, as two real games have; a game "back from a crash" (mp_rejoin's B2)
        # is started with the crashed one's)
        self.clientkey = ("qvrtest%s%d" % (re.sub(r"\W", "", name), port))[:23]
        self.log = os.path.join(session.out, name + ".log")
        self.inbox = os.path.join(session.out, name + ".inbox")
        self.proc = None
        self.lines = []
        self.pos = 0
        self.part = ""

    def args(self):
        a = [self.s.exe, "-basedir", self.s.base] + GAMES + [
            "-condebug", "-condebugfile", self.log, "-cmdinbox", self.inbox,
            "-foreignconfigs", "-noconfigwrite", "-noaddons", "-nomic",
            # (testspeed: no start-up map, the vrstart hub a VR game loads before anything: 6 s per game before its
            # connect or map command could run; a listen host's "disconnect;" in join() is then a no-op;
            # QVR_MP_STARTMAP=1: the hub as before, to compare)
            ] + ([] if os.environ.get("QVR_MP_STARTMAP") == "1" else ["-nostartmap"]) + [
            "-ip", "127.0.0.1", "-port", str(self.port)] + ([] if self.sound else ["-nosound"])
        if self.dedicated:
            a += ["-dedicated", str(self.dedicated)]
        else:
            a += ["-window", "-width", str(self.size[0]), "-height", str(self.size[1])]
            if self.flat:
                a += ["-flatscreenmode"]
        # (run before the first frame, after the configs: "+cmd args" on the command line)
        # (B17: vr_mock_hand_to puts the hand at its point from the play space's place, not by a step from the hand's
        # last drawn place. That place lags the commands under load and is held out by walls and floors: a hand sent at
        # a far weapon its view still drew as lying came back a unit a command (mp_ground), a hand reaching the floor
        # for a shell went to the pouch 8 units low (mp_disarm); the suite's flakes, alone they passed.)
        # (B23: vr_mp_room 0: a test's host never registers with the shipped room service, an internet host; mp_nat
        # turns it on with a local service)
        a += ["+vr_backend", "mock", "+sv_autosave", "0", "+vr_log_flush_frames", "1", "+vr_fixed_frames", "0",
              "+vr_mock_eye_size", "256", "+vr_mp_watch", "1", "+vr_mock_hand_to_absolute", "1", "+vr_mp_room", "0",
              "+vr_mp_clientkey", self.clientkey]
        # (flatgreen3: without the hub a game keeps the campaign its -game list starts it in, rogue (GAMES' last): the hub
        # (vrstart) selected Quake's. A client never takes the server's campaign, so on the server's `changelevel start`
        # it read rogue's start.bsp ("Host_Error: Model *43 not found": mp_predict, mp_predictclimb) and its Load
        # Autosave list hid the server's Quake coop autosave as another game's (mp_flow). Quake's selected here, as the
        # hub did, without its map load.)
        if not self.dedicated and os.environ.get("QVR_MP_STARTMAP") != "1":
            a += ["+vr_campaign_select", "id1"]
        a = a + self.extra
        # (B24: the engine's fence, last so nothing in `extra` can take it out)
        return a[:1] + ["-loopbackonly"] + a[1:]

    def start(self):
        args = self.args()
        room, rendezvous = launch_net_values(args)
        self.net_state = {"room": room, "rendezvous": rendezvous}
        problem = net_safety_problem(room, rendezvous)
        if problem and not self.net_guard_test:
            raise StepFailed("NET SAFETY: %s would start with %s: not started" % (self.name, problem))
        with open(self.inbox, "w", encoding="ascii"):
            pass
        env = dict(os.environ, QVR_TEST_HIDDEN="1", QVR_NO_ERROR_DIALOG="1", QVR_TEST_BACKGROUND="1",
                   SDL_ASSERT="abort")
        if self.sound:
            env["SDL_AUDIODRIVER"] = "dummy"
        # (B25: normal priority, as the kit's runs: below normal, every build and fast-mode test of other work on the
        # machine came first and these real-time games went without -- under mp_load's busy workers a dedicated server
        # did not print its first lines in 60 s; QVR_MP_PRIORITY=below for the old way)
        below = os.environ.get("QVR_MP_PRIORITY") == "below"
        flags = (IDLE if self.s.low_impact else BELOW_NORMAL if below else NORMAL) | NO_WINDOW
        # (Linux too: Windows' priority and no-window flags are Windows' only; Popen refuses them elsewhere)
        self.proc = subprocess.Popen(args, cwd=self.s.base, env=env, creationflags=flags if os.name == "nt" else 0,
                                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.s.note("%s: started (pid %d, port %d%s)" % (
            self.name, self.proc.pid, self.port,
            ", dedicated" if self.dedicated else ", flat" if self.flat else ", VR mock"))
        return self

    def cmd(self, text):
        """Console commands (';' separates them), run at this game's next frame."""
        if self.net_state is not None and not self.net_guard_test:
            state = dict(self.net_state)
            problem = command_net_problem(text, state)
            if problem:
                raise StepFailed("NET SAFETY: %s: not sent (%r would %s)" % (self.name, text, problem))
            self.net_state = state
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
                # (mpopen: its last lines in the failure -- a bind refused, an error at start -- not only its exit)
                self.read()
                tail = " | ".join(l.strip() for l in self.lines[-3:] if l.strip())
                raise StepFailed("%s exited (code %s) waiting for %s; its last lines: %s" % (
                    self.name, self.proc.returncode, what or pattern, tail or "-"))
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
        self.crash_dir = os.path.join(self.base, "quakevr", "crash")  # (B26: crash_reports)
        self.crashes_before = self.crash_listing()
        self.low_impact = author_game_running()
        if self.low_impact:
            self.note("the author's game is running: idle priority (timings are not representative)")
        elif os.environ.get("QVR_MP_LOW_IMPACT") == "1":  # (B20: a long soak asked to keep out of the machine's way)
            self.low_impact = True
            self.note("QVR_MP_LOW_IMPACT: idle priority (timings are not representative)")

    def crash_listing(self):
        try:
            return set(os.listdir(self.crash_dir))
        except OSError:
            return set()

    def crash_reports(self):
        """(B26) The games' crash reports written in this run (vr_crash.cpp: <base>/quakevr/crash, a .txt with the
        stack and a .dmp beside it): copied into the run's folder, the exception line printed (`mp_crash:`)."""
        for name in sorted(self.crash_listing() - self.crashes_before):
            path = os.path.join(self.crash_dir, name)
            if not name.endswith(".txt"):
                continue
            head = ""
            try:
                with open(path, encoding="utf-8", errors="replace") as f:
                    lines = f.readlines()
                head = next((l.strip() for l in lines if l.startswith("Exception") or l.startswith("Fatal")), "")
                shutil.copy(path, os.path.join(self.out, "crash_" + name))
            except OSError:
                pass
            dump = path[:-4] + ".dmp"
            print("mp_crash: %s: %s (dump %s)" % (path, head, dump if os.path.exists(dump) else "none"), flush=True)
            self.note("crash report %s: %s" % (path, head))

    def note(self, text):
        self.notes.append(text)
        print("  " + text, flush=True)

    def server(self, dedicated=True, maxplayers=4, flat=False, name="server", extra=()):
        more = list(getattr(self, "server_extra", [])) if dedicated else []  # (mp_test.py --server-set)
        inst = Instance(self, name, self.port, dedicated=maxplayers if dedicated else 0, flat=flat, extra=list(extra) + more)
        self.instances.append(inst)
        return inst

    def client(self, name, flat=False, lag_ms=0.0, jitter_ms=0.0, loss=0.0, seed=1, proxy=False, extra=()):
        """A client; with lag, jitter or loss (or proxy=True: to count its traffic) it talks to the server through a
        proxy of its own. connect_cmd() is the console command that joins the server."""
        inst = Instance(self, name, self.take_port(), flat=flat, extra=extra)
        inst.sound = getattr(self, "client_sound", False)  # (mp_test.py syncaudit: every client's sound on)
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

    def check_net_safety(self):
        """(B24) Every game kept to this machine: each live game's vr_mp_room and vr_mp_rendezvous as it holds them now
        (a config or a command may have set them), and no "NET SAFETY:" refusal in any log (-loopbackonly's)."""
        problems = []
        for inst in self.instances:
            if inst.net_guard_test:
                continue
            if inst.proc and inst.proc.poll() is None:
                try:
                    since = inst.mark()
                    inst.cmd("vr_mp_room; vr_mp_rendezvous")
                    _, room_line = inst.wait(r'^"vr_mp_room" is ', 10, since, "its vr_mp_room")
                    _, rv_line = inst.wait(r'^"vr_mp_rendezvous" is ', 10, since, "its vr_mp_rendezvous")
                    room = room_line.split('" is "', 1)[1].split('"', 1)[0]
                    rendezvous = rv_line.split('" is "', 1)[1].split('"', 1)[0]
                    problem = net_safety_problem(room, rendezvous)
                    if problem:
                        problems.append("%s: %s" % (inst.name, problem))
                except (StepFailed, IndexError) as e:
                    self.note("%s: its room settings not read (%s)" % (inst.name, e))
            inst.read()
            refused = [l for l in inst.lines if NET_SAFETY_LINE in l]
            if refused:
                problems.append("%s: %d refusal(s), the first: %s" % (inst.name, len(refused), refused[0].strip()))
        self.check("every game kept to loopback (vr_mp_room/vr_mp_rendezvous, no NET SAFETY refusals)", not problems,
                   "; ".join(problems))

    def finish(self, failure=None):
        if failure:
            self.check("scenario", False, str(failure))
        self.check_net_safety()
        # B12's byte table: what each proxied client read, by kind (vr_net_bytes), before the games stop.
        kinds = {}
        for inst in self.instances:
            if getattr(inst, "proxy", None) is None or not inst.proc or inst.proc.poll() is not None:
                continue
            try:
                since = inst.mark()
                inst.cmd("vr_net_bytes")
                _, line = inst.wait(r"mp_netbytes: ", 10, since)
                kinds = fields(line)
                self.metric("netbytes_%s" % inst.name, kinds)
            except StepFailed:
                pass
        # Clients first, the server last (its exit code: a clean quit, MP_DESIGN.md's old crash on quit).
        for inst in sorted(self.instances, key=lambda i: 1 if i.name == "server" else 0):
            code = inst.stop()
            crashed = any("ENGINE CRASH" in l for l in inst.lines)
            self.check("%s quits cleanly" % inst.name, code == 0 and not crashed,
                       "exit %s%s" % (code, ", ENGINE CRASH in its log" if crashed else ""))
        self.crash_reports()
        for p in self.proxies:
            p.stop()
            self.note(p.summary())
            if p.first is not None and p.last is not None and p.last > p.first:
                w = p.window((p.first, {d: [0, 0, 0, 0, 0] for d in ("up", "down")}), (p.last, p.stats))
                self.metric("link_%d" % p.listen_port, w)
                w["kinds"] = kind_rates(kinds, w["down"]["bytes_s"])
                LINKS.append((self.test, w))
                print("mp_bytes: %s up %d B/s (mean %d B) down %d B/s (mean %d B)" % (
                    self.test, w["up"]["bytes_s"], w["up"]["mean_B"], w["down"]["bytes_s"], w["down"]["mean_B"]),
                    flush=True)
                if w["kinds"]:
                    print("mp_bytes: %s down by kind, B/s: %s" % (self.test, format_kinds(w["kinds"])), flush=True)
        # (mpopen) nothing of this scenario left holding its ports (the next scenario's): its games all gone
        alive = [i.name for i in self.instances if i.proc and i.proc.poll() is None]
        held = busy_ports(self.port, self.next_port - self.port + 1)
        if alive or held:
            self.note("left over after the scenario: games %s, ports %s held" % (alive or "-", held or "-"))
            print("  note: %s: left over: games %s, ports %s held" % (self.test, alive or "-", held or "-"), flush=True)
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

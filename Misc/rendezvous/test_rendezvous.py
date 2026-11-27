#!/usr/bin/env python3
"""test_rendezvous.py -- the room service's protocol and abuse limits (qvr_rendezvous.py), then one real exchange over
UDP with the service run locally (127.0.0.1). Ends with "rendezvous_test: N passed, M failed" (exit 0: all passed).

    python3 test_rendezvous.py
"""

import os
import random
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvr_rendezvous as rv  # noqa: E402

RESULTS = []


def check(name, ok, detail=""):
    RESULTS.append(bool(ok))
    print("  %s %s%s" % ("PASS" if ok else "FAIL", name, (": " + str(detail)) if detail else ""), flush=True)


def req(kind, code=b"", nonce=b"\1\2\3\4", version=rv.VERSION, size=rv.REQUEST, magic=rv.MAGIC, cookie=b""):
    out = bytearray(max(size, 26))
    out[0:4] = magic[:4]
    if size > 5:
        out[4] = version
        out[5] = kind
    out[8:8 + len(code)] = code
    out[14:18] = nonce
    out[18:18 + len(cookie)] = cookie
    return bytes(out[:size])


def register(svc, src, t, code=b"", nonce=b"\1\2\3\4"):
    """A host's registration with the cookie round (version 2): REGISTER, COOKIE back, REGISTER with it -> the reply."""
    r = svc.handle(req(rv.REGISTER, code, nonce), src, t)
    if r and r[0][0][5] == rv.COOKIE:
        r = svc.handle(req(rv.REGISTER, code, nonce, cookie=r[0][0][24:32]), src, t)
    return r


def addr_of(pkt):
    return socket.inet_ntoa(pkt[18:22]), int.from_bytes(pkt[22:24], "big")


def unit():
    svc = rv.Service(rv.Limits(max_rooms=10, max_rooms_per_ip=3, max_sources=1000, joins_per_room=3), random.Random(7))
    host, joiner = ("203.0.113.5", 40001), ("198.51.100.9", 51234)
    t = 1000.0
    out = []
    check("a datagram of another size gets no answer", svc.handle(req(rv.REGISTER, size=63), host, t) == [] and
          svc.handle(req(rv.REGISTER, size=65), host, t) == [])
    check("a datagram without the magic gets no answer", svc.handle(req(rv.REGISTER, magic=b"QUAK"), host, t) == [])
    r = register(svc, host, t)
    out += r
    code = r[0][0][8:14] if r else b""
    check("REGISTER: REGISTERED back to the host, a code of 6 letters, his outside address",
          len(r) == 1 and r[0][1] == host and r[0][0][5] == rv.REGISTERED and len(code) == 6 and
          all(chr(c) in rv.LETTERS for c in code) and addr_of(r[0][0]) == host and r[0][0][14:18] == b"\1\2\3\4",
          code.decode("ascii", "replace"))
    r = register(svc, host, t + 20, code, b"\x05\x06\x07\x08")
    out += r
    check("the host registering again keeps his code (and his nonce comes back)",
          r and r[0][0][8:14] == code and r[0][0][14:18] == b"\x05\x06\x07\x08")
    r = svc.handle(req(rv.JOIN, code, b"\x09\x09\x09\x09"), joiner, t + 21)
    out += r
    kinds = {p[5]: (p, d) for p, d in r}
    check("JOIN: PEER to the joiner with the host's address, INCOMING to the host with the joiner's",
          rv.PEER in kinds and kinds[rv.PEER][1] == joiner and addr_of(kinds[rv.PEER][0]) == host and
          kinds[rv.PEER][0][14:18] == b"\x09\x09\x09\x09" and rv.INCOMING in kinds and kinds[rv.INCOMING][1] == host and
          addr_of(kinds[rv.INCOMING][0]) == joiner)
    r = svc.handle(req(rv.JOIN, code.lower()), joiner, t + 21)
    check("JOIN of an unknown code: NOROOM", len(r) == 1 and r[0][0][5] == rv.NOROOM and r[0][1] == joiner)
    r = svc.handle(req(rv.JOIN, code, version=rv.VERSION + 1), joiner, t + 21)
    out += r
    check("another version: BADVERSION with the service's, the request's nonce echoed (the game checks it)",
          len(r) == 1 and r[0][0][5] == rv.BADVERSION and r[0][0][18] == rv.VERSION and r[0][0][14:18] == b"\1\2\3\4")
    check("no amplification: every reply is %d bytes, a request %d" % (rv.REPLY, rv.REQUEST),
          out and all(len(p) == rv.REPLY for p, _ in out) and rv.REPLY < rv.REQUEST)
    # Introductions to one room: joins_per_room (3) in a window, then none until the window is over
    room_svc = rv.Service(rv.Limits(joins_per_room=3, join_window=10.0, burst=1000, rate=1000), random.Random(1))
    c = register(room_svc, host, t)[0][0][8:14]
    n = sum(1 for i in range(10) if room_svc.handle(req(rv.JOIN, c), ("198.51.100.%d" % i, 5000), t + 1))
    later = room_svc.handle(req(rv.JOIN, c), ("198.51.100.99", 5000), t + 12)
    check("introductions to one room limited (3 in 10 s), then again after the window", n == 3 and len(later) == 2, n)
    # A source's rate: a burst of 20, then 5 a second
    rate_svc = rv.Service(rv.Limits(rate=5.0, burst=20.0), random.Random(1))
    answered = sum(1 for _ in range(100) if rate_svc.handle(req(rv.JOIN, b"AAAAAA"), joiner, t))
    after = sum(1 for _ in range(100) if rate_svc.handle(req(rv.JOIN, b"AAAAAA"), joiner, t + 1.0))
    check("a source's requests rate limited (a burst of 20, then 5 a second)", answered == 20 and after == 5,
          "%d then %d" % (answered, after))
    # Bounded memory: the tracked sources and the rooms
    for i in range(5000):
        svc.handle(req(rv.JOIN, b"BBBBBB"), ("10.%d.%d.%d" % (i >> 16, (i >> 8) & 255, i & 255), 1), t + 30)
    check("the tracked sources bounded (5000 sources, at most 1000 kept)", len(svc.buckets) <= 1000, len(svc.buckets))
    granted = refused = 0
    for i in range(20):
        r = register(svc, ("192.0.2.%d" % i, 26000), t + 31)
        granted += r[0][0][5] == rv.REGISTERED
        refused += r[0][0][5] == rv.REFUSED
    check("the rooms bounded (10): the rest REFUSED", len(svc.rooms) == 10 and refused == 11 and granted == 9,
          "%d rooms, %d granted, %d refused" % (len(svc.rooms), granted, refused))
    per = sum(1 for p in range(6) if register(svc, ("192.0.2.200", 27000 + p), t + 300)[0][0][5] == rv.REGISTERED)
    check("one address's rooms bounded (3)", per == 3, per)
    # (review A6) REGISTERs at a cap scan the room table at most once a second
    scans = [0]
    real_expire = svc.expire
    def counted(now):
        scans[0] += 1
        real_expire(now)
    svc.expire = counted
    for p in range(50):
        register(svc, ("192.0.2.200", 28000 + p), t + 301 + p * 0.01)
    svc.expire = real_expire
    check("REGISTERs at a cap: the room table scanned at most once a second (50 in 0.5 s: 1 scan)", scans[0] == 1,
          scans[0])
    # Expiry: 120 s after the last registration; then its code is free again for its host
    late = t + 300 + 121
    svc.expire(late)
    check("rooms forgotten 120 s after their last registration", len(svc.rooms) == 0 and not svc.by_host and
          not svc.per_ip, "%d rooms" % len(svc.rooms))
    r = svc.handle(req(rv.JOIN, code), joiner, late)
    check("an expired room: NOROOM", r and r[0][0][5] == rv.NOROOM)
    r = register(svc, ("203.0.113.77", 1234), late, code)
    check("a host's old code given back when free (a service restart, a new outside port)", r and r[0][0][8:14] == code)
    r = register(svc, ("203.0.113.78", 1234), late, code)
    check("a code in use is not given to another host", r and r[0][0][5] == rv.REGISTERED and r[0][0][8:14] != code)
    cookies()
    moves()


def cookies():
    """(review 2026-11-18, finding 11) The REGISTER cookie round: no room without the cookie of the source address."""
    svc = rv.Service(rv.Limits(max_rooms=50, burst=1000, rate=1000), random.Random(3), secret=b"k" * 32)
    host = ("203.0.113.5", 40001)
    t = 5000.0
    r = svc.handle(req(rv.REGISTER, nonce=b"\7\7\7\7"), host, t)
    check("a REGISTER without a cookie: COOKIE back (his nonce, 8 bytes at 24), no room made",
          len(r) == 1 and r[0][1] == host and r[0][0][5] == rv.COOKIE and r[0][0][14:18] == b"\7\7\7\7" and
          any(r[0][0][24:32]) and len(r[0][0]) == rv.REPLY and not svc.rooms, r[0][0][24:32].hex() if r else r)
    cookie = r[0][0][24:32]
    r = svc.handle(req(rv.REGISTER, nonce=b"\7\7\7\x08", cookie=cookie), host, t + 1)
    check("the cookie echoed: REGISTERED, a room", r and r[0][0][5] == rv.REGISTERED and len(svc.rooms) == 1)
    r = svc.handle(req(rv.REGISTER, cookie=cookie), ("203.0.113.5", 40002), t + 1)
    check("another port's REGISTER with that cookie: COOKIE (a cookie is its address's)", r and r[0][0][5] == rv.COOKIE)
    r = svc.handle(req(rv.REGISTER, cookie=cookie), ("198.51.100.1", 40001), t + 1)
    check("another address's REGISTER with that cookie: COOKIE", r and r[0][0][5] == rv.COOKIE and len(svc.rooms) == 1)
    r = svc.handle(req(rv.REGISTER, cookie=b"\0" * 7 + b"\1"), host, t + 1)
    check("a wrong cookie: COOKIE", r and r[0][0][5] == rv.COOKIE)
    r = svc.handle(req(rv.REGISTER, cookie=cookie), host, t + svc.limits.cookie_period * 1.5)
    check("the cookie still good in the next period (a renewal 20 s later never needs a new one)",
          r and r[0][0][5] == rv.REGISTERED)
    r = svc.handle(req(rv.REGISTER, cookie=cookie), host, t + svc.limits.cookie_period * 3)
    check("two periods on: COOKIE again (a new one)", r and r[0][0][5] == rv.COOKIE and r[0][0][24:32] != cookie)
    # Spoofed registrations from 20000 addresses (version 1 filled the table with them): none makes a room
    flood = rv.Service(rv.Limits(max_rooms=100, max_sources=50000), random.Random(4), secret=b"s" * 32)
    for i in range(20000):
        flood.handle(req(rv.REGISTER), ("10.%d.%d.%d" % (i >> 16, (i >> 8) & 255, i & 255), 26000), t)
    r = register(flood, host, t + 1)
    check("20000 spoofed REGISTERs (no cookie: they can't read it) make no room; a real host still gets his code",
          r and r[0][0][5] == rv.REGISTERED and len(flood.rooms) == 1, "%d rooms" % len(flood.rooms))
    # The cookie a guess can't make: of 2^16 random ones none passes
    hits = sum(1 for _ in range(1 << 16) if svc.cookie_ok(random.getrandbits(64).to_bytes(8, "big"), host, t))
    check("random cookies don't pass (2^16 tried)", hits == 0, hits)


def moves():
    """(finding 11) A host whose router gave him a new port registers his code again from it: the room moves."""
    svc = rv.Service(rv.Limits(burst=1000, rate=1000), random.Random(5), secret=b"m" * 32)
    t = 9000.0
    old, new, other = ("203.0.113.9", 40001), ("203.0.113.9", 52777), ("198.51.100.4", 40001)
    code = register(svc, old, t)[0][0][8:14]
    r = register(svc, new, t + 30, code)
    check("his code again from a new port of the same IP address: REGISTERED with it, the room moved there",
          r and r[0][0][5] == rv.REGISTERED and r[0][0][8:14] == code and svc.rooms[code].host == new and
          svc.by_host.get(new) == code and old not in svc.by_host and len(svc.rooms) == 1,
          "%s; host %s" % (r[0][0][8:14] if r else r, svc.rooms[code].host if code in svc.rooms else None))
    r = svc.handle(req(rv.JOIN, code), ("192.0.2.1", 5000), t + 31)
    peer = [p for p, _ in r if p[5] == rv.PEER]
    check("a joiner is then sent the new port", peer and addr_of(peer[0]) == new)
    r = register(svc, other, t + 32, code)
    check("the code asked for from another IP address: a new code, the room stays",
          r and r[0][0][5] == rv.REGISTERED and r[0][0][8:14] != code and svc.rooms[code].host == new)


def probes():
    """(version 3) The port check: a PROBECHECK from the room's address gets one PROBE to his IP address at his game
    port; its abuse limits; a version 2 game served as version 2."""
    svc = rv.Service(rv.Limits(burst=1000, rate=1000), random.Random(9), secret=b"p" * 32)
    t = 5000.0
    host, other = ("203.0.113.7", 40001), ("198.51.100.8", 40001)
    code = register(svc, host, t)[0][0][8:14]

    def probecheck(src, game_port, at, c=None, version=rv.VERSION):
        pkt = bytearray(req(rv.PROBECHECK, code if c is None else c, b"\5\6\7\x08", version=version))
        pkt[22:24] = game_port.to_bytes(2, "big")
        return svc.handle(bytes(pkt), src, at)

    r = probecheck(host, 26000, t + 1)
    probe = [(p, d) for p, d in r if p[5] == rv.PROBE]
    probing = [(p, d) for p, d in r if p[5] == rv.PROBING]
    check("PROBECHECK: one PROBE to his IP address at his game port (his code, his nonce, where it went), PROBING (1) "
          "back to him", len(r) == 2 and len(probe) == 1 and probe[0][1] == (host[0], 26000) and
          probe[0][0][8:14] == code and probe[0][0][14:18] == b"\5\6\7\x08" and addr_of(probe[0][0]) == (host[0], 26000)
          and len(probing) == 1 and probing[0][1] == host and probing[0][0][18] == 1, r)
    check("no amplification: the replies to a PROBECHECK are %d bytes in all, the request %d" % (
        sum(len(p) for p, _ in r), rv.REQUEST), sum(len(p) for p, _ in r) <= rv.REQUEST)
    r = probecheck(host, 26000, t + 2)
    check("one probe per registration: a second PROBECHECK gets PROBING (0) and no PROBE",
          len(r) == 1 and r[0][0][5] == rv.PROBING and r[0][0][18] == 0, r)
    register(svc, host, t + 20)
    r = probecheck(host, 0, t + 21)
    check("his renewal gives him another; game port 0: to the port he sends from",
          [d for p, d in r if p[5] == rv.PROBE] == [host], r)
    r = probecheck(other, 26000, t + 22)
    check("a PROBECHECK from an address holding no room: NOROOM, no PROBE (a probe goes only to a registered address)",
          len(r) == 1 and r[0][0][5] == rv.NOROOM and r[0][1] == other, r)
    r = probecheck(other, 26000, t + 22, c=code)
    check("... naming another host's code changes nothing", len(r) == 1 and r[0][0][5] == rv.NOROOM, r)
    r = probecheck((host[0], 40002), 26000, t + 22)
    check("... nor from his IP address at another port", len(r) == 1 and r[0][0][5] == rv.NOROOM, r)
    register(svc, host, t + 40)
    r = probecheck(host, 26000, t + 41, version=2)
    check("a PROBECHECK in a version 2 request is dropped", r == [], r)
    r = svc.handle(req(rv.JOIN, code, version=2), other, t + 42)
    check("a version 2 game is served as version 2 (a 1.1 game joins by code)",
          r and all(p[4] == 2 for p, _ in r) and r[0][0][5] == rv.PEER, r)
    old = rv.Service(rv.Limits(), random.Random(3), secret=b"o" * 32, newest=2)
    r = old.handle(req(rv.PROBECHECK, code), host, t)
    check("a service run as version 2 (--newest 2) answers a version 3 request BADVERSION (2), the nonce echoed",
          len(r) == 1 and r[0][0][5] == rv.BADVERSION and r[0][0][18] == 2 and r[0][0][14:18] == b"\1\2\3\4", r)


def udp():
    """The service as a program, on 127.0.0.1: a host and a joiner over real sockets."""
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
    probe.close()
    proc = subprocess.Popen([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "qvr_rendezvous.py"),
                             "--bind", "127.0.0.1", "--port", str(port), "--report", "0"],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        line = proc.stdout.readline()
        check("the service starts locally", "qvr_rendezvous: version %d on 127.0.0.1:%d" % (rv.VERSION, port) in line,
              line.strip())
        h = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        j = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        h.bind(("127.0.0.1", 0))
        j.bind(("127.0.0.1", 0))
        h.settimeout(5)
        j.settimeout(5)
        h.sendto(req(rv.REGISTER), ("127.0.0.1", port))
        pkt, _ = h.recvfrom(100)
        check("over UDP: a first REGISTER gets a COOKIE", pkt[5] == rv.COOKIE)
        h.sendto(req(rv.REGISTER, cookie=pkt[24:32]), ("127.0.0.1", port))
        pkt, _ = h.recvfrom(100)
        code = pkt[8:14]
        check("over UDP: REGISTERED with the host's address", pkt[5] == rv.REGISTERED and addr_of(pkt) == h.getsockname())
        j.sendto(req(rv.JOIN, code), ("127.0.0.1", port))
        peer, _ = j.recvfrom(100)
        incoming, _ = h.recvfrom(100)
        check("over UDP: the joiner gets the host's address, the host the joiner's",
              peer[5] == rv.PEER and addr_of(peer) == h.getsockname() and incoming[5] == rv.INCOMING and
              addr_of(incoming) == j.getsockname())
        pc = bytearray(req(rv.PROBECHECK, code))
        pc[22:24] = j.getsockname()[1].to_bytes(2, "big")  # (the "game port": j's, to see the probe arrive)
        h.sendto(bytes(pc), ("127.0.0.1", port))
        probing, _ = h.recvfrom(100)
        probe, frm = j.recvfrom(100)
        check("over UDP: a PROBECHECK's PROBE arrives at the game port from another port of the service; PROBING back",
              probing[5] == rv.PROBING and probing[18] == 1 and probe[5] == rv.PROBE and frm[0] == "127.0.0.1" and
              frm[1] != port, "probe from %s:%d" % frm)
        h.close()
        j.close()
    except (socket.timeout, OSError) as e:
        check("over UDP", False, repr(e))
    finally:
        proc.terminate()
        proc.wait(10)


if __name__ == "__main__":
    unit()
    probes()
    udp()
    passed = sum(RESULTS)
    print("rendezvous_test: %d passed, %d failed" % (passed, len(RESULTS) - passed), flush=True)
    sys.exit(0 if passed == len(RESULTS) else 1)

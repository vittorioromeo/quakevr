#!/usr/bin/env python3
"""qvr_rendezvous.py -- Quake VR's room service ("meeting point") for joining a game by room code.

A host registers his game from the socket he plays on; the service sees his router's outside address and port for
it and gives him a room code (6 letters). A player joining by that code asks the service from the socket he will play
on; the service tells each of them the other's outside address, and both send to each other at once (UDP hole
punching): the game itself then goes directly between them. The service never carries game traffic, logs no
addresses and forgets a room ROOM_TTL seconds after its host's last registration.

Python 3 standard library only. One UDP socket, one thread.

    python3 qvr_rendezvous.py [--bind 0.0.0.0] [--port 26100] [--room-ttl 120] [--max-rooms 20000] ...
    python3 qvr_rendezvous.py --local      (a local test: 127.0.0.1:26100, verbose)

The wire format (the game's side: Quake/net_dgrm.c, "room codes"). Every datagram: "QVRR", a version byte (2), a
type byte, two zero bytes, then the type's fields; requests are exactly 64 bytes, replies 32 (a reply never outgrows
the request it answers: no amplification). Codes are 6 ASCII letters at offset 8, nonces 4 bytes at 14, addresses an
IPv4 address at 18 and a port at 22 (network order), cookies 8 bytes (at 18 in a request, 24 in a reply).

    0x01 REGISTER    host -> service: the code he had (or zeros), a nonce, the cookie he was given (or zeros)
    0x02 JOIN        joiner -> service: the code, a nonce
    0x81 REGISTERED  -> host: his code, his nonce, his outside address
    0x82 PEER        -> joiner: the code, his nonce, the host's outside address
    0x83 INCOMING    -> host: the code, 0, the joiner's outside address (the host punches towards it)
    0x84 NOROOM      -> joiner: the code, his nonce: no such room
    0x85 REFUSED     -> host: the code, his nonce, why at 18 (1: no room for more rooms)
    0x86 BADVERSION  -> either: the code, the nonce, the service's version at 18 (a request of another version)
    0x87 COOKIE      -> host: the code, his nonce, his address's cookie at 24: his REGISTER again with it

Version 2 (review 2026-11-18, finding 11): a REGISTER makes or renews a room only with the cookie of the address it
comes from (an HMAC of the address and the time under a secret made at start, good for one to two --cookie-period),
else the answer is COOKIE: a registration spoofed from an address that can't read the reply makes nothing (version 1's
rate limits and room caps were keyed by the claimed source alone: 20000 spoofed REGISTERs filled the table). A host
whose router gave him a new port registers his code again from it: the room moves to the new port when the code's
room is held from the same IP address. BADVERSION echoes the nonce (the game checks it on every reply it acts on).

Abuse limits: a request of another size or without the magic gets no answer; each source address has a token bucket
(--rate a second, --burst at once); the tracked sources, the rooms (and the rooms of one source address) are bounded;
introductions to one room are rate limited (--joins-per-room in --join-window seconds). A relay for the hosts no
punch reaches (symmetric NATs) is a design note in README.md, not built.
"""

import argparse
import collections
import hashlib
import hmac
import os
import select
import socket
import sys
import time

MAGIC = b"QVRR"
VERSION = 2
REQUEST = 64
REPLY = 32
CODE_LEN = 6
COOKIE_LEN = 8
LETTERS = "ABCDEFGHJKLMNPQRSTUVWXYZ"  # (no I or O: read as 1 and 0)

REGISTER, JOIN = 0x01, 0x02
REGISTERED, PEER, INCOMING, NOROOM, REFUSED, BADVERSION, COOKIE = 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87
REFUSED_FULL = 1

DEFAULT_PORT = 26100


class Limits:
    def __init__(self, room_ttl=120.0, max_rooms=20000, max_rooms_per_ip=8, max_sources=50000, rate=5.0, burst=20.0,
                 joins_per_room=20, join_window=10.0, cookie_period=60.0):
        self.room_ttl = room_ttl
        self.max_rooms = max_rooms
        self.max_rooms_per_ip = max_rooms_per_ip
        self.max_sources = max_sources
        self.rate = rate
        self.burst = burst
        self.joins_per_room = joins_per_room
        self.join_window = join_window
        self.cookie_period = cookie_period  # s: a cookie is good in its period and the next


class Room:
    __slots__ = ("code", "host", "expires", "joins", "window_start")

    def __init__(self, code, host, expires, now):
        self.code = code
        self.host = host          # (ip, port): his outside address
        self.expires = expires
        self.joins = 0
        self.window_start = now


def reply(kind, code=b"\0" * CODE_LEN, nonce=b"\0" * 4, addr=None, extra=None, cookie=None):
    out = bytearray(REPLY)
    out[0:4] = MAGIC
    out[4] = VERSION
    out[5] = kind
    out[8:14] = code
    out[14:18] = nonce
    if addr is not None:
        out[18:22] = socket.inet_aton(addr[0])
        out[22:24] = addr[1].to_bytes(2, "big")
    if extra is not None:
        out[18] = extra
    if cookie is not None:
        out[24:24 + COOKIE_LEN] = cookie
    return bytes(out)


class Service:
    """The room table and the protocol, without the socket: handle() -> the datagrams to send (tests call it
    directly with their own clock)."""

    def __init__(self, limits=None, rng=None, secret=None):
        self.limits = limits or Limits()
        self.rng = rng or __import__("random").SystemRandom()
        self.secret = secret if secret is not None else os.urandom(32)  # (the cookies' key: a restart makes a new one)
        self.rooms = {}                                # code -> Room
        self.by_host = {}                              # (ip, port) -> code
        self.per_ip = collections.Counter()            # ip -> rooms
        self.buckets = collections.OrderedDict()       # ip -> [tokens, last time] (least recently seen first)
        self.stats = collections.Counter()

    # -- limits
    def allow(self, ip, now):
        b = self.buckets.get(ip)
        if b is None:
            if len(self.buckets) >= self.limits.max_sources:
                self.buckets.popitem(last=False)   # (the least recently seen source forgotten: bounded memory)
            b = self.buckets[ip] = [self.limits.burst, now]
        else:
            self.buckets.move_to_end(ip)
            b[0] = min(self.limits.burst, b[0] + (now - b[1]) * self.limits.rate)
            b[1] = now
        if b[0] < 1.0:
            self.stats["rate_limited"] += 1
            return False
        b[0] -= 1.0
        return True

    def expire(self, now):
        for code in [c for c, r in self.rooms.items() if r.expires <= now]:
            self.drop(code)

    def drop(self, code):
        r = self.rooms.pop(code)
        self.by_host.pop(r.host, None)
        self.per_ip[r.host[0]] -= 1
        if self.per_ip[r.host[0]] <= 0:
            del self.per_ip[r.host[0]]

    def cookie(self, src, now, back=0):
        """The cookie of address `src` (ip, port) in the period of `now` (`back` periods before)."""
        period = int(now // self.limits.cookie_period) - back
        msg = ("%s:%d:%d" % (src[0], src[1], period)).encode("ascii")
        return hmac.new(self.secret, msg, hashlib.sha256).digest()[:COOKIE_LEN]

    def cookie_ok(self, given, src, now):
        return any(hmac.compare_digest(given, self.cookie(src, now, back)) for back in (0, 1))

    def new_code(self):
        for _ in range(100):
            code = "".join(self.rng.choice(LETTERS) for _ in range(CODE_LEN)).encode("ascii")
            if code not in self.rooms:
                return code
        return None

    # -- the protocol
    def handle(self, data, src, now):
        """A datagram `data` from `src` (ip, port) at `now` -> [(datagram, destination)]."""
        if len(data) != REQUEST or data[0:4] != MAGIC:
            self.stats["dropped"] += 1
            return []
        if not self.allow(src[0], now):
            return []
        kind, code, nonce = data[5], bytes(data[8:14]), bytes(data[14:18])
        if data[4] != VERSION:
            self.stats["bad_version"] += 1
            return [(reply(BADVERSION, code, nonce, extra=VERSION), src)]
        if kind == REGISTER:
            if not self.cookie_ok(bytes(data[18:18 + COOKIE_LEN]), src, now):
                self.stats["cookie_sent"] += 1
                return [(reply(COOKIE, code, nonce, cookie=self.cookie(src, now)), src)]
            return self.register(code, nonce, src, now)
        if kind == JOIN:
            return self.join(code, nonce, src, now)
        self.stats["dropped"] += 1
        return []

    def register(self, wanted, nonce, src, now):
        code = self.by_host.get(src)
        if code is not None and code in self.rooms:
            self.rooms[code].expires = now + self.limits.room_ttl
            self.stats["renewed"] += 1
            return [(reply(REGISTERED, code, nonce, src), src)]
        # His code's room held from his IP address on another port: his router gave him a new one (or the game was
        # restarted): the room moves to it, his code kept
        moving = self.rooms.get(wanted)
        if moving is not None and moving.host[0] == src[0] and moving.host != src and moving.expires > now:
            self.by_host.pop(moving.host, None)
            moving.host = src
            moving.expires = now + self.limits.room_ttl
            self.by_host[src] = wanted
            self.stats["moved"] += 1
            return [(reply(REGISTERED, wanted, nonce, src), src)]
        if len(self.rooms) >= self.limits.max_rooms or self.per_ip[src[0]] >= self.limits.max_rooms_per_ip:
            self.expire(now)
            if len(self.rooms) >= self.limits.max_rooms or self.per_ip[src[0]] >= self.limits.max_rooms_per_ip:
                self.stats["refused"] += 1
                return [(reply(REFUSED, wanted, nonce, extra=REFUSED_FULL), src)]
        # His old code again if it's free (the service restarted, his router's port changed), else a new one
        if not (len(wanted) == CODE_LEN and all(chr(c) in LETTERS for c in wanted) and wanted not in self.rooms):
            wanted = self.new_code()
            if wanted is None:
                self.stats["refused"] += 1
                return [(reply(REFUSED, b"\0" * CODE_LEN, nonce, extra=REFUSED_FULL), src)]
        self.rooms[wanted] = Room(wanted, src, now + self.limits.room_ttl, now)
        self.by_host[src] = wanted
        self.per_ip[src[0]] += 1
        self.stats["registered"] += 1
        return [(reply(REGISTERED, wanted, nonce, src), src)]

    def join(self, code, nonce, src, now):
        r = self.rooms.get(code)
        if r is None or r.expires <= now:
            if r is not None:
                self.drop(code)
            self.stats["no_room"] += 1
            return [(reply(NOROOM, code, nonce), src)]
        if now - r.window_start >= self.limits.join_window:
            r.window_start, r.joins = now, 0
        if r.joins >= self.limits.joins_per_room:
            self.stats["join_limited"] += 1
            return []
        r.joins += 1
        self.stats["introduced"] += 1
        return [(reply(PEER, code, nonce, r.host), src), (reply(INCOMING, code, b"\0" * 4, src), r.host)]


def serve(args):
    limits = Limits(args.room_ttl, args.max_rooms, args.max_rooms_per_ip, args.max_sources, args.rate, args.burst,
                    args.joins_per_room, args.join_window, args.cookie_period)
    svc = Service(limits)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.bind, args.port))
    sock.setblocking(False)
    print("qvr_rendezvous: version %d on %s:%d (rooms last %g s; at most %d)" % (
        VERSION, args.bind, sock.getsockname()[1], limits.room_ttl, limits.max_rooms), flush=True)
    last_expire = last_report = time.monotonic()
    while True:
        ready, _, _ = select.select([sock], [], [], 1.0)
        now = time.monotonic()
        if ready:
            for _ in range(256):  # (a burst read, then the expiry and the report get their turn)
                try:
                    data, src = sock.recvfrom(2048)
                except (BlockingIOError, InterruptedError):
                    break
                except OSError:
                    continue  # (Windows, local tests: a port unreachable of an earlier reply)
                for out, dst in svc.handle(data, src, now):
                    try:
                        sock.sendto(out, dst)
                    except OSError:
                        pass
                    if args.verbose:
                        print("  %02x -> %s:%d" % (out[5], dst[0], dst[1]), flush=True)
        if now - last_expire >= 1.0:
            svc.expire(now)
            last_expire = now
        if args.report and now - last_report >= args.report:
            print("qvr_rendezvous: %d rooms; since the start: %s" % (
                len(svc.rooms), ", ".join("%s %d" % kv for kv in sorted(svc.stats.items())) or "nothing"), flush=True)
            last_report = now


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bind", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    ap.add_argument("--room-ttl", type=float, default=120.0, help="s a room lasts after its host's last registration")
    ap.add_argument("--max-rooms", type=int, default=20000)
    ap.add_argument("--max-rooms-per-ip", type=int, default=8)
    ap.add_argument("--max-sources", type=int, default=50000, help="source addresses tracked by the rate limit")
    ap.add_argument("--rate", type=float, default=5.0, help="requests a second per source address")
    ap.add_argument("--burst", type=float, default=20.0, help="requests at once per source address")
    ap.add_argument("--joins-per-room", type=int, default=20)
    ap.add_argument("--join-window", type=float, default=10.0)
    ap.add_argument("--cookie-period", type=float, default=60.0, help="s a REGISTER cookie's period (good for two)")
    ap.add_argument("--report", type=float, default=3600.0, help="s between the log's counts (0: none)")
    ap.add_argument("--verbose", action="store_true", help="every reply's type and destination (local tests only)")
    ap.add_argument("--local", action="store_true", help="a local test: 127.0.0.1, verbose, a report a minute")
    args = ap.parse_args()
    if args.local:
        args.bind, args.verbose, args.report = "127.0.0.1", True, 60.0
    if os.environ.get("QVR_RENDEZVOUS_PORT"):
        args.port = int(os.environ["QVR_RENDEZVOUS_PORT"])
    try:
        serve(args)
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())

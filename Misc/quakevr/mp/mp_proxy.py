"""mp_proxy.py -- a UDP proxy between one Quake client and a server, adding latency, jitter and loss.

NetQuake's handshake: the client sends CCREQ_CONNECT to the server's control port; the server answers CCREP_ACCEPT
with the port of a new socket for that client, and both talk on it from then on (net_dgrm.c). The proxy listens on
one port for the client and talks to the server from one socket of its own (the server checks the sender's address),
sends control packets to the server's control port and game packets to the port the accept named, and rewrites that
port in the accept to its own, so the client keeps talking to the proxy.

    lag_ms     one-way delay added in each direction (a round trip gains 2 x lag_ms)
    jitter_ms  each packet's delay varies by up to this much either way (packets may arrive out of order, as UDP may)
    loss       the chance a game packet is dropped, each direction (control packets are never dropped: a lost
               CCREQ_CONNECT only slows the connect down, and the tests would measure the retry, not the game)

Standard library only. As a script: python mp_proxy.py <listen_port> <server_port> [lag_ms] [jitter_ms] [loss] [seed]
"""

import heapq
import random
import select
import socket
import sys
import threading
import time

NETFLAG_CTL = 0x80000000
NETFLAG_DATA = 0x00010000  # a reliable message's chunk (net_dgrm.c)
NETFLAG_ACK = 0x00020000   # its ack
NETFLAG_UNRELIABLE = 0x00100000  # a datagram (the snapshot's)
CCREP_ACCEPT = 0x81
HOST = "127.0.0.1"
# (B26) The fixed argument bytes of server commands (protocol.h svc_*), for the fuzz's "a command after": the whole
# command, so the client's checks on its values are what meets it (not the message's end)
SVC_ARG_BYTES = {1: 0, 3: 5, 5: 2, 7: 4, 14: 3, 16: 2, 17: 2, 24: 1, 25: 1, 27: 0, 28: 0, 30: 0, 32: 2, 33: 0, 40: 0}


def control_or_short(pkt):
    return len(pkt) < 8 or is_control(pkt)


def is_control(pkt):
    return len(pkt) >= 4 and (int.from_bytes(pkt[0:4], "big") & 0xFFFF0000) == NETFLAG_CTL


class Proxy:
    def __init__(self, listen_port, server_port, lag_ms=0.0, jitter_ms=0.0, loss=0.0, seed=1):
        self.listen_port = listen_port
        self.server_ctl = (HOST, server_port)
        self.server_game = None  # (HOST, port) once the accept named it
        self.client = None
        self.lag = lag_ms / 1000.0
        self.jitter = jitter_ms / 1000.0
        self.loss = loss
        self.rng = random.Random(seed)
        self.down = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # the client's side
        self.down.bind((HOST, listen_port))
        self.up = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # the server's side
        self.up.bind((HOST, 0))
        self.queue = []  # (due, seq, socket, address, packet)
        self.seq = 0
        self.stop_flag = False
        self.thread = None
        # Stats, per direction: packets, bytes (UDP payload), dropped; first and last packet time.
        self.stats = {"up": [0, 0, 0, 0, 0], "down": [0, 0, 0, 0, 0]}  # packets, bytes, dropped, largest, datagrams
        # (B16) the server gone silent: every packet down dropped while set (a crashed server, a link cut)
        self.blackout = False
        # (B25) the client gone silent at the server: every packet up dropped while set (a game hitched or stalled)
        self.blackout_up = False
        # (B18) game packets corrupted, each direction: the chance a packet is (mp_test.py fuzz: a hostile or broken
        # peer); how many were, by kind (bits flipped, cut short, random bytes, garbage after). The NetQuake header's
        # flags and sequence are kept, its length rewritten to the packet's: what reaches the message parsers.
        self.corrupt = {"up": 0.0, "down": 0.0}
        self.corrupted = {"up": [0, 0, 0, 0, 0], "down": [0, 0, 0, 0, 0]}
        # (B26) the corruption's own generator (set_fuzz_seed: a run's seed, printed; the link's loss and jitter keep
        # theirs)
        self.fuzz_rng = random.Random(seed)
        # (B26) given commands after the next game packets' messages, each way (mp_test.py fuzz's hostile commands:
        # one each, in order); how many went
        self.inject = {"up": [], "down": []}
        self.injected = {"up": 0, "down": 0}
        self.first = None
        self.last = None
        # (B12b) reliable chunks down to drop (drop_reliable_down): how many more; each dropped one's sequence and time,
        # and the delay until the server sent it again
        self.drop_reliable = 0
        self.drop_least = 0
        self.dropped = {}  # sequence -> time dropped
        self.resent_after = {}  # sequence -> s
        # (B13) a reordering to stage (reorder_reliable_down): the chunk down held back, the next one's sequence and
        # the delays; whether each was done
        self.reorder_least = None
        self.reorder_extra = 0.0
        self.reorder_next = None
        self.reordered = {"held": False, "ack_late": False}
        # (review 2026-11-18, finding 2) each direction's last unreliable datagram's sequence (None: none yet), for a
        # forged one far from it
        self.last_unreliable = {"up": None, "down": None}

    def forge_up(self, pkt, control):
        """(finding 2) A datagram to the server from the client's address as the server sees it (this proxy's socket):
        a forgery in his name. Control ones go to the server's control port, the rest to its game port."""
        self.up.sendto(pkt, self.server_ctl if control or self.server_game is None else self.server_game)

    def forge_down(self, pkt):
        """(finding 2) A datagram to the client from the server's address as the client sees it (this proxy)."""
        if self.client is not None:
            self.down.sendto(pkt, self.client)

    def delay(self):
        return max(0.0, self.lag + (self.rng.uniform(-self.jitter, self.jitter) if self.jitter > 0 else 0.0))

    def drop_reliable_down(self, n=1, least=0):
        """Drop the next `n` reliable chunks down of `least` bytes or more (B12b's resend timings: resent_after)."""
        self.dropped.clear()
        self.resent_after.clear()
        self.drop_least = least
        self.drop_reliable = n

    def reorder_reliable_down(self, least, extra_s):
        """(B13) Stage the reordering that stalled NetQuake's window: the next reliable chunk down of `least` bytes or
        more is held back `extra_s`, so the chunk after it overtakes it; that later chunk's ack up is held back twice
        as long, so it reaches the server after the first chunk's ack (NetQuake's receiver dropped the overtaking
        chunk but acked it: the server then took the message as delivered)."""
        self.reorder_least = least
        self.reorder_extra = extra_s
        self.reorder_next = None
        self.reordered = {"held": False, "ack_late": False}

    def reorder_extra_delay(self, pkt, direction):
        """The extra delay of a packet staged by reorder_reliable_down (0: none)."""
        if control_or_short(pkt):
            return 0.0
        flags, seq = int.from_bytes(pkt[0:4], "big"), int.from_bytes(pkt[4:8], "big")
        if direction == "down" and flags & NETFLAG_DATA and self.reorder_least is not None and len(pkt) >= self.reorder_least:
            self.reorder_least = None
            self.reorder_next = (seq + 1) & 0xFFFFFFFF  # (sequences wrap: each side's first is random, finding 2)
            self.reordered["held"] = True
            return self.reorder_extra
        if direction == "up" and flags & NETFLAG_ACK and self.reorder_next is not None and seq == self.reorder_next:
            self.reorder_next = None
            self.reordered["ack_late"] = True
            return 2 * self.reorder_extra
        return 0.0

    def reliable_down(self, pkt):
        """A chunk of a reliable message down: dropped if armed; its resend after a drop timed. True: drop it."""
        if control_or_short(pkt) or not (int.from_bytes(pkt[0:4], "big") & NETFLAG_DATA):
            return False
        seq = int.from_bytes(pkt[4:8], "big")
        now = time.monotonic()
        if seq in self.dropped and seq not in self.resent_after:
            self.resent_after[seq] = now - self.dropped[seq]
        if self.drop_reliable > 0 and seq not in self.dropped and len(pkt) >= self.drop_least:
            self.drop_reliable -= 1
            self.dropped[seq] = now
            return True
        return False

    @staticmethod
    def append(pkt, extra):
        """A game packet with `extra` after its message, the header's length made the packet's again."""
        size = len(pkt) + len(extra)
        return ((int.from_bytes(pkt[0:4], "big") & 0xFFFF0000) | size).to_bytes(4, "big") + pkt[4:] + bytes(extra)

    def set_fuzz_seed(self, seed):
        """(B26) The corruption's generator from `seed` (a fuzz run's, printed: its choices again)."""
        self.fuzz_rng = random.Random(seed)

    def mangle(self, pkt, direction):
        """(B18) A game packet corrupted at the direction's chance: its payload's bits flipped, cut short, replaced
        with random bytes, or random bytes added after it; the header's length made the packet's again."""
        rng = self.fuzz_rng
        if self.corrupt[direction] <= 0 or len(pkt) < 9 or rng.random() >= self.corrupt[direction]:
            return pkt
        kind = rng.randrange(5)
        body = bytearray(pkt[8:])
        if kind == 0:
            for _ in range(rng.randint(1, 8)):
                i = rng.randrange(len(body))
                body[i] ^= 1 << rng.randrange(8)
        elif kind == 1:
            body = body[:rng.randrange(len(body))]
        elif kind == 2:
            body = bytearray(rng.getrandbits(8) for _ in range(len(body)))
        elif kind == 3:
            body += bytearray(rng.getrandbits(8) for _ in range(rng.randint(1, 64)))
        else:
            # (B26) one whole command after the message, random arguments: a command code the parser takes (svc 0-56,
            # svc_quakevr 39 with a sub-command; clc 0-15) so its own reads meet the bad values, and the message ending
            # right after (B25b's crash: a garbage svc_setview's entity, nothing after it to fail on first)
            if direction == "down" and rng.random() < 0.3:
                body += bytes([39, rng.randrange(64)])
                n = rng.randint(0, 12)
            else:
                cmd = rng.randrange(57 if direction == "down" else 16)
                body += bytes([cmd])
                fixed = SVC_ARG_BYTES.get(cmd) if direction == "down" else None
                n = fixed if fixed is not None and rng.random() < 0.7 else rng.randint(0, 12)
            body += bytearray(rng.getrandbits(8) for _ in range(n))
        self.corrupted[direction][kind] += 1
        head = int.from_bytes(pkt[0:4], "big")
        size = 8 + len(body)
        return ((head & 0xFFFF0000) | size).to_bytes(4, "big") + pkt[4:8] + bytes(body)

    def schedule(self, sock, addr, pkt, direction, control):
        st = self.stats[direction]
        if not control_or_short(pkt):
            pkt = self.mangle(pkt, direction)
            # (B28) only after an unreliable datagram's message: the receiver parses it whole. An ack's bytes after its
            # header are never read, and a reliable chunk's join the next chunk's (or a resend is taken as a duplicate):
            # up, most of B's packets are acks of the server's reliable chunks, so B27's injections went unread
            if self.inject[direction] and int.from_bytes(pkt[0:4], "big") & NETFLAG_UNRELIABLE:
                pkt = self.append(pkt, self.inject[direction].pop(0))
                self.injected[direction] += 1
        if not control_or_short(pkt):
            head = int.from_bytes(pkt[0:4], "big")
            if head & NETFLAG_UNRELIABLE:
                self.last_unreliable[direction] = int.from_bytes(pkt[4:8], "big")
        if (direction == "down" and self.blackout) or (direction == "up" and self.blackout_up):
            st[2] += 1
            return
        if direction == "down" and self.reliable_down(pkt):
            st[2] += 1
            return
        if not control and self.loss > 0 and self.rng.random() < self.loss:
            st[2] += 1
            return
        st[0] += 1
        st[1] += len(pkt)
        st[3] = max(st[3], len(pkt))
        if not control_or_short(pkt) and int.from_bytes(pkt[0:4], "big") & NETFLAG_UNRELIABLE:
            st[4] += 1
        now = time.monotonic()
        self.first = self.first if self.first is not None else now
        self.last = now
        self.seq += 1
        extra = self.reorder_extra_delay(pkt, direction)
        heapq.heappush(self.queue, (now + self.delay() + extra, self.seq, sock, addr, pkt))

    def from_client(self, pkt, addr):
        self.client = addr
        control = is_control(pkt)
        dest = self.server_ctl if control or self.server_game is None else self.server_game
        self.schedule(self.up, dest, pkt, "up", control)

    def from_server(self, pkt, addr):
        control = is_control(pkt)
        if control and len(pkt) >= 9 and pkt[4] == CCREP_ACCEPT:
            self.server_game = (HOST, int.from_bytes(pkt[5:9], "little"))
            pkt = pkt[:5] + self.listen_port.to_bytes(4, "little") + pkt[9:]
        if self.client is not None:
            self.schedule(self.down, self.client, pkt, "down", control)

    def run(self):
        while not self.stop_flag:
            now = time.monotonic()
            while self.queue and self.queue[0][0] <= now:
                _, _, sock, addr, pkt = heapq.heappop(self.queue)
                try:
                    sock.sendto(pkt, addr)
                except OSError:
                    pass
            timeout = 0.05 if not self.queue else max(0.0, min(0.05, self.queue[0][0] - now))
            ready, _, _ = select.select([self.down, self.up], [], [], timeout)
            for s in ready:
                try:
                    pkt, addr = s.recvfrom(65535)
                except OSError:
                    continue  # (Windows: an ICMP port unreachable from a game that quit)
                if s is self.down:
                    self.from_client(pkt, addr)
                else:
                    self.from_server(pkt, addr)

    def start(self):
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()
        return self

    def stop(self):
        self.stop_flag = True
        if self.thread:
            self.thread.join(2)
        self.down.close()
        self.up.close()

    def snapshot(self):
        return time.monotonic(), {d: list(v) for d, v in self.stats.items()}

    def wait_packets(self, direction, count, timeout):
        """Waits until `count` more packets have gone `direction` ("up"/"down") -> a snapshot then; None after
        `timeout` seconds (a safety net: the caller fails its step)."""
        start = self.stats[direction][0]
        deadline = time.monotonic() + timeout
        while self.stats[direction][0] - start < count:
            if time.monotonic() > deadline:
                return None
            time.sleep(0.02)
        return self.snapshot()

    @staticmethod
    def window(a, b):
        """Between two snapshots: per direction, packets a second, bytes a second, the mean packet."""
        span = max(b[0] - a[0], 1e-6)
        out = {}
        for d in ("up", "down"):
            n = b[1][d][0] - a[1][d][0]
            by = b[1][d][1] - a[1][d][1]
            out[d] = {"pkts_s": round(n / span, 1), "bytes_s": round(by / span), "mean_B": by // max(n, 1),
                      "pkts": n, "datagrams": b[1][d][4] - a[1][d][4]}
        return out

    def summary(self):
        span = (self.last - self.first) if self.first is not None and self.last is not None else 0.0
        parts = []
        for d in ("up", "down"):
            n, b, dropped, most = self.stats[d][:4]
            rate = b / span / 1024.0 if span > 0 else 0.0
            parts.append("%s %d pkts %d B (%.1f KB/s, mean %d B, largest %d B) dropped %d" % (
                d, n, b, rate, b // max(n, 1), most, dropped))
        return "proxy :%d lag %d ms jitter %d ms loss %.1f%%: %s" % (
            self.listen_port, self.lag * 1000, self.jitter * 1000, self.loss * 100, "; ".join(parts))


if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) < 2:
        print(__doc__)
        sys.exit(2)
    p = Proxy(int(a[0]), int(a[1]), float(a[2]) if len(a) > 2 else 0, float(a[3]) if len(a) > 3 else 0,
              float(a[4]) if len(a) > 4 else 0, int(a[5]) if len(a) > 5 else 1)
    print("mp_proxy: listening on %d for server %d" % (p.listen_port, p.server_ctl[1]))
    try:
        p.run()
    except KeyboardInterrupt:
        pass
    print(p.summary())

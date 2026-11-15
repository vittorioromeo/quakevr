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
CCREP_ACCEPT = 0x81
HOST = "127.0.0.1"


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
        self.stats = {"up": [0, 0, 0, 0], "down": [0, 0, 0, 0]}  # packets, bytes, dropped, largest
        self.first = None
        self.last = None
        # (B12b) reliable chunks down to drop (drop_reliable_down): how many more; each dropped one's sequence and time,
        # and the delay until the server sent it again
        self.drop_reliable = 0
        self.drop_least = 0
        self.dropped = {}  # sequence -> time dropped
        self.resent_after = {}  # sequence -> s

    def delay(self):
        return max(0.0, self.lag + (self.rng.uniform(-self.jitter, self.jitter) if self.jitter > 0 else 0.0))

    def drop_reliable_down(self, n=1, least=0):
        """Drop the next `n` reliable chunks down of `least` bytes or more (B12b's resend timings: resent_after)."""
        self.dropped.clear()
        self.resent_after.clear()
        self.drop_least = least
        self.drop_reliable = n

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

    def schedule(self, sock, addr, pkt, direction, control):
        st = self.stats[direction]
        if direction == "down" and self.reliable_down(pkt):
            st[2] += 1
            return
        if not control and self.loss > 0 and self.rng.random() < self.loss:
            st[2] += 1
            return
        st[0] += 1
        st[1] += len(pkt)
        st[3] = max(st[3], len(pkt))
        now = time.monotonic()
        self.first = self.first if self.first is not None else now
        self.last = now
        self.seq += 1
        heapq.heappush(self.queue, (now + self.delay(), self.seq, sock, addr, pkt))

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
            out[d] = {"pkts_s": round(n / span, 1), "bytes_s": round(by / span), "mean_B": by // max(n, 1)}
        return out

    def summary(self):
        span = (self.last - self.first) if self.first is not None and self.last is not None else 0.0
        parts = []
        for d in ("up", "down"):
            n, b, dropped, most = self.stats[d]
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

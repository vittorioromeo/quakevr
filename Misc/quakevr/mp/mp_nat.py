"""mp_nat.py -- a NAT router simulator for the multiplayer harness (B23: room codes, one server socket).

A game started with -natgateway <port> (net_dgrm.c) sends every datagram to this router's gateway socket on
127.0.0.1:<port>, its real destination in a 6-byte prefix (IPv4 address and port, network order), and takes datagrams
only from the gateway, the real source in the same prefix. The router sends each one on from a socket of its own
bound to its outside address (another loopback address: 127.0.0.2, 127.0.0.3...), as a home router maps an inside
port to an outside one, and lets in only what its kind allows:

    cone       one outside port for each inside port, whatever the destination (endpoint-independent mapping); a
               datagram in only from an address and port the inside port has sent to (port-restricted filtering):
               most home routers. Hole punching works.
    symmetric  an outside port for each inside port and destination: the room service sees another port than the
               one the host gets datagrams from. Hole punching fails.
    preserve   (cone) the outside port is the inside port's number when free (a port-preserving router); else any.

forward(outside_port, inside_port): a forwarded port (unfiltered in, and the inside port's datagrams out by it).
Standard library only.
"""

import random
import select
import socket
import threading

GATEWAY_HOST = "127.0.0.1"


class Mapping:
    def __init__(self, sock, inside_port, forwarded=False):
        self.sock = sock
        self.inside_port = inside_port
        self.forwarded = forwarded
        self.sent_to = set()


class Nat:
    def __init__(self, outside_ip, kind="cone", preserve=False, seed=1):
        assert kind in ("cone", "symmetric")
        self.outside_ip = outside_ip
        self.kind = kind
        self.preserve = preserve
        self.rng = random.Random(seed)
        self.gateway = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.gateway.bind((GATEWAY_HOST, 0))
        self.gateway_port = self.gateway.getsockname()[1]
        self.maps = {}      # inside port (cone) or (inside port, destination) (symmetric) -> Mapping
        self.by_sock = {}   # outside socket -> Mapping
        self.lock = threading.Lock()
        self.stats = {"out": 0, "in": 0, "filtered": 0}
        self.stop_flag = False
        self.thread = None

    def describe(self):
        return "%s NAT %s%s (gateway 127.0.0.1:%d)" % (self.kind, self.outside_ip, ", port preserving" if self.preserve
                                                      else "", self.gateway_port)

    def _bind(self, port):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.bind((self.outside_ip, port))
        except OSError:
            if port == 0:
                raise
            s.bind((self.outside_ip, 0))
        return s

    def forward(self, outside_port, inside_port):
        """A forwarded port: datagrams to outside_port go in to inside_port unfiltered, and inside_port's go out by
        it (as a real router's forward is the mapping of its inside port)."""
        with self.lock:
            m = Mapping(self._bind(outside_port), inside_port, forwarded=True)
            for key in [k for k, v in self.maps.items() if v.inside_port == inside_port]:
                old = self.maps.pop(key)
                self.by_sock.pop(old.sock, None)
                old.sock.close()
            self.maps[inside_port] = m
            self.by_sock[m.sock] = m
        return m.sock.getsockname()[1]

    def _mapping(self, inside_port, dest):
        key = inside_port if self.kind == "cone" or inside_port in self.maps and self.maps[inside_port].forwarded \
            else (inside_port, dest)
        m = self.maps.get(key)
        if m is None:
            m = Mapping(self._bind(inside_port if self.preserve else 0), inside_port)
            self.maps[key] = m
            self.by_sock[m.sock] = m
        return m

    def outside_port(self, inside_port):
        """The cone mapping's outside port of an inside port (None: none yet)."""
        m = self.maps.get(inside_port)
        return m.sock.getsockname()[1] if m else None

    def from_inside(self, pkt, src):
        if len(pkt) < 6 or src[0] != GATEWAY_HOST:
            return
        dest = (socket.inet_ntoa(pkt[0:4]), int.from_bytes(pkt[4:6], "big"))
        if dest[0].endswith(".255") or dest[1] == 0:
            return  # (a LAN broadcast: not routed)
        with self.lock:
            m = self._mapping(src[1], dest)
            m.sent_to.add(dest)
        try:
            m.sock.sendto(pkt[6:], dest)
            self.stats["out"] += 1
        except OSError:
            pass

    def from_outside(self, m, pkt, src):
        if not m.forwarded and src not in m.sent_to:
            self.stats["filtered"] += 1
            return
        self.stats["in"] += 1
        try:
            self.gateway.sendto(socket.inet_aton(src[0]) + src[1].to_bytes(2, "big") + pkt, (GATEWAY_HOST, m.inside_port))
        except OSError:
            pass

    def run(self):
        while not self.stop_flag:
            with self.lock:
                socks = [self.gateway] + list(self.by_sock)
            try:
                ready, _, _ = select.select(socks, [], [], 0.05)
            except OSError:
                continue  # (a mapping replaced by a forward meanwhile)
            for s in ready:
                try:
                    pkt, src = s.recvfrom(65535)
                except OSError:
                    continue  # (Windows: an ICMP port unreachable from a game that quit)
                if s is self.gateway:
                    self.from_inside(pkt, src)
                else:
                    m = self.by_sock.get(s)
                    if m:
                        self.from_outside(m, pkt, src)

    def start(self):
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()
        return self

    def stop(self):
        self.stop_flag = True
        if self.thread:
            self.thread.join(2)
        for s in [self.gateway] + list(self.by_sock):
            s.close()

    def summary(self):
        return "%s: %d out, %d in, %d filtered, %d mappings" % (self.describe(), self.stats["out"], self.stats["in"],
                                                               self.stats["filtered"], len(self.maps))

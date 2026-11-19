# Quake VR room service

`qvr_rendezvous.py` lets players join a multiplayer game by a **room code** instead of an IP address and a forwarded
port. A host's game registers with it and gets a 6-letter code (Multiplayer > Host Game shows it); a player types the
code at Multiplayer > Join Game (or `connect @CODE`). The service tells each the other's public address and port, both
games send to each other at once (UDP hole punching), and the game then goes **directly** between them: the service
never carries game traffic.

- One UDP port: **26100**. One Python 3 process, standard library only, no state on disk.
- Protocol version 2 (the wire format: the top of `qvr_rendezvous.py`; the game's side: `Quake/net_dgrm.c`): a
  registration makes a room only with the cookie the service gave the address it comes from (one more round trip
  before a host has his code); a game of version 1 is told the service's version and gets no code.
- Privacy: it sees each player's public IP address and the room code, keeps them only while the room lives (2 minutes
  after the host's last registration, which his game sends every 20 s), and never logs an address.
- Abuse limits: requests of exactly 64 bytes with the magic (anything else gets no answer), replies of 32 (no
  amplification), registrations only from addresses that read the service's cookie (spoofed ones make no room), a per-address rate limit (5 a second, bursts of 20), at most 20000 rooms (8 per address), 50000
  tracked addresses, 20 introductions per room in 10 s. Memory stays under a few tens of MB.
- The game's setting `vr_mp_rendezvous` (default `vittorioromeo.com:26100`) names the service; `vr_mp_room 0` stops
  a host registering.

## Deploy (Debian / Ubuntu)

From your machine, in the repository:

    bash Misc/rendezvous/deploy.sh vittorioromeo.com            # ssh as yourself (needs sudo there)
    bash Misc/rendezvous/deploy.sh vittorioromeo.com someuser   # ssh as someuser

It copies `qvr_rendezvous.py` and `qvr-rendezvous.service`, creates the system user `qvr-rendezvous`, installs them
in `/opt/qvr-rendezvous/` and `/etc/systemd/system/`, opens UDP 26100 in `ufw` if ufw is active, and (re)starts the
service. Running it again **updates** the service the same way.

By hand, the same steps:

    sudo apt install python3                                       # (stock on Debian/Ubuntu)
    sudo useradd --system --no-create-home --shell /usr/sbin/nologin qvr-rendezvous
    sudo install -d /opt/qvr-rendezvous
    sudo install -m 644 qvr_rendezvous.py /opt/qvr-rendezvous/
    sudo install -m 644 qvr-rendezvous.service /etc/systemd/system/
    sudo ufw allow 26100/udp                                       # or your firewall's equivalent
    sudo systemctl daemon-reload && sudo systemctl enable --now qvr-rendezvous

A cloud firewall in front of the VPS (the provider's panel) needs UDP 26100 inbound too.

## Run, logs, update

    systemctl status qvr-rendezvous
    journalctl -u qvr-rendezvous -f          # its start line, then an hourly count of rooms and requests
    sudo systemctl restart qvr-rendezvous    # rooms re-register by themselves within 20 s, with their codes

Check it from anywhere with Quake VR: host a game (Multiplayer > Host Game, 2 or more players); the console says
`Room code: ABCDEF (... you are <your public address> to the room service ...)`.

## Local test mode

    python3 qvr_rendezvous.py --local        # 127.0.0.1:26100, every reply printed
    python3 test_rendezvous.py               # the protocol and the limits, then a real exchange on 127.0.0.1

In the game: `vr_mp_rendezvous 127.0.0.1:26100`. The multiplayer harness runs it with games behind simulated routers:
`python Misc/quakevr/mp/mp_test.py nat --base <a Quake folder>` (cone NATs join by code, a symmetric NAT is told why).

## When the punch fails, and a relay (design note, not built)

Hole punching works when both routers are "cone" NATs (most home routers). Behind a symmetric NAT (some mobile and
carrier-grade NATs, some business routers) the joiner is told there is no direct link: the host forwards his UDP port
(26000), or they use a virtual LAN. A relay would close that gap: the service (or a second process) forwards the
datagrams of a pair that couldn't punch, like TURN. It would carry game traffic, about 25 KB/s down per player (16
players: ~400 KB/s), add a hop of latency, and need its own limits (bytes per room, rooms relayed at once). Worth
building only if the failures turn out common in practice.

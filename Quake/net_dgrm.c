/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include "q_stdinc.h"
#include "arch_def.h"
#include "net_sys.h"
#include "net_defs.h"
#include "net_dgrm.h"
#include "vr/vr_api.h" // QVR: PRFL_QUAKEVR, QVR_NET_VERSION
#include "q_ctype.h" // QVR: (B23) room codes

#if defined(_WIN32)
#pragma comment (lib, "advapi32.lib")
unsigned char __stdcall SystemFunction036 (void *buffer, unsigned long length); // (RtlGenRandom: no header declares it)
#elif !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__NetBSD__)
#include <unistd.h> // (getentropy)
#endif

// This is enables a simple IP banning mechanism
#define BAN_TEST

// these two macros are to make the code more readable
#define sfunc	net_landrivers[sock->landriver]
#define dfunc	net_landrivers[net_landriverlevel]

static int net_landriverlevel;

/* statistic counters */
static int packetsSent = 0;
static int packetsReSent = 0;
static int packetsTimedReSent = 0; // QVR: windows resent at their timeout (B12b)
static int packetsFastReSent = 0; // QVR: ... at acks of later chunks (B12b)
static int packetsReceived = 0;
static int receivedDuplicateCount = 0;
static int shortPacketCount = 0;
static int droppedDatagrams;

static struct
{
	unsigned int	length;
	unsigned int	sequence;
	byte	data[MAX_DATAGRAM];
} packetBuffer;

static int myDriverLevel;

// QVR: (review 2026-11-18, finding 2) 32 random bits no other host can predict (a connection's first sequences, a room
// request's nonce): the system's random source, with the clock and a counter mixed in (a failed call still differs)
static unsigned int DgrmRandom (void)
{
	static unsigned int	count;
	unsigned int	r = 0;
	unsigned long long	x;

#if defined(_WIN32)
	SystemFunction036 (&r, sizeof (r));
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	r = arc4random ();
#else
	if (getentropy (&r, sizeof (r)) != 0)
		r = 0;
#endif
	x = (unsigned long long) (Sys_DoubleTime () * 1e9) ^ ((unsigned long long) ++count << 32) ^ r; // (splitmix64)
	x += 0x9E3779B97F4A7C15ull;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
	return r ^ (unsigned int) (x ^ (x >> 31));
}

extern qboolean m_return_onerror;
extern char m_return_reason[256]; // QVR: (netmismatch) 32 before: a refusal's whole reason

// QVR: (B23) one server socket for every client (docs/vr-port/MP_DESIGN.md "NAT traversal", step 1). NetQuake's server
// opened a socket on a new port for each client and named that port in its accept: behind a router that doesn't keep a
// port's number outside, that socket's mapping got another, and a host who had forwarded his port still couldn't be
// joined. The server now talks to every client from its listening socket and tells them apart by address and port:
// that socket's datagrams are read here and queued for their client (qsocket_t's qvrIn, which its Datagram_GetMessage
// takes), the rest (connect requests, queries, the room service's) for _Datagram_CheckNewConnections. Bounded: past
// a queue's most its oldest datagram goes, as a full socket buffer would drop one.
#define DGRM_QUEUE_CLIENT	256	// 3.5 s of a client's moves
#define DGRM_QUEUE_CONTROL	64

typedef struct dgrmpacket_s
{
	struct dgrmpacket_s	*next;
	struct qsockaddr	from;
	int		length;
	byte	data[4];	// (length bytes: allocated so)
} dgrmpacket_t;

static dgrmqueue_t	controlQueue[MAX_NET_DRIVERS];	// each LAN driver's listening socket's
static byte		sharedBuffer[NET_DATAGRAMSIZE];
static int		sharedQueueDrops;	// datagrams dropped from a full queue (net_stats)

static void QueuePush (dgrmqueue_t *q, int most, const byte *data, int length, const struct qsockaddr *from)
{
	dgrmpacket_t *p = (dgrmpacket_t *) malloc (sizeof (dgrmpacket_t) + length);
	if (!p)
		return;
	p->next = NULL;
	p->from = *from;
	p->length = length;
	memcpy (p->data, data, length);
	if (q->last)
		q->last->next = p;
	else
		q->first = p;
	q->last = p;
	if (++q->count > most)
	{
		dgrmpacket_t *old = q->first;
		q->first = old->next;
		q->count--;
		free (old);
		sharedQueueDrops++;
	}
}

// The oldest datagram into `out` (at most `size` bytes) -> its length; 0: none
static int QueuePop (dgrmqueue_t *q, byte *out, int size, struct qsockaddr *from)
{
	dgrmpacket_t *p = q->first;
	int length;
	if (!p)
		return 0;
	q->first = p->next;
	if (!q->first)
		q->last = NULL;
	q->count--;
	length = q_min (p->length, size);
	memcpy (out, p->data, length);
	*from = p->from;
	free (p);
	return length;
}

static void QueueClear (dgrmqueue_t *q)
{
	while (q->first)
	{
		dgrmpacket_t *p = q->first;
		q->first = p->next;
		free (p);
	}
	q->last = NULL;
	q->count = 0;
}

// QVR: (B23) hosting and joining by room code (MP_DESIGN.md "NAT traversal", step 2; the service: Misc/rendezvous/).
// A host whose vr_mp_room is on registers with the room service (vr_mp_rendezvous) from his listening socket every
// ROOM_REGISTER_EVERY seconds: the service sees his router's outside address and port for that socket and gives him
// a room code. A joiner asks the service for the room's host from the socket he will play on; the service tells each
// the other's outside address, and both send at once -- the joiner his connect requests, the host ROOM_PUNCH
// datagrams -- so each router sees its side's datagram go out first and lets the other's in (UDP hole punching: cone
// NATs; a symmetric NAT on either side maps each destination to another port and the join fails with the reason).
// The wire format (Misc/rendezvous/qvr_rendezvous.py has the service's side): "QVRR", version, type, two zero bytes,
// then the type's fields; requests are ROOM_REQUEST bytes, replies ROOM_REPLY (a reply never outgrows its request).
// (review 2026-11-18, finding 11; version 2) A REGISTER carries the cookie the service gave this host's address
// (ROOM_COOKIE: an HMAC of it, so spoofed registrations from addresses that can't read the reply make no room), and
// every reply the host acts on has the nonce of his last request (a REFUSED or BADVERSION forged from the service's
// address stopped registering for 600 s).
// (2026-11-21, the port check; version 3) Once registered, the host asks the service (ROOM_PROBECHECK, the one
// request of version 3: REGISTER and JOIN stay version 2, so a version 2 service still gives room codes) to send a
// ROOM_PROBE from another of its ports to his outside IP address at his game port (net_hostport). His router has
// never sent to that port, so the probe gets in only through a forward (or a router that lets anything in): it arrives
// -> reachable (anyone can join, a player behind a symmetric NAT too); ROOM_PROBING (the probe sent) and no probe
// within ROOM_PROBE_WAIT -> not reachable (room codes still work for most players: hole punching); a version 2 service
// (BADVERSION) or none -> unknown. NET_PortCheck* (net.h): the Host Game page, the net HUD, vr_mp_roomcode.
#define ROOM_MAGIC	"QVRR"
#define ROOM_VERSION	2
#define ROOM_VERSION_PROBE	3	// (the port check's request only)
#define ROOM_COOKIE_LEN	8
#define ROOM_REQUEST	64
#define ROOM_REPLY	32
#define ROOM_CODE_LEN	6
#define ROOM_REGISTER	0x01	// host -> service: code wanted (6 bytes or zeros) at 8, nonce at 14, cookie at 18 (or zeros)
#define ROOM_JOIN	0x02	// joiner -> service: code at 8, nonce at 14
#define ROOM_PROBECHECK	0x03	// host -> service (version 3): code at 8, nonce at 14, his game port at 22
#define ROOM_REGISTERED	0x81	// -> host: code at 8, nonce at 14, his outside IPv4 at 18, port at 22
#define ROOM_PEER	0x82	// -> joiner: code, nonce, the host's outside address
#define ROOM_INCOMING	0x83	// -> host: code, 0, the joiner's outside address (punch it)
#define ROOM_NOROOM	0x84	// -> joiner: code, nonce: no such room
#define ROOM_REFUSED	0x85	// -> host: code, nonce, why at 18 (1: the service is full)
#define ROOM_BADVERSION	0x86	// -> either: code, nonce, the service's version at 18
#define ROOM_COOKIE	0x87	// -> host: code, nonce, his address's cookie at 24 (to be echoed in a REGISTER)
#define ROOM_PROBING	0x88	// -> host: code, nonce, the probes sent at 18 (0: this registration's was used)
#define ROOM_PROBE	0x89	// -> host's game port, from the service's other port: code, nonce, where it was sent
#define ROOM_PUNCH	0x90	// host -> joiner: opens the host's router to him (the joiner ignores it)
#define ROOM_DEFAULT_PORT	26100
#define ROOM_REGISTER_EVERY	20.0	// s (the service forgets a room 120 s after its last registration)
#define ROOM_RETRY_EVERY	2.0	// s, until the first reply
#define ROOM_PUNCH_FOR		1.0	// s a host punches towards a joiner (mpfix A4: 8 s at 0.1 s before -- 80 punches for one forged JOIN
					// with a victim's source; his router is open to the joiner after the first, and his connect
					// requests, which go on 8 s, are answered directly)
#define ROOM_PUNCH_EVERY	0.2	// s (5 punches)
#define ROOM_JOIN_WAIT		3.0	// s a joiner waits for the service's answer
#define ROOM_CONNECT_TRIES	32	// a room's connect requests (punches), ROOM_CONNECT_EVERY apart
#define ROOM_CONNECT_EVERY	0.25	// s
#define ROOM_PEERS	8
#define ROOM_PROBE_WAIT		2.0	// s after ROOM_PROBING without the probe: not reachable
#define ROOM_PROBE_GIVEUP	6.0	// s without ROOM_PROBING: unknown (asked again at the next registration)

typedef struct
{
	struct qsockaddr	addr;
	double	until;
	double	next;
} roompeer_t;

static struct
{
	char	service[64];		// vr_mp_rendezvous as last resolved ("": none)
	struct qsockaddr	serviceAddr;
	qboolean	resolved;
	double	resolveAt;		// the next look at the lookup (finding 6: its own back-off, vr_netlookup.cpp)
	qboolean	toldNotFound;
	char	code[ROOM_CODE_LEN + 1];	// this host's room ("": none yet)
	unsigned int	nonce;
	byte	cookie[ROOM_COOKIE_LEN];	// the service's last cookie for this host (zeros: none yet)
	double	nextRegister;
	double	firstSent;		// the first unanswered registration's time (0: answered)
	qboolean	toldUnanswered;
	roompeer_t	peers[ROOM_PEERS];
	// (the port check) its state (NET_PORTCHECK_*), its request's nonce, when it was sent (0: none waiting) and
	// answered (PROBING; 0: not yet), the port checked, the outside address it was checked at
	int		probeState;
	unsigned int	probeNonce;
	double	probeSent;
	double	probeAnswered;
	int		probePort;
	struct qsockaddr	probeOutside;
	qboolean	probeAsk;		// a REGISTERED wants a check sent (by RoomHostTick: the socket's)
	qboolean	listening;		// Datagram_Listen's last (a new hosting checks again)
} room;

static const char roomLetters[] = "ABCDEFGHJKLMNPQRSTUVWXYZ"; // (no I or O: read as 1 and 0)

static const char *RoomServiceName (void)
{
	return Cvar_VariableString ("vr_mp_rendezvous");
}

// QVR: (review 2026-11-18, finding 6) "host[:port]" (the port `port` when none is given) into `out` -> 1; 0 a name still
// being looked up (on a worker: vr_netlookup.cpp; none started unless `mayStart`); -1 not found, refused or malformed.
// An address in figures is read at once; a name is never looked up on the game thread (gethostbyname held the frame
// for the resolver's timeout, seconds, with no working DNS).
static int LookupAddr (const char *text, int port, struct qsockaddr *out, qboolean mayStart)
{
	char	host[64];
	char	*colon;
	unsigned int	ip;
	int		found;

	q_strlcpy (host, text, sizeof (host));
	if (!host[0])
		return -1;
	if (net_landriverlevel != 0) // (not the IPv4 driver: IPX's names are its own, read at once)
		return dfunc.GetAddrFromName (host, out) == -1 ? -1 : 1;
	if ((colon = strrchr (host, ':')) != NULL)
	{
		*colon = 0;
		port = Q_atoi (colon + 1);
	}
	if (port <= 0 || port > 65535)
		return -1;
	if (host[0] >= '0' && host[0] <= '9')
	{
		if (dfunc.GetAddrFromName (host, out) == -1)
			return -1;
	}
	else
	{
		if (NET_LoopbackRefusesName (host, NULL))
			return -1;
		found = VR_NetLookup (host, &ip, mayStart);
		if (found != 1)
			return found;
		memset (out, 0, sizeof (*out));
		out->qsa_family = AF_INET;
		((struct sockaddr_in *) out)->sin_addr.s_addr = ip;
	}
	dfunc.SetSocketPort (out, port);
	return 1;
}

// QVR: (finding 6) a lookup may start: no remote client is connected (a hosted game in play keeps its worker)
static qboolean LookupMayStart (void)
{
	qsocket_t *s;
	for (s = net_activeSockets; s; s = s->next)
		if (s->qvrShared)
			return false;
	return true;
}

static void RoomWrite (sys_socket_t s, int type, const char *code, unsigned int nonce, const byte *cookie, int size,
	const struct qsockaddr *to)
{
	byte	pkt[ROOM_REQUEST];

	memset (pkt, 0, sizeof (pkt));
	memcpy (pkt, ROOM_MAGIC, 4);
	pkt[4] = type == ROOM_PROBECHECK ? ROOM_VERSION_PROBE : ROOM_VERSION;
	pkt[5] = (byte) type;
	if (type == ROOM_PROBECHECK) // (the port checked: this game's)
	{
		pkt[22] = (byte) (net_hostport >> 8);
		pkt[23] = (byte) net_hostport;
	}
	if (code)
		memcpy (pkt + 8, code, q_min ((int) strlen (code), ROOM_CODE_LEN));
	pkt[14] = (byte) (nonce >> 24);
	pkt[15] = (byte) (nonce >> 16);
	pkt[16] = (byte) (nonce >> 8);
	pkt[17] = (byte) nonce;
	if (cookie)
		memcpy (pkt + 18, cookie, ROOM_COOKIE_LEN);
	dfunc.Write (s, pkt, size, (struct qsockaddr *) to);
}

static qboolean IsRoomPacket (const byte *data, int length)
{
	return length >= 8 && memcmp (data, ROOM_MAGIC, 4) == 0;
}

static unsigned int RoomNonce (const byte *pkt)
{
	return ((unsigned int) pkt[14] << 24) | ((unsigned int) pkt[15] << 16) | ((unsigned int) pkt[16] << 8) | pkt[17];
}

// The address a reply carries (IPv4 at 18, port at 22, network order)
static void RoomAddr (const byte *pkt, struct qsockaddr *out)
{
	memset (out, 0, sizeof (*out));
	out->qsa_family = AF_INET;
	memcpy (&((struct sockaddr_in *) out)->sin_addr, pkt + 18, 4);
	memcpy (&((struct sockaddr_in *) out)->sin_port, pkt + 22, 2);
}

// A room code typed by a player ("abc-def", " ABCDEF") made the service's -> qtrue
static qboolean RoomCleanCode (const char *in, char *out)
{
	int n = 0;
	for (; *in; in++)
	{
		char c = (char) q_toupper (*in);
		if (c == ' ' || c == '-')
			continue;
		if (n >= ROOM_CODE_LEN || !strchr (roomLetters, c))
			return false;
		out[n++] = c;
	}
	out[n] = 0;
	return n == ROOM_CODE_LEN;
}

const char *NET_RoomCode (void)
{
	return room.code;
}

// QVR: (the port check) this host's game port's reachability (NET_PORTCHECK_*; net.h)
int NET_PortCheck (void)
{
	if (!Cvar_VariableValue ("vr_mp_room") || !RoomServiceName ()[0])
		return NET_PORTCHECK_OFF;
	return room.probeState == NET_PORTCHECK_OFF ? NET_PORTCHECK_WAITING : room.probeState;
}

static char portCheckText[160]; // (NET_PortCheckText's)
static const char *const portCheckBriefs[] = {"unknown", "checking", "reachable", "NOT REACHABLE", "unknown", "unknown"};

// QVR: (the port check) as text: `brief` the net HUD's ("port 26000 reachable"), else the Host Game page's and
// vr_mp_roomcode's ("Port 26000: reachable")
const char *NET_PortCheckText (qboolean brief)
{
	const char *const *briefs = portCheckBriefs;
	const int state = NET_PortCheck ();
	if (brief)
		q_snprintf (portCheckText, sizeof (portCheckText), "port %d %s", net_hostport, briefs[state]);
	else if (state == NET_PORTCHECK_CLOSED)
		q_snprintf (portCheckText, sizeof (portCheckText), "Port %d: not reachable from the internet (forward UDP %d "
			"for players behind strict routers)", net_hostport, net_hostport);
	else
		q_snprintf (portCheckText, sizeof (portCheckText), "Port %d: %s", net_hostport,
			state == NET_PORTCHECK_OFF ? "unknown (room service off)"
			: state == NET_PORTCHECK_WAITING ? "checking..."
			: state == NET_PORTCHECK_REACHABLE ? "reachable"
			: state == NET_PORTCHECK_OLDSERVICE ? "unknown (the room service is too old to check)"
			: "unknown (the room service does not answer)");
	return portCheckText;
}

// (the port check) The result found: told once in the console
static void RoomProbeDecide (int state)
{
	if (state == room.probeState)
		return;
	room.probeState = state;
	if (state == NET_PORTCHECK_REACHABLE)
		Con_Printf ("Port %d is reachable from the internet (the room service's probe got in)\n", room.probePort);
	else if (state == NET_PORTCHECK_CLOSED)
		Con_Printf ("Port %d is not reachable from the internet: players behind strict routers can't join (forward UDP "
			"%d to this computer in your router; room codes still work for most players)\n", room.probePort,
			room.probePort);
	else if (state == NET_PORTCHECK_OLDSERVICE)
		Con_Printf ("Port %d: unknown (the room service %s is too old to check it)\n", room.probePort, room.service);
}

// (the port check) The room service's answers to it -> qtrue: taken. Its PROBE comes from another of the service's
// ports (the same IP address), so it is read before the service's own replies' source check.
static qboolean RoomProbeRead (const byte *pkt, int length, struct qsockaddr *from)
{
	if (!room.resolved || length < ROOM_REPLY || room.probeNonce == 0 || RoomNonce (pkt) != room.probeNonce
		|| memcmp (room.code, pkt + 8, ROOM_CODE_LEN) != 0 || dfunc.AddrCompare (from, &room.serviceAddr) < 0)
		return false;
	if (pkt[5] == ROOM_PROBE)
	{
		room.probeSent = 0;
		RoomProbeDecide (NET_PORTCHECK_REACHABLE);
		return true;
	}
	if (dfunc.AddrCompare (from, &room.serviceAddr) != 0)
		return false;
	switch (pkt[5])
	{
	case ROOM_PROBING:
		if (room.probeSent == 0)
			break;
		if (pkt[18] == 0)
			room.probeSent = 0; // (this registration's probe used: asked again at the next)
		else if (room.probeAnswered == 0)
			room.probeAnswered = net_time;
		break;
	case ROOM_BADVERSION: // (a version 2 service: no check; its room codes still work)
		room.probeSent = 0;
		RoomProbeDecide (NET_PORTCHECK_OLDSERVICE);
		break;
	case ROOM_NOROOM: // (the room not yet his from this address: asked again at the next registration)
		room.probeSent = 0;
		break;
	default:
		return false;
	}
	return true;
}

// A host's datagram from the room service (on his listening socket) -> qtrue: taken (any datagram of the room format
// is: a stray one is dropped)
static qboolean RoomHostRead (const byte *pkt, int length, struct qsockaddr *from)
{
	int i;
	struct qsockaddr peer;

	if (!IsRoomPacket (pkt, length))
		return false;
	if (RoomProbeRead (pkt, length, from))
		return true;
	if (!room.resolved || length < ROOM_REPLY || dfunc.AddrCompare (from, &room.serviceAddr) != 0)
		return true;
	// (finding 11) every reply acted on answers this host's last request (a UDP source is anyone's to write)
	if (pkt[5] != ROOM_INCOMING && RoomNonce (pkt) != room.nonce)
		return true;
	switch (pkt[5])
	{
	case ROOM_COOKIE:
		// (finding 11) the service wants its cookie for this address echoed: registered again at once with it
		memcpy (room.cookie, pkt + 24, ROOM_COOKIE_LEN);
		room.nextRegister = net_time;
		break;
	case ROOM_REGISTERED:
		room.firstSent = 0;
		room.toldUnanswered = false;
		// (the port check) asked for while undecided, and again when his outside address or game port changed
		RoomAddr (pkt, &peer);
		if (room.probeSent == 0 && (room.probeState == NET_PORTCHECK_WAITING || room.probeState == NET_PORTCHECK_NOANSWER
			|| room.probePort != net_hostport || dfunc.AddrCompare (&peer, &room.probeOutside) != 0))
		{
			if (room.probePort != net_hostport || dfunc.AddrCompare (&peer, &room.probeOutside) != 0)
				room.probeState = NET_PORTCHECK_WAITING;
			room.probeOutside = peer;
			room.probePort = net_hostport;
			room.probeAsk = true;
		}
		if (memcmp (room.code, pkt + 8, ROOM_CODE_LEN) != 0)
		{
			memcpy (room.code, pkt + 8, ROOM_CODE_LEN);
			room.code[ROOM_CODE_LEN] = 0;
			RoomAddr (pkt, &peer);
			Con_Printf ("Room code: %s (players join with it at Multiplayer > Join Game; you are %s to the room "
				"service %s)\n", room.code, dfunc.AddrToString (&peer), room.service);
		}
		room.nextRegister = net_time + ROOM_REGISTER_EVERY;
		break;
	case ROOM_INCOMING:
		if (memcmp (room.code, pkt + 8, ROOM_CODE_LEN) != 0)
			break;
		RoomAddr (pkt, &peer);
		for (i = 0; i < ROOM_PEERS; i++)
			if (room.peers[i].until < net_time || dfunc.AddrCompare (&room.peers[i].addr, &peer) == 0)
				break;
		if (i == ROOM_PEERS)
			break; // (8 joiners in the last 8 s: the next gets punched for at his retry)
		room.peers[i].addr = peer;
		room.peers[i].until = net_time + ROOM_PUNCH_FOR;
		room.peers[i].next = net_time;
		Con_DPrintf ("Room %s: %s is joining\n", room.code, dfunc.AddrToString (&peer));
		break;
	case ROOM_REFUSED:
		if (!room.toldUnanswered)
			Con_Printf ("The room service %s has no room for this game now (players can still join by your IP address)\n",
				room.service);
		room.toldUnanswered = true;
		room.nextRegister = net_time + 60.0;
		break;
	case ROOM_BADVERSION:
		if (!room.toldUnanswered)
			Con_Printf ("The room service %s speaks version %d, this game %d: no room code\n", room.service, pkt[18],
				ROOM_VERSION);
		room.toldUnanswered = true;
		room.nextRegister = net_time + 600.0;
		break;
	}
	return true;
}

// A host's room each server frame: registered (again), its joiners punched for
static void RoomHostTick (sys_socket_t acceptsock)
{
	const char	*name = RoomServiceName ();
	qsocket_t	*s;
	int		i;

	if (!Cvar_VariableValue ("vr_mp_room") || !name[0])
	{
		room.code[0] = 0;
		room.service[0] = 0;
		room.resolved = false;
		room.probeState = NET_PORTCHECK_OFF;
		return;
	}
	if (Q_strcmp (name, room.service) != 0)
	{
		room.probeState = NET_PORTCHECK_WAITING;
		room.probeSent = 0;
		room.probeAsk = false;
		memset (&room.probeOutside, 0, sizeof (room.probeOutside));
		q_strlcpy (room.service, name, sizeof (room.service));
		room.resolved = false;
		room.resolveAt = 0;
		room.toldNotFound = false;
		room.code[0] = 0;
		memset (room.cookie, 0, sizeof (room.cookie));
		room.nextRegister = 0;
		room.firstSent = 0;
		room.toldUnanswered = false;
	}
	if (!room.resolved)
	{
		int found;
		if (net_time < room.resolveAt)
			return;
		// (finding 6: on a worker, none started while remote clients are connected; a failed name tried again after
		// the lookup's back-off)
		found = LookupAddr (room.service, ROOM_DEFAULT_PORT, &room.serviceAddr, LookupMayStart ());
		if (found != 1)
		{
			if (found == -1 && !room.toldNotFound)
				Con_Printf ("The room service %s was not found: no room code (players can still join by your IP "
					"address)\n", room.service);
			room.toldNotFound |= found == -1;
			if (found == -1 && room.probeState == NET_PORTCHECK_WAITING)
				room.probeState = NET_PORTCHECK_NOANSWER;
			room.resolveAt = net_time + (found == -1 ? 1.0 : 0.1);
			return;
		}
		room.resolved = true;
	}
	if (net_time >= room.nextRegister)
	{
		room.nonce = DgrmRandom (); // (finding 11: not guessable)
		RoomWrite (acceptsock, ROOM_REGISTER, room.code, room.nonce, room.cookie, ROOM_REQUEST, &room.serviceAddr);
		if (room.firstSent == 0)
			room.firstSent = net_time;
		room.nextRegister = net_time + (room.code[0] ? ROOM_REGISTER_EVERY / 4 : ROOM_RETRY_EVERY);
		if (net_time - room.firstSent > 3 * ROOM_RETRY_EVERY && !room.toldUnanswered)
		{
			Con_Printf ("The room service %s does not answer: no room code yet (players can still join by your IP "
				"address)\n", room.service);
			room.toldUnanswered = true;
			if (room.probeState == NET_PORTCHECK_WAITING)
				room.probeState = NET_PORTCHECK_NOANSWER;
		}
		if (net_time - room.firstSent > 60.0)
			room.nextRegister = net_time + ROOM_REGISTER_EVERY; // (gone a minute: asked less often)
	}
	// (the port check) asked for by a REGISTERED; decided by the probe (RoomProbeRead) or its absence
	if (room.probeAsk)
	{
		room.probeAsk = false;
		do
			room.probeNonce = DgrmRandom ();
		while (room.probeNonce == 0);
		room.probeSent = net_time;
		room.probeAnswered = 0;
		RoomWrite (acceptsock, ROOM_PROBECHECK, room.code, room.probeNonce, NULL, ROOM_REQUEST, &room.serviceAddr);
	}
	if (room.probeSent != 0 && room.probeAnswered != 0 && net_time - room.probeAnswered > ROOM_PROBE_WAIT)
	{
		room.probeSent = 0;
		RoomProbeDecide (NET_PORTCHECK_CLOSED);
	}
	else if (room.probeSent != 0 && room.probeAnswered == 0 && net_time - room.probeSent > ROOM_PROBE_GIVEUP)
	{
		room.probeSent = 0;
		RoomProbeDecide (NET_PORTCHECK_NOANSWER);
	}
	for (i = 0; i < ROOM_PEERS; i++)
	{
		roompeer_t *p = &room.peers[i];
		if (p->until < net_time || p->next > net_time)
			continue;
		for (s = net_activeSockets; s; s = s->next)
			if (s->qvrShared && dfunc.AddrCompare (&s->addr, &p->addr) == 0)
				break;
		if (s)
		{
			p->until = 0; // (joined)
			continue;
		}
		RoomWrite (acceptsock, ROOM_PUNCH, room.code, 0, NULL, ROOM_REPLY, &p->addr);
		p->next = net_time + ROOM_PUNCH_EVERY;
	}
}

// The listening socket's datagrams read: each client's queued for him, the room service's taken, the rest queued for
// _Datagram_CheckNewConnections
#define DGRM_SHARED_READS	1024
static void ReadShared (int landriver, sys_socket_t acceptsock)
{
	struct qsockaddr	from;
	int		length;
	unsigned int	head;
	qsocket_t	*s;
	net_landriver_t	*driver = &net_landrivers[landriver];

	int		reads = 0;

	net_landriverlevel = landriver; // (dfunc: RoomHostRead's)

	// (mpfix A5) DGRM_SHARED_READS a call at most: a flood as fast as it is read kept the server frame in this loop (the
	// rest waits in the system's buffer for the next call, the next frame's at the latest)
	while (reads++ < DGRM_SHARED_READS && (length = driver->Read (acceptsock, sharedBuffer, sizeof (sharedBuffer), &from)) > 0)
	{
		if (RoomHostRead (sharedBuffer, length, &from))
			continue;
		head = length >= 4 ? ((unsigned int) sharedBuffer[0] << 24) : 0;
		if (length >= NET_HEADERSIZE && !(head & NETFLAG_CTL))
		{
			for (s = net_activeSockets; s; s = s->next)
				if (s->qvrShared && s->landriver == landriver && driver->AddrCompare (&from, &s->addr) == 0)
					break;
			if (s)
				QueuePush (&s->qvrIn, DGRM_QUEUE_CLIENT, sharedBuffer, length, &from);
			continue; // (nobody's: a client gone)
		}
		QueuePush (&controlQueue[landriver], DGRM_QUEUE_CONTROL, sharedBuffer, length, &from);
	}
}

// A connection's next datagram into packetBuffer -> its length (0 none, -1 an error)
static int ReadDatagram (qsocket_t *sock, struct qsockaddr *from)
{
	if (!sock->qvrShared)
		return sfunc.Read (sock->socket, (byte *)&packetBuffer, NET_DATAGRAMSIZE, from);
	ReadShared (sock->landriver, sock->socket);
	return QueuePop (&sock->qvrIn, (byte *)&packetBuffer, NET_DATAGRAMSIZE, from);
}

// QVR: (B23) joining by room code: the room's host asked of the room service from the joiner's socket -> qtrue, his
// outside address in `host`; qfalse: m_return_reason says why (the console the whole of it)
static qboolean RoomJoin (sys_socket_t sock, const char *typed, struct qsockaddr *host)
{
	char	code[ROOM_CODE_LEN + 1];
	struct qsockaddr	service;
	struct qsockaddr	from;
	const char	*name = RoomServiceName ();
	unsigned int	nonce;
	double	start, sent;
	int		length;

	if (!RoomCleanCode (typed, code))
	{
		Con_Printf ("\"%s\" is not a room code: a room code is %d letters (the host's Multiplayer > Host Game page "
			"shows it)\n", typed, ROOM_CODE_LEN);
		q_strlcpy (m_return_reason, "A room code is 6 letters", sizeof (m_return_reason));
		return false;
	}
	if (!name[0])
	{
		Con_Printf ("No room service set (vr_mp_rendezvous): join by the host's IP address\n");
		q_strlcpy (m_return_reason, "No room service set", sizeof (m_return_reason));
		return false;
	}
	// (finding 6: looked up on a worker, waited for at most ROOM_JOIN_WAIT: a resolver with no network held the
	// connect for its own timeout)
	start = SetNetTime ();
	while ((length = LookupAddr (name, ROOM_DEFAULT_PORT, &service, true)) == 0 && SetNetTime () - start < ROOM_JOIN_WAIT)
		Sys_Sleep (5);
	if (length != 1)
	{
		Con_Printf ("The room service %s was not found (no internet?): join by the host's IP address\n", name);
		q_strlcpy (m_return_reason, "Room service not found", sizeof (m_return_reason));
		return false;
	}
	Con_Printf ("Looking up room %s at %s...\n", code, name);
	SCR_UpdateScreen ();
	nonce = DgrmRandom (); // (finding 11: not guessable)
	start = SetNetTime ();
	sent = -1.0;
	while (SetNetTime () - start < ROOM_JOIN_WAIT)
	{
		if (sent < 0 || net_time - sent >= 0.5)
		{
			RoomWrite (sock, ROOM_JOIN, code, nonce, NULL, ROOM_REQUEST, &service);
			sent = net_time;
		}
		length = dfunc.Read (sock, net_message.data, net_message.maxsize, &from);
		if (length <= 0)
		{
			Sys_Sleep (1);
			continue;
		}
		if (length < ROOM_REPLY || !IsRoomPacket (net_message.data, length) || dfunc.AddrCompare (&from, &service) != 0)
			continue;
		if (RoomNonce (net_message.data) != nonce) // (finding 11: a BADVERSION too)
			continue;
		if (net_message.data[5] == ROOM_BADVERSION)
		{
			Con_Printf ("The room service %s speaks version %d, this game %d: join by the host's IP address\n", name,
				net_message.data[18], ROOM_VERSION);
			q_strlcpy (m_return_reason, "Room service: other version", sizeof (m_return_reason));
			return false;
		}
		if (memcmp (net_message.data + 8, code, ROOM_CODE_LEN) != 0)
			continue;
		if (net_message.data[5] == ROOM_NOROOM)
		{
			Con_Printf ("No room %s at %s: check the code (a room ends 2 minutes after its game)\n", code, name);
			q_strlcpy (m_return_reason, "No room with that code", sizeof (m_return_reason));
			return false;
		}
		if (net_message.data[5] == ROOM_PEER)
		{
			RoomAddr (net_message.data, host);
			Con_Printf ("Room %s: the host is %s\n", code, dfunc.AddrToString (host));
			return true;
		}
	}
	Con_Printf ("The room service %s does not answer: try again later, or join by the host's IP address\n", name);
	q_strlcpy (m_return_reason, "Room service not answering", sizeof (m_return_reason));
	return false;
}

static void NET_RoomCode_f (void)
{
	if (sv.active && svs.maxclients > 1) // (the port check: also without a code)
		Con_Printf ("%s\n", NET_PortCheckText (false));
	if (!room.code[0] || !sv.active)
	{
		Con_Printf ("No room code: %s\n", !sv.active || svs.maxclients < 2 ? "not hosting a multiplayer game"
			: !Cvar_VariableValue ("vr_mp_room") ? "vr_mp_room is 0"
			: !RoomServiceName ()[0] ? "no room service (vr_mp_rendezvous)" : "the room service has not answered yet");
		return;
	}
	Con_Printf ("Room code: %s\n", room.code);
	if (cls.state != ca_dedicated && Cmd_Argc () > 1 && !Q_strcmp (Cmd_Argv (1), "copy"))
	{
		if (SDL_SetClipboardText (room.code) == 0)
			Con_Printf ("(copied to the clipboard)\n");
	}
}


// QVR: (B23, tests) -natgateway <port>: every datagram of the IPv4 driver (the first LAN driver, both platforms) by way of
// the multiplayer harness's NAT simulator on 127.0.0.1:<port> (Misc/quakevr/mp/mp_nat.py), the real address before it
// in 6 bytes (IPv4 and port, network order) both ways; a datagram from anywhere else is dropped (only the NAT reaches
// this game). The driver's Read and Write are wrapped at start-up (NatShim), so the drivers carry no test code.
static struct
{
	struct qsockaddr	gateway;
	int		(*read) (sys_socket_t socketid, byte *buf, int len, struct qsockaddr *addr);
	int		(*write) (sys_socket_t socketid, byte *buf, int len, struct qsockaddr *addr);
	byte	buffer[NET_DATAGRAMSIZE + 6];
} nat;

static int NatRead (sys_socket_t socketid, byte *buf, int len, struct qsockaddr *addr)
{
	struct qsockaddr	from;
	int		ret, skipped;

	for (skipped = 0; skipped < NET_EMPTY_READS; skipped++)
	{
		ret = nat.read (socketid, nat.buffer, sizeof (nat.buffer), &from);
		if (ret <= 0)
			return ret;
		if (ret <= 6 || net_landrivers[0].AddrCompare (&from, &nat.gateway) != 0)
			continue; // (an empty one skipped as the drivers' Read does; another sender's dropped)
		memset (addr, 0, sizeof (*addr));
		addr->qsa_family = AF_INET;
		memcpy (&((struct sockaddr_in *) addr)->sin_addr, nat.buffer, 4);
		memcpy (&((struct sockaddr_in *) addr)->sin_port, nat.buffer + 4, 2);
		ret = q_min (ret - 6, len);
		memcpy (buf, nat.buffer + 6, ret);
		return ret;
	}
	return 0;
}

static int NatWrite (sys_socket_t socketid, byte *buf, int len, struct qsockaddr *addr)
{
	if (NET_LoopbackRefusesTo (addr)) // (the real destination's fence: the gateway's is loopback)
		return len;
	if (len < 0 || len > NET_DATAGRAMSIZE)
		return nat.write (socketid, buf, len, addr);
	memcpy (nat.buffer, &((struct sockaddr_in *) addr)->sin_addr, 4);
	memcpy (nat.buffer + 4, &((struct sockaddr_in *) addr)->sin_port, 2);
	memcpy (nat.buffer + 6, buf, len);
	return nat.write (socketid, nat.buffer, len + 6, &nat.gateway) < 0 ? 0 : len;
}

static void NatShim (void)
{
	struct sockaddr_in	*gw = (struct sockaddr_in *) &nat.gateway;
	int		i = COM_CheckParm ("-natgateway");

	if (!i || i + 1 >= com_argc)
		return;
	memset (&nat.gateway, 0, sizeof (nat.gateway));
	gw->sin_family = AF_INET;
	gw->sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	gw->sin_port = htons ((unsigned short) Q_atoi (com_argv[i + 1]));
	nat.read = net_landrivers[0].Read;
	nat.write = net_landrivers[0].Write;
	net_landrivers[0].Read = NatRead;
	net_landrivers[0].Write = NatWrite;
	Con_SafePrintf ("UDP: every datagram through the NAT simulator on 127.0.0.1:%s\n", com_argv[i + 1]);
}


static char *StrAddr (struct qsockaddr *addr)
{
	static char buf[34];
	byte *p = (byte *)addr;
	int n;

	for (n = 0; n < 16; n++)
		sprintf (buf + n * 2, "%02x", *p++);
	return buf;
}


#ifdef BAN_TEST

static struct in_addr	banAddr;
static struct in_addr	banMask;

static void NET_Ban_f (void)
{
	char	addrStr [32];
	char	maskStr [32];
	// QVR: (review S1) the console's only: a guest's `ban` is refused before it gets here (cmd.c, B28)
	void	(*print_fn)(const char *fmt, ...) FUNCP_PRINTF(1,2) = Con_Printf;

	if (!sv.active)
	{
		Cmd_ForwardToServer ();
		return;
	}

	switch (Cmd_Argc ())
	{
	case 1:
		if (banAddr.s_addr != INADDR_ANY)
		{
			Q_strcpy(addrStr, inet_ntoa(banAddr));
			Q_strcpy(maskStr, inet_ntoa(banMask));
			print_fn("Banning %s [%s]\n", addrStr, maskStr);
		}
		else
			print_fn("Banning not active\n");
		break;

	case 2:
		if (q_strcasecmp(Cmd_Argv(1), "off") == 0)
			banAddr.s_addr = INADDR_ANY;
		else
			banAddr.s_addr = inet_addr(Cmd_Argv(1));
		banMask.s_addr = INADDR_NONE;
		break;

	case 3:
		banAddr.s_addr = inet_addr(Cmd_Argv(1));
		banMask.s_addr = inet_addr(Cmd_Argv(2));
		break;

	default:
		print_fn("BAN ip_address [mask]\n");
		break;
	}
}
#endif	// BAN_TEST


// QVR: a reliable message goes out in chunks of at most DGRM_RELIABLE_CHUNK bytes (each its own UDP datagram, small
// enough never to be IP-fragmented: NetQuake sent up to MAX_DATAGRAM, 64000 B, one lost fragment losing it all and some
// routers dropping fragments), with up to DGRM_RELIABLE_WINDOW chunks in flight (NetQuake waited for each chunk's ack: a
// round trip a chunk) (docs/vr-port/MP_PLAN.md, B2).
//
// Selective acks (B13): NetQuake's receiver acked every data packet but took only the next one in order, so the ack of
// a chunk it dropped (one that overtook a late one) read as delivered once the late one's ack came first: under
// reordering the sender called a message done that the receiver never completed, and the connection stalled (a join
// through 30 ms of jitter: signon keepalives only). A Quake VR receiver now holds the chunks that come before their
// turn (a window's worth: every chunk in flight fits) and acks a chunk only once it has it; the sender marks each chunk
// acked, moves on past the ones acked in order and resends only the ones not acked. A peer that is not Quake VR (no
// network version in its connect request or accept reply) still acks the chunks it drops: it gets a window of one
// chunk, NetQuake's stop and wait, where an ack is always for the chunk in flight.
//
// Resends (B12b): NetQuake's timeout was a fixed second. It is now twice the chunks' smoothed round trip (Karn's: a
// resent chunk's ack not measured) plus DGRM_RESEND_MARGIN, at least DGRM_RESEND_LEAST, doubling with each timeout in a
// row (a peer not reading, a map loading: never more often than NetQuake's backstop allows) up to DGRM_RESEND_MOST. And
// fast: DGRM_FAST_ACKS acks of later chunks (the receiver has them, so one before was lost) send the chunks not acked
// before them again at once, once until the next ack in order.
// (review 2026-11-18, finding 12) Past two timeouts in a row the peer is not reading (a map loading, a game gone without a
// word): the timeout goes on doubling past DGRM_RESEND_MOST to DGRM_RESEND_STALLED, so it is not sent the whole window
// (up to 16 x 1208 B) every second for net_messagetimeout's 300 s.
#define DGRM_RELIABLE_CHUNK		NET_RELIABLE_CHUNK
#define DGRM_RELIABLE_WINDOW	NET_RELIABLE_WINDOW
#define DGRM_RESEND_LEAST		0.15
#define DGRM_RESEND_MOST		1.0
#define DGRM_RESEND_STALLED		4.0
#define DGRM_RESEND_MARGIN		0.05
#define DGRM_FAST_ACKS			2

// QVR: (review 2026-11-18, finding 2) NetQuake's sequences started at 0 and its unreliable test was an unsigned "less
// than" with no bound: anyone who knew a player's address (CCREP_PLAYER_INFO gave it) could send one datagram in his
// name with a sequence far ahead, and every later real one read as stale (frozen), or inject a reliable chunk at the
// next sequence (a stufftext). Each side now draws its own channels' first sequences at random (the client's in its
// CCREQ_CONNECT, the server's in its CCREP_ACCEPT, network version 39: no header byte added), the sequences wrap, and
// an unreliable datagram is taken only within a window around the next one expected: DGRM_UNRELIABLE_AHEAD ahead (a
// second of 72 Hz datagrams lost in a row is 72), DGRM_UNRELIABLE_BEHIND behind for a connection that reads overtaken
// ones (qvrAcceptStale: the server's of a client's moves). One outside it is dropped, unless nothing in the window came
// for DGRM_RESYNC_SILENCE (a link back after a long cut: the peer's sequence went on): then it is the next one. A guess
// lands in the window once in 2^22, and freezes the link for at most DGRM_RESYNC_SILENCE.
#define DGRM_UNRELIABLE_AHEAD	1024
#define DGRM_UNRELIABLE_BEHIND	256
#define DGRM_RESYNC_SILENCE		3.0
static int	outOfWindow;	// unreliable datagrams dropped outside the window (net_stats)

static unsigned int ReliableChunks (const qsocket_t *sock)
{
	return ((unsigned int) sock->sendMessageLength + DGRM_RELIABLE_CHUNK - 1) / DGRM_RELIABLE_CHUNK;
}

// the chunks in flight at most: a Quake VR peer's window, else NetQuake's one (selective acks, above); (mpfix A1) one
// until the peer has acked a chunk (a server's new client: no more sent to an address not shown to take replies)
static unsigned int ReliableWindow (const qsocket_t *sock)
{
	return sock->qvrNetVersion && sock->qvrHeard ? DGRM_RELIABLE_WINDOW : 1;
}

// the chunk with this sequence (sendChunkBase is the first's)
static int SendReliableChunk (qsocket_t *sock, unsigned int sequence)
{
	unsigned int	offset = (sequence - sock->sendChunkBase) * DGRM_RELIABLE_CHUNK;
	unsigned int	dataLen = q_min ((unsigned int) DGRM_RELIABLE_CHUNK, (unsigned int) sock->sendMessageLength - offset);
	unsigned int	eom = (offset + dataLen >= (unsigned int) sock->sendMessageLength) ? NETFLAG_EOM : 0;
	unsigned int	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong(packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong(sequence);
	Q_memcpy (packetBuffer.data, sock->sendMessage + offset, dataLen);

	if (sfunc.Write (sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->qvrBytesOut += packetLen; // QVR: (B13) the net HUD's
	if (sequence != sock->sendSequence)
		sock->qvrResent++;
	sock->lastSendTime = net_time;
	sock->chunkResent[sequence % DGRM_RELIABLE_WINDOW] = sequence != sock->sendSequence; // (a first send: the newest)
	sock->chunkSentTime[sequence % DGRM_RELIABLE_WINDOW] = net_time;
	return 1;
}

// the resend timeout (B12b: above)
static double ResendTimeout (const qsocket_t *sock)
{
	double	t;

	// (nothing measured yet: NetQuake's second)
	t = sock->reliableRtt <= 0 ? DGRM_RESEND_MOST : q_max (DGRM_RESEND_LEAST, 2 * sock->reliableRtt + DGRM_RESEND_MARGIN);
	t *= (double) (1 << q_min (sock->resendBackoff, 6));
	if (sock->resendBackoff < 2)
		return q_min (t, DGRM_RESEND_MOST);
	// (two timed resends unanswered: 2 s, then 4 s at most -- finding 12)
	return q_min (t, q_min (DGRM_RESEND_STALLED, DGRM_RESEND_MOST * (double) (1 << q_min (sock->resendBackoff - 1, 6))));
}

// the chunks not sent yet that the window has room for
static int SendMessageNext (qsocket_t *sock)
{
	unsigned int	end = sock->sendChunkBase + ReliableChunks (sock);

	sock->sendNext = false;
	if (sock->canSend)
		return 1; // (all of it acked: nothing in flight)
	while ((int) (end - sock->sendSequence) > 0 && sock->sendSequence - sock->ackSequence < ReliableWindow (sock))
	{
		if (SendReliableChunk (sock, sock->sendSequence) == -1)
			return -1;
		sock->sendSequence++;
		packetsSent++;
	}
	return 1;
}

// the chunks in flight before `upTo` not acked yet, again (B13: the receiver holds the ones acked)
static int ReSendMessage (qsocket_t *sock, unsigned int upTo)
{
	unsigned int	sequence;

	sock->sendNext = false;
	for (sequence = sock->ackSequence; sequence != upTo; sequence++)
	{
		if (sock->chunkAcked[sequence % DGRM_RELIABLE_WINDOW])
			continue;
		if (SendReliableChunk (sock, sequence) == -1)
			return -1;
		packetsReSent++;
	}
	return 1;
}



int Datagram_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
#ifdef DEBUG
	if (data->cursize == 0)
		Sys_Error("Datagram_SendMessage: zero length message");

	if (data->cursize > NET_MAXMESSAGE)
		Sys_Error("Datagram_SendMessage: message too big: %u", data->cursize);

	if (sock->canSend == false)
		Sys_Error("SendMessage: called with canSend == false");
#endif

	Q_memcpy(sock->sendMessage, data->data, data->cursize);
	sock->sendMessageLength = data->cursize;
	sock->sendChunkBase = sock->sendSequence; // QVR: (== ackSequence: the last message all acked)
	sock->canSend = false;

	return SendMessageNext (sock); // QVR: its first chunks, as many as the window takes
}


qboolean Datagram_CanSendMessage (qsocket_t *sock)
{
	if (sock->sendNext)
		SendMessageNext (sock);

	return sock->canSend;
}


qboolean Datagram_CanSendUnreliableMessage (qsocket_t *sock)
{
	return true;
}


int Datagram_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int	packetLen;

#ifdef DEBUG
	if (data->cursize == 0)
		Sys_Error("Datagram_SendUnreliableMessage: zero length message");

	if (data->cursize > MAX_DATAGRAM)
		Sys_Error("Datagram_SendUnreliableMessage: message too big: %u", data->cursize);
#endif

	packetLen = NET_HEADERSIZE + data->cursize;

	packetBuffer.length = BigLong(packetLen | NETFLAG_UNRELIABLE);
	packetBuffer.sequence = BigLong(sock->unreliableSendSequence++);
	Q_memcpy (packetBuffer.data, data->data, data->cursize);

	if (sfunc.Write (sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->qvrBytesOut += packetLen; // QVR: (B13) the net HUD's
	packetsSent++;
	return 1;
}


// QVR: (B13) the next reliable chunk in order taken into the message: 1 the message is whole (in net_message), -1 it
// overflows (a broken peer)
static int TakeChunk (qsocket_t *sock, const byte *data, unsigned int length, qboolean eom)
{
	sock->receiveSequence++;
	if ((unsigned int) sock->receiveMessageLength + length > NET_MAXMESSAGE)
	{
		Con_Printf ("Reliable message too long\n");
		sock->receiveMessageLength = 0;
		return -1;
	}
	if (eom)
	{
		SZ_Clear (&net_message);
		SZ_Write (&net_message, sock->receiveMessage, sock->receiveMessageLength);
		SZ_Write (&net_message, data, length);
		sock->receiveMessageLength = 0;
		return 1;
	}
	Q_memcpy (sock->receiveMessage + sock->receiveMessageLength, data, length);
	sock->receiveMessageLength += length;
	return 0;
}

// QVR: (B13) the held chunks next in order taken (selective acks, above): as TakeChunk
static int TakeHeldChunks (qsocket_t *sock)
{
	int	slot, length, ret;

	for (;;)
	{
		slot = sock->receiveSequence % DGRM_RELIABLE_WINDOW;
		length = sock->heldLength[slot];
		if (length <= 0)
			return 0;
		sock->heldLength[slot] = 0;
		ret = TakeChunk (sock, sock->held[slot], (unsigned int) length, sock->heldEom[slot]);
		if (ret)
			return ret;
	}
}

static void SendAck (qsocket_t *sock, unsigned int sequence, struct qsockaddr *to)
{
	packetBuffer.length = BigLong(NET_HEADERSIZE | NETFLAG_ACK);
	packetBuffer.sequence = BigLong(sequence);
	sfunc.Write (sock->socket, (byte *)&packetBuffer, NET_HEADERSIZE, to);
	sock->qvrBytesOut += NET_HEADERSIZE; // QVR: (B13) the net HUD's
}

int	Datagram_GetMessage (qsocket_t *sock)
{
	unsigned int	length;
	unsigned int	flags;
	int				ret = 0;
	struct qsockaddr readaddr;
	unsigned int	sequence;
	unsigned int	count;
	unsigned int	received;
	unsigned int	slot;

	if (!sock->canSend)
		if ((net_time - sock->lastSendTime) > ResendTimeout (sock)) // QVR: (B12b; NetQuake's: a second)
		{
			ReSendMessage (sock, sock->sendSequence);
			sock->resendBackoff++;
			packetsTimedReSent++;
		}

	// QVR: (B13) chunks held behind a message that was whole on the last read
	ret = TakeHeldChunks (sock);
	if (ret)
		return ret;

	while (1)
	{
		length = (unsigned int) ReadDatagram (sock, &readaddr); // QVR: (B23) a server's client's from his queue

	//	if ((rand() & 255) > 220)
	//		continue;

		if (length == 0)
			break;

		if (length == (unsigned int)-1)
		{
			Con_Printf("Read error\n");
			return -1;
		}

		// QVR: (B23) a room's host's punches still coming after the join, the room service's answer to a JOIN sent again
		// (net_dgrm.c's room codes): not game datagrams
		if (IsRoomPacket ((const byte *)&packetBuffer, (int) length))
			continue;

		if (sfunc.AddrCompare(&readaddr, &sock->addr) != 0)
		{
			Con_Printf("Forged packet received\n");
			Con_Printf("Expected: %s\n", StrAddr (&sock->addr));
			Con_Printf("Received: %s\n", StrAddr (&readaddr));
			continue;
		}

		if (length < NET_HEADERSIZE)
		{
			shortPacketCount++;
			continue;
		}

		received = length;
		length = BigLong(packetBuffer.length);
		flags = length & (~NETFLAG_LENGTH_MASK);
		length &= NETFLAG_LENGTH_MASK;

		if (flags & NETFLAG_CTL)
			continue;

		// QVR: the header's length is the peer's word: never less than a header (the data's length below would be
		// negative: a copy of 4 GB) nor more than what came (the last datagram's bytes read as this one's)
		if (length < NET_HEADERSIZE || length > received)
		{
			shortPacketCount++;
			continue;
		}

		sequence = BigLong(packetBuffer.sequence);
		packetsReceived++;
		sock->qvrBytesIn += length; // QVR: (B13) the net HUD's

		if (flags & NETFLAG_UNRELIABLE)
		{
			// QVR: (review finding 2) a window around the next one expected (above); sequences wrap
			int ahead = (int) (sequence - sock->unreliableReceiveSequence);
			if ((ahead >= DGRM_UNRELIABLE_AHEAD || ahead < -DGRM_UNRELIABLE_BEHIND) &&
				net_time - sock->qvrUnreliableTime <= DGRM_RESYNC_SILENCE)
			{
				outOfWindow++;
				Con_DPrintf("An unreliable datagram outside the window (%d)\n", ahead);
				continue;
			}
			if (ahead < 0 && ahead >= -DGRM_UNRELIABLE_BEHIND)
			{
				if (sock->qvrAcceptStale)
				{
					// QVR: (B20) read all the same, marked (a remote client's moves at the server: a jittery link
					// reorders them past their repeats, and each is idempotent by its number)
					sock->qvrStaleRead++;
					length -= NET_HEADERSIZE;
					SZ_Clear (&net_message);
					SZ_Write (&net_message, packetBuffer.data, length);
					net_stalemessage = true;
					ret = 2;
					break;
				}
				// QVR: (B13) skipped, the reading goes on (NetQuake stopped reading for the frame at one: under
				// reordering the acks and chunks behind it waited a frame each)
				Con_DPrintf("Got a stale datagram\n");
				continue;
			}
			if (ahead > 0 && ahead < DGRM_UNRELIABLE_AHEAD)
			{
				count = (unsigned int) ahead;
				droppedDatagrams += count;
				sock->qvrUnreliableSkipped += count; // QVR: (B13) the net HUD's loss
				Con_DPrintf("Dropped %u datagram(s)\n", count);
			}
			sock->unreliableReceiveSequence = sequence + 1;
			sock->qvrUnreliableTime = net_time; // QVR: (review finding 2)
			sock->qvrUnreliableRead++;

			length -= NET_HEADERSIZE;

			SZ_Clear (&net_message);
			SZ_Write (&net_message, packetBuffer.data, length);

			ret = 2;
			break;
		}

		if (flags & NETFLAG_ACK)
		{
			// QVR: (B13) a chunk in flight not acked yet: the receiver has it (selective acks, above)
			slot = sequence % DGRM_RELIABLE_WINDOW;
			if ((int) (sequence - sock->ackSequence) < 0 || (int) (sequence - sock->sendSequence) >= 0 ||
				sock->chunkAcked[slot])
			{
				Con_DPrintf("Stale ACK received\n");
				continue;
			}
			sock->chunkAcked[slot] = true;
			sock->qvrHeard = true; // QVR: (mpfix A1) (a chunk's random sequence acked: the address takes replies)
			if (!sock->chunkResent[slot]) // QVR: (B12b) its round trip (Karn's: a first send's)
			{
				double rtt = net_time - sock->chunkSentTime[slot];
				sock->reliableRtt = sock->reliableRtt > 0 ? 0.875 * sock->reliableRtt + 0.125 * rtt : rtt;
			}
			if (sequence != sock->ackSequence)
			{
				// QVR: (B12b) a later chunk's: one before it was lost (or is late): enough of them, the ones before it
				// not acked again at once (once until the next in order)
				if (++sock->aheadAcks >= DGRM_FAST_ACKS && !sock->fastResent)
				{
					ReSendMessage (sock, sequence);
					sock->fastResent = true;
					packetsFastReSent++;
				}
				continue;
			}
			while (sock->ackSequence != sock->sendSequence && sock->chunkAcked[sock->ackSequence % DGRM_RELIABLE_WINDOW])
			{
				sock->chunkAcked[sock->ackSequence % DGRM_RELIABLE_WINDOW] = false;
				sock->ackSequence++;
			}
			sock->aheadAcks = 0;
			sock->resendBackoff = 0;
			sock->fastResent = false;
			if (sock->ackSequence - sock->sendChunkBase >= ReliableChunks (sock))
			{
				sock->sendMessageLength = 0;
				sock->canSend = true;
				sock->sendNext = false;
			}
			else
				sock->sendNext = true; // the window's room: the next chunks (after this read)
			continue;
		}

		if (flags & NETFLAG_DATA)
		{
			length -= NET_HEADERSIZE;

			if ((int) (sequence - sock->receiveSequence) < 0)
			{
				SendAck (sock, sequence, &readaddr); // (taken: its ack was lost, or is late)
				receivedDuplicateCount++;
				continue;
			}
			if (sequence != sock->receiveSequence)
			{
				// QVR: (B13) a chunk before its turn: held, and acked only then (selective acks, above); a peer that is
				// not Quake VR: NetQuake's (acked, dropped)
				slot = sequence % DGRM_RELIABLE_WINDOW;
				if (!sock->qvrNetVersion)
					SendAck (sock, sequence, &readaddr);
				else if (sequence - sock->receiveSequence < DGRM_RELIABLE_WINDOW && length > 0 && length <= DGRM_RELIABLE_CHUNK)
				{
					if (sock->heldLength[slot] <= 0)
					{
						Q_memcpy (sock->held[slot], packetBuffer.data, length);
						sock->heldLength[slot] = (int) length;
						sock->heldEom[slot] = (flags & NETFLAG_EOM) != 0;
					}
					SendAck (sock, sequence, &readaddr);
				}
				receivedDuplicateCount++;
				continue;
			}

			SendAck (sock, sequence, &readaddr);
			ret = TakeChunk (sock, packetBuffer.data, length, (flags & NETFLAG_EOM) != 0);
			if (!ret)
				ret = TakeHeldChunks (sock);
			if (ret)
				break;
			continue;
		}
	}

	if (sock->sendNext)
		SendMessageNext (sock);

	return ret;
}


static void PrintStats(qsocket_t *s)
{
	Con_Printf("canSend = %4u   \n", s->canSend);
	Con_Printf("sendSeq = %4u   ", s->sendSequence);
	Con_Printf("recvSeq = %4u   \n", s->receiveSequence);
	Con_Printf("\n");
}

static void NET_Stats_f (void)
{
	qsocket_t	*s;

	if (Cmd_Argc () == 1)
	{
		Con_Printf("unreliable messages sent   = %i\n", unreliableMessagesSent);
		Con_Printf("unreliable messages recv   = %i\n", unreliableMessagesReceived);
		Con_Printf("reliable messages sent     = %i\n", messagesSent);
		Con_Printf("reliable messages received = %i\n", messagesReceived);
		Con_Printf("packetsSent                = %i\n", packetsSent);
		Con_Printf("packetsReSent              = %i\n", packetsReSent);
		Con_Printf("windows resent: timeout    = %i\n", packetsTimedReSent); // QVR (B12b)
		Con_Printf("windows resent: fast       = %i\n", packetsFastReSent);
		Con_Printf("packetsReceived            = %i\n", packetsReceived);
		Con_Printf("receivedDuplicateCount     = %i\n", receivedDuplicateCount);
		Con_Printf("shortPacketCount           = %i\n", shortPacketCount);
		Con_Printf("droppedDatagrams           = %i\n", droppedDatagrams);
		Con_Printf("server queue drops         = %i\n", sharedQueueDrops); // QVR (B23)
		Con_Printf("out of window              = %i\n", outOfWindow); // QVR (review 2026-11-18, finding 2)
	}
	else if (Q_strcmp(Cmd_Argv(1), "*") == 0)
	{
		for (s = net_activeSockets; s; s = s->next)
			PrintStats(s);
		for (s = net_freeSockets; s; s = s->next)
			PrintStats(s);
	}
	else
	{
		for (s = net_activeSockets; s; s = s->next)
		{
			if (q_strcasecmp(Cmd_Argv(1), s->address) == 0)
				break;
		}

		if (s == NULL)
		{
			for (s = net_freeSockets; s; s = s->next)
			{
				if (q_strcasecmp(Cmd_Argv(1), s->address) == 0)
					break;
			}
		}

		if (s == NULL)
			return;

		PrintStats(s);
	}
}


// recognize ip:port (based on ProQuake)
// QVR: (review A8) the port goes to *port (0: none given) for that one address (GetAddrWithPort), no longer into
// net_hostport for the rest of the session (a favourite without a port was then asked at the last one joined)
static char	strip_port_noport[MAX_QPATH]; /* array size as in Host_Connect_f() */
static const char *Strip_Port (const char *host, int *port)
{
	char		*p;
	int		given;

	*port = 0;
	if (!host || !*host)
		return host;
	q_strlcpy (strip_port_noport, host, sizeof(strip_port_noport));
	if ((p = Q_strrchr(strip_port_noport, ':')) == NULL)
		return host;
	*p++ = '\0';
	given = Q_atoi (p);
	if (given > 0 && given < 65536)
		*port = given;
	return strip_port_noport;
}

// QVR: (review A8) the address of `host` (no port in it), at `port` when one was given (Strip_Port), else the default
static int GetAddrWithPort (const char *host, int port, struct qsockaddr *addr)
{
	if (dfunc.GetAddrFromName (host, addr) == -1)
		return -1;
	if (port > 0)
		dfunc.SetSocketPort (addr, port);
	return 0;
}


static qboolean testInProgress = false;
static int		testPollCount;
static int		testDriver;
static sys_socket_t	testSocket;

static void Test_Poll (void *);
static PollProcedure	testPollProcedure = {NULL, 0.0, Test_Poll};

static void Test_Poll (void *unused)
{
	struct qsockaddr clientaddr;
	int		control;
	int		len;
	char	name[32];
	char	address[64];
	int		colors;
	int		frags;
	int		connectTime;

	net_landriverlevel = testDriver;

	while (1)
	{
		len = dfunc.Read (testSocket, net_message.data, net_message.maxsize, &clientaddr);
		if (len < (int) sizeof(int))
			break;

		net_message.cursize = len;

		MSG_BeginReading ();
		control = BigLong(*((int *)net_message.data));
		MSG_ReadLong();
		if (control == -1)
			break;
		if ((control & (~NETFLAG_LENGTH_MASK)) != (int)NETFLAG_CTL)
			break;
		if ((control & NETFLAG_LENGTH_MASK) != len)
			break;

		if (MSG_ReadByte() != CCREP_PLAYER_INFO)
			Sys_Error("Unexpected response to Player Info request\n");

		MSG_ReadByte(); /* playerNumber */
		Q_strcpy(name, MSG_ReadString());
		colors = MSG_ReadLong();
		frags = MSG_ReadLong();
		connectTime = MSG_ReadLong();
		Q_strcpy(address, MSG_ReadString());

		Con_Printf("%s\n  frags:%3i  colors:%d %d  time:%d\n  %s\n", name, frags, colors >> 4, colors & 0x0f, connectTime / 60, address);
	}

	testPollCount--;
	if (testPollCount)
	{
		SchedulePollProcedure(&testPollProcedure, 0.1);
	}
	else
	{
		dfunc.Close_Socket(testSocket);
		testInProgress = false;
	}
}

static void Test_f (void)
{
	const char	*host;
	int		n;
	int		maxusers = MAX_SCOREBOARD;
	int		port;
	struct qsockaddr sendaddr;

	if (testInProgress)
		return;

	host = Strip_Port (Cmd_Argv(1), &port);

	if (host && hostCacheCount)
	{
		for (n = 0; n < hostCacheCount; n++)
		{
			if (q_strcasecmp (host, hostcache[n].name) == 0)
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				maxusers = hostcache[n].maxusers;
				Q_memcpy(&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		}

		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (GetAddrWithPort(host, port, &sendaddr) != -1)
			break;
	}

	if (net_landriverlevel == net_numlandrivers)
	{
		Con_Printf("Could not resolve %s\n", host);
		return;
	}

JustDoIt:
	testSocket = dfunc.Open_Socket(0);
	if (testSocket == INVALID_SOCKET)
		return;

	testInProgress = true;
	testPollCount = 20;
	testDriver = net_landriverlevel;

	for (n = 0; n < maxusers; n++)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_PLAYER_INFO);
		MSG_WriteByte(&net_message, n);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (testSocket, net_message.data, net_message.cursize, &sendaddr);
	}
	SZ_Clear(&net_message);
	SchedulePollProcedure(&testPollProcedure, 0.1);
}


static qboolean test2InProgress = false;
static int		test2Driver;
static sys_socket_t	test2Socket;

static void Test2_Poll (void *);
static PollProcedure	test2PollProcedure = {NULL, 0.0, Test2_Poll};

static void Test2_Poll (void *unused)
{
	struct qsockaddr clientaddr;
	int		control;
	int		len;
	char	name[256];
	char	value[256];

	net_landriverlevel = test2Driver;
	name[0] = 0;

	len = dfunc.Read (test2Socket, net_message.data, net_message.maxsize, &clientaddr);
	if (len < (int) sizeof(int))
		goto Reschedule;

	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong();
	if (control == -1)
		goto Error;
	if ((control & (~NETFLAG_LENGTH_MASK)) != (int)NETFLAG_CTL)
		goto Error;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		goto Error;

	if (MSG_ReadByte() != CCREP_RULE_INFO)
		goto Error;

	Q_strcpy(name, MSG_ReadString());
	if (name[0] == 0)
		goto Done;
	Q_strcpy(value, MSG_ReadString());

	Con_Printf("%-16.16s  %-16.16s\n", name, value);

	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREQ_RULE_INFO);
	MSG_WriteString(&net_message, name);
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (test2Socket, net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear(&net_message);

Reschedule:
	SchedulePollProcedure(&test2PollProcedure, 0.05);
	return;

Error:
	Con_Printf("Unexpected response to Rule Info request\n");
Done:
	dfunc.Close_Socket(test2Socket);
	test2InProgress = false;
	return;
}

static void Test2_f (void)
{
	const char	*host;
	int		n;
	int		port;
	struct qsockaddr sendaddr;

	if (test2InProgress)
		return;

	host = Strip_Port (Cmd_Argv(1), &port);

	if (host && hostCacheCount)
	{
		for (n = 0; n < hostCacheCount; n++)
		{
			if (q_strcasecmp (host, hostcache[n].name) == 0)
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				Q_memcpy(&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		}

		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (GetAddrWithPort(host, port, &sendaddr) != -1)
			break;
	}

	if (net_landriverlevel == net_numlandrivers)
	{
		Con_Printf("Could not resolve %s\n", host);
		return;
	}

JustDoIt:
	test2Socket = dfunc.Open_Socket(0);
	if (test2Socket == INVALID_SOCKET)
		return;

	test2InProgress = true;
	test2Driver = net_landriverlevel;

	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREQ_RULE_INFO);
	MSG_WriteString(&net_message, "");
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (test2Socket, net_message.data, net_message.cursize, &sendaddr);
	SZ_Clear(&net_message);
	SchedulePollProcedure(&test2PollProcedure, 0.05);
}


static void CtlBudget_f (void); // QVR: (review A10)

int Datagram_Init (void)
{
	int	i, num_inited;
	sys_socket_t	csock;

#ifdef BAN_TEST
	banAddr.s_addr = INADDR_ANY;
	banMask.s_addr = INADDR_NONE;
#endif
	myDriverLevel = net_driverlevel;

	Cmd_AddCommand ("net_stats", NET_Stats_f);
	Cmd_AddCommand ("vr_mp_roomcode", NET_RoomCode_f); // QVR: (B23) this host's room code ("vr_mp_roomcode copy": copied)
	VR_NetLookupInit (); // QVR: (review finding 6) net_lookups

	if (safemode || COM_CheckParm("-nolan"))
		return -1;

	NatShim (); // QVR: (B23, tests)
	num_inited = 0;
	for (i = 0; i < net_numlandrivers; i++)
	{
		csock = net_landrivers[i].Init ();
		if (csock == INVALID_SOCKET)
			continue;
		net_landrivers[i].initialized = true;
		net_landrivers[i].controlSock = csock;
		num_inited++;
	}

	if (num_inited == 0)
		return -1;

#ifdef BAN_TEST
	Cmd_AddCommand ("ban", NET_Ban_f);
#endif
	Cmd_AddCommand ("test", Test_f);
	Cmd_AddCommand ("test2", Test2_f);
	Cmd_AddCommand ("vr_net_ctlbudget", CtlBudget_f); // QVR: (review A10)

	return 0;
}


void Datagram_Shutdown (void)
{
	int i;

	VR_NetLookupShutdown (); // QVR: (review finding 6) a lookup under way waited for

//
// shutdown the lan drivers
//
	for (i = 0; i < net_numlandrivers; i++)
	{
		if (net_landrivers[i].initialized)
		{
			net_landrivers[i].Shutdown ();
			net_landrivers[i].initialized = false;
		}
	}
}


void Datagram_Close (qsocket_t *sock)
{
	if (sock->qvrShared) // QVR: (B23) the server's listening socket: not his to close
		QueueClear (&sock->qvrIn);
	else
		sfunc.Close_Socket(sock->socket);
}


void Datagram_Listen (qboolean state)
{
	int i;

	// QVR: (the port check) a new hosting checked again (his router's forwards may have changed since)
	if (state && !room.listening)
	{
		room.probePort = 0;
		room.probeSent = 0;
		if (room.probeState != NET_PORTCHECK_OFF)
			room.probeState = NET_PORTCHECK_WAITING;
	}
	room.listening = state;

	for (i = 0; i < net_numlandrivers; i++)
	{
		if (net_landrivers[i].initialized)
			net_landrivers[i].Listen (state);
	}
}


// QVR: (B15) seconds without a message from a connection after which a connect request of the same name from its
// address on another port is that player back from a crash (his game sends moves every frame while it runs)
#define QVR_GHOST_SILENCE 3.0

// QVR: (B25) vr_mp_clientkey made up when empty (16 hex digits from the clock and this process; kept in the config, so
// the same game after a crash and a restart sends the same)
static void ClientKeyMade (void)
{
	char		key[24];
	unsigned	a, b;

	if (Cvar_VariableString ("vr_mp_clientkey")[0])
		return;
	a = DgrmRandom (); // (mpfix A12: the system's random source; the clock, rand() and stack addresses before)
	b = DgrmRandom ();
	q_snprintf (key, sizeof (key), "%08x%08x", a, b);
	Cvar_Set ("vr_mp_clientkey", key);
}

// QVR: a connection's CCREP_ACCEPT (its first, and again for a duplicate request): the listening socket's port (a
// NetQuake client switches to it), (B13) this server's network version (selective acks) and (review finding 2) the
// first sequence of the server's channels to a client that sent his
static void SendAccept (sys_socket_t acceptsock, const struct qsockaddr *to, const qsocket_t *sock)
{
	struct qsockaddr	self;

	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREP_ACCEPT);
	dfunc.GetSocketAddr(acceptsock, &self);
	MSG_WriteLong(&net_message, dfunc.GetSocketPort(&self));
	MSG_WriteLong(&net_message, QVR_NET_VERSION);
	if (sock->qvrSeeded)
		MSG_WriteLong(&net_message, (int) sock->qvrFirstOut);
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (acceptsock, net_message.data, net_message.cursize, (struct qsockaddr *) to);
	SZ_Clear(&net_message);
}

// QVR: (mpfix A1) NetQuake gave a slot (and queued the serverinfo, resent for 300 s) to any CCREQ_CONNECT: 16 forged
// ones filled a server, and one with a spoofed source made it send a victim the serverinfo window again and again. A
// request now gets a slot only with a cookie this server made for its address and port (CCREP_CHALLENGE: smaller than
// the request it answers), a keyed hash (SipHash-2-4, a key drawn once a run) of the address and the time in
// QVR_COOKIE_PERIOD steps (this step's or the last's taken). Stateless: a forged request costs one small reply.
#define QVR_COOKIE_PERIOD	10.0
#define SIP_ROTL(x, b)	(((x) << (b)) | ((x) >> (64 - (b))))
#define SIP_ROUND	do { v0 += v1; v1 = SIP_ROTL (v1, 13); v1 ^= v0; v0 = SIP_ROTL (v0, 32); v2 += v3; v3 = SIP_ROTL (v3, 16); \
	v3 ^= v2; v0 += v3; v3 = SIP_ROTL (v3, 21); v3 ^= v0; v2 += v1; v1 = SIP_ROTL (v1, 17); v1 ^= v2; \
	v2 = SIP_ROTL (v2, 32); } while (0)
static unsigned long long	cookieKey[2];
static qboolean	cookieKeyMade;

static unsigned long long SipHash24 (const unsigned long long k[2], const byte *in, size_t len)
{
	unsigned long long	v0 = 0x736f6d6570736575ull ^ k[0], v1 = 0x646f72616e646f6dull ^ k[1];
	unsigned long long	v2 = 0x6c7967656e657261ull ^ k[0], v3 = 0x7465646279746573ull ^ k[1];
	unsigned long long	m, last = (unsigned long long) len << 56;
	size_t	i, j;

	for (i = 0; i + 8 <= len; i += 8)
	{
		for (m = 0, j = 0; j < 8; j++)
			m |= (unsigned long long) in[i + j] << (8 * j);
		v3 ^= m; SIP_ROUND; SIP_ROUND; v0 ^= m;
	}
	for (j = 0; i + j < len; j++)
		last |= (unsigned long long) in[i + j] << (8 * j);
	v3 ^= last; SIP_ROUND; SIP_ROUND; v0 ^= last;
	v2 ^= 0xff; SIP_ROUND; SIP_ROUND; SIP_ROUND; SIP_ROUND;
	return v0 ^ v1 ^ v2 ^ v3;
}

// the cookie for `addr` (its address and port) in time step `step` (never 0)
static unsigned int ConnectCookie (const struct qsockaddr *addr, int step)
{
	char	text[NET_NAMELEN + 16];
	int		len;

	if (!cookieKeyMade)
	{
		cookieKey[0] = ((unsigned long long) DgrmRandom () << 32) | DgrmRandom ();
		cookieKey[1] = ((unsigned long long) DgrmRandom () << 32) | DgrmRandom ();
		cookieKeyMade = true;
	}
	len = q_snprintf (text, sizeof (text), "%s|%d", dfunc.AddrToString ((struct qsockaddr *) addr), step);
	len = q_min (len, (int) sizeof (text) - 1);
	return (unsigned int) SipHash24 (cookieKey, (const byte *) text, (size_t) q_max (len, 0)) | 1u;
}

// whether `cookie` is one ConnectCookie made for `addr` this step or the last
static qboolean CookieValid (const struct qsockaddr *addr, unsigned int cookie)
{
	const int	step = (int) (net_time / QVR_COOKIE_PERIOD);

	return cookie == ConnectCookie (addr, step) || cookie == ConnectCookie (addr, step - 1);
}

// QVR: (review A10) the replies to connectionless requests (server info, players, rules, rejects) are 5-10x their
// requests and go to whatever address a request names: a reflector. Each source IP address (any port) has a byte budget
// for them, CTL_REPLY_BURST, refilled at CTL_REPLY_RATE a second (a browser's refresh is a few hundred bytes); this
// machine's own (127.0.0.1: the tests, `test`) has none. A table of slots by a keyed hash of the address (ConnectCookie's
// key: no one can aim two addresses at one slot); a slot taken by another address starts full.
#define CTL_REPLY_SLOTS	1024
#define CTL_REPLY_BURST	8192.f
#define CTL_REPLY_RATE	4096.f
typedef struct
{
	unsigned int	tag; // the address's hash (0: never used)
	float			bytes;
	double			time;
} ctlbudget_t;
static ctlbudget_t	ctl_budget[CTL_REPLY_SLOTS];
static int			ctl_budget_refused; // replies not sent (vr_net_ctlbudget)

static qboolean ControlReplyAllowed (const struct qsockaddr *to, int bytes)
{
	struct qsockaddr	ip = *to;
	unsigned int		h;
	ctlbudget_t			*b;

	if (to->qsa_family == AF_INET && ((const struct sockaddr_in *) to)->sin_addr.s_addr == htonl (INADDR_LOOPBACK))
		return true;
	dfunc.SetSocketPort (&ip, 0);
	h = ConnectCookie (&ip, 0); // (never 0)
	b = &ctl_budget[h % CTL_REPLY_SLOTS];
	if (b->tag != h)
	{
		b->tag = h;
		b->bytes = CTL_REPLY_BURST;
	}
	else
		b->bytes = q_min (CTL_REPLY_BURST, b->bytes + (float) (net_time - b->time) * CTL_REPLY_RATE);
	b->time = net_time;
	if (b->bytes < (float) bytes)
	{
		ctl_budget_refused++;
		return false;
	}
	b->bytes -= (float) bytes;
	return true;
}

// the control reply in net_message to `to`, within its address's budget (ControlReplyAllowed)
static void ControlReply (sys_socket_t acceptsock, struct qsockaddr *to)
{
	if (ControlReplyAllowed (to, net_message.cursize))
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, to);
}

// vr_net_ctlbudget [selftest]: the replies refused so far; selftest: a source's budget spent, refused, refilled
static void CtlBudget_f (void)
{
	struct qsockaddr	a, b2;
	const double		was = net_time;
	const int			refusedWas = ctl_budget_refused;
	int					passed = 0, failed = 0, n = 0;
	qboolean			ok;

	Con_Printf ("vr_net_ctlbudget: %d control replies refused (budget %.0f bytes, %.0f a second, per address)\n",
		ctl_budget_refused, CTL_REPLY_BURST, CTL_REPLY_RATE);
	if (Cmd_Argc () < 2 || Q_strcmp (Cmd_Argv (1), "selftest") != 0)
		return;
#define CTL_CHECK(cond, what) do { ok = (cond); if (ok) passed++; else failed++; Con_Printf ("  %s: %s\n", ok ? "PASS" : "FAIL", what); } while (0)
	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
		if (net_landrivers[net_landriverlevel].initialized)
			break; // (the UDP driver: dfunc's SetSocketPort, AddrToString)
	if (net_landriverlevel == net_numlandrivers)
	{
		Con_Printf ("vr_net_ctlbudget_selftest: 0 passed, 1 failed (no network driver)\n");
		return;
	}
	memset (&a, 0, sizeof (a));
	a.qsa_family = AF_INET;
	((struct sockaddr_in *) &a)->sin_addr.s_addr = htonl (0xCB007105); // 203.0.113.5 (documentation range)
	b2 = a;
	dfunc.SetSocketPort (&a, 26000);
	dfunc.SetSocketPort (&b2, 26001);
	net_time = 1000000.0;
	memset (ctl_budget, 0, sizeof (ctl_budget));
	while (n < 1000 && ControlReplyAllowed (n & 1 ? &b2 : &a, 100))
		n++;
	CTL_CHECK (n == (int) (CTL_REPLY_BURST / 100), va ("a source's 100-byte replies until its budget is spent: %d (any port)", n));
	net_time += 0.5;
	CTL_CHECK (ControlReplyAllowed (&a, 2000) && !ControlReplyAllowed (&a, 200), "half a second later: half the rate back");
	net_time += 10.0;
	CTL_CHECK (ControlReplyAllowed (&a, (int) CTL_REPLY_BURST) && !ControlReplyAllowed (&a, 1), "later: the full budget, no more");
	((struct sockaddr_in *) &b2)->sin_addr.s_addr = htonl (0xCB007106);
	CTL_CHECK (ControlReplyAllowed (&b2, 4000), "another address: its own budget");
	((struct sockaddr_in *) &b2)->sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	for (n = 0; n < 100 && ControlReplyAllowed (&b2, 1000); n++)
		;
	CTL_CHECK (n == 100, "this machine (127.0.0.1): no budget");
#undef CTL_CHECK
	memset (ctl_budget, 0, sizeof (ctl_budget));
	net_time = was;
	ctl_budget_refused = refusedWas;
	Con_Printf ("vr_net_ctlbudget_selftest: %d passed, %d failed\n", passed, failed);
}

static void SendChallenge (sys_socket_t acceptsock, const struct qsockaddr *to)
{
	SZ_Clear(&net_message);
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREP_CHALLENGE);
	MSG_WriteLong(&net_message, (int) ConnectCookie (to, (int) (net_time / QVR_COOKIE_PERIOD)));
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (acceptsock, net_message.data, net_message.cursize, (struct qsockaddr *) to);
	SZ_Clear(&net_message);
}

// QVR: (B23) a datagram of the listening socket's that isn't a client's (in net_message: its length `len`, from `from`)
static qsocket_t *_Datagram_ControlRequest (sys_socket_t acceptsock, const struct qsockaddr *from, int len)
{
	struct qsockaddr clientaddr = *from;
	struct qsockaddr newaddr;
	qsocket_t	*sock;
	qsocket_t	*s;
	int			command;
	int			control;
	int			ret;
	int			qvrVersion = 0; // QVR
	char		qvrName[32] = {0}; // QVR: the player's name (a loaded coop game's slot)
	char		qvrKey[24] = {0}; // QVR: (B25) his game's vr_mp_clientkey ("": an older client's request)
	unsigned int	qvrFirstIn = 0; // QVR: (review finding 2) his channels' first sequence ...
	qboolean	qvrSeeded = false; // QVR: ... when he sent it (network version 39)
	unsigned int	qvrCookie = 0; // QVR: (mpfix A1) the cookie this server gave his address (0: none sent)

	if (len < (int) sizeof(int))
		return NULL;
	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong();
	if (control == -1)
		return NULL;
	if ((control & (~NETFLAG_LENGTH_MASK)) != (int)NETFLAG_CTL)
		return NULL;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		return NULL;

	command = MSG_ReadByte();
	if (command == CCREQ_SERVER_INFO)
	{
		if (Q_strcmp(MSG_ReadString(), "QUAKE") != 0)
			return NULL;

		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_SERVER_INFO);
		dfunc.GetSocketAddr(acceptsock, &newaddr);
		MSG_WriteString(&net_message, dfunc.AddrToString(&newaddr));
		MSG_WriteString(&net_message, hostname.string);
		MSG_WriteString(&net_message, sv.name);
		MSG_WriteByte(&net_message, net_activeconnections);
		MSG_WriteByte(&net_message, svs.maxclients);
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		// QVR: (netmismatch) after NetQuake's fields (a client before it reads none of it): "QVR", the Quake VR network
		// version this server takes and its release, the browser's "needs Quake VR 1.2.0-beta7". Only from a server
		// that takes that version only (CCREQ_CONNECT's check below)
		if (sv.active && (sv.protocolflags & PRFL_QUAKEVR))
		{
			MSG_WriteString(&net_message, "QVR");
			MSG_WriteLong(&net_message, QVR_NET_VERSION);
			MSG_WriteString(&net_message, NET_QvrRelease ());
		}
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		ControlReply (acceptsock, &clientaddr); // QVR: (review A10) within its address's budget
		SZ_Clear(&net_message);
		return NULL;
	}

	if (command == CCREQ_PLAYER_INFO)
	{
		int			playerNumber;
		int			activeNumber;
		int			clientNumber;
		client_t	*client;

		playerNumber = MSG_ReadByte();
		activeNumber = -1;

		for (clientNumber = 0, client = svs.clients; clientNumber < svs.maxclients; clientNumber++, client++)
		{
			if (client->active)
			{
				activeNumber++;
				if (activeNumber == playerNumber)
					break;
			}
		}

		if (clientNumber == svs.maxclients)
			return NULL;

		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_PLAYER_INFO);
		MSG_WriteByte(&net_message, playerNumber);
		MSG_WriteString(&net_message, client->name);
		MSG_WriteLong(&net_message, client->colors);
		MSG_WriteLong(&net_message, (int)client->edict->v.frags);
		MSG_WriteLong(&net_message, (int)(net_time - client->netconnection->connecttime));
		// QVR: (review 2026-11-18, finding 2) his port only to an asker on this machine (127.0.0.1: `test`): a
		// stranger had every player's address and port, all a forged request in his name needs
		{
			char	address[NET_NAMELEN];
			char	*colon;
			q_strlcpy(address, client->netconnection->address, sizeof(address));
			if ((clientaddr.qsa_family != AF_INET ||
				((struct sockaddr_in *) &clientaddr)->sin_addr.s_addr != htonl (INADDR_LOOPBACK)) &&
				(colon = strrchr(address, ':')) != NULL)
				*colon = 0;
			MSG_WriteString(&net_message, address);
		}
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		ControlReply (acceptsock, &clientaddr); // QVR: (review A10) within its address's budget
		SZ_Clear(&net_message);

		return NULL;
	}

	if (command == CCREQ_RULE_INFO)
	{
		const char	*prevCvarName;
		cvar_t			*var;

		// find the search start location
		prevCvarName = MSG_ReadString();
		var = Cvar_FindVarAfter (prevCvarName, CVAR_SERVERINFO);

		// send the response
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_RULE_INFO);
		if (var)
		{
			MSG_WriteString(&net_message, var->name);
			MSG_WriteString(&net_message, var->string);
		}
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		ControlReply (acceptsock, &clientaddr); // QVR: (review A10) within its address's budget
		SZ_Clear(&net_message);

		return NULL;
	}

	if (command != CCREQ_CONNECT)
		return NULL;

	if (Q_strcmp(MSG_ReadString(), "QUAKE") != 0)
		return NULL;

	if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_REJECT);
		MSG_WriteString(&net_message, "Incompatible version.\n");
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		ControlReply (acceptsock, &clientaddr); // QVR: (review A10) within its address's budget
		SZ_Clear(&net_message);
		return NULL;
	}

	// QVR: Quake VR's network version ("QVR" and a long after NetQuake's; vr/vr_api.h): a VR server takes the same only
	{
		char reason[320];
		if (msg_readcount < net_message.cursize && Q_strcmp(MSG_ReadString(), "QVR") == 0)
		{
			qvrVersion = MSG_ReadLong();
			if (msg_readcount < net_message.cursize)
				q_strlcpy(qvrName, MSG_ReadString(), sizeof(qvrName));
			if (msg_readcount < net_message.cursize)
				q_strlcpy(qvrKey, MSG_ReadString(), sizeof(qvrKey));
			if (msg_readcount + 4 <= net_message.cursize)
			{
				qvrFirstIn = (unsigned int) MSG_ReadLong();
				qvrSeeded = true;
			}
			if (msg_readcount + 4 <= net_message.cursize) // QVR: (mpfix A1, network version 40)
				qvrCookie = (unsigned int) MSG_ReadLong();
		}
		reason[0] = 0;
		if (sv.active && (sv.protocolflags & PRFL_QUAKEVR) && qvrVersion != QVR_NET_VERSION)
		{
			// (netmismatch) its release said, what to install: the reason a client shows as it is (a client before it
			// too: its console, its menu's first 31 characters)
			const char *release = NET_QvrRelease ();
			if (qvrVersion == 0)
				q_snprintf(reason, sizeof(reason), "This server runs Quake VR %s (network version %d): join with Quake VR "
					"%s.\n", release, QVR_NET_VERSION, release);
			else if (qvrVersion < QVR_NET_VERSION)
				q_snprintf(reason, sizeof(reason), "This server runs Quake VR %s (network version %d), newer than your game "
					"(network version %d): update to Quake VR %s to join.\n", release, QVR_NET_VERSION, qvrVersion, release);
			else
				q_snprintf(reason, sizeof(reason), "This server runs Quake VR %s (network version %d), older than your game "
					"(network version %d): join with Quake VR %s, or its host updates.\n", release, QVR_NET_VERSION,
					qvrVersion, release);
		}
		if (reason[0])
		{
			Con_Printf("Rejected a connection from %s: %s", dfunc.AddrToString(&clientaddr), reason);
			SZ_Clear(&net_message);
			// save space for the header, filled in later
			MSG_WriteLong(&net_message, 0);
			MSG_WriteByte(&net_message, CCREP_REJECT);
			MSG_WriteString(&net_message, reason);
			*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
			ControlReply (acceptsock, &clientaddr); // QVR: (review A10) within its address's budget
			SZ_Clear(&net_message);
			return NULL;
		}
	}

#ifdef BAN_TEST
	// check for a ban
	if (clientaddr.qsa_family == AF_INET)
	{
		in_addr_t	testAddr;
		testAddr = ((struct sockaddr_in *)&clientaddr)->sin_addr.s_addr;
		if ((testAddr & banMask.s_addr) == banAddr.s_addr)
		{
			SZ_Clear(&net_message);
			// save space for the header, filled in later
			MSG_WriteLong(&net_message, 0);
			MSG_WriteByte(&net_message, CCREP_REJECT);
			MSG_WriteString(&net_message, "You have been banned.\n");
			*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
			ControlReply (acceptsock, &clientaddr); // QVR: (review A10) within its address's budget
			SZ_Clear(&net_message);
			return NULL;
		}
	}
#endif

	// QVR: (mpfix A1) no slot, nor a closed ghost, nor an accept again, but for a request with his address's cookie
	if (!CookieValid (&clientaddr, qvrCookie))
	{
		SendChallenge (acceptsock, &clientaddr);
		return NULL;
	}

	// see if this guy is already connected
	for (s = net_activeSockets; s; s = s->next)
	{
		if (s->driver != net_driverlevel)
			continue;
		ret = dfunc.AddrCompare(&clientaddr, &s->addr);
		// QVR: the same address and port only (ret 0). NetQuake took any client from the same IP (ret 1: another
		// port) as this one back from a crash and closed this one: a second player behind the same router (or a
		// second game on this machine, the multiplayer harness) kicked the first (docs/vr-port/MP_PLAN.md). One back
		// from a crash on a new port leaves his old slot to time out instead -- unless (B15) the request names the same
		// player (his connect request's name) and his old connection has been silent a while (a game in play sends 72
		// moves a second): that is him back, and his old connection is closed as below (his things kept for him by
		// name: vr/vr_coopsave.cpp).
		// (B25) and the same game: his request's key (vr_mp_clientkey) his old connection's. By the name alone, a second
		// game on that address by the same name (the default "player": two players behind one router, the harness's
		// games) closed a live player's connection that a hitch had left silent over 3 s (a busy machine: the suite's
		// joins under load) -- a real player kicked, the newcomer's connection request unanswered.
		if (ret == 1 && qvrName[0] && !Q_strcmp (qvrName, s->qvrName) && qvrKey[0] && !Q_strcmp (qvrKey, s->qvrKey) &&
			net_time - s->lastMessageTime > QVR_GHOST_SILENCE)
		{
			if (!s->qvrClosing)
				Con_Printf ("%s back from %s (his old connection silent %.1f s): the old one closed\n", qvrName,
					dfunc.AddrToString (&clientaddr), net_time - s->lastMessageTime);
			// QVR: (mpfix A2) marked, not freed: a freed qsocket went to the head of the free list while his old slot
			// still held it, and the next request in this call's queue was given the same one (two slots on one
			// qsocket). Its slot drops it at its next read (NET_GetMessage: -1), and a retry then joins.
			s->qvrClosing = true;
			return NULL;
		}
		if (ret == 0)
		{
			// is this a duplicate connection reqeust?
			if (net_time - s->connecttime < 2.0)
			{
				// yes, so send a duplicate reply
				SendAccept (acceptsock, &clientaddr, s);
				return NULL;
			}
			// it's somebody coming back in from a crash/disconnect
			// so close the old qsocket and let their retry get them back in
			// QVR: (review 2026-11-18, finding 2) only once his connection has been silent QVR_GHOST_SILENCE, as for one
			// back on another port above: a UDP source is anyone's to write, so one forged request in a live player's
			// name closed his connection. A game back on the same port retries for 7.5 s: his old connection is closed at
			// a retry once silent, and the next retry joins.
			if (net_time - s->lastMessageTime > QVR_GHOST_SILENCE)
			{
				if (!s->qvrClosing)
					Con_Printf ("%s: a new connection request from a connection silent %.1f s: the old one closed\n",
						dfunc.AddrToString (&clientaddr), net_time - s->lastMessageTime);
				s->qvrClosing = true; // QVR: (mpfix A2) (as above)
			}
			return NULL;
		}
	}

	// allocate a QSocket
	sock = NET_NewQSocket ();
	if (sock == NULL)	// no room; try to let him know
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_REJECT);
		MSG_WriteString(&net_message, "Server is full.\n");
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		ControlReply (acceptsock, &clientaddr); // QVR: (review A10) within its address's budget
		SZ_Clear(&net_message);
		return NULL;
	}

	// QVR: (B23) no socket of his own: the listening one (net_dgrm.c's ReadShared). The accept still names its port (a
	// NetQuake client switches to it); a Quake VR client of network version 36 or later keeps the address it reached.
	sock->socket = acceptsock;
	sock->qvrShared = true;
	sock->qvrHeard = false; // QVR: (mpfix A1) until he acks a chunk: a window of one, QVR_UNHEARD_TIMEOUT
	sock->landriver = net_landriverlevel;
	sock->addr = clientaddr;
	sock->qvrNetVersion = qvrVersion; // QVR
	q_strlcpy(sock->qvrName, qvrName, sizeof(sock->qvrName)); // QVR
	q_strlcpy(sock->qvrKey, qvrKey, sizeof(sock->qvrKey)); // QVR: (B25)
	Q_strcpy(sock->address, dfunc.AddrToString(&clientaddr));
	if (qvrSeeded) // QVR: (review finding 2) his channels from his first sequence, this server's from its own
	{
		sock->qvrSeeded = true;
		sock->qvrFirstOut = DgrmRandom ();
		sock->receiveSequence = sock->unreliableReceiveSequence = qvrFirstIn;
		sock->sendSequence = sock->ackSequence = sock->sendChunkBase = sock->unreliableSendSequence = sock->qvrFirstOut;
	}

	// send him back the info about the server connection he has been allocated
	SendAccept (acceptsock, &clientaddr, sock);

	return sock;
}

// QVR: (B23) the listening socket read (its clients' datagrams queued for them), the host's room kept, then its other
// datagrams answered until one is a new connection
static qsocket_t *_Datagram_CheckNewConnections (void)
{
	struct qsockaddr	from;
	qsocket_t	*sock;
	int		len;
	sys_socket_t	acceptsock = dfunc.CheckNewConnections ();

	if (acceptsock == INVALID_SOCKET)
		return NULL;
	ReadShared (net_landriverlevel, acceptsock);
	RoomHostTick (acceptsock);
	while (1)
	{
		SZ_Clear (&net_message);
		len = QueuePop (&controlQueue[net_landriverlevel], net_message.data, net_message.maxsize, &from);
		if (len <= 0)
			return NULL;
		sock = _Datagram_ControlRequest (acceptsock, &from, len);
		if (sock)
			return sock;
	}
}

qsocket_t *Datagram_CheckNewConnections (void)
{
	qsocket_t *ret = NULL;

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			if ((ret = _Datagram_CheckNewConnections ()) != NULL)
				break;
		}
	}
	return ret;
}


// QVR: (B13) the server browser's addresses (its favourites and recent servers, vr_serverbrowser.cpp), asked one by one
// besides the broadcast: their resolved addresses (a reply from one of them is listed under the address he wrote), and
// the last query's time (a reply's round trip: its ping); each LAN driver's own
typedef struct
{
	struct qsockaddr	addr[HOSTCACHESIZE];
	int		index[HOSTCACHESIZE];	// (its VR_BrowserAddress index)
	int		count;
	double	sentTime;
} slistasked_t;
static slistasked_t	slistAskedBy[MAX_NET_DRIVERS];

static void _Datagram_SearchForHosts (qboolean xmit)
{
	int		ret;
	int		n;
	int		i;
	struct qsockaddr readaddr;
	struct qsockaddr myaddr;
	struct qsockaddr fromaddr;
	int		control;
	int		asked;
	slistasked_t	*sa = &slistAskedBy[net_landriverlevel];

	dfunc.GetSocketAddr (dfunc.controlSock, &myaddr);
	if (xmit)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString(&net_message, "QUAKE");
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Broadcast(dfunc.controlSock, net_message.data, net_message.cursize);
		// QVR: (B13) the browser's addresses; (finding 6) a host name looked up on a worker, asked once it is found (a
		// search sends twice, 0.75 s apart; the browser searches again)
		sa->count = 0;
		asked = VR_BrowserAddressCount ();
		for (i = 0; i < asked && sa->count < HOSTCACHESIZE; i++)
		{
			struct qsockaddr *to = &sa->addr[sa->count];
			if (LookupAddr (VR_BrowserAddress (i), net_hostport, to, LookupMayStart ()) != 1)
				continue;
			dfunc.Write (dfunc.controlSock, net_message.data, net_message.cursize, to);
			sa->index[sa->count++] = i;
		}
		sa->sentTime = Sys_DoubleTime ();
		SZ_Clear(&net_message);
	}

	while ((ret = dfunc.Read (dfunc.controlSock, net_message.data, net_message.maxsize, &readaddr)) > 0)
	{
		if (ret < (int) sizeof(int))
			continue;
		net_message.cursize = ret;

		// don't answer our own query
		// QVR: (B13) only this game's own server's (its accept socket's port); NetQuake left out every server on the
		// same machine (any reply from its address): another game's server on this PC is listed
		if (dfunc.AddrCompare(&readaddr, &myaddr) == 0 ||
			(dfunc.AddrCompare(&readaddr, &myaddr) > 0 && sv.active && dfunc.GetSocketPort (&readaddr) == net_hostport))
			continue;
		fromaddr = readaddr;

		// is the cache full?
		if (hostCacheCount == HOSTCACHESIZE)
			continue;

		MSG_BeginReading ();
		control = BigLong(*((int *)net_message.data));
		MSG_ReadLong();
		if (control == -1)
			continue;
		if ((control & (~NETFLAG_LENGTH_MASK)) != (int)NETFLAG_CTL)
			continue;
		if ((control & NETFLAG_LENGTH_MASK) != ret)
			continue;

		if (MSG_ReadByte() != CCREP_SERVER_INFO)
			continue;

		// QVR: (B13) listed at the address the reply came from (NetQuake: the one the server wrote in it, its own idea
		// of its address: its LAN's behind a router, 0.0.0.0's stand-in on a server bound to every interface); one the
		// browser asked by its address, under the address he wrote
		MSG_ReadString();
		readaddr = fromaddr;
		for (asked = 0; asked < sa->count; asked++)
		{
			if (dfunc.AddrCompare(&fromaddr, &sa->addr[asked]) == 0)
				break;
		}
		// search the cache for this server
		for (n = 0; n < hostCacheCount; n++)
		{
			if (dfunc.AddrCompare(&readaddr, &hostcache[n].addr) == 0)
				break;
		}

		// is it already there?
		if (n < hostCacheCount)
			continue;

		// add it
		hostCacheCount++;
		q_strlcpy(hostcache[n].name, MSG_ReadString(), sizeof(hostcache[n].name)); // QVR: (B13) bounded (a remote server's)
		q_strlcpy(hostcache[n].map, MSG_ReadString(), sizeof(hostcache[n].map));
		hostcache[n].users = MSG_ReadByte();
		hostcache[n].maxusers = MSG_ReadByte();
		hostcache[n].version = MSG_ReadByte(); // QVR: (B13) kept: the browser says why it cannot be joined
		// QVR: (netmismatch) a Quake VR server's network version and release after them (none from a server before it)
		hostcache[n].qvrNetVersion = 0;
		hostcache[n].qvrRelease[0] = 0;
		if (msg_readcount < net_message.cursize && Q_strcmp(MSG_ReadString(), "QVR") == 0 &&
			msg_readcount + 4 <= net_message.cursize)
		{
			hostcache[n].qvrNetVersion = MSG_ReadLong();
			if (msg_readcount < net_message.cursize)
				q_strlcpy(hostcache[n].qvrRelease, MSG_ReadString(), sizeof(hostcache[n].qvrRelease));
		}
		hostcache[n].ping = (int) ((Sys_DoubleTime () - sa->sentTime) * 1000.0 + 0.5);
		if (hostcache[n].version != NET_PROTOCOL_VERSION)
		{
			Q_strcpy(hostcache[n].cname, hostcache[n].name);
			hostcache[n].cname[14] = 0;
			Q_strcpy(hostcache[n].name, "*");
			Q_strcat(hostcache[n].name, hostcache[n].cname);
		}
		Q_memcpy(&hostcache[n].addr, &readaddr, sizeof(struct qsockaddr));
		hostcache[n].driver = net_driverlevel;
		hostcache[n].ldriver = net_landriverlevel;
		if (asked < sa->count && *VR_BrowserAddress (sa->index[asked]))
			q_strlcpy(hostcache[n].cname, VR_BrowserAddress (sa->index[asked]), sizeof(hostcache[n].cname));
		else
			q_strlcpy(hostcache[n].cname, dfunc.AddrToString(&readaddr), sizeof(hostcache[n].cname));

		// check for a name conflict
		for (i = 0; i < hostCacheCount; i++)
		{
			if (i == n)
				continue;
			if (q_strcasecmp (hostcache[n].name, hostcache[i].name) == 0)
			{
				i = Q_strlen(hostcache[n].name);
				if (i == 0) // QVR: (mpfix A9) an empty name: name[i-1] was the byte before the entry
				{
					Q_strcpy (hostcache[n].name, "*");
					i = 1;
				}
				if (i < 15 && hostcache[n].name[i-1] > '8')
				{
					hostcache[n].name[i] = '0';
					hostcache[n].name[i+1] = 0;
				}
				else
					hostcache[n].name[i-1]++;

				i = -1;
			}
		}
	}
}

void Datagram_SearchForHosts (qboolean xmit)
{
	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (hostCacheCount == HOSTCACHESIZE)
			break;
		if (net_landrivers[net_landriverlevel].initialized)
			_Datagram_SearchForHosts (xmit);
	}
}


// QVR: a connect request (CCREQ_CONNECT; `cookie`: the server's for this address, mpfix A1, 0 none yet)
static void SendConnectRequest (sys_socket_t sock, struct qsockaddr *to, unsigned int firstOut, unsigned int cookie)
{
	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREQ_CONNECT);
	MSG_WriteString(&net_message, "QUAKE");
	MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
	// QVR: Quake VR's network version (vr/vr_api.h; vr_net_test_version N: another, the harness's reject test)
	MSG_WriteString(&net_message, "QVR");
	MSG_WriteLong(&net_message, (int) Cvar_VariableValue ("vr_net_test_version") ? (int) Cvar_VariableValue ("vr_net_test_version") : QVR_NET_VERSION);
	MSG_WriteString(&net_message, cl_name.string); // QVR: a loaded coop game gives him his saved slot by it
	MSG_WriteString(&net_message, Cvar_VariableString ("vr_mp_clientkey")); // QVR: (B25) this game's, for "back from a crash"
	MSG_WriteLong(&net_message, (int) firstOut); // QVR: (review finding 2) its channels' first sequence (39)
	if (cookie) // QVR: (mpfix A1) the server's cookie for this address (40)
		MSG_WriteLong(&net_message, (int) cookie);
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (sock, net_message.data, net_message.cursize, to);
	SZ_Clear(&net_message);
}

static qsocket_t *_Datagram_Connect (const char *host, int port)
{
	struct qsockaddr sendaddr;
	struct qsockaddr readaddr;
	qsocket_t	*sock;
	sys_socket_t		newsock;
	int			ret;
	int			reps;
	double		start_time;
	int			control;
	const char		*reason;
	const qboolean	byRoom = host[0] == '@'; // QVR: (B23) "connect @CODE": a room's host, by the room service
	const unsigned int	firstOut = DgrmRandom (); // QVR: (review finding 2) this client's channels' first sequence
	const int		tries = byRoom ? ROOM_CONNECT_TRIES : 3;
	const double	tryFor = byRoom ? ROOM_CONNECT_EVERY : 2.5;
	unsigned int	cookie = 0; // QVR: (mpfix A1) the server's for this address (CCREP_CHALLENGE; 0: none yet)
	int			challenges = 0; // QVR: ... how many came

	// see if we can resolve the host name
	if (!byRoom && GetAddrWithPort(host, port, &sendaddr) == -1)
	{
		Con_Printf("Could not resolve %s\n", host);
		return NULL;
	}

	newsock = dfunc.Open_Socket (0);
	if (newsock == INVALID_SOCKET)
		return NULL;

	sock = NET_NewQSocket ();
	if (sock == NULL)
		goto ErrorReturn2;
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;

	// QVR: (B23) the room's host asked of the service from this socket: its outside address and port for it is the one
	// the host punches towards while the connect requests below punch his router
	if (byRoom && !RoomJoin (newsock, host + 1, &sendaddr))
		goto ErrorReturn; // (RoomJoin's reason given)

	// connect to the host
	if (dfunc.Connect (newsock, &sendaddr) == -1)
		goto ErrorReturn;

	// send the connection request
	Con_Printf("trying...\n");
	SCR_UpdateScreen ();
	start_time = net_time;
	ClientKeyMade ();

	for (reps = 0; reps < tries; reps++)
	{
		SendConnectRequest (newsock, &sendaddr, firstOut, cookie);
		do
		{
			ret = dfunc.Read (newsock, net_message.data, net_message.maxsize, &readaddr);
			// if we got something, validate it
			if (ret > 0)
			{
				// is it from the right place?
				if (sfunc.AddrCompare(&readaddr, &sendaddr) != 0)
				{
					Con_Printf("wrong reply address\n");
					Con_Printf("Expected: %s | %s\n", dfunc.AddrToString (&sendaddr), StrAddr(&sendaddr));
					Con_Printf("Received: %s | %s\n", dfunc.AddrToString (&readaddr), StrAddr(&readaddr));
					SCR_UpdateScreen ();
					ret = 0;
					continue;
				}

				if (ret < (int) sizeof(int))
				{
					ret = 0;
					continue;
				}

				net_message.cursize = ret;
				MSG_BeginReading ();

				control = BigLong(*((int *)net_message.data));
				MSG_ReadLong();
				if (control == -1)
				{
					ret = 0;
					continue;
				}
				if ((control & (~NETFLAG_LENGTH_MASK)) != (int)NETFLAG_CTL)
				{
					ret = 0;
					continue;
				}
				if ((control & NETFLAG_LENGTH_MASK) != ret)
				{
					ret = 0;
					continue;
				}
				// QVR: (mpfix A1) the server's cookie for this address: the request again with it, the wait going on (a
				// few at most: a server that keeps asking is answered by "No Response")
				if (ret >= 9 && net_message.data[4] == CCREP_CHALLENGE && challenges < 8)
				{
					MSG_ReadByte();
					cookie = (unsigned int) MSG_ReadLong();
					challenges++;
					SendConnectRequest (newsock, &sendaddr, firstOut, cookie);
					ret = 0;
					continue;
				}
			}
			else if (byRoom)
				Sys_Sleep (1); // QVR: (B23) (NetQuake's loop spins)
		}
		while (ret == 0 && (SetNetTime() - start_time) < tryFor);

		if (ret)
			break;

		if (!byRoom || reps % 4 == 3)
		{
			Con_Printf("still trying...\n");
			SCR_UpdateScreen ();
		}
		start_time = SetNetTime();
	}

	if (ret == 0 && byRoom)
	{
		// QVR: (B23) punched for 8 s without an answer: a router on the way maps each destination to another port
		// (a symmetric NAT), or drops what it hasn't seen asked for whatever was sent
		Con_Printf ("No answer from the room's host at %s: his router or yours doesn't allow a direct link. The host "
			"can forward his game's UDP port (26000 unless he set another) to his PC and be joined by his IP address, or "
			"you both use a virtual LAN.\n", dfunc.AddrToString (&sendaddr));
		q_strlcpy (m_return_reason, "No direct link to the host", sizeof (m_return_reason));
		goto ErrorReturn;
	}

	if (ret == 0)
	{
		reason = "No Response";
		Con_Printf("%s\n", reason);
		Q_strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	if (ret == -1)
	{
		reason = "Network Error";
		Con_Printf("%s\n", reason);
		Q_strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	ret = MSG_ReadByte();
	if (ret == CCREP_REJECT)
	{
		// QVR: (netmismatch) kept, one line (CL_EstablishConnection prints it, with the release to install: the console
		// had it here, then the server list and a stack trace)
		reason = MSG_ReadString();
		q_strlcpy(net_connectrefusal, reason, sizeof(net_connectrefusal));
		{
			size_t len = strlen(net_connectrefusal);
			while (len > 0 && (net_connectrefusal[len - 1] == '\n' || net_connectrefusal[len - 1] == '\r'))
				net_connectrefusal[--len] = 0;
		}
		if (!net_connectrefusal[0])
			q_strlcpy(net_connectrefusal, "Refused", sizeof(net_connectrefusal));
		q_strlcpy(m_return_reason, NET_ConnectRefusal(), sizeof(m_return_reason));
		goto ErrorReturn;
	}

	if (ret == CCREP_ACCEPT)
	{
		Q_memcpy(&sock->addr, &sendaddr, sizeof(struct qsockaddr));
		dfunc.SetSocketPort (&sock->addr, MSG_ReadLong());
		// QVR: (B13) a Quake VR server's network version after the port (selective acks: net_dgrm.c), none from NetQuake's
		if (msg_readcount + 4 <= net_message.cursize)
			sock->qvrNetVersion = MSG_ReadLong();
		// QVR: (review finding 2) the server's channels' first sequence (network version 39): this client's from its own
		if (msg_readcount + 4 <= net_message.cursize)
		{
			sock->receiveSequence = sock->unreliableReceiveSequence = (unsigned int) MSG_ReadLong();
			sock->sendSequence = sock->ackSequence = sock->sendChunkBase = sock->unreliableSendSequence = firstOut;
		}
		// QVR: (B23) one socket for every client from network version 36: the server goes on at the address it was
		// reached at (the port it names is its own inside its router: another outside it, behind a router that
		// doesn't keep port numbers or by a room code)
		if (sock->qvrNetVersion >= 36)
			Q_memcpy (&sock->addr, &sendaddr, sizeof (struct qsockaddr));
	}
	else
	{
		reason = "Bad Response";
		Con_Printf("%s\n", reason);
		Q_strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	dfunc.GetNameFromAddr (&sendaddr, sock->address);

	Con_Printf ("Connection accepted\n");
	sock->lastMessageTime = SetNetTime();

	// switch the connection to the specified address
	if (dfunc.Connect (newsock, &sock->addr) == -1)
	{
		reason = "Connect to Game failed";
		Con_Printf("%s\n", reason);
		Q_strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	m_return_onerror = false;
	return sock;

ErrorReturn:
	NET_FreeQSocket(sock);
ErrorReturn2:
	dfunc.Close_Socket(newsock);
	if (m_return_onerror)
	{
		IN_DeactivateForMenu();
		key_dest = key_menu;
		m_state = m_return_state;
		m_return_onerror = false;
	}
	return NULL;
}

qsocket_t *Datagram_Connect (const char *host)
{
	qsocket_t *ret = NULL;
	int port;

	host = Strip_Port (host, &port);
	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			if ((ret = _Datagram_Connect (host, port)) != NULL)
				break;
		}
	}
	return ret;
}


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

extern qboolean m_return_onerror;
extern char m_return_reason[32];

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
#define ROOM_MAGIC	"QVRR"
#define ROOM_VERSION	1
#define ROOM_REQUEST	64
#define ROOM_REPLY	32
#define ROOM_CODE_LEN	6
#define ROOM_REGISTER	0x01	// host -> service: code wanted (6 bytes or zeros) at 8, nonce at 14
#define ROOM_JOIN	0x02	// joiner -> service: code at 8, nonce at 14
#define ROOM_REGISTERED	0x81	// -> host: code at 8, nonce at 14, his outside IPv4 at 18, port at 22
#define ROOM_PEER	0x82	// -> joiner: code, nonce, the host's outside address
#define ROOM_INCOMING	0x83	// -> host: code, 0, the joiner's outside address (punch it)
#define ROOM_NOROOM	0x84	// -> joiner: code, nonce: no such room
#define ROOM_REFUSED	0x85	// -> host: code, nonce, why at 18 (1: the service is full)
#define ROOM_BADVERSION	0x86	// -> either: the service's version at 18
#define ROOM_PUNCH	0x90	// host -> joiner: opens the host's router to him (the joiner ignores it)
#define ROOM_DEFAULT_PORT	26100
#define ROOM_REGISTER_EVERY	20.0	// s (the service forgets a room 120 s after its last registration)
#define ROOM_RETRY_EVERY	2.0	// s, until the first reply
#define ROOM_PUNCH_FOR		8.0	// s a host punches towards a joiner
#define ROOM_PUNCH_EVERY	0.1	// s
#define ROOM_JOIN_WAIT		3.0	// s a joiner waits for the service's answer
#define ROOM_CONNECT_TRIES	32	// a room's connect requests (punches), ROOM_CONNECT_EVERY apart
#define ROOM_CONNECT_EVERY	0.25	// s
#define ROOM_PEERS	8

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
	double	resolveAt;		// a failed lookup's next try
	char	code[ROOM_CODE_LEN + 1];	// this host's room ("": none yet)
	unsigned int	nonce;
	double	nextRegister;
	double	firstSent;		// the first unanswered registration's time (0: answered)
	qboolean	toldUnanswered;
	roompeer_t	peers[ROOM_PEERS];
} room;

static const char roomLetters[] = "ABCDEFGHJKLMNPQRSTUVWXYZ"; // (no I or O: read as 1 and 0)

static const char *RoomServiceName (void)
{
	return Cvar_VariableString ("vr_mp_rendezvous");
}

// vr_mp_rendezvous ("host[:port]") resolved into `out` -> 0; -1 none set or not found
static int RoomResolve (const char *name, struct qsockaddr *out)
{
	char	host[64];
	char	*colon;
	int		port = ROOM_DEFAULT_PORT;

	q_strlcpy (host, name, sizeof (host));
	if (!host[0])
		return -1;
	if ((colon = strrchr (host, ':')) != NULL)
	{
		*colon = 0;
		port = Q_atoi (colon + 1);
	}
	if (port <= 0 || port > 65535 || dfunc.GetAddrFromName (host, out) == -1)
		return -1;
	dfunc.SetSocketPort (out, port);
	return 0;
}

static void RoomWrite (sys_socket_t s, int type, const char *code, unsigned int nonce, int size,
	const struct qsockaddr *to)
{
	byte	pkt[ROOM_REQUEST];

	memset (pkt, 0, sizeof (pkt));
	memcpy (pkt, ROOM_MAGIC, 4);
	pkt[4] = ROOM_VERSION;
	pkt[5] = (byte) type;
	if (code)
		memcpy (pkt + 8, code, q_min ((int) strlen (code), ROOM_CODE_LEN));
	pkt[14] = (byte) (nonce >> 24);
	pkt[15] = (byte) (nonce >> 16);
	pkt[16] = (byte) (nonce >> 8);
	pkt[17] = (byte) nonce;
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

// A host's datagram from the room service (on his listening socket) -> qtrue: taken (any datagram of the room format
// is: a stray one is dropped)
static qboolean RoomHostRead (const byte *pkt, int length, struct qsockaddr *from)
{
	int i;
	struct qsockaddr peer;

	if (!IsRoomPacket (pkt, length))
		return false;
	if (!room.resolved || length < ROOM_REPLY || dfunc.AddrCompare (from, &room.serviceAddr) != 0)
		return true;
	switch (pkt[5])
	{
	case ROOM_REGISTERED:
		if (RoomNonce (pkt) != room.nonce)
			break;
		room.firstSent = 0;
		room.toldUnanswered = false;
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
		return;
	}
	if (Q_strcmp (name, room.service) != 0)
	{
		q_strlcpy (room.service, name, sizeof (room.service));
		room.resolved = false;
		room.resolveAt = 0;
		room.code[0] = 0;
		room.nextRegister = 0;
		room.firstSent = 0;
		room.toldUnanswered = false;
	}
	if (!room.resolved)
	{
		if (net_time < room.resolveAt)
			return;
		if (RoomResolve (room.service, &room.serviceAddr) == -1)
		{
			if (room.resolveAt == 0)
				Con_Printf ("The room service %s was not found: no room code (players can still join by your IP "
					"address)\n", room.service);
			room.resolveAt = net_time + 60.0;
			return;
		}
		room.resolved = true;
	}
	if (net_time >= room.nextRegister)
	{
		room.nonce = (unsigned int) rand () ^ ((unsigned int) rand () << 15) ^ ((unsigned int) (net_time * 1000.0));
		RoomWrite (acceptsock, ROOM_REGISTER, room.code, room.nonce, ROOM_REQUEST, &room.serviceAddr);
		if (room.firstSent == 0)
			room.firstSent = net_time;
		room.nextRegister = net_time + (room.code[0] ? ROOM_REGISTER_EVERY / 4 : ROOM_RETRY_EVERY);
		if (net_time - room.firstSent > 3 * ROOM_RETRY_EVERY && !room.toldUnanswered)
		{
			Con_Printf ("The room service %s does not answer: no room code yet (players can still join by your IP "
				"address)\n", room.service);
			room.toldUnanswered = true;
		}
		if (net_time - room.firstSent > 60.0)
			room.nextRegister = net_time + ROOM_REGISTER_EVERY; // (gone a minute: asked less often)
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
		RoomWrite (acceptsock, ROOM_PUNCH, room.code, 0, ROOM_REPLY, &p->addr);
		p->next = net_time + ROOM_PUNCH_EVERY;
	}
}

// The listening socket's datagrams read: each client's queued for him, the room service's taken, the rest queued for
// _Datagram_CheckNewConnections
static void ReadShared (int landriver, sys_socket_t acceptsock)
{
	struct qsockaddr	from;
	int		length;
	unsigned int	head;
	qsocket_t	*s;
	net_landriver_t	*driver = &net_landrivers[landriver];

	net_landriverlevel = landriver; // (dfunc: RoomHostRead's)

	while ((length = driver->Read (acceptsock, sharedBuffer, sizeof (sharedBuffer), &from)) > 0)
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
	if (RoomResolve (name, &service) == -1)
	{
		Con_Printf ("The room service %s was not found (no internet?): join by the host's IP address\n", name);
		q_strlcpy (m_return_reason, "Room service not found", sizeof (m_return_reason));
		return false;
	}
	Con_Printf ("Looking up room %s at %s...\n", code, name);
	SCR_UpdateScreen ();
	nonce = (unsigned int) rand () ^ ((unsigned int) rand () << 15) ^ ((unsigned int) (Sys_DoubleTime () * 1000.0));
	start = SetNetTime ();
	sent = -1.0;
	while (SetNetTime () - start < ROOM_JOIN_WAIT)
	{
		if (sent < 0 || net_time - sent >= 0.5)
		{
			RoomWrite (sock, ROOM_JOIN, code, nonce, ROOM_REQUEST, &service);
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
		if (net_message.data[5] == ROOM_BADVERSION)
		{
			Con_Printf ("The room service %s speaks version %d, this game %d: join by the host's IP address\n", name,
				net_message.data[18], ROOM_VERSION);
			q_strlcpy (m_return_reason, "Room service: other version", sizeof (m_return_reason));
			return false;
		}
		if (RoomNonce (net_message.data) != nonce || memcmp (net_message.data + 8, code, ROOM_CODE_LEN) != 0)
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
	void	(*print_fn)(const char *fmt, ...) FUNCP_PRINTF(1,2);

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print_fn = Con_Printf;
	}
	else
	{
		if (pr_global_struct->deathmatch)
			return;
		print_fn = SV_ClientPrintf;
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
#define DGRM_RELIABLE_CHUNK		NET_RELIABLE_CHUNK
#define DGRM_RELIABLE_WINDOW	NET_RELIABLE_WINDOW
#define DGRM_RESEND_LEAST		0.15
#define DGRM_RESEND_MOST		1.0
#define DGRM_RESEND_MARGIN		0.05
#define DGRM_FAST_ACKS			2

static unsigned int ReliableChunks (const qsocket_t *sock)
{
	return ((unsigned int) sock->sendMessageLength + DGRM_RELIABLE_CHUNK - 1) / DGRM_RELIABLE_CHUNK;
}

// the chunks in flight at most: a Quake VR peer's window, else NetQuake's one (selective acks, above)
static unsigned int ReliableWindow (const qsocket_t *sock)
{
	return sock->qvrNetVersion ? DGRM_RELIABLE_WINDOW : 1;
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

	if (sock->reliableRtt <= 0)
		return DGRM_RESEND_MOST; // (nothing measured yet: NetQuake's)
	t = q_max (DGRM_RESEND_LEAST, 2 * sock->reliableRtt + DGRM_RESEND_MARGIN);
	t *= (double) (1 << q_min (sock->resendBackoff, 4));
	return q_min (t, DGRM_RESEND_MOST);
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
			if (sequence < sock->unreliableReceiveSequence)
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
			if (sequence != sock->unreliableReceiveSequence)
			{
				count = sequence - sock->unreliableReceiveSequence;
				droppedDatagrams += count;
				sock->qvrUnreliableSkipped += count; // QVR: (B13) the net HUD's loss
				Con_DPrintf("Dropped %u datagram(s)\n", count);
			}
			sock->unreliableReceiveSequence = sequence + 1;
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
static const char *Strip_Port (const char *host)
{
	static char	noport[MAX_QPATH];
			/* array size as in Host_Connect_f() */
	char		*p;
	int		port;

	if (!host || !*host)
		return host;
	q_strlcpy (noport, host, sizeof(noport));
	if ((p = Q_strrchr(noport, ':')) == NULL)
		return host;
	*p++ = '\0';
	port = Q_atoi (p);
	if (port > 0 && port < 65536 && port != net_hostport)
	{
		net_hostport = port;
		Con_Printf("Port set to %d\n", net_hostport);
	}
	return noport;
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
	struct qsockaddr sendaddr;

	if (testInProgress)
		return;

	host = Strip_Port (Cmd_Argv(1));

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
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
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
	struct qsockaddr sendaddr;

	if (test2InProgress)
		return;

	host = Strip_Port (Cmd_Argv(1));

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
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
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

	return 0;
}


void Datagram_Shutdown (void)
{
	int i;

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
	double		t;

	if (Cvar_VariableString ("vr_mp_clientkey")[0])
		return;
	t = Sys_DoubleTime ();
	a = (unsigned) (t * 1000003.0) ^ ((unsigned) (uintptr_t) &key << 7) ^ (unsigned) (long long) (t * 1e9);
	b = (unsigned) rand () * 2654435761u ^ (unsigned) (uintptr_t) &t ^ (unsigned) (t * 1e6);
	q_snprintf (key, sizeof (key), "%08x%08x", a, b);
	Cvar_Set ("vr_mp_clientkey", key);
}

// QVR: a connection's CCREP_ACCEPT (its first, and again for a duplicate request): the listening socket's port (a
// NetQuake client switches to it) and (B13) this server's network version (selective acks)
static void SendAccept (sys_socket_t acceptsock, const struct qsockaddr *to)
{
	struct qsockaddr	self;

	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREP_ACCEPT);
	dfunc.GetSocketAddr(acceptsock, &self);
	MSG_WriteLong(&net_message, dfunc.GetSocketPort(&self));
	MSG_WriteLong(&net_message, QVR_NET_VERSION);
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
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
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
		MSG_WriteString(&net_message, client->netconnection->address);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
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
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
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
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		return NULL;
	}

	// QVR: Quake VR's network version ("QVR" and a long after NetQuake's; vr/vr_api.h): a VR server takes the same only
	{
		char reason[160];
		if (msg_readcount < net_message.cursize && Q_strcmp(MSG_ReadString(), "QVR") == 0)
		{
			qvrVersion = MSG_ReadLong();
			if (msg_readcount < net_message.cursize)
				q_strlcpy(qvrName, MSG_ReadString(), sizeof(qvrName));
			if (msg_readcount < net_message.cursize)
				q_strlcpy(qvrKey, MSG_ReadString(), sizeof(qvrKey));
		}
		reason[0] = 0;
		if (sv.active && (sv.protocolflags & PRFL_QUAKEVR) && qvrVersion != QVR_NET_VERSION)
		{
			if (qvrVersion == 0)
				q_snprintf(reason, sizeof(reason), "This is a Quake VR server (network version %d): join with Quake VR.\n",
					QVR_NET_VERSION);
			else
				q_snprintf(reason, sizeof(reason), "This Quake VR server has network version %d, your game %d: join with "
					"the same Quake VR version.\n", QVR_NET_VERSION, qvrVersion);
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
			dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
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
			dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
			SZ_Clear(&net_message);
			return NULL;
		}
	}
#endif

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
			Con_Printf ("%s back from %s (his old connection silent %.1f s): the old one closed\n", qvrName,
				dfunc.AddrToString (&clientaddr), net_time - s->lastMessageTime);
			NET_Close (s);
			return NULL;
		}
		if (ret == 0)
		{
			// is this a duplicate connection reqeust?
			if (net_time - s->connecttime < 2.0)
			{
				// yes, so send a duplicate reply
				SendAccept (acceptsock, &clientaddr);
				return NULL;
			}
			// it's somebody coming back in from a crash/disconnect
			// so close the old qsocket and let their retry get them back in
			NET_Close(s);
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
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		return NULL;
	}

	// QVR: (B23) no socket of his own: the listening one (net_dgrm.c's ReadShared). The accept still names its port (a
	// NetQuake client switches to it); a Quake VR client of network version 36 or later keeps the address it reached.
	sock->socket = acceptsock;
	sock->qvrShared = true;
	sock->landriver = net_landriverlevel;
	sock->addr = clientaddr;
	sock->qvrNetVersion = qvrVersion; // QVR
	q_strlcpy(sock->qvrName, qvrName, sizeof(sock->qvrName)); // QVR
	q_strlcpy(sock->qvrKey, qvrKey, sizeof(sock->qvrKey)); // QVR: (B25)
	Q_strcpy(sock->address, dfunc.AddrToString(&clientaddr));

	// send him back the info about the server connection he has been allocated
	SendAccept (acceptsock, &clientaddr);

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
		// QVR: (B13) the browser's addresses (a host name is looked up here: as connect's, it may block a moment)
		sa->count = 0;
		asked = VR_BrowserAddressCount ();
		for (i = 0; i < asked && sa->count < HOSTCACHESIZE; i++)
		{
			struct qsockaddr *to = &sa->addr[sa->count];
			if (dfunc.GetAddrFromName (VR_BrowserAddress (i), to) == -1)
				continue;
			if (dfunc.GetSocketPort (to) == 0)
				dfunc.SetSocketPort (to, net_hostport);
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


static qsocket_t *_Datagram_Connect (const char *host)
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
	const int		tries = byRoom ? ROOM_CONNECT_TRIES : 3;
	const double	tryFor = byRoom ? ROOM_CONNECT_EVERY : 2.5;

	// see if we can resolve the host name
	if (!byRoom && dfunc.GetAddrFromName(host, &sendaddr) == -1)
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
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_CONNECT);
		MSG_WriteString(&net_message, "QUAKE");
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		// QVR: Quake VR's network version (vr/vr_api.h; -mpnetversion N: another, the harness's reject test)
		MSG_WriteString(&net_message, "QVR");
		MSG_WriteLong(&net_message, COM_CheckParm("-mpnetversion") && COM_CheckParm("-mpnetversion") + 1 < com_argc
			? Q_atoi(com_argv[COM_CheckParm("-mpnetversion") + 1]) : QVR_NET_VERSION);
		MSG_WriteString(&net_message, cl_name.string); // QVR: a loaded coop game gives him his saved slot by it
		MSG_WriteString(&net_message, Cvar_VariableString ("vr_mp_clientkey")); // QVR: (B25) this game's, for "back from a crash"
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (newsock, net_message.data, net_message.cursize, &sendaddr);
		SZ_Clear(&net_message);
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
		reason = MSG_ReadString();
		Con_Printf("%s\n", reason);
		q_strlcpy(m_return_reason, reason, sizeof(m_return_reason));
		goto ErrorReturn;
	}

	if (ret == CCREP_ACCEPT)
	{
		Q_memcpy(&sock->addr, &sendaddr, sizeof(struct qsockaddr));
		dfunc.SetSocketPort (&sock->addr, MSG_ReadLong());
		// QVR: (B13) a Quake VR server's network version after the port (selective acks: net_dgrm.c), none from NetQuake's
		if (msg_readcount + 4 <= net_message.cursize)
			sock->qvrNetVersion = MSG_ReadLong();
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

	host = Strip_Port (host);
	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			if ((ret = _Datagram_Connect (host)) != NULL)
				break;
		}
	}
	return ret;
}


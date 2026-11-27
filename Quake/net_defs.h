/*
 * net_defs.h -- functions and data private to the network layer
 * net_sys.h and its dependencies must be included before net_defs.h.
 *
 * Copyright (C) 1996-1997  Id Software, Inc.
 * Copyright (C) 2005-2012  O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __NET_DEFS_H
#define __NET_DEFS_H

struct qsockaddr
{
#if defined(HAVE_SA_LEN)
	unsigned char qsa_len;
	unsigned char qsa_family;
#else
	short qsa_family;
#endif	/* BSD, sockaddr */
	unsigned char qsa_data[14];
};

#define NET_HEADERSIZE		(2 * sizeof(unsigned int))
#define NET_DATAGRAMSIZE	(MAX_DATAGRAM + NET_HEADERSIZE)

// NetHeader flags
#define NETFLAG_LENGTH_MASK	0x0000ffff
#define NETFLAG_DATA		0x00010000
#define NETFLAG_ACK		0x00020000
#define NETFLAG_NAK		0x00040000
#define NETFLAG_EOM		0x00080000
#define NETFLAG_UNRELIABLE	0x00100000
#define NETFLAG_CTL		0x80000000

#if (NETFLAG_LENGTH_MASK & NET_MAXMESSAGE) != NET_MAXMESSAGE
#error "NET_MAXMESSAGE must fit within NETFLAG_LENGTH_MASK"
#endif

#define NET_PROTOCOL_VERSION	3

/**

This is the network info/connection protocol.  It is used to find Quake
servers, get info about them, and connect to them.  Once connected, the
Quake game protocol (documented elsewhere) is used.


General notes:
	game_name is currently always "QUAKE", but is there so this same protocol
		can be used for future games as well; can you say Quake2?

CCREQ_CONNECT
		string	game_name		"QUAKE"
		byte	net_protocol_version	NET_PROTOCOL_VERSION

CCREQ_SERVER_INFO
		string	game_name		"QUAKE"
		byte	net_protocol_version	NET_PROTOCOL_VERSION

CCREQ_PLAYER_INFO
		byte	player_number

CCREQ_RULE_INFO
		string	rule

CCREP_ACCEPT
		long	port

CCREP_REJECT
		string	reason

CCREP_SERVER_INFO
		string	server_address
		string	host_name
		string	level_name
		byte	current_players
		byte	max_players
		byte	protocol_version	NET_PROTOCOL_VERSION

CCREP_PLAYER_INFO
		byte	player_number
		string	name
		long	colors
		long	frags
		long	connect_time
		string	address

CCREP_RULE_INFO
		string	rule
		string	value

	note:
		There are two address forms used above.  The short form is just a
		port number.  The address that goes along with the port is defined as
		"whatever address you receive this reponse from".  This lets us use
		the host OS to solve the problem of multiple host addresses (possibly
		with no routing between them); the host will use the right address
		when we reply to the inbound connection request.  The long from is
		a full address and port in a string.  It is used for returning the
		address of a server that is not running locally.

**/

#define CCREQ_CONNECT		0x01
#define CCREQ_SERVER_INFO	0x02
#define CCREQ_PLAYER_INFO	0x03
#define CCREQ_RULE_INFO		0x04

#define CCREP_ACCEPT		0x81
#define CCREP_REJECT		0x82
#define CCREP_SERVER_INFO	0x83
#define CCREP_PLAYER_INFO	0x84
#define CCREP_RULE_INFO		0x85
#define CCREP_CHALLENGE		0x86 // QVR: (mpfix A1, network version 40) long cookie: send CCREQ_CONNECT again with it
					// after the rest (the server gives a slot only to a request whose cookie it made for that
					// address: the address takes replies)

#define QVR_UNHEARD_TIMEOUT	15.0 // QVR: (mpfix A1) seconds a server's new client may go without acking a chunk

#define NET_RELIABLE_WINDOW	16 // QVR: reliable chunks in flight at most (net_dgrm.c)
#define NET_RELIABLE_CHUNK	1200 // QVR: a reliable chunk's bytes at most (net_dgrm.c)
#define NET_EMPTY_READS	256 // QVR: empty datagrams a driver's Read skips at most in one call (one past them: none waiting)

// QVR: (B23) datagrams queued for a reader (net_dgrm.c: a server's clients on its one socket), oldest first
typedef struct
{
	struct dgrmpacket_s	*first;
	struct dgrmpacket_s	*last;
	int		count;
} dgrmqueue_t;

typedef struct qsocket_s
{
	struct qsocket_s	*next;
	double		connecttime;
	double		lastMessageTime;
	double		lastSendTime;

	qboolean	disconnected;
	qboolean	canSend;
	qboolean	sendNext;

	int		driver;
	int		landriver;
	sys_socket_t	socket;
	void		*driverdata;

	unsigned int	ackSequence;
	unsigned int	sendSequence;
	unsigned int	sendChunkBase;	// QVR: the sequence of the reliable message's first chunk (net_dgrm.c)
	// QVR: the reliable window's resends (net_dgrm.c, MP_PLAN.md B12b): each chunk in flight's last send (by its sequence
	// modulo the window) and whether it was a resend; the chunks' smoothed round trip (0: none measured yet); the acks of
	// later chunks since the last in order (a lost chunk's sign); timeouts in a row; a fast resend since the last in order
	double		chunkSentTime[NET_RELIABLE_WINDOW];
	qboolean	chunkResent[NET_RELIABLE_WINDOW];
	qboolean	chunkAcked[NET_RELIABLE_WINDOW];	// QVR: (B13) acked out of order: the receiver holds it (net_dgrm.c)
	double		reliableRtt;
	int		aheadAcks;
	int		resendBackoff;
	qboolean	fastResent;
	// QVR: (B13) the link's counts for the net HUD (vr_nethud.cpp): unreliable datagrams read in order and the ones
	// their sequence skipped (lost, or late), reliable chunks sent again, the packets' bytes each way
	unsigned int	qvrUnreliableRead;
	unsigned int	qvrUnreliableSkipped;
	unsigned int	qvrResent;
	unsigned int	qvrBytesIn;
	unsigned int	qvrBytesOut;
	qboolean	qvrAcceptStale;	// QVR: (B20) an unreliable datagram overtaken on the way is still read (net_stalemessage):
					// a remote client's moves at the server (SV_ReadClientMessage reads only its QVR_CLC_MOVES)
	unsigned int	qvrStaleRead;	// QVR: ... how many were
	double		qvrUnreliableTime;	// QVR: (review 2026-11-18, finding 2) the last unreliable datagram's read in its window
	qboolean	qvrSeeded;	// QVR: ... the channels' first sequences drawn at random (network version 39; net_dgrm.c)
	unsigned int	qvrFirstOut;	// QVR: ... a server's first one to its client (the accept again for a duplicate request)
	int		qvrNetVersion;	// QVR: the peer's Quake VR network version from its connect request (0: none; net_dgrm.c)
	char		qvrName[32];	// QVR: the player's name from its connect request ("": none): a loaded coop game's slot by it
	char		qvrKey[24];	// QVR: (B25) his game's vr_mp_clientkey from its connect request ("": none): "back from a crash" only by it
	qboolean	qvrShared;	// QVR: (B23) a server's client on the server's listening socket (net_dgrm.c): its datagrams
					// are read from there and queued for it (qvrIn), its socket is not its own to close
	dgrmqueue_t	qvrIn;		// QVR: ... its queued datagrams, oldest first
	qboolean	qvrHeard;	// QVR: (mpfix A1) the peer has acked a reliable chunk (a server's client: its address takes
					// replies; until then one chunk in flight, and NET_GetMessage drops it after QVR_UNHEARD_TIMEOUT)
	qboolean	qvrClosing;	// QVR: (mpfix A2) closed by the driver (that player back from a crash): NET_GetMessage
					// gives -1 and its owner frees it (never handed out again while a client slot still holds it)
	unsigned int	unreliableSendSequence;
	int		sendMessageLength;
	byte		sendMessage [NET_MAXMESSAGE];

	unsigned int	receiveSequence;
	unsigned int	unreliableReceiveSequence;
	int		receiveMessageLength;
	byte		receiveMessage [NET_MAXMESSAGE];
	// QVR: (B13) the chunks after the next one in order that came first, held until it comes (by their sequence modulo
	// the window; length 0: none held): a Quake VR peer's window survives reordering (net_dgrm.c)
	int		heldLength[NET_RELIABLE_WINDOW];
	qboolean	heldEom[NET_RELIABLE_WINDOW];
	byte		held[NET_RELIABLE_WINDOW][NET_RELIABLE_CHUNK];

	struct qsockaddr	addr;
	char		address[NET_NAMELEN];

} qsocket_t;

extern qsocket_t	*net_activeSockets;
extern qsocket_t	*net_freeSockets;
extern int		net_numsockets;

typedef struct
{
	const char	*name;
	qboolean	initialized;
	sys_socket_t	controlSock;
	sys_socket_t	(*Init) (void);
	void		(*Shutdown) (void);
	void		(*Listen) (qboolean state);
	sys_socket_t	(*Open_Socket) (int port);
	int		(*Close_Socket) (sys_socket_t socketid);
	int		(*Connect) (sys_socket_t socketid, struct qsockaddr *addr);
	sys_socket_t	(*CheckNewConnections) (void);
	int		(*Read) (sys_socket_t socketid, byte *buf, int len, struct qsockaddr *addr);
	int		(*Write) (sys_socket_t socketid, byte *buf, int len, struct qsockaddr *addr);
	int		(*Broadcast) (sys_socket_t socketid, byte *buf, int len);
	const char *	(*AddrToString) (struct qsockaddr *addr);
	int		(*StringToAddr) (const char *string, struct qsockaddr *addr);
	int		(*GetSocketAddr) (sys_socket_t socketid, struct qsockaddr *addr);
	int		(*GetNameFromAddr) (struct qsockaddr *addr, char *name);
	int		(*GetAddrFromName) (const char *name, struct qsockaddr *addr);
	int		(*AddrCompare) (struct qsockaddr *addr1, struct qsockaddr *addr2);
	int		(*GetSocketPort) (struct qsockaddr *addr);
	int		(*SetSocketPort) (struct qsockaddr *addr, int port);
} net_landriver_t;

#define	MAX_NET_DRIVERS		8
extern net_landriver_t	net_landrivers[];
extern const int	net_numlandrivers;

typedef struct
{
	const char	*name;
	qboolean	initialized;
	int		(*Init) (void);
	void		(*Listen) (qboolean state);
	void		(*SearchForHosts) (qboolean xmit);
	qsocket_t	*(*Connect) (const char *host);
	qsocket_t	*(*CheckNewConnections) (void);
	int		(*QGetMessage) (qsocket_t *sock);
	int		(*QSendMessage) (qsocket_t *sock, sizebuf_t *data);
	int		(*SendUnreliableMessage) (qsocket_t *sock, sizebuf_t *data);
	qboolean	(*CanSendMessage) (qsocket_t *sock);
	qboolean	(*CanSendUnreliableMessage) (qsocket_t *sock);
	void		(*Close) (qsocket_t *sock);
	void		(*Shutdown) (void);
} net_driver_t;

extern net_driver_t	net_drivers[];
extern const int	net_numdrivers;

/* Loop driver must always be registered the first */
#define IS_LOOP_DRIVER(p)	((p) == 0)

extern int		net_driverlevel;

extern int		messagesSent;
extern int		messagesReceived;
extern int		unreliableMessagesSent;
extern int		unreliableMessagesReceived;

qsocket_t *NET_NewQSocket (void);
void NET_FreeQSocket(qsocket_t *);
double SetNetTime(void);


#define HOSTCACHESIZE	32 // QVR: (B13) 8 before: the server browser's favourites and recent servers too

typedef struct
{
	char	name[16];
	char	map[16];
	char	cname[64]; // QVR: 32 before: a favourite's address as he wrote it (a host name and port)
	int		users;
	int		maxusers;
	int		ping;	// QVR: (B13) the reply's round trip in ms (the server browser's)
	int		version;	// QVR: (B13) its NetQuake protocol version (NET_PROTOCOL_VERSION: joinable)
	int		qvrNetVersion;	// QVR: (netmismatch) the Quake VR network version it takes (0: not said: a server before
							// this field, or one that takes any client)
	char	qvrRelease[24];	// QVR: (netmismatch) its Quake VR release ("1.2.0-beta7"; "": not said)
	int		driver;
	int		ldriver;
	struct qsockaddr addr;
} hostcache_t;

extern int hostCacheCount;
extern hostcache_t hostcache[HOSTCACHESIZE];

// QVR: (netmismatch) the last connect's refusal: the reason the server gave (CCREP_REJECT), "" none (net_main.c)
extern char net_connectrefusal[256];


typedef struct _PollProcedure
{
	struct _PollProcedure	*next;
	double				nextTime;
	void				(*procedure)(void *);
	void				*arg;
} PollProcedure;

void SchedulePollProcedure(PollProcedure *pp, double timeOffset);

// QVR: (B24, tests) -loopbackonly's fence in the IPv4 drivers' Write and GetAddrFromName (net_main.c)
qboolean NET_LoopbackRefusesTo (const struct qsockaddr *addr);
qboolean NET_LoopbackRefusesName (const char *name, const struct qsockaddr *parsed);

#endif	/* __NET_DEFS_H */


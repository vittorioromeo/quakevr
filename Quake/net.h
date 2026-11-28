/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2009-2010 Ozkan Sezer
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

/*
	net.h
	quake's interface to the networking layer
	network functions and data, common to the
	whole engine
*/

#ifndef _QUAKE_NET_H
#define _QUAKE_NET_H


#define	NET_NAMELEN		64

#define NET_MAXMESSAGE		65535	/* ericw -- was 32000 */

extern int		DEFAULTnet_hostport;
extern int		net_hostport;

extern cvar_t		hostname;

extern	double		net_time;
extern	sizebuf_t	net_message;
extern	qboolean	net_loopbackonly; // QVR: (B24, tests) -loopbackonly: 127.0.0.0/8 only (net_main.c)
void NET_LoopbackRefused (const char *what); // QVR: (B24) prints "NET SAFETY: refused <what>" (the first 16)
qboolean NET_LoopbackAddr (unsigned int netorder_ipv4); // QVR: (B24) in 127.0.0.0/8
extern	qboolean	net_stalemessage; // QVR: (B20) the message just read was overtaken on the way (only a remote client's moves are read so)
void NET_RestoreMessage (void); // QVR: net_message back on its own buffer, emptied (Host_Error: a parse that pointed it elsewhere, MP_PLAN B19)
extern	int		net_activeconnections;


void	NET_Init (void);
void	NET_Shutdown (void);

const char	*NET_RoomCode (void); // QVR: (B23) this host's room code ("": none; net_dgrm.c)
// QVR: (2026-11-21) the port check: whether this host's game port is reachable from the internet (the room service's
// probe; net_dgrm.c). OFF: no room service (vr_mp_room 0 or no vr_mp_rendezvous); OLDSERVICE: a version 2 service.
enum { NET_PORTCHECK_OFF, NET_PORTCHECK_WAITING, NET_PORTCHECK_REACHABLE, NET_PORTCHECK_CLOSED, NET_PORTCHECK_OLDSERVICE,
	NET_PORTCHECK_NOANSWER };
int		NET_PortCheck (void);
const char	*NET_PortCheckText (qboolean brief); // "Port 26000: reachable"; brief: "port 26000 reachable" (the net HUD's)

struct qsocket_s	*NET_CheckNewConnections (void);
// returns a new connection number if there is one pending, else -1

struct qsocket_s	*NET_Connect (const char *host);
// called by client to connect to a host.  Returns -1 if not able to

double NET_QSocketGetTime (const struct qsocket_s *sock);
void NET_AcceptStale (struct qsocket_s *sock, int on); // QVR: (B20) its overtaken unreliable datagrams read all the same (net_stalemessage)
const char *NET_QSocketGetAddressString (const struct qsocket_s *sock);
int NET_QSocketLocalSeat (const struct qsocket_s *sock); // QVR: splitscreen: the seat a loopback server side is (0: the host's own; 1-3; -1: not the loopback's)
qboolean NET_QSocketIsLocal (const struct qsocket_s *sock); // QVR: the loopback's server side, the listen host's own client (net_loop.c names it "LOCAL"); NULL: no
const char *NET_QSocketGetQvrName (const struct qsocket_s *sock); // QVR: the name in its connect request
const char *NET_QSocketGetQvrKey (const struct qsocket_s *sock); // QVR: the vr_mp_clientkey in its connect request ("": none)

qboolean NET_CanSendMessage (struct qsocket_s *sock);
// Returns true or false if the given qsocket can currently accept a
// message to be transmitted.

int	NET_GetMessage (struct qsocket_s *sock);
// returns data in net_message sizebuf
// returns 0 if no data is waiting
// returns 1 if a message was received
// returns 2 if an unreliable message was received
// returns -1 if the connection died

int	NET_SendMessage (struct qsocket_s *sock, sizebuf_t *data);
int	NET_SendUnreliableMessage (struct qsocket_s *sock, sizebuf_t *data);
// returns 0 if the message connot be delivered reliably, but the connection
//		is still considered valid
// returns 1 if the message was sent properly
// returns -1 if the connection died

int	NET_SendToAll(sizebuf_t *data, double blocktime);
// This is a reliable *blocking* send to all attached clients.

void	NET_Close (struct qsocket_s *sock);
// if a dead connection is returned by a get or send function, this function
// should be called when it is convenient

// Server calls when a client is kicked off for a game related misbehavior
// like an illegal protocal conversation.  Client calls when disconnecting
// from a server.
// A netcon_t number will not be reused until this function is called for it

void	NET_Poll (void);


// Server list related globals:
extern	qboolean	slistInProgress;
extern	qboolean	slistSilent;
extern	qboolean	slistLocal;

extern	int		hostCacheCount;

void	NET_Slist_f (void);
void	NET_SlistSort (void);
const char *NET_SlistPrintServer (int n);
const char *NET_SlistPrintServerName (int n);
// QVR: (B13) a search's reply (the server browser's, vr_serverbrowser.cpp): 0 out of range
int NET_SlistEntry (int n, const char **name, const char **map, const char **address, int *users, int *maxusers,
					int *ping, int *version);
// QVR: (netmismatch) a search reply's Quake VR part: the network version the server takes and its release (0 / "": not
// said: a server before 1.2.0-beta7, or one that takes any client); 0 out of range
int NET_SlistQvr (int n, int *netVersion, const char **release);
// QVR: (netmismatch) this game's Quake VR network version as it says it (vr_net_test_version N: another, the tests')
int NET_QvrOwnVersion (void);
// QVR: (netmismatch) the Quake VR release of a network version the releases before 1.2.0-beta7 had ("1.2.0-beta3 to
// 1.2.0-beta5"; later servers say their own), "" not one of them
const char *NET_QvrReleaseOf (int netVersion);
// QVR: (netmismatch) the last connect's refusal, the server's reason with what to install when it names a network
// version (one line, no newline), "" the connect was not refused (no answer, an error)
const char *NET_ConnectRefusal (void);
// QVR: (netmismatch) this game's Quake VR release as a server says it ("1.2.0-beta7", "-dev" after a development build's)
const char *NET_QvrRelease (void);
// QVR: (B13) a connection's counts (the net HUD's, vr_nethud.cpp; qsocket_t's): unreliable datagrams read and skipped,
// reliable chunks resent, bytes in and out; its reliable chunks' smoothed round trip in seconds (0: none yet)
void NET_QvrLinkCounts (struct qsocket_s *sock, unsigned int counts[5], double *reliableRtt);
// QVR: (B16) the wall-clock seconds since the connection last read a message (0: a loopback one, never silent)
double NET_QvrSilence (struct qsocket_s *sock);


/* FIXME: driver related, but public:
 */
extern	qboolean	ipxAvailable;
extern	qboolean	tcpipAvailable;
extern	char		my_ipx_address[NET_NAMELEN];
extern	char		my_tcpip_address[NET_NAMELEN];

#endif	/* _QUAKE_NET_H */


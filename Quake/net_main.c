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
#include "vr/vr_tracy.h"
#include "vr/vr_api.h" // QVR: VR_BrowserSearchDone
#include "q_stdinc.h"
#include "arch_def.h"
#include "net_sys.h"
#include "net_defs.h"
#include "net_loop.h" // QVR: Loop_SocketSeat

#ifndef WITHOUT_CURL
#include <curl/curl.h>
#endif

qsocket_t	*net_activeSockets = NULL;
qsocket_t	*net_freeSockets = NULL;
int		net_numsockets = 0;

qboolean	ipxAvailable = false;
qboolean	tcpipAvailable = false;

int		net_hostport;
int		DEFAULTnet_hostport = 26000;

char		my_ipx_address[NET_NAMELEN];
char		my_tcpip_address[NET_NAMELEN];

static qboolean	listening = false;

qboolean	slistInProgress = false;
qboolean	slistSilent = false;
qboolean	slistLocal = true;
static double	slistStartTime;
static int		slistLastShown;

static void Slist_Send (void *);
static void Slist_Poll (void *);
static PollProcedure	slistSendProcedure = {NULL, 0.0, Slist_Send};
static PollProcedure	slistPollProcedure = {NULL, 0.0, Slist_Poll};

sizebuf_t	net_message;
qboolean	net_stalemessage; // QVR: (B20) the message NET_GetMessage just read came overtaken (qsocket_t.qvrAcceptStale)

// QVR: (B24, tests) -loopbackonly: this game's network talks to 127.0.0.0/8 only. The multiplayer harness starts every
// game with it (a B23 run's dedicated server once looked up the shipped room service and sent it datagrams): a name other
// than "localhost" is never looked up, an address outside 127.0.0.0/8 never resolved nor sent to (a LAN broadcast is
// dropped quietly), no reverse lookups, no IPX, no add-on downloads. Each refusal prints "NET SAFETY:" (the first 16),
// which fails the harness's run.
qboolean	net_loopbackonly;
static int	net_loopbackrefusals;

void NET_LoopbackRefused (const char *what)
{
	if (++net_loopbackrefusals <= 16)
		Con_SafePrintf ("NET SAFETY: refused %s (-loopbackonly: 127.0.0.0/8 only)\n", what);
}

qboolean NET_LoopbackAddr (unsigned int netorder_ipv4)
{
	return ((const unsigned char *) &netorder_ipv4)[0] == 127; // (network order: the first octet first)
}

// The fence on a datagram's destination (an IPv4 driver's Write) -> qtrue: refused, not to be sent (a LAN broadcast
// quietly, anything else printed)
qboolean NET_LoopbackRefusesTo (const struct qsockaddr *addr)
{
	const struct sockaddr_in *in = (const struct sockaddr_in *) addr;
	const byte *ip = (const byte *) &in->sin_addr.s_addr;

	if (!net_loopbackonly || NET_LoopbackAddr (in->sin_addr.s_addr))
		return false;
	if (in->sin_addr.s_addr != 0xFFFFFFFFu) // (INADDR_BROADCAST)
		NET_LoopbackRefused (va ("%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], ntohs (in->sin_port)));
	return true;
}

// The fence on an IPv4 driver's GetAddrFromName: `name` as typed, `parsed` its address when it was one (NULL: a name to
// look up, never looked up but "localhost") -> qtrue: refused (printed)
qboolean NET_LoopbackRefusesName (const char *name, const struct qsockaddr *parsed)
{
	if (!net_loopbackonly)
		return false;
	if (parsed ? NET_LoopbackAddr (((const struct sockaddr_in *) parsed)->sin_addr.s_addr) : !q_strcasecmp (name, "localhost"))
		return false;
	NET_LoopbackRefused (name);
	return true;
}
// QVR: net_message's own buffer. A snapshot's records are parsed with net_message pointed at each (vr_netsnap.cpp), and
// a Host_Error inside one (a damaged record: mp_fuzz) left it there: the next connection's request overflowed its few
// bytes (SZ_GetSpace, "B joined again 1 times"). Host_Error puts it back (NET_RestoreMessage).
static sizebuf_t	net_message_own;

void NET_AcceptStale (qsocket_t *sock, int on)
{
	if (sock)
		sock->qvrAcceptStale = on != 0;
}

void NET_RestoreMessage (void)
{
	if (!net_message_own.data)
		return;
	net_message = net_message_own;
	SZ_Clear (&net_message);
	msg_readcount = 0;
	msg_badread = false;
}
int		net_activeconnections		= 0;

int		messagesSent			= 0;
int		messagesReceived		= 0;
int		unreliableMessagesSent		= 0;
int		unreliableMessagesReceived	= 0;

static	cvar_t	net_messagetimeout = {"net_messagetimeout","300",CVAR_NONE};
cvar_t	hostname = {"hostname", "UNNAMED", CVAR_NONE};

// these two macros are to make the code more readable
#define sfunc	net_drivers[sock->driver]
#define dfunc	net_drivers[net_driverlevel]

int		net_driverlevel;

double		net_time;


double SetNetTime (void)
{
	net_time = Sys_DoubleTime();
	return net_time;
}


/*
===================
NET_NewQSocket

Called by drivers when a new communications endpoint is required
The sequence and buffer fields will be filled in properly
===================
*/
qsocket_t *NET_NewQSocket (void)
{
	qsocket_t	*sock;

	if (net_freeSockets == NULL)
		return NULL;

	if (net_activeconnections >= svs.maxclients)
		return NULL;

	// get one from free list
	sock = net_freeSockets;
	net_freeSockets = sock->next;

	// add it to active list
	sock->next = net_activeSockets;
	net_activeSockets = sock;

	sock->disconnected = false;
	sock->connecttime = net_time;
	Q_strcpy (sock->address,"UNSET ADDRESS");
	sock->driver = net_driverlevel;
	sock->socket = 0;
	sock->qvrShared = false; // QVR: (B23)
	sock->qvrHeard = true; // QVR: (mpfix A1) (false for a server's new client: net_dgrm.c's connect request)
	sock->qvrClosing = false; // QVR: (mpfix A2)
	memset (&sock->qvrIn, 0, sizeof (sock->qvrIn));
	sock->driverdata = NULL;
	sock->canSend = true;
	sock->sendNext = false;
	sock->lastMessageTime = net_time;
	sock->ackSequence = 0;
	sock->sendSequence = 0;
	sock->sendChunkBase = 0; // QVR
	sock->reliableRtt = 0; // QVR (net_dgrm.c: the reliable window's resends)
	sock->aheadAcks = 0;
	sock->resendBackoff = 0;
	sock->fastResent = false;
	sock->qvrNetVersion = 0; // QVR
	sock->qvrName[0] = 0; // QVR
	sock->qvrKey[0] = 0; // QVR: (B25)
	sock->qvrUnreliableRead = sock->qvrUnreliableSkipped = sock->qvrResent = 0; // QVR: (B13) the net HUD's
	sock->qvrAcceptStale = false; sock->qvrStaleRead = 0; // QVR: (B20) SV_ReadClientMessage opts in for a remote client
	sock->qvrBytesIn = sock->qvrBytesOut = 0;
	sock->qvrUnreliableTime = net_time; // QVR: (review 2026-11-18, finding 2)
	sock->qvrSeeded = false;
	sock->qvrFirstOut = 0;
	sock->unreliableSendSequence = 0;
	sock->sendMessageLength = 0;
	sock->receiveSequence = 0;
	sock->unreliableReceiveSequence = 0;
	sock->receiveMessageLength = 0;
	memset (sock->chunkAcked, 0, sizeof (sock->chunkAcked)); // QVR (B13)
	memset (sock->heldLength, 0, sizeof (sock->heldLength));

	return sock;
}


void NET_FreeQSocket(qsocket_t *sock)
{
	qsocket_t	*s;

	// remove it from active list
	if (sock == net_activeSockets)
		net_activeSockets = net_activeSockets->next;
	else
	{
		for (s = net_activeSockets; s; s = s->next)
		{
			if (s->next == sock)
			{
				s->next = sock->next;
				break;
			}
		}

		if (!s)
			Sys_Error ("NET_FreeQSocket: not active");
	}

	// add it to free list
	sock->next = net_freeSockets;
	net_freeSockets = sock;
	sock->disconnected = true;
}


double NET_QSocketGetTime (const qsocket_t *s)
{
	return s->connecttime;
}


// QVR: the player's name its connect request carried ("": none; net_dgrm.c)
const char *NET_QSocketGetQvrName (const qsocket_t *s)
{
	return s->qvrName;
}

// QVR: (B25) the vr_mp_clientkey its connect request carried ("": none, an older client's; net_dgrm.c)
const char *NET_QSocketGetQvrKey (const qsocket_t *s)
{
	return s->qvrKey;
}

const char *NET_QSocketGetAddressString (const qsocket_t *s)
{
	return s->address;
}

// QVR: the one test of "the listen host's own client": the loopback's server side, which net_loop.c names "LOCAL"
// (its client side is "localhost"). SV_IsLocalClient for a client; this where only the socket is at hand.
qboolean NET_QSocketIsLocal (const qsocket_t *s)
{
	return s && Q_strncmp (s->address, "LOCAL", 5) == 0; // QVR: (splitscreen: any seat's, "LOCAL <n>": a loopback)
}

int NET_QSocketLocalSeat (const qsocket_t *s)
{
	return NET_QSocketIsLocal (s) ? Loop_SocketSeat (s) : -1;
}


static void NET_Listen_f (void)
{
	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"listen\" is \"%d\"\n", listening ? 1 : 0);
		return;
	}

	listening = Q_atoi(Cmd_Argv(1)) ? true : false;

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.Listen (listening);
	}
}


static void MaxPlayers_f (void)
{
	int	n;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"maxplayers\" is \"%d\"\n", svs.maxclients);
		return;
	}

	if (sv.active)
	{
		Con_Printf ("maxplayers can not be changed while a server is running.\n");
		return;
	}

	n = Q_atoi(Cmd_Argv(1));
	if (n < 1)
		n = 1;
	if (n > svs.maxclientslimit)
	{
		n = svs.maxclientslimit;
		Con_Printf ("\"maxplayers\" set to \"%d\"\n", n);
	}

	if ((n == 1) && listening)
		Cbuf_AddText ("listen 0\n");

	if ((n > 1) && (!listening))
		Cbuf_AddText ("listen 1\n");

	svs.maxclients = n;
	if (n == 1)
		Cvar_Set ("deathmatch", "0");
	else
		Cvar_Set ("deathmatch", "1");
}


static void NET_Port_f (void)
{
	int	n;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"port\" is \"%d\"\n", net_hostport);
		return;
	}

	n = Q_atoi(Cmd_Argv(1));
	if (n < 1 || n > 65534)
	{
		Con_Printf ("Bad value, must be between 1 and 65534\n");
		return;
	}

	DEFAULTnet_hostport = n;
	net_hostport = n;

	if (listening)
	{
		// force a change to the new port
		Cbuf_AddText ("listen 0\n");
		Cbuf_AddText ("listen 1\n");
	}
}


static void PrintSlistHeader(void)
{
	Con_Printf("Server          Map             Users\n");
	Con_Printf("--------------- --------------- -----\n");
	slistLastShown = 0;
}


// QVR: (netmismatch) " needs Quake VR 1.2.0-beta7" after a server of another Quake VR network version, else ""
static const char *SlistMismatch (int n)
{
	if (!hostcache[n].qvrNetVersion || hostcache[n].qvrNetVersion == NET_QvrOwnVersion ())
		return "";
	if (hostcache[n].qvrRelease[0])
		return va (" needs Quake VR %s", hostcache[n].qvrRelease);
	return va (" needs network version %d", hostcache[n].qvrNetVersion);
}

static void PrintSlist(void)
{
	int		n;

	for (n = slistLastShown; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
			Con_Printf("%-15.15s %-15.15s %2u/%2u%s\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers,
				SlistMismatch (n));
		else
			Con_Printf("%-15.15s %-15.15s%s\n", hostcache[n].name, hostcache[n].map, SlistMismatch (n));
	}
	slistLastShown = n;
}


static void PrintSlistTrailer(void)
{
	if (hostCacheCount)
		Con_Printf("== end list ==\n\n");
	else
		Con_Printf("No Quake servers found.\n\n");
}


void NET_Slist_f (void)
{
	if (slistInProgress)
		return;

	if (! slistSilent)
	{
		Con_Printf("Looking for Quake servers...\n");
		PrintSlistHeader();
	}

	slistInProgress = true;
	slistStartTime = Sys_DoubleTime();

	SchedulePollProcedure(&slistSendProcedure, 0.0);
	SchedulePollProcedure(&slistPollProcedure, 0.0); // QVR: (B13) from the next frame on (0.1 s before: the pings)

	hostCacheCount = 0;
}


void NET_SlistSort (void)
{
	if (hostCacheCount > 1)
	{
		int	i, j;
		hostcache_t temp;
		for (i = 0; i < hostCacheCount; i++)
		{
			for (j = i + 1; j < hostCacheCount; j++)
			{
				if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
				{
					memcpy(&temp, &hostcache[j], sizeof(hostcache_t));
					memcpy(&hostcache[j], &hostcache[i], sizeof(hostcache_t));
					memcpy(&hostcache[i], &temp, sizeof(hostcache_t));
				}
			}
		}
	}
}


const char *NET_SlistPrintServer (int idx)
{
	static char	string[64];

	if (idx < 0 || idx >= hostCacheCount)
		return "";

	if (hostcache[idx].maxusers)
	{
		q_snprintf(string, sizeof(string), "%-15.15s %-15.15s %2u/%2u\n",
					hostcache[idx].name, hostcache[idx].map,
					hostcache[idx].users, hostcache[idx].maxusers);
	}
	else
	{
		q_snprintf(string, sizeof(string), "%-15.15s %-15.15s\n",
					hostcache[idx].name, hostcache[idx].map);
	}

	return string;
}


const char *NET_SlistPrintServerName (int idx)
{
	if (idx < 0 || idx >= hostCacheCount)
		return "";
	return hostcache[idx].cname;
}


#include "vr/iw/net_main_slist.c" // QVR: the server browser's entries and Quake VR's versions (NET_SlistEntry...) (moved out as it was)
static void Slist_Send (void *unused)
{
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistLocal && IS_LOOP_DRIVER(net_driverlevel))
			continue;
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.SearchForHosts (true);
	}

	if ((Sys_DoubleTime() - slistStartTime) < 0.5)
		SchedulePollProcedure(&slistSendProcedure, 0.75);
}


static void Slist_Poll (void *unused)
{
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistLocal && IS_LOOP_DRIVER(net_driverlevel))
			continue;
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.SearchForHosts (false);
	}

	if (! slistSilent)
		PrintSlist();

	if ((Sys_DoubleTime() - slistStartTime) < 1.5)
	{
		// QVR: (B13) read at every frame's poll (0.1 s apart before): the server browser's pings are the replies'
		// round trips (vr_serverbrowser.cpp)
		SchedulePollProcedure(&slistPollProcedure, 0.0);
		return;
	}

	if (! slistSilent)
		PrintSlistTrailer();
	slistInProgress = false;
	slistSilent = false;
	slistLocal = true;
	VR_BrowserSearchDone (); // QVR: (B13) its rows printed if asked (vr_mp_browse)
}


/*
===================
NET_Connect
===================
*/

int hostCacheCount = 0;
hostcache_t hostcache[HOSTCACHESIZE];

qsocket_t *NET_Connect (const char *host)
{
	qsocket_t		*ret;
	int				n;
	int				numdrivers = net_numdrivers;

	SetNetTime();
	net_connectrefusal[0] = 0; // QVR: (netmismatch) a refusal of this connect's, by the datagram driver

	if (host && *host == 0)
		host = NULL;

	if (host)
	{
		if (q_strcasecmp (host, "local") == 0)
		{
			numdrivers = 1;
			goto JustDoIt;
		}

		if (hostCacheCount)
		{
			for (n = 0; n < hostCacheCount; n++)
				if (q_strcasecmp (host, hostcache[n].name) == 0)
				{
					host = hostcache[n].cname;
					break;
				}
			if (n < hostCacheCount)
				goto JustDoIt;
		}
	}

	slistSilent = host ? true : false;
	NET_Slist_f ();

	while (slistInProgress)
		NET_Poll();

	if (host == NULL)
	{
		if (hostCacheCount != 1)
			return NULL;
		host = hostcache[0].cname;
		Con_Printf("Connecting to...\n%s @ %s\n\n", hostcache[0].name, host);
	}

	if (hostCacheCount)
	{
		for (n = 0; n < hostCacheCount; n++)
		{
			if (q_strcasecmp (host, hostcache[n].name) == 0)
			{
				host = hostcache[n].cname;
				break;
			}
		}
	}

JustDoIt:
	for (net_driverlevel = 0; net_driverlevel < numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		ret = dfunc.Connect (host);
		if (ret)
			return ret;
	}

	if (host && !net_connectrefusal[0]) // QVR: (netmismatch) not after a refusal: the server answered, with why
	{
		Con_Printf("\n");
		PrintSlistHeader();
		PrintSlist();
		PrintSlistTrailer();
	}

	return NULL;
}


/*
===================
NET_CheckNewConnections
===================
*/
qsocket_t *NET_CheckNewConnections (void)
{
	qsocket_t	*ret;

	SetNetTime();

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		if (!IS_LOOP_DRIVER(net_driverlevel) && listening == false)
			continue;
		ret = dfunc.CheckNewConnections ();
		if (ret)
		{
			return ret;
		}
	}

	return NULL;
}

/*
===================
NET_Close
===================
*/
void NET_Close (qsocket_t *sock)
{
	if (!sock)
		return;

	if (sock->disconnected)
		return;

	SetNetTime();

	// call the driver_Close function
	sfunc.Close (sock);

	NET_FreeQSocket(sock);
}


/*
=================
NET_GetMessage

If there is a complete message, return it in net_message

returns 0 if no data is waiting
returns 1 if a message was received
returns -1 if connection is invalid
=================
*/
int	NET_GetMessage (qsocket_t *sock)
{
	VR_TRACY_SCOPE("Network receive");
	int ret;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_GetMessage: disconnected socket\n");
		return -1;
	}
	if (sock->qvrClosing) // QVR: (mpfix A2) closed by its driver: its owner drops it (NET_Close frees it)
		return -1;

	SetNetTime();
	net_stalemessage = false; // QVR: (B20) set by the driver for an overtaken datagram it still reads

	ret = sfunc.QGetMessage(sock);

	// see if this connection has timed out
	if (ret == 0 && !IS_LOOP_DRIVER(sock->driver))
	{
		if (net_time - sock->lastMessageTime > net_messagetimeout.value)
		{
			NET_Close(sock);
			return -1;
		}
		// QVR: (mpfix A1) a server's client that never acked a chunk (a forged or lost connect: a slot held for
		// net_messagetimeout's 300 s otherwise)
		if (!sock->qvrHeard && net_time - sock->connecttime > QVR_UNHEARD_TIMEOUT)
		{
			Con_Printf ("%s never answered its accept in %.0f s: dropped\n", sock->address, QVR_UNHEARD_TIMEOUT);
			NET_Close(sock);
			return -1;
		}
	}

	if (ret > 0)
	{
		if (!IS_LOOP_DRIVER(sock->driver))
		{
			sock->lastMessageTime = net_time;
			if (ret == 1)
				messagesReceived++;
			else if (ret == 2)
				unreliableMessagesReceived++;
		}
	}

	return ret;
}


/*
==================
NET_SendMessage

Try to send a complete length+message unit over the reliable stream.
returns 0 if the message cannot be delivered reliably, but the connection
		is still considered valid
returns 1 if the message was sent properly
returns -1 if the connection died
==================
*/
int NET_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	int		r;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();
	r = sfunc.QSendMessage(sock, data);
	if (r == 1 && !IS_LOOP_DRIVER(sock->driver))
		messagesSent++;

	return r;
}


int NET_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int		r;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();
	r = sfunc.SendUnreliableMessage(sock, data);
	if (r == 1 && !IS_LOOP_DRIVER(sock->driver))
		unreliableMessagesSent++;

	return r;
}


/*
==================
NET_CanSendMessage

Returns true or false if the given qsocket can currently accept a
message to be transmitted.
==================
*/
qboolean NET_CanSendMessage (qsocket_t *sock)
{
	if (!sock)
		return false;

	if (sock->disconnected)
		return false;

	SetNetTime();

	return sfunc.CanSendMessage(sock);
}


int NET_SendToAll (sizebuf_t *data, double blocktime)
{
	double		start;
	int			i;
	int			count = 0;
	qboolean	msg_init[MAX_SCOREBOARD];	/* did we write the message to the client's connection	*/
	qboolean	msg_sent[MAX_SCOREBOARD];	/* did the msg arrive its destination (canSend state).	*/

	for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
	{
		/*
		if (!host_client->netconnection)
			continue;
		if (host_client->active)
		*/
		if (host_client->netconnection && host_client->active)
		{
			if (IS_LOOP_DRIVER(host_client->netconnection->driver))
			{
				NET_SendMessage(host_client->netconnection, data);
				msg_init[i] = true;
				msg_sent[i] = true;
				continue;
			}
			count++;
			msg_init[i] = false;
			msg_sent[i] = false;
		}
		else
		{
			msg_init[i] = true;
			msg_sent[i] = true;
		}
	}

	start = Sys_DoubleTime();
	while (count)
	{
		count = 0;
		for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
		{
			if (! msg_init[i])
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					msg_init[i] = true;
					NET_SendMessage(host_client->netconnection, data);
				}
				else
				{
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}

			if (! msg_sent[i])
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					msg_sent[i] = true;
				}
				else
				{
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}
		}
		if ((Sys_DoubleTime() - start) > blocktime)
			break;
	}
	return count;
}


//=============================================================================

/*
====================
NET_Init
====================
*/

void NET_Init (void)
{
	int			i;
	qsocket_t	*s;

	net_loopbackonly = COM_CheckParm ("-loopbackonly") != 0; // QVR: (B24, tests) before the drivers start
	if (net_loopbackonly)
		Con_SafePrintf ("NET: -loopbackonly: 127.0.0.0/8 only\n");

	i = COM_CheckParm ("-port");
	if (!i)
		i = COM_CheckParm ("-udpport");
	if (!i)
		i = COM_CheckParm ("-ipxport");

	if (i)
	{
		if (i < com_argc-1)
			DEFAULTnet_hostport = Q_atoi (com_argv[i+1]);
		else
			Sys_Error ("NET_Init: you must specify a number after -port");
	}
	net_hostport = DEFAULTnet_hostport;

	net_numsockets = svs.maxclientslimit;
	if (cls.state != ca_dedicated)
		net_numsockets += MAX_SPLITSEATS; // QVR: splitscreen: each local seat's client side of its loopback pair (was 1: seat 0's; seats 3-4 found none)
	if (COM_CheckParm("-listen") || cls.state == ca_dedicated)
		listening = true;

	SetNetTime();

	for (i = 0; i < net_numsockets; i++)
	{
		s = (qsocket_t *)Hunk_AllocName(sizeof(qsocket_t), "qsocket");
		s->next = net_freeSockets;
		net_freeSockets = s;
		s->disconnected = true;
	}

	// allocate space for network message buffer
	SZ_Alloc (&net_message, NET_MAXMESSAGE);
	net_message_own = net_message; // QVR

	Cvar_RegisterVariable (&net_messagetimeout);
	Cvar_RegisterVariable (&hostname);

	Cmd_AddCommand ("slist", NET_Slist_f);
	Cmd_AddCommand ("listen", NET_Listen_f);
	Cmd_AddCommand ("maxplayers", MaxPlayers_f);
	Cmd_AddCommand ("port", NET_Port_f);

	// initialize all the drivers
	for (i = net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].Init() == -1)
			continue;
		i++;
		net_drivers[net_driverlevel].initialized = true;
		if (listening)
			net_drivers[net_driverlevel].Listen (true);
	}

	/* Loop_Init() returns -1 for dedicated server case,
	 * therefore the i == 0 check is correct */
	if (i == 0
			&& cls.state == ca_dedicated
	   )
	{
		Sys_Error("Network not available!");
	}

	if (*my_ipx_address)
	{
		Con_DPrintf("IPX address %s\n", my_ipx_address);
	}
	if (*my_tcpip_address)
	{
		Con_DPrintf("TCP/IP address %s\n", my_tcpip_address);
	}

#ifndef WITHOUT_CURL
	curl_global_init (CURL_GLOBAL_DEFAULT);
#endif
}

/*
====================
NET_Shutdown
====================
*/

void NET_Shutdown (void)
{
	qsocket_t	*sock;

#ifndef WITHOUT_CURL
	curl_global_cleanup ();
#endif
	SetNetTime();

	for (sock = net_activeSockets; sock; sock = sock->next)
		NET_Close(sock);

//
// shutdown the drivers
//
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == true)
		{
			net_drivers[net_driverlevel].Shutdown ();
			net_drivers[net_driverlevel].initialized = false;
		}
	}
}


static PollProcedure *pollProcedureList = NULL;

void NET_Poll(void)
{
	PollProcedure *pp;

	SetNetTime();

	for (pp = pollProcedureList; pp; pp = pp->next)
	{
		if (pp->nextTime > net_time)
			break;
		pollProcedureList = pp->next;
		pp->procedure(pp->arg);
	}
}


void SchedulePollProcedure(PollProcedure *proc, double timeOffset)
{
	PollProcedure *pp, *prev;

	proc->nextTime = Sys_DoubleTime() + timeOffset;
	for (pp = pollProcedureList, prev = NULL; pp; pp = pp->next)
	{
		if (pp->nextTime >= proc->nextTime)
			break;
		prev = pp;
	}

	if (prev == NULL)
	{
		proc->next = pollProcedureList;
		pollProcedureList = proc;
		return;
	}

	proc->next = pp;
	prev->next = proc;
}


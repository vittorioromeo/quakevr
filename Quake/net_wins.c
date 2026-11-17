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

static sys_socket_t net_acceptsocket = INVALID_SOCKET;	// socket for fielding new connections
static sys_socket_t net_controlsocket;
static sys_socket_t net_broadcastsocket = 0;
static struct sockaddr_in broadcastaddr;

static in_addr_t	myAddr;

// QVR: (B23, tests) -natgateway <port>: every datagram by way of the multiplayer harness's NAT simulator on
// 127.0.0.1:<port> (Misc/quakevr/mp/mp_nat.py), the real address before it in 6 bytes (IPv4 and port, network order)
// both ways; a datagram from anywhere else is dropped (only the NAT reaches this game)
static struct sockaddr_in	natGateway;
static byte	natBuffer[NET_DATAGRAMSIZE + 6];

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET	_WSAIOW(IOC_VENDOR, 12)
#endif

#include "net_wins.h"

int winsock_initialized = 0;
WSADATA		winsockdata;
#define __wsaerr_static			/* not static: used by net_wipx.c too */
#include "wsaerror.h"

//=============================================================================

#if !defined(_USE_WINSOCK2)
static double	blocktime;

static INT_PTR PASCAL FAR BlockingHook (void)
{
	MSG	msg;
	BOOL	ret;

	if ((Sys_DoubleTime() - blocktime) > 2.0)
	{
		WSACancelBlockingCall();
		return FALSE;
	}

	/* get the next message, if any */
	ret = (BOOL) PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);

	/* if we got one, process it */
	if (ret)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	/* TRUE if we got a message */
	return ret;
}
#endif	/* ! _USE_WINSOCK2 */


static void WINS_GetLocalAddress (void)
{
	struct hostent	*local = NULL;
	char		buff[MAXHOSTNAMELEN];
	in_addr_t	addr;
	int		err;

	if (myAddr != INADDR_ANY)
		return;

	if (gethostname(buff, MAXHOSTNAMELEN) == SOCKET_ERROR)
	{
		err = SOCKETERRNO;
		Con_SafePrintf("WINS_GetLocalAddress: WARNING: gethostname failed (%s)\n",
						socketerror(err));
		return;
	}

	buff[MAXHOSTNAMELEN - 1] = 0;
#ifndef _USE_WINSOCK2
	blocktime = Sys_DoubleTime();
	WSASetBlockingHook(BlockingHook);
#endif
	local = gethostbyname(buff);
	err = WSAGetLastError();
#ifndef _USE_WINSOCK2
	WSAUnhookBlockingHook();
#endif
	if (local == NULL)
	{
		Con_SafePrintf("WINS_GetLocalAddress: gethostbyname failed (%s)\n",
							__WSAE_StrError(err));
		return;
	}

	myAddr = *(in_addr_t *)local->h_addr_list[0];

	addr = ntohl(myAddr);
	sprintf(my_tcpip_address, "%ld.%ld.%ld.%ld", (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
}


sys_socket_t WINS_Init (void)
{
	int	i, err;
	char	buff[MAXHOSTNAMELEN];

	if (COM_CheckParm ("-noudp"))
		return -1;

	i = COM_CheckParm ("-natgateway"); // QVR: (B23, tests)
	if (i && i + 1 < com_argc)
	{
		natGateway.sin_family = AF_INET;
		natGateway.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
		natGateway.sin_port = htons ((unsigned short) Q_atoi (com_argv[i + 1]));
		Con_SafePrintf ("UDP: every datagram through the NAT simulator on 127.0.0.1:%s\n", com_argv[i + 1]);
	}

	if (winsock_initialized == 0)
	{
		err = WSAStartup(MAKEWORD(1,1), &winsockdata);
		if (err != 0)
		{
			Con_SafePrintf("Winsock initialization failed (%s)\n",
					socketerror(err));
			return INVALID_SOCKET;
		}
	}
	winsock_initialized++;

	// determine my name & address
	if (gethostname(buff, MAXHOSTNAMELEN) != 0)
	{
		err = SOCKETERRNO;
		Con_SafePrintf("WINS_Init: WARNING: gethostname failed (%s)\n",
						socketerror(err));
	}
	else
	{
		buff[MAXHOSTNAMELEN - 1] = 0;
	}
	i = COM_CheckParm ("-ip");
	if (i)
	{
		if (i < com_argc-1)
		{
			myAddr = inet_addr(com_argv[i + 1]);
			if (myAddr == INADDR_NONE)
				Sys_Error ("%s is not a valid IP address", com_argv[i + 1]);
			strcpy(my_tcpip_address, com_argv[i + 1]);
		}
		else
		{
			Sys_Error ("NET_Init: you must specify an IP address after -ip");
		}
	}
	else
	{
		myAddr = INADDR_ANY;
		strcpy(my_tcpip_address, "INADDR_ANY");
	}

	if ((net_controlsocket = WINS_OpenSocket(0)) == INVALID_SOCKET)
	{
		Con_SafePrintf("WINS_Init: Unable to open control socket, UDP disabled\n");
		if (--winsock_initialized == 0)
			WSACleanup ();
		return INVALID_SOCKET;
	}

	broadcastaddr.sin_family = AF_INET;
	broadcastaddr.sin_addr.s_addr = INADDR_BROADCAST;
	broadcastaddr.sin_port = htons((unsigned short)net_hostport);

	Con_SafePrintf("UDP Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

//=============================================================================

void WINS_Shutdown (void)
{
	WINS_Listen (false);
	WINS_CloseSocket (net_controlsocket);
	if (--winsock_initialized == 0)
		WSACleanup ();
}

//=============================================================================

void WINS_Listen (qboolean state)
{
	// enable listening
	if (state)
	{
		if (net_acceptsocket != INVALID_SOCKET)
			return;
		WINS_GetLocalAddress();
		if ((net_acceptsocket = WINS_OpenSocket (net_hostport)) == INVALID_SOCKET)
			Sys_Error ("WINS_Listen: Unable to open accept socket");
		// QVR: (B23) every client's datagrams come to this socket now (net_dgrm.c): a client gone makes Windows report
		// the ICMP "port unreachable" of the server's next datagram to him as an error of this socket's next read, which
		// would cut the read of everyone else's datagrams short -- off; and room for a few frames of 16 clients' moves
		{
			BOOL	report = FALSE;
			DWORD	unused = 0;
			int	size = 1 << 20;
			WSAIoctl (net_acceptsocket, SIO_UDP_CONNRESET, &report, sizeof (report), NULL, 0, &unused, NULL, NULL);
			setsockopt (net_acceptsocket, SOL_SOCKET, SO_RCVBUF, (const char *) &size, sizeof (size));
		}
		return;
	}

	// disable listening
	if (net_acceptsocket == INVALID_SOCKET)
		return;
	WINS_CloseSocket (net_acceptsocket);
	net_acceptsocket = INVALID_SOCKET;
}

//=============================================================================

sys_socket_t WINS_OpenSocket (int port)
{
	sys_socket_t newsocket;
	struct sockaddr_in address;
	u_long _true = 1;
	int err;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		err = SOCKETERRNO;
		Con_SafePrintf("WINS_OpenSocket: %s\n", socketerror(err));
		return INVALID_SOCKET;
	}

	if (ioctlsocket (newsocket, FIONBIO, &_true) == SOCKET_ERROR)
		goto ErrorReturn;

	memset(&address, 0, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = myAddr;
	address.sin_port = htons((unsigned short)port);
	if (bind (newsocket, (struct sockaddr *)&address, sizeof(address)) == 0)
		return newsocket;

	if (tcpipAvailable)
	{
		err = SOCKETERRNO;
		Sys_Error ("Unable to bind to %s (%s)",
				WINS_AddrToString ((struct qsockaddr *) &address),
				socketerror(err));
		return INVALID_SOCKET;	/* not reached */
	}
	/* else: we are still in init phase, no need to error */

ErrorReturn:
	err = SOCKETERRNO;
	Con_SafePrintf("WINS_OpenSocket: %s\n", socketerror(err));
	closesocket (newsocket);
	return INVALID_SOCKET;
}

//=============================================================================

int WINS_CloseSocket (sys_socket_t socketid)
{
	if (socketid == net_broadcastsocket)
		net_broadcastsocket = 0;
	return closesocket (socketid);
}

//=============================================================================

/*
============
PartialIPAddress

this lets you type only as much of the net address as required, using
the local network components to fill in the rest
============
*/
static int PartialIPAddress (const char *in, struct qsockaddr *hostaddr)
{
	char	buff[256];
	char	*b;
	int	addr, mask, num, port, run;

	buff[0] = '.';
	b = buff;
	strcpy(buff+1, in);
	if (buff[1] == '.')
		b++;

	addr = 0;
	mask = -1;
	while (*b == '.')
	{
		b++;
		num = 0;
		run = 0;
		while (!( *b < '0' || *b > '9'))
		{
			num = num*10 + *b++ - '0';
			if (++run > 3)
				return -1;
		}
		if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0)
			return -1;
		if (num < 0 || num > 255)
			return -1;
		mask <<= 8;
		addr = (addr<<8) + num;
	}

	if (*b++ == ':')
		port = Q_atoi(b);
	else
		port = net_hostport;

	hostaddr->qsa_family = AF_INET;
	((struct sockaddr_in *)hostaddr)->sin_port = htons((unsigned short)port);
	((struct sockaddr_in *)hostaddr)->sin_addr.s_addr =
					(myAddr & htonl(mask)) | htonl(addr);

	return 0;
}

//=============================================================================

int WINS_Connect (sys_socket_t socketid, struct qsockaddr *addr)
{
	return 0;
}

//=============================================================================

sys_socket_t WINS_CheckNewConnections (void)
{
	// QVR: (B23) the listening socket whenever listening: net_dgrm.c reads it for every client, and keeps the host's room
	// (NetQuake's peek of 4096 bytes also failed on a larger datagram, which then blocked the socket)
	return net_acceptsocket;
}

//=============================================================================

int WINS_Read (sys_socket_t socketid, byte *buf, int len, struct qsockaddr *addr)
{
	socklen_t addrlen = sizeof(struct qsockaddr);
	int ret;

	while (natGateway.sin_port) // QVR: (B23, tests) through the NAT simulator
	{
		struct sockaddr_in	from;
		socklen_t	fromlen = sizeof (from);
		ret = recvfrom (socketid, (char *) natBuffer, sizeof (natBuffer), 0, (struct sockaddr *) &from, &fromlen);
		if (ret == SOCKET_ERROR)
		{
			int err = SOCKETERRNO;
			if (err == NET_EWOULDBLOCK || err == NET_ECONNREFUSED || err == WSAECONNRESET || err == WSAEMSGSIZE)
				return 0;
			Con_SafePrintf ("WINS_Read, recvfrom: %s\n", socketerror(err));
			return ret;
		}
		if (ret < 6 || from.sin_port != natGateway.sin_port || from.sin_addr.s_addr != natGateway.sin_addr.s_addr)
			continue;
		memset (addr, 0, sizeof (*addr));
		addr->qsa_family = AF_INET;
		memcpy (&((struct sockaddr_in *) addr)->sin_addr, natBuffer, 4);
		memcpy (&((struct sockaddr_in *) addr)->sin_port, natBuffer + 4, 2);
		ret = q_min (ret - 6, len);
		memcpy (buf, natBuffer + 6, ret);
		return ret;
	}

	ret = recvfrom (socketid, (char *)buf, len, 0, (struct sockaddr *)addr, &addrlen);
	if (ret == SOCKET_ERROR)
	{
		int err = SOCKETERRNO;
		if (err == NET_EWOULDBLOCK || err == NET_ECONNREFUSED)
			return 0;
		Con_SafePrintf ("WINS_Read, recvfrom: %s\n", socketerror(err));
	}
	return ret;
}

//=============================================================================

static int WINS_MakeSocketBroadcastCapable (sys_socket_t socketid)
{
	int	i = 1;

	// make this socket broadcast capable
	if (setsockopt(socketid, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i))
								 == SOCKET_ERROR)
	{
		int err = SOCKETERRNO;
		Con_SafePrintf ("UDP, setsockopt: %s\n", socketerror(err));
		return -1;
	}
	net_broadcastsocket = socketid;

	return 0;
}

//=============================================================================

int WINS_Broadcast (sys_socket_t socketid, byte *buf, int len)
{
	int	ret;

	if (socketid != net_broadcastsocket)
	{
		if (net_broadcastsocket != 0)
			Sys_Error("Attempted to use multiple broadcasts sockets");
		WINS_GetLocalAddress();
		ret = WINS_MakeSocketBroadcastCapable (socketid);
		if (ret == -1)
		{
			Con_Printf("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return WINS_Write (socketid, buf, len, (struct qsockaddr *)&broadcastaddr);
}

//=============================================================================

int WINS_Write (sys_socket_t socketid, byte *buf, int len, struct qsockaddr *addr)
{
	int	ret;

	if (net_loopbackonly && !NET_LoopbackAddr (((struct sockaddr_in *)addr)->sin_addr.s_addr)) // QVR: (B24, tests)
	{
		if (((struct sockaddr_in *)addr)->sin_addr.s_addr != INADDR_BROADCAST) // (a LAN broadcast: dropped quietly)
			NET_LoopbackRefused (WINS_AddrToString (addr));
		return len; // (as if sent and lost)
	}

	if (natGateway.sin_port && len >= 0 && len <= NET_DATAGRAMSIZE) // QVR: (B23, tests) through the NAT simulator
	{
		memcpy (natBuffer, &((struct sockaddr_in *) addr)->sin_addr, 4);
		memcpy (natBuffer + 4, &((struct sockaddr_in *) addr)->sin_port, 2);
		memcpy (natBuffer + 6, buf, len);
		ret = sendto (socketid, (char *) natBuffer, len + 6, 0, (struct sockaddr *) &natGateway, sizeof (natGateway));
		return ret == SOCKET_ERROR ? 0 : len;
	}

	ret = sendto (socketid, (char *)buf, len, 0, (struct sockaddr *)addr,
							sizeof(struct qsockaddr));
	if (ret == SOCKET_ERROR)
	{
		int err = SOCKETERRNO;
		if (err == NET_EWOULDBLOCK)
			return 0;
		Con_SafePrintf ("WINS_Write, sendto: %s\n", socketerror(err));
	}
	return ret;
}

//=============================================================================

const char *WINS_AddrToString (struct qsockaddr *addr)
{
	static char buffer[22];
	int		haddr;

	haddr = ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr);
	sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff,
			  (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff,
			  ntohs(((struct sockaddr_in *)addr)->sin_port));
	return buffer;
}

//=============================================================================

int WINS_StringToAddr (const char *string, struct qsockaddr *addr)
{
	int	ha1, ha2, ha3, ha4, hp, ipaddr;

	sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
	ipaddr = (ha1 << 24) | (ha2 << 16) | (ha3 << 8) | ha4;

	addr->qsa_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_addr.s_addr = htonl(ipaddr);
	((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)hp);
	return 0;
}

//=============================================================================

int WINS_GetSocketAddr (sys_socket_t socketid, struct qsockaddr *addr)
{
	socklen_t addrlen = sizeof(struct qsockaddr);
	in_addr_t a;

	memset(addr, 0, sizeof(struct qsockaddr));
	getsockname(socketid, (struct sockaddr *)addr, &addrlen);

	a = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
	if (a == 0 || a == htonl(INADDR_LOOPBACK))
		((struct sockaddr_in *)addr)->sin_addr.s_addr = myAddr;

	return 0;
}

//=============================================================================

int WINS_GetNameFromAddr (struct qsockaddr *addr, char *name)
{
	struct hostent *hostentry;

	hostentry = net_loopbackonly ? NULL // QVR: (B24, tests) no reverse lookups
		: gethostbyaddr ((char *)&((struct sockaddr_in *)addr)->sin_addr, sizeof(struct in_addr), AF_INET);
	if (hostentry)
	{
		Q_strncpy (name, (char *)hostentry->h_name, NET_NAMELEN - 1);
		return 0;
	}

	Q_strcpy (name, WINS_AddrToString (addr));
	return 0;
}

//=============================================================================

int WINS_GetAddrFromName (const char *name, struct qsockaddr *addr)
{
	struct hostent *hostentry;

	if (name[0] >= '0' && name[0] <= '9')
	{
		if (PartialIPAddress (name, addr) == -1)
			return -1;
		if (net_loopbackonly && !NET_LoopbackAddr (((struct sockaddr_in *)addr)->sin_addr.s_addr)) // QVR: (B24, tests)
		{
			NET_LoopbackRefused (name);
			return -1;
		}
		return 0;
	}

	if (net_loopbackonly && q_strcasecmp (name, "localhost")) // QVR: (B24, tests) never looked up
	{
		NET_LoopbackRefused (name);
		return -1;
	}

	hostentry = gethostbyname (name);
	if (!hostentry)
		return -1;

	addr->qsa_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)net_hostport);
	((struct sockaddr_in *)addr)->sin_addr.s_addr =
						*(in_addr_t *)hostentry->h_addr_list[0];

	return 0;
}

//=============================================================================

int WINS_AddrCompare (struct qsockaddr *addr1, struct qsockaddr *addr2)
{
	if (addr1->qsa_family != addr2->qsa_family)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_addr.s_addr !=
	    ((struct sockaddr_in *)addr2)->sin_addr.s_addr)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_port !=
	    ((struct sockaddr_in *)addr2)->sin_port)
		return 1;

	return 0;
}

//=============================================================================

int WINS_GetSocketPort (struct qsockaddr *addr)
{
	return ntohs(((struct sockaddr_in *)addr)->sin_port);
}


int WINS_SetSocketPort (struct qsockaddr *addr, int port)
{
	((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)port);
	return 0;
}

//=============================================================================


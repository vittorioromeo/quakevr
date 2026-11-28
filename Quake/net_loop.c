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
#include "net_loop.h"

// QVR: splitscreen (docs/vr-port/SPLITSCREEN.md, phase 6): a loopback pair a seat (clientseat_t's loopclient,
// loopserver, loopconnect), so up to four local players each have a connection of their own into the server, as
// network players do. Loop_Connect makes the current seat's; the server's side takes them in seat order. Seat 0's
// server side is "LOCAL" as before, seat n's "LOCAL <n+1>" (NET_QSocketIsLocal: any of them; NET_QSocketLocalSeat).

// the seat a loop socket (either side) belongs to (-1: none)
static int Loop_SeatOf (const qsocket_t *sock)
{
	int s;
	clientseat_t *net;

	for (s = 0; s < CL_SeatCount (); s++)
	{
		net = CL_SeatNetIfMade (s);
		if (net && sock && (net->loopclient == sock || net->loopserver == sock))
			return s;
	}
	return -1;
}

int Loop_Init (void)
{
	if (cls.state == ca_dedicated)
		return -1;
	return 0;
}


void Loop_Shutdown (void)
{
}


void Loop_Listen (qboolean state)
{
}


void Loop_SearchForHosts (qboolean xmit)
{
	if (!sv.active)
		return;

	hostCacheCount = 1;
	if (Q_strcmp(hostname.string, "UNNAMED") == 0)
		Q_strcpy(hostcache[0].name, "local");
	else
		Q_strcpy(hostcache[0].name, hostname.string);
	Q_strcpy(hostcache[0].map, sv.name);
	hostcache[0].users = net_activeconnections;
	hostcache[0].maxusers = svs.maxclients;
	hostcache[0].driver = net_driverlevel;
	hostcache[0].qvrNetVersion = 0; // QVR: (netmismatch) this game's own server: always joinable
	hostcache[0].qvrRelease[0] = 0;
	Q_strcpy(hostcache[0].cname, "local");
}


qsocket_t *Loop_Connect (const char *host)
{
	clientseat_t *net = cl_seat; // QVR: the current seat's pair
	const int seat = CL_SeatIndex ();

	if (Q_strcmp(host,"local") != 0)
		return NULL;

	net->loopconnect = true;

	if (!net->loopclient)
	{
		if ((net->loopclient = NET_NewQSocket ()) == NULL)
		{
			Con_Printf("Loop_Connect: no qsocket available\n");
			return NULL;
		}
		Q_strcpy (net->loopclient->address, "localhost");
	}
	net->loopclient->receiveMessageLength = 0;
	net->loopclient->sendMessageLength = 0;
	net->loopclient->canSend = true;

	if (!net->loopserver)
	{
		if ((net->loopserver = NET_NewQSocket ()) == NULL)
		{
			Con_Printf("Loop_Connect: no qsocket available\n");
			return NULL;
		}
		if (seat == 0)
			Q_strcpy (net->loopserver->address, "LOCAL");
		else
			q_snprintf (net->loopserver->address, sizeof (net->loopserver->address), "LOCAL %d", seat + 1);
	}
	net->loopserver->receiveMessageLength = 0;
	net->loopserver->sendMessageLength = 0;
	net->loopserver->canSend = true;
	if (seat > 0) // QVR: the seat's player's name, as a network player's connect request carries it (VR_CoopPickSlot)
	{
		CL_SeatName (seat, net->loopserver->qvrName, sizeof (net->loopserver->qvrName));
		// split20: and a game key (next's mplows A11: a rejoin record needs one; a loopback seat's cannot be forged from
		// the network): the seat's, so the same seat coming back gets his things again (vr_coopsave.cpp)
		q_snprintf (net->loopserver->qvrKey, sizeof (net->loopserver->qvrKey), "local-seat%d", seat + 1);
	}

	net->loopclient->driverdata = (void *)net->loopserver;
	net->loopserver->driverdata = (void *)net->loopclient;

	return net->loopclient;
}


qsocket_t *Loop_CheckNewConnections (void)
{
	int s;
	clientseat_t *net;

	for (s = 0; s < CL_SeatCount (); s++) // QVR: each seat's connect waiting, in seat order
	{
		net = CL_SeatNetIfMade (s);
		if (!net || !net->loopconnect || !net->loopserver || !net->loopclient)
			continue;
		net->loopconnect = false;
		net->loopserver->sendMessageLength = 0;
		net->loopserver->receiveMessageLength = 0;
		net->loopserver->canSend = true;
		net->loopclient->sendMessageLength = 0;
		net->loopclient->receiveMessageLength = 0;
		net->loopclient->canSend = true;
		return net->loopserver;
	}
	return NULL;
}

// QVR: the seat a loop socket's server side is (0: the host's own, "LOCAL"; 1-3: a splitscreen seat's; -1: not one)
int Loop_SocketSeat (const qsocket_t *sock)
{
	const int s = Loop_SeatOf (sock);
	clientseat_t *net = s >= 0 ? CL_SeatNetIfMade (s) : NULL;

	return net && net->loopserver == sock ? s : -1;
}


static int IntAlign(int value)
{
	return (value + (sizeof(int) - 1)) & (~(sizeof(int) - 1));
}


int Loop_GetMessage (qsocket_t *sock)
{
	int		ret;
	int		length;

	if (sock->receiveMessageLength == 0)
		return 0;

	ret = sock->receiveMessage[0];
	length = sock->receiveMessage[1] + (sock->receiveMessage[2] << 8);
	// alignment byte skipped here
	SZ_Clear (&net_message);
	SZ_Write (&net_message, &sock->receiveMessage[4], length);

	length = IntAlign(length + 4);
	sock->receiveMessageLength -= length;

	if (sock->receiveMessageLength)
		memmove (sock->receiveMessage, &sock->receiveMessage[length], sock->receiveMessageLength);

	if (sock->driverdata && ret == 1)
		((qsocket_t *)sock->driverdata)->canSend = true;

	return ret;
}


int Loop_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	byte *buffer;
	int  *bufferLength;

	if (!sock->driverdata)
		return -1;

	bufferLength = &((qsocket_t *)sock->driverdata)->receiveMessageLength;

	if ((*bufferLength + data->cursize + 4) > NET_MAXMESSAGE)
		Sys_Error("Loop_SendMessage: overflow");

	buffer = ((qsocket_t *)sock->driverdata)->receiveMessage + *bufferLength;

	// message type
	*buffer++ = 1;

	// length
	*buffer++ = data->cursize & 0xff;
	*buffer++ = data->cursize >> 8;

	// align
	buffer++;

	// message
	Q_memcpy(buffer, data->data, data->cursize);
	*bufferLength = IntAlign(*bufferLength + data->cursize + 4);

	sock->canSend = false;
	return 1;
}


int Loop_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	byte *buffer;
	int  *bufferLength;

	if (!sock->driverdata)
		return -1;

	bufferLength = &((qsocket_t *)sock->driverdata)->receiveMessageLength;

	if ((*bufferLength + data->cursize + sizeof(byte) + sizeof(short)) > NET_MAXMESSAGE)
		return 0;

	buffer = ((qsocket_t *)sock->driverdata)->receiveMessage + *bufferLength;

	// message type
	*buffer++ = 2;

	// length
	*buffer++ = data->cursize & 0xff;
	*buffer++ = data->cursize >> 8;

	// align
	buffer++;

	// message
	Q_memcpy(buffer, data->data, data->cursize);
	*bufferLength = IntAlign(*bufferLength + data->cursize + 4);
	return 1;
}


qboolean Loop_CanSendMessage (qsocket_t *sock)
{
	if (!sock->driverdata)
		return false;
	return sock->canSend;
}


qboolean Loop_CanSendUnreliableMessage (qsocket_t *sock)
{
	return true;
}


void Loop_Close (qsocket_t *sock)
{
	const int s = Loop_SeatOf (sock);
	clientseat_t *net = s >= 0 ? CL_SeatNetIfMade (s) : NULL;

	if (sock->driverdata)
		((qsocket_t *)sock->driverdata)->driverdata = NULL;
	sock->receiveMessageLength = 0;
	sock->sendMessageLength = 0;
	sock->canSend = true;
	if (!net)
		return;
	if (sock == net->loopclient)
		net->loopclient = NULL;
	else
		net->loopserver = NULL;
}


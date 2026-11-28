// QVR: the server browser's entries and Quake VR's versions (NET_SlistEntry...).
// Moved as it was out of net_main.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

// QVR: (B13) the server browser's (net.h)
int NET_SlistEntry (int n, const char **name, const char **map, const char **address, int *users, int *maxusers,
					int *ping, int *version)
{
	if (n < 0 || n >= hostCacheCount)
		return 0;
	*name = hostcache[n].name;
	*map = hostcache[n].map;
	*address = hostcache[n].cname;
	*users = hostcache[n].users;
	*maxusers = hostcache[n].maxusers;
	*ping = hostcache[n].ping;
	*version = hostcache[n].version;
	return 1;
}

// QVR: (netmismatch) a search reply's Quake VR part (net.h)
int NET_SlistQvr (int n, int *netVersion, const char **release)
{
	if (n < 0 || n >= hostCacheCount)
		return 0;
	*netVersion = hostcache[n].qvrNetVersion;
	*release = hostcache[n].qvrRelease;
	return 1;
}

int NET_QvrOwnVersion (void)
{
	const int test = (int) Cvar_VariableValue ("vr_net_test_version");
	return test ? test : QVR_NET_VERSION;
}

static char net_ownrelease[40];		// NET_QvrRelease's // seat-shared: the release string
static char net_refusaltext[400];	// NET_ConnectRefusal's // seat-shared: the last refusal's text
char net_connectrefusal[256]; // seat-shared: the last connect refusal (the process's)

const char *NET_QvrRelease (void)
{
	q_snprintf (net_ownrelease, sizeof (net_ownrelease), "%s%s", VR_Version (), VR_VersionIsDev () ? "-dev" : "");
	return net_ownrelease;
}

// (frozen: the servers from 1.2.0-beta7 on say their release in their search replies and refusals)
const char *NET_QvrReleaseOf (int netVersion)
{
	switch (netVersion)
	{
	case 42: return "1.2.0-beta1";
	case 44: return "1.2.0-beta2";
	case 46: return "1.2.0-beta3 to 1.2.0-beta5";
	case 48: return "1.2.0-beta6";
	default: return "";
	}
}

const char *NET_ConnectRefusal (void)
{
	// a server before 1.2.0-beta7 names only the network versions ("This Quake VR server has network version 46, your
	// game 48: join with the same Quake VR version."): the release that is said instead
	const char *oldText = "This Quake VR server has network version ";
	const size_t oldLen = strlen (oldText);
	const char *release;
	int theirs;

	q_strlcpy (net_refusaltext, net_connectrefusal, sizeof (net_refusaltext));
	if (strncmp (net_connectrefusal, oldText, oldLen) == 0)
	{
		theirs = atoi (net_connectrefusal + oldLen);
		release = NET_QvrReleaseOf (theirs);
		if (*release)
			q_snprintf (net_refusaltext, sizeof (net_refusaltext), "This server runs Quake VR %s (network version %d), "
				"your game %s (network version %d): join with Quake VR %s.", release, theirs, NET_QvrRelease (),
				NET_QvrOwnVersion (), release);
	}
	return net_refusaltext;
}

// QVR: (B13) the net HUD's (net.h)
void NET_QvrLinkCounts (struct qsocket_s *sock, unsigned int counts[5], double *reliableRtt)
{
	counts[0] = sock->qvrUnreliableRead;
	counts[1] = sock->qvrUnreliableSkipped;
	counts[2] = sock->qvrResent;
	counts[3] = sock->qvrBytesIn;
	counts[4] = sock->qvrBytesOut;
	*reliableRtt = sock->reliableRtt;
}

// QVR: (B16) a client's vanished server (vr_nethud.cpp; net.h)
double NET_QvrSilence (struct qsocket_s *sock)
{
	if (!sock || IS_LOOP_DRIVER (sock->driver))
		return 0.0;
	return SetNetTime () - sock->lastMessageTime;
}



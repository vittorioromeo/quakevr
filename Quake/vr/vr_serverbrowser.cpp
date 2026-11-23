// vr_serverbrowser.cpp -- the server browser (MP_PLAN.md, B13): the LAN's servers with the favourites and the servers
// joined last, in one list (menu.c's server list: Multiplayer > Join Game > Server Browser).
//
// NetQuake's search (net_main.c's slist) broadcasts a server-info request to the LAN; besides it each favourite and
// recent server is asked by its address (net_dgrm.c, VR_BrowserAddress), so a server on the internet answers too. A
// reply's ping is its round trip. The rows are built again from the replies and the two lists (vr_mp_favourites,
// vr_mp_recent: addresses separated by ';') whenever they are drawn: the favourites first, then the recent servers, then
// the LAN's others; a listed server that did not answer shows its address and "no reply". Joining a server (here, at the
// Join Game page's address or with connect) puts it first in the recent list (VR_MpNoteConnect).
//
// Commands: vr_mp_browse (a search; its rows printed when it ends: "mp_browser:", then "mp_browser_done:"),
// vr_mp_browse join|favourite|forget <row> (what the menu's keys do), vr_mp_favourite [<address>] (one toggled by its
// address; none: the lists printed).

#include "vr_serverbrowser.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strlen.hpp"

#include <string.h>

namespace qvr::browser
{

namespace
{

constexpr int addressLength = 64;
constexpr int maxFavourites = 16;
constexpr int maxRecent = 8;
constexpr int maxRows = maxFavourites + maxRecent + 32; // (32: net_defs.h's HOSTCACHESIZE)
constexpr int rowTextLength = 48;
constexpr int netProtocolVersion = 3; // (net_defs.h's NET_PROTOCOL_VERSION: a reply of another is not joinable)

struct AddressList
{
    char items[maxFavourites + maxRecent][addressLength]{};
    int count{0};
};

struct Row
{
    char address[addressLength]{};
    int reply{-1}; // NET_SlistEntry's (-1: none)
    bool favourite{false};
    bool recent{false};
};

struct State
{
    Row rows[maxRows];
    int count{0};
    AddressList asked; // the favourites and recent servers (VR_BrowserAddress), as the last count took them
    bool printPending{false};
    char text[rowTextLength]{};
};
State state;

// An address as kept: no spaces, quotes or ';' (the lists' separator), at most addressLength - 1 characters.
void clean(const char* in, char* out)
{
    int n = 0;
    for(; *in && n < addressLength - 1; in++)
    {
        if(*in != ' ' && *in != '\t' && *in != '"' && *in != ';' && *in != '\n' && *in != '\r')
        {
            out[n++] = *in;
        }
    }
    out[n] = 0;
}

[[nodiscard]] int find(const AddressList& l, const char* address)
{
    for(int i = 0; i < l.count; i++)
    {
        if(!q_strcasecmp(l.items[i], address))
        {
            return i;
        }
    }
    return -1;
}

void parse(const char* text, AddressList& out, int max)
{
    out.count = 0;
    char item[addressLength];
    while(*text && out.count < max)
    {
        const char* end = strchr(text, ';');
        const int len = end ? static_cast<int>(end - text) : static_cast<int>(ZA_STRLEN(text));
        char raw[addressLength];
        q_strlcpy(raw, text, static_cast<za::SizeT>(q_min(len + 1, addressLength)));
        clean(raw, item);
        if(item[0] && find(out, item) < 0)
        {
            q_strlcpy(out.items[out.count++], item, addressLength);
        }
        text += len + (end ? 1 : 0);
    }
}

void store(const AddressList& l, cvar_t& var)
{
    char text[(maxFavourites + maxRecent) * addressLength];
    text[0] = 0;
    for(int i = 0; i < l.count; i++)
    {
        if(i > 0)
        {
            q_strlcat(text, ";", sizeof(text));
        }
        q_strlcat(text, l.items[i], sizeof(text));
    }
    Cvar_Set(var.name, text);
}

void removeAt(AddressList& l, int i)
{
    for(; i + 1 < l.count; i++)
    {
        q_strlcpy(l.items[i], l.items[i + 1], addressLength);
    }
    l.count--;
}

void rebuild()
{
    AddressList favourites, recent;
    parse(vr_mp_favourites.string, favourites, maxFavourites);
    parse(vr_mp_recent.string, recent, maxRecent);
    state.count = 0;
    for(int i = 0; i < favourites.count; i++)
    {
        Row& r = state.rows[state.count++];
        r = Row{};
        q_strlcpy(r.address, favourites.items[i], addressLength);
        r.favourite = true;
        r.recent = find(recent, r.address) >= 0;
    }
    for(int i = 0; i < recent.count; i++)
    {
        if(find(favourites, recent.items[i]) >= 0)
        {
            continue;
        }
        Row& r = state.rows[state.count++];
        r = Row{};
        q_strlcpy(r.address, recent.items[i], addressLength);
        r.recent = true;
    }
    const char *name, *map, *address;
    int users, maxusers, ping, version;
    for(int n = 0; NET_SlistEntry(n, &name, &map, &address, &users, &maxusers, &ping, &version); n++)
    {
        int at = 0;
        while(at < state.count && q_strcasecmp(state.rows[at].address, address))
        {
            at++;
        }
        if(at == state.count)
        {
            if(state.count == maxRows)
            {
                continue;
            }
            Row& r = state.rows[state.count++];
            r = Row{};
            q_strlcpy(r.address, address, addressLength);
        }
        state.rows[at].reply = n;
    }
}

[[nodiscard]] bool valid(int row)
{
    return row >= 0 && row < state.count;
}

void printRows()
{
    rebuild();
    int answered = 0;
    for(int i = 0; i < state.count; i++)
    {
        const Row& r = state.rows[i];
        const char *name = "", *map = "", *address;
        int users = 0, maxusers = 0, ping = -1, version = 0;
        if(r.reply >= 0 && NET_SlistEntry(r.reply, &name, &map, &address, &users, &maxusers, &ping, &version))
        {
            answered++;
        }
        Con_Printf("mp_browser: row=%d address=%s favourite=%d recent=%d answered=%d users=%d max=%d ping=%d version=%d "
                   "map=%s name=%s\n",
            i, r.address, r.favourite ? 1 : 0, r.recent ? 1 : 0, r.reply >= 0 ? 1 : 0, users, maxusers, ping, version,
            map[0] ? map : "-", name);
    }
    Con_Printf("mp_browser_done: rows=%d answered=%d\n", state.count, answered);
}

void browse_f()
{
    if(Cmd_Argc() >= 3)
    {
        rebuild();
        const int row = Q_atoi(Cmd_Argv(2));
        if(!valid(row))
        {
            Con_Printf("vr_mp_browse: no row %d (%d rows)\n", row, state.count);
            return;
        }
        const char* what = Cmd_Argv(1);
        if(!q_strcasecmp(what, "join"))
        {
            VR_BrowserJoin(row);
        }
        else if(!q_strcasecmp(what, "favourite"))
        {
            VR_BrowserToggleFavourite(row);
        }
        else if(!q_strcasecmp(what, "forget"))
        {
            VR_BrowserForget(row);
        }
        else
        {
            Con_Printf("vr_mp_browse [join|favourite|forget <row>]\n");
        }
        return;
    }
    state.printPending = true;
    VR_BrowserRefresh();
}

void toggleFavourite(const char* typed)
{
    char address[addressLength];
    clean(typed, address);
    if(!address[0])
    {
        return;
    }
    AddressList favourites;
    parse(vr_mp_favourites.string, favourites, maxFavourites);
    const int at = find(favourites, address);
    if(at >= 0)
    {
        removeAt(favourites, at);
    }
    else if(favourites.count == maxFavourites)
    {
        Con_Printf("The favourite servers are full (%d): take one off first.\n", maxFavourites);
        return;
    }
    else
    {
        q_strlcpy(favourites.items[favourites.count++], address, addressLength);
    }
    store(favourites, vr_mp_favourites);
    Con_Printf("mp_browser: favourite %s %s\n", address, at >= 0 ? "off" : "on");
}

void favourite_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("mp_favourites: %s\nmp_recent: %s\n", vr_mp_favourites.string, vr_mp_recent.string);
        return;
    }
    toggleFavourite(Cmd_Argv(1));
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_mp_browse", browse_f);
    Cmd_AddCommand("vr_mp_favourite", favourite_f);
}

void noteConnected(const char* host)
{
    char address[addressLength];
    clean(host, address);
    if(!address[0] || !q_strcasecmp(address, "local"))
    {
        return;
    }
    AddressList recent;
    parse(vr_mp_recent.string, recent, maxRecent);
    const int at = find(recent, address);
    if(at >= 0)
    {
        removeAt(recent, at);
    }
    else if(recent.count == maxRecent)
    {
        recent.count--;
    }
    for(int i = recent.count; i > 0; i--)
    {
        q_strlcpy(recent.items[i], recent.items[i - 1], addressLength);
    }
    q_strlcpy(recent.items[0], address, addressLength);
    recent.count++;
    store(recent, vr_mp_recent);
}

} // namespace qvr::browser

using namespace qvr;
using namespace qvr::browser;

extern "C" int VR_BrowserAddressCount(void)
{
    // (the favourites, then the recent servers that are not: taken once a search's send, read by VR_BrowserAddress)
    AddressList recent;
    parse(vr_mp_favourites.string, state.asked, maxFavourites);
    parse(vr_mp_recent.string, recent, maxRecent);
    for(int i = 0; i < recent.count; i++)
    {
        if(find(state.asked, recent.items[i]) < 0)
        {
            q_strlcpy(state.asked.items[state.asked.count++], recent.items[i], addressLength);
        }
    }
    return state.asked.count;
}

extern "C" const char* VR_BrowserAddress(int i)
{
    return i >= 0 && i < state.asked.count ? state.asked.items[i] : "";
}

extern "C" void VR_BrowserSearchDone(void)
{
    if(state.printPending)
    {
        state.printPending = false;
        printRows();
    }
}

extern "C" void VR_BrowserRefresh(void)
{
    if(slistInProgress)
    {
        return;
    }
    slistSilent = true;
    slistLocal = false;
    NET_Slist_f();
}

extern "C" int VR_BrowserSearching(void)
{
    return slistInProgress ? 1 : 0;
}

extern "C" int VR_BrowserCount(void)
{
    rebuild();
    return state.count;
}

extern "C" const char* VR_BrowserRowText(int row)
{
    if(!valid(row))
    {
        return "";
    }
    const Row& r = state.rows[row];
    const char *name, *map, *address;
    int users, maxusers, ping, version;
    if(r.reply >= 0 && NET_SlistEntry(r.reply, &name, &map, &address, &users, &maxusers, &ping, &version))
    {
        char pingText[8];
        if(version != netProtocolVersion)
        {
            q_strlcpy(pingText, " old", sizeof(pingText)); // (another NetQuake protocol: not joinable)
        }
        else
        {
            q_snprintf(pingText, sizeof(pingText), "%4d", q_min(ping, 9999));
        }
        q_snprintf(state.text, sizeof(state.text), "%-14.14s %-8.8s %2d/%-2d %s", name, map, users, maxusers, pingText);
    }
    else
    {
        q_snprintf(state.text, sizeof(state.text), "%-24.24s %9s", r.address, slistInProgress ? "..." : "no reply");
    }
    return state.text;
}

extern "C" int VR_BrowserRowFavourite(int row)
{
    return valid(row) && state.rows[row].favourite ? 1 : 0;
}

extern "C" int VR_BrowserRowRecent(int row)
{
    return valid(row) && state.rows[row].recent ? 1 : 0;
}

extern "C" void VR_BrowserToggleFavourite(int row)
{
    if(!valid(row))
    {
        return;
    }
    toggleFavourite(state.rows[row].address);
    rebuild();
}

extern "C" void VR_BrowserForget(int row)
{
    if(!valid(row) || !state.rows[row].recent)
    {
        return;
    }
    AddressList recent;
    parse(vr_mp_recent.string, recent, maxRecent);
    const int at = find(recent, state.rows[row].address);
    if(at >= 0)
    {
        removeAt(recent, at);
        store(recent, vr_mp_recent);
        Con_Printf("mp_browser: forgot %s\n", state.rows[row].address);
    }
    rebuild();
}

extern "C" void VR_BrowserJoin(int row)
{
    if(!valid(row))
    {
        return;
    }
    Cbuf_AddText(va("connect \"%s\"\n", state.rows[row].address));
}

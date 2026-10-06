// vr_relight_maps.cpp -- see vr_relight_maps.hpp.

#include "vr_relight_maps.hpp"

#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mapinstall.hpp"

#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/String/StringView.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

extern "C" {
int VR_MapGameFolder(const char* name, char* out, size_t size); // vr_gamedir.cpp
}

namespace qvr::relight::maps
{

namespace
{

// The games in the order a batch takes them (the rest after, by name; the Library's last).
constexpr const char* gameOrder[] = {"id1", "hipnotic", "rogue", "dopa", "mg1", "mg3"};
constexpr int libraryRank = 1000;

[[nodiscard]] int gameRank(const za::String& game, bool library)
{
    if(library)
    {
        return libraryRank;
    }
    for(int i = 0; i < static_cast<int>(sizeof(gameOrder) / sizeof(gameOrder[0])); i++)
    {
        if(!q_strcasecmp(game.cStr(), gameOrder[i]))
        {
            return i;
        }
    }
    return static_cast<int>(sizeof(gameOrder) / sizeof(gameOrder[0]));
}

// The folder name of a path ("C:/q/id1": "id1"; "C:/q/id1/pak0.pak": "id1"), as vr_gamedir.cpp's gameFolderName.
[[nodiscard]] za::String folderName(const char* path, bool isPak)
{
    za::StringView p{path};
    if(isPak)
    {
        p = files::parentPath(p);
    }
    while(!p.empty() && (p[p.size() - 1] == '/' || p[p.size() - 1] == '\\'))
    {
        p = p.substrByPosLen(0, p.size() - 1);
    }
    return za::String{files::fileName(p)};
}

// "maps/<name>.bsp" (any case): <name>, lower case; false for another file, and for the brush models (b_*.bsp).
[[nodiscard]] bool mapName(const char* path, za::String& out)
{
    const size_t n = strlen(path);
    if(n <= 9 || q_strncasecmp(path, "maps/", 5) || q_strcasecmp(path + n - 4, ".bsp"))
    {
        return false;
    }
    out.clear();
    for(size_t i = 5; i < n - 4; i++)
    {
        out.pushBack(static_cast<char>(tolower(static_cast<unsigned char>(path[i]))));
    }
    const char* base = strrchr(out.cStr(), '/');
    base = base ? base + 1 : out.cStr();
    return !(base[0] == 'b' && base[1] == '_');
}

// e1m2 before e1m10: the digits' runs compared as numbers.
[[nodiscard]] int naturalCompare(const char* a, const char* b)
{
    while(*a && *b)
    {
        if(isdigit(static_cast<unsigned char>(*a)) && isdigit(static_cast<unsigned char>(*b)))
        {
            while(*a == '0')
            {
                a++;
            }
            while(*b == '0')
            {
                b++;
            }
            size_t na = 0, nb = 0;
            while(isdigit(static_cast<unsigned char>(a[na])))
            {
                na++;
            }
            while(isdigit(static_cast<unsigned char>(b[nb])))
            {
                nb++;
            }
            if(na != nb)
            {
                return na < nb ? -1 : 1;
            }
            if(const int c = strncmp(a, b, na))
            {
                return c;
            }
            a += na;
            b += nb;
            continue;
        }
        if(*a != *b)
        {
            return static_cast<unsigned char>(*a) < static_cast<unsigned char>(*b) ? -1 : 1;
        }
        a++;
        b++;
    }
    return *a ? 1 : *b ? -1 : 0;
}

// A map one can play: its entities have a place to start (info_player_*). Not the brush models a game folder keeps
// in maps/ (quakevr's buttons and prop tables, a mod's ammo boxes not named b_*). Only the entity lump is read.
[[nodiscard]] bool playable(const Source& s)
{
    FILE* f = Sys_fopen(s.file.cStr(), "rb");
    if(!f)
    {
        return false;
    }
    unsigned char header[12];
    bool ok = fseek(f, static_cast<long>(s.offset), SEEK_SET) == 0 && fread(header, 1, sizeof(header), f) == sizeof(header);
    int at = 0, length = 0;
    if(ok)
    {
        memcpy(&at, header + 4, 4);
        memcpy(&length, header + 8, 4);
        at = LittleLong(at);
        length = LittleLong(length);
        ok = at > 0 && length > 0 && length < (16 << 20) && static_cast<za::I64>(at) + length <= s.length;
    }
    za::Vector<char> text;
    if(ok)
    {
        text.resize(static_cast<za::SizeT>(length) + 1, '\0');
        ok = fseek(f, static_cast<long>(s.offset + at), SEEK_SET) == 0 &&
             fread(text.data(), 1, static_cast<size_t>(length), f) == static_cast<size_t>(length);
    }
    fclose(f);
    return ok && strstr(text.data(), "info_player_") != nullptr;
}

struct Found
{
    Source source;
    int rank{0};
};

[[nodiscard]] bool seen(const za::Vector<Found>& all, const za::String& game, const za::String& map)
{
    for(const Found& f : all)
    {
        if(f.source.map == map && !q_strcasecmp(f.source.game.cStr(), game.cStr()))
        {
            return true;
        }
    }
    return false;
}

// Every map of the game folders on the search path (each searched as the engine does: the first of a name in a game
// wins), and of the Library's installed packages.
void everyMap(za::Vector<Found>& all, bool searchPaths, bool library)
{
    if(searchPaths)
    {
        for(const searchpath_t* s = com_searchpaths; s; s = s->next)
        {
            za::String name;
            if(s->pack)
            {
                const za::String game = folderName(s->pack->filename, true);
                for(int i = 0; i < s->pack->numfiles; i++)
                {
                    const packfile_t& f = s->pack->files[i];
                    if(mapName(f.name, name) && !seen(all, game, name))
                    {
                        all.pushBack(Found{Source{name, game, za::String{s->pack->filename}, f.filepos, f.filelen, true},
                            gameRank(game, false)});
                    }
                }
                continue;
            }
            const za::String game = folderName(s->filename, false);
            const za::String dir = files::join(za::StringView{s->filename}, za::StringView{"maps"});
            files::forEachEntry(dir.cStr(), [&](const char* entry, bool isDirectory) {
                if(isDirectory || !mapName(va("maps/%s", entry), name) || seen(all, game, name))
                {
                    return;
                }
                const za::String file = files::join(dir, za::StringView{entry});
                all.pushBack(Found{Source{name, game, file, 0, static_cast<za::I64>(files::fileSize(file.cStr())), false},
                    gameRank(game, false)});
            });
        }
    }
    if(library)
    {
        for(const mapinstall::Installed& p : mapinstall::installedList())
        {
            const za::String folder = mapinstall::packageFolder(p.sha);
            const za::String game = folderName(folder.cStr(), false);
            za::Vector<za::String> names;
            mapinstall::packageMaps(p.sha, names);
            for(const za::String& n : names)
            {
                const za::String file = files::join(folder, za::StringView{va("maps/%s.bsp", n.cStr())});
                const char* base = strrchr(n.cStr(), '/');
                base = base ? base + 1 : n.cStr();
                if((base[0] == 'b' && base[1] == '_') || !files::isFile(file.cStr()) || seen(all, game, n))
                {
                    continue;
                }
                all.pushBack(Found{Source{n, game, file, 0, static_cast<za::I64>(files::fileSize(file.cStr())), false},
                    libraryRank});
            }
        }
    }
}

// The map in play's game folder ("" : none).
[[nodiscard]] za::String currentGame(const char* current)
{
    char game[MAX_QPATH];
    if(!current || !current[0] || !VR_MapGameFolder(va("maps/%s.bsp", current), game, sizeof(game)))
    {
        return {};
    }
    return za::String{game};
}

[[nodiscard]] bool startsWithEpisode(const za::String& map, const za::String& prefix)
{
    if(map.size() <= prefix.size() || q_strncasecmp(map.cStr(), prefix.cStr(), prefix.size()))
    {
        return false;
    }
    for(za::SizeT i = prefix.size(); i < map.size(); i++)
    {
        if(!isdigit(static_cast<unsigned char>(map[i])))
        {
            return false;
        }
    }
    return true;
}

} // namespace

za::String episodePrefix(const char* map)
{
    // letters, digits, 'm', digits: e1m3, hip2m4, r1m7 (and dopa's e5m1).
    const char* p = map;
    while(isalpha(static_cast<unsigned char>(*p)))
    {
        p++;
    }
    if(p == map || !isdigit(static_cast<unsigned char>(*p)))
    {
        return {};
    }
    while(isdigit(static_cast<unsigned char>(*p)))
    {
        p++;
    }
    if(*p != 'm' && *p != 'M')
    {
        return {};
    }
    const char* digits = p + 1;
    const char* q = digits;
    while(isdigit(static_cast<unsigned char>(*q)))
    {
        q++;
    }
    if(q == digits || *q)
    {
        return {};
    }
    za::String out{map, static_cast<za::SizeT>(digits - map)};
    for(char& c : out)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool locate(const char* map, Source& out)
{
    const char* name = va("maps/%s.bsp", map);
    if(!COM_FileExists(name, nullptr) || !com_filesource[0])
    {
        return false;
    }
    char source[MAX_OSPATH];
    q_strlcpy(source, com_filesource, sizeof(source));
    const char* ext = COM_FileGetExtension(source);
    const bool isPak = !q_strcasecmp(ext, "pak");
    out = Source{};
    out.map = za::String{map};
    for(char& c : out.map)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    out.game = folderName(source, isPak);
    if(!isPak)
    {
        out.file = files::join(za::StringView{source}, za::StringView{name});
        out.length = static_cast<za::I64>(files::fileSize(out.file.cStr()));
        return out.length > 0;
    }
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        if(!s->pack || strcmp(s->pack->filename, source))
        {
            continue;
        }
        for(int i = 0; i < s->pack->numfiles; i++)
        {
            if(!q_strcasecmp(s->pack->files[i].name, name))
            {
                out.file = za::String{source};
                out.offset = s->pack->files[i].filepos;
                out.length = s->pack->files[i].filelen;
                out.inPak = true;
                return true;
            }
        }
    }
    return false;
}

bool read(const Source& source, za::Vector<unsigned char>& out)
{
    if(!source.inPak)
    {
        return files::readBytes(source.file.cStr(), out);
    }
    FILE* f = Sys_fopen(source.file.cStr(), "rb");
    if(!f)
    {
        return false;
    }
    out.resize(static_cast<za::SizeT>(source.length));
    const bool ok = fseek(f, static_cast<long>(source.offset), SEEK_SET) == 0 &&
                    fread(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

bool collect(Set set, const char* arg, const char* current, za::Vector<Source>& out, za::String& why)
{
    out.clear();
    const bool named = arg && arg[0];
    za::Vector<Found> all;
    if(set == Set::Map)
    {
        Source s;
        if(!current || !current[0] || !locate(current, s))
        {
            why = "no map loaded";
            return false;
        }
        out.pushBack(ZA_MOVE(s));
        return true;
    }
    everyMap(all, set != Set::Library, set == Set::Library || set == Set::Everything);

    za::String game, prefix;
    if(set == Set::Episode)
    {
        if(named)
        {
            // "e2" (or "e2m"): the first game, in the batch's order, with maps of it.
            prefix = za::String{arg};
            for(char& c : prefix)
            {
                c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            }
            if(prefix.empty() || prefix[prefix.size() - 1] != 'm')
            {
                prefix += "m";
            }
            int best = libraryRank + 1;
            for(const Found& f : all)
            {
                if(f.rank < best && startsWithEpisode(f.source.map, prefix))
                {
                    best = f.rank;
                    game = f.source.game;
                }
            }
            if(game.empty())
            {
                why = za::String{va("no maps of episode %s", arg)};
                return false;
            }
        }
        else
        {
            prefix = current ? episodePrefix(current) : za::String{};
            game = currentGame(current);
            if(prefix.empty() || game.empty())
            {
                why = current && current[0] ? za::String{va("%s is in no episode: pick one (E1 to E4)", current)}
                                            : za::String{"no map loaded: pick an episode"};
                return false;
            }
        }
    }
    else if(set == Set::Game)
    {
        game = named ? za::String{arg} : currentGame(current);
        if(game.empty())
        {
            why = "no map loaded: pick a game";
            return false;
        }
    }

    // The games' order (the Library's last), then the names'.
    za::stableSort(all.begin(), all.end(), [](const Found& a, const Found& b) {
        if(a.rank != b.rank)
        {
            return a.rank < b.rank;
        }
        if(const int c = q_strcasecmp(a.source.game.cStr(), b.source.game.cStr()))
        {
            return c < 0;
        }
        return naturalCompare(a.source.map.cStr(), b.source.map.cStr()) < 0;
    });
    for(const Found& f : all)
    {
        if(!game.empty() && q_strcasecmp(f.source.game.cStr(), game.cStr()))
        {
            continue;
        }
        if(!prefix.empty() && !startsWithEpisode(f.source.map, prefix))
        {
            continue;
        }
        if(playable(f.source))
        {
            out.pushBack(f.source);
        }
    }
    if(out.empty())
    {
        why = set == Set::Library  ? za::String{"no maps installed from the Map Library"}
              : !game.empty()      ? za::String{va("no maps of %s found", game.cStr())}
                                   : za::String{"no maps found"};
        return false;
    }
    return true;
}

} // namespace qvr::relight::maps

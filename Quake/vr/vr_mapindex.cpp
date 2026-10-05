// vr_mapindex.cpp -- the external map index: Quaddicted's own package index (no curated manifest of ours), fetched at
// start-up on a thread of its own, cached on disk, parsed with the engine's json.c into the slim model in
// vr_mapindex.hpp, and read from the console (maps_list / maps_info / maps_stats / maps_fetch) until the VR map
// browser exists. Nothing here installs or extracts anything.
//
// The API (measured, 2026-10): `https://www.quaddicted.com/api/v1/?q=*:*&rows=N&start=N` returns a bare JSON array.
// One unpaginated call does return the whole index (1947 packages, 18.4 MB, 1.6 s) — but json.c builds one
// jsonentry_t per token, so that payload alone is a ~44 MB parse tree. A page of 200 is ~1.9 MB with a ~5 MB tree,
// the pages come in one stable order (checked against the unpaginated call), and a page that fails costs only itself:
// so the index is paged, and the cache is written in our own slim line format rather than as the JSON we were given
// (1 MB rather than 18 MB, and reading it back needs no JSON parser and no big tree).
//
// The cache lives in the user's game dir — the same place the installed maps will go: quakevr/cache/maps_index.txt.
// It is not in git. -nomapindex, or vr_maps_fetch 0, keeps the fetch off; a failed one says so in one console line
// and leaves the game running with no index.

#include "vr_mapindex.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mem.hpp"
#include "vr_zancle.hpp"

#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

extern "C"
{
#include "json.h" // (the engine's C header: as vr_engine.hpp includes the others)
}

#include <ctype.h>
#include <string.h>
#include <time.h>

namespace qvr::mapindex
{
namespace
{

constexpr int pageRows = 200;             // packages per API call
constexpr int maxPages = 30;              // a guard: 10 cover the index today
constexpr za::I64 cacheSeconds = 24 * 60 * 60; // an index older than this is fetched again (host_cmd.c's MANIFEST_RETENTION)
constexpr int listLimit = 20;             // maps_list's default
constexpr char cacheMagic[] = "#quakevr-mapindex-3";
const char* acceptHeader = "Accept: application/json"; // (file-scope, not a function-local static: download_t.headers wants a const char**)

// The live index, counted by vr_memstats (mem::Never: only the fetch thread's handoff replaces it). Main thread.
mem::Cache<Index> indexSet{"map index", mem::Never};

// The handoff: the fetch thread fills it, poll() takes it on the main thread (the console is never written from the
// fetch thread, as host_cmd.c's Modlist_DownloadJSON does with Host_InvokeOnMainThread).
za::AtomicMutex handoff;
za::UniquePtr<Index> pending;
za::String pendingStatus;
za::Atomic<bool> pendingReady{false};

// The fetch thread (one pass per start()). `cancel` is the engine's Download API's abort flag.
za::Thread worker;
za::Atomic<bool> running{false};
SDL_atomic_t cancel{};

// What the last fetch or cache load said, for maps_stats (main thread only, taken from pendingStatus).
za::String lastStatus;

[[nodiscard]] za::String cachePath()
{
    return za::String{com_basedirs[com_numbasedirs - 1]} + "/cache/maps_index.txt";
}

[[nodiscard]] za::String indexUrl()
{
    if(vr_maps_index_url.string && vr_maps_index_url.string[0])
    {
        return za::String{vr_maps_index_url.string};
    }
    return za::String{"https://www.quaddicted.com/api/v1/"};
}

// ---------------------------------------------------------------- the arena

// `s` into the arena: control characters made spaces, so that a field's own separators (fieldSep, partSep) can never
// appear inside a value. The arena always ends a field with a NUL, so Index::field is a C string and a field's values
// are separated by partSep with nothing between them.
void appendRaw(Index& idx, const char* s, za::SizeT n)
{
    for(za::SizeT i = 0; i < n; i++)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        idx.text.pushBack(c < ' ' ? ' ' : static_cast<char>(c));
    }
}

// A field that holds one value.
Text addField(Index& idx, const char* s, za::SizeT n)
{
    Text t;
    t.off = static_cast<za::U32>(idx.text.size());
    t.len = static_cast<za::U32>(n);
    appendRaw(idx, s, n);
    idx.text.pushBack('\0');
    return t;
}

// One more value in a multi-valued field (empty values are dropped): the previous value's terminator becomes the
// separator, and the new value's terminator ends the field.
void addPart(Index& idx, Text& field, const char* value)
{
    if(!value || !value[0])
    {
        return;
    }
    if(field.len)
    {
        idx.text[idx.text.size() - 1] = partSep;
        field.len++;
    }
    else
    {
        field.off = static_cast<za::U32>(idx.text.size());
    }
    field.len += static_cast<za::U32>(strlen(value));
    appendRaw(idx, value, strlen(value));
    idx.text.pushBack('\0');
}

// A field read back from the cache: its values, split by partSep.
Text loadField(Index& idx, const za::String& s)
{
    Text t;
    za::SizeT i = 0;
    while(i < s.size())
    {
        za::SizeT j = i;
        while(j < s.size() && s.cStr()[j] != partSep)
        {
            j++;
        }
        addPart(idx, t, za::String{s.cStr() + i, j - i}.cStr());
        i = j + 1;
    }
    return t;
}

// ---------------------------------------------------------------- parsing one page

// A tag is "key=value"; the keys the model keeps. `size` is the old spelling of `map_size` (6 packages still use it).
void parseEntry(Index& idx, const jsonentry_t* e, za::Vector<za::String>& zipNames)
{
    za::String zipName; // the filename tag (its ratings' key)
    const char* sha = JSON_FindString(e, "sha256");
    if(!sha || !sha[0])
    {
        return;
    }

    Entry out;
    out.sha256 = addField(idx, sha, strlen(sha));
    if(const double* bytes = JSON_FindNumber(e, "bytes"))
    {
        out.bytes = static_cast<za::U64>(za::max(*bytes, 0.0));
    }

    if(const jsonentry_t* tags = JSON_Find(e, "tags", JSON_ARRAY))
    {
        for(const jsonentry_t* tag = tags->firstchild; tag; tag = tag->next)
        {
            if(!tag->string)
            {
                continue;
            }
            const char* eq = strchr(tag->string, '=');
            if(!eq)
            {
                continue;
            }
            const za::SizeT keyLen = static_cast<za::SizeT>(eq - tag->string);
            const auto isKey = [&](const char* name)
            {
                return strlen(name) == keyLen && !memcmp(tag->string, name, keyLen);
            };
            const char* value = eq + 1;
            if(isKey("title"))
            {
                out.title = addField(idx, value, strlen(value));
            }
            else if(isKey("author"))
            {
                addPart(idx, out.author, value);
            }
            else if(isKey("release_date"))
            {
                addPart(idx, out.date, value);
            }
            else if(isKey("type"))
            {
                addPart(idx, out.types, value);
            }
            else if(isKey("game_mode"))
            {
                addPart(idx, out.modes, value);
            }
            else if(isKey("map_size") || isKey("size"))
            {
                addPart(idx, out.sizes, value);
            }
            else if(isKey("theme"))
            {
                addPart(idx, out.themes, value);
            }
            else if(isKey("startmap"))
            {
                addPart(idx, out.startmap, value);
            }
            else if(isKey("filename"))
            {
                zipName = za::String{value};
            }
        }
    }

    if(const jsonentry_t* install = JSON_Find(e, "install", JSON_OBJECT))
    {
        if(const char* extract = JSON_FindString(install, "extract"))
        {
            out.extract = addField(idx, extract, strlen(extract));
        }
    }

    if(const char* description = JSON_FindString(e, "description"))
    {
        out.description = addField(idx, description, strlen(description));
    }

    if(const jsonentry_t* urls = JSON_Find(e, "urls", JSON_ARRAY))
    {
        for(const jsonentry_t* url = urls->firstchild; url; url = url->next)
        {
            addPart(idx, out.urls, url->string);
        }
    }

    // `files` is an object; its children are its keys, each with its value as a child (json.c's JSON_Find). Its count
    // is what the browser shows ("12 files"), and a file named progs.dat at any path is what vr_maps_allow_progs gates.
    if(const jsonentry_t* files = JSON_Find(e, "files", JSON_OBJECT))
    {
        for(const jsonentry_t* f = files->firstchild; f; f = f->next)
        {
            if(!f->string)
            {
                continue;
            }
            out.files++;
            const char* base = strrchr(f->string, '/');
            base = base ? base + 1 : f->string;
            if(!q_strcasecmp(base, "progs.dat"))
            {
                out.hasProgs = true;
            }
        }
    }

    idx.entries.pushBack(out);
    zipNames.pushBack(ZA_MOVE(zipName));
}

// ---------------------------------------------------------------- the ratings

// Quaddicted's ratings live only in its old Quake Injector database (frozen, archived as one 1.3 MB XML): per package
// `<file id="czg07" type="1" rating="5" normalized_users_rating="4.31">`, the id being the zip's name without ".zip".
bool fetchPage(const za::String& url, za::Vector<char>& body, const char*& error); // (the fetch, below)

constexpr const char* ratingsUrl = "https://www.quaddicted.com/static/archive/quaddicted_database.xml";

// An attribute's value in [tag, end), copied (empty when it is not there).
za::String xmlAttribute(const char* tag, const char* end, const char* name)
{
    const za::SizeT n = strlen(name);
    for(const char* p = tag; p + n + 2 < end; p++)
    {
        if(p[-1] == ' ' && !memcmp(p, name, n) && p[n] == '=' && p[n + 1] == '"')
        {
            const char* v = p + n + 2;
            const char* q = v;
            while(q < end && *q != '"')
            {
                q++;
            }
            return za::String{v, static_cast<za::SizeT>(q - v)};
        }
    }
    return za::String{};
}

// The ratings into the entries (zipNames: each entry's zip name, by index). A failed fetch costs only the ratings.
int addRatings(Index& idx, const za::Vector<za::String>& zipNames, za::Vector<char>& body)
{
    const char* error = nullptr;
    if(!fetchPage(za::String{ratingsUrl}, body, error))
    {
        return -1;
    }
    body.pushBack('\0');
    int matched = 0;
    for(const char* p = strstr(body.data(), "<file "); p; p = strstr(p + 1, "<file "))
    {
        const char* end = strchr(p, '>');
        if(!end)
        {
            break;
        }
        const za::String id = xmlAttribute(p + 1, end, "id");
        if(id.empty())
        {
            continue;
        }
        const za::String zip = id + ".zip";
        const za::String stars = xmlAttribute(p + 1, end, "rating");
        const za::String users = xmlAttribute(p + 1, end, "normalized_users_rating");
        for(za::SizeT i = 0; i < idx.entries.size() && i < zipNames.size(); i++)
        {
            if(q_strcasecmp(zipNames[i].cStr(), zip.cStr()))
            {
                continue;
            }
            Entry& e = idx.entries[i];
            const int s = stars.empty() ? 0 : Q_atoi(stars.cStr());
            e.rating = static_cast<za::U8>(s >= 1 && s <= 5 ? s : 0);
            const double u = users.empty() ? 0.0 : atof(users.cStr());
            e.userRating = static_cast<za::U16>(u > 0.0 && u <= 5.0 ? static_cast<int>(u * 100.0 + 0.5) : 0);
            matched++;
            break;
        }
    }
    return matched;
}

// ---------------------------------------------------------------- the cache

// Written whole: a header, then one line per package (fields by fieldSep, a field's values by partSep).
za::String cacheText(const Index& idx)
{
    za::String out;
    out += cacheMagic;
    out += "\n#";
    out += idx.source;
    out += "\n#";
    out += za::toString(idx.fetchedAt);
    out += "\n";
    char bytes[32];
    for(const Entry& e : idx.entries)
    {
        const auto addField = [&](const Text& t)
        {
            out += fieldSep;
            out.append(idx.field(t), idx.length(t));
        };
        out.append(idx.field(e.sha256), idx.length(e.sha256));
        addField(e.title);
        addField(e.author);
        addField(e.date);
        addField(e.types);
        addField(e.modes);
        addField(e.sizes);
        addField(e.themes);
        q_snprintf(bytes, sizeof(bytes), "%llu", static_cast<unsigned long long>(e.bytes));
        out += fieldSep;
        out += bytes;
        addField(e.startmap);
        addField(e.extract);
        out += fieldSep;
        out += e.hasProgs ? '1' : '0';
        addField(e.urls);
        addField(e.description);
        q_snprintf(bytes, sizeof(bytes), "%d", e.files);
        out += fieldSep;
        out += bytes;
        q_snprintf(bytes, sizeof(bytes), "%d", static_cast<int>(e.rating));
        out += fieldSep;
        out += bytes;
        q_snprintf(bytes, sizeof(bytes), "%d", static_cast<int>(e.userRating));
        out += fieldSep;
        out += bytes;
        out += '\n';
    }
    return out;
}

bool writeCache(Index& idx)
{
    const za::String path = cachePath();
    files::createDirectories(za::String{files::parentPath(path)}.cStr()); // (the parent of a path, "" without a '/')
    const za::String text = cacheText(idx);
    if(!files::writeText(path.cStr(), text))
    {
        return false;
    }
    idx.cacheBytes = text.size();
    return true;
}

// A cache line's fields, by position: an empty field stays empty (files::forPieces skips an empty piece at the end of
// a line, which would drop a package that has no download URL). Returns how many fields it filled.
int splitFields(const char* line, za::SizeT len, za::String (&fields)[17])
{
    za::SizeT i = 0;
    int n = 0;
    while(n < 17)
    {
        za::SizeT j = i;
        while(j < len && line[j] != fieldSep)
        {
            j++;
        }
        fields[n] = za::String{line + i, j - i};
        n++;
        if(j >= len)
        {
            break;
        }
        i = j + 1;
    }
    return n;
}

// A cached index, if the file is there, of this version and this source URL, and no older than cacheSeconds.
bool loadCache(Index& idx, const za::String& url)
{
    za::String text;
    if(!files::readText(cachePath().cStr(), text) || text.size() < sizeof(cacheMagic))
    {
        return false;
    }
    za::I64 fetchedAt = 0;
    int line = 0;
    za::String header[3];
    files::forLines(text, [&](za::StringView l)
    {
        if(line < 3)
        {
            header[line] = za::String{l};
        }
        line++;
    });
    if(header[0] != za::String{cacheMagic} || header[1] != za::String{"#"} + url)
    {
        return false;
    }
    fetchedAt = static_cast<za::I64>(strtoll(header[2].cStr() + 1, nullptr, 10));
    za::I64 now = 0;
    time(&now);
    if(fetchedAt <= 0 || now - fetchedAt > cacheSeconds)
    {
        return false;
    }

    idx.source = url;
    idx.fetchedAt = fetchedAt;
    idx.cacheBytes = text.size();

    files::forLines(text, [&](za::StringView l)
    {
        if(l.empty() || l.data()[0] == '#')
        {
            return;
        }
        // sha title author date types modes sizes themes bytes startmap extract progs urls description files rating
        // userRating
        za::String fields[17];
        if(splitFields(l.data(), l.size(), fields) < 17)
        {
            return;
        }
        Entry e;
        e.sha256 = loadField(idx, fields[0]);
        e.title = loadField(idx, fields[1]);
        e.author = loadField(idx, fields[2]);
        e.date = loadField(idx, fields[3]);
        e.types = loadField(idx, fields[4]);
        e.modes = loadField(idx, fields[5]);
        e.sizes = loadField(idx, fields[6]);
        e.themes = loadField(idx, fields[7]);
        e.bytes = static_cast<za::U64>(strtoull(fields[8].cStr(), nullptr, 10));
        e.startmap = loadField(idx, fields[9]);
        e.extract = loadField(idx, fields[10]);
        e.hasProgs = fields[11].size() && fields[11].cStr()[0] == '1';
        e.urls = loadField(idx, fields[12]);
        e.description = loadField(idx, fields[13]);
        e.files = static_cast<int>(strtol(fields[14].cStr(), nullptr, 10));
        e.rating = static_cast<za::U8>(za::min(za::max(static_cast<int>(strtol(fields[15].cStr(), nullptr, 10)), 0), 5));
        e.userRating =
            static_cast<za::U16>(za::min(za::max(static_cast<int>(strtol(fields[16].cStr(), nullptr, 10)), 0), 500));
        idx.entries.pushBack(e);
    });

    if(idx.entries.empty())
    {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- the fetch

size_t writeChunk(void* buffer, size_t size, size_t nmemb, void* stream)
{
    if(SDL_AtomicGet(&cancel))
    {
        return 0; // (the transfer stops: as host_cmd.c's WriteManifestChunk)
    }
    za::Vector<char>& body = *static_cast<za::Vector<char>*>(stream);
    const za::SizeT n = size * nmemb;
    body.reserveMore(n);
    body.unsafeEmplaceBackRange(static_cast<const char*>(buffer), n);
    return nmemb;
}

bool fetchPage(const za::String& url, za::Vector<char>& body, const char*& error)
{
    body.clear();
    download_t dl{};
    dl.headers = &acceptHeader;
    dl.num_headers = 1;
    dl.write_fn = writeChunk;
    dl.write_data = &body;
    dl.abort = &cancel;
    const bool ok = Download(url.cStr(), &dl);
    error = dl.error ? dl.error : (dl.response ? va("HTTP %d", dl.response) : "no response");
    return ok;
}

// The whole index, a page at a time. False: it did not finish (nothing is published in that case).
bool fetchIndex(Index& idx, const za::String& url, za::String& status)
{
    za::Vector<char> body;
    za::Vector<za::String> zipNames; // by entry (the ratings' key)
    for(int start = 0; start < maxPages * pageRows; start += pageRows)
    {
        if(SDL_AtomicGet(&cancel))
        {
            status = "map index: the fetch was cancelled (the game quit)";
            return false;
        }
        const za::String pageUrl =
            url + "?q=*:*&rows=" + za::toString(pageRows) + "&start=" + za::toString(start);
        const za::U32 t0 = SDL_GetTicks();
        const char* error = nullptr;
        if(!fetchPage(pageUrl, body, error))
        {
            status = za::String{"map index: none ("} + pageUrl + ": " + error + ") - the game runs without it";
            return false;
        }
        idx.fetchMs += static_cast<int>(SDL_GetTicks() - t0);
        idx.fetchedBytes += body.size();
        idx.pages++;
        body.pushBack('\0');

        const za::U32 t1 = SDL_GetTicks();
        json_t* json = JSON_Parse(body.data());
        idx.parseMs += static_cast<int>(SDL_GetTicks() - t1);
        if(!json)
        {
            status = za::String{"map index: none ("} + pageUrl + ": not JSON) - the game runs without it";
            return false;
        }
        idx.jsonPeak = za::max(idx.jsonPeak, static_cast<za::U64>(json->memsize) + body.size());

        const za::SizeT before = idx.entries.size();
        const jsonentry_t* root = json->root;
        if(root->type == JSON_ARRAY)
        {
            for(const jsonentry_t* e = root->firstchild; e; e = e->next)
            {
                if(e->type == JSON_OBJECT)
                {
                    parseEntry(idx, e, zipNames);
                }
            }
        }
        JSON_Free(json);

        if(idx.entries.size() - before < static_cast<za::SizeT>(pageRows))
        {
            break; // the last page
        }
    }

    if(idx.entries.empty())
    {
        status = "map index: none (the index was empty) - the game runs without it";
        return false;
    }
    idx.rated = addRatings(idx, zipNames, body);
    return true;
}

void publish(Index&& built, za::String&& status)
{
    za::LockGuard lock{handoff};
    pending = za::makeUnique<Index>(ZA_MOVE(built));
    pendingStatus = ZA_MOVE(status);
    pendingReady.storeSeqCst(true);
}

za::String runUrl; // the pass's URL, read on the main thread as it starts (a cvar's string may be freed meanwhile)

void run() noexcept
{
    struct Done
    {
        ~Done() { running.storeSeqCst(false); } // every way out: a cache hit, a failure, a fetch
    } done;
    const za::String url = runUrl;
    Index built;

    if(loadCache(built, url))
    {
        char status[256];
        q_snprintf(status, sizeof(status),
            "map index: %d packages, from the cache (%llu KiB, fetched %lld s ago); not fetched",
            static_cast<int>(built.entries.size()), static_cast<unsigned long long>(built.cacheBytes / 1024),
            static_cast<long long>(time(nullptr) - built.fetchedAt));
        publish(ZA_MOVE(built), za::String{status});
        return;
    }

    built.source = url;
    time(&built.fetchedAt);
    za::String status;
    if(!fetchIndex(built, url, status))
    {
        // The status only: the index held (a cached or earlier fetch) is kept.
        za::LockGuard lock{handoff};
        pendingStatus = ZA_MOVE(status);
        pendingReady.storeSeqCst(true);
        return;
    }

    const bool cached = writeCache(built);
    char head[256];
    q_snprintf(head, sizeof(head),
        "map index: %d packages from %s (%llu KiB in %d calls, %d ms; parsed in %d ms; peak %llu KiB) %s",
        static_cast<int>(built.entries.size()), url.cStr(),
        static_cast<unsigned long long>(built.fetchedBytes / 1024), built.pages, built.fetchMs, built.parseMs,
        static_cast<unsigned long long>(built.jsonPeak / 1024), cached ? "" : "(the cache could not be written)");
    status = za::String{head};
    status += built.rated >= 0 ? za::String{", "} + za::toString(built.rated) + " rated" : za::String{", no ratings"};
    if(cached)
    {
        status += za::String{", cached to "} + cachePath();
    }
    publish(ZA_MOVE(built), ZA_MOVE(status));
}

// ---------------------------------------------------------------- reading it back

int foldCompare(const char* a, const char* b)
{
    while(*a && *b)
    {
        const int ca = tolower(static_cast<unsigned char>(*a));
        const int cb = tolower(static_cast<unsigned char>(*b));
        if(ca != cb)
        {
            return ca - cb;
        }
        a++;
        b++;
    }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

bool containsFolded(const char* hay, const char* needle)
{
    if(!needle[0])
    {
        return true;
    }
    for(const char* p = hay; *p; p++)
    {
        za::SizeT i = 0;
        while(needle[i] && tolower(static_cast<unsigned char>(p[i])) == tolower(static_cast<unsigned char>(needle[i])))
        {
            i++;
        }
        if(!needle[i])
        {
            return true;
        }
    }
    return false;
}

// A multi-valued field holds `want` (whole, any case).
bool hasValue(const Index& idx, const Text& field, const char* want)
{
    if(!want[0])
    {
        return true;
    }
    bool found = false;
    forParts(idx.field(field), [&](const za::String& part)
    {
        if(!found && !foldCompare(part.cStr(), want))
        {
            found = true;
        }
    });
    return found;
}

za::String joinParts(const Index& idx, const Text& field, const char* sep)
{
    za::String out;
    forParts(idx.field(field), [&](const za::String& part)
    {
        if(out.size())
        {
            out += sep;
        }
        out += part;
    });
    return out;
}

struct ByQuery
{
    const Index& idx;
    Sort sort;
    bool newestFirst;

    [[nodiscard]] bool operator()(const Entry* a, const Entry* b) const
    {
        int c = 0;
        if(sort == Sort::Bytes)
        {
            c = a->bytes < b->bytes ? -1 : (a->bytes > b->bytes ? 1 : 0);
            if(newestFirst)
            {
                c = -c;
            }
        }
        else if(sort == Sort::Rating)
        {
            c = b->score() - a->score();
            if(!c)
            {
                c = strcmp(idx.field(b->date), idx.field(a->date)); // (then the newest)
            }
        }
        else if(sort == Sort::Date)
        {
            c = strcmp(idx.field(a->date), idx.field(b->date));
            if(newestFirst)
            {
                c = -c;
            }
        }
        else
        {
            c = foldCompare(idx.field(a->title), idx.field(b->title));
        }
        if(c)
        {
            return c < 0;
        }
        return foldCompare(idx.field(a->title), idx.field(b->title)) < 0; // (one order for equal keys)
    }
};

// ---------------------------------------------------------------- the console

void splitWords(const za::String& text, za::Vector<za::String>& words)
{
    words.clear();
    files::forPieces(za::StringView{text}, ' ', [&](za::StringView part)
    {
        if(!part.empty())
        {
            words.pushBack(za::String{part});
        }
    });
}

void parseFilters(int argc, Query& q, za::String& text)
{
    for(int i = 1; i < argc; i++)
    {
        const char* a = Cmd_Argv(i);
        const char* eq = strchr(a, '=');
        if(!eq)
        {
            if(text.size())
            {
                text += ' ';
            }
            text += a;
            continue;
        }
        const za::String key{a, static_cast<za::SizeT>(eq - a)};
        const char* value = eq + 1;
        if(!q_strcasecmp(key.cStr(), "type"))
        {
            q.type = za::String{value};
        }
        else if(!q_strcasecmp(key.cStr(), "mode") || !q_strcasecmp(key.cStr(), "game_mode"))
        {
            q.gameMode = za::String{value};
        }
        else if(!q_strcasecmp(key.cStr(), "size") || !q_strcasecmp(key.cStr(), "map_size"))
        {
            q.mapSize = za::String{value};
        }
        else if(!q_strcasecmp(key.cStr(), "limit"))
        {
            q.limit = Q_atoi(value);
        }
        else if(!q_strcasecmp(key.cStr(), "progs"))
        {
            q.allowProgs = Q_atoi(value) != 0;
        }
        else if(!q_strcasecmp(key.cStr(), "oldest"))
        {
            q.newestFirst = Q_atoi(value) == 0;
        }
        else if(!q_strcasecmp(key.cStr(), "sort"))
        {
            q.sort = !q_strcasecmp(value, "bytes")   ? Sort::Bytes
                    : !q_strcasecmp(value, "title") ? Sort::Title
                    : !q_strcasecmp(value, "rating") ? Sort::Rating
                                                    : Sort::Date;
        }
        else if(!q_strcasecmp(key.cStr(), "rating"))
        {
            q.minRating = static_cast<int>(atof(value) * 100.0 + 0.5);
        }
        else
        {
            Con_Printf("maps_list: unknown filter \"%s\" (type= mode= size= sort= rating= limit= progs= oldest=)\n",
                key.cStr());
        }
    }
}

void list_f()
{
    Query q;
    q.allowProgs = vr_maps_allow_progs.value != 0.f;
    q.limit = listLimit;
    za::String text;
    parseFilters(Cmd_Argc(), q, text);
    q.text = ZA_MOVE(text);

    za::Vector<const Entry*> out;
    search(q, out);

    char filters[256];
    q_snprintf(filters, sizeof(filters), "text \"%s\" type=%s mode=%s size=%s sort=%s %s limit=%d", q.text.cStr(),
        q.type.size() ? q.type.cStr() : "any", q.gameMode.size() ? q.gameMode.cStr() : "any",
        q.mapSize.size() ? q.mapSize.cStr() : "any",
        q.sort == Sort::Bytes ? "bytes" : q.sort == Sort::Title ? "title" : q.sort == Sort::Rating ? "rating" : "date",
        q.allowProgs ? "progs=1" : "progs=0", q.limit);
    Con_Printf("maps_list (%s): %d of %d packages\n", filters, static_cast<int>(out.size()),
        static_cast<int>(indexSet.entries.size()));
    Con_Printf("%-10s %-9s %-6s %8s %-42s %-22s %-10s\n", "date", "type", "size", "MB", "title", "author", "sha256");
    for(const Entry* e : out)
    {
        char date[40], type[24], size[16], title[88], author[48], sha[16];
        q_strlcpy(date, joinParts(indexSet, e->date, ",").cStr(), sizeof(date));
        q_strlcpy(type, joinParts(indexSet, e->types, ",").cStr(), sizeof(type));
        q_strlcpy(size, joinParts(indexSet, e->sizes, ",").cStr(), sizeof(size));
        q_strlcpy(title, indexSet.field(e->title), sizeof(title));
        q_strlcpy(author, joinParts(indexSet, e->author, ",").cStr(), sizeof(author));
        q_strlcpy(sha, indexSet.field(e->sha256), sizeof(sha));
        Con_Printf("%-10s %-9s %-6s %8.1f %-42s %-22s %-10s%s\n", date, type, size,
            static_cast<double>(e->bytes) / 1048576.0, title, author, sha, e->hasProgs ? " progs" : "");
    }
}

void info_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("maps_info <sha256 or a prefix of one>\n");
        return;
    }
    const Entry* e = find(za::String{Cmd_Argv(1)});
    if(!e)
    {
        Con_Printf("maps_info: no package matches \"%s\" (maps_list finds their sha256s)\n", Cmd_Argv(1));
        return;
    }
    Con_Printf("maps_info %s\n", indexSet.field(e->sha256));
    Con_Printf("  title       %s\n", indexSet.field(e->title));
    Con_Printf("  author      %s\n", joinParts(indexSet, e->author, ", ").cStr());
    Con_Printf("  released    %s\n", joinParts(indexSet, e->date, ", ").cStr());
    Con_Printf("  type        %s\n", joinParts(indexSet, e->types, ", ").cStr());
    Con_Printf("  game_mode   %s\n", joinParts(indexSet, e->modes, ", ").cStr());
    Con_Printf("  map_size    %s\n", joinParts(indexSet, e->sizes, ", ").cStr());
    Con_Printf("  theme       %s\n", joinParts(indexSet, e->themes, ", ").cStr());
    Con_Printf("  bytes       %llu\n", static_cast<unsigned long long>(e->bytes));
    Con_Printf("  startmap    %s\n", indexSet.field(e->startmap));
    Con_Printf("  extract     %s\n", indexSet.field(e->extract));
    Con_Printf("  files       %d\n", e->files);
    Con_Printf("  rating      users %.2f, Quaddicted %d (0: none)\n", static_cast<double>(e->userRating) / 100.0,
        static_cast<int>(e->rating));
    Con_Printf("  progs.dat   %s\n", e->hasProgs ? "yes (vr_maps_allow_progs)" : "no");
    Con_Printf("  description %s\n", indexSet.field(e->description));
    Con_Printf("  download    %s\n", joinParts(indexSet, e->urls, "\n              ").cStr());
}

void countField(const Index& idx, ankerl::unordered_dense::map<za::String, int>& counts, const Text& field)
{
    if(!idx.length(field))
    {
        counts["(none)"]++;
        return;
    }
    forParts(idx.field(field), [&](const za::String& part) { counts[part]++; });
}

void stats_f()
{
    const Index& idx = indexSet;
    Con_Printf("map index: %d packages%s\n", static_cast<int>(idx.entries.size()),
        idx.entries.empty() ? " (none: nothing has been fetched in this session)" : "");
    if(!lastStatus.empty())
    {
        Con_Printf("  %s\n", lastStatus.cStr());
    }
    if(idx.pages)
    {
        Con_Printf("  fetched   %llu KiB in %d calls, %d ms; parsed in %d ms; peak held at once %llu KiB\n",
            static_cast<unsigned long long>(idx.fetchedBytes / 1024), idx.pages, idx.fetchMs, idx.parseMs,
            static_cast<unsigned long long>(idx.jsonPeak / 1024));
    }
    Con_Printf("  cache     %s (%llu KiB)\n", cachePath().cStr(),
        static_cast<unsigned long long>(idx.cacheBytes / 1024));
    Con_Printf("  held      %llu KiB (%d entries x %llu B, %llu KiB of text)\n",
        static_cast<unsigned long long>((mem::heldBytes(idx.entries) + mem::heldBytes(idx.text)) / 1024),
        static_cast<int>(idx.entries.size()), static_cast<unsigned long long>(sizeof(Entry)),
        static_cast<unsigned long long>(mem::heldBytes(idx.text) / 1024));

    using Counts = ankerl::unordered_dense::map<za::String, int>;
    Counts types, modes, sizes;
    int progs = 0;
    for(const Entry& e : idx.entries)
    {
        countField(idx, types, e.types);
        countField(idx, modes, e.modes);
        countField(idx, sizes, e.sizes);
        if(e.hasProgs)
        {
            progs++;
        }
    }
    const auto print = [](const char* name, const Counts& counts)
    {
        Con_Printf("  %-10s", name);
        for(const auto* kv : qza::sortedByKey(counts))
        {
            Con_Printf(" %s %d", kv->first.cStr(), kv->second);
        }
        Con_Printf("\n");
    };
    print("type", types);
    print("game_mode", modes);
    print("map_size", sizes);
    Con_Printf("  progs.dat %d packages carry one; %d shown with vr_maps_allow_progs %.0f, %d with it off\n", progs,
        static_cast<int>(idx.entries.size()) - (vr_maps_allow_progs.value ? 0 : progs), vr_maps_allow_progs.value,
        static_cast<int>(idx.entries.size()) - progs);
}

void fetch_f()
{
    const bool force = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "force");
    if(running.loadSeqCst())
    {
        Con_Printf("maps_fetch: a fetch is already under way\n");
        return;
    }
    if(force)
    {
        za::String path = cachePath();
        files::remove(path.cStr()); // (loadCache would otherwise take the cached copy)
    }
    start();
    Con_Printf("maps_fetch: fetching %s (maps_stats shows what happened)\n", indexUrl().cStr());
}

} // namespace

// ---------------------------------------------------------------- the API

void start()
{
    if(COM_CheckParm("-nomapindex"))
    {
        Con_DPrintf("map index: off (-nomapindex)\n");
        return;
    }
    if(!vr_maps_fetch.value)
    {
        Con_DPrintf("map index: off (vr_maps_fetch 0)\n");
        return;
    }
    if(running.loadSeqCst())
    {
        return;
    }
    if(worker.joinable())
    {
        worker.join(); // (a pass that finished; its handle was never joined)
    }
    SDL_AtomicSet(&cancel, 0);
    runUrl = indexUrl();
    running.storeSeqCst(true);
    worker = za::Thread(run);
}

void finish()
{
    SDL_AtomicSet(&cancel, 1);
    if(worker.joinable())
    {
        worker.join();
    }
    running.storeSeqCst(false);
}

void poll()
{
    if(!pendingReady.loadSeqCst())
    {
        return;
    }
    za::UniquePtr<Index> taken;
    za::String status;
    {
        za::LockGuard lock{handoff};
        taken = ZA_MOVE(pending);
        status = ZA_MOVE(pendingStatus);
        pendingStatus = za::String{};
        pendingReady.storeSeqCst(false);
    }
    if(taken)
    {
        static_cast<Index&>(indexSet) = ZA_MOVE(*taken); // (the registered set holds the live index: vr_memstats)
    }
    lastStatus = status;
    if(status.size())
    {
        Con_SafePrintf("%s\n", status.cStr());
    }
}

const Index& index()
{
    return indexSet;
}

void search(const Query& q, za::Vector<const Entry*>& out)
{
    out.clear();
    za::Vector<za::String> words;
    splitWords(q.text, words);

    za::Vector<const Entry*> all;
    for(const Entry& e : indexSet.entries)
    {
        if(e.hasProgs && !q.allowProgs)
        {
            continue;
        }
        if(q.minRating > 0 && e.score() < q.minRating)
        {
            continue;
        }
        if(!hasValue(indexSet, e.types, q.type.cStr()) || !hasValue(indexSet, e.modes, q.gameMode.cStr()) ||
           !hasValue(indexSet, e.sizes, q.mapSize.cStr()))
        {
            continue;
        }
        bool matches = true;
        for(const za::String& word : words)
        {
            if(!containsFolded(indexSet.field(e.title), word.cStr()) &&
               !containsFolded(indexSet.field(e.author), word.cStr()))
            {
                matches = false;
                break;
            }
        }
        if(matches)
        {
            all.pushBack(&e);
        }
    }

    za::stableSort(all.begin(), all.end(), ByQuery{indexSet, q.sort, q.newestFirst});

    const za::SizeT n =
        q.limit > 0 ? za::min(static_cast<za::SizeT>(q.limit), all.size()) : all.size();
    out.reserve(n);
    for(za::SizeT i = 0; i < n; i++)
    {
        out.pushBack(all[i]);
    }
}

const Entry* find(const za::String& shaPrefix)
{
    const Entry* found = nullptr;
    int matches = 0;
    for(const Entry& e : indexSet.entries)
    {
        if(!q_strncasecmp(indexSet.field(e.sha256), shaPrefix.cStr(), shaPrefix.size()))
        {
            found = &e;
            matches++;
        }
    }
    return matches == 1 ? found : nullptr;
}

void registerCommands()
{
    Cmd_AddCommand("maps_list", list_f);
    Cmd_AddCommand("maps_info", info_f);
    Cmd_AddCommand("maps_stats", stats_f);
    Cmd_AddCommand("maps_fetch", fetch_f);
}

} // namespace qvr::mapindex

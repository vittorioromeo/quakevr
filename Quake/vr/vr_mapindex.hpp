#pragma once

// vr_mapindex.hpp -- the external map index: Quaddicted's package index, fetched at start-up on a thread of its own,
// cached on disk, and held in a slim model (vr_mapindex.cpp). Nothing here installs or extracts anything: the map
// browser (the next step) reads this and hands a chosen package to the downloader.
//
// The index is one flat list of packages, each with the fields the browser filters and shows. Packages whose files
// carry a progs.dat are kept in the list with `hasProgs` set, and left out of the results unless
// vr_maps_allow_progs is on: re-including them is the cvar, never a change to this model.

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include "vr_mem.hpp"

namespace qvr::mapindex
{

// A field's text: an offset and a length in the index's arena, NUL terminated at `off + len` (Index::field gives a
// pointer to it). Multi-valued fields hold their values separated by partSep.
struct Text
{
    za::U32 off{0};
    za::U32 len{0};
};

constexpr char fieldSep = '\t';  // a cache line's fields
constexpr char partSep = '\x1f'; // a multi-valued field's values, in the arena and in the cache alike

// One package. The strings point into the index's arena; they live as long as the index does.
struct Entry
{
    Text sha256;
    Text title;
    Text author;   // every author tag
    Text date;     // release_date
    Text startmap; // the map to start (a browser's "play" needs it)
    Text extract;  // install.extract, e.g. "{base}/id1/maps/"
    Text urls;     // the download mirrors, quaddicted first
    Text types;    // map, mod, speedmap, episode, ... (a package may carry more than one)
    Text modes;    // singleplayer, deathmatch, cooperative
    Text sizes;    // tiny, small, medium, large, huge
    Text themes;
    Text description; // the package's own text (the browser's detail view)
    za::U64 bytes{0};
    int files{0}; // how many files its zip holds (the count only: the browser says "12 files")
    bool hasProgs{false}; // its files carry a progs.dat at any path (vr_maps_allow_progs)
    // Community ratings: Quaddicted's archived database (the API carries none), joined on the zip's name.
    za::U16 userRating{0}; // the users' average, x100 (100..500); 0: none
    za::U8 rating{0};      // Quaddicted's own stars, 1..5; 0: none

    // What the Rating sort and filter go by (x100): the users' average, else Quaddicted's stars; 0: unrated.
    [[nodiscard]] int score() const
    {
        return userRating ? static_cast<int>(userRating) : static_cast<int>(rating) * 100;
    }
};

// The index, and what the fetch that made it cost (maps_stats). Registered with mem::Cache (mem::Never: its owner
// replaces it, at a fetch).
struct Index
{
    za::Vector<Entry> entries;
    za::Vector<char> text; // every Entry's strings, one arena
    za::String source;     // the URL it came from (a cached copy of another URL is not used)
    za::I64 fetchedAt{0};  // unix seconds
    za::U64 fetchedBytes{0};
    za::U64 jsonPeak{0}; // the largest (one page's text + its parse tree) held at once
    za::U64 cacheBytes{0}; // the cached copy written for it
    int fetchMs{0};
    int parseMs{0};
    int pages{0};
    int rated{-1}; // packages the ratings were found for (-1: none fetched, e.g. read from the cache)

    auto members()
    {
        return mem::list(entries, text, source, fetchedAt, fetchedBytes, jsonPeak, cacheBytes, fetchMs, parseMs, pages);
    }

    // A field as a C string ("" when it has nothing), and its length.
    [[nodiscard]] const char* field(const Text& t) const { return t.len ? text.data() + t.off : ""; }
    [[nodiscard]] za::SizeT length(const Text& t) const { return t.len; }
};

// What to show (the browser's filter; maps_list builds the same thing from the console).
enum class Sort
{
    Date, // release_date
    Bytes,
    Title,
    Rating, // Entry::score, the best first (unrated last)
};

struct Query
{
    za::String text;                    // words, any case, each found in the title or an author
    za::String type, gameMode, mapSize; // "" : any
    bool allowProgs{false};            // include the packages that carry their own progs.dat
    Sort sort{Sort::Date};
    bool newestFirst{true};            // Date and Bytes only
    int minRating{0};                  // Entry::score at least this (x100); 0: any, the unrated included
    int limit{0};                      // 0: every match
};

// Start-up: the fetch thread (nothing here waits for it). -nomapindex, or vr_maps_fetch 0, keeps it off; `asked`
// (maps_fetch) starts it whatever they say: they are about the start-up fetch.
void start(bool asked = false);
// Quit: the fetch is cancelled and its thread joined.
void finish();
// The main thread, every frame: takes the index the fetch thread finished (if any) and says what happened.
void poll();
// maps_list, maps_info, maps_stats, maps_fetch.
void registerCommands();

// The index: never null, empty until one has arrived. Read from the main thread only.
[[nodiscard]] const Index& index();

// What the fetch is doing (its latest step while it runs), or how the last one ended. Main thread.
[[nodiscard]] za::String state();

// `out` cleared, then every match in the query's order (the caller keeps `out`; the menu keeps it in its scratch).
void search(const Query&, za::Vector<const Entry*>& out);

// A package by its sha256, or by an unambiguous prefix of it. Null: none, or more than one.
[[nodiscard]] const Entry* find(const za::String& shaPrefix);

// A multi-valued field's values, in order: f(value).
template <typename F>
void forParts(const char* field, F&& f)
{
    const char* p = field;
    for(;;)
    {
        const char* end = p;
        while(*end && *end != partSep)
        {
            end++;
        }
        if(end > p)
        {
            f(za::String{p, static_cast<za::SizeT>(end - p)});
        }
        if(!*end)
        {
            break;
        }
        p = end + 1;
    }
}

} // namespace qvr::mapindex

#pragma once

// vr_slipseer.hpp -- Slipseer (slipseer.com) as the Map Library's second source, beside Quaddicted's index
// (vr_mapindex.hpp). Its map pages are read the way a person browsing the site would read them (permission granted by
// the site, 2026-11-11): one page at a time, seconds apart (jittered), a realistic browser's User-Agent picked once per
// session from a built-in list and that browser's usual headers, every page and screenshot kept in a cache on disk
// (revalidated with ETag / If-Modified-Since when it is old), robots.txt honoured, and a 429 or a 5xx answered by
// stopping and waiting (minutes, then longer) rather than retrying. The listing pages make the index (their entries
// go into the same mapindex::Entry model, `source` Slipseer); a map's own page (description, screenshot, download
// link, what it needs) is fetched only when the player picks that map, and its download only when they install it.
// Test runs (QVR_TEST_BACKGROUND) and vr_maps_fetch 0 read the cache only: no network.

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include "vr_mapindex.hpp"

namespace qvr::slipseer
{

// ---------------------------------------------------------------- parsing (pure: the fixture tests run these)

// One map as a listing page shows it.
struct Item
{
    za::String page;   // its own page (absolute URL): the map's identity
    za::String title;
    za::String author;
    za::String date;   // YYYY-MM-DD ("" : none given)
    za::String thumb;  // the listing's picture (absolute URL)
    za::String needs;    // what it needs ("Arcane Dimensions", "Quoth", ...; "" : the stock game)
    za::String category; // the site's category slug ("vanilla", "copper", "multiplayer", ...)
    za::String tagline;  // its one line
    int rating{0};       // the users' stars x100 (0: none)
};

// What a map's own page adds.
struct Details
{
    za::String title;
    za::String author;
    za::String date;
    za::String description; // plain text (tags dropped, entities decoded, whitespace folded)
    za::String download;    // the download link (absolute URL)
    za::String screenshot;  // the first screenshot (absolute URL)
    za::String needs;
    int rating{0}; // x100
};

// A listing page's maps (appended to `out`), and the next listing page's URL ("" : the last one).
void parseListing(const char* html, const char* pageUrl, za::Vector<Item>& out, za::String& next);
// A map page's details. False: it does not look like one.
bool parseDetails(const char* html, const char* pageUrl, Details& out);
// `href` made absolute against `base` (entities decoded).
[[nodiscard]] za::String absoluteUrl(const char* base, const za::String& href);
// The mapindex id of a Slipseer map: the sha256 of "slipseer:<page>", 64 hex digits (its folder's name, as a
// Quaddicted package's sha256 is).
[[nodiscard]] za::String idFor(const za::String& page);
// What `needs` installs as: mapindex Entry's extract ("{base}/ad/") and theme tags ("arcane_dimensions").
void requirementFields(const za::String& needs, za::String& extract, za::String& theme);

// ---------------------------------------------------------------- the module

// Start-up (and maps_fetch, `asked`): the listing pass, on a thread of its own (the cache only in test runs and with
// vr_maps_fetch 0, unless asked). vr_maps_slipseer 0 keeps Slipseer out.
void start(bool asked = false);
void finish();
// The main thread, every frame: hands the entries the pass built to mapindex (setSlipseer), prints its lines, and
// makes the picked map's screenshot a picture.
void poll();
// maps_slipseer, maps_slipseer_get, maps_slipseer_test.
void registerCommands();

// The picked map's page wanted (its details, then its screenshot): fetched when its turn comes, unless cached.
void want(const za::String& page);
// Whether its details are known (its download link among them), and whether they are being fetched.
[[nodiscard]] bool detailed(const za::String& page);
[[nodiscard]] bool fetching(const za::String& page);
// What the module is doing, in one line.
[[nodiscard]] za::String state();

// The screenshot of `page`, when it is decoded (false: not yet, or none): its size in pixels, and drawn into a box
// (x, y, w, h in the canvas' units; the caller keeps its aspect).
[[nodiscard]] bool screenshotSize(const za::String& page, int& width, int& height);
void drawScreenshot(float x, float y, float w, float h);

// The session's browser headers for a download of `referer`'s map ("User-Agent: ...", "Referer: ...").
void downloadHeaders(const char* referer, za::Vector<za::String>& out);

// Live requests made this session (maps_slipseer; the tests check it stays 0).
[[nodiscard]] int requestsMade();

} // namespace qvr::slipseer

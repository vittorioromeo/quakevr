//
// vr_arena_supply.qc - Survival Arena: where a wave clear's supplies land (vr_arena.qc's VR_ARENA_Drops).
//
// The author's ask (2026-10-29): the medkits, the armour and the rest put down at random all over the arena, the
// upper ledges too, "pretty much everywhere where there's walkable terrain", so the player goes looking for them
// rather than camping; one or two armour shards a wave on top; a line in the middle of the view naming what came.
// The foes' guns are not placed here (they fall where their holders fall, under their loot pillars).
//
// The walkable places: found once a run begins (VR_ARNAV_Frame, from VR_ARENA_Frame) by the engine's navplace walk
// (vr_builtins.cpp), a walk out from the player's start over a lattice AR_NAV_GRID apart, every move between two cells
// walked the way the player walks: his box, a step up of 18 at most and down as much, never off a ledge, never up a
// slope too steep to stand on. A place is therefore one he can walk to (a 48-high cover's top is not; a tower top up
// its stairs is). AR_NAV_MAX at most (a big map's walk stops there). The engine keeps them: the QC's globals are near
// the id format's limit, so none of it is held here.
//
// A supply's place (VR_ARENA_ScatterPlace): one of those at random, a share of the picks up high (AR_SCATTER_HIGH,
// when the map has places AR_NAV_HIGH over its lowest), out of any liquid, no nearer the player than
// AR_SCATTER_NEAR, no nearer another of the same clear than AR_SCATTER_GAP, its box clear of anything lying or standing
// there (a foe, a gun, an earlier clear's medkit). The rolls are their own stream (vr_arena_seed's, salted), so the
// waves' plans do not move with them. vr_arena_scatter 0: the map's info_arena_item points, as before.
//

#define AR_SCATTER_MAX 12      // a clear's supplies remembered for their spacing
float AR_NAV_MAX       = 4096; // walkable places at most
float AR_NAV_GRID      = 48;   // the lattice's spacing (units)
float AR_NAV_HIGH      = 128;  // a place this much over the lowest is a high one (a tower top, a wall walk, a ledge)
float AR_SCATTER_NEAR  = 256;  // a supply lands no nearer the player than this
float AR_SCATTER_GAP   = 160;  // nor nearer another of the same clear
float AR_SCATTER_HIGH  = 0.35; // the share of the picks that look up high
float AR_SCATTER_TRIES = 64;   // picks tried for a supply (half of them up high, when it looks there)
float AR_SHARDS_MAX    = 4;    // vr_arena_shards' own clamp
float AR_DROP_SALT     = 2000; // the supplies' stream's offset in the seed's waves (the armour's is 1000)

float ar_nav_n;                  // walkable places found (navplace(0 .. ar_nav_n - 1))
float ar_nav_hi_n;               // ... of them up high
float ar_nav_state;              // 0 not walked yet, 2 done
float ar_nav_low;                // the lowest place's origin height
vector ar_sc_at[AR_SCATTER_MAX]; // this clear's supplies so far (ar_sc_n of them, ar_sc_high up high: vr_arena.qc)
float ar_sc_scattered;           // the run's supplies put on random places (the tests read it)
float ar_ds1; float ar_ds2; float ar_ds3; // the supplies' rolls (a Levy triple, as VR_ARENA_Rand's)

float() VR_ARENA_Opt_Scatter = { return cvar("vr_arena_scatter") != 0; }
float() VR_ARENA_Opt_Shards  = { return VR_ARENA_Clamp(floor(cvar("vr_arena_shards")), 0, AR_SHARDS_MAX); }

// ---------------------------------------------------------------------------
// The supplies' rolls
// ---------------------------------------------------------------------------

void(float xWave) VR_ARENA_DropSeed =
{
    float base = VR_ARENA_Mod(cvar("vr_arena_seed"), 30000);
    if(cvar("vr_arena_seed") <= 0) { base = 1 + floor(29998 * random()); }
    base = base + 7919 * (xWave + AR_DROP_SALT);
    ar_ds1 = 1 + VR_ARENA_Mod(base, 30268);
    ar_ds2 = 1 + VR_ARENA_Mod(3 * base + 17, 30306);
    ar_ds3 = 1 + VR_ARENA_Mod(7 * base + 29, 30322);
}

float() VR_ARENA_DropRand =
{
    ar_ds1 = VR_ARENA_Mod(171 * ar_ds1, 30269);
    ar_ds2 = VR_ARENA_Mod(172 * ar_ds2, 30307);
    ar_ds3 = VR_ARENA_Mod(170 * ar_ds3, 30323);
    float r = ar_ds1 / 30269 + ar_ds2 / 30307 + ar_ds3 / 30323;
    return r - floor(r);
}

// A wave clear's armour shards: 1 to vr_arena_shards of them (0: none).
float() VR_ARENA_ShardCount =
{
    float most = VR_ARENA_Opt_Shards();
    if(most <= 0) { return 0; }
    return min(most, 1 + floor(VR_ARENA_DropRand() * most));
}

// ---------------------------------------------------------------------------
// The walkable places
// ---------------------------------------------------------------------------

// The walk, once a run (from its first frame with a player in, or the first wave clear before that).
void() VR_ARNAV_Frame =
{
    if(ar_nav_state == 2) { return; }
    ar_nav_state = 2;
    ar_nav_n = 0;
    ar_nav_hi_n = 0;

    vector from;
    entity s = find(world, classname, "info_player_start");
    if(s) { from = s.origin; }
    else
    {
        entity p = VR_ARENA_FindPlayer();
        if(!p) { ar_nav_state = 0; return; }
        from = p.origin;
    }
    tracebox(from + '0 0 32', VEC_HULL_MIN, VEC_HULL_MAX, from - '0 0 128', TRUE, world); // (a start's origin may sit low: vrarena's is 8 over its pad)
    if(trace_startsolid || trace_fraction >= 1)
    {
        dprint(sprintf("arena: supply places: no floor under the start %s\n", vtos(from)));
        return;
    }
    vector walked = navplace(-1, trace_endpos, AR_NAV_GRID, AR_NAV_MAX);
    ar_nav_n = walked_x;

    float k;
    for(k = 0; k < ar_nav_n; k++)
    {
        vector o = navplace(k);
        if(k == 0 || o_z < ar_nav_low) { ar_nav_low = o_z; }
    }
    for(k = 0; k < ar_nav_n; k++)
    {
        vector h = navplace(k);
        if(h_z - ar_nav_low >= AR_NAV_HIGH) { ar_nav_hi_n = ar_nav_hi_n + 1; }
    }
    dprint(sprintf("arena: supply places %g (%g high), lowest floor %g%s\n", ar_nav_n, ar_nav_hi_n, ar_nav_low - 24,
                   ar_nav_n >= AR_NAV_MAX ? " (the cap)" : ""));
}

float() VR_ARNAV_Ready = { return ar_nav_state == 2 && ar_nav_n > 0; }

// ---------------------------------------------------------------------------
// A supply's place
// ---------------------------------------------------------------------------

// A wave clear's supplies begin: none placed yet.
void(float xWave) VR_ARENA_ScatterBegin =
{
    VR_ARENA_DropSeed(xWave);
    ar_sc_n = 0;
    ar_sc_high = 0;
    if(VR_ARENA_Opt_Scatter()) { VR_ARNAV_Frame(); }
}

float(vector f) VR_ARENA_ScatterCrowded =
{
    float k;
    for(k = 0; k < ar_sc_n; k++) { if(vlen(ar_sc_at[k] - f) < AR_SCATTER_GAP) { return TRUE; } }
    return FALSE;
}

// Is a supply's place good (the tests' check too): out of any liquid, the player's box standing on it clear of the
// brushes, floor right under it, its own box clear of anything lying or standing there.
float(vector f) VR_ARENA_SupplyRoom =
{
    vector o = f + '0 0 24';
    if(pointcontents(o) != CONTENT_EMPTY || pointcontents(f + '0 0 2') != CONTENT_EMPTY) { return FALSE; }
    tracebox(o, VEC_HULL_MIN, VEC_HULL_MAX, o, TRUE, world);
    if(trace_startsolid) { return FALSE; }
    traceline(f + '0 0 2', f - '0 0 8', TRUE, world);
    if(trace_fraction >= 1) { return FALSE; }
    return VR_ARENA_BoxBlocker(f + '-24 -24 0', f + '24 24 64', world) == world;
}

// A supply's place (its floor) at random over the walkable places; FALSE in ar_place_ok when there is none (no walk
// in this map, every pick crowded): the caller falls back on the map's points.
vector() VR_ARENA_ScatterPlace =
{
    ar_place_ok = FALSE;
    if(!VR_ARENA_Opt_Scatter() || !VR_ARNAV_Ready()) { return '0 0 0'; }

    entity p = VR_ARENA_FindPlayer();
    float high = ar_nav_hi_n > 0 && VR_ARENA_DropRand() < AR_SCATTER_HIGH;
    float t;
    for(t = 0; t < AR_SCATTER_TRIES; t++)
    {
        if(t == AR_SCATTER_TRIES / 2) { high = FALSE; } // (no room up high: anywhere)
        vector o = navplace(min(ar_nav_n - 1, floor(VR_ARENA_DropRand() * ar_nav_n)));
        float up = o_z - ar_nav_low >= AR_NAV_HIGH;
        if(high && !up) { continue; } // (a pick up high: one of the high places, drawn again until it is)
        vector f = o - '0 0 24';
        if(p != world && vlen(o - p.origin) < AR_SCATTER_NEAR) { continue; }
        if(VR_ARENA_ScatterCrowded(f)) { continue; }
        if(!VR_ARENA_SupplyRoom(f)) { continue; }
        if(ar_sc_n < AR_SCATTER_MAX) { ar_sc_at[ar_sc_n] = f; ar_sc_n = ar_sc_n + 1; }
        if(up) { ar_sc_high = ar_sc_high + 1; }
        ar_sc_scattered = ar_sc_scattered + 1;
        ar_place_ok = TRUE;
        return f;
    }
    return '0 0 0';
}

// ---------------------------------------------------------------------------
// What came: the wave clear's line (VR_ARENA_WaveClear), said again once the perk cards are down
// ---------------------------------------------------------------------------

string(string xSoFar, string xMore) VR_ARENA_ListAdd =
{
    if(xSoFar == "") { return xMore; }
    return sprintf("%s, %s", xSoFar, xMore);
}

// "Supplies around the arena: 2 medkits, green armour, 2 armour shards" ("" when the clear dropped nothing).
string() VR_ARENA_SupplyLine =
{
    string s = "";
    if(ar_drop_health == 1) { s = "a medkit"; }
    else if(ar_drop_health > 1) { s = sprintf("%g medkits", ar_drop_health); }
    if(ar_drop_armour > 0) { s = VR_ARENA_ListAdd(s, ar_drop_armour_kind == 3 ? "yellow armour" : "green armour"); }
    if(ar_drop_shards == 1) { s = VR_ARENA_ListAdd(s, "an armour shard"); }
    else if(ar_drop_shards > 1) { s = VR_ARENA_ListAdd(s, sprintf("%g armour shards", ar_drop_shards)); }
    if(s == "") { return ""; }
    return sprintf("%s: %s", ar_sc_n > 0 ? "Supplies around the arena" : "Supplies", s);
}

void() VR_ARENA_SupplyFrame =
{
    if(ar_nav_state == 0 && VR_ARENA_Opt_Scatter()) { VR_ARNAV_Frame(); }
    if(ar_supply_say == 2) { ar_supply_say = ark_open ? 1 : 0; } // (the clear's own line stands when no cards came up)
    if(ar_supply_say == 1 && !ark_open)
    {
        ar_supply_say = 0;
        string s = VR_ARENA_SupplyLine();
        if(s != "") { VR_ARENA_ShowAll(sprintf("%s\n", s), AR_SAY); }
    }
}

// A run's start (VR_ARENA_WorldSpawn): the walk begins afresh.
void() VR_ARENA_SupplyReset =
{
    ar_nav_state = 0;
    ar_nav_n = 0;
    ar_nav_hi_n = 0;
    ar_sc_n = 0;
    ar_sc_high = 0;
    ar_sc_scattered = 0;
}

// vr_wornpack.qc -- worn backpacks (ROUND23.md, "Worn backpacks"; his rule of 2026-11-02): an armed grunt, knight or death
// knight that carries a ranged weapon as it spawns wears the backpack (progs/backpack.mdl on its back, as its model is drawn:
// the engine's agarmor::packWhere), and as a ragdoll the pack is a box on its chest part that weighs, collides and is held
// with it (vr_box3d_wornpack.cpp). Melee-only and unarmed ones wear none and drop none (VR_WP_Packless); the rule is read
// once, before it is seen: a pack never pops in or out in view, so one that picks up a gun later stays packless and one that
// loses its only gun keeps its pack. Not Mission Pack 3's campaign's (MG3_NoBackpack). An enforcer's is baked into its model
// (BACKLOG.md, making it a real one): whatever it holds, it has only the lift zone (.vr_wp_baked: an unseen pack entity,
// the engine's agarmor::packRest box round the drawn pack): lifted by it like any other, it never comes off, nothing counts
// on it, its loot its own.
// A hand's pull never takes it off: a living wearer pulled by its pack past vr_worn_backpack_lift_force is lifted by it
// (VR_WP_Lift: the engine's foegrab::packLift, as a two-hand lift: down, flailing, held by the pack on its chest part,
// carried and thrown, breaking free after vr_foegrab_lift_time); a gentler pull drags a ragdoll.
// It comes off, with a fabric rip (vr/pack_rip.wav), on
// - a ragdoll's hard landing (vr_worn_backpack_impact, m/s);
// - damage on the pack itself or an explosion's or fire's share (a blow, a throw, a shot, a projectile: vr_worn_backpack_pool;
//   vr_worn_backpack_hit_roll, if on, at once);
// - its wearer gibbed or gone;
// - a dead one's pack taken (grabbed or force-grabbed: only the dead's are takeable).
// Lost by a living one, the pack takes its spare rounds (vr_worn_backpack_reserve of each reserve) and its pockets: it
// fights on with what its weapons hold, then drops them as an empty one does. Dying, its loot goes into the pack it
// wears (a knight's and a death knight's too, who drop nothing else: VR_AG_Dying); a wearer drops no other backpack.

float VR_WP_WORN = 1; // on a living one
float VR_WP_DEAD = 2; // on a dead one: takeable
float VR_WP_OFF = 3;  // torn off, taken: a backpack item
.float vr_wp_size;    // a pack's: its size as drawn (its kind's vr_worn_backpack_<kind>_scale; the engine reads it, a hand on a dead one's)
.float vr_wp_baked;   // a pack's: an enforcer's lift zone, its pack the one in its model (nothing to draw, it can't come off)
string VR_WP_RIP_SOUND = VR_WP_RIP_SOUND_FILE;
float VR_WP_HIT_REACH = 3;  // units: a blow, a throw, a projectile this near the pack's box is on it
float VR_WP_SHOT_REACH = 0.1; // and a shot (hitscan: it meets the pack's box itself, vr_hitmodel.cpp's VR_HitModelClip)
float VR_WP_HAND_REACH = 0.92; // units: a hand this near a dead one's pack takes the pack, not the body (the palm's 3.5 cm sphere on its box: it was 3 units, 11 cm off it)
float VR_WP_PULL_SETTLE = 0.25; // s: a hold's first moments not counted (its spring taking up the body's weight)
.float vr_wp_hold_at;  // a pack's: since when a hand holds its ragdoll by it (0: none)
.float vr_wp_over_at;  // and since when that pull is past vr_worn_backpack_lift_force (0: not)
.float vr_wp_dmg;      // the damage its pack took (torn at vr_worn_backpack_pool)
.float vr_wp_lifted;   // its wearer lifted by it (since; 0: not), till it is let go of

void() VR_WP_Think;

float vr_wp_rips;        // packs torn off so far (the tests)
string vr_wp_rip_why;    // the last one's cause ("pulled", "landing", "hit", "gibbed", "taken")
string vr_wp_rip_sound;  // the last rip's sound
entity vr_wp_rip_pack;   // and its pack

void(string s) VR_WP_Log =
{
    if(cvar_hget(cvarh_vr_worn_backpack_debug))
    {
        dprint(sprintf("wornpack: %s\n", s));
    }
};

// Its size by its wearer's kind (.vr_ag_kind: the pack keeps it, its wearer may be gone).
float(entity pk) VR_WP_Scale =
{
    float s = pk.vr_ag_kind == VR_AM_KNIGHT ? cvar_hget(cvarh_vr_worn_backpack_knight_scale) :
        pk.vr_ag_kind == VR_AM_DKNIGHT ? cvar_hget(cvarh_vr_worn_backpack_dknight_scale) : cvar_hget(cvarh_vr_worn_backpack_grunt_scale);
    return min(2, max(0.1, s));
};

// Drawn at its size now (its slider shows at once).
void(entity pk) VR_WP_Size =
{
    float s = VR_WP_Scale(pk);
    if(s != pk.vr_wp_size)
    {
        pk.vr_wp_size = s;
        VR_ScaleUtil_SetScale(pk, VR_ScaleUtil_GetUniformScaleVector(s));
    }
};

// Its place on its wearer as drawn (its ragdoll's chest, else its posed model): FALSE when the wearer has none now (its
// model not a wearer's: gibbed, its head).
float(entity pk, entity g) VR_WP_Place =
{
    vector o = limbpiece(g, 0, 30); // (20..23 found at once: 30 its 20, then 31..33 its 21..23)
    if(o == '0 0 0')
    {
        return FALSE;
    }
    pk.angles = limbpiece(g, 0, 31);
    pk.origin = o; // (linked once, by setsize below: setorigin's link as well cost a second one each frame, PERF_1102.md)
    vector mid = limbpiece(g, 0, 32) - o;
    vector ext = limbpiece(g, 0, 33); // (the level box round its model's box as it is turned)
    if(ext == '0 0 0')
    {
        ext = '1 1 1' * 16.6 * VR_WP_Scale(pk);
    }
    setsize(pk, mid - ext, mid + ext);
    pk.velocity = g.velocity;
    return TRUE;
};

// Its wearer's pockets into the pack (their spare rounds; none: its monster's own loot).
void(entity pk, entity g) VR_WP_Stock =
{
    pk.ammo_shells = pk.ammo_shells + g.ammo_shells;
    pk.ammo_nails = pk.ammo_nails + g.ammo_nails;
    pk.ammo_rockets = pk.ammo_rockets + g.ammo_rockets;
    pk.ammo_cells = pk.ammo_cells + g.ammo_cells;
    pk.ammo_lava_nails = pk.ammo_lava_nails + g.ammo_lava_nails;
    pk.ammo_multi_rockets = pk.ammo_multi_rockets + g.ammo_multi_rockets;
    pk.ammo_plasma = pk.ammo_plasma + g.ammo_plasma;
    g.ammo_shells = g.ammo_nails = g.ammo_rockets = g.ammo_cells = 0;
    g.ammo_lava_nails = g.ammo_multi_rockets = g.ammo_plasma = 0;
    if(!(pk.ammo_shells + pk.ammo_nails + pk.ammo_rockets + pk.ammo_cells + pk.ammo_lava_nails + pk.ammo_multi_rockets +
         pk.ammo_plasma))
    {
        VR_AM_Row(g.vr_ag_kind);
        VR_AmmoUtil_SetAmmoCountByAmmoId(pk, vr_am_loot_aid, vr_am_loot_n);
    }
};

// Torn off a living armed one: its spare rounds go with the pack (vr_worn_backpack_reserve of each reserve), and its
// pockets; what its weapons hold stays.
void(entity pk, entity g) VR_WP_Lose =
{
    float share = min(1, max(0, cvar_hget(cvarh_vr_worn_backpack_reserve)));
    float n = floor(g.vr_ag_reserve * share);
    VR_AG_Pocket(pk, g.vr_ag_wid, g.vr_ag_wflags, n);
    g.vr_ag_reserve = g.vr_ag_reserve - n;
    if(g.vr_ag_prop2)
    {
        n = floor(g.vr_ag_reserve2 * share);
        VR_AG_Pocket(pk, g.vr_ag_wid2, g.vr_ag_wflags2, n);
        g.vr_ag_reserve2 = g.vr_ag_reserve2 - n;
    }
    if(g.vr_ag_main_wid > 0)
    {
        n = floor(g.vr_ag_main_reserve * share);
        VR_AG_Pocket(pk, g.vr_ag_main_wid, g.vr_ag_main_wflags, n);
        g.vr_ag_main_reserve = g.vr_ag_main_reserve - n;
    }
    VR_WP_Stock(pk, g);
    VR_WP_Log(sprintf("%s lost its pack alive: reserve now %g (shells in the pack %g)", g.classname, g.vr_ag_reserve,
        pk.ammo_shells));
};

// A backpack item now (Backpack_Setup), falling free at `vel`.
void(entity pk, vector vel) VR_WP_Free =
{
    vector mn = pk.mins;
    vector mx = pk.maxs;
    Backpack_Setup(pk, VR_WP_Scale(pk));
    setsize(pk, mn, mx); // (its box round it as it was turned on the back)
    pk.vr_monster_drop = time; // (it can't hurt players just yet: VR_Prop_Flung)
    pk.velocity = vel;
    pk.flags = pk.flags - (pk.flags & FL_ONGROUND);
};

// Off its wearer now (its ragdoll's box too), with a rip (every way it comes off). A living one's spare rounds go with it.
void(entity pk, string why) VR_WP_Off =
{
    entity g = pk.vr_wp_wearer;
    if(pk.vr_wp_state == VR_WP_OFF)
    {
        return;
    }
    if(g && g.vr_wp_pack == pk)
    {
        ragdollbone(g, '1 0 0', QVR_WORNPACK);
        if(pk.vr_wp_state == VR_WP_WORN)
        {
            VR_WP_Lose(pk, g);
        }
        else if(!(pk.ammo_shells + pk.ammo_nails + pk.ammo_rockets + pk.ammo_cells + pk.ammo_lava_nails +
                  pk.ammo_multi_rockets + pk.ammo_plasma))
        {
            VR_WP_Stock(pk, g); // (dead, its loot not yet counted: SoldierDrop's not run, gibbed first)
        }
        g.vr_wp_worn = 0;
    }
    pk.vr_wp_state = VR_WP_OFF;
    sound(pk, CHAN_AUTO, VR_WP_RIP_SOUND, min(1, max(0, cvar_hget(cvarh_vr_worn_backpack_volume))), ATTN_NORM);
    vr_wp_rips = vr_wp_rips + 1;
    vr_wp_rip_why = why;
    vr_wp_rip_sound = VR_WP_RIP_SOUND;
    vr_wp_rip_pack = pk;
    VR_WP_Log(sprintf("pack of %s torn off (%s)", g.classname, why));
};

// Torn off and flung at `vel`.
void(entity pk, string why, vector vel) VR_WP_Rip =
{
    if(pk.vr_wp_state == VR_WP_OFF)
    {
        return;
    }
    VR_WP_Off(pk, why);
    VR_WP_Free(pk, vel);
};

// A hand's pull on its ragdoll held by the pack now (N; -1: that hand doesn't hold it by the pack): the hold's force, or
// its stretch times vr_worn_backpack_lift_stiffness if more.
float(entity g, entity pl, float pnum, float hand) VR_WP_HandPull =
{
    if(VR_Hand_Thing(pl, hand) != g)
    {
        return -1;
    }
    float f = ragdollbone(g, [2, pnum, hand], QVR_WORNPACK);
    if(f < 0)
    {
        return -1; // (not held by its pack)
    }
    float s = ragdollbone(g, [3, pnum, hand], QVR_WORNPACK);
    if(cvar_hget(cvarh_vr_worn_backpack_debug) >= 2)
    {
        VR_WP_Log(sprintf("hand %g's hold: force %.0f N, the held point %.2f units off the hand (your fall %.0f u/s)", hand, f, s,
            -pl.velocity_z));
    }
    return max(f, s * max(0, cvar_hget(cvarh_vr_worn_backpack_lift_stiffness)));
};

float vr_wp_two_hands; // the last lift's hands (the tests: 2 both)
float vr_wp_lifts;     // wearers lifted by their packs (the tests)
float vr_wp_lift_pull; // and the last one's pull (N)

// Player `pl` (number `pnum`) pulling living `g` by its pack `pk` at `pull` N (`hands`): past vr_worn_backpack_lift_force
// for vr_worn_backpack_lift_hold it is lifted by the pack (standing: knocked down; a ragdoll held down already: its
// get-up held off; either way the engine's foegrab::packLift: flailing, held by the pack, breaking free after
// vr_foegrab_lift_time). TRUE: lifted now.
float(entity pk, entity g, entity pl, float pnum, float pull, float hands) VR_WP_Lift =
{
    if(g.health <= 0 || pk.vr_wp_lifted > 0 || pull < max(1, cvar_hget(cvarh_vr_worn_backpack_lift_force)))
    {
        pk.vr_wp_over_at = pk.vr_wp_lifted > 0 ? pk.vr_wp_over_at : 0;
        return FALSE;
    }
    if(pk.vr_wp_over_at <= 0)
    {
        pk.vr_wp_over_at = time;
    }
    if(time - pk.vr_wp_over_at < max(0, cvar_hget(cvarh_vr_worn_backpack_lift_hold)))
    {
        return FALSE;
    }
    float down = isragdoll(g);
    if(down)
    {
        if(!g.vr_knockdown)
        {
            return FALSE; // (staggering: a hand on it takes it down first, then it can be lifted)
        }
        g.vr_kd_until = max(g.vr_kd_until, time + (max(0.1, cvar_hget(cvarh_vr_foegrab_lift_time)) + 1) * VR_KD_DownScale(g));
    }
    float held = ragdollbone(g, [8, pnum, 0], QVR_WORNPACK);
    if(held < 1)
    {
        VR_WP_Log(sprintf("pulled at %.0f N: %s can't be lifted now", pull, g.classname));
        pk.vr_wp_over_at = 0;
        return FALSE;
    }
    pk.vr_wp_lifted = time;
    vr_wp_lifts = vr_wp_lifts + 1;
    vr_wp_lift_pull = pull;
    vr_wp_two_hands = hands;
    VR_WP_Log(sprintf("pulled at %.0f N (%g hands): %s lifted by its pack (%s), held by %g hands", pull, hands, g.classname,
        down ? "down already" : "standing", held));
    return TRUE;
};

// A player's pull on its ragdoll held by the pack, each frame (`pull` N, both hands' added times
// vr_worn_backpack_two_hand_mult): a living one lifted past vr_worn_backpack_lift_force. (Player number `pnum`.)
void(entity pk, entity g, entity pl, float pnum, float pull, float hands) VR_WP_Pull =
{
    if(pk.vr_wp_hold_at <= 0)
    {
        pk.vr_wp_hold_at = time;
    }
    if(cvar_hget(cvarh_vr_worn_backpack_debug) >= 2)
    {
        VR_WP_Log(sprintf("held %.3f s: pull %.0f N (%g hands)", time - pk.vr_wp_hold_at, pull, hands));
    }
    // (Its first moments ignored: the hold's spring taking up the body's weight jerks; then the pull kept up a while.)
    if(time - pk.vr_wp_hold_at < VR_WP_PULL_SETTLE)
    {
        return;
    }
    pk.vr_wp_peak = max(pk.vr_wp_peak, pull);
    VR_WP_Lift(pk, g, pl, pnum, pull, hands);
};

// The players' hands holding its ragdoll by the pack.
void(entity pk, entity g) VR_WP_Pulls =
{
    entity pl = nextent(world);
    float pnum = 1;
    float held = FALSE;
    // (ends once no player is left after it: those edicts do nothing here; find() steps through the classname index)
    while(pl && pnum <= 16 && (pl.classname == "player" || find(pl, classname, "player")))
    {
        if(pl.classname == "player")
        {
            float pm =VR_WP_HandPull(g, pl, pnum, cVR_MainHand);
            float po = VR_WP_HandPull(g, pl, pnum, cVR_OffHand);
            if(pm >= 0 || po >= 0)
            {
                held = TRUE;
                float hands = (pm >= 0) + (po >= 0);
                float pull = max(0, pm) + max(0, po);
                if(hands == 2)
                {
                    pull = pull * max(1, cvar_hget(cvarh_vr_worn_backpack_two_hand_mult));
                }
                VR_WP_Pull(pk, g, pl, pnum, pull, hands);
            }
        }
        pl = nextent(pl);
        pnum = pnum + 1;
    }
    if(!held)
    {
        pk.vr_wp_hold_at = 0; // (let go of: the next hold settles again)
        pk.vr_wp_over_at = 0;
        pk.vr_wp_lifted = 0;
    }
};

// A living one's pack held by your hands (the engine's foegrab pack hold: the fist on it took it; it spotted you), each
// frame: the hands' stretch off the pack as a force (vr_worn_backpack_lift_stiffness; both hands: added, times
// vr_worn_backpack_two_hand_mult) lifts it past the lift force.
void(entity pk, entity g) VR_WP_Tug =
{
    entity pl = nextent(world);
    float pnum = 1;
    float held = FALSE;
    // (ends once no player is left after it: those edicts do nothing here; find() steps through the classname index)
    while(pl && pnum <= 16 && (pl.classname == "player" || find(pl, classname, "player")))
    {
        if(pl.classname == "player")
        {
            float sm =ragdollbone(g, [5, pnum, cVR_MainHand], QVR_WORNPACK);
            float so = ragdollbone(g, [5, pnum, cVR_OffHand], QVR_WORNPACK);
            if(sm >= 0 || so >= 0)
            {
                held = TRUE;
                float hands = (sm >= 0) + (so >= 0);
                float force = (max(0, sm) + max(0, so)) * max(0, cvar_hget(cvarh_vr_worn_backpack_lift_stiffness));
                force = force * (hands == 2 ? max(1, cvar_hget(cvarh_vr_worn_backpack_two_hand_mult)) : 1);
                pk.vr_wp_peak = max(pk.vr_wp_peak, force);
                if(cvar_hget(cvarh_vr_worn_backpack_debug) >= 2)
                {
                    VR_WP_Log(sprintf("tug: %.0f N (%g hands; main %.1f, off %.1f units)", force, hands, sm, so));
                }
                if(VR_WP_Lift(pk, g, pl, pnum, force, hands))
                {
                    return;
                }
            }
        }
        pl = nextent(pl);
        pnum = pnum + 1;
    }
    if(!held)
    {
        pk.vr_wp_over_at = 0;
        pk.vr_wp_lifted = 0;
    }
};

// Its wearer just died: its pack takeable (grabbed or force-grabbed it comes off: VR_WP_Taken), still on its back.
void(entity pk) VR_WP_Dead =
{
    if(pk.vr_wp_state != VR_WP_WORN)
    {
        return;
    }
    pk.vr_wp_state = VR_WP_DEAD;
    vector mn = pk.mins;
    vector mx = pk.maxs;
    Backpack_Setup(pk, VR_WP_Scale(pk));
    setsize(pk, mn, mx);
    pk.model_offset = '0 0 0'; // (drawn where it sits on the back: not a lying item's offset)
    pk.movetype = MOVETYPE_NONE; // (placed on its back each frame, not falling: VR_WP_Think)
    pk.vr_rigid = 0;
    pk.velocity = '0 0 0';
    pk.think = VR_WP_Think;
    pk.nextthink = time;
    pk.nextthink2 = 0; // (no fading while it is worn)
    VR_WP_Log(sprintf("its wearer dead: takeable (shells %g nails %g)", pk.ammo_shells, pk.ammo_nails));
};

// Every frame while worn: on its wearer's back (hidden while it is), pulled at, or torn off as its wearer goes.
void() VR_WP_Think =
{
    entity pk = self;
    entity g = pk.vr_wp_wearer;
    if(pk.vr_wp_state == VR_WP_OFF)
    {
        return;
    }
    pk.nextthink = time;
    if(pk.vr_wp_baked)
    {
        // (A lift zone: only pulled at; it ends with its wearer's life, quietly.)
        if(!g || g.vr_wp_pack != pk || g.health <= 0)
        {
            if(g && g.vr_wp_pack == pk)
            {
                ragdollbone(g, '1 0 0', QVR_WORNPACK);
                g.vr_wp_worn = 0;
                g.vr_wp_pack = world;
            }
            remove(pk);
            return;
        }
        if(g.model == "" || g.MG_deferred)
        {
            return;
        }
        if(isragdoll(g))
        {
            VR_WP_Pulls(pk, g);
        }
        else
        {
            VR_WP_Tug(pk, g);
        }
        return;
    }
    if(!g || g.vr_wp_pack != pk)
    {
        VR_WP_Rip(pk, "its wearer gone", pk.velocity); // (removed, faded)
        return;
    }
    if(g.health <= 0 && pk.vr_wp_state == VR_WP_WORN)
    {
        VR_WP_Dead(pk); // (its loot counted as it drops it: VR_WP_DeathLoot)
    }
    if(g.model == "" || g.MG_deferred)
    {
        setmodel(pk, ""); // (unseen with it)
        return;
    }
    if(pk.model == "")
    {
        setmodel(pk, "progs/backpack.mdl");
    }
    VR_WP_Size(pk);
    if(!VR_WP_Place(pk, g))
    {
        // (Gibbed: its model a head now.)
        VR_WP_Rip(pk, "gibbed", g.velocity + [crandom() * 150, crandom() * 150, 250]);
        return;
    }
    if(isragdoll(g))
    {
        VR_WP_Pulls(pk, g);
    }
    else if(pk.vr_wp_state == VR_WP_WORN && g.health > 0)
    {
        VR_WP_Tug(pk, g);
    }
};

// A ranged weapon (not fists, not a melee weapon).
float(float wid) VR_WP_Ranged =
{
    return wid > 0 && VR_AG_Class(wid) != VR_AG_CLASS_FISTS && VR_AG_Class(wid) != VR_AG_CLASS_MELEE;
};

// The pack's rule: it carries a ranged weapon now, in a hand or stowed (behind a flare gun).
float(entity e) VR_WP_CarriesRanged =
{
    return VR_WP_Ranged(e.vr_ag_wid) || (e.vr_ag_prop2 && VR_WP_Ranged(e.vr_ag_wid2)) || VR_WP_Ranged(e.vr_ag_main_wid);
};

// An armed one the pack's rule left without one: it drops no backpack either. (Enforcers aren't under the rule yet, nor
// Mission Pack 3's campaign's, whose monsters drop none anyway.)
float(entity e) VR_WP_Packless =
{
    return e.vr_ag && e.vr_ag_kind != VR_AM_ENFORCER && !e.vr_wp_had && !MG3_NoBackpack(e);
};

void(entity e) VR_WP_Spawn =
{
    if(!e.vr_ag || e.vr_wp_pack || e.vr_dummy_dead)
    {
        return;
    }
    if(e.vr_ag_kind == VR_AM_ENFORCER)
    {
        // (Whatever it holds, Mission Pack 3's too: the pack is on its model for all to see. His note 2026-11-03 02:25:39.)
        entity zone = spawn();
        zone.classname = "vr_wornpack";
        zone.netname = "backpack";
        zone.solid = SOLID_NOT;
        zone.movetype = MOVETYPE_NONE;
        zone.vr_wp_baked = 1;
        zone.vr_wp_wearer = e;
        zone.vr_wp_state = VR_WP_WORN;
        zone.think = VR_WP_Think;
        zone.nextthink = time;
        e.vr_wp_pack = zone;
        e.vr_wp_worn = 1; // (the engine's hold, lift and ragdoll box on its model's pack; not .vr_wp_had: its loot its own)
        setorigin(zone, e.origin);
        VR_WP_Log(sprintf("%s has a lift zone on its model's pack", e.classname));
        return;
    }
    if(MG3_NoBackpack(e) || !VR_WP_CarriesRanged(e))
    {
        return;
    }
    precache_model("progs/backpack.mdl");
    precache_sound(VR_WP_RIP_SOUND);
    entity pk = spawn();
    pk.classname = "vr_wornpack";
    pk.netname = "backpack";
    setmodel(pk, "progs/backpack.mdl");
    pk.vr_ag_kind = e.vr_ag_kind;
    VR_WP_Size(pk);
    pk.solid = SOLID_NOT; // (no hand takes it from the living, no force grab finds it)
    pk.movetype = MOVETYPE_NONE;
    pk.vr_wp_wearer = e;
    pk.vr_wp_state = VR_WP_WORN;
    pk.think = VR_WP_Think;
    pk.nextthink = time;
    e.vr_wp_pack = pk;
    e.vr_wp_worn = 1;
    e.vr_wp_had = 1;
    setorigin(pk, e.origin);
    VR_WP_Log(sprintf("%s wears a pack", e.classname));
};

// Its loadout changed before anyone saw it (a replacement's, rolled as a player came in: VR_AGR_Resolve): the rule read again.
void(entity e) VR_WP_Refit =
{
    if(e.vr_wp_pack && !e.vr_wp_pack.vr_wp_baked && !VR_WP_CarriesRanged(e))
    {
        remove(e.vr_wp_pack);
        e.vr_wp_pack = world;
        e.vr_wp_worn = 0;
        e.vr_wp_had = 0;
        return;
    }
    VR_WP_Spawn(e);
};

float vr_wp_hit_counted; // the last damage counted on a pack (the tests; 0: none)

// Damage to a living wearer (T_Damage): what lands on its pack counts towards vr_worn_backpack_pool (a blow, a throw, a
// shot, a projectile there; vr_worn_backpack_pool_blast of an explosion's anywhere, _pool_fire of fire's: the
// flamethrower, burning; never poison); at the pool it tears off (and vr_worn_backpack_hit_roll, if on, may tear it off
// at once on a hit there). It spots the attacker if it hadn't (a hit on its pack wakes it).
void(entity targ, entity inflictor, entity attacker, float take) VR_WP_Hit =
{
    entity pk = targ.vr_wp_pack;
    vr_wp_hit_counted = 0;
    if(!pk || pk.vr_wp_baked || pk.vr_wp_state != VR_WP_WORN || pk.vr_wp_wearer != targ || inflictor == world || take <= 0)
    {
        return;
    }
    if(inflictor.classname == "vr_adpoison" || inflictor.vr_adgun_poison > 0)
    {
        return; // (poison: the pack doesn't mind)
    }
    float share = 1;
    string kind = "hit";
    float d = 0;
    if(hit_kind == QVR_HITKIND_SPLASH)
    {
        share = min(1, max(0, cvar_hget(cvarh_vr_worn_backpack_pool_blast)));
        kind = "blast";
    }
    else if(inflictor.classname == "vr_burn" || inflictor.classname == "vr_adflame")
    {
        share = min(1, max(0, cvar_hget(cvarh_vr_worn_backpack_pool_fire)));
        kind = "fire";
    }
    else
    {
        VR_HitReact_Where(targ, inflictor, attacker);
        if(vr_hr_at == (targ.absmin + targ.absmax) * 0.5)
        {
            return; // (where it struck isn't known)
        }
        d = ragdollbone(targ, vr_hr_at, QVR_WORNPACK_AT);
        if(d < 0 || d > (hit_kind == QVR_HITKIND_SHOT ? VR_WP_SHOT_REACH : VR_WP_HIT_REACH))
        {
            return;
        }
    }
    if(share <= 0)
    {
        return;
    }
    vr_wp_hit_counted = take * share;
    pk.vr_wp_dmg = pk.vr_wp_dmg + vr_wp_hit_counted;
    float pool = max(0, cvar_hget(cvarh_vr_worn_backpack_pool));
    VR_WP_Log(sprintf("%s on its pack (%.1f units off): %.1f damage counted, %.1f of %g", kind, d, vr_wp_hit_counted, pk.vr_wp_dmg,
        pool));
    if((attacker.flags & FL_CLIENT) && targ.enemy == world && targ.health > 0 && (targ.th_run || VR_Stealth_AD(targ)))
    {
        VR_Stealth_Spot(targ, attacker);
    }
    float torn = FALSE;
    if(pool > 0)
    {
        torn = pk.vr_wp_dmg >= pool;
    }
    if(!torn)
    {
        if(kind == "hit")
        {
            float chance = min(1, max(0, cvar_hget(cvarh_vr_worn_backpack_hit_roll)) * take / max(1, cvar_hget(cvarh_vr_worn_backpack_hit_ref)));
            if(chance > 0)
            {
                torn = random() < chance;
            }
        }
    }
    if(!torn)
    {
        return;
    }
    vector from = kind == "hit" ? vr_hr_at : limbpiece(targ, 0, 22);
    vector dir = normalize(from - (inflictor.origin == from ? attacker.origin : inflictor.origin));
    VR_WP_Rip(pk, kind == "hit" ? "hit" : kind, targ.velocity + dir * 180 + '0 0 120');
};

void(entity m, vector xAt, float xSpeed) VR_WP_Impact =
{
    entity pk = m.vr_wp_pack;
    if(!pk || pk.vr_wp_baked || pk.vr_wp_state == VR_WP_OFF || xSpeed < max(0.5, cvar_hget(cvarh_vr_worn_backpack_impact)) || !isragdoll(m))
    {
        return;
    }
    VR_WP_Log(sprintf("a landing at %.1f m/s", xSpeed));
    VR_WP_Rip(pk, "landing", m.velocity * 0.5 + '0 0 160');
};

void(entity e) VR_WP_Taken =
{
    if(e.classname != "vr_wornpack" || e.vr_wp_state == VR_WP_OFF)
    {
        return;
    }
    VR_WP_Off(e, "taken");
    e.think = backpack_think;
    e.nextthink = time + 0.02;
    e.nextthink2 = time + 120;
    e.movetype = MOVETYPE_TOSS;
    e.vr_rigid = 1;
};

float(entity m, entity pl, float hand) VR_WP_HandOnPack =
{
    entity pk = m.vr_wp_pack;
    if(!pk || pk.vr_wp_state != VR_WP_DEAD)
    {
        return FALSE;
    }
    float d = ragdollbone(m, VRGetEntHandPos(pl, hand), QVR_WORNPACK_AT);
    return d >= 0 && d <= VR_WP_HAND_REACH;
};

float vr_wp_deathloots; // the wearers' deaths counted (the tests)

float() VR_WP_DeathLoot =
{
    if(!self.vr_wp_had)
    {
        return FALSE;
    }
    vr_wp_deathloots = vr_wp_deathloots + 1;
    entity pk = self.vr_wp_pack;
    if(pk && pk.vr_wp_wearer == self && pk.vr_wp_state != VR_WP_OFF)
    {
        VR_WP_Stock(pk, self);
        VR_WP_Dead(pk);
    }
    return TRUE;
};

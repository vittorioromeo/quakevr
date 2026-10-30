// vr_wornpack.qc -- worn backpacks (experimental, vr_worn_backpacks 0 by default; ROUND23.md, "Worn backpacks"): an armed
// grunt wears the backpack it would drop (progs/backpack.mdl on its back, as its model is drawn: the engine's
// agarmor::packWhere), and as a ragdoll the pack is a box on its chest part that weighs, collides and is held with it
// (vr_box3d_wornpack.cpp). It comes off, with a fabric rip (vr/derived/fabric_rip.wav), on
// - a hand's hard pull on a ragdoll held by its pack (vr_worn_backpack_rip_force): the pack is then in that hand;
// - a ragdoll's hard landing (vr_worn_backpack_impact, m/s);
// - a hit on the pack itself (a blow, a throw, a shot, a projectile: vr_worn_backpack_hit_chance at _hit_ref damage);
// - its wearer gibbed or gone;
// - a dead one's pack taken (grabbed or force-grabbed: only the dead's are takeable).
// Torn off a living one, the pack takes its spare rounds (vr_worn_backpack_reserve of each reserve) and its pockets: it
// fights on with what its weapons hold, then drops them as an empty one does. Dying, its loot goes into the pack it
// wears; a wearer drops no other backpack.

float VR_WP_WORN = 1; // on a living one
float VR_WP_DEAD = 2; // on a dead one: takeable
float VR_WP_OFF = 3;  // torn off, taken: a backpack item
string VR_WP_RIP_SOUND = "vr/derived/fabric_rip.wav";
float VR_WP_HIT_REACH = 3;  // units: a blow, a throw, a projectile this near the pack's box is on it
float VR_WP_SHOT_REACH = 0.1; // and a shot (hitscan: it meets the pack's box itself, vr_hitmodel.cpp's VR_HitModelClip)
float VR_WP_HAND_REACH = 3; // units: a hand this near a dead one's pack takes the pack, not the body
float VR_WP_PULL_SETTLE = 0.25; // s: a hold's first moments not counted (its spring taking up the body's weight)
.float vr_wp_hold_at;  // a pack's: since when a hand holds its ragdoll by it (0: none)
.float vr_wp_over_at;  // and since when that pull is past vr_worn_backpack_rip_force (0: not); a living one's tug: its first counted pull
.float vr_wp_prog;     // a living one's tug: how far on (s of pull at vr_worn_backpack_tug_stretch: torn at vr_worn_backpack_tug_time)
.float vr_wp_dmg;      // the damage its pack took (torn at vr_worn_backpack_pool)

void() VR_WP_Think;

float vr_wp_rips;        // packs torn off so far (the tests)
string vr_wp_rip_why;    // the last one's cause ("pulled", "landing", "hit", "gibbed", "taken")
string vr_wp_rip_sound;  // the last rip's sound
entity vr_wp_rip_pack;   // and its pack

void(string s) VR_WP_Log =
{
    if(cvar("vr_worn_backpack_debug"))
    {
        dprint(sprintf("wornpack: %s\n", s));
    }
};

float() VR_WP_Scale = { return min(2, max(0.1, cvar("vr_worn_backpack_scale"))); };

// Its place on its wearer as drawn (its ragdoll's chest, else its posed model): FALSE when the wearer has none now (its
// model not a wearer's: gibbed, its head).
float(entity pk, entity g) VR_WP_Place =
{
    vector o = limbpiece(g, 0, 20);
    if(o == '0 0 0')
    {
        return FALSE;
    }
    pk.angles = limbpiece(g, 0, 21);
    setorigin(pk, o);
    vector mid = limbpiece(g, 0, 22) - o;
    vector ext = limbpiece(g, 0, 23); // (the level box round its model's box as it is turned)
    if(ext == '0 0 0')
    {
        ext = '1 1 1' * 16.6 * VR_WP_Scale();
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
    float share = min(1, max(0, cvar("vr_worn_backpack_reserve")));
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
    Backpack_Setup(pk, VR_WP_Scale());
    setsize(pk, mn, mx); // (its box round it as it was turned on the back)
    pk.vr_monster_drop = time; // (it can't hurt players just yet: VR_Prop_Flung)
    pk.velocity = vel;
    pk.flags = pk.flags - (pk.flags & FL_ONGROUND);
};

// Off its wearer now (its ragdoll's box too), with a rip. A living one's spare rounds go with it.
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
    sound(pk, CHAN_AUTO, VR_WP_RIP_SOUND, min(1, max(0, cvar("vr_worn_backpack_volume"))), ATTN_NORM);
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
// its stretch times vr_worn_backpack_rip_stiffness if more.
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
    return max(f, s * max(0, cvar("vr_worn_backpack_rip_stiffness")));
};

float vr_wp_two_hands; // the last pull's hands (the tests: 2 both)

// A player's pull on its ragdoll held by the pack, each frame (`pull` N, both hands' added times
// vr_worn_backpack_two_hand_mult): past vr_worn_backpack_rip_force it tears off into `hand` (the harder pulling). TRUE: torn off.
float(entity pk, entity g, entity pl, float hand, float pull, float hands) VR_WP_Pull =
{
    if(pk.vr_wp_hold_at <= 0)
    {
        pk.vr_wp_hold_at = time;
    }
    if(cvar("vr_worn_backpack_debug") >= 2)
    {
        VR_WP_Log(sprintf("held %.3f s: pull %.0f N (%g hands)", time - pk.vr_wp_hold_at, pull, hands));
    }
    // (Its first moments ignored: the hold's spring taking up the body's weight jerks; then the pull kept up a while.)
    if(time - pk.vr_wp_hold_at < VR_WP_PULL_SETTLE)
    {
        return FALSE;
    }
    pk.vr_wp_peak = max(pk.vr_wp_peak, pull);
    if(pull < max(1, cvar("vr_worn_backpack_rip_force")))
    {
        pk.vr_wp_over_at = 0;
        return FALSE;
    }
    if(pk.vr_wp_over_at <= 0)
    {
        pk.vr_wp_over_at = time;
    }
    if(time - pk.vr_wp_over_at < max(0, cvar("vr_worn_backpack_rip_time")))
    {
        return FALSE;
    }
    VR_WP_Log(sprintf("pulled off at %.0f N (%g hands)", pull, hands));
    vr_wp_two_hands = hands;
    if(hands == 2 && VR_Hand_Thing(pl, VRGetOtherHand(hand)) == g)
    {
        ragdollrelease(pl, VRGetOtherHand(hand), '0 0 0'); // (the other hand on it too: it lets go, the pack in this one)
        VR_Hand_Release(pl, VRGetOtherHand(hand), "wornpack");
    }
    ragdollrelease(pl, hand, '0 0 0');
    VR_Hand_Release(pl, hand, "wornpack");
    VR_WP_Off(pk, "pulled");
    VR_WP_Free(pk, '0 0 0');
    VR_Carry_Start(pl, hand, pk);
    return TRUE;
};

// The players' hands holding its ragdoll by the pack.
void(entity pk, entity g) VR_WP_Pulls =
{
    entity pl = nextent(world);
    float pnum = 1;
    float held = FALSE;
    while(pl && pnum <= 16)
    {
        if(pl.classname == "player")
        {
            float pm = VR_WP_HandPull(g, pl, pnum, cVR_MainHand);
            float po = VR_WP_HandPull(g, pl, pnum, cVR_OffHand);
            if(pm >= 0 || po >= 0)
            {
                held = TRUE;
                float hands = (pm >= 0) + (po >= 0);
                float pull = max(0, pm) + max(0, po);
                if(hands == 2)
                {
                    pull = pull * max(1, cvar("vr_worn_backpack_two_hand_mult"));
                }
                if(VR_WP_Pull(pk, g, pl, pm >= po ? cVR_MainHand : cVR_OffHand, pull, hands))
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
        pk.vr_wp_hold_at = 0; // (let go of: the next hold settles again)
        pk.vr_wp_over_at = 0;
    }
};

float vr_wp_topples;   // knockdowns by a tear (the tests)
vector vr_wp_topple_dir; // and the last one's way (level)
float vr_wp_tug_secs;  // the last living tug's time, from its first counted pull to the tear (the tests)

// Torn off a living one by your pull (vr_worn_backpack_topple, a ragdoll trigger): knocked down as a shove knocks it,
// falling the way you tore (`dir`, level).
void(entity g, vector dir) VR_WP_Topple =
{
    dir_z = 0;
    if(!cvar("vr_worn_backpack_topple") || vlen(dir) < 0.01 || g.health <= 0 || g.vr_knockdown || !canragdoll(g))
    {
        return;
    }
    dir = normalize(dir);
    if(!VR_Knockdown_MakeRoom(g))
    {
        VR_WP_Log("torn off: no room to topple");
        return;
    }
    float slide = VR_Shove_Reach(g);
    vector wasVel = g.velocity;
    float wasFlags = g.flags;
    g.velocity = '0 0 0';
    g.flags = g.flags | FL_ONGROUND;
    if(!VR_Knockdown_Start(g))
    {
        g.velocity = wasVel;
        g.flags = wasFlags;
        return;
    }
    ragdollshovetopple(g, dir, min(2, max(0.1, cvar("vr_worn_backpack_topple_push"))), FALSE, slide);
    vr_wp_topples = vr_wp_topples + 1;
    vr_wp_topple_dir = dir;
    VR_WP_Log(sprintf("torn off: %s topples along (%.2f %.2f)", g.classname, dir_x, dir_y));
};

// A living one's pack held by your hands (the engine's foegrab pack hold: the fist on it took it; it spotted you), each
// frame: a hand pulled vr_worn_backpack_tug_stretch cm or more off its place counts (both hands: their pulls added, times
// vr_worn_backpack_two_hand_mult), harder sooner; kept up vr_worn_backpack_tug_time it tears off into the harder pulling
// hand (its spare rounds with it), and the grunt topples that way (VR_WP_Topple).
void(entity pk, entity g) VR_WP_Tug =
{
    float need = max(0.5, cvar("vr_worn_backpack_tug_stretch"));
    float cm = VRMetersToUnits() * 0.01;
    entity pl = nextent(world);
    float pnum = 1;
    float held = FALSE;
    while(pl && pnum <= 16)
    {
        if(pl.classname == "player")
        {
            float sm = ragdollbone(g, [5, pnum, cVR_MainHand], QVR_WORNPACK);
            float so = ragdollbone(g, [5, pnum, cVR_OffHand], QVR_WORNPACK);
            if(sm >= 0 || so >= 0)
            {
                held = TRUE;
                float hands = (sm >= 0) + (so >= 0);
                float pull = (max(0, sm) + max(0, so)) / cm;
                if(hands == 2)
                {
                    pull = pull * max(1, cvar("vr_worn_backpack_two_hand_mult"));
                }
                pk.vr_wp_peak = max(pk.vr_wp_peak, pull);
                if(pull >= need)
                {
                    if(pk.vr_wp_over_at <= 0)
                    {
                        pk.vr_wp_over_at = time;
                    }
                    pk.vr_wp_prog = pk.vr_wp_prog + frametime * pull / need;
                }
                else
                {
                    pk.vr_wp_prog = max(0, pk.vr_wp_prog - frametime * 2); // (let up: it gets its footing back)
                }
                if(cvar("vr_worn_backpack_debug") >= 2)
                {
                    VR_WP_Log(sprintf("tug: pull %.1f cm (%g hands; main %.1f, off %.1f units), %.2f of %.2f s", pull, hands, sm,
                        so, pk.vr_wp_prog, cvar("vr_worn_backpack_tug_time")));
                }
                if(pk.vr_wp_prog >= max(0.01, cvar("vr_worn_backpack_tug_time")))
                {
                    // (Into the main hand if it holds it; the way: the hands' pull, each palm from its spot on the pack.)
                    float hand = sm >= 0 ? cVR_MainHand : cVR_OffHand;
                    vector dir = limbpiece(g, pnum, 24);
                    vr_wp_tug_secs = time - pk.vr_wp_over_at;
                    vr_wp_two_hands = hands;
                    VR_WP_Log(sprintf("tug: torn off a living %s by %g hands, pulling %.1f cm, after %.2f s", g.classname, hands,
                        pull, vr_wp_tug_secs));
                    ragdollbone(g, [6, 0, 0], QVR_WORNPACK); // (the hands' holds on it end: the grips the QC's)
                    VR_WP_Off(pk, "pulled");
                    VR_WP_Free(pk, '0 0 0');
                    VR_Carry_Start(pl, hand, pk);
                    VR_WP_Topple(g, dir);
                    return;
                }
            }
        }
        pl = nextent(pl);
        pnum = pnum + 1;
    }
    if(!held)
    {
        pk.vr_wp_prog = 0;
        pk.vr_wp_over_at = 0;
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
    Backpack_Setup(pk, VR_WP_Scale());
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

void(entity e) VR_WP_Spawn =
{
    if(!cvar("vr_worn_backpacks") || !e.vr_ag || e.vr_ag_kind != VR_AM_GRUNT || e.vr_wp_pack || MG3_NoBackpack(e) ||
       e.vr_dummy_dead)
    {
        return;
    }
    precache_model("progs/backpack.mdl");
    precache_sound(VR_WP_RIP_SOUND);
    entity pk = spawn();
    pk.classname = "vr_wornpack";
    pk.netname = "backpack";
    setmodel(pk, "progs/backpack.mdl");
    VR_ScaleUtil_SetScale(pk, VR_ScaleUtil_GetUniformScaleVector(VR_WP_Scale()));
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

float vr_wp_hit_counted; // the last damage counted on a pack (the tests; 0: none)

// Damage to a living wearer (T_Damage): what lands on its pack counts towards vr_worn_backpack_pool (a blow, a throw, a
// shot, a projectile there; vr_worn_backpack_pool_blast of an explosion's anywhere, _pool_fire of fire's: the
// flamethrower, burning; never poison); at the pool it tears off (and vr_worn_backpack_hit_roll, if on, may tear it off
// at once on a hit there). It spots the attacker if it hadn't (a hit on its pack wakes it).
void(entity targ, entity inflictor, entity attacker, float take) VR_WP_Hit =
{
    entity pk = targ.vr_wp_pack;
    vr_wp_hit_counted = 0;
    if(!pk || pk.vr_wp_state != VR_WP_WORN || pk.vr_wp_wearer != targ || inflictor == world || take <= 0)
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
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        share = min(1, max(0, cvar("vr_worn_backpack_pool_blast")));
        kind = "blast";
    }
    else if(inflictor.classname == "vr_burn" || inflictor.classname == "vr_adflame")
    {
        share = min(1, max(0, cvar("vr_worn_backpack_pool_fire")));
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
        if(d < 0 || d > (vr_hitkind == QVR_HITKIND_SHOT ? VR_WP_SHOT_REACH : VR_WP_HIT_REACH))
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
    float pool = max(0, cvar("vr_worn_backpack_pool"));
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
            float chance = min(1, max(0, cvar("vr_worn_backpack_hit_roll")) * take / max(1, cvar("vr_worn_backpack_hit_ref")));
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
    if(!pk || pk.vr_wp_state == VR_WP_OFF || xSpeed < max(0.5, cvar("vr_worn_backpack_impact")) || !isragdoll(m))
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

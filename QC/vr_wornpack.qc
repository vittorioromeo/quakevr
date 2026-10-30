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
float VR_WP_HIT_REACH = 3;  // units: a hit this near the pack's box is on it
float VR_WP_HAND_REACH = 3; // units: a hand this near a dead one's pack takes the pack, not the body
float VR_WP_PULL_SETTLE = 0.25; // s: a hold's first moments not counted (its spring taking up the body's weight)
.float vr_wp_hold_at;  // a pack's: since when a hand holds its ragdoll by it (0: none)
.float vr_wp_over_at;  // and since when that pull is past vr_worn_backpack_rip_force (0: not)

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
    float r = 16.6 * VR_WP_Scale(); // (the model's half diagonal: a box round it however it is turned)
    setsize(pk, mid - '1 1 1' * r, mid + '1 1 1' * r);
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

// A hand's pull on its ragdoll held by the pack, each frame: past vr_worn_backpack_rip_force it tears off into that hand.
// TRUE: torn off.
float(entity pk, entity g, entity pl, float pnum, float hand) VR_WP_Pull =
{
    if(VR_Hand_Thing(pl, hand) != g)
    {
        return FALSE;
    }
    float f = ragdollbone(g, [2, pnum, hand], QVR_WORNPACK);
    if(f < 0)
    {
        return FALSE; // (not held by its pack)
    }
    float s = ragdollbone(g, [3, pnum, hand], QVR_WORNPACK);
    float pull = max(f, s * max(0, cvar("vr_worn_backpack_rip_stiffness")));
    if(pk.vr_wp_hold_at <= 0)
    {
        pk.vr_wp_hold_at = time;
    }
    if(cvar("vr_worn_backpack_debug") >= 2)
    {
        VR_WP_Log(sprintf("held %.3f s: pull %.0f N (force %.0f, stretch %.1f units)", time - pk.vr_wp_hold_at, pull, f, s));
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
    VR_WP_Log(sprintf("pulled off at %.0f N (force %.0f, stretch %.1f units)", pull, f, s));
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
            if(VR_WP_Pull(pk, g, pl, pnum, cVR_MainHand) || VR_WP_Pull(pk, g, pl, pnum, cVR_OffHand))
            {
                return;
            }
            if(ragdollbone(g, [2, pnum, cVR_MainHand], QVR_WORNPACK) >= 0)
            {
                held = TRUE;
            }
            if(ragdollbone(g, [2, pnum, cVR_OffHand], QVR_WORNPACK) >= 0)
            {
                held = TRUE;
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

void(entity targ, entity inflictor, entity attacker, float take) VR_WP_Hit =
{
    entity pk = targ.vr_wp_pack;
    if(!pk || pk.vr_wp_state == VR_WP_OFF || inflictor == world || inflictor.classname == "vr_burn" ||
       vr_hitkind == QVR_HITKIND_SPLASH || take <= 0)
    {
        return;
    }
    VR_HitReact_Where(targ, inflictor, attacker);
    if(vr_hr_at == (targ.absmin + targ.absmax) * 0.5)
    {
        return; // (where it struck isn't known)
    }
    float d = ragdollbone(targ, vr_hr_at, QVR_WORNPACK_AT);
    if(d < 0 || d > VR_WP_HIT_REACH)
    {
        return;
    }
    float chance = min(1, max(0, cvar("vr_worn_backpack_hit_chance")) * take / max(1, cvar("vr_worn_backpack_hit_ref")));
    float roll = random();
    VR_WP_Log(sprintf("hit on its pack (%.1f units off): %.0f damage, chance %.2f, roll %.2f", d, take, chance, roll));
    if(roll >= chance)
    {
        return;
    }
    vector dir = normalize(vr_hr_at - (inflictor.origin == vr_hr_at ? attacker.origin : inflictor.origin));
    VR_WP_Rip(pk, "hit", targ.velocity + dir * 180 + '0 0 120');
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

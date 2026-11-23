// vr_dmloose.qc -- deathmatch's loose gibs and dropped weapons capped (MP_PLAN B13b).
//
// Sixteen players dying a few times a second each leave their gibs and heads (lying 30-45 s: VR_Gib_FadeAnew) and the
// weapons they held (10 s in deathmatch: wpnthrow_think) about the map: mp_soak counted ~450 gibs and ~340 weapon props
// awake in Box3D, which took ~60% of the server's frame, their touches most of the QuakeC's. In deathmatch, once a frame,
// the oldest of them go first while there are more than Most Loose Gibs (vr_dm_gibs_max) and Most Dropped Weapons
// (vr_dm_weapons_max); 0: no cap. Nothing in a hand, on a blade or being force-pulled goes. Single player and coop are
// as they were. The small gibs (vr_smallgibs_max) and the limbs (vr_limbs_max) have caps of their own.

float vr_dml_gibs_gone;    // gibs removed by the cap (the tests, vr_dm_test 7)
float vr_dml_weapons_gone; // weapons removed by the cap

// A loose gib or head the cap counts: not a player (a gibbed player's head is the player), nor a body in the queue, nor a
// small gib or a limb (their own caps), nor one held, skewered or being pulled.
float(entity g) VR_DMLoose_GibCounts =
{
    return !(g.flags & FL_CLIENT) && g.classname != "bodyque" && !g.vr_sgib && g.classname != "vr_limb" &&
           g.carry_player == world && g.vr_skewer_p == world && g.fg_state == 0;
};

// A weapon lying (or flying) loose the cap counts: not one being pulled, nor one deathmatch's 10 s already took.
float(entity w) VR_DMLoose_WeaponCounts =
{
    return w.carry_player == world && w.fg_state == 0 && time <= GetThrownWeaponTimeout(w);
};

// The loose gibs (kind 0) or weapons (1) the cap counts, chained by vr_dml_next in the edicts' order, in one pass over
// them (mpreview D5: each removal rescanned every edict) -> the first (world: none); vr_dml_count how many.
.entity vr_dml_next;
float vr_dml_count;
entity(float kind) VR_DMLoose_Chain =
{
    entity first = world, last = world;
    vr_dml_count = 0;
    entity e = kind ? find(world, classname, "thrown_weapon") : findflags(world, vr_gib, 1);
    while(e)
    {
        if(kind ? VR_DMLoose_WeaponCounts(e) : VR_DMLoose_GibCounts(e))
        {
            e.vr_dml_next = world;
            if(last) { last.vr_dml_next = e; } else { first = e; }
            last = e;
            vr_dml_count++;
        }
        e = kind ? find(e, classname, "thrown_weapon") : findflags(e, vr_gib, 1);
    }
    return first;
};

// How old the cap takes `e` for (the smaller the older): a gib's making time; a weapon's deathmatch removal time (each
// is made with it 10 s on).
float(entity e, float kind) VR_DMLoose_Age =
{
    return kind ? GetThrownWeaponTimeout(e) : e.vr_gib_made;
};

// The oldest of a kind go while there are more than `most` (of equal age, the first in the edicts' order).
void(float kind, float most) VR_DMLoose_Cap =
{
    entity first = VR_DMLoose_Chain(kind);
    while(first && vr_dml_count > most)
    {
        entity oldest = world, before = world, prev = world;
        for(entity e = first; e; e = e.vr_dml_next)
        {
            if(oldest == world || VR_DMLoose_Age(e, kind) < VR_DMLoose_Age(oldest, kind))
            {
                oldest = e;
                before = prev;
            }
            prev = e;
        }
        if(before) { before.vr_dml_next = oldest.vr_dml_next; } else { first = oldest.vr_dml_next; }
        vr_dml_count--;
        if(kind)
        {
            VR_Saw_PropStop(oldest); // (as wpnthrow_think's deathmatch removal)
            remove(oldest);
            vr_dml_weapons_gone++;
        }
        else
        {
            VR_Head_StopFlies(oldest);
            oldest.vr_gib = FALSE;
            remove(oldest);
            vr_dml_gibs_gone++;
        }
    }
};

// vr_dm_test 7: the caps as out of deathmatch, the game itself left in it. (B25b: the test set `deathmatch` 0 for 3 frames,
// and a player's respawn in them took the out-of-deathmatch way, "restart": dm4 started again, a suite run under load.)
float vr_dml_test_off;

// StartFrame: in deathmatch, the oldest over each cap go (at the frame's start: nothing is using them).
void() VR_DMLoose_Frame =
{
    if(!deathmatch || vr_dml_test_off)
    {
        return;
    }
    float most = floor(cvar_hget(cvarh_vr_dm_gibs_max));
    if(most > 0)
    {
        VR_DMLoose_Cap(0, most);
    }
    most = floor(cvar_hget(cvarh_vr_dm_weapons_max));
    if(most > 0)
    {
        VR_DMLoose_Cap(1, most);
    }
};

// ----------------------------------------------------------------------------
// Settled gore trimmed (qtune; the Quest's Standalone preset): out of deathmatch, twice a second, while more gibs, heads
// and limbs lie about than Most Settled Gore (vr_gore_trim_max), the farthest from every player of those lying there for
// vr_gore_trim_age seconds go first (after a fight: fresh ones are left alone), never one within vr_gore_trim_near units
// of a player (the look up close kept), nor one in a hand, on a blade or being pulled. Small gibs have their own cap
// (vr_smallgibs_max). 0: no cap (the default; Standalone 24).

.float vr_gtrim_dist; // its distance to the nearest player (VR_GoreTrim_Frame)
float vr_gtrim_next;  // the next pass's time
float vr_gtrim_gone;  // removed by it (the tests)

// One that counts: a gib, a head or a limb (not a small gib, a player's head or a body in the queue), free.
float(entity g) VR_GoreTrim_Counts =
{
    return !(g.flags & FL_CLIENT) && g.classname != "bodyque" && !g.vr_sgib && g.carry_player == world &&
           g.vr_skewer_p == world && g.fg_state == 0;
};

// Its distance to the nearest player.
float(entity g) VR_GoreTrim_Dist =
{
    float best = 999999;
    for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        best = min(best, vlen(g.origin - p.origin));
    }
    return best;
};

void() VR_GoreTrim_Frame =
{
    float most = floor(cvar_hget(cvarh_vr_gore_trim_max));
    if(deathmatch || most <= 0 || time < vr_gtrim_next)
    {
        return;
    }
    vr_gtrim_next = time + 0.5;
    float age = cvar_hget(cvarh_vr_gore_trim_age);
    float near = cvar_hget(cvarh_vr_gore_trim_near);
    // The ones that count, and the candidates (old enough, far enough) chained with their distances.
    entity first = world, last = world;
    float n = 0;
    for(entity e = findflags(world, vr_gib, 1); e; e = findflags(e, vr_gib, 1))
    {
        if(!VR_GoreTrim_Counts(e))
        {
            continue;
        }
        n++;
        if(time - e.vr_gib_made < age)
        {
            continue;
        }
        float d = VR_GoreTrim_Dist(e);
        if(d <= near)
        {
            continue;
        }
        e.vr_dml_next = world;
        e.vr_gtrim_dist = d;
        if(last) { last.vr_dml_next = e; } else { first = e; }
        last = e;
    }
    while(first && n > most)
    {
        entity far = world, before = world, prev = world;
        for(entity c = first; c; c = c.vr_dml_next)
        {
            if(far == world || c.vr_gtrim_dist > far.vr_gtrim_dist ||
               (c.vr_gtrim_dist == far.vr_gtrim_dist && c.vr_gib_made < far.vr_gib_made))
            {
                far = c;
                before = prev;
            }
            prev = c;
        }
        if(before) { before.vr_dml_next = far.vr_dml_next; } else { first = far.vr_dml_next; }
        n--;
        vr_gtrim_gone++;
        if(cvar_hget(cvarh_developer)) dprint(sprintf("goretrim: %s removed at %d units (the most is %d)\n", far.model, far.vr_gtrim_dist, most));
        if(far.classname == "vr_limb")
        {
            far.classname = "vr_limb_gone"; // (as VR_Limb_MakeRoom)
            far.nextthink2 = 0;
        }
        else
        {
            VR_Head_StopFlies(far);
        }
        far.vr_gib = FALSE;
        remove(far);
    }
};

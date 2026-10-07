// vr_smallgibs.qc -- small gibs (docs/vr-port/ROUND21.md, "Small gibs"): chunks of meat torn out of monsters (and,
// with vr_smallgibs_player, the player) by the hits they take. The gibs' own models (gib1, gib3) scaled down to between
// half a rock's size and a rock's (vr_smallgibs_size_min/max), light (vr_smallgibs_mass, kg: the engine's
// .vr_prop_mass), carried, thrown and bleeding as the gibs are (VR_MakeGibGrabbable), gone after vr_smallgibs_time as a
// crate's pieces are (not while held, nor until one let go of has landed again). At most vr_smallgibs_max lie about: a
// ring of them, the oldest (not one in a hand) removed first.
//
// When: a hit's damage (a shotgun blast's pellets summed: one hit) from vr_smallgibs_min_damage rises to a sure one at
// vr_smallgibs_full_damage, times its kind's multiplier (shots, nails, blades, blunt blows, props, explosions, the
// rest); more of them the harder (one more each vr_smallgibs_damage_per_gib). Always: under Quad Damage, the chainsaw's
// chain (a monster or a corpse; at most one each vr_smallgibs_saw_interval), a gibbing (vr_smallgibs_gibbing with the
// gibs), a large gib or head bursting (vr_smallgibs_burst). They fly out from where the hit landed, and up
// (vr_smallgibs_speed, vr_smallgibs_up, m/s); for vr_smallgibs_grace they pass through the body they came from (its
// .owner: Quake's moves skip it; a rigid body only after it). Each situation scales both (vr_smallgibs_speed_<it>,
// vr_smallgibs_up_<it>: melee, saw, guns, explosions, thrown, other, gibbing, corpse, burst; VR_SmallGib_Situation).
//
// Brain chunks (vr_smallgibs_brains): a head burst (a head gib destroyed: VR_Gib_Burst; a head pop:
// VR_Decap_PopEffects) throws torn lumps of brain too (gib_brain1..3.mdl, two skins: Misc/quakevr/make_brains.py), small gibs in all else
// (carried, thrown, bleeding, sticking, squishing, fading, counted in vr_smallgibs_max), a burst's flight, their own
// throw (vr_smallgibs_brains_speed, vr_smallgibs_brains_up: their own Speed and Up, not the meat gibs'), their own size
// (vr_smallgibs_brains_size times the small gibs') and mass (vr_smallgibs_brains_mass: lighter). How many:
// vr_smallgibs_brains_burst for a head gib, vr_smallgibs_brains_pop for a head pop.
//
// Per enemy (VR_SmallGib_Mult): each monster's counts, vr_smallgibs_mult_<monster> and
// vr_smallgibs_brains_mult_<monster> (1 as ever, 0 none, 2 twice as many), times a hit's, a gibbing's and a head or
// gib burst's (the same monsters as vr_corpse_health_<monster>: VR_Corpse_Parts, combat.qc). The player, a training
// dummy and a thrown prop are not one of them: 1.

.float vr_prop_mass;      // the engine's (vr_fields.inc): a prop's own mass, kg (Box3D's massSetting)
.float vr_sgib_born;      // its time runs from here (held, and in the air after: now)
.float vr_sgib_air;       // let go of: its time waits until it lands
.float vr_sgib_grace;     // until then it passes through its .owner (the body it came from)
.float vr_sgib_rigidlater; // landed in its grace: a rigid body after it
.float vr_sgib_sawtime;   // (a monster or corpse) when the chainsaw last tore one out of it
.float vr_sgib_sit;       // what tore it out (QVR_SGIB_SIT_*: the tests)
.float vr_sgib_brain;     // a brain chunk (VR_SmallGib_Brains)
.float vr_sgib_mult;      // (a head cut off: VR_Decap_ThrowHead) the enemy it came from's small gib count multiplier,
.float vr_sgib_brains_mult; // and its brain chunk multiplier: bursting the head later still counts them (VR_SmallGib_Mult)
.float vr_sgib_multset;   // they are stored on it (a head is not the monster: its .th_die is gone)
.vector vr_sgib_org;      // where it was made (the tests)
.float vr_sgib_peak;      // its fastest across in its first second, and when (s after it was made: the tests)
.float vr_sgib_peakt;
.float vr_sgib_tstep;     // vr_smallgibs_trace: its next sample, its last speed, its fastest and when, done
.float vr_sgib_tlast;
.float vr_sgib_tpeak;
.float vr_sgib_tpeakt;
.float vr_sgib_tdone;

// A shotgun blast's pellets (FireBulletsImpl: VR_SmallGib_BlastBegin .. BlastEnd), summed on each thing they hit.
.float vr_sgib_bmark;     // the blast it was last hit by (vr_sgib_blastserial)
.float vr_sgib_bdmg;      // its damage, summed
.vector vr_sgib_bpos;     // where its pellets struck, summed,
.float vr_sgib_bn;        // and how many
.entity vr_sgib_bnext;    // the blast's next target

float QVR_SGIB_SHOTS = 1;
float QVR_SGIB_NAILS = 2;
float QVR_SGIB_BLADES = 3;
float QVR_SGIB_BLUNT = 4;
float QVR_SGIB_PROPS = 5;
float QVR_SGIB_EXPLOSIONS = 6;
float QVR_SGIB_OTHER = 7;
float QVR_SGIB_SAW = 8;

// What tore it out: its flight's multipliers (vr_smallgibs_speed_<situation>, vr_smallgibs_up_<situation>).
float QVR_SGIB_SIT_MELEE = 1;      // a hand's blow (a blade, a punch, a pommel, a thing in the fist), a bash, a headbutt
float QVR_SGIB_SIT_SAW = 2;        // the chainsaw's chain or swing
float QVR_SGIB_SIT_GUNS = 3;       // shotguns, nails, lasers, lightning, a grunt's gun
float QVR_SGIB_SIT_EXPLOSIONS = 4; // rockets, grenades, every blast
float QVR_SGIB_SIT_THROWN = 5;     // a thrown weapon, rock, brick or gib; a projectile batted back
float QVR_SGIB_SIT_OTHER = 6;      // monsters' blows, the rest
float QVR_SGIB_SIT_GIBBED = 7;     // a living monster (or the player) gibbed
float QVR_SGIB_SIT_CORPSE = 8;     // a corpse gibbed
float QVR_SGIB_SIT_BURST = 9;      // a large gib or head bursting
float vr_sgib_st_n[10];            // launches by situation, their speeds out and up summed (the tests)
float vr_sgib_st_h[10];
float vr_sgib_st_v[10];

entity vr_sgib_ring[256]; // the small gibs, in the order they were made (a slot free: its id no longer matches)
float vr_sgib_ringid[256];
float vr_sgib_serial;     // the last one's serial
float vr_sgib_head;       // the ring's oldest entry
float vr_sgib_used;       // its entries from there, in the order made (some may be gone)
float vr_sgib_counted;    // the time the live ones were last counted (VR_SmallGib_MakeRoom)
float vr_sgib_made;       // how many were made (the tests)
float vr_sgib_brains_made; // of them brain chunks (the tests)
float vr_sgib_rolls;      // hits weighed, and their damage summed (the tests)
float vr_sgib_rolldmg;
float vr_sgib_rocksize;   // a rock's size (its longest side, units, at vr_world_scale 1): measured once

float vr_sgib_blast;       // inside a shotgun blast
float vr_sgib_blastserial;
entity vr_sgib_blastlist;
entity vr_sgib_blastinf;
entity vr_sgib_blastby;
entity vr_sgib_blastself; // who fires it (its pellets' inflictor: not a barrel it sets off)
float vr_sgib_flushing;    // rolling a blast's hits (VR_SmallGib_Kind: shots)
vector vr_sgib_at;         // a melee blow's contact (client.qc PlayerVRMeleeImpl; '0 0 0' none)

vector(float dm) VelocityForDamage;

void() VR_SmallGib_Precache =
{
    precache_sound("vr/squish1.wav"); // a gib landing or sticking (make_sounds.py)
    precache_sound("vr/squish2.wav");
    precache_sound("vr/squish3.wav");
    precache_sound("vr/squish4.wav");
    precache_sound("vr/squish_s1.wav"); // a small gib's
    precache_sound("vr/squish_s2.wav");
    precache_sound("vr/squish_s3.wav");
    precache_sound("vr/squish_s4.wav");
    precache_model("progs/gib_brain1.mdl"); // brain chunks (make_brains.py)
    precache_model("progs/gib_brain2.mdl");
    precache_model("progs/gib_brain3.mdl");
};

string(float xSmall) VR_Gib_SquishSound =
{
    float k = floor(random() * 4);
    if(xSmall)
    {
        return k < 1 ? "vr/squish_s1.wav" : (k < 2 ? "vr/squish_s2.wav" : (k < 3 ? "vr/squish_s3.wav" : "vr/squish_s4.wav"));
    }
    return k < 1 ? "vr/squish1.wav" : (k < 2 ? "vr/squish2.wav" : (k < 3 ? "vr/squish3.wav" : "vr/squish4.wav"));
};

// A gib's (or a small gib's) squish as it lands from Quake's bounce or sticks: a rigid one's knocks are the engine's
// (vr_physsound.cpp, flesh).
void(entity g, float xVolume) VR_Gib_Squish =
{
    if(g.vr_sgib)
    {
        VR_SoundVaried(g, CHAN_AUTO, VR_Gib_SquishSound(TRUE), xVolume * 0.6, ATTN_NORM);
        return;
    }
    VR_SoundVaried(g, CHAN_AUTO, random() < 0.25 ? "zombie/z_miss.wav" : VR_Gib_SquishSound(FALSE), xVolume, ATTN_NORM);
};

// A rock's longest side (the five rocks' mean), times vr_world_scale (the pieces' size).
float() VR_SmallGib_RockSize =
{
    if(vr_sgib_rocksize <= 0)
    {
        entity t = spawn();
        float sum = 0;
        for(float k = 1; k <= 5; k++)
        {
            setmodel(t, k == 1 ? "progs/vr_rock1.mdl" : (k == 2 ? "progs/vr_rock2.mdl" : (k == 3 ? "progs/vr_rock3.mdl" :
                     (k == 4 ? "progs/vr_rock4.mdl" : "progs/vr_rock5.mdl"))));
            vector d = modelbounds(t, 1) - modelbounds(t, 0);
            sum = sum + max(d_x, max(d_y, d_z));
        }
        remove(t);
        vr_sgib_rocksize = max(1, sum / 5);
        dprint(sprintf("smallgib: a rock's size %.1f units\n", vr_sgib_rocksize));
    }
    return vr_sgib_rocksize * max(0.1, cvar_hget(cvarh_vr_world_scale));
};

// ----------------------------------------------------------------------------
// The ring

// The ring: vr_sgib_used entries from slot vr_sgib_head (the oldest), in the order made; an entry whose small gib went
// (burst, its time up, removed) stays until the old end reaches it or the ring is compacted. A small gib made costs a
// step or two here: a walk of the whole ring at most once a frame (the count's truth) or once per 256 made (compacting).
// (Before, each one made walked all 256 slots three times, QC's arrays a call each: past Most Small Gibs about 100,000
// instructions a small gib, and a frame of many blows ran away. ROUND21.md, "The QC runaway of many blows".)

float(float xSlot) VR_SmallGib_SlotLive =
{
    float id = vr_sgib_ringid[xSlot];
    if(id <= 0)
    {
        return FALSE;
    }
    entity e = vr_sgib_ring[xSlot];
    return e != world && e.vr_sgib_id == id;
};

float() VR_SmallGib_Count =
{
    float n = 0;
    for(float k = 0; k < vr_sgib_used; k++)
    {
        if(VR_SmallGib_SlotLive((vr_sgib_head + k) & 255))
        {
            n++;
        }
    }
    return n;
};

// The entries gone at the old end, dropped.
void() VR_SmallGib_TrimOld =
{
    while(vr_sgib_used > 0 && !VR_SmallGib_SlotLive(vr_sgib_head))
    {
        vr_sgib_ringid[vr_sgib_head] = 0;
        vr_sgib_head = (vr_sgib_head + 1) & 255;
        vr_sgib_used--;
    }
};

// The ring full of entries, some gone: the live ones moved up to the old end, in order.
void() VR_SmallGib_Compact =
{
    float n = 0;
    for(float k = 0; k < vr_sgib_used; k++)
    {
        float s = (vr_sgib_head + k) & 255;
        if(VR_SmallGib_SlotLive(s))
        {
            float d = (vr_sgib_head + n) & 255;
            if(d != s)
            {
                vr_sgib_ring[d] = vr_sgib_ring[s];
                vr_sgib_ringid[d] = vr_sgib_ringid[s];
                vr_sgib_ringid[s] = 0;
            }
            n++;
        }
        else
        {
            vr_sgib_ringid[s] = 0;
        }
    }
    vr_sgib_used = n;
};

// Gone at once (its time is up, or the ring needs its place).
void(entity g) VR_SmallGib_Remove =
{
    VR_Carry_Forget(g);
    if(g.vr_sgib_id)
    {
        vr_sgib_live = max(0, vr_sgib_live - 1);
    }
    g.vr_sgib_id = 0;
    g.think2 = SUB_Null;
    g.nextthink2 = 0;
    remove(g);
};

// Room for one more under Most Small Gibs (vr_smallgibs_max): the oldest lying about go first (not one in a hand). The
// slot for it; -1 if there is none (as many in hands as the most).
float() VR_SmallGib_MakeRoom =
{
    float most = min(256, max(1, cvar_hget(cvarh_vr_smallgibs_max)));
    VR_SmallGib_TrimOld();
    float live = vr_sgib_live;
    if(live >= most && vr_sgib_counted != time)
    {
        live = VR_SmallGib_Count(); // (the truth, once a frame: some went other ways)
        vr_sgib_counted = time;
    }
    while(live >= most)
    {
        // The oldest not in a hand: the first such from the old end.
        float oldest = -1;
        for(float k = 0; k < vr_sgib_used && oldest < 0; k++)
        {
            float s = (vr_sgib_head + k) & 255;
            if(VR_SmallGib_SlotLive(s) && vr_sgib_ring[s].carry_player == world)
            {
                oldest = s;
            }
        }
        if(oldest < 0)
        {
            vr_sgib_live = live;
            return -1;
        }
        dprint(sprintf("smallgib: #%d removed (the most is %d)\n", vr_sgib_ringid[oldest], most));
        VR_SmallGib_Remove(vr_sgib_ring[oldest]);
        vr_sgib_ringid[oldest] = 0;
        live--;
        VR_SmallGib_TrimOld();
    }
    vr_sgib_live = live;
    if(vr_sgib_used >= 256)
    {
        VR_SmallGib_Compact();
        if(vr_sgib_used >= 256)
        {
            return -1;
        }
    }
    float slot = (vr_sgib_head + vr_sgib_used) & 255;
    vr_sgib_used++;
    vr_sgib_ringid[slot] = 0; // (taken: VR_SmallGib_Spawn fills it)
    return slot;
};

// ----------------------------------------------------------------------------
// A small gib

void() VR_SmallGib_Think;

// Quake's bounce (which passes through monsters) in its grace, a rigid body after it (VR_SmallGib_Rigid): landing in
// it, not yet (Box3D would push it out of the body it came from).
void() VR_SmallGib_Touch =
{
    if(other.vr_gib && self.throwhit != QVR_THROWHIT_NEVER_HIT)
    {
        return; // another gib in its box (VR_Gib_Touch: a pile's n x n touches a frame)
    }
    float wasRigid = self.vr_rigid;
    float speed = vlen(self.velocity);
    if(other != world && cvar_hget(cvarh_vr_smallgibs_trace))
    {
        bprint(sprintf("sgibtrace: #%g %.3f s: Quake touch with %s, %.0f u/s\n", self.vr_sgib_id, time - self.vr_sgib,
            other.classname, speed));
    }
    if(!wasRigid && other.solid == SOLID_BSP && speed > 120 && speed <= 220)
    {
        VR_Gib_Squish(self, 0.8); // (VR_Gib_Touch squishes the faster ones)
    }
    VR_Gib_Touch();
    if(!wasRigid && self.vr_rigid && self.vr_sgib_grace)
    {
        self.vr_rigid = 0;
        self.vr_sgib_rigidlater = TRUE;
    }
};

// Its grace over (or none): a rigid body from now, in flight too (Box3D: it meets the monsters' bodies, which Quake's
// bounce passes through), unless held or stuck to a wall.
void(entity g) VR_SmallGib_Rigid =
{
    g.vr_sgib_rigidlater = FALSE;
    if(g.vr_gib && !g.vr_rigid && g.carry_player == world && g.movetype == MOVETYPE_BOUNCE)
    {
        g.vr_rigid = 1; // (as VR_Gib_Touch would have made it)
        g.vr_spin = VRThrowSpinCapped(g.avelocity * 0.0087);
        g.avelocity = '0 0 0';
    }
};

// One at `org` going at `vel`, out of `from` (passing through it for vr_smallgibs_grace); `xBrain`: a brain chunk.
// world if there is no room.
entity(vector org, vector vel, entity from, float xBrain) VR_SmallGib_SpawnAs =
{
    float slot = VR_SmallGib_MakeRoom();
    if(slot < 0)
    {
        return world;
    }
    entity g = spawn();
    g.classname = "vr_smallgib";
    float smin = max(0.05, cvar_hget(cvarh_vr_smallgibs_size_min));
    float smax = max(smin, cvar_hget(cvarh_vr_smallgibs_size_max));
    float s = VR_SmallGib_RockSize() * (smin + random() * (smax - smin));
    if(xBrain)
    {
        float k = floor(random() * 3);
        setmodel(g, k < 1 ? "progs/gib_brain1.mdl" : (k < 2 ? "progs/gib_brain2.mdl" : "progs/gib_brain3.mdl"));
        g.skin = random() < 0.5 ? 0 : 1; // pinkish, greyish
        g.vr_sgib_brain = TRUE;
        s = s * max(0.05, cvar_hget(cvarh_vr_smallgibs_brains_size));
    }
    else
    {
        setmodel(g, random() < 0.5 ? "progs/gib1.mdl" : "progs/gib3.mdl"); // the smaller two
    }
    vector lo = modelbounds(g, 0);
    vector hi = modelbounds(g, 1);
    vector d = hi - lo;
    s = s / max(1, max(d_x, max(d_y, d_z)));
    g.model_scale = '1 1 1' * (s - 1);
    setorigin(g, org);
    g.velocity = vel;
    g.movetype = MOVETYPE_BOUNCE;
    g.solid = SOLID_NOT;
    g.angles = '1 0 0' * (random() * 360) + '0 1 0' * (random() * 360) + '0 0 1' * (random() * 360);
    g.avelocity = '1 0 0' * (crandom() * 600) + '0 1 0' * (crandom() * 600) + '0 0 1' * (crandom() * 600);
    g.ltime = time;
    g.flags = 0;
    g.vr_sgib = time;
    g.vr_prop_mass = max(0.05, cvar_hget(xBrain ? cvarh_vr_smallgibs_brains_mass : cvarh_vr_smallgibs_mass));
    VR_MakeGibGrabbable(g); // (vr_grab_gibs: else a bouncing thing that only fades)
    setsize(g, lo * s, hi * s);
    if(g.vr_gib)
    {
        g.touch = VR_SmallGib_Touch;
        if(cvar_hget(cvarh_vr_smallgibs_destroy))
        {
            VR_Gib_MakeDestroyable(g, 0.3);
        }
        VR_Gore_WatchFling(g); // it may stick to a ceiling or a wall (vr_gore_stick)
    }
    float grace = max(0, cvar_hget(cvarh_vr_smallgibs_grace));
    g.vr_sgib_grace = grace > 0 ? time + grace : 0;
    g.owner = grace > 0 ? from : world;
    if(!g.vr_sgib_grace)
    {
        VR_SmallGib_Rigid(g);
    }
    g.vr_sgib_born = time;
    g.think = VR_SmallGib_Think;
    g.nextthink = time + max(0.01, min(grace, 0.5));

    vr_sgib_serial++;
    vr_sgib_made++;
    vr_sgib_brains_made = vr_sgib_brains_made + (xBrain ? 1 : 0);
    vr_sgib_live++;
    g.vr_sgib_id = vr_sgib_serial;
    vr_sgib_ring[slot] = g;
    vr_sgib_ringid[slot] = vr_sgib_serial;
    return g;
};

// A small gib of meat (VR_SmallGib_SpawnAs).
entity(vector org, vector vel, entity from) VR_SmallGib_Spawn =
{
    return VR_SmallGib_SpawnAs(org, vel, from, FALSE);
};

// `g` (if made) was torn out by `xSit`: tagged, its launch counted (the tests).
void(entity g, float xSit) VR_SmallGib_Tag =
{
    if(g == world)
    {
        return;
    }
    g.vr_sgib_sit = xSit;
    g.vr_sgib_org = g.origin;
    if(cvar_hget(cvarh_vr_smallgibs_trace))
    {
        bprint(sprintf("sgibtrace: #%g made at %v (situation %g), %.0f u/s (%v), grace %.2f s, rigid %g\n", g.vr_sgib_id,
            g.origin, xSit, vlen(g.velocity), g.velocity, g.vr_sgib_grace ? g.vr_sgib_grace - time : 0, g.vr_rigid));
    }
    vector v = g.velocity;
    vr_sgib_st_n[xSit] = vr_sgib_st_n[xSit] + 1;
    vr_sgib_st_v[xSit] = vr_sgib_st_v[xSit] + v_z;
    v_z = 0;
    vr_sgib_st_h[xSit] = vr_sgib_st_h[xSit] + vlen(v);
};

// Its grace ends; held (and in the air after) its time waits; past it, it fades away over two seconds.
void() VR_SmallGib_Think =
{
    if(self.vr_sgib_grace && time >= self.vr_sgib_grace)
    {
        self.vr_sgib_grace = 0;
        self.owner = world;
        VR_SmallGib_Rigid(self);
    }
    if(self.carry_player != world)
    {
        self.vr_sgib_born = time;
        self.vr_sgib_air = TRUE;
    }
    else if(self.vr_sgib_air)
    {
        if(self.flags & FL_ONGROUND)
        {
            self.vr_sgib_air = FALSE;
        }
        else
        {
            self.vr_sgib_born = time;
        }
    }
    float life = cvar_hget(cvarh_vr_smallgibs_time);
    if(self.carry_player != world || life <= 0 || time < self.vr_sgib_born + life)
    {
        self.alpha = 0;
        self.nextthink = time + 0.25;
        return;
    }
    if(!self.alpha)
    {
        self.alpha = 1;
    }
    self.alpha = self.alpha - 0.05;
    if(self.alpha <= 0.05)
    {
        dprint(sprintf("smallgib: #%d faded away\n", self.vr_sgib_id));
        VR_SmallGib_Remove(self);
        return;
    }
    self.nextthink = time + 0.1;
};

// ----------------------------------------------------------------------------
// When they come

// What tears them out of `targ`: monsters (the training dummy too, unless vr_dummy_gore is off; not a statue), corpses, the player with
// vr_smallgibs_player.
float(entity targ) VR_SmallGib_Bleeds =
{
    if(targ.vr_gib || targ.model == string_null || VR_Dummy_Clean(targ))
    {
        return FALSE;
    }
    if((targ.th_die == knight_die || targ.th_die == hknight_die) && (targ.spawnflags & 2))
    {
        return FALSE; // a statue (statgib)
    }
    if(targ.flags & FL_CLIENT)
    {
        return cvar_hget(cvarh_vr_smallgibs_player) != 0;
    }
    return (targ.flags & FL_MONSTER) || targ.vr_corpse == 2;
};

// `e`'s count multiplier: its monster's (vr_smallgibs_mult_<monster>, or vr_smallgibs_brains_mult_<monster> with
// `xBrains`), the same monsters as VR_Corpse_Parts' corpse health (QC combat.qc). 1 for anything it doesn't know: the
// player, a training dummy, a gib, a rock. A head cut off (VR_Decap_ThrowHead) is no longer the monster, so it carries
// its enemy's pair on itself (vr_sgib_multset): bursting that head later counts them as its enemy's.
float(entity e, float xBrains) VR_SmallGib_Mult =
{
    if(e == world)
    {
        return 1;
    }
    if(e.vr_sgib_multset)
    {
        return max(0, xBrains ? e.vr_sgib_brains_mult : e.vr_sgib_mult);
    }

    float h = 0;
    float known = FALSE;
    if(e.th_die == army_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_grunt : cvarh_vr_smallgibs_mult_grunt; known = TRUE; }
    else if(e.th_die == enf_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_enforcer : cvarh_vr_smallgibs_mult_enforcer; known = TRUE; }
    else if(e.th_die == dog_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_dog : cvarh_vr_smallgibs_mult_dog; known = TRUE; }
    else if(e.th_die == demon_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_fiend : cvarh_vr_smallgibs_mult_fiend; known = TRUE; }
    else if(e.th_die == ogre_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_ogre : cvarh_vr_smallgibs_mult_ogre; known = TRUE; }
    else if(e.th_die == knight_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_knight : cvarh_vr_smallgibs_mult_knight; known = TRUE; }
    else if(e.th_die == hknight_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_hellknight : cvarh_vr_smallgibs_mult_hellknight; known = TRUE; }
    else if(e.th_die == shalrath_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_vore : cvarh_vr_smallgibs_mult_vore; known = TRUE; }
    else if(e.th_die == sham_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_shambler : cvarh_vr_smallgibs_mult_shambler; known = TRUE; }
    else if(e.th_die == wiz_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_scrag : cvarh_vr_smallgibs_mult_scrag; known = TRUE; }
    else if(e.th_die == fish_die || e.th_die == f_death1) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_fish : cvarh_vr_smallgibs_mult_fish; known = TRUE; }
    else if(e.th_die == gremlin_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_gremlin : cvarh_vr_smallgibs_mult_gremlin; known = TRUE; }
    else if(e.th_die == scourge_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_scourge : cvarh_vr_smallgibs_mult_scourge; known = TRUE; }
    else if(e.th_die == eel_death) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_eel : cvarh_vr_smallgibs_mult_eel; known = TRUE; }
    else if(e.th_die == VR_Decap_ZombieDie) // a zombie or a mummy beheaded (vr_decap.qc): as VR_Corpse_Parts reads them
    {
        h = e.classname == "monster_mummy" ? (xBrains ? cvarh_vr_smallgibs_brains_mult_mummy : cvarh_vr_smallgibs_mult_mummy) :
            (xBrains ? cvarh_vr_smallgibs_brains_mult_zombie : cvarh_vr_smallgibs_mult_zombie);
        known = TRUE;
    }
    else if(e.th_die == zombie_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_zombie : cvarh_vr_smallgibs_mult_zombie; known = TRUE; }
    else if(e.th_die == mummy_die) { h = xBrains ? cvarh_vr_smallgibs_brains_mult_mummy : cvarh_vr_smallgibs_mult_mummy; known = TRUE; }

    if(!known)
    {
        return 1;
    }
    return max(0, cvar_hget(h));
};

// `n` of them, times a monster's count multiplier (`mult`): 0 none, 1 as now, 2 twice as many (nearest whole, never
// less than one where some came).
float(float n, float mult) VR_SmallGib_Times =
{
    if(n <= 0 || mult <= 0)
    {
        return 0;
    }
    if(mult == 1)
    {
        return n;
    }
    return max(1, floor(n * mult + 0.5));
};

// A gib or head thrown out of `xFrom` (ThrowGib, VR_Decap_ThrowHead: it is no longer the monster) carries its enemy's
// counts on itself: bursting or destroying it later still counts them as that monster's (VR_SmallGib_Mult). The head a
// gibbing throws is the monster itself, and keeps its .th_die, so it needs none of this.
void(entity xFrom, entity g) VR_SmallGib_CarryMults =
{
    if(xFrom == world || xFrom == g || g.vr_sgib_multset)
    {
        return;
    }
    g.vr_sgib_mult = VR_SmallGib_Mult(xFrom, FALSE);
    g.vr_sgib_brains_mult = VR_SmallGib_Mult(xFrom, TRUE);
    g.vr_sgib_multset = TRUE;
};

// The kind of the hit (vr_hitkind and the inflictor, as VR_GibKind; a hand's blow by the weapon and its part).
float(entity inflictor, entity attacker) VR_SmallGib_Kind =
{
    float k = vr_hitkind;
    if(vr_sgib_flushing)
    {
        return QVR_SGIB_SHOTS; // a shotgun blast's (a player's or a grunt's)
    }
    if(k == QVR_HITKIND_SPLASH)
    {
        return QVR_SGIB_EXPLOSIONS;
    }
    if(k == QVR_HITKIND_MELEE && vr_hithand >= 0 && (attacker.flags & FL_CLIENT))
    {
        float w = VRGetEntWeapon(attacker, vr_hithand);
        if(w == WID_CHAINSAW)
        {
            return QVR_SGIB_SAW;
        }
        if(vr_melee_blunt)
        {
            return QVR_SGIB_BLUNT; // a pommel, a hilt, a butt (client.qc: the blow's part)
        }
        if(w == WID_AXE || w == WID_SWORD)
        {
            return QVR_SGIB_BLADES;
        }
        if(w == WID_FIST && VR_Carry_Held(attacker, vr_hithand) != world)
        {
            return QVR_SGIB_PROPS; // a rock, a brick, a gib in the fist
        }
        return QVR_SGIB_BLUNT; // a punch, a gun or the crowbar swung, Mjolnir
    }
    if(k == QVR_HITKIND_MELEE || k == QVR_HITKIND_HEADBUTT || k == QVR_HITKIND_BASH || k == QVR_HITKIND_SHOVE)
    {
        return QVR_SGIB_BLUNT;
    }
    if(k == QVR_HITKIND_SHOT)
    {
        if(vr_hitweapon != "" ||
           (vr_hithand >= 0 && (attacker.flags & FL_CLIENT) && VRGetEntWeapon(attacker, vr_hithand) == WID_LIGHTNING))
        {
            return QVR_SGIB_OTHER; // lightning
        }
        return QVR_SGIB_SHOTS;
    }
    if(inflictor == world || inflictor == attacker)
    {
        return QVR_SGIB_OTHER; // a monster's own blow, a kick, a stuck axe's bleeding
    }
    string c = inflictor.classname;
    if(c == "missile" || c == "grenade" || c == "MultiRocket" || c == "MiniGrenade" || c == "MultiGrenade" ||
       c == "proximity_grenade" || c == "plasma")
    {
        return QVR_SGIB_EXPLOSIONS;
    }
    if(c == "spike" || c == "lava_spike" || c == "laser" || c == "hiplaser" || c == "knightspike" || c == "wizspike")
    {
        return QVR_SGIB_NAILS;
    }
    if(c == "thrown_weapon")
    {
        return (inflictor.weapon == WID_AXE || inflictor.weapon == WID_SWORD || inflictor.weapon == WID_CHAINSAW) ?
            QVR_SGIB_BLADES : QVR_SGIB_PROPS;
    }
    if(inflictor.vr_gib || (inflictor.carry_use && !inflictor.vr_grenade) || inflictor.vr_deflect_time > 0)
    {
        return QVR_SGIB_PROPS; // thrown or flung things, a projectile batted back
    }
    return QVR_SGIB_OTHER;
};

string(float xKind) VR_SmallGib_KindName =
{
    if(xKind == QVR_SGIB_SHOTS) { return "shots"; }
    if(xKind == QVR_SGIB_NAILS) { return "nails"; }
    if(xKind == QVR_SGIB_BLADES) { return "blades"; }
    if(xKind == QVR_SGIB_BLUNT) { return "blunt"; }
    if(xKind == QVR_SGIB_PROPS) { return "props"; }
    if(xKind == QVR_SGIB_EXPLOSIONS) { return "explosions"; }
    if(xKind == QVR_SGIB_SAW) { return "chainsaw"; }
    return "other";
};

float(float xKind) VR_SmallGib_KindMult =
{
    float h = cvarh_vr_smallgibs_other;
    if(xKind == QVR_SGIB_SHOTS) { h = cvarh_vr_smallgibs_shots; }
    else if(xKind == QVR_SGIB_NAILS) { h = cvarh_vr_smallgibs_nails; }
    else if(xKind == QVR_SGIB_BLADES) { h = cvarh_vr_smallgibs_blades; }
    else if(xKind == QVR_SGIB_BLUNT) { h = cvarh_vr_smallgibs_blunt; }
    else if(xKind == QVR_SGIB_PROPS) { h = cvarh_vr_smallgibs_props; }
    else if(xKind == QVR_SGIB_EXPLOSIONS) { h = cvarh_vr_smallgibs_explosions; }
    return max(0, cvar_hget(h));
};

// ----------------------------------------------------------------------------
// The damage corpses and ragdolls take, by its kind (VR_Corpse_Damage, combat.qc; Gibs and Corpses > Corpse Damage and
// Health): the small gibs' kinds (VR_SmallGib_Kind), with lightning, fists, fire and bashes told apart too.

// The kind of the hit on a corpse (QVR_CDMG_*, combat.qc).
float(entity inflictor, entity attacker) VR_Corpse_DamageKind =
{
    float k = vr_hitkind;
    if(k == QVR_HITKIND_HEADBUTT || k == QVR_HITKIND_BASH || k == QVR_HITKIND_SHOVE)
    {
        return QVR_CDMG_BASH;
    }
    if(k == QVR_HITKIND_BLEED)
    {
        return QVR_CDMG_BLADES; // a thrown axe stuck in it
    }
    if(k == QVR_HITKIND_SPLASH && inflictor == attacker && (attacker.flags & FL_CLIENT))
    {
        return QVR_CDMG_LIGHTNING; // the lightning gun's discharge under water
    }
    if(inflictor.classname == "vr_burn")
    {
        return QVR_CDMG_FIRE; // (vr_burning.qc)
    }
    float s = VR_SmallGib_Kind(inflictor, attacker);
    if(s == QVR_SGIB_SHOTS) { return QVR_CDMG_SHOTS; }
    if(s == QVR_SGIB_NAILS) { return QVR_CDMG_NAILS; }
    if(s == QVR_SGIB_EXPLOSIONS) { return QVR_CDMG_EXPLOSIONS; }
    if(s == QVR_SGIB_BLADES) { return QVR_CDMG_BLADES; }
    if(s == QVR_SGIB_SAW) { return QVR_CDMG_CHAINSAW; }
    if(s == QVR_SGIB_PROPS) { return QVR_CDMG_PROPS; }
    if(s == QVR_SGIB_BLUNT)
    {
        // An empty fist (a punch), not a gun, the crowbar or Mjolnir swung, nor a pommel.
        if(k == QVR_HITKIND_MELEE && vr_hithand >= 0 && (attacker.flags & FL_CLIENT) && !vr_melee_blunt &&
           VRGetEntWeapon(attacker, vr_hithand) == WID_FIST && VR_Carry_Held(attacker, vr_hithand) == world)
        {
            return QVR_CDMG_FISTS;
        }
        return QVR_CDMG_BLUNT;
    }
    if(k == QVR_HITKIND_SHOT)
    {
        return QVR_CDMG_LIGHTNING; // (VR_SmallGib_Kind: a hitscan's "other" is lightning: the gun's, Mjolnir's)
    }
    return QVR_CDMG_OTHER;
};

string(float xKind) VR_Corpse_DamageKindName =
{
    if(xKind == QVR_CDMG_SHOTS) { return "shots"; }
    if(xKind == QVR_CDMG_NAILS) { return "nails"; }
    if(xKind == QVR_CDMG_EXPLOSIONS) { return "explosions"; }
    if(xKind == QVR_CDMG_LIGHTNING) { return "lightning"; }
    if(xKind == QVR_CDMG_BLUNT) { return "blunt"; }
    if(xKind == QVR_CDMG_FISTS) { return "fists"; }
    if(xKind == QVR_CDMG_BLADES) { return "blades"; }
    if(xKind == QVR_CDMG_CHAINSAW) { return "chainsaw"; }
    if(xKind == QVR_CDMG_PROPS) { return "props"; }
    if(xKind == QVR_CDMG_FIRE) { return "fire"; }
    if(xKind == QVR_CDMG_BASH) { return "bash"; }
    return "other";
};

// Times the damage of a hit of kind `xKind` on a corpse (vr_corpse_dmg_<kind>).
float(float xKind) VR_Corpse_DamageKindMult =
{
    float h = cvarh_vr_corpse_dmg_other;
    if(xKind == QVR_CDMG_SHOTS) { h = cvarh_vr_corpse_dmg_shots; }
    else if(xKind == QVR_CDMG_NAILS) { h = cvarh_vr_corpse_dmg_nails; }
    else if(xKind == QVR_CDMG_EXPLOSIONS) { h = cvarh_vr_corpse_dmg_explosions; }
    else if(xKind == QVR_CDMG_LIGHTNING) { h = cvarh_vr_corpse_dmg_lightning; }
    else if(xKind == QVR_CDMG_BLUNT) { h = cvarh_vr_corpse_dmg_blunt; }
    else if(xKind == QVR_CDMG_FISTS) { h = cvarh_vr_corpse_dmg_fists; }
    else if(xKind == QVR_CDMG_BLADES) { h = cvarh_vr_corpse_dmg_blades; }
    else if(xKind == QVR_CDMG_CHAINSAW) { h = cvarh_vr_corpse_dmg_chainsaw; }
    else if(xKind == QVR_CDMG_PROPS) { h = cvarh_vr_corpse_dmg_props; }
    else if(xKind == QVR_CDMG_FIRE) { h = cvarh_vr_corpse_dmg_fire; }
    else if(xKind == QVR_CDMG_BASH) { h = cvarh_vr_corpse_dmg_bash; }
    return max(0, cvar_hget(h));
};

// The chance a hit of `xDamage` tears any out: none under the threshold, rising to a sure one at Full Damage (to the
// power of vr_smallgibs_curve: light hits rarer).
// The situation of a hit of kind `xKind` (vr_hitkind's: a hand's blow, a shot; else the kind's).
float(float xKind) VR_SmallGib_Situation =
{
    float k = vr_hitkind;
    if(xKind == QVR_SGIB_SAW)
    {
        return QVR_SGIB_SIT_SAW;
    }
    if(k == QVR_HITKIND_MELEE || k == QVR_HITKIND_HEADBUTT || k == QVR_HITKIND_BASH || k == QVR_HITKIND_SHOVE)
    {
        return QVR_SGIB_SIT_MELEE;
    }
    if(xKind == QVR_SGIB_SHOTS || xKind == QVR_SGIB_NAILS || k == QVR_HITKIND_SHOT)
    {
        return QVR_SGIB_SIT_GUNS;
    }
    if(xKind == QVR_SGIB_EXPLOSIONS)
    {
        return QVR_SGIB_SIT_EXPLOSIONS;
    }
    if(xKind == QVR_SGIB_BLADES || xKind == QVR_SGIB_PROPS)
    {
        return QVR_SGIB_SIT_THROWN;
    }
    return QVR_SGIB_SIT_OTHER;
};

string(float xSit) VR_SmallGib_SitName =
{
    if(xSit == QVR_SGIB_SIT_MELEE) { return "melee"; }
    if(xSit == QVR_SGIB_SIT_SAW) { return "chainsaw"; }
    if(xSit == QVR_SGIB_SIT_GUNS) { return "guns"; }
    if(xSit == QVR_SGIB_SIT_EXPLOSIONS) { return "explosions"; }
    if(xSit == QVR_SGIB_SIT_THROWN) { return "thrown"; }
    if(xSit == QVR_SGIB_SIT_GIBBED) { return "gibbing"; }
    if(xSit == QVR_SGIB_SIT_CORPSE) { return "corpse gibbing"; }
    if(xSit == QVR_SGIB_SIT_BURST) { return "burst"; }
    return "other";
};

// The situation's multiplier of Speed (`xUp` FALSE) or Up (TRUE).
float(float xSit, float xUp) VR_SmallGib_SitMult =
{
    float h;
    if(xSit == QVR_SGIB_SIT_MELEE) { h = xUp ? cvarh_vr_smallgibs_up_melee : cvarh_vr_smallgibs_speed_melee; }
    else if(xSit == QVR_SGIB_SIT_SAW) { h = xUp ? cvarh_vr_smallgibs_up_saw : cvarh_vr_smallgibs_speed_saw; }
    else if(xSit == QVR_SGIB_SIT_GUNS) { h = xUp ? cvarh_vr_smallgibs_up_guns : cvarh_vr_smallgibs_speed_guns; }
    else if(xSit == QVR_SGIB_SIT_EXPLOSIONS) { h = xUp ? cvarh_vr_smallgibs_up_explosions : cvarh_vr_smallgibs_speed_explosions; }
    else if(xSit == QVR_SGIB_SIT_THROWN) { h = xUp ? cvarh_vr_smallgibs_up_thrown : cvarh_vr_smallgibs_speed_thrown; }
    else if(xSit == QVR_SGIB_SIT_GIBBED) { h = xUp ? cvarh_vr_smallgibs_up_gibbing : cvarh_vr_smallgibs_speed_gibbing; }
    else if(xSit == QVR_SGIB_SIT_CORPSE) { h = xUp ? cvarh_vr_smallgibs_up_corpse : cvarh_vr_smallgibs_speed_corpse; }
    else if(xSit == QVR_SGIB_SIT_BURST) { h = xUp ? cvarh_vr_smallgibs_up_burst : cvarh_vr_smallgibs_speed_burst; }
    else { h = xUp ? cvarh_vr_smallgibs_up_other : cvarh_vr_smallgibs_speed_other; }
    return max(0, cvar_hget(h));
};

float(float xDamage) VR_SmallGib_Chance =
{
    float lo = max(0, cvar_hget(cvarh_vr_smallgibs_min_damage));
    float hi = max(lo + 1, cvar_hget(cvarh_vr_smallgibs_full_damage));
    if(xDamage < lo)
    {
        return 0;
    }
    return pow(min(1, (xDamage - lo) / (hi - lo)), max(0.1, cvar_hget(cvarh_vr_smallgibs_curve)));
};

// Where on `targ` the hit landed: `xAt` if it is on it (within its box grown by 16), else the point of its box nearest
// what dealt it (an explosion's middle, a missile), else its middle.
vector(entity targ, entity inflictor, vector xAt) VR_SmallGib_Where =
{
    vector lo = targ.absmin - '16 16 16';
    vector hi = targ.absmax + '16 16 16';
    if(xAt != '0 0 0' && xAt_x >= lo_x && xAt_x <= hi_x && xAt_y >= lo_y && xAt_y <= hi_y && xAt_z >= lo_z && xAt_z <= hi_z)
    {
        return xAt;
    }
    vector p = (targ.absmin + targ.absmax) * 0.5;
    if(inflictor != world && inflictor != targ)
    {
        p = inflictor.origin;
        if(inflictor.absmax != inflictor.absmin)
        {
            p = (inflictor.absmin + inflictor.absmax) * 0.5;
        }
    }
    p_x = min(targ.absmax_x, max(targ.absmin_x, p_x));
    p_y = min(targ.absmax_y, max(targ.absmin_y, p_y));
    p_z = min(targ.absmax_z - 2, max(targ.absmin_z + 4, p_z));
    return p;
};

// Their flight out of `targ` from `xAt`: away from its middle across (or back along the blow), a little at random,
// at about Speed, and up at about Up (each from two thirds to four thirds of it), times the situation's multipliers.
vector(entity targ, vector xAt, vector xBlow, float xSit) VR_SmallGib_Fling =
{
    vector mid = (targ.absmin + targ.absmax) * 0.5;
    vector out = xAt - mid;
    out_z = 0;
    if(vlen(out) < 1)
    {
        out = -xBlow;
        out_z = 0;
    }
    if(vlen(out) < 1)
    {
        out = '1 0 0' * crandom() + '0 1 0' * crandom();
    }
    if((xSit == QVR_SGIB_SIT_MELEE || xSit == QVR_SGIB_SIT_SAW) && vlen(xBlow) > 0.001)
    {
        out = normalize(xBlow); // the actual swing, including upward and downward blows
    }
    out = normalize(out) + ('1 0 0' * crandom() + '0 1 0' * crandom()) * 0.2;
    out = normalize(out);
    float m2u = VRMetersToUnits();
    return out * (max(0, cvar_hget(cvarh_vr_smallgibs_speed)) * VR_SmallGib_SitMult(xSit, FALSE) * m2u * (0.67 + random() * 0.67)) +
           '0 0 1' * (max(0, cvar_hget(cvarh_vr_smallgibs_up)) * VR_SmallGib_SitMult(xSit, TRUE) * m2u * (0.67 + random() * 0.67));
};

// `n` out of `targ` at `xAt` (pulled back out of any wall between it and its middle).
void(entity targ, vector xAt, vector xBlow, float n, float xSit) VR_SmallGib_Tear =
{
    vector mid = (targ.absmin + targ.absmax) * 0.5;
    traceline(mid, xAt, TRUE, targ);
    vector at = trace_endpos;
    if(trace_fraction < 1)
    {
        at = at - normalize(xAt - mid) * 2;
    }
    for(float i = 0; i < n; i++)
    {
        VR_SmallGib_Tag(VR_SmallGib_Spawn(at + '1 0 0' * crandom() + '0 1 0' * crandom() + '0 0 1' * crandom(),
                                          VR_SmallGib_Fling(targ, at, xBlow, xSit), targ), xSit);
    }
};

// A hit of `xDamage` on `targ` landing at `xAt`: whether any are torn out, and how many.
void(entity targ, entity inflictor, entity attacker, float xDamage, vector xAt) VR_SmallGib_Roll =
{
    vr_sgib_rolls++;
    vr_sgib_rolldmg = vr_sgib_rolldmg + xDamage;
    float kind = VR_SmallGib_Kind(inflictor, attacker);
    float quad = attacker.super_damage_finished > time;
    float n = 0;
    float chance = 1;
    if(kind == QVR_SGIB_SAW)
    {
        if(time < targ.vr_sgib_sawtime + max(0, cvar_hget(cvarh_vr_smallgibs_saw_interval)))
        {
            return;
        }
        targ.vr_sgib_sawtime = time;
        n = 1;
    }
    else
    {
        if(!quad)
        {
            chance = VR_SmallGib_Chance(xDamage) * VR_SmallGib_KindMult(kind);
        }
        if(random() < chance)
        {
            n = floor(xDamage / max(1, cvar_hget(cvarh_vr_smallgibs_damage_per_gib)) + random());
            n = min(max(1, cvar_hget(cvarh_vr_smallgibs_per_hit)), max(1, n));
        }
    }
    float sit = VR_SmallGib_Situation(kind);
    float mult = VR_SmallGib_Mult(targ, FALSE); // its own vr_smallgibs_mult_<monster> (1: as every other monster)
    n = VR_SmallGib_Times(n, mult);
    dprint(sprintf("smallgib: %s hit for %.1f (%s%s, %s): chance %.2f, %d\n", targ.classname, xDamage,
        VR_SmallGib_KindName(kind), quad ? ", quad" : "", VR_SmallGib_SitName(sit), chance, n));
    if(mult != 1)
    {
        dprint(sprintf("smallgib: %s's small gibs x%.2f\n", targ.classname, mult));
    }
    if(n > 0)
    {
        VR_SmallGib_Tear(targ, VR_SmallGib_Where(targ, inflictor, xAt), vr_hitdir, n, sit);
    }
};

// From T_DamageDeal (a monster's or the player's damage, armour not taken off) and VR_Corpse_Damage (a corpse's).
void(entity targ, entity inflictor, entity attacker, float xDamage) VR_SmallGib_Hit =
{
    if(!cvar_hget(cvarh_vr_smallgibs) || xDamage <= 0 || !VR_SmallGib_Bleeds(targ))
    {
        return;
    }
    if(vr_sgib_blast && inflictor == vr_sgib_blastself)
    {
        // A pellet's: summed with the blast's others (VR_SmallGib_BlastEnd).
        if(targ.vr_sgib_bmark != vr_sgib_blastserial)
        {
            targ.vr_sgib_bmark = vr_sgib_blastserial;
            targ.vr_sgib_bdmg = 0;
            targ.vr_sgib_bpos = '0 0 0';
            targ.vr_sgib_bn = 0;
            targ.vr_sgib_bnext = vr_sgib_blastlist;
            vr_sgib_blastlist = targ;
        }
        targ.vr_sgib_bdmg = targ.vr_sgib_bdmg + xDamage;
        vr_sgib_blastinf = inflictor;
        vr_sgib_blastby = attacker;
        return;
    }
    vector at = vr_hitpos;
    if(vr_hitkind == QVR_HITKIND_MELEE && vr_sgib_at != '0 0 0')
    {
        at = vr_sgib_at;
    }
    VR_SmallGib_Roll(targ, inflictor, attacker, xDamage, at);
};

// A shotgun blast (FireBulletsImpl): its pellets' damage on each target summed into one hit.
void() VR_SmallGib_BlastBegin =
{
    vr_sgib_blast = TRUE;
    vr_sgib_blastself = self;
    vr_sgib_blastserial++;
    vr_sgib_blastlist = world;
};

// A pellet striking `e` at `xAt` (before its damage): where the blast's hit on it lands is its pellets' middle.
void(entity e, vector xAt) VR_SmallGib_Pellet =
{
    if(!vr_sgib_blast || !e.takedamage || e == world)
    {
        return;
    }
    if(e.vr_sgib_bmark != vr_sgib_blastserial)
    {
        e.vr_sgib_bmark = vr_sgib_blastserial;
        e.vr_sgib_bdmg = 0;
        e.vr_sgib_bpos = '0 0 0';
        e.vr_sgib_bn = 0;
        e.vr_sgib_bnext = vr_sgib_blastlist;
        vr_sgib_blastlist = e;
    }
    e.vr_sgib_bpos = e.vr_sgib_bpos + xAt;
    e.vr_sgib_bn = e.vr_sgib_bn + 1;
};

void() VR_SmallGib_BlastEnd =
{
    vr_sgib_blast = FALSE;
    vr_sgib_flushing = TRUE;
    entity e = vr_sgib_blastlist;
    vr_sgib_blastlist = world;
    while(e != world)
    {
        entity next = e.vr_sgib_bnext;
        e.vr_sgib_bnext = world;
        // (Gibbed by it: its gibbing tore them out. Each pellet's damage was summed as dealt, after Quad.)
        if(e.vr_sgib_bdmg > 0 && VR_SmallGib_Bleeds(e))
        {
            vector at = e.vr_sgib_bn > 0 ? e.vr_sgib_bpos * (1 / e.vr_sgib_bn) : '0 0 0';
            VR_SmallGib_Roll(e, vr_sgib_blastinf, vr_sgib_blastby, e.vr_sgib_bdmg, at);
        }
        e.vr_sgib_bdmg = 0;
        e = next;
    }
    vr_sgib_flushing = FALSE;
};

// From ThrowHead (self is the monster, corpse or player gibbed, `dm` its health: VelocityForDamage) and a headless
// corpse's gibbing: Gibbing's many from all over its body, flung out from its middle at Speed and up at Up (as a hit's)
// and with its gibs' flight at 0.6 (the blow's push), across times the situation's Speed and up times its Up
// (Gibbing a Live One, or Gibbing a Corpse: vr_sgib_corpsegib).
void(float dm) VR_SmallGib_Gibbed =
{
    if(!cvar_hget(cvarh_vr_smallgibs) || ((self.flags & FL_CLIENT) && !cvar_hget(cvarh_vr_smallgibs_player)) ||
       ((self.th_die == knight_die || self.th_die == hknight_die) && (self.spawnflags & 2)))
    {
        return;
    }
    float n = floor(max(0, cvar_hget(cvarh_vr_smallgibs_gibbing)) + 0.5);
    float mult = VR_SmallGib_Mult(self, FALSE); // its own vr_smallgibs_mult_<monster>
    n = VR_SmallGib_Times(n, mult);
    float sit = vr_sgib_corpsegib ? QVR_SGIB_SIT_CORPSE : QVR_SGIB_SIT_GIBBED;
    dprint(sprintf("smallgib: %s gibbed: %d (%s)\n", self.classname, n, VR_SmallGib_SitName(sit)));
    float m2u = VRMetersToUnits();
    float sm = VR_SmallGib_SitMult(sit, FALSE);
    float um = VR_SmallGib_SitMult(sit, TRUE);
    float speed = max(0, cvar_hget(cvarh_vr_smallgibs_speed)) * m2u;
    float up = max(0, cvar_hget(cvarh_vr_smallgibs_up)) * m2u;
    vector mid = (self.absmin + self.absmax) * 0.5;
    vector lo = self.absmin;
    vector size = self.absmax - self.absmin;
    for(float i = 0; i < n; i++)
    {
        vector at = lo + '1 0 0' * (size_x * (0.2 + random() * 0.6)) + '0 1 0' * (size_y * (0.2 + random() * 0.6)) +
                    '0 0 1' * (size_z * (0.3 + random() * 0.6));
        vector out = at - mid;
        out_z = 0;
        out = normalize(out) + ('1 0 0' * crandom() + '0 1 0' * crandom()) * 0.6;
        out_z = 0;
        out = normalize(out + '0.01 0 0');
        vector v = VR_Gore_Fling(VelocityForDamage(dm)) * 0.6 + out * speed * (0.67 + random() * 0.67) +
                   '0 0 1' * up * (0.67 + random() * 0.67);
        v_x = v_x * sm;
        v_y = v_y * sm;
        v_z = v_z * um;
        VR_SmallGib_Tag(VR_SmallGib_Spawn(at, v, self), sit);
    }
};

// From VR_Gib_Burst: a large gib (`xSize` 1) or head (1.5) bursting at `org` breaks into small ones. `xFrom` is the
// gib itself (its monster's vr_smallgibs_mult_<monster> counts them: a head that was cut off carries them: vr_sgib_multset).
void(vector org, vector xDir, float xSize, entity xFrom) VR_SmallGib_Burst =
{
    if(!cvar_hget(cvarh_vr_smallgibs))
    {
        return;
    }
    float n = floor(max(0, cvar_hget(cvarh_vr_smallgibs_burst)) * max(1, xSize) + 0.5);
    dprint(sprintf("smallgib: a gib burst: %d\n", n));
    VR_SmallGib_BurstOf(org, xDir, n, FALSE, xFrom);
};

// `n` flung out of a burst at `org` (a burst's flight: across every way and along `xDir`, and up); `xBrain`: brain
// chunks, which fly at their own Speed and Up (vr_smallgibs_brains_speed, vr_smallgibs_brains_up). `xFrom` the entity
// they came from (world for none): its vr_smallgibs_mult_<monster> / vr_smallgibs_brains_mult_<monster> times the count.
void(vector org, vector xDir, float n, float xBrain, entity xFrom) VR_SmallGib_BurstOf =
{
    float mult = VR_SmallGib_Mult(xFrom, xBrain);
    n = VR_SmallGib_Times(n, mult);
    if(mult != 1)
    {
        dprint(sprintf("smallgib: %s's %s x%.2f: %d\n", xFrom.classname, xBrain ? "brain chunks" : "small gibs", mult, n));
    }
    float m2u = VRMetersToUnits();
    float speed = max(0, cvar_hget(xBrain ? cvarh_vr_smallgibs_brains_speed : cvarh_vr_smallgibs_speed)) *
                  VR_SmallGib_SitMult(QVR_SGIB_SIT_BURST, FALSE) * m2u;
    float up = max(0, cvar_hget(xBrain ? cvarh_vr_smallgibs_brains_up : cvarh_vr_smallgibs_up)) *
               VR_SmallGib_SitMult(QVR_SGIB_SIT_BURST, TRUE) * m2u;
    vector along = xDir != '0 0 0' ? normalize(xDir) : '0 0 0';
    for(float i = 0; i < n; i++)
    {
        vector out = '1 0 0' * crandom() + '0 1 0' * crandom();
        out = normalize(out + '0.01 0 0') * speed * (0.67 + random() * 0.67) + along * speed * 0.5;
        VR_SmallGib_Tag(VR_SmallGib_SpawnAs(org + '1 0 0' * crandom() * 2 + '0 1 0' * crandom() * 2 + '0 0 1' * random() * 2,
                                            out + '0 0 1' * up * (0.67 + random() * 0.67), world, xBrain), QVR_SGIB_SIT_BURST);
    }
};

// A head burst at `org` (`xPop`: a head pop, VR_Decap_PopEffects, `xFrom` the monster it lost its head from; else a
// head gib destroyed, VR_Gib_Burst, `xFrom` the head): brain chunks (vr_smallgibs_brains_pop or _burst of them, times
// its vr_smallgibs_brains_mult_<monster>).
void(vector org, vector xDir, float xPop, entity xFrom) VR_SmallGib_Brains =
{
    if(!cvar_hget(cvarh_vr_smallgibs) || !cvar_hget(cvarh_vr_smallgibs_brains))
    {
        return;
    }
    float n = floor(max(0, cvar_hget(xPop ? cvarh_vr_smallgibs_brains_pop : cvarh_vr_smallgibs_brains_burst)) + 0.5);
    dprint(sprintf("smallgib: brains: %d (%s)\n", n, xPop ? "a head pop" : "a head gib burst"));
    VR_SmallGib_BurstOf(org, xDir, n, TRUE, xFrom);
};

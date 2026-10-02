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
// .owner: Quake's moves skip it; a rigid body only after it).

.float vr_prop_mass;      // the engine's (vr_fields.inc): a prop's own mass, kg (Box3D's massSetting)
.float vr_sgib_born;      // its time runs from here (held, and in the air after: now)
.float vr_sgib_air;       // let go of: its time waits until it lands
.float vr_sgib_grace;     // until then it passes through its .owner (the body it came from)
.float vr_sgib_rigidlater; // landed in its grace: a rigid body after it
.float vr_sgib_sawtime;   // (a monster or corpse) when the chainsaw last tore one out of it

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

entity vr_sgib_ring[256]; // the small gibs, in the order they were made (a slot free: its id no longer matches)
float vr_sgib_ringid[256];
float vr_sgib_serial;     // the last one's serial
float vr_sgib_next;       // the ring's next slot to try
float vr_sgib_made;       // how many were made (the tests)
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
        sound(g, CHAN_AUTO, VR_Gib_SquishSound(TRUE), xVolume * 0.6, ATTN_NORM);
        return;
    }
    sound(g, CHAN_AUTO, random() < 0.25 ? "zombie/z_miss.wav" : VR_Gib_SquishSound(FALSE), xVolume, ATTN_NORM);
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

float(float xSlot) VR_SmallGib_SlotLive =
{
    entity e = vr_sgib_ring[xSlot];
    return vr_sgib_ringid[xSlot] > 0 && e != world && e.vr_sgib_id == vr_sgib_ringid[xSlot];
};

float() VR_SmallGib_Count =
{
    float n = 0;
    for(float i = 0; i < 256; i++)
    {
        if(VR_SmallGib_SlotLive(i))
        {
            n++;
        }
    }
    return n;
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
    float live = vr_sgib_live;
    if(live >= most)
    {
        live = VR_SmallGib_Count(); // (the truth: some went other ways)
    }
    float i;
    while(live >= most)
    {
        float oldest = -1;
        for(i = 0; i < 256; i++)
        {
            if(VR_SmallGib_SlotLive(i) && vr_sgib_ring[i].carry_player == world &&
               (oldest < 0 || vr_sgib_ringid[i] < vr_sgib_ringid[oldest]))
            {
                oldest = i;
            }
        }
        if(oldest < 0)
        {
            return -1;
        }
        dprint(sprintf("smallgib: #%d removed (the most is %d)\n", vr_sgib_ringid[oldest], most));
        VR_SmallGib_Remove(vr_sgib_ring[oldest]);
        vr_sgib_ringid[oldest] = 0;
        live--;
    }
    vr_sgib_live = live;
    for(i = 0; i < 256; i++)
    {
        float slot = vr_sgib_next;
        vr_sgib_next = (vr_sgib_next + 1) & 255;
        if(!VR_SmallGib_SlotLive(slot))
        {
            return slot;
        }
    }
    return -1;
};

// ----------------------------------------------------------------------------
// A small gib

void() VR_SmallGib_Think;

// Quake's bounce (which passes through monsters) in its grace, a rigid body after it (VR_SmallGib_Rigid): landing in
// it, not yet (Box3D would push it out of the body it came from).
void() VR_SmallGib_Touch =
{
    float wasRigid = self.vr_rigid;
    float speed = vlen(self.velocity);
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

// One at `org` going at `vel`, out of `from` (passing through it for vr_smallgibs_grace). world if there is no room.
entity(vector org, vector vel, entity from) VR_SmallGib_Spawn =
{
    float slot = VR_SmallGib_MakeRoom();
    if(slot < 0)
    {
        return world;
    }
    entity g = spawn();
    g.classname = "vr_smallgib";
    setmodel(g, random() < 0.5 ? "progs/gib1.mdl" : "progs/gib3.mdl"); // the smaller two
    vector lo = modelbounds(g, 0);
    vector hi = modelbounds(g, 1);
    vector d = hi - lo;
    float smin = max(0.05, cvar_hget(cvarh_vr_smallgibs_size_min));
    float smax = max(smin, cvar_hget(cvarh_vr_smallgibs_size_max));
    float s = VR_SmallGib_RockSize() * (smin + random() * (smax - smin)) / max(1, max(d_x, max(d_y, d_z)));
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
    g.vr_prop_mass = max(0.05, cvar_hget(cvarh_vr_smallgibs_mass));
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
    vr_sgib_live++;
    g.vr_sgib_id = vr_sgib_serial;
    vr_sgib_ring[slot] = g;
    vr_sgib_ringid[slot] = vr_sgib_serial;
    return g;
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

// What tears them out of `targ`: monsters (not the training dummy, not a statue), corpses, the player with
// vr_smallgibs_player.
float(entity targ) VR_SmallGib_Bleeds =
{
    if(targ.vr_gib || targ.model == string_null || targ.classname == "vr_dummy")
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

// The chance a hit of `xDamage` tears any out: none under the threshold, rising to a sure one at Full Damage (to the
// power of vr_smallgibs_curve: light hits rarer).
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
// at about Speed, and up at about Up (each from two thirds to four thirds of it).
vector(entity targ, vector xAt, vector xBlow) VR_SmallGib_Fling =
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
    out = normalize(out) + ('1 0 0' * crandom() + '0 1 0' * crandom()) * 0.45;
    out_z = 0;
    out = normalize(out);
    float m2u = VRMetersToUnits();
    return out * (max(0, cvar_hget(cvarh_vr_smallgibs_speed)) * m2u * (0.67 + random() * 0.67)) +
           '0 0 1' * (max(0, cvar_hget(cvarh_vr_smallgibs_up)) * m2u * (0.67 + random() * 0.67));
};

// `n` out of `targ` at `xAt` (pulled back out of any wall between it and its middle).
void(entity targ, vector xAt, vector xBlow, float n) VR_SmallGib_Tear =
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
        VR_SmallGib_Spawn(at + '1 0 0' * crandom() + '0 1 0' * crandom() + '0 0 1' * crandom(),
                          VR_SmallGib_Fling(targ, at, xBlow), targ);
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
    dprint(sprintf("smallgib: %s hit for %.1f (%s%s): chance %.2f, %d\n", targ.classname, xDamage,
        VR_SmallGib_KindName(kind), quad ? ", quad" : "", chance, n));
    if(n > 0)
    {
        VR_SmallGib_Tear(targ, VR_SmallGib_Where(targ, inflictor, xAt), vr_hitdir, n);
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
// corpse's gibbing: Gibbing's many from all over its body, flung as its gibs are, slower.
void(float dm) VR_SmallGib_Gibbed =
{
    if(!cvar_hget(cvarh_vr_smallgibs) || ((self.flags & FL_CLIENT) && !cvar_hget(cvarh_vr_smallgibs_player)) ||
       ((self.th_die == knight_die || self.th_die == hknight_die) && (self.spawnflags & 2)))
    {
        return;
    }
    float n = floor(max(0, cvar_hget(cvarh_vr_smallgibs_gibbing)) + 0.5);
    dprint(sprintf("smallgib: %s gibbed: %d\n", self.classname, n));
    vector lo = self.absmin;
    vector size = self.absmax - self.absmin;
    for(float i = 0; i < n; i++)
    {
        vector at = lo + '1 0 0' * (size_x * (0.2 + random() * 0.6)) + '0 1 0' * (size_y * (0.2 + random() * 0.6)) +
                    '0 0 1' * (size_z * (0.3 + random() * 0.6));
        VR_SmallGib_Spawn(at, VR_Gore_Fling(VelocityForDamage(dm)) * 0.6, self);
    }
};

// From VR_Gib_Burst: a large gib (`xSize` 1) or head (1.5) bursting at `org` breaks into small ones.
void(vector org, vector xDir, float xSize) VR_SmallGib_Burst =
{
    if(!cvar_hget(cvarh_vr_smallgibs))
    {
        return;
    }
    float n = floor(max(0, cvar_hget(cvarh_vr_smallgibs_burst)) * max(1, xSize) + 0.5);
    dprint(sprintf("smallgib: a gib burst: %d\n", n));
    float m2u = VRMetersToUnits();
    float speed = max(0, cvar_hget(cvarh_vr_smallgibs_speed)) * m2u;
    float up = max(0, cvar_hget(cvarh_vr_smallgibs_up)) * m2u;
    vector along = xDir != '0 0 0' ? normalize(xDir) : '0 0 0';
    for(float i = 0; i < n; i++)
    {
        vector out = '1 0 0' * crandom() + '0 1 0' * crandom();
        out = normalize(out + '0.01 0 0') * speed * (0.67 + random() * 0.67) + along * speed * 0.5;
        VR_SmallGib_Spawn(org + '1 0 0' * crandom() * 2 + '0 1 0' * crandom() * 2 + '0 0 1' * random() * 2,
                          out + '0 0 1' * up * (0.67 + random() * 0.67), world);
    }
};

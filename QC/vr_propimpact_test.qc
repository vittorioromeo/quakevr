#ifdef QVR_TESTS
// vr_propimpact_test.qc -- the props' impact rule's test (vr_prop_impact_test 1; Debug > Gore Tests, "Prop Impacts: Tap,
// Hit, Smash, Throw"): vr_carry.qc VR_Prop_Impact. Each kind made in turn ahead of you, alone (a small crate, an explosive
// box, a crate's piece; Scourge of Armagon's wall with rubble and AD's vase and metal breakable as stand-ins: their class
// and fields): struck by a blow of the off hand at 0.4x, 0.75x and 1.2x its break speed (VR_Prop_BreakSpeed), and at 0.75x
// with an axe's heft; a piece by the hand's touch; the carried ones held and smashed into the nearest wall at 2x (and a
// crate at 0.75x); thrown into it (their Box3D hit, .vr_impact). One "propimpact:" line each: health before and after,
// broken or not. Use god mode: an explosive box blows up.

float vr_propimp_dead; // a stand-in's death (its th_die)

void() VR_PropImp_StandInDie =
{
    vr_propimp_dead = TRUE;
    self.takedamage = DAMAGE_NO;
};

float(entity e) VR_PropImp_Broken =
{
    return vr_propimp_dead || e.takedamage == DAMAGE_NO || e.health <= 0 || e.classname == "explo_box" ||
           e.classname == "vr_crate_broken";
};

// `xWhat` (0 a small crate, 1 an explosive box, 2 a crate's piece, 3 a wall with rubble's stand-in) at `xAt`.
entity(float xWhat, vector xAt) VR_PropImp_Spawn =
{
    vr_propimp_dead = FALSE;
    entity e;
    if(xWhat == 0)
    {
        e = VR_Crate_TestSpawn(xAt, 1, 0, FALSE);
        e.flags = e.flags - (e.flags & FL_ONGROUND);
        return e;
    }
    entity os = self;
    if(xWhat == 1)
    {
        self = spawn();
        setorigin(self, xAt - '16 16 0'); // (a box's origin is its corner)
        misc_explobox();
        e = self;
        self = os;
        return e;
    }
    e = spawn();
    if(xWhat == 2)
    {
        setmodel(e, "progs/vr_plank1.mdl");
        setorigin(e, xAt + '0 0 16');
        VR_CratePiece_Setup(e);
        e.takedamage = DAMAGE_YES; // (armed now, not in a moment)
        return e;
    }
    e.classname = "breakablewallwithrubble";
    e.solid = SOLID_BBOX;
    setsize(e, '-16 -16 0', '16 16 32');
    setorigin(e, xAt);
    e.health = 10;
    e.takedamage = DAMAGE_YES;
    e.th_die = VR_PropImp_StandInDie;
    return e;
};

string(float xWhat) VR_PropImp_Name =
{
    return xWhat == 0 ? "small crate" : (xWhat == 1 ? "explosive box" : (xWhat == 2 ? "crate piece" : "rubble wall"));
};

// Done with `e`: a broken one goes by itself; else it goes now.
void(entity e) VR_PropImp_Done =
{
    if(!VR_PropImp_Broken(e))
    {
        VR_Carry_Forget(e);
        remove(e);
    }
};

// A blow of the off hand on `e` at `xMult` of its break speed, its damage a punch's times `xHeft` squared (the heft).
void(entity e, string xName, float xMult, float xHeft) VR_PropImp_Blow =
{
    entity b = VR_Blow(cVR_OffHand);
    float s0 = b.mh_speed;
    float st0 = b.mh_strength;
    float c0 = b.mh_counter;
    float x0 = b.mh_exhaust;
    float bs = VR_Prop_BreakSpeed(e, VR_Prop_Kind(e));
    float h0 = e.health;
    b.mh_speed = bs * xMult;
    b.mh_strength = 1;
    b.mh_counter = 0;
    b.mh_exhaust = 0;
    float dmg = cvar_hget(cvarh_vr_dmg_fist) * cvar_hget(cvarh_vr_melee_dmg_multiplier) * VRMeleeDmgQuadMult() * xHeft * xHeft;
    vector mid = (e.absmin + e.absmax) * 0.5;
    VR_SetHitKind(QVR_HITKIND_MELEE, cVR_OffHand, mid, normalize(mid - self.origin));
    vr_melee_blowhand = cVR_OffHand + 1; // (as PlayerVRMeleeImpl's blow)
    T_Damage_VRMelee(e, self, self, dmg);
    vr_melee_blowhand = 0;
    VR_ClearHitKind();
    b.mh_speed = s0;
    b.mh_strength = st0;
    b.mh_counter = c0;
    b.mh_exhaust = x0;
    bprint(sprintf("propimpact: %s: a blow at %.2fx (%.2f m/s, heft %.2f): health %.1f -> %.1f, %s\n", xName, xMult,
        bs * xMult, xHeft, h0, e.health, VR_PropImp_Broken(e) ? "broken" : (e.health < h0 ? "damaged" : "unhurt")));
};

// A piece touched by the off hand at `xMult` of its break speed (VR_CratePiece_Struck).
void(entity e, float xMult) VR_PropImp_Touch =
{
    vector oldVel = self.offhandvel;
    float bs = VR_Prop_BreakSpeed(e, VR_PROPK_PIECE);
    float h0 = e.health;
    self.offhandvel = '0 0 1' * (bs * xMult);
    VR_CratePiece_Struck(self, cVR_OffHand, e);
    self.offhandvel = oldVel;
    bprint(sprintf("propimpact: crate piece: the hand's touch at %.2fx (%.2f m/s): health %.1f -> %.1f, %s\n", xMult,
        bs * xMult, h0, e.health, VR_PropImp_Broken(e) ? "broken" : (e.health < h0 ? "damaged" : "unhurt (pushed)")));
};

vector vr_propimp_wall;    // the nearest wall's point, at the eyes' height
vector vr_propimp_walldir; // the way to it

float() VR_PropImp_FindWall =
{
    float bestFrac = 2;
    vector eye = self.origin + '0 0 16';
    for(float yaw = 0; yaw < 360; yaw = yaw + 90)
    {
        makevectors('0 1 0' * yaw);
        traceline(eye, eye + v_forward * 400, TRUE, self);
        if(trace_fraction < bestFrac && !trace_startsolid)
        {
            bestFrac = trace_fraction;
            vr_propimp_walldir = v_forward;
        }
    }
    vr_propimp_wall = eye + vr_propimp_walldir * (400 * bestFrac);
    return bestFrac < 1;
};

// `e` held in the off hand, its middle a little off the wall, driven into it at `xMult` of its break speed (the hand's
// motion and its own: VR_Carry_Follow).
void(entity e, string xName, float xMult) VR_PropImp_Smash =
{
    float m2u = VRMetersToUnits();
    float bs = VR_Prop_BreakSpeed(e, VR_Prop_Kind(e));
    float h0 = e.health;
    vector dir = vr_propimp_walldir;
    VR_Carry_Start(self, cVR_OffHand, e);
    float held = e.carry_player == self;
    vector mid = (e.absmin + e.absmax) * 0.5 - e.origin;
    vector half = (e.maxs - e.mins) * 0.5;
    float clear = max(1, min(half_x, min(half_y, half_z)));
    setorigin(e, vr_propimp_wall - dir * (clear + 4) - mid);
    float spd = bs * xMult;
    e.velocity = dir * (spd * m2u);
    vector oldVel = self.offhandvel;
    self.offhandvel = dir * spd;
    VR_Carry_Follow(e, e.origin + dir * 10);
    self.offhandvel = oldVel;
    float broken = VR_PropImp_Broken(e);
    bprint(sprintf("propimpact: %s: held %d, smashed into the wall at %.2fx (%.2f m/s): health %.1f -> %.1f, %s\n", xName,
        held, xMult, spd, h0, broken ? 0 : e.health, broken ? "broken" : (e.health < h0 ? "damaged" : "unhurt")));
    if(!broken && e.carry_player == self)
    {
        VR_Carry_Forget(e);
    }
};

// `e`'s Box3D hit (.vr_impact) at `xSpeed` m/s on the level; `xThrown`: just thrown by you.
void(entity e, string xName, float xSpeed, float xThrown) VR_PropImp_Thrown =
{
    float h0 = e.health;
    if(xThrown)
    {
        e.carry_thrown = time;
        e.carry_letgo = self;
    }
    entity os = self;
    self = e;
    other = world;
    e.vr_impact(xSpeed);
    self = os;
    float broken = VR_PropImp_Broken(e);
    bprint(sprintf("propimpact: %s: %s, hit the wall at %.1f m/s: health %.1f -> %.1f, %s\n", xName,
        xThrown ? "thrown" : "dropped", xSpeed, h0, broken ? 0 : e.health, broken ? "broken" : (e.health < h0 ? "damaged" : "unhurt")));
};

// AD's breakables by their fields (no models here): kind, break speed, and the rule's damage at 0.4x, 0.75x, 1.2x.
void(vector xAt, float xStyle, float xHealth, string xName) VR_PropImp_ADStandIn =
{
    entity e = spawn();
    setorigin(e, xAt);
    e.ad_classgroup = ad_CG_BREAKABLE;
    e.ad_classtype = ad_CT_FUNCBREAKMDL;
    e.style = xStyle;
    e.health = xHealth;
    e.takedamage = DAMAGE_YES;
    float kind = VR_Prop_Kind(e);
    bprint(sprintf("propimpact: %s (AD, style %g, health %g): kind %s, break speed %.2f m/s, damage at 0.4x %.1f, ", xName,
        xStyle, xHealth, VR_Prop_KindName(kind), VR_Prop_BreakSpeed(e, kind), VR_Prop_Wear(e, kind, 0.4, "test")));
    bprint(sprintf("0.75x %.1f, 1.2x %.1f\n", VR_Prop_Wear(e, kind, 0.75, "test"), VR_Prop_Wear(e, kind, 1.2, "test")));
    remove(e);
};

// The throws' speeds, of each kind's own threshold (VR_PropImp_Run).
float(float r) VR_PropImp_ThrowMult =
{
    if(r == 0)
    {
        return 0.6;
    }
    if(r == 1)
    {
        return 0.85;
    }
    if(r == 3)
    {
        return 0.9;
    }
    if(r == 5)
    {
        return 0.4;
    }
    if(r == 6)
    {
        return 0.75;
    }
    return r == 8 ? 1.5 : 1.1;
};

void() VR_PropImp_Run =
{
    makevectors('0 1 0' * self.v_viewangle_y);
    vector at = self.origin + v_forward * 96 + '0 0 -24';
    for(float k = 0; k < 4; k++)
    {
        string name = VR_PropImp_Name(k);
        entity e = VR_PropImp_Spawn(k, at);
        bprint(sprintf("propimpact: %s: kind %s, health %g, break speed %.2f m/s\n", name, VR_Prop_KindName(VR_Prop_Kind(e)),
            e.health, VR_Prop_BreakSpeed(e, VR_Prop_Kind(e))));
        VR_PropImp_Done(e);
        for(float j = 0; j < 4; j++)
        {
            e = VR_PropImp_Spawn(k, at);
            VR_PropImp_Blow(e, name, j == 0 ? 0.4 : (j == 2 ? 1.2 : 0.75), j == 3 ? sqrt(2) : 1);
            VR_PropImp_Done(e);
        }
    }
    for(float t = 0; t < 3; t++)
    {
        entity p = VR_PropImp_Spawn(2, at);
        VR_PropImp_Touch(p, t == 0 ? 0.5 : (t == 1 ? 0.9 : 1.2));
        VR_PropImp_Done(p);
    }

    if(!VR_PropImp_FindWall() || !VRIsHandEmpty(self, cVR_OffHand))
    {
        bprint("propimpact: no wall within 400 units, or the off hand is full\n");
    }
    else
    {
        for(float s = 0; s < 4; s++)
        {
            float what = s < 2 ? 0 : (s == 2 ? 1 : 2);
            entity h = VR_PropImp_Spawn(what, vr_propimp_wall - vr_propimp_walldir * 48);
            VR_PropImp_Smash(h, VR_PropImp_Name(what), s == 1 ? 0.75 : 2);
            VR_PropImp_Done(h);
        }
    }

    // Thrown (or dropped) into the wall: a crate at 0.6x, 0.85x, 1.1x Breaks On Impact; an explosive box at 0.9x, 1.1x
    // Blow Up On Impact; a piece thrown at 0.4x, 0.75x, 1.1x Thrown Pieces Burst, and dropped at 1.5x.
    float ci = cvar("vr_crate_impact");
    float ei = cvar_hget(cvarh_vr_explobox_impact);
    float pi = cvar_hget(cvarh_vr_crate_piece_impact);
    for(float r = 0; r < 9; r++)
    {
        float kindT = r < 3 ? 0 : (r < 5 ? 1 : 2);
        float base = kindT == 0 ? ci : (kindT == 1 ? ei : pi);
        entity q = VR_PropImp_Spawn(kindT, at);
        VR_PropImp_Thrown(q, VR_PropImp_Name(kindT), base * VR_PropImp_ThrowMult(r), r != 8);
        VR_PropImp_Done(q);
    }

    VR_PropImp_ADStandIn(at, ad_BTYPE_CERAMIC, 1, "vase");
    VR_PropImp_ADStandIn(at, ad_BTYPE_GLASS, 20, "window");
    VR_PropImp_ADStandIn(at, ad_BTYPE_WOOD, 75, "wooden");
    VR_PropImp_ADStandIn(at, ad_BTYPE_METAL, 200, "metal");
    bprint("propimpact: done\n");
};

// vr_prop_impact_test 2 (Debug > Gore Tests, "Light Ammo Thrown at a Crate"; ROUND22.md, "Prop pushback, heavy throws,
// light ammo"): a nailgun magazine, a super nailgun's, a cell, a shell and, to compare, a small crate, each thrown three
// times at vr_propimp_lt_speed m/s into a fresh small crate ahead of you, one a second, as a hand's throw lets go of it
// (VR_Carry_Loose, VR_Carry_Throw: its Box3D hit and its throw's damage). One "lightthrow:" line a throw: the crate's
// health after it. The magazines, the cell and the shell hardly scratch it; the crate thrown into it breaks it as before.
float vr_propimp_lt_next;   // the next step's time (0: no test)
float vr_propimp_lt_step;   // throws made (3 a kind)
entity vr_propimp_lt_crate; // the crate thrown at
entity vr_propimp_lt_thing; // the last thing thrown
float vr_propimp_lt_h0;     // the crate's health before the throw
float vr_propimp_lt_mass;   // the thrown thing's mass (kg)
float vr_propimp_lt_dmg;    // and its throw's damage on what it hits (VR_Thrown_Damage)
float vr_propimp_lt_speed = 14;

string(float k) VR_PropImp_LightName =
{
    return k == 0 ? "nail magazine" : (k == 1 ? "super nail magazine" : (k == 2 ? "cell" : (k == 3 ? "shell" : "small crate")));
};

// Thing `k` (VR_PropImp_LightName) made at `xAt`.
entity(float k, vector xAt) VR_PropImp_LightMake =
{
    if(k == 4)
    {
        entity c = VR_Crate_TestSpawn(xAt, 1, 0, FALSE);
        c.flags = c.flags - (c.flags & FL_ONGROUND);
        return c;
    }
    float wid = k == 0 ? WID_NAILGUN : (k == 1 ? WID_SUPER_NAILGUN : (k == 2 ? WID_LIGHTNING : 0));
    float aid = k == 2 ? AID_CELLS : (k == 3 ? AID_SHELLS : AID_NAILS);
    return VR_Reload_MakeRound(wid, aid, k == 3 ? 1 : 20, xAt, '0 0 0');
};

// self: the player.
void() VR_PropImp_LightStep =
{
    float k = floor(vr_propimp_lt_step / 3);
    makevectors('0 1 0' * self.v_viewangle_y);
    vector at = self.origin + v_forward * 112 + '0 0 -24';
    if(vr_propimp_lt_thing != world)
    {
        float gone = vr_propimp_lt_crate == world || VR_PropImp_Broken(vr_propimp_lt_crate);
        float h = gone ? 0 : vr_propimp_lt_crate.health;
        // (Two sprintfs: a varargs builtin takes at most 8 parameters.)
        bprint(strcat(sprintf("lightthrow: %s (%.2f kg) throw %g at %g m/s: ",
            VR_PropImp_LightName(floor((vr_propimp_lt_step - 1) / 3)), vr_propimp_lt_mass,
            mod(vr_propimp_lt_step - 1, 3) + 1, vr_propimp_lt_speed),
            sprintf("its throw damage %.1f; crate health %.1f -> %.1f%s\n", vr_propimp_lt_dmg, vr_propimp_lt_h0, h,
            gone ? ", broken" : "")));
        if(vr_propimp_lt_step > 12)
        {
            VR_PropImp_Done(vr_propimp_lt_thing); // (a crate: unless it broke too)
        }
        else if(vr_propimp_lt_thing.modelindex)
        {
            VR_Carry_Forget(vr_propimp_lt_thing);
            remove(vr_propimp_lt_thing);
        }
    }
    vr_propimp_lt_thing = world;
    if(vr_propimp_lt_step >= 15)
    {
        bprint("lightthrow: done\n");
        vr_propimp_lt_next = 0;
        return;
    }
    if(mod(vr_propimp_lt_step, 3) == 0 || vr_propimp_lt_crate == world ||
       VR_PropImp_Broken(vr_propimp_lt_crate))
    {
        if(vr_propimp_lt_crate != world && !VR_PropImp_Broken(vr_propimp_lt_crate))
        {
            VR_PropImp_Done(vr_propimp_lt_crate);
        }
        vr_propimp_lt_crate = VR_Crate_TestSpawn(at, 1, 0, FALSE);
    }
    vector mid = (vr_propimp_lt_crate.absmin + vr_propimp_lt_crate.absmax) * 0.5;
    vector from = mid - v_forward * 56;
    entity t = VR_PropImp_LightMake(k, from);
    vr_propimp_lt_h0 = vr_propimp_lt_crate.health;
    t.carry_player = self; // (let go of by this player's hand: VR_Carry_Loose)
    VR_Carry_Loose(t);
    t.vr_spin = '0 0 0';
    VR_Carry_Throw(t, v_forward * vr_propimp_lt_speed * VRMetersToUnits());
    vr_propimp_lt_mass = propvalue(t, "mass");
    vr_propimp_lt_dmg = VR_Thrown_Damage(t, vr_propimp_lt_speed * VRMetersToUnits()); // (what it deals what it hits)
    vr_propimp_lt_thing = t;
    vr_propimp_lt_step++;
    vr_propimp_lt_next = time + 1;
};

// vr_prop_impact_test 3 (Debug > Gore Tests, "Blasts Push Props: AD and Quake"): a small crate 48 units from a blast of
// 60 by Arcane Dimensions' radius damage (its Plasma Gun's, rockets'...: ad_T_RadiusDamage), then a fresh one by Quake's
// (T_RadiusDamage); a quarter of a second later, "blastpush:" its speed. Both throw it alike (physicsblast). Then an AD
// plasma bolt's blast (its splash 20, thrown as vr_ad_plasma_push: VR_ADGun_BlastPush) and Quake's of 20 (as the plasma
// threw before).
float vr_propimp_bl_next; // the next step's time (0: no test)
float vr_propimp_bl_step;
entity vr_propimp_bl_crate;
vector vr_propimp_bl_org; // where it stood
entity vr_propimp_bl_pending; // made, its blast next

void() VR_PropImp_BlastStep =
{
    makevectors('0 1 0' * self.v_viewangle_y);
    vector at = self.origin + v_forward * 128 + '0 0 -24';
    if(vr_propimp_bl_crate != world)
    {
        float gone = VR_PropImp_Broken(vr_propimp_bl_crate);
        bprint(sprintf("blastpush: %s blast: the crate %s, its speed %.0f u/s, moved %.1f units\n",
            vr_propimp_bl_step == 1 ? "AD's" : (vr_propimp_bl_step == 3 ? "AD's plasma (20)" :
            (vr_propimp_bl_step == 4 ? "Quake's 20 (plasma's before)" : "Quake's")),
            gone ? "broken" : "whole", gone ? 0 : vlen(vr_propimp_bl_crate.velocity), gone ? 0 : vlen(vr_propimp_bl_crate.origin - vr_propimp_bl_org)));
        VR_PropImp_Done(vr_propimp_bl_crate);
        vr_propimp_bl_crate = world;
    }
    if(vr_propimp_bl_step >= 4)
    {
        bprint("blastpush: done\n");
        vr_propimp_bl_next = 0;
        return;
    }
    if(!vr_propimp_bl_pending)
    {
        // (Made now, blown a moment later: its rigid body is made with the physics' next step.)
        vr_propimp_bl_pending = VR_Crate_TestSpawn(at, 1, 0, FALSE);
        vr_propimp_bl_pending.health = 1000; // (thrown, not broken)
        vr_propimp_bl_next = time + 0.5;
        return;
    }
    vr_propimp_bl_crate = vr_propimp_bl_pending;
    vr_propimp_bl_pending = world;
    vr_propimp_bl_org = vr_propimp_bl_crate.origin;
    entity boom = spawn();
    setorigin(boom, (vr_propimp_bl_crate.absmin + vr_propimp_bl_crate.absmax) * 0.5 - v_forward * 48);
    if(vr_propimp_bl_step == 0)
    {
        ad_T_RadiusDamage(boom, self, 60, world, ad_DAMAGEALL);
    }
    else if(vr_propimp_bl_step == 2)
    {
        // An AD plasma bolt's blast (the Plasma Gun's splash, 20): thrown as vr_ad_plasma_push (VR_ADGun_BlastPush).
        boom.ad_classtype = ad_CT_PROJ_PLASMA;
        ad_T_RadiusDamage(boom, self, 20, world, ad_DAMAGEALL);
    }
    else
    {
        T_RadiusDamage(boom, self, vr_propimp_bl_step == 3 ? 20 : 60, world);
    }
    remove(boom);
    vr_propimp_bl_step++;
    vr_propimp_bl_next = time + 0.25;
};

// vr_prop_impact_test 4 (Debug > Gore Tests, "Fuel Tank and Crate Thrown at Monsters"; ROUND22.md, "A thrown fuel tank
// hurts"): a flamethrower's tank lying about (vr_ad_enemyguns.qc VR_FuelTank_Attach, its gun made ahead) and a small
// crate as heavy as it (.vr_prop_mass vr_adgun_tank_mass) each thrown at 10, 13 and 16 m/s from 40 units off its face
// into a grunt, then an ogre (made 200 ahead, its health 1000 to see the whole hit), as a hand lets go (VR_Carry_Loose,
// VR_Carry_Throw); the tank's rope 3 m meanwhile (at 0.9 its gun held it back short of the monster). One "fueltankhit:" line a throw (the damage dealt), one a pair (passed: the tank hurt, stayed whole,
// and where the crate hit its damage within 0.3-3x the crate's), then "fueltankhit: N passed, M failed". Use god and notarget. Each throw waits
// for its hit (the throw over: .throwhit, or the monster hurt); 1.5 s without: no hit.
float vr_propimp_ft_on;     // running
float vr_propimp_ft_case;   // 0..11: monster (6 each), speed (2 each), tank or crate
float vr_propimp_ft_phase;  // 0 make, 1 settle, 2 in flight
float vr_propimp_ft_frames;
float vr_propimp_ft_h0;     // the monster's health before the throw
float vr_propimp_ft_bursts; // tanks burst before the throw
float vr_propimp_ft_pass;
float vr_propimp_ft_fail;
float vr_propimp_ft_tankdmg; // this pair's tank damage (-1: no hit)
float vr_propimp_ft_tankok;  // and whether it stayed whole
entity vr_propimp_ft_mon;
entity vr_propimp_ft_gun;   // the tank's gun (world: the crate's throw)
entity vr_propimp_ft_prop;  // what is thrown
entity vr_propimp_ft_at;    // where you stood at the start, facing the way you faced
float vr_propimp_ft_rope;   // vr_adgun_tank_rope before (3 m while it runs: its gun 0.9 m off held the tank back)
float VR_PROPIMP_FT_SETTLE = 36; // frames: the monster dropped to the floor, the bodies made
float VR_PROPIMP_FT_GIVEUP = 108; // frames (1.5 s) in flight without a hit

float(float c) VR_PropImp_FtSpeed = { return 10 + 3 * mod(floor(c / 2), 3); };

void() VR_PropImp_FtClear =
{
    if(vr_propimp_ft_mon != world && vr_propimp_ft_mon.modelindex)
    {
        remove(vr_propimp_ft_mon);
    }
    if(vr_propimp_ft_gun != world && vr_propimp_ft_gun.modelindex)
    {
        remove(vr_propimp_ft_gun); // (its tank goes with it: VR_FuelTank_Think)
    }
    else if(vr_propimp_ft_prop != world && vr_propimp_ft_prop.modelindex)
    {
        VR_Carry_Forget(vr_propimp_ft_prop);
        remove(vr_propimp_ft_prop);
    }
    vr_propimp_ft_mon = world;
    vr_propimp_ft_gun = world;
    vr_propimp_ft_prop = world;
};

// Each frame while it runs; self the player.
void() VR_PropImp_FtStep =
{
    float c = vr_propimp_ft_case;
    float tank = mod(c, 2) == 0;
    float ogre = c >= 6;
    float speed = VR_PropImp_FtSpeed(c);
    entity at = vr_propimp_ft_at; // (where you stood at the start: you may be pushed about)
    makevectors('0 1 0' * at.angles_y);
    vector fwd = v_forward;
    entity oself;
    if(vr_propimp_ft_phase == 0)
    {
        oself = self;
        self = spawn();
        self.classname = ogre ? "monster_ogre" : "monster_army";
        setorigin(self, at.origin + fwd * 200);
        self.angles_y = at.angles_y + 180;
        if(ogre) { monster_ogre(); } else { monster_army(); }
        vr_propimp_ft_mon = self;
        self = oself;
        if(tank)
        {
            vr_propimp_ft_gun = VR_ADWeapons_Make(at, 318, 130); // (a flamethrower lying with its tank)
            vr_propimp_ft_prop = VR_FuelTank_Of(vr_propimp_ft_gun);
        }
        vr_propimp_ft_phase = 1;
        vr_propimp_ft_frames = 0;
        return;
    }
    vr_propimp_ft_frames++;
    entity m = vr_propimp_ft_mon;
    entity t = vr_propimp_ft_prop;
    if(vr_propimp_ft_phase == 1)
    {
        if(vr_propimp_ft_frames < VR_PROPIMP_FT_SETTLE)
        {
            return;
        }
        vector mid = (m.absmin + m.absmax) * 0.5;
        vector from = mid - fwd * (m.maxs_x + 40); // (40 units clear of its face)
        from_z = m.absmin_z + 20; // (both as high: a small crate's middle, a little over the floor)
        if(tank)
        {
            if(t == world)
            {
                bprint("fueltankhit: no tank made with the flamethrower\n");
                vr_propimp_ft_tankdmg = -1;
                vr_propimp_ft_tankok = FALSE;
                vr_propimp_ft_phase = 3;
                return;
            }
            VR_Carry_Start(self, cVR_MainHand, t);
            VR_Carry_Forget(t);
            t.carry_player = self;
            setorigin(t, from);
            VR_Carry_Loose(t);
        }
        else
        {
            t = VR_Crate_TestSpawn(from, 1, 0, FALSE);
            setorigin(t, t.origin + '0 0 4'); // (off the floor: it would slide on it)
            t.flags = t.flags - (t.flags & FL_ONGROUND);
            t.vr_prop_mass = max(0.2, cvar("vr_adgun_tank_mass")); // (as heavy as the tank)
            t.carry_player = self;
            VR_Carry_Loose(t);
            vr_propimp_ft_prop = t;
        }
        t.vr_spin = '0 0 0';
        t.angles = '0 0 0';
        m.health = 1000;
        vr_propimp_ft_h0 = m.health;
        vr_propimp_ft_bursts = vr_fueltank_bursts;
        VR_Carry_Throw(t, fwd * speed * VRMetersToUnits());
        dprint(sprintf("fueltankhit: thrown from %v (box %v %v), the monster's middle %v\n", t.origin, t.mins, t.maxs, mid));
        vr_propimp_ft_phase = 2;
        vr_propimp_ft_frames = 0;
        return;
    }
    if(vr_propimp_ft_phase == 2)
    {
        float hurt = m.health < vr_propimp_ft_h0;
        if(!hurt && t.throwhit == QVR_THROWHIT_NEVER_HIT && vr_propimp_ft_frames < VR_PROPIMP_FT_GIVEUP)
        {
            return;
        }
        float dmg = hurt ? vr_propimp_ft_h0 - m.health : (vr_propimp_ft_frames >= VR_PROPIMP_FT_GIVEUP ? -1 : 0);
        float whole = !tank || (t.modelindex && t.health == t.max_health && vr_fueltank_bursts == vr_propimp_ft_bursts);
        bprint(strcat(sprintf("fueltankhit: %s (%.1f kg) at %g m/s into the %s: ", tank ? "fuel tank" : "small crate",
            propvalue(t, "mass"), speed, ogre ? "ogre" : "grunt"),
            sprintf("%s after %g frames%s", dmg < 0 ? "no hit" : sprintf("%.1f damage", dmg), vr_propimp_ft_frames,
            tank ? (whole ? ", the tank whole" : ", the tank hurt or burst") : ""),
            sprintf(" (it at %v, the monster's middle %v)\n", t.origin, (m.absmin + m.absmax) * 0.5)));
        if(tank)
        {
            vr_propimp_ft_tankdmg = dmg;
            vr_propimp_ft_tankok = whole;
        }
        else
        {
            // (The tank hurts and stays whole; against the crate's, where it hit square: a small tank meets a monster's
            // body where it is rounded, a crate where it is flat, so the hit's own speed differs, 50-100% of the throw.)
            float ratio = dmg > 0 ? vr_propimp_ft_tankdmg / dmg : 0;
            float ok = vr_propimp_ft_tankdmg > 0 && vr_propimp_ft_tankok && (dmg <= 0 || (ratio >= 0.3 && ratio <= 3));
            bprint(sprintf("fueltankhit: %s at %g m/s: tank %.1f, crate %.1f (x%.2f%s): %s\n", ogre ? "ogre" : "grunt", speed,
                vr_propimp_ft_tankdmg, dmg, ratio, dmg > 0 ? "" : ", the crate missed", ok ? "passed" : "FAILED"));
            if(ok) { vr_propimp_ft_pass++; } else { vr_propimp_ft_fail++; }
        }
        vr_propimp_ft_phase = 3;
    }
    // Phase 3: this throw done.
    VR_PropImp_FtClear();
    vr_propimp_ft_case++;
    vr_propimp_ft_phase = 0;
    if(vr_propimp_ft_case >= 12)
    {
        vr_propimp_ft_on = FALSE;
        cvar_set("vr_adgun_tank_rope", ftos(vr_propimp_ft_rope));
        bprint(sprintf("fueltankhit: %g passed, %g failed\n", vr_propimp_ft_pass, vr_propimp_ft_fail));
    }
};

// vr_prop_impact_test 5 (Debug > Gore Tests, "Flail Head Hits"; ROUND22.md, "His config's tuning as defaults; the loose
// flail head hits"): the head of a flail lying about (vr_ad_melee.qc VR_FlailHead_Impact; AD mounted), its chain 2 m
// meanwhile (vr_flail_chain: the handle made 130 ahead holds it within reach of the target). Cases, each from 40 units
// off the target, its health 1000 to see the whole hit; each watched until a second after its first hit, or 1.5 s:
//   0-5: flung (no hand: kicked, trailing its handle) into a grunt at 13, 16, 19 m/s, then a small crate as heavy
//        (vr_flail_mass) the same way: a pair passes if the head hurt it once (no repeat hit) and, where the crate hit,
//        within 0.3-3x the crate's damage;
//   6:   thrown by a hand at 13 m/s into a grunt (VR_Carry_Loose, VR_Carry_Throw): hurt once;
//   7:   made from your hand as you let go of the flail (its grace) and flung back at you: you unhurt;
//   8:   (the control) a head lying beside a grunt's feet (no grace) flung at it: it hurt once;
//   9:   the same, the grunt's own (dropped by it: its grace): it unhurt. (A Box3D prop never meets a player: his
//        flung hits are touches, which the head has none of, as the fuel tank; case 7 checks nothing reaches you.)
// One "flailhead_hit:" line a case, then "flailhead_hit: N passed, M failed". Use notarget, not god (cases 7, 8 read
// your health).
float vr_propimp_fh_on;
float vr_propimp_fh_case;     // 0..9 (above)
float vr_propimp_fh_phase;    // 0 make, 1 settle, 2 in flight
float vr_propimp_fh_frames;
float vr_propimp_fh_h0;       // the target's health before
float vr_propimp_fh_mass;     // what flies, kg (kept: a crate broken by its hit is gone)
float vr_propimp_fh_last;     // and last frame
float vr_propimp_fh_hits;     // its health's drops in flight
float vr_propimp_fh_first;    // the frame of the first (-1: none yet)
float vr_propimp_fh_spared0;  // vr_flailhead_spared before the case
float vr_propimp_fh_headdmg;  // this speed's head damage and hits (cases 0-5)
float vr_propimp_fh_headhits;
float vr_propimp_fh_pass;
float vr_propimp_fh_fail;
float vr_propimp_fh_chain;    // vr_flail_chain before
float vr_propimp_fh_health;   // your health before
entity vr_propimp_fh_mon;
entity vr_propimp_fh_gun;     // the flail's handle
entity vr_propimp_fh_prop;    // what flies
entity vr_propimp_fh_at;      // where you stood at the start, facing the way you faced
float VR_PROPIMP_FH_AFTER = 72;  // frames watched after the first hit (a repeat hit)
float VR_PROPIMP_FH_GIVEUP = 108; // frames (1.5 s) without a hit: none (past the heads' 1 s grace)

float(float c) VR_PropImp_FhSpeed = { return c < 6 ? 13 + 3 * floor(c / 2) : 13; };

// The flail made this frame (VR_ADWeapons_Make's).
entity() VR_PropImp_FhFlail =
{
    for(entity w = find(world, classname, "thrown_weapon"); w; w = find(w, classname, "thrown_weapon"))
    {
        if(VR_FlailHead_IsFlail(w) && w.vr_drop_born == time)
        {
            return w;
        }
    }
    return world;
};

// `w`'s head made again from `xFrom` (a player's hand, a monster's drop: its grace), the one beside it gone.
entity(entity w, entity xFrom) VR_PropImp_FhRemake =
{
    entity h = VR_FlailHead_Of(w);
    if(h)
    {
        w.vr_fuellink = world;
        remove(h);
    }
    VR_FlailHead_Attach(w, xFrom);
    return VR_FlailHead_Of(w);
};

void() VR_PropImp_FhClear =
{
    if(vr_propimp_fh_mon != world && vr_propimp_fh_mon.modelindex)
    {
        remove(vr_propimp_fh_mon);
    }
    if(vr_propimp_fh_gun != world && vr_propimp_fh_gun.modelindex)
    {
        remove(vr_propimp_fh_gun); // (its head goes with it: VR_FlailHead_Think)
    }
    else if(vr_propimp_fh_prop != world && vr_propimp_fh_prop.modelindex)
    {
        VR_Carry_Forget(vr_propimp_fh_prop);
        remove(vr_propimp_fh_prop);
    }
    vr_propimp_fh_mon = world;
    vr_propimp_fh_gun = world;
    vr_propimp_fh_prop = world;
};

// Each frame while it runs; self the player.
void() VR_PropImp_FhStep =
{
    float c = vr_propimp_fh_case;
    float crate = (c < 6 && mod(c, 2) == 1);
    float onYou = (c == 7);
    float speed = VR_PropImp_FhSpeed(c);
    float m2u = VRMetersToUnits();
    entity at = vr_propimp_fh_at;
    makevectors('0 1 0' * at.angles_y);
    vector fwd = v_forward, right = v_right;
    entity oself;
    if(vr_propimp_fh_phase == 0)
    {
        if(!onYou)
        {
            oself = self;
            self = spawn();
            self.classname = "monster_army";
            setorigin(self, at.origin + fwd * 200);
            self.angles_y = at.angles_y + 180;
            monster_army();
            vr_propimp_fh_mon = self;
            self = oself;
        }
        if(!crate)
        {
            VR_ADWeapons_Make(at, 346, onYou ? 60 : 130); // (the death sergeant's flail lying with its head)
            vr_propimp_fh_gun = VR_PropImp_FhFlail();
            vr_propimp_fh_prop = VR_FlailHead_Of(vr_propimp_fh_gun);
        }
        vr_propimp_fh_phase = 1;
        vr_propimp_fh_frames = 0;
        return;
    }
    vr_propimp_fh_frames++;
    entity m = vr_propimp_fh_mon;
    entity target = onYou ? self : m;
    entity t = vr_propimp_fh_prop;
    entity w = vr_propimp_fh_gun;
    if(vr_propimp_fh_phase == 1)
    {
        if(vr_propimp_fh_frames < VR_PROPIMP_FT_SETTLE)
        {
            return;
        }
        vector mid = onYou ? self.origin + '0 0 8' : (m.absmin + m.absmax) * 0.5;
        vector from = onYou ? self.origin + fwd * (self.maxs_x + 40) + '0 0 8' : mid - fwd * (m.maxs_x + 40);
        if(!onYou)
        {
            from_z = m.absmin_z + 20;
        }
        if(!crate && (w == world || t == world))
        {
            bprint(sprintf("flailhead_hit: case %g: no flail or head made (AD mounted?)\n", c));
            vr_propimp_fh_fail++;
            vr_propimp_fh_phase = 3;
            return;
        }
        vector vel = fwd * speed * m2u;
        if(crate)
        {
            t = VR_Crate_TestSpawn(from, 1, 0, FALSE);
            setorigin(t, t.origin + '0 0 4');
            t.flags = t.flags - (t.flags & FL_ONGROUND);
            t.vr_prop_mass = max(0.2, cvar("vr_flail_mass")); // (as heavy as the head)
            vr_propimp_fh_prop = t;
        }
        else if(c == 7)
        {
            // (its handle off to your side: the head flies on past you and the chain pulls the handle after it, which
            // would hit you as a flung weapon if it came straight at you)
            setorigin(w, self.origin + right * 48 + fwd * 24);
            vel = -vel;
            self.vr_flail_head = from; // (where the engine left your hand's head, flying back at you)
            self.vr_flail_headvel = vel;
            t = VR_PropImp_FhRemake(w, self);
            vr_propimp_fh_prop = t;
        }
        else if(c >= 8)
        {
            setorigin(w, m.origin + right * 4 + '0 0 8'); // (the handle at its feet: dropped)
            t = VR_PropImp_FhRemake(w, c == 9 ? m : world); // (9 its drop: its grace; 8 none: the control)
            vr_propimp_fh_prop = t;
            vel = normalize(mid - t.origin) * speed * m2u;
        }
        else
        {
            setorigin(t, from);
        }
        if(t == world)
        {
            bprint(sprintf("flailhead_hit: case %g: no head made again\n", c));
            vr_propimp_fh_fail++;
            vr_propimp_fh_phase = 3;
            return;
        }
        t.vr_spin = '0 0 0';
        if(c == 6)
        {
            VR_Carry_Start(self, cVR_MainHand, t);
            VR_Carry_Forget(t);
            t.carry_player = self;
            VR_Carry_Loose(t);
            VR_Carry_Throw(t, vel);
        }
        else
        {
            t.velocity = vel; // (flung: no hand's throw)
        }
        target.health = 1000;
        vr_propimp_fh_h0 = target.health;
        vr_propimp_fh_mass = t.vr_prop_mass > 0 ? t.vr_prop_mass : propvalue(t, "mass");
        vr_propimp_fh_last = target.health;
        vr_propimp_fh_hits = 0;
        vr_propimp_fh_first = -1;
        vr_propimp_fh_spared0 = vr_flailhead_spared;
        dprint(sprintf("flailhead_hit: case %g from %v at %v, the target's middle %v\n", c, t.origin, vel, mid));
        vr_propimp_fh_phase = 2;
        vr_propimp_fh_frames = 0;
        return;
    }
    if(vr_propimp_fh_phase == 2)
    {
        if(target.health < vr_propimp_fh_last)
        {
            vr_propimp_fh_hits++;
            if(vr_propimp_fh_first < 0)
            {
                vr_propimp_fh_first = vr_propimp_fh_frames;
            }
        }
        vr_propimp_fh_last = target.health;
        if(vr_propimp_fh_first >= 0 ? vr_propimp_fh_frames < vr_propimp_fh_first + VR_PROPIMP_FH_AFTER :
                                      vr_propimp_fh_frames < VR_PROPIMP_FH_GIVEUP)
        {
            return;
        }
        float dmg = vr_propimp_fh_h0 - target.health;
        float spared = vr_flailhead_spared - vr_propimp_fh_spared0;
        string what = crate ? "small crate" : "flail head";
        string how = c == 6 ? "thrown" : "flung";
        string into = onYou ? "you" : "a grunt";
        if(c == 7) { what = "your hand's flail head"; }
        if(c == 9) { what = "its own dropped flail head"; }
        bprint(sprintf("flailhead_hit: %s (%.1f kg) %s at %g m/s into %s: %.1f damage, %g hits, %g spared\n", what,
            vr_propimp_fh_mass, how, speed, into, dmg, vr_propimp_fh_hits, spared));
        float ok = -1;
        if(c < 6 && !crate)
        {
            vr_propimp_fh_headdmg = dmg;
            vr_propimp_fh_headhits = vr_propimp_fh_hits;
        }
        else if(crate)
        {
            float ratio = dmg > 0 ? vr_propimp_fh_headdmg / dmg : 0;
            ok = (vr_propimp_fh_headdmg > 0 && vr_propimp_fh_headhits == 1 && (dmg <= 0 || (ratio >= 0.3 && ratio <= 3)));
            bprint(sprintf("flailhead_hit: flung at %g m/s: head %.1f (%g hits), crate %.1f (x%.2f%s): %s\n", speed,
                vr_propimp_fh_headdmg, vr_propimp_fh_headhits, dmg, ratio, dmg > 0 ? "" : ", the crate missed",
                ok ? "passed" : "FAILED"));
        }
        else if(c == 6 || c == 8)
        {
            ok = (dmg > 0 && vr_propimp_fh_hits == 1);
        }
        else
        {
            ok = dmg <= 0;
        }
        if(ok >= 0)
        {
            if(!crate)
            {
                bprint(sprintf("flailhead_hit: case %g: %s\n", c, ok ? "passed" : "FAILED"));
            }
            if(ok) { vr_propimp_fh_pass++; } else { vr_propimp_fh_fail++; }
        }
        vr_propimp_fh_phase = 3;
    }
    // Phase 3: this case done.
    if(onYou)
    {
        self.health = vr_propimp_fh_health;
    }
    VR_PropImp_FhClear();
    vr_propimp_fh_case++;
    vr_propimp_fh_phase = 0;
    if(vr_propimp_fh_case >= 10)
    {
        vr_propimp_fh_on = FALSE;
        cvar_set("vr_flail_chain", ftos(vr_propimp_fh_chain));
        bprint(sprintf("flailhead_hit: %g passed, %g failed\n", vr_propimp_fh_pass, vr_propimp_fh_fail));
    }
};

// Every server frame (StartFrame): a test asked for (vr_prop_impact_test).
void() VR_PropImp_TestFrame =
{
    entity p;
    entity oself;
    if(vr_propimp_ft_on)
    {
        p = find(world, classname, "player");
        if(p != world)
        {
            oself = self;
            self = p;
            VR_PropImp_FtStep();
            self = oself;
        }
    }
    if(vr_propimp_fh_on)
    {
        p = find(world, classname, "player");
        if(p != world)
        {
            oself = self;
            self = p;
            VR_PropImp_FhStep();
            self = oself;
        }
    }
    if(vr_propimp_bl_next && time >= vr_propimp_bl_next)
    {
        p = find(world, classname, "player");
        if(p != world)
        {
            oself = self;
            self = p;
            VR_PropImp_BlastStep();
            self = oself;
        }
    }
    if(vr_propimp_lt_next && time >= vr_propimp_lt_next)
    {
        p = find(world, classname, "player");
        if(p != world)
        {
            oself = self;
            self = p;
            VR_PropImp_LightStep();
            self = oself;
        }
    }
    float ask = cvar("vr_prop_impact_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_prop_impact_test", "0");
    p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    oself = self;
    self = p;
    if(ask == 2)
    {
        vr_propimp_lt_step = 0;
        vr_propimp_lt_thing = world;
        vr_propimp_lt_crate = world;
        VR_PropImp_LightStep();
    }
    else if(ask == 4)
    {
        VR_PropImp_FtClear();
        vr_propimp_ft_on = TRUE;
        vr_propimp_ft_case = 0;
        vr_propimp_ft_phase = 0;
        vr_propimp_ft_pass = 0;
        vr_propimp_ft_fail = 0;
        if(!vr_propimp_ft_at)
        {
            vr_propimp_ft_at = spawn();
            vr_propimp_ft_at.classname = "vr_propimp_ft_at";
        }
        setorigin(vr_propimp_ft_at, self.origin);
        vr_propimp_ft_at.angles = '0 1 0' * self.angles_y;
        vr_propimp_ft_rope = cvar("vr_adgun_tank_rope");
        cvar_set("vr_adgun_tank_rope", "3");
    }
    else if(ask == 5)
    {
        VR_PropImp_FhClear();
        vr_propimp_fh_on = TRUE;
        vr_propimp_fh_case = 0;
        vr_propimp_fh_phase = 0;
        vr_propimp_fh_pass = 0;
        vr_propimp_fh_fail = 0;
        if(!vr_propimp_fh_at)
        {
            vr_propimp_fh_at = spawn();
            vr_propimp_fh_at.classname = "vr_propimp_fh_at";
        }
        setorigin(vr_propimp_fh_at, self.origin);
        vr_propimp_fh_at.angles = '0 1 0' * self.angles_y;
        vr_propimp_fh_chain = cvar("vr_flail_chain");
        vr_propimp_fh_health = self.health;
        cvar_set("vr_flail_chain", "2");
    }
    else if(ask == 3)
    {
        vr_propimp_bl_step = 0;
        vr_propimp_bl_crate = world;
        vr_propimp_bl_pending = world;
        VR_PropImp_BlastStep();
    }
    else
    {
        VR_PropImp_Run();
    }
    self = oself;
};
#endif

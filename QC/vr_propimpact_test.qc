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

// Every server frame (StartFrame): a test asked for (vr_prop_impact_test).
void() VR_PropImp_TestFrame =
{
    float ask = cvar("vr_prop_impact_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_prop_impact_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    VR_PropImp_Run();
    self = oself;
};

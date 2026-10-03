// vr_burning.qc -- things set on fire (docs/vr-port/ROUND21.md, "Burning"; Combat > Burning).
//
// A lit torch's blow, a thrown lit torch, a lava nail (the nailguns' lava ammo: vr_burn_lava_nails) and, with
// vr_burn_touch, a lit torch held or thrown touching it set a monster, a player or a corpse (vr_burn_corpses) on fire.
// The fire (a vr_burn) is one flame where it was struck (id's small flame, progs/flame2.mdl, which lights it), spreading
// over the body: a smaller flame every vr_burn_spread s, turned a little but upright, up to vr_burn_flames; more hits
// add a flame each, up to vr_burn_flames_max. Its damage never stacks: vr_burn_damage a second, whatever the flames, for
// vr_burn_time s from the last hit (vr_burn_corpse_time for a corpse; vr_burn_corpse_damage of it hurts a corpse:
// vr_corpse_gib's health). Burns are painted under every flame as it burns (the wounds' QVR_WOUND_BURN, no wound under
// them: extra 1). The flames sit on the model as drawn (hitmodel_any), fitted again a few times a second as it moves,
// dies and falls. Out when its time is up (the flames shrink away), in water, gibbed or gone.
// Wood (VR_Burn_Wood: a crate, a piece of one) burns its time without damage and breaks (a crate: charred pieces) or
// burns away (a piece), setting the wood near it on fire; an explosive box is metal (VR_Burn_Metal): it never catches.
// Your own torch's flame on you (vr_burn_self) sets you on fire; held upside down, on its hand, it is dropped
// (vr_burn_drop): VR_Burn_OnYou (ROUND21.md, "Torch flames: upright from the head's top, swings, smoke, burning you").
// `vr_burn_test <n>` (Debug > Burning Tests): 1 a torch's blow, 2 a lava nail, 3 a touch, on the nearest monster or
// corpse; `developer 1` prints `burning: ...`.

// On what burns:
.entity burn_fire;        // its fire (a vr_burn): world, not burning
.float burn_add_next;     // when a hit can next add a flame
.float burn_lit;          // when it was last set on fire (a piece lit by a hit isn't burst by it: VR_Burn_JustLit)
// On a vr_burn (enemy: what burns; owner: who lit it):
.entity burn_flames;      // its flames (a chain through burn_next; the first is lit, the rest cast no light)
.entity burn_next;
.float burn_count;        // its flames
.float burn_end;          // when it goes out (the flames shrink away over VR_BURN_FADE s after)
.float burn_tick;         // its next damage
.float burn_spread_next;  // its next flame spreading
.float burn_fit_next;     // when its flames are next fitted to the body
.float burn_model;        // the body's modelindex when lit (gibbed: another)
.float burn_dead;         // the body was dead at the last fit
.float burn_catch_next;   // wood's fire: when it next sets the crates and pieces touching it on fire
.float burn_char_at;      // a piece's fire: when the piece is charred (half its time)
// On a flame (owner: its fire):
.vector burn_off;         // where on the body: from its origin, in its yaw's axes
.float burn_size;         // its full scale
.float burn_born;         // when it came (it grows in)
.float burn_part;         // a player's: 0 on the body (burn_off in its yaw's axes); a hand + 1: on that hand or its arm
                          // (burn_off in the hand's axes: it goes with it)
.vector burn_out;         // a player's: the way out of the body where it sits (its burns are painted from there)

// Where a player is set on fire (VR_Burn_OnYou, vr_burn_self: the body's shapes, not its Quake model): the part, the way
// out of the body; set just round VR_Burn_Ignite.
float vr_burn_part;
vector vr_burn_out;

float VR_BURN_FADE = 0.5;    // s the flames take to shrink away
float VR_BURN_GROW = 0.3;    // s a new flame takes to grow
float VR_BURN_ADD_GAP = 0.3; // s between flames added by hits (a super nailgun's stream adds one a gap)
float VR_BURN_TICK = 0.5;    // s between its damages

float VR_BURN_BLOW = 0;      // how it was lit (VR_Burn_Ignite): a blow (a mark where it struck),
float VR_BURN_TOUCH = 1;     // a touch (no mark but its flame's),
float VR_BURN_NAIL = 2;      // a missile (its own hit marks it)
float VR_BURN_SPREAD = 3;    // burning wood touching it (vr_burn_crate_spread)
float VR_BURN_SCATTER = 4;   // a piece of a crate broken while burning (no sound: they come many at once)

// What fire doesn't hurt (VR_Liquid_Immune's: what lives in lava or water, the bosses), but the training dummy burns.
float(entity e) VR_Burn_Fireproof =
{
    return e.classname != "vr_dummy" && VR_Liquid_Immune(e);
};

// Wood that burns as a crate does (its flames on its box, turned with it; no damage: it burns its time and breaks or
// burns away): a crate, a piece of one.
float(entity e) VR_Burn_Wood =
{
    return VR_Crate_Is(e) || VR_CratePiece_Is(e);
};

// An explosive box (misc_explobox, misc_explobox2; blowing up: explo_box): metal.
float(entity e) VR_Burn_Metal =
{
    return VR_Prop_Metal(e); // (vr_crates.qc)
};

// Whether `e` can be set on fire: a monster or a player alive, a monster's corpse (dying or lying: vr_corpse_gib), a
// wooden crate (vr_burn_crates), a piece of one not charred (vr_burn_pieces). Never an explosive box: it is metal
// (NOTES.md vrfiringrange_2026-10-03_02-29-24); nothing burning sets one off.
float(entity e) VR_Burn_Can =
{
    if(e == world || !e.modelindex || e.vr_gib || VR_Burn_Fireproof(e) || VR_Burn_Metal(e))
    {
        return FALSE;
    }
    if(VR_Crate_Is(e))
    {
        return cvar_hget(cvarh_vr_burn_crates) != 0;
    }
    if(VR_CratePiece_Is(e))
    {
        return cvar_hget(cvarh_vr_burn_pieces) != 0 && !e.vr_charred;
    }
    if(e.health > 0)
    {
        return (e.flags & (FL_MONSTER | FL_CLIENT)) && e.takedamage;
    }
    return cvar_hget(cvarh_vr_burn_corpses) && (e.flags & FL_MONSTER) && (e.vr_corpse || e.vr_corpse_dying);
};

// A point from the body's origin in its yaw's axes, and back (no makevectors: callers' v_forward is kept). A crate's in
// its model's own axes (it tips and rolls: its flames go with it).
vector(entity v, vector p) VR_Burn_ToLocal =
{
    if(VR_Burn_Wood(v))
    {
        vector cd = p - v.origin;
        vector ax = modelpoint(v, v.angles, '1 0 0');
        vector ay = modelpoint(v, v.angles, '0 1 0');
        vector az = modelpoint(v, v.angles, '0 0 1');
        vector cr;
        cr_x = (cd * ax) / max(0.0001, ax * ax);
        cr_y = (cd * ay) / max(0.0001, ay * ay);
        cr_z = (cd * az) / max(0.0001, az * az);
        return cr;
    }
    float a = v.angles_y * 0.0174533;
    vector d = p - v.origin;
    vector r;
    r_x = d_x * cos(a) + d_y * sin(a);
    r_y = d_y * cos(a) - d_x * sin(a);
    r_z = d_z;
    return r;
};

vector(entity v, vector o) VR_Burn_ToWorld =
{
    if(VR_Burn_Wood(v))
    {
        return v.origin + modelpoint(v, v.angles, o);
    }
    float a = v.angles_y * 0.0174533;
    vector r;
    r_x = o_x * cos(a) - o_y * sin(a);
    r_y = o_x * sin(a) + o_y * cos(a);
    r_z = o_z;
    return v.origin + r;
};

// A player's flame on a hand or its arm (xPart: the hand + 1): its place in the hand's axes, and back (v_forward etc.
// kept).
vector(entity v, float xPart, vector p) VR_Burn_ToHand =
{
    vector oldForward = v_forward, oldRight = v_right, oldUp = v_up;
    makevectors(VRGetEntHandRot(v, xPart - 1));
    vector d = p - VRGetEntHandPos(v, xPart - 1);
    vector r;
    r_x = d * v_forward;
    r_y = d * v_right;
    r_z = d * v_up;
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return r;
};

vector(entity v, float xPart, vector o) VR_Burn_FromHand =
{
    vector oldForward = v_forward, oldRight = v_right, oldUp = v_up;
    makevectors(VRGetEntHandRot(v, xPart - 1));
    vector r = VRGetEntHandPos(v, xPart - 1) + v_forward * o_x + v_right * o_y + v_up * o_z;
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return r;
};

// A flame's place on its body, as it is now.
vector(entity v, entity fl) VR_Burn_FlameAt =
{
    if((v.flags & FL_CLIENT) && fl.burn_part > 0)
    {
        return VR_Burn_FromHand(v, fl.burn_part, fl.burn_off);
    }
    return VR_Burn_ToWorld(v, fl.burn_off);
};

// Where along a..b it first meets a crate (its model's box, turned with it: a crate is a box; the drawn model's tracking
// lags a crate that was moved and lies still), -1 missed.
float(entity v, vector a, vector b) VR_Burn_CrateCast =
{
    vector la = VR_Burn_ToLocal(v, a);
    vector lb = VR_Burn_ToLocal(v, b);
    vector lo = modelbounds(v, 0);
    vector hi = modelbounds(v, 1);
    vector d = lb - la;
    float t0 = 0;
    float t1 = 1;
    float k = 0;
    while(k < 3)
    {
        float pa = k == 0 ? la_x : (k == 1 ? la_y : la_z);
        float pd = k == 0 ? d_x : (k == 1 ? d_y : d_z);
        float mn = k == 0 ? lo_x : (k == 1 ? lo_y : lo_z);
        float mx = k == 0 ? hi_x : (k == 1 ? hi_y : hi_z);
        if(fabs(pd) < 0.0001)
        {
            if(pa < mn || pa > mx)
            {
                return -1;
            }
        }
        else
        {
            float ta = (mn - pa) / pd;
            float tb = (mx - pa) / pd;
            t0 = max(t0, min(ta, tb));
            t1 = min(t1, max(ta, tb));
            if(t0 > t1)
            {
                return -1;
            }
        }
        k = k + 1;
    }
    return t0;
};

// The point on the body's model as drawn nearest `p` as a flame would sit on it: standing, met from outside level with
// it, towards its middle; lying (dead), and a crate's top half, met from above. Missed (between its legs, past a lying body's end), tried again
// nearer its drawn middle, a fatter line (it sits a few units off it). Not found (a model the engine doesn't test: its box): `p` kept in the box.
// vr_burn_onbody: whether it was found.
float vr_burn_onbody;
vector(entity v, vector p) VR_Burn_OnBody =
{
    vector mc = modelcentre(v);
    float above = v.health <= 0 || VR_CratePiece_Is(v); // (a piece: a board lying, its flames on its top)
    if(VR_Crate_Is(v) && p_z > mc_z)
    {
        // Its top, unless something lies on it (a crate stacked on it: the flames would be under it).
        vector top = p;
        top_z = v.absmax_z + 1;
        traceline(top, top + '0 0 12', FALSE, v);
        above = trace_fraction >= 1;
    }
    float f = -1;
    float i = 0;
    while(i <= 4)
    {
        vector q = p + (mc - p) * (i / 4);
        vector a;
        vector b;
        if(above)
        {
            a = q;
            a_z = max(v.absmax_z, mc_z) + 48;
            b = q;
            b_z = v.absmin_z - 8;
        }
        else
        {
            b = mc;
            b_z = q_z;
            vector d = q - b;
            d_z = 0;
            if(vlen(d) < 1)
            {
                d = '1 0 0';
            }
            a = b + normalize(d) * 64;
        }
        if(VR_Burn_Wood(v))
        {
            f = VR_Burn_CrateCast(v, a, b);
        }
        else
        {
            f = hitmodel_any(v, a, b, i == 0 ? 1 : 4);
        }
        if(f >= 0)
        {
            vr_burn_onbody = TRUE;
            return a + (b - a) * f;
        }
        if(f == -2)
        {
            break;
        }
        i = i + 1;
    }
    vr_burn_onbody = f == -2;
    p_x = min(v.absmax_x, max(v.absmin_x, p_x));
    p_y = min(v.absmax_y, max(v.absmin_y, p_y));
    p_z = min(v.absmax_z, max(v.absmin_z, p_z));
    return p;
};

// A burn painted under a flame (no wound: extra 1), met from outside the body.
// A player's (vr_burn_out set, or a flame's burn_out): from that way out of the body (their drawn body, not the box).
vector vr_burn_scorch_out;
void(entity v, vector at, float xAmount) VR_Burn_Scorch =
{
    if((v.flags & FL_CLIENT) && vr_burn_scorch_out != '0 0 0')
    {
        woundevent(v, at + vr_burn_scorch_out * 4, vr_burn_scorch_out * -1, QVR_WOUND_BURN, xAmount, 1);
        return;
    }
    if(!VR_Wound_Takes(v) && !VR_Burn_Wood(v))
    {
        return;
    }
    vector mid = (v.absmin + v.absmax) * 0.5;
    vector out = at - mid;
    if(v.health <= 0 || (VR_Burn_Wood(v) && at_z > mid_z + 4))
    {
        out = '0 0 1'; // lying, a crate's top: from above
    }
    else
    {
        out_z = 0;
    }
    out = vlen(out) < 1 ? '0 0 1' : normalize(out);
    woundevent(v, at + out * 8, out * -1, QVR_WOUND_BURN, xAmount, 1);
};

// A flame on the fire's body at `at` (fitted onto it); xMain: the first, full size and lit.
void(entity xFire, vector at, float xMain) VR_Burn_AddFlame =
{
    entity v = xFire.enemy;
    float you = (v.flags & FL_CLIENT) && vr_burn_out != '0 0 0';
    vector p = at;
    vr_burn_onbody = TRUE;
    if(!you)
    {
        p = VR_Burn_OnBody(v, at); // (a player set on fire by the body's shapes: where they touched)
    }
    entity fl = spawn();
    fl.classname = "vr_burn_flame";
    fl.owner = xFire;
    fl.movetype = MOVETYPE_NONE;
    fl.solid = SOLID_NOT;
    setmodel(fl, "progs/flame2.mdl"); // id's small flame (the first flickers its own light: vr_emissive.cpp)
    fl.frame = 0;
    float size = max(0.05, cvar_hget(cvarh_vr_burn_flame_size));
    if(VR_Crate_Is(v))
    {
        size = size * max(0.05, cvar_hget(cvarh_vr_burn_crate_flame_size)); // (a crate is bigger than a grunt's body)
    }
    else if(VR_CratePiece_Is(v))
    {
        size = size * max(0.05, cvar_hget(cvarh_vr_burn_piece_flame_size)); // (a board: smaller)
    }
    if(!xMain)
    {
        size = size * max(0.05, cvar_hget(cvarh_vr_burn_flame_small)) * (0.8 + random() * 0.4);
        fl.alpha = 1; // drawn as ever, but no light of its own: the first flame's lights the body (vr_emissive.cpp)
    }
    fl.burn_size = size;
    fl.burn_born = time;
    fl.scale = size * 0.3;
    float tilt = cvar_hget(cvarh_vr_burn_flame_tilt);
    fl.angles_x = xMain ? 0 : crandom() * tilt;
    fl.angles_y = random() * 360;
    fl.angles_z = xMain ? 0 : crandom() * tilt;
    fl.burn_part = you ? vr_burn_part : 0;
    fl.burn_out = you ? vr_burn_out : '0 0 0';
    fl.burn_off = fl.burn_part > 0 ? VR_Burn_ToHand(v, fl.burn_part, p) : VR_Burn_ToLocal(v, p);
    setorigin(fl, p);
    fl.burn_next = xFire.burn_flames;
    if(xMain || xFire.burn_flames == world)
    {
        xFire.burn_flames = fl;
        fl.alpha = 0;
    }
    else
    {
        // After the first (the lit one stays first).
        fl.burn_next = xFire.burn_flames.burn_next;
        xFire.burn_flames.burn_next = fl;
    }
    xFire.burn_count = xFire.burn_count + 1;
    vr_burn_scorch_out = fl.burn_out;
    VR_Burn_Scorch(v, p, 20);
    vr_burn_scorch_out = '0 0 0';
    dprint(sprintf("burning: %s: flame %g at %v (%s, scale %.2f)\n", v.classname, xFire.burn_count, p,
                   vr_burn_onbody ? "on it" : "off it", size));
};

// A flame spread from one of the fire's: a hand's width from it, over the body.
void(entity xFire) VR_Burn_Spread =
{
    entity v = xFire.enemy;
    float pick = floor(random() * xFire.burn_count);
    entity from = xFire.burn_flames;
    while(pick > 0 && from.burn_next != world)
    {
        from = from.burn_next;
        pick = pick - 1;
    }
    if(from.burn_out != '0 0 0')
    {
        // On a player, from the body's shapes: a few units from it, on the same part, the same way out.
        float pa = random() * 360 * 0.0174533;
        vector o = from.burn_out;
        vector side;
        side_x = o_y; // (o x up)
        side_y = -o_x;
        side_z = 0;
        side = vlen(side) < 0.1 ? '1 0 0' : normalize(side);
        vector side2; // (o x side)
        side2_x = o_y * side_z - o_z * side_y;
        side2_y = o_z * side_x - o_x * side_z;
        side2_z = o_x * side_y - o_y * side_x;
        float pr = from.burn_part > 0 ? 2 + random() * 3 : 5 + random() * 6;
        vr_burn_part = from.burn_part;
        vr_burn_out = o;
        VR_Burn_AddFlame(xFire, from.origin + (side * cos(pa) + side2 * sin(pa)) * pr + '0 0 1' * random() * 2, FALSE);
        vr_burn_part = 0;
        vr_burn_out = '0 0 0';
        return;
    }
    float tries = 0;
    while(tries < 3)
    {
        float a = random() * 360 * 0.0174533;
        float r = 8 + random() * 10;
        vector p = from.origin;
        p_x = p_x + cos(a) * r;
        p_y = p_y + sin(a) * r;
        if(v.health > 0 && !VR_CratePiece_Is(v)) // (a piece's: on its top)
        {
            p_z = p_z + (random() * 16 - 6); // (fire climbs)
            p_z = min(v.absmax_z - 4, max(v.absmin_z + (v.absmax_z - v.absmin_z) * 0.2, p_z));
        }
        VR_Burn_OnBody(v, p);
        if(vr_burn_onbody)
        {
            VR_Burn_AddFlame(xFire, p, FALSE);
            return;
        }
        tries = tries + 1;
    }
};

// Out: its flames gone, the body no longer burning.
void(entity xFire, string xWhy) VR_Burn_Out =
{
    entity v = xFire.enemy;
    entity fl = xFire.burn_flames;
    while(fl != world)
    {
        entity n = fl.burn_next;
        remove(fl);
        fl = n;
    }
    xFire.burn_flames = world;
    if(v.burn_fire == xFire)
    {
        v.burn_fire = world;
    }
    dprint(sprintf("burning: %s out (%s)\n", v.classname, xWhy));
    remove(xFire);
};

// Burning wood (a crate, a piece), every 0.5 s once it has burnt vr_burn_crate_spread s: the crates and pieces touching it
// catch fire, where they are nearest it: two crates' boxes within vr_burn_crate_gap units, a piece and a crate or another
// piece within vr_burn_piece_gap (pieces lie about: near, not touching).
void(entity xFire) VR_Burn_CrateCatch =
{
    entity v = xFire.enemy;
    if(cvar_hget(cvarh_vr_burn_crate_spread) <= 0)
    {
        return;
    }
    float vpiece = VR_CratePiece_Is(v);
    vector mid = (v.absmin + v.absmax) * 0.5;
    entity e = findradius(mid, 128);
    while(e)
    {
        float gap = max(0, cvar_hget((vpiece || VR_CratePiece_Is(e)) ? cvarh_vr_burn_piece_gap : cvarh_vr_burn_crate_gap));
        if(e != v && VR_Burn_Wood(e) && e.burn_fire == world && VR_Burn_Can(e) &&
           e.absmin_x <= v.absmax_x + gap && e.absmax_x >= v.absmin_x - gap &&
           e.absmin_y <= v.absmax_y + gap && e.absmax_y >= v.absmin_y - gap &&
           e.absmin_z <= v.absmax_z + gap && e.absmax_z >= v.absmin_z - gap)
        {
            // Where it is nearest the burning one's middle.
            vector at;
            at_x = min(e.absmax_x, max(e.absmin_x, mid_x));
            at_y = min(e.absmax_y, max(e.absmin_y, mid_y));
            at_z = min(e.absmax_z, max(e.absmin_z, mid_z));
            VR_Burn_Ignite(e, xFire.owner, at, at - mid, VR_BURN_SPREAD);
        }
        e = e.chain;
    }
};

// The fire, every 0.05 s: its flames kept on the body (fitted to it a few times a second), grown in and shrunk away;
// a flame spreading; its damage every VR_BURN_TICK s (one fire's, whatever its flames), burns under its flames.
void() VR_Burn_Think =
{
    entity v = self.enemy;
    if(v == world || v.burn_fire != self || !v.modelindex || v.modelindex != self.burn_model || v.vr_gib)
    {
        VR_Burn_Out(self, "gone or gibbed");
        return;
    }
    if(VR_Wound_Level(v) >= 2)
    {
        particle2((v.absmin + v.absmax) * 0.5, '0 0 30', QVR_PARTICLE_PRESET_BIGSMOKE, 6);
        VR_Burn_Out(self, "in a liquid");
        return;
    }
    float dead = v.health <= 0;
    if(dead && !self.burn_dead)
    {
        // Died burning: it burns on as a corpse, or goes out.
        if(!cvar_hget(cvarh_vr_burn_corpses) || !(v.flags & FL_MONSTER) || VR_Burn_Fireproof(v))
        {
            VR_Burn_Out(self, "died");
            return;
        }
        self.burn_end = max(self.burn_end, time + cvar_hget(cvarh_vr_burn_corpse_time));
        self.burn_fit_next = 0;
        dprint(sprintf("burning: %s died burning: burns as a corpse till %.1f\n", v.classname, self.burn_end));
    }
    float crate = VR_Crate_Is(v);
    if(crate && time >= self.burn_end && v.takedamage && cvar_hget(cvarh_vr_burn_crate_break))
    {
        // Burnt through: it breaks (VR_Crate_Break), in a burst of embers and smoke.
        vector mid = (v.absmin + v.absmax) * 0.5;
        particle2(mid, '0 0 60', QVR_PARTICLE_PRESET_LAVASPIKE, 24);
        particle2(mid, '0 0 30', QVR_PARTICLE_PRESET_BIGSMOKE, 8);
        dprint(sprintf("burning: %s burnt through: it breaks\n", v.classname));
        entity fire = self;
        VR_Burn_Out(fire, "broke");
        v.vr_burnt_through = TRUE; // (its pieces charred: VR_Burn_NewPiece)
        T_Damage(v, world, world, v.health + 1000); // (as from nowhere: its pieces fall where they are)
        return;
    }
    float piece = VR_CratePiece_Is(v);
    if(piece && !v.vr_charred && time >= self.burn_char_at)
    {
        v.skin = VR_PIECE_SKIN_CHARRED; // half burnt: charred under its flames (it burns no more once out)
        v.vr_charred = TRUE;
        dprint(sprintf("burning: %s charred\n", v.classname));
    }
    if(piece && time >= self.burn_end)
    {
        // Burnt away: gone in a puff of embers (VR_CratePiece_BurnAway).
        entity pfire = self;
        VR_Burn_Out(pfire, "burnt away");
        VR_CratePiece_BurnAway(v);
        return;
    }
    float wood = crate || piece;
    if(time >= self.burn_end + VR_BURN_FADE)
    {
        VR_Burn_Out(self, "burnt out");
        return;
    }
    self.nextthink = time + 0.05;

    // Fitted to the body (it moves, dies, falls): each flame onto the model again where it is. A crate's stay where they
    // are on it (it is rigid: they turn with it).
    float fit = !wood && !((v.flags & FL_CLIENT) && self.burn_flames.burn_out != '0 0 0') &&
                (time >= self.burn_fit_next || dead != self.burn_dead); // (a player lit by the body's shapes: they stay put on it)
    self.burn_dead = dead;
    if(fit)
    {
        self.burn_fit_next = time + (dead ? 0.15 : 0.25);
    }
    float fade = min(1, max(0, (self.burn_end + VR_BURN_FADE - time) / VR_BURN_FADE));
    entity fl = self.burn_flames;
    while(fl != world)
    {
        vector p = VR_Burn_FlameAt(v, fl);
        if(fit)
        {
            vector q = VR_Burn_OnBody(v, p);
            if(vr_burn_onbody)
            {
                p = q;
                fl.burn_off = VR_Burn_ToLocal(v, p);
            }
        }
        setorigin(fl, p);
        float grow = min(1, 0.3 + 0.7 * (time - fl.burn_born) / VR_BURN_GROW);
        fl.scale = fl.burn_size * min(grow, fade);
        fl = fl.burn_next;
    }

    if(time >= self.burn_end)
    {
        return; // (shrinking away: no more spreading or damage)
    }
    if(wood && time >= self.burn_catch_next)
    {
        self.burn_catch_next = time + 0.5;
        VR_Burn_CrateCatch(self);
    }
    if(time >= self.burn_spread_next)
    {
        self.burn_spread_next = time + max(0.05, cvar_hget(cvarh_vr_burn_spread));
        float most = cvar_hget(crate ? cvarh_vr_burn_crate_flames : (piece ? cvarh_vr_burn_piece_flames : cvarh_vr_burn_flames));
        if(self.burn_count < min(most, cvar_hget(cvarh_vr_burn_flames_max)))
        {
            VR_Burn_Spread(self);
        }
    }
    if(time < self.burn_tick)
    {
        return;
    }
    self.burn_tick = time + VR_BURN_TICK;
    float dmg = cvar_hget(cvarh_vr_burn_damage) * VR_BURN_TICK;
    if(dead)
    {
        dmg = dmg * cvar_hget(cvarh_vr_burn_corpse_damage);
    }
    if(wood)
    {
        dmg = 0; // (it burns through: it breaks or burns away at its time's end)
    }
    fl = self.burn_flames;
    while(fl != world)
    {
        particle2(fl.origin, '0 0 40', QVR_PARTICLE_PRESET_LAVASPIKE, 2); // embers
        vector jit;
        jit_x = crandom() * 3; // (the burns spread round it, tick by tick)
        jit_y = crandom() * 3;
        jit_z = crandom() * 3;
        vr_burn_scorch_out = fl.burn_out;
        VR_Burn_Scorch(v, fl.origin + jit, min(60, 16 + 4 * (time - fl.burn_born))); // (wider as it burns on)
        vr_burn_scorch_out = '0 0 0';
        fl = fl.burn_next;
    }
    particle2(self.burn_flames.origin, '0 0 20', QVR_PARTICLE_PRESET_SMOKE, 2);
    if(dmg > 0 && v.takedamage)
    {
        dprint(sprintf("burning: %s burns for %g (%g flames, health %g)\n", v.classname, dmg, self.burn_count, v.health));
        VR_SetHitKind(QVR_HITKIND_NONE, -1, self.burn_flames.origin, '0 0 -1');
        T_Damage(v, self, self.owner, dmg); // (vr_debug_shots logs it: "by vr_burn")
        VR_ClearHitKind();
    }
};

// Sets `xVictim` on fire (or keeps it burning: its time from now again, a flame more), lit by `xAttacker` at `xAt`
// going `xDir`, by xHow (VR_BURN_BLOW, VR_BURN_TOUCH, VR_BURN_NAIL). Its damage is one fire's however often it is lit.
void(entity xVictim, entity xAttacker, vector xAt, vector xDir, float xHow) VR_Burn_Ignite =
{
    if(!VR_Burn_Can(xVictim) || xVictim == xAttacker)
    {
        return;
    }
    float crate = VR_Crate_Is(xVictim);
    float wood = VR_Burn_Wood(xVictim);
    float dead = xVictim.health <= 0;
    float len = cvar_hget(dead ? cvarh_vr_burn_corpse_time : cvarh_vr_burn_time);
    if(wood)
    {
        len = cvar_hget(crate ? cvarh_vr_burn_crate_time : cvarh_vr_burn_piece_time);
    }
    if(len <= 0 || (!dead && !wood && cvar_hget(cvarh_vr_burn_damage) <= 0))
    {
        return;
    }
    if(xDir == '0 0 0')
    {
        xDir = '0 0 -1';
    }
    if(xHow == VR_BURN_BLOW && !dead)
    {
        woundevent(xVictim, xAt, normalize(xDir), QVR_WOUND_BURN, 14, wood); // scorched where the head struck (wood: no wound)
    }
    entity f = xVictim.burn_fire;
    float fresh = f == world || f.classname != "vr_burn" || f.enemy != xVictim;
    if(fresh)
    {
        f = spawn();
        f.classname = "vr_burn";
        f.enemy = xVictim;
        f.movetype = MOVETYPE_NONE;
        f.solid = SOLID_NOT;
        f.burn_model = xVictim.modelindex;
        f.burn_dead = dead;
        f.burn_tick = time + VR_BURN_TICK;
        f.burn_spread_next = time + max(0.05, cvar_hget(cvarh_vr_burn_spread));
        f.burn_catch_next = time + cvar_hget(cvarh_vr_burn_crate_spread);
        f.think = VR_Burn_Think;
        f.nextthink = time;
        setorigin(f, xVictim.origin);
        xVictim.burn_fire = f;
        f.burn_char_at = time + len * 0.5;
        if(xHow != VR_BURN_SCATTER)
        {
            sound(xVictim, CHAN_AUTO, "vr/torch_light.wav", 0.6, ATTN_NORM);
        }
        VR_Burn_AddFlame(f, xAt, TRUE);
        xVictim.burn_add_next = time + VR_BURN_ADD_GAP;
    }
    else if(time >= xVictim.burn_add_next && f.burn_count < cvar_hget(cvarh_vr_burn_flames_max))
    {
        xAt_x = xAt_x + crandom() * 5; // (hits on one spot: flames side by side)
        xAt_y = xAt_y + crandom() * 5;
        VR_Burn_AddFlame(f, xAt, FALSE);
        xVictim.burn_add_next = time + VR_BURN_ADD_GAP;
    }
    f.owner = xAttacker;
    xVictim.burn_lit = time;
    if(!wood || fresh)
    {
        f.burn_end = max(f.burn_end, time + len); // (never longer than one lighting's time from the last; wood's: its first)
    }
    string how = "a missile";
    if(xHow == VR_BURN_BLOW)
    {
        how = "a blow";
    }
    else if(xHow == VR_BURN_TOUCH)
    {
        how = "a touch";
    }
    else if(xHow == VR_BURN_SPREAD)
    {
        how = "burning wood";
    }
    else if(xHow == VR_BURN_SCATTER)
    {
        how = "its crate broke burning";
    }
    dprint(sprintf("burning: %s lit (%s) by %s: burns till %.1f, %g flames\n", xVictim.classname, how,
                   xAttacker.classname, f.burn_end, f.burn_count));
};

// Whether piece `xPiece` was set on fire this frame (VR_CratePiece_Damage: a lava nail, a thrown lit torch, a torch's
// blow set it on fire just before their damage; it burns, it doesn't burst).
float(entity xPiece) VR_Burn_JustLit =
{
    return xPiece.burn_lit == time && xPiece.burn_fire != world;
};

// A piece just made of crate `xCrate` (VR_Crate_Pieces; NOTES.md vrfiringrange_2026-10-03_02-04-39): the crate burnt
// through (VR_Burn_Think), it is charred (its charred skin; it burns no more: vr_burn_crate_char); broken while burning
// (a blow, a shot), it burns on (vr_burn_pieces). Broken any other way, it is plain wood: it catches fire as a crate does.
void(entity xCrate, entity xPiece) VR_Burn_NewPiece =
{
    if(xCrate.vr_burnt_through && cvar_hget(cvarh_vr_burn_crate_char))
    {
        xPiece.skin = VR_PIECE_SKIN_CHARRED;
        xPiece.vr_charred = TRUE;
        return;
    }
    entity f = xCrate.burn_fire;
    if(f != world && f.classname == "vr_burn" && f.enemy == xCrate)
    {
        VR_Burn_Ignite(xPiece, f.owner, (xPiece.absmin + xPiece.absmax) * 0.5 + '0 0 4', '0 0 -1', VR_BURN_SCATTER);
    }
};

// A piece struck by a hand at a swing's speed (VR_CratePiece_Struck): a lit torch in it sets the piece on fire instead
// of bursting it (TRUE). A charred one bursts.
float(entity xPlayer, float xHand, entity xPiece) VR_Burn_PieceStruck =
{
    entity t = VR_Carry_Held(xPlayer, xHand);
    if(t == world || !VR_WallTorch_IsTorch(t) || t.wt_state != 1 || t.wt_fire <= 0 || !VR_Burn_Can(xPiece))
    {
        return FALSE;
    }
    VR_Burn_Ignite(xPiece, xPlayer, (xPiece.absmin + xPiece.absmax) * 0.5, VRGetEntHandVel(xPlayer, xHand), VR_BURN_BLOW);
    return xPiece.burn_fire != world;
};

// A lava nail struck `other` (lavaspike_touch, superlavaspike_touch; self the nail): yours set it on fire.
void() VR_Burn_LavaNail =
{
    if(cvar_hget(cvarh_vr_burn_lava_nails) && (self.owner.flags & FL_CLIENT))
    {
        VR_Burn_Ignite(other, self.owner, self.origin, self.velocity, VR_BURN_NAIL);
    }
};

// Whether a torch's head at `xHead` touches `e`: its model as drawn (a ball of 8 round the head), else its box.
float(entity e, vector xHead) VR_Burn_Touches =
{
    float r = 8;
    if(xHead_x < e.absmin_x - r || xHead_x > e.absmax_x + r || xHead_y < e.absmin_y - r || xHead_y > e.absmax_y + r ||
       xHead_z < e.absmin_z - r || xHead_z > e.absmax_z + r)
    {
        return FALSE;
    }
    if(VR_Burn_Wood(e))
    {
        // A crate's box, turned with it (VR_Burn_CrateCast).
        vector l = VR_Burn_ToLocal(e, xHead);
        vector lo = modelbounds(e, 0) - '1 1 1' * r;
        vector hi = modelbounds(e, 1) + '1 1 1' * r;
        return l_x >= lo_x && l_x <= hi_x && l_y >= lo_y && l_y <= hi_y && l_z >= lo_z && l_z <= hi_z;
    }
    float f = hitmodel_any(e, xHead, xHead + '0 0 1', r);
    return f >= 0 || f == -2;
};

// A lit torch, held or thrown or lying, every 0.1 s (VR_WallTorch_Think): with vr_burn_touch, what its head touches
// catches fire. xBy: who holds it or threw it.
void(entity t, vector xHead, entity xBy) VR_Burn_TorchTouch =
{
    if(!cvar_hget(cvarh_vr_burn_touch))
    {
        return;
    }
    entity e = findradius(xHead, 96);
    while(e)
    {
        if(e != xBy && e != t && e.burn_add_next <= time && VR_Burn_Can(e) && VR_Burn_Touches(e, xHead))
        {
            VR_Burn_Ignite(e, xBy == world ? t : xBy, xHead, t.velocity, VR_BURN_TOUCH);
        }
        e = e.chain;
    }
};

// ----------------------------------------------------------------------------
// Your own torch's flame on you (vr_burn_self, vr_burn_drop; the engine tests it against the body's collision shapes:
// torchflametouch). Kept on your other hand, an arm or your body for vr_burn_self_time, you catch fire there (as a
// monster does: flames on that part going with it, burns painted, vr_burn_damage); till then the hand buzzes, harder as
// it comes. Held upside down (it burns along the stick), its flame kept on the hand holding it for vr_burn_drop_time
// burns it: the torch is dropped.

.float wt_burn_on; // s its flame has stayed on you (vr_burn_self); off you, it eases back twice as fast
.float wt_drop_on; // s its flame has stayed on the hand holding it upside down (vr_burn_drop)

float VR_BURN_ON_OTHER = 10; // torchflametouch's bits: the other hand or arm,
float VR_BURN_ON_HOLD = 5;   // the holding hand or arm,
float VR_BURN_ON_YOU = 62;   // anything of you but the holding hand

// Every 0.1 s while lit torch `t` is held (VR_WallTorch_Think), `dt` since the last.
void(entity t, float dt) VR_Burn_OnYou =
{
    entity p = t.carry_player;
    float hand = -1;
    if(p != world && t.wt_fire > 0 && p.health > 0)
    {
        if(VR_Carry_Held(p, cVR_MainHand) == t)
        {
            hand = cVR_MainHand;
        }
        else if(VR_Carry_Held(p, cVR_OffHand) == t)
        {
            hand = cVR_OffHand;
        }
    }
    float selfOn = cvar_hget(cvarh_vr_burn_self);
    float dropOn = cvar_hget(cvarh_vr_burn_drop);
    if(hand < 0 || (!selfOn && !dropOn))
    {
        t.wt_burn_on = 0;
        t.wt_drop_on = 0;
        return;
    }
    float got = torchflametouch(t);
    float deep = floor(got / 256); // (the deepest contact's bit, but the holding hand's)
    float parts = got - deep * 256;
    vector at = trace_endpos;
    vector out = trace_plane_normal;
    float other = hand == cVR_MainHand ? cVR_OffHand : cVR_MainHand;
    float onYou = selfOn && (parts & VR_BURN_ON_YOU);
    vector headAt = VR_WallTorch_Head(t);
    vector handAt = VRGetEntHandPos(p, hand);
    float upsideDown = headAt_z < handAt_z - 2;
    float onHand = dropOn && (parts & 1) && upsideDown;
    t.wt_burn_on = onYou ? t.wt_burn_on + dt : max(0, t.wt_burn_on - 2 * dt);
    t.wt_drop_on = onHand ? t.wt_drop_on + dt : max(0, t.wt_drop_on - 2 * dt);
    float buzz = max(0, cvar_hget(cvarh_vr_burn_self_haptic));
    entity oldSelf = self;
    self = p;
    if(onYou)
    {
        float need = max(0, cvar_hget(cvarh_vr_burn_self_time));
        float side = (deep & VR_BURN_ON_OTHER) ? other : hand;
        if(t.wt_burn_on >= need)
        {
            vr_burn_part = (deep & (VR_BURN_ON_OTHER | VR_BURN_ON_HOLD)) ? side + 1 : 0;
            vr_burn_out = out == '0 0 0' ? '0 0 1' : out;
            dprint(sprintf("burning: your torch's flame on you (%g, deepest %g) at %v for %.2f s: you catch fire\n", parts,
                           deep, at, t.wt_burn_on));
            VR_Burn_Ignite(p, t, at, vr_burn_out * -1, VR_BURN_TOUCH);
            vr_burn_part = 0;
            vr_burn_out = '0 0 0';
            t.wt_burn_on = need; // (kept on you, it keeps you burning)
        }
        else if(buzz > 0 && p.ishuman)
        {
            // The warning: the hand nearest it buzzing, harder as it comes (both for the body).
            float k = need > 0 ? t.wt_burn_on / need : 1;
            float amp = min(1, (0.2 + 0.6 * k) * buzz);
            dprint(sprintf("burning: your torch's flame on you (%g): warning %.2f\n", parts, k));
            haptic(side, 0, 0.1, 90 + 160 * k, amp);
            if(deep & 48)
            {
                haptic(other, 0, 0.1, 90 + 160 * k, amp);
            }
        }
    }
    if(onHand)
    {
        float needD = max(0, cvar_hget(cvarh_vr_burn_drop_time));
        if(t.wt_drop_on >= needD)
        {
            vector hp = VRGetEntHandPos(p, hand);
            dprint(sprintf("burning: the torch held upside down burnt its hand (%g) for %.2f s: dropped\n", hand, t.wt_drop_on));
            woundevent(p, hp - '0 0 6', '0 0 1', QVR_WOUND_BURN, 30, 1); // a burn on the hand, from below
            sound(p, CHAN_VOICE, random() < 0.5 ? "player/lburn1.wav" : "player/lburn2.wav", 0.7, ATTN_NORM);
            if(p.ishuman)
            {
                haptic(hand, 0, 0.25, 220, 1);
            }
            t.wt_drop_on = 0;
            t.wt_burn_on = 0;
            VR_Carry_Release(hand, t);
        }
        else if(buzz > 0 && p.ishuman)
        {
            float kd = needD > 0 ? t.wt_drop_on / needD : 1;
            dprint(sprintf("burning: the torch held upside down on its hand: warning %.2f\n", kd));
            haptic(hand, 0, 0.1, 90 + 160 * kd, min(1, (0.2 + 0.6 * kd) * buzz));
        }
    }
    self = oldSelf;
};

// ----------------------------------------------------------------------------
// Nails through a flame (vr_burn_nail_convert): a nail (the nailgun's, the super nailgun's) passing within
// vr_burn_nail_reach units of a lit torch's flame (held, thrown, lying or on its wall) becomes a lava nail: a lava nail's
// damage (Weapon Damage), its glow and trail, and it sets what it hits on fire (vr_burn_lava_nails).

.entity burn_tnext; // the lit torches, this frame (VR_Burn_NailFrame)

void(entity n, vector xFlame, entity xTorch) VR_Burn_NailToLava =
{
    // A very short fizz from the torch's flame (vr_burn_nail_sound: its volume, 0 none).
    float vol = cvar_hget(cvarh_vr_burn_nail_sound);
    if(vol > 0)
    {
        sound(xTorch, CHAN_AUTO, random() < 0.5 ? "vr/nail_sizzle1.wav" : "vr/nail_sizzle2.wav", min(1, vol), ATTN_IDLE);
    }
    n.classname = "lava_spike";
    n.touch = n.touch == superspike_touch ? superlavaspike_touch : lavaspike_touch;
    setmodel(n, "progs/lspike.mdl");
    setsize(n, VEC_ORIGIN, VEC_ORIGIN);
    n.effects = n.effects | EF_VERYDIMLIGHT | EF_LAVATRAIL;
    particle2(xFlame, n.velocity * 0.02, QVR_PARTICLE_PRESET_LAVASPIKE, 6);
    dprint(sprintf("burning: a nail through a flame at %v: a lava nail (%s)\n", xFlame,
                   n.touch == superlavaspike_touch ? "super" : "normal"));
};

// The lit torches (held, thrown, lying, on their walls), chained through burn_tnext.
entity() VR_Burn_LitTorches =
{
    entity torches = world;
    entity t = find(world, classname, "light_torch_small_walltorch");
    while(t)
    {
        if(t.modelindex && (t.wt_state == 0 || t.wt_fire > 0))
        {
            t.burn_tnext = torches;
            torches = t;
        }
        t = find(t, classname, "light_torch_small_walltorch");
    }
    return torches;
};

// Nail `n` going a..b: through a torch's flame (within vr_burn_nail_reach), a lava nail.
void(entity n, entity xTorches, vector a, vector b) VR_Burn_NailCheck =
{
    float reach = max(0, cvar_hget(cvarh_vr_burn_nail_reach));
    entity t = xTorches;
    while(t)
    {
        vector fl = VR_WallTorch_Head(t) + '0 0 8';
        if(VR_WallTorch_SegmentDist(fl, a, b) <= reach)
        {
            VR_Burn_NailToLava(n, fl, t);
            return;
        }
        t = t.burn_tnext;
    }
};

// A nail just fired (W_FireSpikes, W_FireSuperSpikes): its first two frames' way, before any frame's check (a torch
// held just past the muzzle, a monster just past it, hit in the frame it is fired).
void(entity n) VR_Burn_NailLaunched =
{
    if(!cvar_hget(cvarh_vr_burn_nail_convert))
    {
        return;
    }
    entity torches = VR_Burn_LitTorches();
    if(torches != world)
    {
        VR_Burn_NailCheck(n, torches, n.origin, n.origin + n.velocity * frametime * 2);
    }
};

// Every server frame (StartFrame): the nails in flight that pass a lit torch's flame this frame (from where they were
// to where they will be: one hitting something just past it is converted before it does).
void() VR_Burn_NailFrame =
{
    if(!cvar_hget(cvarh_vr_burn_nail_convert))
    {
        return;
    }
    entity n = find(world, classname, "spike");
    if(n == world)
    {
        return;
    }
    entity torches = VR_Burn_LitTorches();
    if(torches == world)
    {
        return;
    }
    while(n)
    {
        entity next = find(n, classname, "spike");
        VR_Burn_NailCheck(n, torches, n.origin - n.velocity * frametime, n.origin + n.velocity * frametime);
        n = next;
    }
};

// ----------------------------------------------------------------------------
// Tests (vr_burn_test, Debug > Burning Tests): set on fire the monster, corpse, crate, crate's piece or explosive box
// nearest the player (an explosive box never catches: "can't burn"), as a torch's blow (1), a lava nail (2) or a touch
// (3) would, at the point on it facing the player; 4: how it burns (the console); 5: lava nails loaded in the main
// hand's nailgun; 6: a nail shot through the flame of the lit torch nearest the player (from 48 units before it, along
// the player's line to it); 7: where that flame is (the console); 8: the nearest crate smashed (as by your blow: plain
// pieces); 9: the nearest crate burnt through now (lit if it isn't: charred pieces); 10: the pieces lying about (the
// console: how many, charred, burning); 11: a lava nail of yours at the nearest piece (from above it).

// Test 10: the pieces lying about.
void() VR_Burn_TestPieces =
{
    float n = 0;
    float charred = 0;
    float burning = 0;
    float near = 0; // pieces with another piece or a crate within Pieces Catch Within (their boxes)
    float gap = max(0, cvar_hget(cvarh_vr_burn_piece_gap));
    entity e = find(world, classname, "vr_crate_piece");
    while(e)
    {
        if(VR_CratePiece_Is(e))
        {
            n = n + 1;
            charred = charred + (e.vr_charred ? 1 : 0);
            burning = burning + (e.burn_fire != world ? 1 : 0);
            entity o = findradius((e.absmin + e.absmax) * 0.5, 128);
            while(o)
            {
                if(o != e && VR_Burn_Wood(o) && o.absmin_x <= e.absmax_x + gap && o.absmax_x >= e.absmin_x - gap &&
                   o.absmin_y <= e.absmax_y + gap && o.absmax_y >= e.absmin_y - gap &&
                   o.absmin_z <= e.absmax_z + gap && o.absmax_z >= e.absmin_z - gap)
                {
                    near = near + 1;
                    break;
                }
                o = o.chain;
            }
        }
        e = find(e, classname, "vr_crate_piece");
    }
    dprint(sprintf("burning: test 10: %g pieces, %g charred, %g burning, %g with wood within %g\n", n, charred, burning,
                   near, gap));
};

// Tests 8, 9: the crate nearest `p` smashed by its blow (8), or burnt through now (9).
void(entity p, float xAsk) VR_Burn_TestCrate =
{
    entity best = world;
    float bestd = 100000;
    entity e = findradius(p.origin, 1024);
    while(e)
    {
        if(VR_Crate_Is(e) && e.takedamage && vlen(e.origin - p.origin) < bestd)
        {
            best = e;
            bestd = vlen(e.origin - p.origin);
        }
        e = e.chain;
    }
    if(best == world)
    {
        dprint("burning: test: no crate near\n");
        return;
    }
    if(xAsk == 8)
    {
        dprint(sprintf("burning: test 8: %s smashed\n", best.classname));
        T_Damage(best, p, p, best.health + 1000);
        return;
    }
    if(best.burn_fire == world)
    {
        vector mid = (best.absmin + best.absmax) * 0.5;
        VR_Burn_Ignite(best, p, VR_Burn_OnBody(best, mid + normalize(p.origin - mid) * 64), mid - p.origin, VR_BURN_BLOW);
    }
    if(best.burn_fire != world)
    {
        best.burn_fire.burn_end = time; // (burnt through at its next think: VR_Burn_Think)
        dprint(sprintf("burning: test 9: %s burns through now\n", best.classname));
    }
};

void() VR_Burn_TestFrame =
{
    float ask = cvar("vr_burn_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_burn_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    if(ask == 5)
    {
        // The main hand's nailgun or super nailgun loaded with lava nails (no reload needed).
        float wid = VRGetEntWeapon(p, cVR_MainHand);
        if(wid != WID_NAILGUN && wid != WID_SUPER_NAILGUN)
        {
            dprint("burning: test 5: no nailgun in the main hand\n");
            return;
        }
        p.ammo_lava_nails = max(p.ammo_lava_nails, 100);
        VRSetEntWeaponFlags(p, cVR_MainHand, VRGetEntWeaponFlags(p, cVR_MainHand) | QVR_WPNFLAG_USE_SECONDARY_AMMO);
        VRSetEntWeaponClip(p, cVR_MainHand, WeaponIdToBaseClipSize(wid));
        entity oself = self;
        self = p;
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = oself;
        dprint("burning: test 5: lava nails loaded in the main hand\n");
        return;
    }
    if(ask == 6 || ask == 7)
    {
        entity bt = world;
        float btd = 100000;
        entity tt = find(world, classname, "light_torch_small_walltorch");
        while(tt)
        {
            if(tt.modelindex && (tt.wt_state == 0 || tt.wt_fire > 0) && vlen(VR_WallTorch_Head(tt) - p.origin) < btd)
            {
                bt = tt;
                btd = vlen(VR_WallTorch_Head(tt) - p.origin);
            }
            tt = find(tt, classname, "light_torch_small_walltorch");
        }
        if(bt == world)
        {
            dprint("burning: test 6: no lit torch\n");
            return;
        }
        vector flame = VR_WallTorch_Head(bt) + '0 0 8';
        if(ask == 7)
        {
            dprint(sprintf("burning: test 7: the nearest lit torch's flame at %v (%s)\n", flame,
                           bt.carry_player != world ? "held" : (bt.wt_state == 0 ? "on its wall" : "lying")));
            return;
        }
        vector way = normalize(flame - (p.origin + '0 0 16'));
        entity os = self;
        self = p;
        launch_spike(flame - way * 48, way);
        self = os;
        dprint(sprintf("burning: test 6: a nail at the torch's flame at %v\n", flame));
        return;
    }
    if(ask == 8 || ask == 9)
    {
        VR_Burn_TestCrate(p, ask);
        return;
    }
    if(ask == 10)
    {
        VR_Burn_TestPieces();
        return;
    }
    if(ask == 11)
    {
        // A lava nail of yours at the nearest piece, from 40 units above it and towards you.
        entity np = world;
        float npd = 100000;
        entity q = find(world, classname, "vr_crate_piece");
        while(q)
        {
            if(VR_CratePiece_Is(q) && vlen(q.origin - p.origin) < npd)
            {
                np = q;
                npd = vlen(q.origin - p.origin);
            }
            q = find(q, classname, "vr_crate_piece");
        }
        if(np == world)
        {
            dprint("burning: test 11: no piece near\n");
            return;
        }
        vector pm = (np.absmin + np.absmax) * 0.5;
        vector from = pm + '0 0 40' + normalize(p.origin - pm) * 20;
        entity os11 = self;
        self = p;
        entity nl = launch_spike(from, normalize(pm - from));
        self = os11;
        nl.classname = "lava_spike";
        nl.touch = lavaspike_touch;
        setmodel(nl, "progs/lspike.mdl");
        setsize(nl, VEC_ORIGIN, VEC_ORIGIN);
        dprint(sprintf("burning: test 11: a lava nail at the piece at %v (charred %g)\n", pm, np.vr_charred));
        return;
    }
    entity best = world;
    float bestd = 100000;
    entity e = findradius(p.origin, 1024);
    while(e)
    {
        float cand = (e.flags & FL_MONSTER) || VR_Burn_Wood(e) || VR_Burn_Metal(e);
        vector emid = (e.absmin + e.absmax) * 0.5; // (an explosive box's origin is its corner)
        if(e != p && cand && e.modelindex && vlen(emid - p.origin) < bestd)
        {
            best = e;
            bestd = vlen(emid - p.origin);
        }
        e = e.chain;
    }
    if(best == world)
    {
        dprint("burning: test: no monster or corpse near\n");
        return;
    }
    if(ask == 4)
    {
        entity f = best.burn_fire;
        string how = "not burning";
        if(f != world)
        {
            how = sprintf("%g flames, %.1f s left", f.burn_count, f.burn_end - time);
        }
        dprint(sprintf("burning: %s at %v, health %g, corpse %g, skin %g, charred %g: %s\n", best.classname, best.origin,
                       best.health, best.vr_corpse, best.skin, best.vr_charred, how));
        return;
    }
    if(!VR_Burn_Can(best))
    {
        dprint(sprintf("burning: test %g: %s can't burn (health %g)\n", ask,
                       VR_Burn_Metal(best) ? "an explosive box (metal)" : best.classname, best.health));
        return;
    }
    vector mid = (best.absmin + best.absmax) * 0.5;
    vector at = mid + normalize(p.origin - mid) * 64;
    at_z = mid_z + (best.health > 0 ? (best.absmax_z - mid_z) * 0.3 : 0);
    at = VR_Burn_OnBody(best, at);
    dprint(sprintf("burning: test %g on %s (health %g, corpse %g)\n", ask, best.classname, best.health, best.vr_corpse));
    VR_Burn_Ignite(best, p, at, mid - p.origin, ask == 2 ? VR_BURN_NAIL : (ask == 3 ? VR_BURN_TOUCH : VR_BURN_BLOW));
};

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
.float burn_smoke_out;    // a body's fire: when the clients were last told its flames go out (VR_Burn_Smoke; 0 not told)
.float burn_crackle;      // the volume its crackle plays at (VR_Burn_Crackle; 0 silent)
.float burn_crackle_next; // when its crackle is next looked at
// On a flame (owner: its fire):
.vector burn_off;         // where on the body: from its origin, in its yaw's axes
.float burn_size;         // its full scale
.float burn_born;         // when it came (it grows in)
.float burn_part;         // a player's: 0 on the body (burn_off in its yaw's axes); a hand + 1: on that hand or its arm
.vector burn_anchor;      // where on its body's model's surface (modelanchor: it goes with the model as drawn),
.float burn_anchor_model; // ... while the body has this model (0: none)
.float burn_bone;         // a ragdoll's: its limb + 1 (burn_off in the limb's space: it goes with it); 0 none
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
float VR_BURN_LAVA = 5;      // wood in lava (VR_Burn_LavaFrame)

// What fire doesn't hurt (VR_Liquid_Immune's: what lives in lava or water, the bosses), but the training dummy burns.
float(entity e) VR_Burn_Fireproof =
{
    return !VR_IsDummy(e) && VR_Liquid_Immune(e);
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
    // (A dead monster's ragdoll still dying, not yet armed as a corpse, too: a body the map's flames or a lying flare
    // meet, VR_Burn_MapBodyLook, burns as a corpse does.)
    return cvar_hget(cvarh_vr_burn_corpses) && (e.flags & FL_MONSTER) && (e.vr_corpse || e.vr_corpse_dying || isragdoll(e));
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

// A flame's place on its body, as it is now: on its model's surface where it was put (its anchor there: it moves with
// the model's animation, a ragdoll's parts), else a ragdoll's on its limb (vr_ragdoll: it goes with the limb).
vector(entity v, entity fl) VR_Burn_FlameAt =
{
    if((v.flags & FL_CLIENT) && fl.burn_part > 0)
    {
        return VR_Burn_FromHand(v, fl.burn_part, fl.burn_off);
    }
    if(fl.burn_anchor_model && fl.burn_anchor_model == v.modelindex)
    {
        vector at = modelanchorpoint(v, fl.burn_anchor);
        if(trace_fraction > 0)
        {
            return at;
        }
        fl.burn_anchor_model = 0; // (no longer there: by its limb or origin, then fitted again)
    }
    if(fl.burn_bone > 0)
    {
        if(isragdoll(v))
        {
            return ragdollpoint(v, fl.burn_bone - 1, fl.burn_off, 1);
        }
        fl.burn_bone = 0; // (no longer one: from its origin again, where it is)
        fl.burn_off = VR_Burn_ToLocal(v, fl.origin);
    }
    return VR_Burn_ToWorld(v, fl.burn_off);
};

// A flame put at `p` on its body: on its model's surface (modelanchor: not a player's Quake model, which isn't the body
// drawn in the headset; not a crate's); and on a ragdoll, on its limb nearest (in the limb's space); else from its origin.
void(entity v, entity fl, vector p) VR_Burn_Anchor =
{
    fl.burn_anchor_model = 0;
    if(!(v.flags & FL_CLIENT) && !VR_Burn_Wood(v))
    {
        vector an = modelanchor(v, p);
        if(an_x >= 0)
        {
            fl.burn_anchor = an;
            fl.burn_anchor_model = v.modelindex;
        }
    }
    float bone = ragdollbone(v, p);
    if(bone >= 0)
    {
        fl.burn_bone = bone + 1;
        fl.burn_off = ragdollpoint(v, bone, p, 0);
        return;
    }
    fl.burn_bone = 0;
    fl.burn_off = VR_Burn_ToLocal(v, p);
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

// A point onto the surface of the body's model as drawn (modelanchor: its triangles, a ragdoll's as its parts carry it),
// so that a flame sits on it, not off it (a fatter line's hit a few units out, a box's side well in front of a body:
// the author's note of 2026-10-22, "floating in front"). A crate's (a box) and a model the engine can't tell: `q` itself.
vector(entity v, vector q) VR_Burn_Surface =
{
    if(VR_Burn_Wood(v))
    {
        return q;
    }
    vector an = modelanchor(v, q);
    return an_x < 0 ? q : trace_endpos;
};

// The point on the body's model as drawn nearest `p` as a flame would sit on it: standing, met from outside level with
// it, towards its middle; lying (dead), and a crate's top half, met from above. Missed (between its legs, past a lying body's end), tried again
// nearer its drawn middle, a fatter line. Either then put onto the surface (VR_Burn_Surface). Not met at all: the
// surface's point nearest `p` kept in the box; a model the engine doesn't test: `p` kept in the box.
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
            return VR_Burn_Surface(v, a + (b - a) * f);
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
    if(f != -2 && !VR_Burn_Wood(v))
    {
        // Missed by every line (a thin model, a box much bigger than it): its surface's point nearest, never the box's.
        vector an = modelanchor(v, p);
        if(an_x >= 0)
        {
            vr_burn_onbody = TRUE;
            return trace_endpos;
        }
    }
    return p;
};

// A player's body as its shapes are (the tracked body, as it is drawn: vr_selfcollide.cpp, mapflametouch; not its Quake
// model): its point nearest `q` (within 48 units) vr_burn_yat, its part vr_burn_ypart (0 the torso, legs or head; a hand
// + 1: that hand or its arm) and the way out of it there vr_burn_yout. FALSE: no shapes (not the player in the headset,
// or not built this frame: asked for, for the next; VR_Burn_MapFrame keeps them while the player burns).
float vr_burn_ypart;
vector vr_burn_yat;
vector vr_burn_yout;
float(entity p, vector q) VR_Burn_OnYourBody =
{
    if(!(p.flags & FL_CLIENT) || !p.ishuman || p.health <= 0)
    {
        return FALSE;
    }
    float got = mapflametouch(p, q, q, 48, 63);
    if(!got)
    {
        return FALSE;
    }
    float deep = floor(got / 256);
    float side = (deep & 5) ? cVR_OffHand : cVR_MainHand;
    vr_burn_ypart = (deep & 15) ? side + 1 : 0;
    vr_burn_yat = trace_endpos;
    vr_burn_yout = trace_plane_normal == '0 0 0' ? '0 0 1' : trace_plane_normal;
    return TRUE;
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
    float part = vr_burn_part;
    vector out = vr_burn_out;
    vector p = at;
    vr_burn_onbody = TRUE;
    if(VR_Burn_OnYourBody(v, at))
    {
        // A player: onto the body's shapes as they are drawn (where they touched, or its point nearest the hit).
        you = TRUE;
        p = vr_burn_yat;
        part = vr_burn_ypart;
        out = vr_burn_yout;
    }
    else if(!you)
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
    fl.burn_part = you ? part : 0;
    fl.burn_out = you ? out : '0 0 0';
    if(fl.burn_part > 0)
    {
        fl.burn_bone = 0;
        fl.burn_off = VR_Burn_ToHand(v, fl.burn_part, p);
    }
    else
    {
        VR_Burn_Anchor(v, fl, p);
    }
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
        // (off the curved body, a step along its tangent: AddFlame puts it back on the body's shapes, VR_Burn_OnYourBody)
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

// A monster's or a corpse's fire smokes off its body while it burns and a while after its flames go out (the engine's
// vr_smoulder.cpp, client-side: vr_smoulder*): the clients are told when they go out (bodysmoulder), when it is lit and
// again whenever burning on puts that half a second later; `xLeft` 0: just out; below 0: put out in a liquid (no smoke).
void(entity xFire, float xLeft) VR_Burn_Smoke =
{
    entity v = xFire.enemy;
    if(v == world || VR_Burn_Wood(v) || (v.flags & FL_CLIENT) || !((v.flags & FL_MONSTER) || v.vr_corpse))
    {
        return;
    }
    bodysmoulder(v, xLeft);
    xFire.burn_smoke_out = xLeft > 0 ? time + xLeft : 0;
};

// The crackle of a burning body, crate or piece (AUDIO_REVIEW.md row 7): Quake's torch loop (ambience/fire1.wav, as the
// wall torches', VR_WallTorch_Crackle) on the body's own channel (CHAN_BURN: it follows the body), heard only near
// (ATTN_STATIC), at vr_burn_sound; quieter in the last 1.5 s as the flames die (0.6, then 0.3 in the last 0.75 s: the
// loop restarted lower), again at full if it is lit again, and stopped (misc/null.wav) as it goes out, however it does
// (VR_Burn_Out). It starts 0.3 s after the fire (the catching whoosh first). At most vr_burn_sound_max fires crackle at
// once (vr_burn_crackles; recounted when it says the most do): the others burn silent, each looking again every
// 0.25 s, so one starts as another goes out (not in its last 1.5 s). Not on you (you cry out instead: VR_Burn_OnYou).
// `developer` prints each.
float vr_burn_crackles; // fires crackling now

float() VR_Burn_CrackleCount =
{
    float n = 0;
    entity e = find(world, classname, "vr_burn");
    while(e)
    {
        if(e.burn_crackle > 0)
        {
            n = n + 1;
        }
        e = find(e, classname, "vr_burn");
    }
    vr_burn_crackles = n;
    return n;
};

void(entity xFire, float xVol) VR_Burn_CrackleSet =
{
    if(xVol == xFire.burn_crackle)
    {
        return;
    }
    entity v = xFire.enemy;
    if(xVol > 0)
    {
        sound(v, CHAN_BURN, "ambience/fire1.wav", xVol, ATTN_STATIC);
        if(xFire.burn_crackle <= 0)
        {
            vr_burn_crackles = vr_burn_crackles + 1;
        }
    }
    else
    {
        sound(v, CHAN_BURN, "misc/null.wav", 0.1, ATTN_STATIC);
        vr_burn_crackles = max(0, vr_burn_crackles - 1);
    }
    dprint(sprintf("burning: %s crackles at %g (%g crackling)\n", v.classname, xVol, vr_burn_crackles));
    xFire.burn_crackle = xVol;
};

// A loaded game (OnLoadGame, a moment after: SUB_DoLater): sounds are not saved, so the loops playing at the save are
// started again: a wall torch's or a flame prop's crackle (VR_WallTorch_Crackle) at once, a fire's (VR_Burn_Crackle) on
// its next look, as if silent.
void(entity f) VR_Flare_Hiss; // (vr_flaregun.qc: a burning flare's)
void() VR_Crackles_AfterLoad =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.classname == "vr_flare")
        {
            VR_Flare_Hiss(e); // (the flare gun's flares burning: their hiss again)
            n = n + 1;
        }
        if(e.wt_crackle)
        {
            e.wt_crackle = FALSE;
            VR_WallTorch_Crackle(e, TRUE);
            n = n + 1;
        }
        if(e.classname == "vr_burn" && e.burn_crackle > 0)
        {
            e.burn_crackle = 0;
            e.burn_crackle_next = 0;
            n = n + 1;
        }
        e = nextent(e);
    }
    vr_burn_crackles = 0;
    dprint(sprintf("crackles: %g started again after the load\n", n));
};

void(entity xFire) VR_Burn_Crackle =
{
    if(time < xFire.burn_crackle_next)
    {
        return;
    }
    xFire.burn_crackle_next = time + 0.25;
    entity v = xFire.enemy;
    float want = (v.flags & FL_CLIENT) ? 0 : min(1, max(0, cvar_hget(cvarh_vr_burn_sound)));
    float left = xFire.burn_end + VR_BURN_FADE - time;
    if(left < 0.75)
    {
        want = want * 0.3;
    }
    else if(left < 1.5)
    {
        want = want * 0.6;
    }
    if(want > 0 && xFire.burn_crackle <= 0)
    {
        if(left < 1.5)
        {
            return; // (its flames dying: it never starts now)
        }
        float most = cvar_hget(cvarh_vr_burn_sound_max);
        if(vr_burn_crackles >= most && VR_Burn_CrackleCount() >= most)
        {
            return; // (as many crackle as may: it burns silent for now)
        }
    }
    VR_Burn_CrackleSet(xFire, want);
};

// Out: its flames gone, the body no longer burning (it smokes on a while: VR_Burn_Smoke), its crackle stopped.
void(entity xFire, string xWhy) VR_Burn_Out =
{
    entity v = xFire.enemy;
    VR_Burn_CrackleSet(xFire, 0);
    if(xFire.burn_smoke_out)
    {
        VR_Burn_Smoke(xFire, 0);
    }
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
        self.burn_smoke_out = 0; // (no body left to smoke)
        VR_Burn_Out(self, "gone or gibbed");
        return;
    }
    // (In a liquid: out, but not in lava, which lights wood and bodies: VR_Burn_LavaFrame.)
    if(VR_Wound_Level(v) >= 2 && vr_wound_liquid != CONTENT_LAVA)
    {
        particle2((v.absmin + v.absmax) * 0.5, '0 0 30', QVR_PARTICLE_PRESET_BIGSMOKE, 6);
        if(self.burn_smoke_out)
        {
            VR_Burn_Smoke(self, -1); // (doused: it stops smoking)
        }
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
    VR_Burn_Crackle(self);
    if(self.burn_end + VR_BURN_FADE > self.burn_smoke_out + 0.5)
    {
        VR_Burn_Smoke(self, self.burn_end + VR_BURN_FADE - time); // (lit, or burning on longer)
    }

    // Fitted to the body (it moves, dies, falls): each flame onto the model again where it is (between, carried on the
    // model's surface by its anchor: VR_Burn_FlameAt); a player's onto the body's shapes every time (the tracked body
    // leans, crouches and walks off its origin; VR_Burn_OnYourBody), and without them (not in the headset) those put
    // there stay put. A crate's stay where they are on it (it is rigid: they turn with it).
    float client = v.flags & FL_CLIENT;
    float fit = !wood && (client || time >= self.burn_fit_next || dead != self.burn_dead);
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
        if(fit && client && VR_Burn_OnYourBody(v, p))
        {
            p = vr_burn_yat;
            fl.burn_part = vr_burn_ypart;
            fl.burn_out = vr_burn_yout;
            fl.burn_bone = 0;
            fl.burn_off = fl.burn_part > 0 ? VR_Burn_ToHand(v, fl.burn_part, p) : VR_Burn_ToLocal(v, p);
        }
        else if(fit && !(client && fl.burn_out != '0 0 0'))
        {
            vector q = VR_Burn_OnBody(v, p);
            if(vr_burn_onbody)
            {
                p = q;
                VR_Burn_Anchor(v, fl, p);
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
    if((v.flags & FL_CLIENT) && v.waterlevel > 0 && v.watertype == CONTENT_LAVA)
    {
        dmg = 0; // (a player in lava: the lava's own damage alone, client.qc's WaterMove; it burns on once out)
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
    if(AD_BurnHurt(self, v)) {} // AD support: AD's burning on you (its debuff, merged): AD's damage (vr_ad_player.qc)
    else if(dmg > 0 && v.takedamage)
    {
        dprint(sprintf("burning: %s burns for %g (%g flames, health %g)\n", v.classname, dmg, self.burn_count, v.health));
        VR_SetHitKind(QVR_HITKIND_NONE, -1, self.burn_flames.origin, '0 0 -1');
        vr_hit_nobleed = TRUE; // (fire burns it: its own scorches, no blood sprayed: combat.qc VR_HitBloodless)
        T_Damage(v, self, self.owner, dmg); // (vr_debug_shots logs it: "by vr_burn")
        vr_hit_nobleed = FALSE;
        VR_ClearHitKind();
    }
};

// Sets `xVictim` on fire (or keeps it burning: its time from now again, a flame more), lit by `xAttacker` at `xAt`
// going `xDir`, by xHow (VR_BURN_BLOW, VR_BURN_TOUCH, VR_BURN_NAIL). Its damage is one fire's however often it is lit.
// vr_burn_len, when over 0: its time instead of the settings' (a player out of lava: vr_burn_lava_player).
float vr_burn_len;
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
    if(vr_burn_len > 0)
    {
        len = vr_burn_len;
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
        f.burn_crackle_next = time + 0.3; // (the whoosh first)
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
    else if(xHow == VR_BURN_LAVA)
    {
        how = "lava";
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

// A lava nail struck `other` (lavaspike_touch, superlavaspike_touch; self the nail): yours set it on fire, and so do a
// monster's (an armed enemy's lava nailgun, a nail of its through a flame): you burn as they do.
void() VR_Burn_LavaNail =
{
    if(cvar_hget(cvarh_vr_burn_lava_nails) && (self.owner.flags & (FL_CLIENT | FL_MONSTER)))
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
// Mapper flames: the static flame registry and wall torches still on their walls. No extra edicts, and a torch
// taken off its wall stops being a fixture immediately. Hands/arms use the held torch warning and burn delay;
// torso/head/legs use dropped torch touch ignition. The same burn effects/damage continue after leaving the fire.
.float burn_map_next;
.float burn_map_last;
.float burn_map_on;
float vr_map_got;
vector vr_map_at;
vector vr_map_out;
entity vr_map_source;

// The upright flame capsule against the player's physical body box, including feet in avatar modes without legs.
// Prefer tracked body shapes when available; physical walking contact uses the dropped torch ignition path.
float(entity p, vector a, vector b, float r) VR_Burn_MapBody =
{
    vector fire = (a + b) * 0.5;
    vector at;
    at_x = max(p.absmin_x, min(fire_x, p.absmax_x));
    at_y = max(p.absmin_y, min(fire_y, p.absmax_y));
    at_z = max(p.absmin_z, min(fire_z, p.absmax_z));
    fire_z = max(a_z, min(at_z, b_z));
    if(vlen(fire - at) > r)
    {
        return 0;
    }
    trace_endpos = at;
    trace_plane_normal = normalize(fire - (p.absmin + p.absmax) * 0.5);
    return 16 + 16 * 256;
};

void(entity p, entity source, vector o, float kind) VR_Burn_MapContact =
{
    if(vlen(o - p.origin) > 160)
    {
        return;
    }
    vector a = o + '0 0 1';
    vector b = o + '0 0 16';
    float r = 2.2; // wall torch: same hot lower flame as its upright portable version
    if(kind != 2)
    {
        a = o;
        b = o + (kind == 1 ? '0 0 32' : '0 0 12');
        r = kind == 1 ? 6.5 : 3;
    }
    // Body touch takes priority, including when a hand is deeper in the same fire.
    float got = cvar_hget(cvarh_vr_burn_touch) ? mapflametouch(p, a, b, r, 48) : 0;
    if(!got && cvar_hget(cvarh_vr_burn_touch))
    {
        got = VR_Burn_MapBody(p, a, b, r);
    }
    if(!got && cvar_hget(cvarh_vr_burn_self))
    {
        got = mapflametouch(p, a, b, r, 15);
    }
    float deep = floor(got / 256);
    if(!got)
    {
        return;
    }
    if(!vr_map_got || (deep & 48))
    {
        vr_map_got = got;
        vr_map_at = trace_endpos;
        vr_map_out = trace_plane_normal;
        vr_map_source = source;
    }
};

void() VR_Burn_MapFrame =
{
    entity p;
    float anyFire = -1; // (any fire in the map: looked up once per frame, for the first player that needs it)
    for(p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        if(p.ishuman && p.health > 0 && p.burn_fire != world)
        {
            mapflametouch(p, '0 0 0', '0 0 0', -1, 63); // (burning: its flames are fitted to the body's shapes, VR_Burn_OnYourBody)
        }
        else if(p.ishuman && p.health > 0 && (cvar_hget(cvarh_vr_burn_self) || cvar_hget(cvarh_vr_burn_touch)))
        {
            if(anyFire < 0)
            {
                anyFire = (vr_fire_spots > 0 || find(world, classname, "light_torch_small_walltorch") != world ||
                           findflags(world, vr_flameprop, 3) != world);
            }
            if(anyFire)
            {
                mapflametouch(p, '0 0 0', '0 0 0', -1, 63); // keep the body's shapes through the 0.1 s polling gap
            }
        }
        if(!p.ishuman || time < p.burn_map_next)
        {
            continue;
        }
        float dt = p.burn_map_last > 0 ? min(0.5, max(0, time - p.burn_map_last)) : 0;
        p.burn_map_last = time;
        p.burn_map_next = time + 0.1;
        if(p.health <= 0 || (!cvar_hget(cvarh_vr_burn_self) && !cvar_hget(cvarh_vr_burn_touch)))
        {
            p.burn_map_on = 0;
            continue;
        }
        vr_map_got = 0;
        float i;
        for(i = 0; i < vr_fire_spots; i++)
        {
            vector o = vr_fire_spot[i];
            float kind = vr_fire_spot_large[i]; // cache both: the scalar array getter clobbers vector call arguments
            if(kind != 3) // (a candle: nails only)
            {
                VR_Burn_MapContact(p, world, o, kind);
            }
        }
        entity t;
        for(t = find(world, classname, "light_torch_small_walltorch"); t;
            t = find(t, classname, "light_torch_small_walltorch"))
        {
            if(t.wt_state == 0 && t.wt_fire > 0 && t.modelindex)
            {
                VR_Burn_MapContact(p, t, t.origin, 2);
            }
        }
        // The toolgun's flames where they rest (vr_walltorch.qc VR_FlameProp_Setup), as the map's of their size; carried
        // or thrown, a torch's head instead (VR_FlameProp_Think).
        for(t = findflags(world, vr_flameprop, 3); t; t = findflags(t, vr_flameprop, 3))
        {
            if(t.carry_player == world && VR_Throw_Thrower(t) == world)
            {
                VR_Burn_MapContact(p, t, t.origin, t.vr_flameprop == 2 ? 1 : 0);
            }
        }
        p.burn_map_on = vr_map_got ? p.burn_map_on + dt : max(0, p.burn_map_on - 2 * dt);
        if(!vr_map_got)
        {
            continue;
        }
        float deep = floor(vr_map_got / 256);
        float side = (deep & 5) ? cVR_OffHand : cVR_MainHand;
        float need = (deep & 48) ? 0 : max(0, cvar_hget(cvarh_vr_burn_self_time));
        entity oldSelf = self;
        self = p;
        if(p.burn_map_on >= need)
        {
            vr_burn_part = (deep & 15) ? side + 1 : 0;
            vr_burn_out = vr_map_out == '0 0 0' ? '0 0 1' : vr_map_out;
            if(p.burn_fire == world)
            {
                dprint(sprintf("burning: map flame on you (deepest %g) at %v for %.2f s: you catch fire\n",
                               deep, vr_map_at, p.burn_map_on));
            }
            VR_Burn_Ignite(p, vr_map_source, vr_map_at, vr_burn_out * -1, VR_BURN_TOUCH);
            vr_burn_part = 0;
            vr_burn_out = '0 0 0';
            p.burn_map_on = need;
        }
        else if(cvar_hget(cvarh_vr_burn_self_haptic) > 0)
        {
            float k = need > 0 ? p.burn_map_on / need : 1;
            float buzz = max(0, cvar_hget(cvarh_vr_burn_self_haptic));
            dprint(sprintf("burning: map flame on you (deepest %g): warning %.2f\n", deep, k));
            haptic(side, 0, 0.1, 90 + 160 * k, min(1, (0.2 + 0.6 * k) * buzz));
        }
        self = oldSelf;
    }
};

// Nails through a flame (vr_burn_nail_convert): a nail (the nailgun's, the super nailgun's) passing within
// vr_burn_nail_reach units of a lit torch's flame (held, thrown, lying or on its wall) becomes a lava nail: a lava nail's
// damage (Weapon Damage), its glow and trail, and it sets what it hits on fire (vr_burn_lava_nails).

.entity burn_tnext; // the lit torches, this frame (VR_Burn_NailFrame)

void(entity n, vector xFlame, entity xTorch) VR_Burn_NailToLava =
{
    if(!vr_have_lava) { return; }
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

.entity burn_fnext; // the other fires with an entity, this frame (VR_Burn_OtherFires)

// The fires with an entity other than the torches, chained through burn_fnext: the toolgun's flames, the lava balls
// in flight, and (vr_burn_nail_burning) each burning thing's fire.
entity() VR_Burn_OtherFires =
{
    entity fires = world;
    entity t;
    for(t = findflags(world, vr_flameprop, 3); t; t = findflags(t, vr_flameprop, 3))
    {
        t.burn_fnext = fires;
        fires = t;
    }
    for(t = find(world, classname, "fireball"); t; t = find(t, classname, "fireball"))
    {
        if(t.model != "") // (misc_fireball's spawner, in the lava, has no model)
        {
            t.burn_fnext = fires;
            fires = t;
        }
    }
    if(cvar_hget(cvarh_vr_burn_nail_burning))
    {
        for(t = find(world, classname, "vr_burn"); t; t = find(t, classname, "vr_burn"))
        {
            if(t.enemy != world && t.enemy.burn_fire == t && t.enemy.modelindex)
            {
                t.burn_fnext = fires;
                fires = t;
            }
        }
    }
    return fires;
};

// The fire volumes nails catch in (VR_Burn_NailCheck), by the flame's kind (the fire spots' vr_fire_spot_large; the
// toolgun's flames map to them): an upright capsule from vr_fv_lo to vr_fv_hi over the flame's origin, vr_fv_r round
// its axis. Sized to the flame as drawn (flame2.mdl's small ball about 6 wide to 16 over its origin, the large one, a
// brazier's, 13 wide to 39; a wall torch's head; a candle's wick), a little more all round. VR_BURN_FV_CULL: the farthest
// any volume's point is from its origin's middle (the cheap first test).
vector vr_fv_lo;
vector vr_fv_hi;
float vr_fv_r;
float VR_BURN_FV_CULL = 34;

void(float xKind) VR_Burn_FireVolume =
{
    if(xKind == 1)      { vr_fv_lo = '0 0 -6'; vr_fv_hi = '0 0 32'; vr_fv_r = 12; } // a large flame, a brazier's
    else if(xKind == 2) { vr_fv_lo = '0 0 1';  vr_fv_hi = '0 0 16'; vr_fv_r = 6; }  // a wall torch's
    else if(xKind == 3) { vr_fv_lo = '0 0 6';  vr_fv_hi = '0 0 13'; vr_fv_r = 4; }  // a candle's
    else                { vr_fv_lo = '0 0 -6'; vr_fv_hi = '0 0 12'; vr_fv_r = 8; }  // a small flame
};

// Whether a..b (its middle `mid`, half its length `half`) passes through the flame of kind `xKind` at `o`.
float(float xKind, vector o, vector a, vector b, vector mid, float half) VR_Burn_NailInFire =
{
    vector d = o + '0 0 13' - mid;
    float cull = half + VR_BURN_FV_CULL;
    if(d * d > cull * cull)
    {
        return FALSE;
    }
    VR_Burn_FireVolume(xKind);
    return VR_SegmentDistanceEps(a, b, o + vr_fv_lo, o + vr_fv_hi, 0.0001, 0) <= vr_fv_r;
};

// Nail `n` going a..b through a fire, a lava nail: a lit torch's flame (xTorches: within vr_burn_nail_reach), the map's
// flames (its fire spots: flames, braziers, static wall torches, candles; id's, the expansions', MG's, AD's), the
// toolgun's flames, a lava ball, and with vr_burn_nail_burning a burning body's or crate's flames (not yours): xFires.
void(entity n, entity xTorches, entity xFires, vector a, vector b) VR_Burn_NailCheck =
{
    float reach = max(0, cvar_hget(cvarh_vr_burn_nail_reach));
    entity t = xTorches;
    while(t)
    {
        vector fl = VR_WallTorch_Head(t) + '0 0 8';
        if(VR_PointSegmentDistance(fl, a, b) <= reach)
        {
            VR_Burn_NailToLava(n, fl, t);
            return;
        }
        t = t.burn_tnext;
    }
    vector mid = (a + b) * 0.5;
    float half = vlen(b - a) * 0.5;
    float i;
    for(i = 0; i < vr_fire_spots; i++)
    {
        vector o = vr_fire_spot[i];
        float kind = vr_fire_spot_large[i]; // (one array read a statement: the getters clobber vector arguments)
        if(VR_Burn_NailInFire(kind, o, a, b, mid, half))
        {
            VR_Burn_NailToLava(n, o + (vr_fv_lo + vr_fv_hi) * 0.5, n); // (the fizz on the nail: a static flame has no entity)
            return;
        }
    }
    for(t = xFires; t; t = t.burn_fnext)
    {
        if(t.vr_flameprop)
        {
            float pk = t.vr_flameprop == 3 ? 3 : (t.vr_flameprop == 2 ? 1 : 0);
            if(VR_Burn_NailInFire(pk, t.origin, a, b, mid, half))
            {
                VR_Burn_NailToLava(n, t.origin + (vr_fv_lo + vr_fv_hi) * 0.5, t);
                return;
            }
            continue;
        }
        if(t.classname == "fireball")
        {
            if(VR_PointSegmentDistance(t.origin, a, b) <= 10)
            {
                VR_Burn_NailToLava(n, t.origin, t);
                return;
            }
            continue;
        }
        entity v = t.enemy; // a fire burning v (VR_Burn_OtherFires)
        if(v == n.owner)
        {
            continue;
        }
        // Its flames: an upright capsule through its box's middle, as wide as the box's narrower side.
        vector c = (v.absmin + v.absmax) * 0.5;
        float r = min(v.absmax_x - v.absmin_x, v.absmax_y - v.absmin_y) * 0.5 + 2;
        float up = (v.absmax_z - v.absmin_z) * 0.5;
        vector dc = c - mid;
        float cull = half + up + r;
        if(dc * dc > cull * cull)
        {
            continue;
        }
        vector lo = c;
        vector hi = c;
        lo_z = v.absmin_z + min(r, up);
        hi_z = v.absmax_z - min(r, up);
        if(VR_SegmentDistanceEps(a, b, lo, hi, 0.0001, 0) <= r)
        {
            VR_Burn_NailToLava(n, c, v);
            return;
        }
    }
};

// A nail just fired (W_FireSpikes, W_FireSuperSpikes): its first two frames' way, before any frame's check (a torch
// held just past the muzzle, a monster just past it, hit in the frame it is fired).
void(entity n) VR_Burn_NailLaunched =
{
    if(!vr_have_lava || !cvar_hget(cvarh_vr_burn_nail_convert))
    {
        return;
    }
    VR_Burn_NailCheck(n, VR_Burn_LitTorches(), VR_Burn_OtherFires(), n.origin, n.origin + n.velocity * frametime * 2);
};

// ----------------------------------------------------------------------------
// Fire put out by water (vr_burn_water_douse; the author's notes vrtesthall_2026-10-23_01-42-34, _01-43-21): a
// flamethrower's flame meeting the surface (vr_ad_enemyguns.qc VR_ADGun_FlameThink) and a lava nail going in (a nail
// again: VR_Burn_LavaNailsWater) go out in a puff of steam with a hiss. Slime too; not lava.

float(vector p) VR_Burn_InWater =
{
    float c = pointcontents(p);
    return c == CONTENT_WATER || c == CONTENT_SLIME;
};

// The surface between `a` (out of the water) and `b` (in it): six halvings, within a unit of it for a step of 64.
vector(vector a, vector b) VR_Burn_WaterSurface =
{
    for(float k = 0; k < 6; k++)
    {
        vector m = (a + b) * 0.5;
        if(VR_Burn_InWater(m)) { b = m; }
        else { a = m; }
    }
    return a;
};

// A puff of `n` steam at `at` and (at most every 0.12 s, vr_burn_nail_sound's volume) a hiss from `xFrom`.
float vr_burn_hiss_next;
void(vector at, float n, entity xFrom) VR_Burn_Doused =
{
    particle2(at, '0 0 0', QVR_PARTICLE_PRESET_STEAM, n);
    float vol = cvar_hget(cvarh_vr_burn_nail_sound);
    if(vol > 0 && time >= vr_burn_hiss_next)
    {
        vr_burn_hiss_next = time + 0.12;
        sound(xFrom, CHAN_AUTO, random() < 0.5 ? "vr/nail_sizzle1.wav" : "vr/nail_sizzle2.wav", min(1, vol * 0.7), ATTN_STATIC);
    }
};

// Every server frame (StartFrame): each lava nail about to go into water (where it is now, or where it will be next
// frame) put out at the surface: a plain nail (the nailgun's or the super nailgun's: their touch, model and damage),
// no glow or trail.
void() VR_Burn_LavaNailsWater =
{
    entity n = find(world, classname, "lava_spike");
    if(n == world || cvar("vr_burn_water_douse") <= 0)
    {
        return;
    }
    while(n)
    {
        entity next = find(n, classname, "lava_spike");
        vector b = n.origin + n.velocity * frametime;
        float now = VR_Burn_InWater(n.origin);
        if(now || VR_Burn_InWater(b))
        {
            vector at = now ? n.origin : VR_Burn_WaterSurface(n.origin, b);
            float super = n.touch == superlavaspike_touch;
            n.classname = "spike";
            n.touch = super ? superspike_touch : spike_touch;
            setmodel(n, super ? "progs/s_spike.mdl" : "progs/spike.mdl");
            setsize(n, VEC_ORIGIN, VEC_ORIGIN);
            n.effects = n.effects - (n.effects & (EF_VERYDIMLIGHT | EF_LAVATRAIL));
            VR_Burn_Doused(at, 4, n);
            dprint(sprintf("burning: a lava nail into water at %v: put out (%s)\n", at, super ? "super" : "normal"));
        }
        n = next;
    }
};

// ----------------------------------------------------------------------------
// Monsters and bodies touching the map's flames (vr_burn_map_monsters; the author's note vrtesthall_2026-10-23_01-41-13):
// as the player does (VR_Burn_MapFrame), a monster awake, a ragdoll (alive, knocked down, or dead) or a corpse whose box
// meets one of the map's flames (its fire spots: flames, braziers, static wall torches; a lit torch on its wall; the
// toolgun's resting flames) catches fire. A sleeping monster is left alone (it walks into none; one a mapper placed by a
// brazier is not lit before the player comes). Three bodies a server frame, in turn: a room of 60 each looked at about
// every 0.3 s, at a fixed cost a frame.
entity vr_burn_mapbody_next;
float(entity f) VR_Flare_Lit; // (vr_flaregun.qc: a flare burning where it lies is one of the flames)

// Whether the box lo..hi comes within r of the upright flame z0..z1 over o.
float(vector lo, vector hi, vector o, float z0, float z1, float r) VR_Burn_BoxNearFlame =
{
    float dx = max(0, max(lo_x - o_x, o_x - hi_x));
    float dy = max(0, max(lo_y - o_y, o_y - hi_y));
    if(dx > r || dy > r)
    {
        return FALSE;
    }
    float dz = max(0, max(lo_z - (o_z + z1), (o_z + z0) - hi_z));
    return dx * dx + dy * dy + dz * dz <= r * r;
};

vector vr_mb_fire; // the flame VR_Burn_MapBodyFlame found
float(entity e, vector o, float kind) VR_Burn_MapBodyFlame =
{
    float z0 = 0;
    float z1 = 12;
    float r = 3;
    if(kind == 1)      { z1 = 32; r = 6.5; } // (the player's sizes, VR_Burn_MapContact; 2 more round)
    else if(kind == 2) { z0 = 1; z1 = 16; r = 2.2; }
    if(!VR_Burn_BoxNearFlame(e.absmin, e.absmax, o, z0, z1, r + 2))
    {
        return FALSE;
    }
    vr_mb_fire = o + '0 0 1' * ((z0 + z1) * 0.5);
    return TRUE;
};

void(entity e) VR_Burn_MapBodyLook =
{
    float body = isragdoll(e) || e.vr_corpse || e.vr_corpse_dying || e.health <= 0;
    if(e.vr_gib || !e.modelindex || VR_Burn_Fireproof(e) || e.vr_deathdoll != world || (!body && e.enemy == world))
    {
        return;
    }
    if(e.burn_fire != world && e.burn_fire.burn_end > time + 1)
    {
        return; // (burning on: lit again only as it is about to go out)
    }
    float got = FALSE;
    float i;
    for(i = 0; i < vr_fire_spots && !got; i++)
    {
        vector o = vr_fire_spot[i];
        float kind = vr_fire_spot_large[i]; // (one array read a statement: the getters clobber vector arguments)
        if(kind != 3) // (a candle: nails only, as for the player)
        {
            got = VR_Burn_MapBodyFlame(e, o, kind);
        }
    }
    entity t;
    for(t = find(world, classname, "light_torch_small_walltorch"); t && !got; t = find(t, classname, "light_torch_small_walltorch"))
    {
        if(t.wt_state == 0 && t.wt_fire > 0 && t.modelindex)
        {
            got = VR_Burn_MapBodyFlame(e, t.origin, 2);
        }
    }
    for(t = findflags(world, vr_flameprop, 3); t && !got; t = findflags(t, vr_flameprop, 3))
    {
        if(t.carry_player == world && VR_Throw_Thrower(t) == world)
        {
            got = VR_Burn_MapBodyFlame(e, t.origin, t.vr_flameprop == 2 ? 1 : 0);
        }
    }
    // A flare burning where it lies (stuck, on the floor; vr_flaregun.qc): a small flame (ROUND22.md, "Flares ignite
    // ragdolls").
    for(t = find(world, classname, "vr_flare"); t && !got; t = find(t, classname, "vr_flare"))
    {
        if(VR_Flare_Lit(t))
        {
            got = VR_Burn_MapBodyFlame(e, t.origin, 0);
        }
    }
    if(!got || VR_Burn_InWater((e.absmin + e.absmax) * 0.5))
    {
        return;
    }
    vector at;
    at_x = max(e.absmin_x, min(vr_mb_fire_x, e.absmax_x));
    at_y = max(e.absmin_y, min(vr_mb_fire_y, e.absmax_y));
    at_z = max(e.absmin_z, min(vr_mb_fire_z, e.absmax_z));
    if(e.burn_fire == world)
    {
        dprint(sprintf("burning: %s (%s) touches a map flame at %v: catches fire\n", e.classname,
            e.health > 0 ? (isragdoll(e) ? "a ragdoll, alive" : "alive") : (isragdoll(e) ? "a ragdoll" : "a corpse"), vr_mb_fire));
    }
    VR_Burn_Ignite(e, world, at, at - vr_mb_fire, VR_BURN_TOUCH);
};

void() VR_Burn_MapBodiesFrame =
{
    if(cvar_hget(cvarh_vr_burn_map_monsters) <= 0)
    {
        return;
    }
    if(vr_fire_spots <= 0 && find(world, classname, "light_torch_small_walltorch") == world &&
       findflags(world, vr_flameprop, 3) == world && find(world, classname, "vr_flare") == world)
    {
        return; // (no fire in the map)
    }
    entity e = vr_burn_mapbody_next;
    if(e == world || !(e.flags & FL_MONSTER))
    {
        e = findflags(world, flags, FL_MONSTER);
    }
    for(float k = 0; k < 3 && e; k++)
    {
        VR_Burn_MapBodyLook(e);
        e = findflags(e, flags, FL_MONSTER);
    }
    vr_burn_mapbody_next = e;
};

// Wood in lava (NOTES.md start_2026-10-07_23-42-39; vr_burn_lava): a crate, a barrel or a piece of one whose bottom is
// in lava catches fire there (and burns its time on in it: VR_Burn_Think puts a fire out in water and slime, not lava).
// Looked at every VR_BURN_LAVA_LOOK s.
float VR_BURN_LAVA_LOOK = 0.2;
float vr_burn_lava_next;
void(string xClass) VR_Burn_LavaWood =
{
    for(entity e = find(world, classname, xClass); e; e = find(e, classname, xClass))
    {
        if(e.burn_fire != world || e.carry_player != world || !VR_Burn_Wood(e))
        {
            continue;
        }
        vector at = (e.absmin + e.absmax) * 0.5;
        at_z = e.absmin_z + 2;
        if(pointcontents(at) == CONTENT_LAVA && VR_Burn_Can(e))
        {
            VR_Burn_Ignite(e, world, at, '0 0 1', VR_BURN_LAVA);
        }
    }
};
// Bodies in lava (NOTES.md start_2026-10-09_15-27-59; vr_burn_lava_bodies): a monster's ragdoll, dead or knocked down,
// or its corpse, in lava catches fire there (lit again each look while it is in it: it burns on, VR_Burn_Think puts a fire
// out in water and slime, not lava; the engine floats it on the lava, vr_ragdoll_float_lava). After vr_burn_lava_gib s in
// it, burnt through, it bursts: a knocked-down one alive is killed by it (its death code's gibs), a corpse gibbed as a
// corpse (VR_Corpse_Gib; one that can't be goes in a puff of embers). A monster standing in lava is VR_Liquids_Frame's.
.float vr_lava_since; // when it was first found in lava (0: not in it at the last look)
void(entity e) VR_Knockdown_RemoveBody; // (vr_knockdown.qc)

// Whether body `e` is in lava: its middle's bottom, its origin or (a ragdoll) its head.
float(entity e) VR_Burn_BodyInLava =
{
    vector at = (e.absmin + e.absmax) * 0.5;
    at_z = e.absmin_z + 2;
    return pointcontents(at) == CONTENT_LAVA || pointcontents(e.origin) == CONTENT_LAVA ||
           (isragdoll(e) && pointcontents(ragdollcut(e, 6)) == CONTENT_LAVA);
};

void(entity e) VR_Burn_LavaBurst =
{
    vector mid = (e.absmin + e.absmax) * 0.5;
    particle2(mid, '0 0 60', QVR_PARTICLE_PRESET_LAVASPIKE, 24);
    particle2(mid, '0 0 30', QVR_PARTICLE_PRESET_BIGSMOKE, 8);
    if(e.burn_fire != world)
    {
        VR_Burn_Out(e.burn_fire, "burnt through in lava");
    }
    if(e.health > 0)
    {
        dprint(sprintf("burning: %s burnt through in lava (knocked down, health %g): killed\n", e.classname, e.health));
        T_Damage(e, world, world, e.health + 100); // (its death code's gibs: under its gib health)
        return;
    }
    if(VR_Corpse_NoGib(1))
    {
        return; // (never gibbed: vr_corpse_nogib, testing)
    }
    if(VR_Corpse_Parts(e))
    {
        dprint(sprintf("burning: %s burnt through in lava: gibbed\n", e.classname));
        VR_Corpse_Gib(e, world, world, 100);
        return;
    }
    dprint(sprintf("burning: %s burnt away in lava\n", e.classname));
    sound(e, CHAN_BODY, "player/inlava.wav", 1, ATTN_NORM);
    VR_Knockdown_RemoveBody(e);
};

void() VR_Burn_LavaBodies =
{
    float gibAfter = cvar_hget(cvarh_vr_burn_lava_gib);
    entity e = findflags(world, flags, FL_MONSTER);
    while(e)
    {
        entity next = findflags(e, flags, FL_MONSTER);
        float body = isragdoll(e) || e.vr_corpse || e.vr_corpse_dying || (e.health <= 0 && e.solid != SOLID_BBOX && e.solid != SOLID_SLIDEBOX);
        if(!body || e.vr_gib || !e.modelindex || VR_Burn_Fireproof(e) || e.vr_deathdoll != world)
        {
            e.vr_lava_since = 0;
        }
        else if(!VR_Burn_BodyInLava(e))
        {
            e.vr_lava_since = 0;
        }
        else
        {
            if(!e.vr_lava_since)
            {
                e.vr_lava_since = time;
                dprint(sprintf("burning: %s in lava (%s)\n", e.classname,
                    e.health > 0 ? "knocked down" : (isragdoll(e) ? "a ragdoll" : "a corpse")));
            }
            if(e.burn_fire == world || e.burn_fire.burn_end < time + 1)
            {
                vector at = (e.absmin + e.absmax) * 0.5;
                at_z = e.absmin_z + 2;
                VR_Burn_Ignite(e, world, at, '0 0 1', VR_BURN_LAVA); // (lit, or kept burning)
            }
            if(gibAfter > 0 && time >= e.vr_lava_since + gibAfter)
            {
                VR_Burn_LavaBurst(e);
            }
        }
        e = next;
    }
};

// You in lava (NOTES.md start_2026-10-09_17-59-40; vr_burn_lava_player): set on fire there, lit again each look while
// in it, so out of it you burn on that long (Burn Damage a second; in it only the lava's own damage: VR_Burn_Think).
// Water puts it out as ever. Not with the pentagram (no lava damage either) or the biosuit (id's cut lava's to a hit a
// second; it keeps the fire off).
void() VR_Burn_LavaPlayers =
{
    float len = cvar_hget(cvarh_vr_burn_lava_player);
    if(len <= 0)
    {
        return;
    }
    for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        if(p.health <= 0 || !p.takedamage || p.waterlevel <= 0 || p.watertype != CONTENT_LAVA ||
           p.invincible_finished > time || p.radsuit_finished > time ||
           AD_LavaShieldOn(p)) // AD support: AD's Lava Shield keeps lava's fire off too (vr_ad_items.qc)
        {
            continue;
        }
        // A flame where the lava reaches (the legs; the waist and up deeper in), round the body.
        float a = random() * 360;
        vector out = '0 0 0';
        out_x = cos(a * (3.14159265 / 180));
        out_y = sin(a * (3.14159265 / 180));
        vector at = p.origin + out * 6;
        at_z = p.origin_z + (p.waterlevel >= 2 ? -4 : -16) + random() * 8;
        if(p.burn_fire == world)
        {
            dprint(sprintf("burning: you in lava (level %g): you catch fire, burning %g s after it\n", p.waterlevel, len));
        }
        vr_burn_part = 0;
        vr_burn_out = out;
        vr_burn_len = len;
        VR_Burn_Ignite(p, world, at, out * -1, VR_BURN_LAVA);
        vr_burn_len = 0;
        vr_burn_part = 0;
        vr_burn_out = '0 0 0';
    }
};

void() VR_Burn_LavaFrame =
{
    if(time < vr_burn_lava_next)
    {
        return;
    }
    vr_burn_lava_next = time + VR_BURN_LAVA_LOOK;
    if(cvar_hget(cvarh_vr_burn_lava))
    {
        VR_Burn_LavaWood("vr_crate");
        VR_Burn_LavaWood("vr_crate_piece");
    }
    if(cvar_hget(cvarh_vr_burn_lava_bodies))
    {
        VR_Burn_LavaBodies();
    }
    VR_Burn_LavaPlayers();
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
    entity fires = VR_Burn_OtherFires();
    while(n)
    {
        entity next = find(n, classname, "spike");
        VR_Burn_NailCheck(n, torches, fires, n.origin - n.velocity * frametime, n.origin + n.velocity * frametime);
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
// console: how many, charred, burning); 11: a lava nail of yours at the nearest piece (from above it); 12: a nail shot
// through the nearest of the map's flames (a fire spot: flame, brazier, static wall torch, candle), level, from 64 before.

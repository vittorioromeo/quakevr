// vr_axestick.qc -- thrown axes stick (docs/vr-port/ROUND21.md, "Thrown axes stick"; engine: vr_axestick.cpp).
//
// The engine sees a thrown axe's blade go into something blade first, fast and square enough (vr_axestick_speed,
// _angle, _incidence), puts it there with its edge in (vr_axestick_depth), keeps it there as that thing moves (a
// door, a prop, a monster's model) and calls VR_AxeStick_Stuck. Here: the blow (as its touch), the sound, then:
// - in a monster, it bleeds it: vr_axestick_bleed health a second, the thrower's damage (no pain each time), blood
//   from the wound; it falls out when the monster dies or is gibbed, or what it is in goes (a prop taken or blown up);
// - a hand gripping it takes it (its handtouch: wpnthrow_handtouch), out of what it is in;
// - a force grab tugs it (vr_axestick_tug seconds, rocking it and the hand) before it comes free and flies to the hand.
// vr_axestick 0 lets every stuck axe fall.
//
// Embedded blades (ROUND23.md, "Embedded blades"; vr_embed): swords stick too, point first, and whatever is stuck stays in
// a body that dies, riding where it went in: it bleeds only while the body lives, falls when the limb it is in is cut off
// or the body gibbed or gone (VR_AxeStick_CutPrepare, _CutDone; its model changing or gone), and a hand gripping it in a body
// takes hold of it on its axis (vr_embedhold.qc, as a held stab's). A plate stops it (VR_AxeStick_Plate). Armed enemies and the
// drop cap leave it alone (VR_AxeStick_Kept). vr_embed 0: axes alone, as before.

float AXESTICK_LEVEL = 1;       // the level or a brush entity
float AXESTICK_PROP = 2;        // a prop
float AXESTICK_MODEL = 3;       // a monster's model
float AXESTICK_MONSTERBOX = 4;  // a monster's box
float AXESTICK_OUT = -1;        // it came out: it never sticks again
float AXESTICK_BLEED_TICK = 0.5; // seconds between two bleedings
float AXESTICK_POINT = 1;       // .vr_stick_embed: by its point (weapons.txt embed)
float AXESTICK_EDGE = 2;        // by its edge

.entity vr_stick_by;      // who threw it: the bleeding's attacker
.float  vr_stick_model;   // what it is in's model when it stuck (another, or none: gibbed, gone)
.float  vr_stick_next;    // the next bleeding
.float  vr_stick_owed;    // the bleeding's damage not dealt yet (a fraction of a point)
.float  vr_stick_tug;     // a force grab's tug: when it comes free (0: none)
.float  vr_stick_tugstart;
.float  vr_stick_tughand;
.entity vr_stick_tugger;
.float  vr_stick_cutmark; // when the cut under way (VR_AxeStick_CutPrepare) was found to take the limb it is in
.float  vr_stick_bone;    // the bone of the body it went into, as the body stood then (ragdolllimb: -1 the torso, -2 no rig)

float vr_embed_stabbing;  // (vr_embedhold.qc)
float(entity e) VR_Embed_Held;
float() VR_Embed_HoldThink;
void(entity e, string xWhy) VR_Embed_Free;

float(entity e) VR_AxeStick_IsStuck =
{
    return e.vr_stick_kind > 0;
};

float(entity e) VR_AxeStick_Tugging =
{
    return e.vr_stick_kind > 0 && e.vr_stick_tug > 0;
};

// Whether `e` is tugged by `xPlayer`'s hand `xHand` (the hand slot's VR_HOLD_TUG, vr_carry.qc VR_Hand_BusyHolds).
float(entity e, entity xPlayer, float xHand) VR_AxeStick_TuggedBy =
{
    return VR_AxeStick_Tugging(e) && e.vr_stick_tugger == xPlayer && e.vr_stick_tughand == xHand;
};

// No tug on `e` now (its hand free again).
void(entity e) VR_AxeStick_EndTug =
{
    if(e.vr_stick_tug > 0 && e.vr_stick_tugger != world)
    {
        VR_Hand_Unbusy(e.vr_stick_tugger, e.vr_stick_tughand, VR_HOLD_TUG, "axe tug over");
    }
    e.vr_stick_tug = 0;
};

float(entity e) VR_AxeStick_InMonster =
{
    return e.vr_stick_kind == AXESTICK_MODEL || e.vr_stick_kind == AXESTICK_MONSTERBOX;
};

// One that armed enemies don't take and the drop cap doesn't fade (vr_embed 0: as before, it may).
float(entity e) VR_AxeStick_Kept =
{
    return VR_AxeStick_IsStuck(e) && cvar_hget(cvarh_vr_embed);
};

// Whether what it is stays stuck by the settings: an edge's (the Super Axes need vr_embed too), a point's.
float(entity e) VR_AxeStick_On =
{
    if(e.vr_stick_embed == AXESTICK_POINT)
    {
        return cvar_hget(cvarh_vr_embed);
    }
    return cvar_hget(cvarh_vr_axestick) && (e.weapon == WID_AXE || cvar_hget(cvarh_vr_embed));
};

// Whether a blade in the bone `xBone` of `m` is loose on it: arms and legs (fast animations); not the torso, not the head.
float(entity m, float xBone) VR_AxeStick_LooseIn =
{
    if(!cvar_hget(cvarh_vr_embed) || xBone < 0)
    {
        return FALSE;
    }
    return (ragdolllimbs(m, 1) & VR_AgArmor_Bit(xBone)) == 0;
};

// The engine: `self` has just stuck in `other` (what: xKind), its edge going into it at xSpeed units/s; self.velocity
// is still the throw's.
void(float xKind, float xSpeed) VR_AxeStick_Stuck =
{
    self.vr_stick_bone = (xKind == AXESTICK_MODEL || xKind == AXESTICK_MONSTERBOX) ? ragdolllimb(other, self.vr_stick_at) : -1;
    // In an arm or a leg (a limb not the head's): loose on it; in the torso, the pelvis, the head: rigid.
    self.vr_stick_loose = VR_AxeStick_LooseIn(other, self.vr_stick_bone);
    // The throw's blow, as its touch would have struck (damage, blood, the sound), unless it had struck already (its
    // hit box meets a monster before its blade does).
    // Edge first into a monster's head (vr_decap.qc): its blow beheads it if it kills (a zombie: whatever).
    entity decapped = world;
    // (A point stabs: it cuts nothing off.)
    // (A hand's blow, vr_embedhold.qc, has dealt its own.)
    if((xKind == AXESTICK_MODEL || xKind == AXESTICK_MONSTERBOX) && self.vr_stick_embed != AXESTICK_POINT && !vr_embed_stabbing &&
       VR_Dismember_Thrown(other, self))
    {
        decapped = other;
    }
    if(other != world && other.takedamage && self.throwhit == QVR_THROWHIT_NEVER_HIT &&
       !GetThrownWeaponDisappearOnHit(self))
    {
        forcegrabbable_touch();
    }
    if(decapped != world)
    {
        VR_Decap_After(decapped);
    }
    self.throwhit = QVR_THROWHIT_HIT;
    self.vr_stick_by = VR_Throw_Thrower(self); // (its owner; past vr_throw_self_grace who threw it)
    self.vr_stick_model = other.modelindex;
    self.vr_stick_next = time + AXESTICK_BLEED_TICK;
    self.vr_stick_owed = 0;
    self.vr_stick_tug = 0;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT_BUT_TOUCHABLE; // (a hand's grip, a force grab)
    self.velocity = '0 0 0';
    self.avelocity = '0 0 0';
    self.vr_spin = '0 0 0';
    self.flags = self.flags - (self.flags & FL_ONGROUND);

    if(!vr_embed_stabbing)
    {
        VR_Highlight_AxeStick(xSpeed);
    }
    if(VR_AxeStick_InMonster(self))
    {
        // (Not id's axhit1: there's a man's grunt in it, vr_voice.cpp.)
        VR_SoundVaried(self, CHAN_AUTO, "vr/blade_flesh_in.wav", 1, ATTN_NORM);
        SpawnBlood(self.vr_stick_at, self.vr_stick_dir * -60, 12);
    }
    else
    {
        VR_SoundVaried(self, CHAN_AUTO, "player/axhit2.wav", 1, ATTN_NORM);
        particle2(self.vr_stick_at, '0 0 0', QVR_PARTICLE_PRESET_BULLETPUFF, 12);
    }
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("axestick: %s stuck in %s (kind %g) at %.0f u/s, thrown by %s\n", self.netname,
            other == world ? "the level" : other.classname, xKind, xSpeed, self.vr_stick_by.classname));
    }
};

// Out of what it is in: a rigid body again (it never sticks again), going at `xVel`.
void(entity e, vector xVel) VR_AxeStick_Free =
{
    e.vr_stick_kind = AXESTICK_OUT;
    e.vr_stick_ent = world;
    e.vr_stick_ofs = '0 0 0'; // (the engine's; named here so that the compiler keeps them)
    e.vr_stick_ang = '0 0 0';
    e.vr_stick_anchor = '0 0 0';
    e.vr_stick_edge = 0;
    e.vr_stick_vel = '0 0 0';
    e.vr_stick_spin = '0 0 0';
    e.vr_stick_wiggle = 0;
    e.vr_stick_embed = 0;
    e.vr_stick_depth = 0;
    e.vr_stick_sag = 0;
    e.vr_stick_sagv = 0;
    VR_AxeStick_EndTug(e);
    e.movetype = MOVETYPE_TOSS;
    e.solid = SOLID_NOT_BUT_TOUCHABLE;
    e.throwhit = QVR_THROWHIT_HIT; // (no blow when it lands)
    e.flags = e.flags - (e.flags & FL_ONGROUND);
    e.velocity = xVel;
    e.vr_spin = '0 0 0';
    setorigin(e, e.origin);
};

// Pulled out (a hand, a force grab): blood out of a monster, dust out of a wall.
void(entity e) VR_AxeStick_PulledOut =
{
    if(VR_AxeStick_InMonster(e))
    {
        SpawnBlood(e.vr_stick_at, e.vr_stick_dir * -120, 20);
        VR_SoundVaried(e, CHAN_AUTO, "vr/blade_flesh_out.wav", 1, ATTN_NORM);
    }
    else
    {
        particle2(e.vr_stick_at, e.vr_stick_dir * -20, QVR_PARTICLE_PRESET_BULLETPUFF, 8);
    }
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("axestick: %s pulled out of %s\n", e.netname, e.vr_stick_ent == world ? "the level" :
            e.vr_stick_ent.classname));
    }
};

// What it is in has let go of it (died, gibbed, gone; or vr_axestick 0): it falls, with that thing's motion.
void(entity e) VR_AxeStick_Fall =
{
    if(VR_Embed_Held(e))
    {
        VR_Embed_Free(e, "what it is in let go of it"); // (the hand has it back)
        return;
    }
    entity h = e.vr_stick_ent;
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("axestick: %s falls out of %s (its health %g)\n", e.netname, h == world ? "the level" : h.classname,
            h.health));
    }
    vector v = '0 0 0';
    if(h != world && h.modelindex == e.vr_stick_model)
    {
        v = h.velocity;
    }
    setorigin(e, e.origin - e.vr_stick_dir * 2); // (its edge out of the surface)
    VR_AxeStick_Free(e, v - e.vr_stick_dir * 40);
};

// A force grab's pull of a stuck axe (its flick, by `xPlayer`'s hand `xHand`): it tugs a moment first.
void(entity e, entity xPlayer, float xHand) VR_AxeStick_Tug =
{
    e.vr_stick_tugstart = time;
    e.vr_stick_tug = time + max(0, cvar_hget(cvarh_vr_axestick_tug));
    e.vr_stick_tugger = xPlayer;
    e.vr_stick_tughand = xHand;
    VR_Hand_Busy(xPlayer, xHand, VR_HOLD_TUG, e, "axe tug");
    haptic(xHand, 0.0, max(0.05, e.vr_stick_tug - time), 40, 0.8);
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("axestick: %s tugged by a force grab (%.2f s)\n", e.netname, e.vr_stick_tug - time));
    }
};

// While stuck (its think, every 0.02 s): what it is in still holds it; a tug; the bleeding.
void() VR_AxeStick_Think =
{
    entity h = self.vr_stick_ent;
    // (vr_embed: it stays in a dead body until the model it is in changes or goes: gibbed, removed.)
    if(!VR_AxeStick_On(self) ||
       (h != world && (h.modelindex != self.vr_stick_model || h.model == string_null)) ||
       (!cvar_hget(cvarh_vr_embed) && VR_AxeStick_InMonster(self) && (h.health <= 0 || h.solid == SOLID_NOT)))
    {
        VR_AxeStick_Fall(self);
        return;
    }

    if(VR_Embed_Held(self) && VR_Embed_HoldThink())
    {
        return; // (out, in the hand)
    }

    if(self.vr_stick_tug)
    {
        if(time < self.vr_stick_tug)
        {
            // Rocked about its edge, harder towards the end.
            float k = (time - self.vr_stick_tugstart) / max(0.01, self.vr_stick_tug - self.vr_stick_tugstart);
            self.vr_stick_wiggle = sin((time - self.vr_stick_tugstart) * 75) * (3 + 7 * k);
            return;
        }
        // Free: it flies to the hand (the force grab's flight, from here).
        entity axe = self;
        entity p = self.vr_stick_tugger;
        float hand = self.vr_stick_tughand;
        VR_AxeStick_PulledOut(axe);
        VR_AxeStick_Free(axe, '0 0 0');
        // (The tug's own busy kind ended with it, VR_AxeStick_Free: a hand busy now took something else meanwhile.)
        if(p.classname == "player" && p.health > 0 && VR_Hand_Free(p, hand, VR_HANDFREE_LOCKED))
        {
            self = p;
            VR_Forcegrab_Launch(axe, hand);
            VR_Forcegrab_SetPulled(hand, axe);
            self = axe;
        }
        return;
    }

    // The bleeding: the thrower's damage, without a flinch each time (it bleeds, it isn't struck).
    float rate = cvar_hget(cvarh_vr_axestick_bleed);
    if(VR_AxeStick_InMonster(self) && h.health > 0 && time >= self.vr_stick_next)
    {
        self.vr_stick_next = max(self.vr_stick_next + AXESTICK_BLEED_TICK, time); // (its cadence kept: the rate exact)
        // Whole points (Quake's damage is rounded up): the fraction owed until the next.
        self.vr_stick_owed = self.vr_stick_owed + max(0, rate) * AXESTICK_BLEED_TICK;
        float dmg = floor(self.vr_stick_owed);
        if(dmg >= 1 && h.takedamage)
        {
            self.vr_stick_owed = self.vr_stick_owed - dmg;
            SpawnBlood(self.vr_stick_at - self.vr_stick_dir * 2, self.vr_stick_dir * -40 - '0 0 20', rate * 2);
            entity by = self.vr_stick_by;
            if(by == world || by.classname != "player")
            {
                by = self; // (the thrower gone)
            }
            float before = h.health;
            void(entity attacker, float damage) pain = h.th_pain;
            h.th_pain = SUB_NullPain;
            VR_Hit_Begin(QVR_HITKIND_BLEED, -1, self.vr_stick_at, self.vr_stick_dir);
            T_Damage(h, self, by, dmg);
            VR_Hit_End();
            if(h.th_pain == SUB_NullPain)
            {
                h.th_pain = pain;
            }
            if(cvar_hget(cvarh_vr_debug_axestick))
            {
                dprint(sprintf("axestick: %s bleeds %s at %.2f s: health %g -> %g (%g a second, by %s)\n", self.netname,
                    h.classname, time, before, h.health, rate, by.classname));
            }
        }
    }
};

// The engine, as `self` is about to stick in the body `other` at xAt going along xDir: whether an unbroken plate is there,
// which stops it: a spark, a ring, and it bounces.
float(vector xAt, vector xDir) VR_AxeStick_Plate =
{
    if(other.vr_ar_type <= 0 || !other.vr_ar_mask)
    {
        return FALSE;
    }
    float piece = VR_AgArmor_PieceAt(other, xAt); // (QVR_ARMOR_PIECE_AT: a blade has no shot's line)
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("axestick: %s at %s's piece %g\n", self.netname, other.classname, piece));
    }
    if(piece < 0 || piece >= VR_AR_PIECES || !VR_AgArmor_On(other, piece))
    {
        return FALSE;
    }
    VR_AgArmor_HitFx(other, xAt, xDir * -1, 10);
    VR_SoundVaried(self, CHAN_AUTO, "player/axhit2.wav", 1, ATTN_NORM);
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("axestick: %s rings off %s's plate %g\n", self.netname, other.classname, piece));
    }
    return TRUE;
};

// A limb or head of `e` about to be cut (`xTakes`, the bones it takes: ragdolllimbs): the blades stuck in those bones marked,
// and after the cut (VR_AxeStick_CutDone) they fall.
void(entity e, float xTakes) VR_AxeStick_CutPrepare =
{
    if(!cvar_hget(cvarh_vr_embed))
    {
        return;
    }
    for(entity w = find(world, classname, "thrown_weapon"); w; w = find(w, classname, "thrown_weapon"))
    {
        if(w.vr_stick_ent == e && VR_AxeStick_InMonster(w))
        {
            if(w.vr_stick_bone >= 0 && (xTakes & VR_AgArmor_Bit(w.vr_stick_bone)))
            {
                w.vr_stick_cutmark = time;
            }
            if(cvar_hget(cvarh_vr_debug_axestick))
            {
                dprint(sprintf("axestick: %s on bone %g, the cut takes %g\n", w.netname, w.vr_stick_bone, xTakes));
            }
        }
    }
};

void(entity e) VR_AxeStick_CutDone =
{
    if(!cvar_hget(cvarh_vr_embed))
    {
        return;
    }
    for(entity w = find(world, classname, "thrown_weapon"); w; w = find(w, classname, "thrown_weapon"))
    {
        if(w.vr_stick_ent == e && VR_AxeStick_InMonster(w) && w.vr_stick_cutmark == time)
        {
            VR_AxeStick_Fall(w);
        }
    }
};

// `v` turned `deg` degrees about the unit `k` (right-handed).
vector(vector v, vector k, float deg) VR_AxeStick_Turn =
{
    float c = cos(deg * (3.14159265 / 180));
    float s = sin(deg * (3.14159265 / 180));
    vector kv;
    kv_x = k_y * v_z - k_z * v_y;
    kv_y = k_z * v_x - k_x * v_z;
    kv_z = k_x * v_y - k_y * v_x;
    return v * c + kv * s + k * ((k * v) * (1 - c));
};

// A model's angles (as a view's pitch: MakeThrown turns it) for its forward `fw` and up `up` (square to each other).
vector(vector fw, vector up) VR_AxeStick_Angles =
{
    vector a = vectoangles(fw);
    a_x = a_x * -1;
    a_z = 0;
    makevectors(a);
    vector c;
    c_x = v_up_y * up_z - v_up_z * up_y;
    c_y = v_up_z * up_x - v_up_x * up_z;
    c_z = v_up_x * up_y - v_up_y * up_x;
    float roll = atan2(c * fw, v_up * up) * (180 / 3.14159265);
    a_z = roll;
    makevectors(a);
    if(v_up * up < 0.999)
    {
        a_z = -roll; // (the other way round)
    }
    return a;
};

// The stuck axe nearest `xAt` (world: none).
entity(vector xAt) VR_AxeStick_Nearest =
{
    entity best = world;
    float bestDist = 100000;
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(VR_AxeStick_IsStuck(e) && vlen(e.origin - xAt) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - xAt);
        }
    }
    return best;
};

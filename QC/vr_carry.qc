// vr_carry.qc -- carrying and nudging physics items (ammo and health boxes, backpacks, gibs) with
// the hands (vr_carry). See ROUND6.md (removed 2026-10-06; git history), item 14.
//
// - A hand or a gun touching a box without gripping nudges it: the box takes up the hand's speed
//   along its motion, as a rigid body (it tumbles and settles).
// - Gripping a box holds it: it follows the hand, turning with it, and stops at walls (a line
//   from where it is to where the hand wants it) rather than passing through them; if it is left
//   far behind (stuck), it drops.
// - While a hand holds a box it cannot take a weapon (holsters, weapons lying about, force grab).
//   It still punches, harder (vr_carry_melee_mult): the box is an improvised weapon.
// - Taking it (its ammo or health), vr_carry_take: 0 letting go of it at a holster (hip or
//   shoulder: it goes in your pack), 1 pulling the trigger while holding it, 2 either. Walking over
//   it with body interactions on, or without tracked hands, still takes it as before.
// - Letting go leaves it with the hand's throw velocity, as a rigid body; thrown hard, it hurts
//   what it hits (vr_carry_throw_damage).
// vr_carry 0 restores the old behaviour: a hand touching a box takes it.
// Armour (vr_armor_wear, items.qc) is carried the same way and put on by letting go of it over the
// torso (VR_Armor_LetGo), never taken at a holster or with the trigger. Keys, runes, suits and the
// horn (vr_item_objects) are taken only at a holster (VR_PickupObj_Holster), never with the trigger.
// - Two hands (vr_carry_two_hands): the other hand gripping what one carries (a press within reach of
//   its surface, as the first grip) holds it too. It then follows both hands rigidly (the engine's
//   carry2h, vr_carry2h.cpp), and collides as one hand's does. One hand letting go leaves it in the
//   other, held from where it is (to pass it from hand to hand); both letting go together throw it (the
//   client sends both hands' throw estimate: the object's own motion). Let go of in both hands, it is
//   never taken at a holster: only a one-handed carry is.

// (.mainhand_held, .offhand_held: the box each hand carries, vr_fields.qc)
.entity carry_player;    // on a box: who carries it
.vector carry_offset;    // the box's origin in the hand's frame (forward, right, up)
.float carry_trigger;    // the carrying hand's trigger last frame
.float carry_nudgetime;
.float carry_2h;         // held in both hands (VR_Carry_TwoHandFrame)
.float carry_2h_end;     // when it was last held in both hands (let go of by one or both)
.void() carry_use;       // taking it: the box's own pickup (ammo_touch, health_touch)
.float carry_solid;      // let go of, its .solid (0: SOLID_NOT_BUT_TOUCHABLE, a pickup's; an explosive box: SOLID_BBOX)
.float carry_rigid;      // let go of, its .vr_rigid (0: 1; an explosive box: 2, a solid one)
.float carry_thrown;     // when it was last thrown hard enough to hurt (VR_Carry_Release; an explosive box's throw lasts
                         // VR_THROWN_BOX_TIME: VR_Explobox_Impact)
.float carry_teleported; // on a player: until then it has just been teleported (VR_Carry_Teleported; the engine's VR
                         // teleport, vr_physics.cpp): what it carries follows its hands from where it is (VR_Carry_Follow)
.float vr_slam_armed;    // a crate or barrel held in both hands: until when it was last over the head (VR_Crate_SlamFrame)
.float vr_slam_thrown;   // ... let go of from there: when (its first hard landing takes vr_crate_slam_throw_damage)
void(entity xBox) VR_Crate_SlamFrame;            // (vr_crates.qc)
void(entity xBox, float xHands) VR_Crate_SlamReleased;
.entity carry_letgo;     // the player who last let go of it (VR_Carry_Loose), and when: not hurt by it as it leaves the hand,
.float carry_letgo_time; // and credited with its flung hits for a while (VR_Prop_Flung)
.float vr_gib_made;      // when it became a gib (VR_MakeGibGrabbable; VR_Prop_Flung: harmless a while)
.entity vr_gib_bornin;   // what it touched while just made, and when last (VR_Prop_Flung: harmless to it until they part)
.float vr_gib_bornin_t;
.float vr_fling_hit;     // when it last hurt something flung (VR_Prop_Flung: one hit a VR_FLING_REHIT)
.float vr_2h_nomelee;    // on a player: until then neither hand strikes (just after a two-handed throw: VR_Throw_TwoHanded)
.float vr_2h_letgo_hand; // on a player: the hand (+1; 0 none) that let go first of a thing held in both, and when
.float vr_2h_letgo_time;
.entity vr_2h_thrower;   // the player who threw it with both hands, and when: his hands can't strike it for a moment
.float vr_2h_thrown;     // (VR_Throw_SparedBlows)
.float vr_throw_2h;      // on a thing thrown: 1 its last throw was with both hands (vr_throw_2h_damage: VR_Thrown_Damage)

entity(entity xPlayer, float xHand) VR_Carry_Held =
{
    if(xHand == cVR_OffHand)
    {
        return xPlayer.offhand_held;
    }
    return xPlayer.mainhand_held;
};

void(entity xPlayer, float xHand, entity xBox) VR_Carry_SetHeld =
{
    if(xHand == cVR_OffHand)
    {
        xPlayer.offhand_held = xBox;
    }
    else
    {
        xPlayer.mainhand_held = xBox;
    }
};

float(entity xPlayer, float xHand) VR_Carry_IsHolding =
{
    return VR_Carry_Held(xPlayer, xHand) != world;
};

// A box becomes a physics object again: a rigid body, touchable, no longer carried.
void(entity xBox) VR_Carry_Loose =
{
    if(xBox.carry_player != world)
    {
        xBox.carry_letgo = xBox.carry_player;
        xBox.carry_letgo_time = time;
    }
    xBox.carry_player = world;
    xBox.carry_2h = FALSE;
    xBox.movetype = MOVETYPE_TOSS;
    xBox.solid = xBox.carry_solid ? xBox.carry_solid : SOLID_NOT_BUT_TOUCHABLE;
    xBox.vr_rigid = xBox.carry_rigid ? xBox.carry_rigid : 1;
    xBox.flags = xBox.flags - (xBox.flags & FL_ONGROUND);
    if(xBox.carry_rigid == 2)
    {
        // A solid prop (a crate, an explosive box) is nobody's once let go of (taken, it was the carrier's: a trace from
        // him passes through it); thrown, VR_Carry_Release makes it his again until the throw is over.
        xBox.owner = world;
    }
    setorigin(xBox, xBox.origin);
};

// Whoever carries it lets go of it at once, in one hand or both (it blew up: barrel_explode).
void(entity xBox) VR_Carry_Forget =
{
    entity p = xBox.carry_player;
    if(p == world)
    {
        return;
    }
    if(p.mainhand_held == xBox)
    {
        p.mainhand_held = world;
    }
    if(p.offhand_held == xBox)
    {
        p.offhand_held = world;
    }
    xBox.carry_player = world;
    xBox.carry_2h = FALSE;
};

// How much harder a punch holding what `xHand` of `xPlayer` carries hits: Box Punch Damage (vr_carry_melee_mult), a box,
// a rock, a brick, a gib alike (their own Melee Damage x and their mass make the rest: VRMeleeDmg's VR_Weight_MeleeMult);
// 1 with nothing held. A club's blows (a prop with a melee tip: a wall torch, a whole brick) aren't punches: none.
// VR_Melee_Decide puts it in the blow's strength; W_FistMelee only for untracked hands (whose blows have none).
float(entity xPlayer, float xHand) VR_Carry_PunchMult =
{
    return VR_Carry_Held(xPlayer, xHand) == world ? 1 : cvar_hget(cvarh_vr_carry_melee_mult);
};

// ----------------------------------------------------------------------------
// Nudging

// Whether the engine's own body of `xHand` pushes the loose `xBox` (vr_box3d.cpp's reach bodies, ROUND21.md, "Hands
// and weapons as bodies"): an empty hand's (vr_box3d_hand_props: not a grenade, the palm catches it), a held weapon's
// (vr_box3d_weapon_push). Not a solid prop (an explosive box: the fist's sphere, and the gun's push below), nor a
// pickup not yet loose (the nudge knocks it loose).
float(entity xPlayer, float xHand, entity xBox) VR_Carry_BodyPushes =
{
    if(xBox.vr_rigid != 1 || !xPlayer.ishuman || !VRHandsTracked(xPlayer))
    {
        return FALSE;
    }
    if(VRIsHandWeaponless(xPlayer, xHand))
    {
        return cvar("vr_box3d_hand_props") && xBox.model != "progs/grenade.mdl" && xBox.model != "progs/mervup.mdl" &&
               xBox.model != "progs/proxbomb.mdl";
    }
    return cvar("vr_box3d_weapon_push") != 0;
};

void(entity xPlayer, float xHand, entity xBox) VR_Carry_Nudge =
{
    if(time < xBox.carry_nudgetime || xBox.carry_player != world)
    {
        return;
    }

    // Just thrown with both hands: their follow-through neither strikes nor shoves it (NOTES.md
    // vrfiringrange_2026-10-02_15-38-26: the hand that let go first touched the gib as it left, and burst it).
    if(VR_Throw_SparedBlows(xBox, xPlayer))
    {
        return;
    }
    // A small gib just torn out is never nudged: struck at a swing's speed it may burst (vr_smallgibs_blow_burst).
    if(VR_SmallGib_BlowSpared(xBox))
    {
        if(VR_SmallGib_BlowBursts(xBox))
        {
            VR_Gib_Struck(xPlayer, xHand, xBox);
            if(cvar_hget(cvarh_vr_smallgibs_trace))
            {
                bprint(sprintf("sgibtrace: #%g %.3f s: struck by the %s hand in its blow grace (%.2f m/s): %s\n",
                    xBox.vr_sgib_id, time - xBox.vr_sgib, xHand == cVR_MainHand ? "main" : "off",
                    vlen(VRGetEntHandVel(xPlayer, xHand)), xBox.vr_gib ? "not burst" : "burst"));
            }
        }
        return;
    }

    // A gib or head struck with a swing bursts (VR_Gib_Struck).
    if(xBox.vr_gib)
    {
        VR_Gib_Struck(xPlayer, xHand, xBox);
        if(!xBox.vr_gib)
        {
            if(xBox.vr_sgib && cvar_hget(cvarh_vr_smallgibs_trace))
            {
                bprint(sprintf("sgibtrace: #%g %.3f s: struck by the %s hand: burst\n", xBox.vr_sgib_id, time - xBox.vr_sgib,
                    xHand == cVR_MainHand ? "main" : "off"));
            }
            return;
        }
    }

    // So does a crate's piece, into dust (vr_crates.qc).
    if(xBox.vr_cratekind == 3)
    {
        VR_CratePiece_Struck(xPlayer, xHand, xBox);
        if(xBox.vr_cratekind != 3)
        {
            return;
        }
    }

    // Pushed by the hand's or the weapon's own body instead (it would be pushed twice).
    if(VR_Carry_BodyPushes(xPlayer, xHand, xBox))
    {
        return;
    }

    vector v = VRGetEntHandVel(xPlayer, xHand) * VRMetersToUnits() * cvar_hget(cvarh_vr_carry_nudge);
    float speed = vlen(v);
    if(speed < 10)
    {
        return;
    }

    // A solid prop (an explosive box) poked by a gun (or a hand without its body: vr_box3d_hand_push 0): pushed where the
    // hand is, so pushed high it tips over, low it slides (Box3D); by the hand's or the gun's mass, so a flick barely moves
    // a heavy box (ROUND21.md, "Pushes by mass").
    float pushMass = cvar("vr_box3d_hand_mass");
    if(!VRIsHandWeaponless(xPlayer, xHand))
    {
        pushMass = max(0, cvar("vr_box3d_weapon_arm_mass")) +
                   weaponvalue(WeaponIdToModel(VRGetEntWeapon(xPlayer, xHand), VRGetEntWeaponFlags(xPlayer, xHand)), "w_mass");
    }
    if(xBox.vr_rigid == 2 && physicspush(xBox, VRGetEntHandPos(xPlayer, xHand), v, pushMass))
    {
        xBox.carry_nudgetime = time + 0.05;
        if(xPlayer.ishuman)
        {
            entity pushSelf = self;
            self = xPlayer;
            haptic(xHand, 0.0, 0.05, 60, 0.3);
            self = pushSelf;
        }
        return;
    }

    // Pushed: along the hand's motion it moves at least as fast as the hand.
    vector d = v * (1 / speed);
    float along = xBox.velocity * d;
    if(along >= speed)
    {
        return;
    }
    if(xBox.vr_sgib && cvar_hget(cvarh_vr_smallgibs_trace))
    {
        bprint(sprintf("sgibtrace: #%g %.3f s: nudged by the %s hand (VR_Carry_Nudge, rigid %g): %.0f u/s to %.0f\n",
            xBox.vr_sgib_id, time - xBox.vr_sgib, xHand == cVR_MainHand ? "main" : "off", xBox.vr_rigid, vlen(xBox.velocity),
            vlen(xBox.velocity + d * (speed - along))));
    }
    xBox.velocity = xBox.velocity + d * (speed - along);
    VR_Carry_Loose(xBox);
    xBox.carry_nudgetime = time + 0.05;

    if(xPlayer.ishuman)
    {
        entity oldSelf = self;
        self = xPlayer;
        haptic(xHand, 0.0, 0.05, 60, 0.3);
        self = oldSelf;
    }
};

// ----------------------------------------------------------------------------
// Holding

float vr_carry_quiet; // set round VR_Carry_Start: no pickup sound (a wall torch pulled out played its own: vr_walltorch.qc)

// A hand closing on the prop `e` (VR_Carry_Start): what it is made of, as the physics sounds class it (vr_physsound.cpp
// materialOf) -- a crate or a piece of one wood, a rock or a brick stone, an explosive box metal: the climbing hand's
// grabs (a palm's slap and a tap of the material) -- and a gib or a head a small wet squish. "" for the rest (ammo and
// health boxes, backpacks, armour, keys: the old swish). One of three (four) at random.
string(entity e) VR_Carry_GrabSound =
{
    float k = floor(random() * 3);
    if(e.vr_gib)
    {
        return VR_Gib_SquishSound(TRUE);
    }
    if(VR_Prop_Metal(e))
    {
        return k < 1 ? "vr/phys/grab_metal1.wav" : (k < 2 ? "vr/phys/grab_metal2.wav" : "vr/phys/grab_metal3.wav");
    }
    if(VR_Debris_Is(e))
    {
        return k < 1 ? "vr/phys/grab_stone1.wav" : (k < 2 ? "vr/phys/grab_stone2.wav" : "vr/phys/grab_stone3.wav");
    }
    if(VR_Crate_Wooden(e))
    {
        return k < 1 ? "vr/phys/grab_wood1.wav" : (k < 2 ? "vr/phys/grab_wood2.wav" : "vr/phys/grab_wood3.wav");
    }
    return "";
};

void(entity xPlayer, float xHand, entity xBox) VR_Carry_Start =
{
    dprint("carry: taken\n");
    xBox.carry_player = xPlayer;
    xBox.vr_slam_armed = 0;
    xBox.vr_slam_thrown = 0;
    portal_carry(xBox, xPlayer, xHand, TRUE);
    VR_Head_StopFlies(xBox); // (they would buzz on where it lay)

    vector handPos = VRGetEntHandPos(xPlayer, xHand);
    vector handRot = VRGetEntHandRot(xPlayer, xHand);
    makevectors(handRot);

    // Against the curled fingers, not sunk into the fist nor held short of it (vr_held_surface_fit,
    // vr_held_fit_gap): moved along the way the palm faces (palms face in, as held: a right hand's
    // to its left) until its drawn shape just touches the drawn fist. Where it was gripped is kept
    // otherwise (any part of it can be taken).
    float rightHand = xHand == cVR_MainHand; // (the right controller, left-handed too)
    vector palm = rightHand ? v_right * -1 : v_right;
    vector d = xBox.origin + carryfit(xBox, handPos, palm) - handPos;
    xBox.carry_offset_x = d * v_forward;
    xBox.carry_offset_y = d * v_right;
    xBox.carry_offset_z = d * v_up;
    // Its place and turn in the hand by its Held Object Offsets' grip (where taken: as above; fixed; in the palm; along
    // the handle; and the offset on top): the engine keeps them from here, and places it again when they change.
    xBox.carry_offset = carrygrip(xBox, xPlayer, xHand, !rightHand, handRot, xBox.carry_offset);
    xBox.carry_trigger = VRGetEntFireButtonPressed(xPlayer, xHand);
    if(cvar("developer"))
    {
        dprint(sprintf("carry: %s held at %v (forward, right, up) of the hand, turned %v\n", xBox.classname,
            xBox.carry_offset, xBox.angles));
    }

    xBox.movetype = MOVETYPE_NONE;
    xBox.solid = SOLID_NOT;
    xBox.velocity = '0 0 0';
    xBox.avelocity = '0 0 0';
    xBox.vr_spin = '0 0 0';
    xBox.throwhit = QVR_THROWHIT_HIT;
    xBox.owner = xPlayer;
    xBox.carry_2h = FALSE;
    VR_Carry_SetHeld(xPlayer, xHand, xBox);

    entity oldSelf = self;
    self = xPlayer;
    if(VR_WallTorch_IsTorch(xBox))
    {
        if(!vr_carry_quiet)
        {
            sound(self, VRGetHandChannel(xHand), "vr/pommel2.wav", 0.35, ATTN_NORM); // a wooden stick taken up: a soft knock
        }
    }
    else if(!vr_carry_quiet)
    {
        // What it is made of, from the thing itself (in the hand: heard there, not from the middle of the head); the
        // rest (boxes, backpacks, armour) the old soft swish.
        string grab = VR_Carry_GrabSound(xBox);
        if(grab != "")
        {
            sound(xBox, CHAN_AUTO, grab, xBox.vr_gib ? 0.7 : 1, ATTN_NORM);
        }
        else
        {
            VR_SoundVaried(self, VRGetHandChannel(xHand), "knight/sword2.wav", 0.4, ATTN_NORM);
        }
    }
    haptic(xHand, 0.0, 0.1, 70, 0.6);
    self = oldSelf;
};

// Held by `xHand` alone from where it is (self is the player): the other hand let go of it, held in
// both. Its place and turn in the hand are taken now, as at a grip (not fitted to the fist: it does
// not move).
void(float xHand, entity xBox) VR_Carry_Regrip =
{
    vector handRot = VRGetHandRot(xHand);
    makevectors(handRot);
    vector d = xBox.origin - VRGetHandPos(xHand);
    xBox.carry_offset_x = d * v_forward;
    xBox.carry_offset_y = d * v_right;
    xBox.carry_offset_z = d * v_up;
    carryangles(xBox, handRot, TRUE);
    xBox.carry_trigger = VRGetFireButtonPressed(xHand);
};

// Weight (round 21, "Spring only; Weapon Weights and Held Object Weights; weight and damage"): a thrown thing's mass: a
// thrown weapon's (Weapon Weights: Mass) or a prop's (Held Object Weights).
float(entity e) VR_Thrown_Mass =
{
    if(e.classname == "thrown_weapon")
    {
        return weaponvalue(e.model, "w_mass");
    }
    return propvalue(e, "mass");
};

// The factor on its throw's speed thresholds (throwleniency: 1 but for heavy things, which can't be thrown as fast: their
// heavy leniency, and lower still for one too heavy to be thrown vr_throw_hit_top m/s one-handed).
float(entity e) VR_Thrown_Lenient =
{
    return throwweight(0, VR_Thrown_Mass(e));
};

// A thrown weapon's own multiplier of its hit speed, or of its damage curve (Weapon Weights: Damage Thresholds and
// Curves, `xKey` "w_throwmin" or "w_throwcurve"); a prop's 1.
float(entity e, string xKey) VR_Thrown_Own =
{
    if(e.classname == "thrown_weapon")
    {
        return weaponvalue(e.model, xKey);
    }
    return 1;
};

// The least speed (units/s) it hurts what it meets at (throwhitspeed: vr_throw_hit_min_speed times VR_Thrown_Lenient
// and the weapon's own).
float(entity e) VR_Thrown_HitSpeed =
{
    return throwweight(1, VR_Thrown_Mass(e), VR_Thrown_Own(e, "w_throwmin"));
};

// A heavy prop thrown (NOTES.md vrfiringrange_2026-10-07_23-00-55: a barrel thrown with both hands hurt for about 14, a
// blow with it about 90): its throw's damage times (mass / vr_throw_heavy_from)^vr_throw_heavy_exp, at most
// vr_throw_heavy_max, for props heavier than vr_throw_heavy_from kg (a crate, a barrel; weightdamage's curve, which
// every throw has, flattens out: a heavy thing thrown leaves the hand slow); 1 for a weapon (its own Weapon Weights) and
// lighter things.
float(entity e, float xMass) VR_Thrown_HeavyMult =
{
    float from = cvar_hget(cvarh_vr_throw_heavy_from);
    if(e.classname == "thrown_weapon" || from <= 0 || xMass <= from)
    {
        return 1;
    }
    return min(pow(xMass / from, max(0, cvar_hget(cvarh_vr_throw_heavy_exp))), max(1, cvar_hget(cvarh_vr_throw_heavy_max)));
};

// Its damage hitting something at `xSpeed` units/s (0: too slow): its base (GetThrownWeaponBaseDamage) times its speed
// over the throw's full-damage speed (.throwdmgspeed; that times VR_Thrown_Lenient for a heavy thing, as its thresholds
// are; eased in just over its hit speed: throwdamage), its own Throw Damage x and Throw Damage Curve, the weight's curve
// (weightdamage), a heavy prop's own (VR_Thrown_HeavyMult), vr_weapon_throw_damage_mult (every throw's, not only a
// weapon's: 0.5 since 2026-10-02) and, thrown with both hands (.vr_throw_2h), vr_throw_2h_damage.
// A thing thrown with both hands hurts more (NOTES.md vrfiringrange_2026-10-09_15-44-39: two-handed throws hurt no more
// than one-handed ones): vr_throw_2h_damage; 1 for one hand.
float(entity e) VR_Thrown_2hMult =
{
    return e.vr_throw_2h ? max(0, cvar_hget(cvarh_vr_throw_2h_damage)) : 1;
};

float(entity e, float xSpeed) VR_Thrown_Damage =
{
    float dmgSpeed = e.throwdmgspeed;
    if(!dmgSpeed) dmgSpeed = 700;
    float weapon = e.classname == "thrown_weapon";
    float own = weapon ? weaponvalue(e.model, "w_throwdmg") : propvalue(e, "throw_dmg");
    float mass = VR_Thrown_Mass(e);
    float curve = weightdamage(mass);
    float bySpeed = throwweight(2, mass, VR_Thrown_Own(e, "w_throwmin"), VR_Thrown_Own(e, "w_throwcurve"), xSpeed, dmgSpeed);
    return bySpeed * GetThrownWeaponBaseDamage(e) * own * curve * VR_Thrown_HeavyMult(e, mass) *
           cvar_hget(cvarh_vr_weapon_throw_damage_mult) * VR_Thrown_2hMult(e);
};

float VR_THROW_MISSILE_SPEED = 250; // u/s (times VR_Thrown_Lenient): a thing let go of this fast is thrown, a missile

// ----------------------------------------------------------------------------
// Two-handed throws (NOTES.md vrfiringrange_2026-10-02_00-54-03: a gib thrown with both hands burst at once on one hand's
// follow-through, taken for a punch). A throw with both hands (a prop let go of by both, VR_Carry_Release; a weapon held
// in both, DropWeaponInHandScaled): neither hand strikes for vr_throw_2h_nomelee, and the hand that let go first does
// not from its letting go (the other may still throw it with both, within vr_carry_two_hands_window). What was thrown
// can't be struck by the thrower's hands for vr_throw_2h_melee_immune (VR_Blow_Strikable).

// A throw's spin `xAVel` (rad/s) limited by the mass of what `xHands` hands throw (throwweight 3: vr_throw_spin_mass_exp;
// NOTES.md vrfiringrange_2026-10-02_01-22-37: a heavy explosive box spun fast from a wrist snap).
vector(vector xAVel, float xMass, float xHands) VR_Throw_Spin =
{
    float rate = vlen(xAVel);
    if(rate <= 0)
    {
        return xAVel;
    }
    return xAVel * (throwweight(3, xMass, xHands, rate) / rate);
};

// `p` threw `xThing` (world: a weapon, nothing to spare) with both hands.
void(entity p, entity xThing) VR_Throw_TwoHanded =
{
    p.vr_2h_nomelee = time + max(0, cvar_hget(cvarh_vr_throw_2h_nomelee));
    p.vr_2h_letgo_hand = 0;
    if(xThing != world)
    {
        xThing.vr_2h_thrower = p;
        xThing.vr_2h_thrown = time;
    }
    dprint(sprintf("throw: two-handed, %s: no blows for %.2f s, it spared them for %.2f s\n",
        xThing != world ? xThing.classname : "a weapon", max(0, cvar_hget(cvarh_vr_throw_2h_nomelee)),
        xThing != world ? max(0, cvar_hget(cvarh_vr_throw_2h_melee_immune)) : 0));
};

// Whether `xHand` of `p` may strike now: not just after a two-handed throw, nor (the hand that let go first of a thing
// held in both) while the other may still throw it with both.
float(entity p, float xHand) VR_Throw_HandMayStrike =
{
    if(time < p.vr_2h_nomelee)
    {
        return FALSE;
    }
    return !(p.vr_2h_letgo_hand == xHand + 1 && cvar_hget(cvarh_vr_throw_2h_nomelee) > 0 &&
             time - p.vr_2h_letgo_time <= cvar_hget(cvarh_vr_carry_two_hands_window) + 0.05);
};

// Whether `e` can't be struck by `p`'s hands: he threw it with both a moment ago.
// A small gib just torn out (vr_smallgibs_blow_grace): no hand, weapon or held thing strikes, nudges or bats it yet (the
// blow that tore it out went on through it). ROUND21.md, "Small gibs batted by the blade".
float(entity e) VR_SmallGib_BlowSpared =
{
    return e.vr_sgib > 0 && time < e.vr_sgib + max(0, cvar_hget(cvarh_vr_smallgibs_blow_grace));
};

float(entity e) VR_SmallGib_BlowBursts;

float(entity e, entity p) VR_Throw_SparedBlows =
{
    return e.vr_2h_thrower == p && time - e.vr_2h_thrown < cvar_hget(cvarh_vr_throw_2h_melee_immune);
};

// ----------------------------------------------------------------------------
// Your own throws (NOTES.md vrfiringrange_2026-10-01_16-41-40, _16-42-00; ROUND21.md, "Your own throws hurt you")
//
// A thing thrown hard is its thrower's (.owner: VR_Carry_Release, MakeThrown): Quake's owner rule lets it through him
// and its touch spares him (forcegrabbable_touch), so a backpack thrown high fell back through him. For
// vr_throw_self_grace seconds after the throw (as it leaves the hand, the arm's follow-through) it still does; then
// (VR_Throw_SelfFrame) it gives its owner up and meets him as anyone: falling back on him, it hurts him as it would a
// monster, the throw still his (VR_Throw_Thrower: the kills, the monsters' anger). Its throw over, a fast fall on him
// hurts as a flung prop (VR_Prop_Flung, past the same grace from his letting go).

// `e` just thrown hard by `xThrower` (its owner): spares him until vr_throw_self_grace from now. Only a player's, and
// not a grenade (it stays his: its blast's kills, his blows passing it; vr_grenade.qc).
void(entity e, entity xThrower) VR_Throw_SelfStart =
{
    e.vr_throw_self = 0;
    if(!(xThrower.flags & FL_CLIENT) || e.vr_shootnade)
    {
        return;
    }
    e.vr_throw_self = 1;
    e.vr_throw_self_at = time + max(0, cvar_hget(cvarh_vr_throw_self_grace));
    e.carry_letgo = xThrower;
    e.carry_letgo_time = time;
};

// Whose throw `e` is in (the damage is his): its owner, or once it meets its thrower (VR_Throw_SelfFrame), him.
entity(entity e) VR_Throw_Thrower =
{
    if(e.owner != world)
    {
        return e.owner;
    }
    if(e.vr_throw_self == 2)
    {
        return e.carry_letgo;
    }
    return world;
};

// Each frame (StartFrame): the things thrown hard whose grace is over give their owner up (not one taken again, flying
// to a force grab, or no longer its thrower's); a thing let go of gently ends its first fall (.vr_letgo_fall) once it
// lands or bounces (no longer falling, after a moment), is taken (a hand, the grapple), flies to a force grab, or after
// VR_LETGO_FALL_MAX.
float VR_LETGO_FALL_MAX = 3; // s
void() VR_Throw_SelfFrame =
{
    for(entity f = findflags(world, vr_letgo_fall, 1); f; f = findflags(f, vr_letgo_fall, 1))
    {
        float fell = time - f.carry_letgo_time;
        if(f.carry_player != world || f.fg_state != 0 || VR_Grapple_PropHolder(f) != world || fell > VR_LETGO_FALL_MAX ||
           (fell > 0.1 && f.velocity_z > -10))
        {
            dprint(sprintf("letgo: %s's first fall over %.2f s after it was let go (%.0f u/s): it can hurt %s now\n",
                f.classname, fell, vlen(f.velocity), f.carry_letgo.netname));
            f.vr_letgo_fall = 0;
        }
    }
    for(entity e = findflags(world, vr_throw_self, 1); e; e = findflags(e, vr_throw_self, 1))
    {
        if(time < e.vr_throw_self_at)
        {
            continue;
        }
        if(e.carry_player != world || e.fg_state != 0 || e.owner == world || e.owner != e.carry_letgo)
        {
            e.vr_throw_self = 0;
            continue;
        }
        dprint(sprintf("throw: %s %.2f s after %s threw it (%.0f u/s): it can hurt him now\n", e.classname,
            time - e.carry_letgo_time, e.owner.netname, vlen(e.velocity)));
        e.owner = world;
        e.vr_throw_self = 2;
    }
};

// Flung (ROUND21.md, "Flung props hit as thrown ones"; "Flung props: settings, and never you"): a physics prop not in
// a throw (never thrown, or its throw over) that meets a monster (or a player: vr_prop_impact_players) fast enough hurts
// it: swung on the grapple's rope as a flail, batted by a hand, knocked flying, dropped from a height. self is the prop,
// other what it met (a touch, or an explosive box's .vr_impact); `xSpeed` their speed towards each other (units/s,
// VR_Prop_Approach: never a resting contact, something pushed along by what it meets, nor a prop leaving it, as a
// batted one still in your box does), `xOwnSpeed` the prop's own towards it (it flies: never a box a player runs
// into). Both over vr_prop_impact_min_speed m/s (times VR_Thrown_Lenient). Its damage: the throw's base (Thrown Box
// Damage, a gib's, a weapon's own) times its Throw Damage x, vr_prop_impact_mult, the speed over the least one (at most
// vr_prop_impact_speed_max) and the mass's curve (VR_Prop_ImpactWeight). Never: carried, flying to a force grab, a gib
// or head still in its death's fling (Quake's bounce, before it first lands), under vr_prop_impact_min_mass kg, more
// than once a VR_FLING_REHIT, nor to the player who let go of it just now (vr_throw_self_grace; the grapple's holder
// lets go of it every frame it holds it) or whose grapple holds it (reeled in, it comes at the gun). The attacker is
// who holds it on the rope, else who let go of it (within VR_FLING_CREDIT). TRUE if it hurt.
float VR_FLING_REHIT = 0.5;       // s between a flung prop's hits
float VR_GIB_BORNIN_FLIGHT = 2;   // s: a gib's first flight, harmless to what it met as it was made (VR_Prop_Flung)
float vr_fling_gibhits;           // gibs' flung hits, and those spared as just made (vr_smallgibs_test 16, 17)
float vr_fling_gibspared;
float VR_FLING_CREDIT = 10;       // s

// When a monster dropped it (a weapon, a backpack: CreateThrownWeapon, DropBackpack; 0 never): for vr_prop_drop_grace s
// after, it can't hurt a player as a flung prop (vrfiringrange_2026-10-01_11-51-45: its loot, landing on him as it fell);
// after it, only falling on him or thrown by a hand (vr_prop_drop_falls_only). Box3D lets it pass through the body it
// was made in until clear (vr_prop_drop_pass_inside: vr_box3d.cpp noteBornInside).
.float vr_monster_drop;
float vr_fling_drophits;          // monster drops' flung hits on players, and those spared (vr_smallgibs_test 16, 17, 19)
float vr_fling_dropspared;

entity(entity xProp) VR_Grapple_PropHolder; // (vr_grapple.qc)
void(float damage) spawn_touchblood;

// How fast (units/s) `xVel` takes the prop `xProp` into `xOther`: towards the nearest point of the other's box from the
// prop's centre (its centre, the prop's inside it), the speed from half (a glancing blow: a flail sweeping past) to
// all of it (head on). Moving apart (a prop batted away, still in your box) or along it: that negative, or 0.
float(entity xProp, entity xOther, vector xVel) VR_Prop_Approach =
{
    vector c = 0.5 * (xProp.absmin + xProp.absmax);
    vector n;
    n_x = min(max(c_x, xOther.absmin_x), xOther.absmax_x);
    n_y = min(max(c_y, xOther.absmin_y), xOther.absmax_y);
    n_z = min(max(c_z, xOther.absmin_z), xOther.absmax_z);
    vector d = n - c;
    if(vlen(d) < 1)
    {
        d = 0.5 * (xOther.absmin + xOther.absmax) - c;
    }
    if(vlen(d) < 0.001)
    {
        return vlen(xVel);
    }
    float speed = vlen(xVel);
    float towards = xVel * normalize(d);
    if(towards <= 0 || speed <= 0)
    {
        return towards;
    }
    return speed * (0.5 + 0.5 * towards / speed);
};

// Mass's share of a flung prop's damage: (mass / vr_prop_impact_weight_ref)^vr_prop_impact_weight_curve, at most
// vr_prop_impact_weight_max (1 with the curve 0).
float(float xMass) VR_Prop_ImpactWeight =
{
    float k = max(0, cvar_hget(cvarh_vr_prop_impact_weight_curve));
    if(k == 0 || xMass <= 0)
    {
        return 1;
    }
    float ref = max(0.01, cvar_hget(cvarh_vr_prop_impact_weight_ref));
    return min(pow(xMass / ref, k), max(0.01, cvar_hget(cvarh_vr_prop_impact_weight_max)));
};

// Touches defer the two approach calculations until the cheap eligibility and mass
// checks have passed. Solid-impact callbacks provide the pre-bounce speeds directly.
float(float xSpeed, float xOwnSpeed, float xTouch) VR_Prop_Flung =
{
    if(!cvar_hget(cvarh_vr_prop_impact_damage) || other == world || !other.takedamage || other == self ||
       !(other.flags & FL_MONSTER || (other.classname == "player" && cvar_hget(cvarh_vr_prop_impact_players)) ||
         VR_Crate_Is(other)))
    {
        return FALSE;
    }
    // A gib, head or small gib just made (vr_gib_spawn_harmless): not yet (gibbing a body he stood on, its gibs pushed out
    // of him hurt him; ROUND21.md, "Small gibs batted by the blade").
    // Nor, after it, what it met then (him, standing on the body): while they still touch (a head lying at his feet,
    // pushed out of him), and in its first flight (pushed out of him up, falling back on him), VR_GIB_BORNIN_FLIGHT s
    // from its making unless a hand took it since (then its throw's rules).
    float freshTouch = other == self.vr_gib_bornin &&
                       (time < self.vr_gib_bornin_t + 0.1 || (time < self.vr_gib_made + VR_GIB_BORNIN_FLIGHT && self.carry_letgo == world));
    if(self.vr_gib && (time < self.vr_gib_made + max(0, cvar_hget(cvarh_vr_gib_spawn_harmless)) || freshTouch))
    {
        if(other == self.vr_gib_bornin || self.vr_gib_bornin == world || (other.flags & FL_CLIENT))
        {
            self.vr_gib_bornin = other; // (one: a player before a monster)
            self.vr_gib_bornin_t = time;
        }
        if(cvar("developer")) dprint(sprintf("prop: %s made %.2f s ago met %s: no harm yet (vr_gib_spawn_harmless)\n", self.classname,
            time - self.vr_gib_made, other.classname));
        vr_fling_gibspared++;
        return FALSE;
    }
    // In a throw (.throwhit QVR_THROWHIT_NEVER_HIT, 0, with a thrower: VR_Carry_Release, a thrown weapon's): its own hit.
    // A prop never thrown may have .throwhit 0 too (an explosive box), but no .owner.
    if((self.throwhit == QVR_THROWHIT_NEVER_HIT && VR_Throw_Thrower(self) != world) || self.carry_player != world ||
       self.fg_state != 0 ||
       (self.vr_gib && !self.vr_rigid) || time < self.vr_fling_hit + VR_FLING_REHIT)
    {
        return FALSE;
    }
    float mass = VR_Thrown_Mass(self);
    float least = max(1, cvar_hget(cvarh_vr_prop_impact_min_speed) * VRMetersToUnits());
    if(mass < cvar_hget(cvarh_vr_prop_impact_min_mass))
    {
        return FALSE;
    }
    float threshold = least * VR_Thrown_Lenient(self);
    if(xTouch)
    {
        // Approach cannot exceed either velocity's length. Slow resting contacts
        // need neither nearest-box-point calculation; the exact approaches follow
        // for contacts that could reach the configured damage threshold.
        vector relative = self.velocity - other.velocity;
        // Leave room for normalize/dot rounding at the exact threshold.
        if(vlen(self.velocity) < threshold * 0.99 || vlen(relative) < threshold * 0.99)
        {
            return FALSE;
        }
        xSpeed = VR_Prop_Approach(self, other, relative);
        xOwnSpeed = VR_Prop_Approach(self, other, self.velocity);
    }
    if(min(xSpeed, xOwnSpeed) < threshold)
    {
        return FALSE;
    }
    entity holder = VR_Grapple_PropHolder(self);
    if((other.flags & FL_CLIENT) && self.vr_monster_drop > 0)
    {
        if(time < self.vr_monster_drop + max(0, cvar_hget(cvarh_vr_prop_drop_grace)))
        {
            if(cvar("developer")) dprint(sprintf("prop: %s a monster dropped %.2f s ago met %s: no harm yet (vr_prop_drop_grace)\n", self.classname,
                time - self.vr_monster_drop, other.netname));
            vr_fling_dropspared++;
            return FALSE;
        }
        // Past it, only falling on him (from a height: mostly down) or thrown by a hand since (let go of, or on a
        // grapple's rope): never its toss up into him standing over the body, nor Box3D's push out of him (a grunt
        // killed underfoot flung its 15 kg backpack into him at 280 u/s: 19 damage "by nobody").
        vector flat = self.velocity;
        flat_z = 0;
        float falling = self.velocity_z < 0 && -self.velocity_z >= vlen(flat);
        if(cvar_hget(cvarh_vr_prop_drop_falls_only) && !falling && self.carry_letgo == world && holder == world)
        {
            if(cvar("developer")) dprint(sprintf("prop: %s a monster dropped met %s at %v, not falling nor thrown: no harm (vr_prop_drop_falls_only)\n",
                self.classname, other.netname, self.velocity));
            vr_fling_dropspared++;
            return FALSE;
        }
    }
    if(other == holder || (other == self.carry_letgo &&
                             (self.vr_letgo_fall || time < self.carry_letgo_time + max(0, cvar_hget(cvarh_vr_throw_self_grace)))))
    {
        return FALSE;
    }
    entity attacker = holder;
    if(attacker == world && self.carry_letgo != world && time < self.carry_letgo_time + VR_FLING_CREDIT)
    {
        attacker = self.carry_letgo;
    }

    // The throw's base damage (VR_Carry_Release, a thrown weapon's own: CreateThrownWeapon) and its Throw Damage x.
    float weapon = self.classname == "thrown_weapon";
    float base = weapon ? GetThrownWeaponBaseDamage(self) : 0;
    if(!base)
    {
        base = cvar_hget(self.vr_gib ? cvarh_vr_gib_throw_damage : cvarh_vr_carry_throw_damage);
    }
    float own = weapon ? weaponvalue(self.model, "w_throwdmg") : propvalue(self, "throw_dmg");
    float speedMult = min(xSpeed / least, max(0, cvar_hget(cvarh_vr_prop_impact_speed_max)));
    float weightMult = VR_Prop_ImpactWeight(mass);
    float dmg = base * own * cvar_hget(cvarh_vr_prop_impact_mult) * speedMult * weightMult;
    if(dmg <= 0)
    {
        return FALSE;
    }

    self.vr_fling_hit = time;
    if(self.vr_monster_drop > 0 && (other.flags & FL_CLIENT))
    {
        vr_fling_drophits++;
    }
    if(self.vr_gib)
    {
        vr_fling_gibhits++;
    }
    if(cvar("developer")) dprint(sprintf("prop: flung %s (%.1f kg, a gib made %.2f s ago) into %s at %.0f u/s (its own %.0f): %.1f damage", self.classname, mass,
        self.vr_gib ? time - self.vr_gib_made : -1,
        other.classname, xSpeed, xOwnSpeed, dmg));
    if(cvar("developer")) dprint(sprintf(" (base %.1f x%.2f, speed x%.2f, weight x%.2f), by %s\n", base, own, speedMult, weightMult,
        attacker == world ? "nobody" : attacker.netname));
    VR_SoundVaried(self, CHAN_AUTO, "fisthit.wav", 1, ATTN_NORM);
    VR_Blunt_HitLayer(self, CHAN_AUTO, other);
    spawn_touchblood(dmg); // (splinters off a crate: spawn_touchblood)
    float pop = VR_Decap_ThrownArm(other, self, attacker, dmg); // VR: a heavy blunt thing at the head pops it if it kills (vr_decap.qc)
    T_Damage(other, self, attacker, dmg);
    if(pop)
    {
        VR_Decap_After(other);
    }
    return TRUE;
};

// `xBox` (let go of: VR_Carry_Loose) thrown by self at `v` (units/s): its true-scale gravity, and thrown hard, a
// missile (VR_Carry_Release; VR_Throw_TestUp).
void(entity xBox, vector v) VR_Carry_Throw =
{
    xBox.velocity = v;
    xBox.vr_throw_2h = 0; // (VR_Carry_Release: 1 thrown with both hands)
    dprint("carry: thrown at "); dprint(vtos(v)); dprint(" ("); dprint(ftos(vlen(v))); dprint(" u/s), spin ");
    dprint(vtos(xBox.vr_spin)); dprint("\n");
    float m2u = VRMetersToUnits();
    if(cvar_hget(cvarh_vr_throw_gravity) > 0 && cvar_hget(cvarh_sv_gravity) > 0)
    {
        xBox.gravity = cvar_hget(cvarh_vr_throw_gravity) * m2u / cvar_hget(cvarh_sv_gravity);
    }

    // Thrown hard it is a missile: it hurts what bleeds (forcegrabbable_touch; an explosive box: VR_Explobox_Impact), not
    // the thrower. A heavy thing, which can't be thrown as fast, sooner (VR_Thrown_Lenient).
    if(vlen(v) > VR_THROW_MISSILE_SPEED * VR_Thrown_Lenient(xBox))
    {
        xBox.throwhit = QVR_THROWHIT_NEVER_HIT;
        xBox.throwdmgspeed = 700 * m2u / 120;
        xBox.carry_thrown = time;
        if(xBox.takedamage == DAMAGE_NO)
        {
            SetThrownWeaponDealDamage(xBox, TRUE); // (.health: not of a thing with health of its own, an explosive box)
        }
        // A gib's or anything else's (a box, a rock, a brick); its own Throw Damage x and its mass make the rest
        // (VR_Thrown_Damage).
        SetThrownWeaponBaseDamage(xBox, cvar_hget(xBox.vr_gib ? cvarh_vr_gib_throw_damage : cvarh_vr_carry_throw_damage));
        xBox.owner = self;
        xBox.vr_letgo_fall = 0;
        VR_Throw_SelfStart(xBox, self); // (it spares you only for vr_throw_self_grace)
    }
    else
    {
        xBox.owner = world;
        xBox.vr_throw_self = 0;
        // Let go of gently: in its first fall it never hurts who let go of it (VR_Prop_Flung; a heavy thing set down at
        // the feet), whatever the grace (NOTES.md vrfiringrange_2026-10-01_22-33-36).
        xBox.vr_letgo_fall = (xBox.carry_letgo != world);
    }
};

// Let go: it keeps the hand's throw (the same estimate thrown weapons use).
void(float xHand, entity xBox) VR_Carry_Release =
{
    // Thrown with both hands: held in both, or by the other until a moment ago (the hand's estimate is then the prop's
    // own, from both: held::bothHandsThrow).
    float hands = 1;
    if(xBox.carry_2h || time - xBox.carry_2h_end <= cvar_hget(cvarh_vr_carry_two_hands_window) + 0.05)
    {
        hands = 2;
    }

    VR_Carry_SetHeld(self, xHand, world);

    float m2u = VRMetersToUnits();
    vector throwVel = VRGetHandThrowVel(xHand);
    vector v = throwvelocity(throwVel, VRGetHandThrowFlick(xHand), propvalue(xBox, "mass"), hands) * m2u *
               cvar_hget(cvarh_vr_carry_throw_mult);
    v = v * propvalue(xBox, "throw"); // a heavy thing leaves the hand slower (Held Object Offsets: Throw, from its mass)

    VR_Carry_Loose(xBox);
    xBox.vr_spin = VR_Throw_Spin(VRThrowSpinCapped(VRGetHandAVel(xHand)), propvalue(xBox, "mass"), hands);
    VR_Carry_Throw(xBox, v);
    xBox.vr_throw_2h = hands == 2; // (its hits hurt vr_throw_2h_damage times more)
    // It goes on with the player's own motion, as a thrown weapon does (MakeThrown; NOTES.md
    // vrfiringrange_2026-10-07_23-51-46: a brick thrown running forward fell short), vr_carry_throw_inherit of it. (Its
    // throw, a missile or not, is judged on the hand's speed alone: VR_Carry_Throw.)
    xBox.velocity = xBox.velocity + self.velocity * max(0, cvar_hget(cvarh_vr_carry_throw_inherit));
    if(self.velocity != '0 0 0')
    {
        dprint(sprintf("carry: with your motion (%.0f u/s): %v (%.0f u/s)\n", vlen(self.velocity), xBox.velocity,
            vlen(xBox.velocity)));
    }
    if(hands == 2)
    {
        VR_Throw_TwoHanded(self, xBox);
    }
    VR_Crate_SlamReleased(xBox, hands); // a crate or barrel let go of from over the head (vr_crates.qc)
    xBox.vr_gib_handspeed = vlen(throwVel); // (a hard throw bursts it on a wall, however heavy: VR_Gib_HardThrow)
    VR_Gib_Released(xBox, vlen(v), xHand); // a gib or head thrown hard bursts on a wall, softer may stick
    VR_Grenade_Released(xBox, xHand); // a live grenade is the player's (vr_grenade.qc)
};

// The put-away transition (vr_collect_fx: vr_collectfx.cpp): a thing a hand puts away at a holster (into the pack) or a
// pouch is drawn going into it on that player's client, while it is taken at once here as before. VR_CollectFx_Note
// keeps its model and pose before it is taken (a taken thing may be removed); VR_CollectFx_Send (self the player), once it
// is gone, sends them.
nosave entity vr_cfx_ent;
nosave float vr_cfx_model;
nosave vector vr_cfx_org;
nosave vector vr_cfx_ang;

void(entity e) VR_CollectFx_Note =
{
    vr_cfx_ent = e;
    vr_cfx_model = e.modelindex;
    vr_cfx_org = e.origin;
    vr_cfx_ang = e.angles;
};

// `hotspot`: where it went (QVR_HS_*); 0: the hand's hotspot. Nothing unless that is a holster's or a pouch's.
void(float xHand, entity e, float hotspot) VR_CollectFx_Send =
{
    if(!hotspot)
    {
        hotspot = xHand == cVR_OffHand ? self.offhand_hotspot : self.mainhand_hotspot;
    }
    if(e != vr_cfx_ent || !vr_cfx_model ||
       !(isHolsterHotspot(hotspot) || hotspot == QVR_HS_GRENADE_POUCH || hotspot == QVR_HS_AMMO_POUCH))
    {
        return;
    }
    collectfx(xHand, e, hotspot, vr_cfx_model, vr_cfx_org, vr_cfx_ang);
    vr_cfx_ent = world;
};

// Take it: its own pickup, as if the carrying hand had touched it the old way. Taken at a holster or pouch, it is drawn
// going in (VR_CollectFx_Send).
void(float xHand, entity xBox) VR_Carry_Take =
{
    VR_CollectFx_Note(xBox);
    entity oldSelf = self;
    entity oldOther = other;
    self.touchinghand = xHand;
    xBox.handtouch_hand = xHand;
    xBox.handtouch_ent = self;
    other = self;
    self = xBox;
    xBox.solid = SOLID_NOT_BUT_TOUCHABLE; // pickups ignore non-solid items; it is held as one
    xBox.carry_use();
    if(xBox.model != string_null)
    {
        xBox.solid = SOLID_NOT; // not taken (full up): still held
    }
    self = oldSelf;
    other = oldOther;

    if(xBox.model == string_null) // taken (else: full up, still held)
    {
        VR_Carry_SetHeld(self, xHand, world);
        xBox.carry_player = world; // (a deathmatch item comes back: carried by no one)
        VR_CollectFx_Send(xHand, xBox, 0);
    }
};

// Whether `xHand` of `self` is at a holster (a hip, a shoulder, or an upper one), or at the grenade pouch at the small
// of the back (vr_handgrenade: the engine sets that hotspot only then): a place to put what is carried away (health,
// ammo, a power-up). Never a grenade (VR_Carry_HandFrame; an unarmed hand grenade goes back in the pouch first).
float(float xHand) VR_Carry_AtHolster =
{
    float hotspot = xHand == cVR_OffHand ? self.offhand_hotspot : self.mainhand_hotspot;
    return isHolsterHotspot(hotspot) || hotspot == QVR_HS_GRENADE_POUCH || hotspot == QVR_HS_AMMO_POUCH;
};

// Every player frame, for each hand (self is the player).
void(float xHand) VR_Carry_HandFrame =
{
    VR_Corpse_StrikeFrame(xHand); // a blow on a corpse (vr_corpse_gib, combat.qc)

    entity box = VR_Carry_Held(self, xHand);
    if(box != world && VR_Ragdoll_HandFrame(xHand, box))
    {
        return; // (a ragdoll's limb: the engine moves it)
    }
    if(box == world || (box.carry_2h && box == VR_Carry_Held(self, VRGetOtherHand(xHand))))
    {
        return; // (held in both hands: VR_Carry_TwoHandFrame)
    }

    if(box.model == string_null || box.carry_player != self || self.health <= 0 || !cvar_hget(cvarh_vr_carry))
    {
        dprint("carry: lost\n");
        VR_Carry_SetHeld(self, xHand, world);
        if(box.carry_player == self)
        {
            VR_Carry_Loose(box);
        }
        return;
    }

    VR_Carry_KeepAlive(box);
    portal_carry(box, self, xHand, FALSE); // also before a release in this frame

    float takeMode = cvar_hget(cvarh_vr_carry_take);
    if(!VR_HandGrabUtil_IsHandGrabbing(self, xHand))
    {
        // An unarmed hand grenade let go of at the grenade pouch: back in it (vr_grenade.qc).
        if(VR_HandGrenade_LetGo(xHand, box))
        {
            return;
        }
        // A round (a shell) let go of at the ammo pouch: back in the reserve; an ammo box: into it (vr_reload.qc).
        if(VR_Reload_LetGo(xHand, box) || VR_Reload_PouchTakesBox(xHand, box))
        {
            return;
        }

        // Armour (vr_armor_wear, items.qc): let go of over the torso it is put on; else it drops.
        // Never into the pack.
        if(box.vr_wearable)
        {
            if(!VR_Armor_LetGo(xHand, box))
            {
                dprint("carry: let go\n");
                VR_Carry_Release(xHand, box);
            }
            return;
        }

        // A key, a rune, a suit, the horn (vr_item_objects, items.qc): taken only at a holster
        // (whatever vr_carry_take says), as a box is.
        if(box.vr_pickup_obj == QVR_PICKUPOBJ_HOLSTER)
        {
            if(VR_Carry_AtHolster(xHand) && time - box.carry_2h_end > cvar_hget(cvarh_vr_carry_two_hands_window) + 0.05)
            {
                VR_PickupObj_Holster(xHand, box);
                return;
            }
            dprint("carry: let go\n");
            VR_Carry_Release(xHand, box);
            return;
        }

        // Let go at a holster: into the pack (if full up, it drops there). Not let go of by both hands
        // (together, as a throw: the other hand at most vr_carry_two_hands_window before, and a frame). Never a grenade
        // (nothing to take: it is thrown, as an overarm throw's wind-up passes the shoulder holsters).
        if(takeMode != 1 && VR_Carry_AtHolster(xHand) && !VR_Grenade_Is(box) &&
           time - box.carry_2h_end > cvar_hget(cvarh_vr_carry_two_hands_window) + 0.05)
        {
            dprint("carry: into the pack\n");
            VR_Carry_Take(xHand, box);
            if(box.model == string_null)
            {
                return;
            }
            VR_Carry_SetHeld(self, xHand, world);
            VR_Carry_Loose(box);
            box.velocity = '0 0 0';
            return;
        }
        dprint("carry: let go\n");
        VR_Carry_Release(xHand, box);
        return;
    }

    float trigger = VRGetFireButtonPressed(xHand);
    if(box.vr_wearable)
    {
        VR_Armor_HeldFrame(xHand, box); // over the torso: a haptic hint
    }
    else if(VR_HandGrenade_Trigger(xHand, box, trigger && !box.carry_trigger))
    {
        // (a hand grenade's trigger pulls its pin: vr_grenade.qc)
    }
    else if(takeMode != 0 && trigger && !box.carry_trigger && !box.vr_pickup_obj)
    {
        box.carry_trigger = trigger;
        VR_Carry_Take(xHand, box);
        if(box.model == string_null)
        {
            return; // taken (VR_Carry_Take let go of it)
        }
    }
    box.carry_trigger = trigger;

    // Follow the hand, turning with it (its angles set; its place in the hand the grip's: moved at once by a change to its
    // Held Object Offsets); not through walls (monsters: VR_Carry_Follow).
    vector handRot = VRGetHandRot(xHand);
    box.carry_offset = carryplace(box, handRot, box.carry_offset);
    makevectors(handRot);
    vector target = VRGetHandPos(xHand) + v_forward * box.carry_offset_x + v_right * box.carry_offset_y +
                    v_up * box.carry_offset_z;
    if(!VR_Carry_Follow(box, target))
    {
        VR_Carry_Release(xHand, box); // stuck behind something
        return;
    }

    // A round (a shell) held at the loading port of the gun in the other hand goes into it (vr_reload.qc). After it
    // followed the hand: where the hand has it this frame, as the port is (this frame's, moved with the body). Before,
    // it was tested where the last frame left it, a frame's walk behind the drawn gun: walking forward, the load point
    // was found a hand's reach ahead of the receiver (4.4 units at run speed; the author's note, 2026-10-08 14:00).
    VR_Reload_HeldFrame(xHand, box);
};

// Moves the held `box` (self is the player) towards `target`, not through walls (through monsters): FALSE if
// it is left behind (stuck).
// Teleported (a trigger_teleport, or the VR teleport), until self.carry_teleported: the hands move with the player a frame
// or two apart (the player's move as the server next reads them, the play space's turn to the destination's facing when
// the client takes it), so the line from where the held thing was to where the hand wants it can cross the level, or
// a wall: that is no reason to drop it (VR_Carry_Follow). It goes where the hand wants it, stopped short by a wall
// between it and the player's eyes; a hand still at the old place (not yet moved with the player: its target far from
// the body) leaves it where it is. Never dropped. TRUE.
float(entity box, vector target, vector mid, float clearance) VR_Carry_FollowTeleported =
{
    vector flat = target - self.origin;
    flat_z = 0;
    if(vlen(flat) > 64)
    {
        return TRUE; // (the hands not yet moved with the player)
    }
    traceline(self.origin + self.view_ofs, target + mid, TRUE, box); // (the walls: not the player's box, not the thing)
    vector dest = trace_endpos;
    if(trace_fraction < 1)
    {
        dest = dest + trace_plane_normal * clearance;
    }
    dest = dest - mid;
    box.velocity = '0 0 0';
    if(frametime > 0 && vlen(dest - box.origin) < 32)
    {
        box.velocity = (dest - box.origin) * (1 / frametime);
    }
    setorigin(box, dest);
    return TRUE;
};

// The player `p` was teleported from `xFrom` (teleport_touch): what its hands carry, in one hand or both, comes along,
// moved as it was (turned with the play space by its hands' next frames), and follows the hands from there for a moment
// (VR_Carry_FollowTeleported). Level changes still drop it.
void(entity p, vector xFrom) VR_Carry_Teleported =
{
    vector d = p.origin - xFrom;
    entity a = p.mainhand_held;
    entity b = p.offhand_held;
    if(a == world && b == world)
    {
        return;
    }
    p.carry_teleported = time + 0.3;
    // A held thing already carried through a teleporter (carry_portal: the hand reached through it, VR_PortalCarry) is in
    // the room the player now enters, by the hand: not moved again, and no longer mapped (else the next frame maps it
    // back through the gate).
    if(a != world && a.carry_player == p)
    {
        if(a.carry_portal > 0) { a.carry_portal = 0; }
        else { setorigin(a, a.origin + d); }
    }
    if(b != world && b != a && b.carry_player == p)
    {
        if(b.carry_portal > 0) { b.carry_portal = 0; }
        else { setorigin(b, b.origin + d); }
    }
    dprint(sprintf("carry: teleported with the player, moved by %v\n", d));
};

float(entity box, vector target) VR_Carry_Follow =
{
    // A solid prop (an explosive box: a big brush model whose origin is a bottom corner, in the floor it stands on) by its
    // middle, kept its smallest half-size off a wall (the others as before: by their origin).
    vector from = box.origin;
    vector mid = '0 0 0';
    float clearance = max(1, box.maxs_x);
    if(box.carry_rigid == 2)
    {
        mid = modelcentre(box) - box.origin;
        from = box.origin + mid;
        target = target + mid;
        vector half = (box.maxs - box.mins) * 0.5;
        clearance = max(1, min(half_x, min(half_y, half_z)));
    }
    if(time < self.carry_teleported)
    {
        return VR_Carry_FollowTeleported(box, target - mid, mid, clearance);
    }
    // Not stopped by monsters (nor players, nor solid props: only the level and its brush entities): a prop swung into a
    // monster as a club (its melee hit, vr_melee.qc) stopped at the monster's box while the hand went on, and was dropped
    // once the hand was 32 units past it (the author's note, 2026-09-29 21:20). It passes through them (it is not solid).
    traceline(from, target, TRUE, self);
    vector dest = trace_endpos;
    if(trace_fraction < 1)
    {
        dest = dest + trace_plane_normal * clearance;
    }
    dest = dest - mid;
    target = target - mid;
    if(vlen(dest - target) > 32)
    {
        dprint("carry: stuck at "); dprint(vtos(dest)); dprint(", the hand wants "); dprint(vtos(target)); dprint("\n");
        return FALSE;
    }

    if(frametime > 0)
    {
        box.velocity = (dest - box.origin) * (1 / frametime);
    }
    setorigin(box, dest);
    return TRUE;
};

// Dead (PlayerPostThink, self is the player: the hands' frames no longer run): what the hands carried,
// in one hand or both, drops with the motion it had (it hung in the air until the respawn).
void() VR_Carry_Dead =
{
    entity a = self.mainhand_held;
    entity b = self.offhand_held;
    if(a == world && b == world)
    {
        return;
    }
    dprint("carry: dead, let go\n");
    VR_Carry_SetHeld(self, cVR_MainHand, world);
    VR_Carry_SetHeld(self, cVR_OffHand, world);
    if(a != world && a.carry_player == self)
    {
        VR_Carry_Loose(a);
        a.owner = world;
    }
    if(b != world && b != a && b.carry_player == self)
    {
        VR_Carry_Loose(b);
        b.owner = world;
    }
};

// ----------------------------------------------------------------------------
// Two hands (vr_carry_two_hands)

// A held gib or backpack does not vanish.
void(entity box) VR_Carry_KeepAlive =
{
    if(box.think == SUB_Remove && box.nextthink < time + 5)
    {
        box.nextthink = time + 5;
    }
    if(box.think2 == SUB_Remove && box.nextthink2 < time + 5)
    {
        box.nextthink2 = time + 5;
    }
};

// The other hand (`xHand`) takes hold of what one carries (self is the player): a grip press within
// reach of its surface, as the first hand's (VR_Carry_Handtouch: a carried thing is not solid, so no
// touch comes).
void(float xHand) VR_Carry_TrySecondHand =
{
    float other = cVR_MainHand;
    if(xHand == cVR_MainHand)
    {
        other = cVR_OffHand;
    }
    entity box = VR_Carry_Held(self, other);
    if(box == world || box.carry_player != self || box.model == string_null || VR_Carry_IsHolding(self, xHand))
    {
        return;
    }
    if(!VR_Carry_GripPressed(self, xHand) || !VRIsHandEmpty(self, xHand) || VR_Forcegrab_IsHandBusy(self, xHand) ||
       !VR_HandGrabUtil_IsHandGrabbing(self, other) ||
       VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_BUSY : QVR_VRBITS0_OFFHAND_BUSY))
    {
        return;
    }
    if(!carryreach(box, VRGetHandPos(xHand), xHand))
    {
        dprint("carry: the other hand is not within reach\n");
        return;
    }

    if(!propvalue(box, "two_hands"))
    {
        return; // one hand only (Held Object Offsets: Two Hands)
    }

    dprint("carry: both hands\n");
    VR_Carry_SetHeld(self, xHand, box);
    box.carry_2h = TRUE;
    carry2h(box, self, TRUE); // the grips are kept
    VR_SoundVaried(self, VRGetHandChannel(xHand), "knight/sword2.wav", 0.25, ATTN_NORM);
    haptic(xHand, 0.0, 0.1, 70, 0.6);
};

// Let go of by both hands (self is the player): no longer held in both; the main hand's frame then
// lets go of it (thrown with the client's estimate for both hands, or put on over the torso), never
// into the pack.
void(entity box) VR_Carry_EndTwoHands =
{
    box.carry_2h = FALSE;
    box.carry_2h_end = time;
    VR_Carry_SetHeld(self, cVR_OffHand, world);
};

// Every player frame, before the hands' own (self is the player): what both hands hold. Taken by the
// second hand; placed by both; let go of by one (the other keeps it) or by both.
void() VR_Carry_TwoHandFrame =
{
    entity box = self.mainhand_held;
    if(box == world || box != self.offhand_held)
    {
        if(cvar_hget(cvarh_vr_carry_two_hands) && cvar_hget(cvarh_vr_carry) && self.health > 0)
        {
            VR_Carry_TrySecondHand(cVR_MainHand);
            VR_Carry_TrySecondHand(cVR_OffHand);
        }
        return;
    }
    if(box.carry_player != self && isragdoll(box))
    {
        return; // (a ragdoll's limbs in both hands: each hand its own, VR_Ragdoll_HandFrame)
    }

    if(box.model == string_null || box.carry_player != self || self.health <= 0 || !cvar_hget(cvarh_vr_carry))
    {
        dprint("carry: lost (both hands)\n");
        VR_Carry_SetHeld(self, cVR_MainHand, world);
        VR_Carry_SetHeld(self, cVR_OffHand, world);
        if(box.carry_player == self)
        {
            VR_Carry_Loose(box);
        }
        return;
    }
    VR_Carry_KeepAlive(box);
    portal_carry(box, self, cVR_MainHand, FALSE);
    box.carry_2h = TRUE;

    float gripMain = VR_HandGrabUtil_IsHandGrabbing(self, cVR_MainHand);
    float gripOff = VR_HandGrabUtil_IsHandGrabbing(self, cVR_OffHand);
    if(!gripMain && !gripOff)
    {
        dprint("carry: both hands let go\n");
        VR_Carry_EndTwoHands(box); // the main hand's frame throws it
        return;
    }
    // Pulled off it (carry2hoff: a hand further from its grip than the drift and a margin, its fist off the prop),
    // gripping or not: both, it drops with the motion it has (not a throw); one, it lets go of it as below.
    // (Not asked just after a teleport: the hands catch up with the player a frame or two apart,
    // VR_Carry_FollowTeleported; asking would forget the grips.)
    float pulledOff = 0;
    if(time >= self.carry_teleported)
    {
        pulledOff = carry2hoff(box, self);
    }
    if(pulledOff == 3)
    {
        dprint("carry: both hands pulled off it, dropped\n");
        VR_Carry_EndTwoHands(box);
        VR_Carry_SetHeld(self, cVR_MainHand, world);
        VR_Carry_Loose(box); // its velocity: the last frame's follow
        box.owner = world;
        haptic(cVR_MainHand, 0.0, 0.05, 60, 0.3);
        haptic(cVR_OffHand, 0.0, 0.05, 60, 0.3);
        return;
    }
    if(pulledOff == 1)
    {
        gripOff = FALSE;
    }
    else if(pulledOff == 2)
    {
        gripMain = FALSE;
    }
    if(!gripMain || !gripOff)
    {
        // One let go (or pulled off it): the other holds it, from where it is.
        float letGo = cVR_MainHand;
        float keep = cVR_OffHand;
        if(gripMain)
        {
            letGo = cVR_OffHand;
            keep = cVR_MainHand;
        }
        dprint(pulledOff ? "carry: one hand pulled off it, held in the other\n" : "carry: one hand let go, held in the other\n");
        box.carry_2h = FALSE;
        box.carry_2h_end = time;
        self.vr_2h_letgo_hand = letGo + 1; // (it strikes nothing while the other may yet throw it with both)
        self.vr_2h_letgo_time = time;
        VR_Carry_SetHeld(self, letGo, world);
        portal_carry(box, self, keep, FALSE);
        vector keptFrom = box.origin;
        carry2hkeep(box, self, keep); // onto its grip in that hand (pulled off: carry2hoff has done it)
        VR_Carry_Regrip(keep, box);
        if(cvar("developer"))
        {
            dprint(sprintf("carry: kept by hand %g, moved %.2f units by the let-go\n", keep, vlen(box.origin - keptFrom)));
        }
        haptic(letGo, 0.0, 0.05, 60, 0.3);
        return;
    }

    // Taken with either trigger (vr_carry_take 1 or 2; not armour).
    float trigger = VRGetFireButtonPressed(cVR_MainHand);
    float triggerHand = cVR_MainHand;
    if(!trigger && VRGetFireButtonPressed(cVR_OffHand))
    {
        trigger = TRUE;
        triggerHand = cVR_OffHand;
    }
    if(box.vr_wearable)
    {
        VR_Armor_HeldFrame(cVR_MainHand, box); // over the torso: a haptic hint
    }
    else if(VR_HandGrenade_Trigger(triggerHand, box, trigger && !box.carry_trigger))
    {
        // (a hand grenade held in both hands: either trigger pulls its pin, as in one; vr_grenade.qc)
    }
    else if(cvar_hget(cvarh_vr_carry_take) != 0 && trigger && !box.carry_trigger)
    {
        box.carry_trigger = trigger;
        VR_Carry_Take(triggerHand, box);
        if(box.model == string_null)
        {
            VR_Carry_SetHeld(self, cVR_MainHand, world);
            VR_Carry_SetHeld(self, cVR_OffHand, world);
            return; // taken
        }
    }
    box.carry_trigger = trigger;

    // Follow both hands, turning with them; not through walls. With vr_carry_two_hands_solid the engine has kept its whole
    // box out of the level already (carry2h: stopped at a wall, slid along it), so it goes there as it is (VR_Carry_Follow's
    // trace of its centre, kept its smallest half-size off what it met, put a turned box's corners into walls).
    vector to = carry2h(box, self, FALSE);
    if(cvar("vr_carry_two_hands_solid") && time >= self.carry_teleported)
    {
        if(frametime > 0)
        {
            box.velocity = (to - box.origin) * (1 / frametime);
        }
        setorigin(box, to);
    }
    else if(!VR_Carry_Follow(box, to))
    {
        VR_Carry_EndTwoHands(box);
        VR_Carry_Release(cVR_MainHand, box); // stuck behind something
        return;
    }
    VR_Crate_SlamFrame(box); // a crate or barrel swung down from over the head onto something breaks (vr_crates.qc)
};

// ----------------------------------------------------------------------------
// The box's touches

// QVR r20: a grip pressed on the box, now or at most VR_CARRY_GRIP_WINDOW before (the press may come a
// frame or two before the hand's touch), as a weapon is taken. A grip already held (a fist) never
// takes it: it punches or nudges it instead (a punch through a gib was a grab).
float VR_CARRY_GRIP_WINDOW = 0.2;
float(entity xPlayer, float xHand) VR_Carry_GripPressed =
{
    if(!VR_HandGrabUtil_IsHandGrabbing(xPlayer, xHand))
    {
        return FALSE;
    }
    if(!VR_HandGrabUtil_IsHandPrevGrabbing(xPlayer, xHand))
    {
        return TRUE; // pressed this frame (the press's time is recorded after the touches)
    }
    return time - VR_HandGrabUtil_Impl_GetLastGrabTime(xPlayer, xHand) <= VR_CARRY_GRIP_WINDOW;
};

// A ragdoll's hand touch (the engine gives it to a ragdoll: vr_box3d.cpp, "Ragdolls"; vr_ragdoll_grab): a hand on one of
// its limbs (within vr_ragdoll_grab_reach of its surface) pressing the grip takes it: the limb follows the hand through
// its joints, the body hanging from it (the engine's joint); let go of, it keeps the hand's throw (VR_Ragdoll_HandFrame).
// The hand holds it as a carried thing (its held field: busy), but the engine moves it.
void() VR_Ragdoll_Handtouch =
{
    if(other.classname != "player" || VRIsTouchingHandFake(other) || !isragdoll(self))
    {
        return;
    }
    float hand = VRGetTouchingHand(other);
    if(VR_Carry_GripPressed(other, hand) && VRIsHandEmpty(other, hand) && !VR_Forcegrab_IsHandBusy(other, hand) &&
       ragdollgrab(self, other, hand))
    {
        VR_Carry_SetHeld(other, hand, self);
        dprint("ragdoll: a limb taken by hand ");
        dprint(ftos(hand));
        dprint("\n");
        entity oldSelf = self;
        self = other;
        haptic(hand, 0.0, 0.08, 60, 0.5);
        self = oldSelf;
    }
};

// Two-handed ragdoll throws (ROUND21.md, "Two-handed ragdoll throws"; as a big gib's, "Two-handed throws of big gibs"):
// a ragdoll held in both hands (a limb each) and pushed away was struck by the hands themselves: a hand holding it is
// "armed" with it (a prop in the fist), and the hand that let go first swept on through it at a swing's speed, so the
// throw hurt it (or gibbed it) and its knock bent the trajectory. Now (VR_Ragdoll_HandSpares, in VR_Melee_Armed) a hand
// never strikes a ragdoll it holds, nor one it let go of VR_RAGDOLL_LETGO_SPARE ago; the hand that lets go of a ragdoll
// the other still holds strikes nothing while the other may still throw it with both (vr_2h_letgo_hand,
// VR_Throw_HandMayStrike), and the other's letting go within vr_carry_two_hands_window is a two-handed throw
// (VR_Throw_TwoHanded: no blows for vr_throw_2h_nomelee, the ragdoll spared for vr_throw_2h_melee_immune). The free hand
// still punches a ragdoll held in the other.
float VR_RAGDOLL_LETGO_SPARE = 0.4; // s (as a gib's releasing hand: VR_Gib_Struck)
.entity vr_rag_letgo;      // on a ragdoll: the player who last let go of it, with which hand (+1), and when
.float vr_rag_letgo_hand;
.float vr_rag_letgo_time;

// `xHand` of the player `self` let go of the ragdoll `xBox` (its held field already cleared).
void(float xHand, entity xBox) VR_Ragdoll_LetGo =
{
    float other = VRGetOtherHand(xHand);
    float twoHanded = FALSE;
    if(VR_Carry_Held(self, other) == xBox)
    {
        // The other still holds it: this hand strikes nothing while the other may still throw it with both.
        self.vr_2h_letgo_hand = xHand + 1;
        self.vr_2h_letgo_time = time;
    }
    else if(self.vr_2h_letgo_hand == other + 1 && xBox.vr_rag_letgo == self && xBox.vr_rag_letgo_hand == other + 1 &&
            time - xBox.vr_rag_letgo_time <= cvar_hget(cvarh_vr_carry_two_hands_window) + 0.05)
    {
        twoHanded = TRUE; // the other let go of it a moment ago: thrown with both
    }
    xBox.vr_rag_letgo = self;
    xBox.vr_rag_letgo_hand = xHand + 1;
    xBox.vr_rag_letgo_time = time;
    dprint(sprintf("ragdoll: limb let go of by hand %g at %.2f m/s%s\n", xHand, vlen(VRGetHandThrowVel(xHand)),
        twoHanded ? " (a two-handed throw)" : ""));
    if(twoHanded)
    {
        VR_Throw_TwoHanded(self, xBox);
    }
};

// Whether `xHand` of `p` spares the ragdoll `e` (VR_Melee_Armed): it holds it, or let go of it a moment ago.
float(entity e, entity p, float xHand) VR_Ragdoll_HandSpares =
{
    if(VR_Carry_Held(p, xHand) == e && e != world)
    {
        return TRUE;
    }
    return e.vr_rag_letgo == p && e.vr_rag_letgo_hand == xHand + 1 && time < e.vr_rag_letgo_time + VR_RAGDOLL_LETGO_SPARE;
};

// Every player frame, for a hand holding `xBox` (VR_Carry_HandFrame; self is the player): a ragdoll's limb. Let go of, it
// keeps the hand's throw; let go of by the engine (the ragdoll gibbed, the limb left behind a wall), the hand is empty
// again. FALSE: not a ragdoll's limb (a carried thing).
float(float xHand, entity xBox) VR_Ragdoll_HandFrame =
{
    if(xBox.carry_player == self)
    {
        return FALSE;
    }
    if(ragdollheld(self, xHand) != 1 || self.health <= 0)
    {
        if(isragdoll(xBox) || ragdollheld(self, xHand) == 0)
        {
            dprint("ragdoll: limb lost\n");
        }
        ragdollrelease(self, xHand, '0 0 0');
        VR_Carry_SetHeld(self, xHand, world);
        return TRUE;
    }
    if(!VR_HandGrabUtil_IsHandGrabbing(self, xHand))
    {
        VR_Carry_SetHeld(self, xHand, world);
        ragdollrelease(self, xHand, VRGetHandThrowVel(xHand) * VRMetersToUnits());
        VR_Ragdoll_LetGo(xHand, xBox);
    }
    return TRUE;
};

// A hand touching the box (handtouch): press the grip on it to hold it, else nudge it.
void() VR_Carry_Handtouch =
{
    if(!cvar_hget(cvarh_vr_carry) || other.classname != "player" || VRIsTouchingHandFake(other))
    {
        self.carry_use(); // the old way: touching takes it
        return;
    }

    if(self.carry_player != world || self.model == string_null)
    {
        return;
    }

    float hand = VRGetTouchingHand(other);
    // A live grenade in flight is caught or stopped by an open palm, never nudged (vr_grenade.qc).
    if(!forcegrabCatching && VR_Grenade_HandTouch(other, hand))
    {
        return;
    }
    // A force grab's catch (VR_Forcegrab_Catch) has checked the grip already: its window
    // (vr_forcegrab_catch_early) is longer than a touch's.
    if((forcegrabCatching || VR_Carry_GripPressed(other, hand)) && VRIsHandEmpty(other, hand) &&
       !VR_Forcegrab_IsHandBusy(other, hand))
    {
        VR_Carry_Start(other, hand, self);
        return;
    }

    // A solid prop (an explosive box) is pushed by the hand's own body in the physics (vr_box3d_hand_push): no nudge.
    if(self.vr_rigid == 2 && cvar("vr_box3d_hand_push"))
    {
        return;
    }
    VR_Carry_Nudge(other, hand, self);
};

// A gun poking the box (vr_wpntouch): nudge it.
void() VR_Carry_Weapontouch =
{
    if(!cvar_hget(cvarh_vr_carry) || other.classname != "player")
    {
        return;
    }
    if(VR_Grenade_InFlight(self))
    {
        return; // (a live grenade in flight: batted by a swing fast enough, VR_Deflect, not nudged)
    }
    VR_Carry_Nudge(other, VRGetTouchingHand(other), self);
};

// ----------------------------------------------------------------------------
// Gibs and heads (vr_grab_gibs): carried, thrown and force-grabbed like the boxes, with nothing to
// take. Their box is the model's; thrown hard they hurt (forcegrabbable_touch) and splatter.

void(vector org, vector xDir, float xSize) VR_Gore_Burst;
void(entity g, float xVolume) VR_Gib_Squish;                 // (vr_smallgibs.qc)
void(vector org, vector xDir, float xSize, entity xFrom) VR_SmallGib_Burst;
void(vector org, vector xDir, float xPop, entity xFrom) VR_SmallGib_Brains;
float vr_sgib_live;                                          // how many small gibs lie about

void() VR_Gib_Touch =
{
    // Another gib, head or small gib moving in its box (the engine touches what overlaps a mover each frame, a pile of
    // them n x n touches; vr_physics.cpp, VR_TouchLinks): nothing to do unless this one is thrown (its hit). Gibs
    // meeting hard are Box3D's hits; a thrown one's sweep is touchNearby's (vr_box3d.cpp).
    if(other.vr_gib && self.throwhit != QVR_THROWHIT_NEVER_HIT)
    {
        return;
    }
    float speed = vlen(self.velocity);
    float fast = speed > 220;
    if(fast && other.solid == SOLID_BSP)
    {
        // (A rigid one's knocks are the engine's: vr_physsound.cpp, flesh. This is its landing from Quake's bounce.)
        if(!self.vr_rigid)
        {
            VR_Gib_Squish(self, 0.7); // a squish (a small gib's smaller)
        }

        // With vr_gib_blood the engine splats and spurts blood where a gib hits the world (not doors
        // or lifts, which get this).
        if(!cvar_hget(cvarh_vr_gib_blood) || other != world)
        {
            particle2(self.origin, self.velocity * -0.1, QVR_PARTICLE_PRESET_BLOOD, 12);
        }
    }

    // Quake's bounce flings it from the body; from its first landing it is a rigid body (Box3D,
    // vr_box3d.cpp), which lies on the floor by its drawn shape. Quake's box does not turn with the
    // model (and a gib's collides as a player-sized one), so gibs came to rest up to half their size
    // sunk into the floor.
    if(!self.vr_rigid && other.solid == SOLID_BSP)
    {
        self.vr_rigid = 1;
        self.vr_spin = VRThrowSpinCapped(self.avelocity * 0.0087); // half its tumble, in radians
        self.avelocity = '0 0 0';
    }

    // Thrown hard into a monster: it hurts it (forcegrabbable_touch, which ends the throw; not the
    // thrower, whose body it leaves through), and bursts.
    float thrown = self.throwhit == QVR_THROWHIT_NEVER_HIT;
    vector dir = self.velocity;
    forcegrabbable_touch();
    if(thrown && self.throwhit != QVR_THROWHIT_NEVER_HIT && other.takedamage &&
       speed >= cvar_hget(cvarh_vr_gib_splat_speed) && VR_Gib_IsDestroyable(self))
    {
        dprint("gib: struck "); dprint(other.classname); dprint(" at "); dprint(ftos(speed)); dprint("\n");
        VR_Gib_Burst(self, dir);
    }
};

void(entity xGib) VR_MakeGibGrabbable =
{
    if(!cvar_hget(cvarh_vr_grab_gibs) || xGib.model == string_null)
    {
        return;
    }

    // The model's own box (Quake gave gibs none, a head a fixed one).
    setsize(xGib, modelbounds(xGib, 0), modelbounds(xGib, 1));
    xGib.solid = SOLID_NOT_BUT_TOUCHABLE;
    xGib.touch = VR_Gib_Touch;
    xGib.handtouch = SUB_Null; // nothing to take: VR_Carry_Setup keeps it as carry_use
    // A head is the monster itself: no longer one (not thrown by the killing blow's push, not a
    // target). Killed restores its solid after the death code (which often clears it).
    if(xGib.flags & FL_MONSTER)
    {
        xGib.vr_was_monster = TRUE; // (what it drops after: still a monster's, .vr_monster_drop)
    }
    xGib.flags = (xGib.flags - (xGib.flags & FL_MONSTER)) | FL_FORCEGRABBABLE;
    xGib.vr_gib = TRUE;
    xGib.vr_gib_made = time; // (harmless as a flung prop for vr_gib_spawn_harmless: VR_Prop_Flung)
    xGib.throwhit = QVR_THROWHIT_HIT;
    xGib.owner = world;
    SetThrownWeaponDealDamage(xGib, FALSE);
    SetThrownWeaponDisappearOnHit(xGib, FALSE);
    VR_Carry_Setup(xGib);
    xGib.vr_rigid = 0; // Quake's bounce until it first lands (VR_Gib_Touch) or is picked up; then a rigid body

    // They lie about longer, to be played with.
    if(xGib.think == SUB_Remove)
    {
        xGib.nextthink = time + 30 + random() * 15;
    }
};

// ----------------------------------------------------------------------------
// Destroying gibs and heads (vr_gib_destroy): shot, blown up, struck, or thrown hard at a wall or a
// monster, a gib or head bursts in a mist of blood and is gone. It has a little health
// (vr_gib_health); its damage never reaches Quake's death code (T_Damage hands it to
// VR_Gib_Damage), so nothing is counted or killed again. Shots and missiles stop at it (the
// engine's MOVE_HITGIBS: it is not solid, and never blocks anything else). Only a grabbable one
// (vr_grab_gibs) can be destroyed: it has a box to hit.

// Added to a traceline's "nomonsters" (FALSE or TRUE): it also stops at gibs and heads that can be
// destroyed (the engine's MOVE_HITGIBS). Missiles always do.
float MOVE_HITGIBS = 256;

.float vr_gib_healthmult; // 1 a gib, 1.5 a head
.float vr_gib_hp;         // what damage it takes yet (not .health: a head is a dead monster, which must stay dead)
.float vr_gib_armtime;    // when it can first be hit (not by the rest of the blow that threw it)
.float vr_gib_flight;     // thrown: until when a hard hit on a wall bursts it
.vector vr_gib_lastvel;   // thrown: its velocity at the last check, to see it strike a wall
.float vr_gib_releasetime; // when a hand last let go of it,
.float vr_gib_releasehand; // and which
.float vr_gib_handspeed;   // the hand's speed (m/s) as it threw it, before its mass limits the gib's (VR_Gib_HardThrow)

float(entity xGib) VR_Gib_IsDestroyable =
{
    return xGib.th_die == VR_Gib_Die && cvar_hget(cvarh_vr_gib_destroy);
};

// A small gib in its blow grace (VR_SmallGib_BlowSpared) that a blow still bursts (vr_smallgibs_blow_burst; Can Be
// Destroyed): the blade that tore it out bursts it as it goes on through, from its making (not only once armed,
// VR_Gib_Think2), and a blow that falls short of bursting it leaves it be (VR_Gib_Damage: never knocked away).
float(entity e) VR_SmallGib_BlowBursts =
{
    return VR_SmallGib_BlowSpared(e) && cvar_hget(cvarh_vr_smallgibs_blow_burst) && cvar_hget(cvarh_vr_smallgibs_destroy) &&
           VR_Gib_IsDestroyable(e) && e.carry_player == world;
};

float vr_sgib_graceblow; // set round a blow's T_Damage on a small gib in its blow grace (VR_Gib_Hit): it bursts or stays put

// A hand's blow or strike of `xDmg` on a loose gib or head (VR_Gib_Blow, VR_Gib_Struck). A small gib in its blow grace
// (VR_SmallGib_BlowBursts) takes it before it is armed, and isn't knocked away by it.
void(entity xGib, entity xPlayer, float xDmg) VR_Gib_Hit =
{
    float grace = VR_SmallGib_BlowBursts(xGib);
    float armedNow = grace && !xGib.takedamage;
    if(armedNow)
    {
        xGib.takedamage = DAMAGE_YES;
    }
    vr_sgib_graceblow = grace;
    T_Damage(xGib, xPlayer, xPlayer, xDmg);
    vr_sgib_graceblow = FALSE;
    if(armedNow && xGib.vr_gib)
    {
        xGib.takedamage = DAMAGE_NO; // armed as before (VR_Gib_Think2)
    }
};

// From ThrowGib and ThrowHead, after VR_MakeGibGrabbable: `xHealthMult` of vr_gib_health.
void(entity xGib, float xHealthMult) VR_Gib_MakeDestroyable =
{
    // Not a player's head (the player itself, who comes back).
    if(!xGib.vr_gib || (xGib.flags & FL_CLIENT) || !cvar_hget(cvarh_vr_gib_destroy))
    {
        return;
    }

    xGib.vr_gib_healthmult = xHealthMult;
    xGib.vr_gib_hp = max(1, cvar_hget(cvarh_vr_gib_health) * xHealthMult);
    if(xGib.health >= 0)
    {
        xGib.health = -1; // dead meat: a rocket's direct hit looks for some health (T_MissileTouch), a headbutt for more
    }
    xGib.th_die = VR_Gib_Die; // a head's own death code (it is the monster) never runs again
    xGib.th_pain = SUB_NullPain;
    xGib.takedamage = DAMAGE_NO; // until armed: VR_Gib_Think2
    xGib.vr_gib_armtime = time + 0.2;
    xGib.think2 = VR_Gib_Think2;
    xGib.nextthink2 = xGib.vr_gib_armtime;
};

float vr_gore_stick_thrown; // VR_Gore_Stick's chance for a thrown gib (0: by the gore's level)
float(entity g, vector at, vector normal, float xSpeed) VR_Gore_Stick;

// Whether a thrown gib's sharp turn (VR_Gib_Think2) was its thrower meeting it, not a wall: it is at his box or a hand
// of his (Box3D's capsule and hands' bodies, Quake's box), and no wall is just ahead of where it was going (NOTES.md
// vrfiringrange_2026-10-02_15-38-26: a gib thrown with both hands burst on the thrower's own body). Not with
// vr_gib_burst_on_thrower.
float(entity g) VR_Gib_TurnedByThrower =
{
    entity p = g.carry_letgo;
    if(p == world || !(p.flags & FL_CLIENT) || cvar_hget(cvarh_vr_gib_burst_on_thrower))
    {
        return FALSE;
    }
    vector m = '12 12 12';
    vector o = g.origin;
    float at = o_x >= p.absmin_x - m_x && o_y >= p.absmin_y - m_y && o_z >= p.absmin_z - m_z &&
               o_x <= p.absmax_x + m_x && o_y <= p.absmax_y + m_y && o_z <= p.absmax_z + m_z;
    if(!at && vlen(o - VRGetEntHandPos(p, cVR_MainHand)) > 16 && vlen(o - VRGetEntHandPos(p, cVR_OffHand)) > 16)
    {
        return FALSE;
    }
    float ahead = vlen(g.vr_gib_lastvel) * 0.03 + 12;
    traceline(o, o + normalize(g.vr_gib_lastvel) * ahead, TRUE, g);
    return trace_fraction >= 1 && !trace_startsolid;
};

// Whether `g` (one that can be destroyed) was thrown hard: the hand at Hard Throw Bursts (vr_gib_splat_throw, m/s) or
// faster. Its mass limits how fast a gib leaves the hand (a 20 kg one at 86 u/s from 3 m/s on, a 12 kg one at 136 from
// 4 m/s on: ROUND21.md "Hard throws burst gibs on walls"), so its own speed can't tell a hard throw from a soft one: the
// hand's does. Such a gib bursts where a softer one would stick (VR_Gib_Think2's wall hit). 0: off.
float(entity g) VR_Gib_HardThrow =
{
    float hard = cvar_hget(cvarh_vr_gib_splat_throw);
    return hard > 0 && g.vr_gib_handspeed >= hard && VR_Gib_IsDestroyable(g);
};

// Arms it, and follows a thrown one's flight (VR_Gib_Released). A thrown gib is a rigid body, whose
// landing on the world touches nothing: a hard hit is seen in its velocity, turned sharply between
// two frames: bursting it (one that can be destroyed), or slower sticking it (Thrown Gibs Stick).
void() VR_Gib_Think2 =
{
    float destroyable = VR_Gib_IsDestroyable(self);
    float hardThrow = VR_Gib_HardThrow(self);
    if(!destroyable)
    {
        self.takedamage = DAMAGE_NO;
        if(cvar_hget(cvarh_vr_gore_stick_thrown) <= 0)
        {
            self.vr_gib_flight = 0;
            return;
        }
    }

    float next = 0;
    if(destroyable && !self.takedamage)
    {
        if(time >= self.vr_gib_armtime && self.carry_player == world)
        {
            self.takedamage = DAMAGE_YES; // not DAMAGE_AIM: no autoaim at it
        }
        else
        {
            next = time + 0.1; // picked up at once: armed once let go of
        }
    }

    if(self.vr_gib_flight && (time >= self.vr_gib_flight || self.carry_player != world))
    {
        self.vr_gib_flight = 0;
    }
    if(self.vr_gib_flight)
    {
        float splat = cvar_hget(cvarh_vr_gib_splat_speed);
        vector change = self.velocity - self.vr_gib_lastvel;
        float lastSpeed = vlen(self.vr_gib_lastvel);
        if(vlen(change) >= min(splat, lastSpeed * 0.5) && VR_Gib_TurnedByThrower(self))
        {
            dprint(sprintf("gib: turned by its thrower's body or hands at %.1f, %.2f s after it was let go of: kept\n",
                lastSpeed, time - self.vr_gib_releasetime));
            self.vr_gib_lastvel = self.velocity;
            self.nextthink2 = time + 0.01;
            return;
        }
        if(destroyable && lastSpeed >= splat && vlen(change) >= splat)
        {
            dprint(sprintf("gib: struck the world at %.1f, %.2f s after it was let go of, %.0f units from you, now %v\n",
                lastSpeed, time - self.vr_gib_releasetime, vlen(self.origin - self.carry_letgo.origin), self.velocity));
            VR_Gib_Burst(self, self.vr_gib_lastvel);
            return;
        }
        // Slower (or one that can't burst): into a wall or a ceiling it may stick (vr_gore_stick_thrown), or burst there
        // if it was thrown hard (VR_Gib_HardThrow: the hand's speed, Hard Throw Bursts). What it struck:
        // what lies along its way; else across (falling steeply down a wall, its way down meets the floor first). A
        // hit is its speed into that surface (not along it) turned by a quarter or more in a frame (Box3D's contact
        // can take two), at Speed to Stick: a gib lobbed into a wall falls faster than it goes in, so its whole turn was
        // under half its speed (2026-10-06: about half of the throws at a wall never stuck).
        float stickSpeed = cvar_hget(cvarh_vr_gore_stick_speed);
        if(lastSpeed >= stickSpeed && vlen(change) >= max(stickSpeed, lastSpeed * 0.25) &&
           (cvar_hget(cvarh_vr_gore_stick_thrown) > 0 || hardThrow))
        {
            traceline(self.origin, self.origin + normalize(self.vr_gib_lastvel) * 40, TRUE, self);
            vector across = self.vr_gib_lastvel;
            across_z = 0;
            if((trace_fraction >= 1 || trace_startsolid || trace_plane_normal_z > 0.7) && vlen(across) > 1)
            {
                traceline(self.origin, self.origin + normalize(across) * 24, TRUE, self);
            }
            float into = -(self.vr_gib_lastvel * trace_plane_normal);
            if(trace_fraction < 1 && !trace_startsolid && into >= stickSpeed && change * trace_plane_normal >= into * 0.25)
            {
                if(hardThrow)
                {
                    dprint(sprintf("gib: thrown hard (the hand at %.1f m/s) into the world at %.1f, %.0f into it, %.2f s after "
                        "it was let go of: burst\n", self.vr_gib_handspeed, lastSpeed, into, time - self.vr_gib_releasetime));
                    VR_Gib_Burst(self, self.vr_gib_lastvel);
                    return;
                }
                vr_gore_stick_thrown = min(1, cvar_hget(cvarh_vr_gore_stick_thrown));
                float stuck = VR_Gore_Stick(self, trace_endpos, trace_plane_normal, lastSpeed);
                vr_gore_stick_thrown = 0;
                if(stuck)
                {
                    self.vr_gib_flight = 0;
                    return; // (VR_Gib_StuckThink2 has it now)
                }
            }
            dprint(sprintf("gib: struck the world at %.1f (turned by %.1f), %.2f s after it was let go of: not stuck%s\n",
                lastSpeed, vlen(change), time - self.vr_gib_releasetime, trace_fraction >= 1 ? " (no wall ahead)" :
                sprintf(" (a surface facing %.2f up, %.0f into it)", trace_plane_normal_z, into)));
        }
        self.vr_gib_lastvel = self.velocity;
        next = time + 0.01; // the next frame
    }

    if(next)
    {
        self.nextthink2 = next;
    }
};

// Let go of by `xHand` (VR_Carry_Release): that hand does not strike it as it follows through
// (VR_Gib_Struck; the other one may, after vr_gib_letgo_spare: toss it up and punch it). Thrown at `xSpeed` (units/s) as
// fast as it could burst (vr_gib_splat_speed) or stick (vr_gore_stick_speed, with vr_gore_stick_thrown), for a few
// seconds a hard hit on a wall bursts it, a softer one may stick it (vr_gore_stick_thrown: one that can't be destroyed
// too, a small gib). (Watched only from 250 u/s until 2026-10-02: a gib of the author's 8 to 20 kg leaves the hand at
// 6 m/s at most, about 200 u/s, so a thrown gib never stuck: NOTES.md vrfiringrange_2026-10-02_19-22-07.)
void(entity xGib, float xSpeed, float xHand) VR_Gib_Released =
{
    float stick = xGib.vr_gib && cvar_hget(cvarh_vr_gore_stick_thrown) > 0 && cvar_hget(cvarh_vr_gore) > 0 &&
                  cvar_hget(cvarh_vr_gore_stick) > 0;
    float destroyable = VR_Gib_IsDestroyable(xGib);
    if(!destroyable && !stick)
    {
        return;
    }
    xGib.vr_gib_releasetime = time;
    xGib.vr_gib_releasehand = xHand;
    float watch = destroyable ? cvar_hget(cvarh_vr_gib_splat_speed) : 100000;
    if(stick || VR_Gib_HardThrow(xGib))
    {
        watch = min(watch, cvar_hget(cvarh_vr_gore_stick_speed));
    }
    if(xSpeed < max(1, watch))
    {
        return;
    }
    dprint("gib: thrown at "); dprint(vtos(xGib.velocity)); dprint("\n");
    xGib.vr_gib_flight = time + 3;
    xGib.vr_gib_lastvel = xGib.velocity;
    xGib.think2 = VR_Gib_Think2;
    xGib.nextthink2 = time + 0.01;
};

// Its damage, from T_Damage (instead of Quake's). Pushed by the blow, burst at no health. Held, it
// takes none.
float(entity targ, entity inflictor, entity attacker, float damage) VR_Gib_Damage =
{
    if(!cvar_hget(cvarh_vr_gib_destroy))
    {
        targ.takedamage = DAMAGE_NO;
        return FALSE;
    }
    if(targ.carry_player != world || damage <= 0)
    {
        return FALSE;
    }

    vector dir = '0 0 0';
    if(inflictor != world && inflictor != targ)
    {
        dir = normalize((targ.absmin + targ.absmax) * 0.5 - (inflictor.absmin + inflictor.absmax) * 0.5);
    }

    targ.vr_gib_hp = targ.vr_gib_hp - damage;
    dprint("gib: hit by "); dprint(inflictor.classname); dprint(" for "); dprint(ftos(damage)); dprint(", ");
    dprint(ftos(max(0, targ.vr_gib_hp))); dprint(" left\n");
    if(targ.vr_gib_hp <= 0)
    {
        VR_Gib_Burst(targ, dir);
        return TRUE;
    }
    if(vr_sgib_graceblow)
    {
        return TRUE; // a small gib in its blow grace: never batted away (VR_Gib_Hit)
    }

    // Knocked away (a rigid body lying asleep wakes as it moves).
    targ.velocity = targ.velocity + dir * min(400, damage * 12) + '0 0 1' * min(150, damage * 4);
    targ.flags = targ.flags - (targ.flags & FL_ONGROUND);
    return TRUE;
};

// Quake's death code, should anything run it (Killed): it bursts.
void() VR_Gib_Die =
{
    VR_Gib_Burst(self, '0 0 0');
};

// A severed head's flies (Scourge of Armagon's HeadThink, player.qc): misc/flys.wav loops, and a loop plays on
// where it started until something else takes its channel. Removing the head doesn't stop it (nor does the engine), so
// a head burst or cleared away, or carried off, left the buzz hanging in the air. Silenced then.
void(entity xHead) VR_Head_StopFlies =
{
    if(xHead.vr_flies)
    {
        xHead.vr_flies = FALSE;
        sound(xHead, 6, "misc/null.wav", 1, ATTN_STATIC);
        dprint("head: flies stopped at "); dprint(vtos(xHead.origin)); dprint("\n");
    }
};

// A head whose flies were stopped, removed: stopped again (the first stop goes out with the frame's other news, and
// is lost when a big blast's gore fills it).
void() VR_Head_RemoveSilenced =
{
    sound(self, 6, "misc/null.wav", 1, ATTN_STATIC);
    remove(self);
};

float vr_gib_burstsoundtime;

// A mist of blood at `org`, `size` times a gib's: soft clouds of blood through it, a spray of blood
// all round (splatting the walls and floor near by: the particles' decals), and more along the blow
// (`xDir`, if any). A gib bursting, a corpse gibbed (VR_Corpse_Gib, combat.qc).
void(vector org, vector xDir, float size) VR_BloodMist =
{
    vector d;
    float i;

    // The mist.
    for(i = 0; i < 3; i++)
    {
        d_x = crandom() * 6;
        d_y = crandom() * 6;
        d_z = crandom() * 6;
        particle2(org + d, d * 0.5, QVR_PARTICLE_PRESET_BLOOD, 12 * size);
    }

    // The spray, every way.
    for(i = 0; i < 4; i++)
    {
        d_x = crandom();
        d_y = crandom();
        d_z = crandom() * 0.7;
        particle2(org, normalize(d) * 22, QVR_PARTICLE_PRESET_BLOOD, 8 * size);
    }

    // Along the blow.
    if(xDir != '0 0 0')
    {
        particle2(org, normalize(xDir) * 30, QVR_PARTICLE_PRESET_BLOOD, 16 * size);
    }
};

// Bursts in a mist of blood (VR_BloodMist) and is gone, with the gib sound.
void(entity xGib, vector xDir) VR_Gib_Burst =
{
    vector org = (xGib.absmin + xGib.absmax) * 0.5;
    dprint("gib: burst at "); dprint(vtos(org)); dprint("\n");
    float size = max(1, xGib.vr_gib_healthmult);
    if(xGib.vr_sgib)
    {
        size = 0.35; // a small gib (vr_smallgibs_destroy): a puff
        if(xGib.vr_sgib_id)
        {
            vr_sgib_live = max(0, vr_sgib_live - 1);
        }
        xGib.vr_sgib_id = 0;
    }
    VR_BloodMist(org, xDir, size);
    VR_Gore_Burst(org, xDir, size);
    if(!xGib.vr_sgib)
    {
        VR_SmallGib_Burst(org, xDir, size, xGib); // broken into small ones (vr_smallgibs.qc)
        if(xGib.vr_gib_healthmult >= 1.5)
        {
            VR_SmallGib_Brains(org, xDir, FALSE, xGib); // a head: its brain too
        }
    }

    // Once a frame (a rocket bursts many).
    if(vr_gib_burstsoundtime != time)
    {
        vr_gib_burstsoundtime = time;
        sound(xGib, CHAN_AUTO, "player/udeath.wav", min(1, 0.55 * size), ATTN_NORM);
    }

    // Gone. Removed the next frame, not now: what struck it may still use it this frame.
    xGib.think = xGib.vr_flies ? VR_Head_RemoveSilenced : SUB_Remove;
    VR_Head_StopFlies(xGib);
    xGib.takedamage = DAMAGE_NO;
    xGib.th_die = SUB_Null;
    xGib.vr_gib_hp = 0;
    xGib.touch = SUB_Null;
    xGib.handtouch = SUB_Null;
    xGib.vr_wpntouch = SUB_Null;
    xGib.vr_gib = FALSE;
    xGib.vr_gib_flight = 0;
    xGib.flags = xGib.flags - (xGib.flags & FL_FORCEGRABBABLE);
    xGib.movetype = MOVETYPE_NONE;
    xGib.solid = SOLID_NOT;
    xGib.velocity = '0 0 0';
    xGib.avelocity = '0 0 0';
    xGib.vr_spin = '0 0 0';
    xGib.vr_rigid = 0;
    xGib.effects = 0;
    xGib.model = string_null;
    xGib.modelindex = 0;
    setorigin(xGib, xGib.origin);
    xGib.nextthink = time + 0.1;
    xGib.think2 = SUB_Null;
    xGib.nextthink2 = 0;
};

// ----------------------------------------------------------------------------
// Gore (vr_gore; the engine's vr_gore.cpp draws it all): events sent as particle2 presets far above
// the particle presets. A hit sprays blood onto the walls, floor and ceiling behind what it hits; a
// gibbing or a gib bursting paints the room round it; a monster that dies whole bleeds a pool under
// its corpse. Gibs flung from a body into a ceiling or a wall may stick there a while
// (vr_gore_stick), dripping (the engine sees them hang), then fall.

float QVR_GORE_HIT    = 40; // org: the hit, dir: its way * 7, count: damage + 1000 * kind (VR_Gore_Hit)
float QVR_GORE_BURST  = 41; // org, dir: the blow's way * 7 (or none), count: 10 * size
float QVR_GORE_CORPSE = 42; // org: the corpse's origin, dir_x: its size / 8

.float vr_gib_stuck; // stuck to a ceiling or a wall (vr_gore_stick): until when
.float vr_gib_stuckat; // when it stuck,
.vector vr_gib_stuckpos; // and where (it settles there: VR_Gib_StuckThink2)

// The firing range's training dummy bleeds as a grunt (its gore, wounds and small gibs) unless vr_dummy_gore is off
// (vr_dummy.qc; NOTES.md vrfiringrange_2026-10-03_02-07-05).
float(entity e) VR_Dummy_Clean =
{
    return VR_IsDummy(e) && !cvar_hget(cvarh_vr_dummy_gore);
};

float vr_gore_sent; // gore hits sent (VR_Gore_Hit: the small gibs' tests count them)

float(entity e) VR_Gore_Bleeds =
{
    return (e.flags & (FL_MONSTER | FL_CLIENT)) && !VR_Dummy_Clean(e) && cvar_hget(cvarh_vr_gore) > 0;
};

// From T_DamageImpl, as `take` is taken: the blow's way from what hit (a shot's own, a missile's
// flight, away from an explosion), and its kind (0 a shot or a missile, 1 a shotgun blast, 2 an
// explosion's, 3 a hand's blow). Not for damage from the world (lava, slime, drowning, falls).
void(entity targ, entity inflictor, float take) VR_Gore_Hit =
{
    if(take <= 0 || inflictor == world || !VR_Gore_Bleeds(targ))
    {
        return;
    }

    vector mid = (targ.absmin + targ.absmax) * 0.5;
    vector org = mid;
    vector dir = vr_hitdir;
    float kind = 0;
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        kind = 2;
        dir = mid - inflictor.origin;
    }
    else
    {
        if(vr_hitkind == QVR_HITKIND_MELEE || vr_hitkind == QVR_HITKIND_HEADBUTT || vr_hitkind == QVR_HITKIND_BASH ||
           vr_hitkind == QVR_HITKIND_SHOVE)
        {
            kind = 3;
        }
        else if(vr_hitpellets > 1)
        {
            kind = 1;
        }
        if(vr_hitpos != '0 0 0' && vlen(vr_hitpos - mid) < 64)
        {
            org = vr_hitpos;
        }
        if(dir == '0 0 0' && inflictor != targ)
        {
            if(inflictor.velocity != '0 0 0' && !(inflictor.flags & (FL_MONSTER | FL_CLIENT)))
            {
                dir = inflictor.velocity; // a missile's flight
            }
            else
            {
                dir = mid - (inflictor.absmin + inflictor.absmax) * 0.5;
            }
        }
    }
    if(dir == '0 0 0')
    {
        return;
    }
    vr_gore_sent++;
    particle2(org, normalize(dir) * 7, QVR_GORE_HIT, min(999, ceil(take)) + kind * 1000);
};

// A monster gibbed (ThrowHead) or a gib bursting (VR_Gib_Burst) at `org`: blood all round, more
// along the blow (`xDir`, if any), `xSize` times a gib's.
void(vector org, vector xDir, float xSize) VR_Gore_Burst =
{
    if(cvar_hget(cvarh_vr_gore) <= 0)
    {
        return;
    }
    vector d = '0 0 0';
    if(xDir != '0 0 0')
    {
        d = normalize(xDir) * 7;
    }
    particle2(org, d, QVR_GORE_BURST, rint(xSize * 10));
};

// From ThrowHead (self is the monster gibbed): the gore's burst, away from what gibbed it.
void() VR_Gore_Gibbed =
{
    vector dir = '0 0 0';
    if(damage_inflictor != world && damage_inflictor != self)
    {
        dir = self.origin - (damage_inflictor.absmin + damage_inflictor.absmax) * 0.5;
    }
    VR_Gore_Burst(self.origin, dir, 2);
};

// From Killed, after the death code: a monster (or player) that died whole bleeds a pool under it.
void(entity e) VR_Gore_Killed =
{
    if(!VR_Gore_Bleeds(e) || e.vr_gib || e.movetype == MOVETYPE_BOUNCE || e.model == string_null ||
       (e.flags & FL_SWIM))
    {
        return;
    }
    particle2(e.origin, '1 0 0' * min(7.9, max(16, e.maxs_x) / 8), QVR_GORE_CORPSE, 1);
};

// Gibs fly harder from a body with the gore (higher: onto the ceiling; not a gib thrown by hand).
vector(vector v) VR_Gore_Fling =
{
    float lvl = cvar_hget(cvarh_vr_gore);
    if(lvl >= 2)
    {
        v = v * 1.3;
        v_z = v_z + 150 + random() * 100;
    }
    else if(lvl >= 1)
    {
        v_z = v_z + 50;
    }
    return v;
};

.float vr_gib_flingtill; // flung from a body: until when it is watched for hitting a ceiling or a wall

void() VR_Gib_StuckThink2;

// Sticks `g`, flung from a body and just struck at `at` a surface facing `normal` going at `xSpeed`,
// to it: often a ceiling, less often a wall (vr_gore: 2 over the top more). TRUE if it stuck. Thrown by a hand
// (vr_gore_stick_thrown > 0: VR_Gib_Think2), Thrown Gibs Stick's chance on a ceiling or a wall alike.
float(entity g, vector at, vector normal, float xSpeed) VR_Gore_Stick =
{
    float lvl = cvar_hget(cvarh_vr_gore);
    float stick = cvar_hget(cvarh_vr_gore_stick);
    if(lvl <= 0 || stick <= 0 || xSpeed < cvar_hget(cvarh_vr_gore_stick_speed) || g.carry_player != world || g.vr_gib_stuck)
    {
        return FALSE;
    }
    float chance;
    if(normal_z < -0.7)
    {
        chance = lvl >= 2 ? 0.75 : 0.3; // a ceiling
    }
    else if(normal_z > -0.3 && normal_z < 0.3)
    {
        chance = lvl >= 2 ? 0.35 : 0.1; // a wall
    }
    else
    {
        return FALSE;
    }
    if(vr_gore_stick_thrown > 0)
    {
        chance = vr_gore_stick_thrown;
    }
    if(random() >= chance)
    {
        return FALSE;
    }

    dprint("gib: stuck at "); dprint(vtos(at)); dprint("\n");
    VR_Gib_Squish(g, 0.8);
    g.vr_gib_stuck = time + stick * (0.5 + random());
    g.movetype = MOVETYPE_NONE;
    g.vr_rigid = 0; // not the rigid bodies' any more (vr_rigid.cpp) until it lands again
    g.vr_spin = '0 0 0';
    g.velocity = '0 0 0';
    g.avelocity = '0 0 0';
    g.flags = g.flags - (g.flags & FL_ONGROUND);
    setorigin(g, at + normal * 3);
    g.vr_gib_stuckat = time;
    g.vr_gib_stuckpos = g.origin;
    g.think2 = VR_Gib_StuckThink2;
    g.nextthink2 = time + 0.1;
    return TRUE;
};

// When a gib is let go of by the fling watch or a stuck gib falls: armed as VR_Gib_Think2 would,
// then that (a destroyable gib) or nothing.
void() VR_Gore_Handover =
{
    if(VR_Gib_IsDestroyable(self))
    {
        self.think2 = VR_Gib_Think2;
        self.nextthink2 = time + 0.01;
    }
    else
    {
        self.think2 = SUB_Null;
        self.nextthink2 = 0;
    }
};

// A gib flung from a body, every frame for its first seconds: going fast, its velocity turned by a
// ceiling (it was rising, now it is not) or a wall (its way across reversed) may stick it there
// (VR_Gore_Stick; Quake's touches do not tell: from its first touch it is a rigid body). Armed
// meanwhile (VR_Gib_Think2's job).
void() VR_Gib_FlingThink2 =
{
    if(self.carry_player != world || time > self.vr_gib_flingtill || self.vr_gib_stuck || !self.vr_gib)
    {
        self.vr_gib_flingtill = 0;
        VR_Gore_Handover();
        return;
    }
    if(VR_Gib_IsDestroyable(self) && !self.takedamage && time >= self.vr_gib_armtime)
    {
        self.takedamage = DAMAGE_YES;
    }

    vector v0 = self.vr_gib_lastvel;
    vector v = self.velocity;
    self.vr_gib_lastvel = v;
    self.nextthink2 = time + 0.01;
    float speed = vlen(v0);
    if(speed < cvar_hget(cvarh_vr_gore_stick_speed))
    {
        return;
    }
    vector across = v0;
    across_z = 0;
    if(v0_z > 120 && v_z < v0_z * 0.3)
    {
        traceline(self.origin, self.origin + '0 0 40', TRUE, self); // a ceiling?
    }
    else if(vlen(across) > 150 && v * normalize(across) < vlen(across) * 0.3)
    {
        traceline(self.origin, self.origin + normalize(across) * 40, TRUE, self); // a wall?
    }
    else
    {
        return;
    }
    if(trace_fraction < 1 && !trace_startsolid && VR_Gore_Stick(self, trace_endpos, trace_plane_normal, speed))
    {
        self.vr_gib_flingtill = 0;
    }
};

// From ThrowGib and ThrowHead: a grabbable gib flung from a body is watched (VR_Gib_FlingThink2).
void(entity g) VR_Gore_WatchFling =
{
    if(!g.vr_gib || cvar_hget(cvarh_vr_gore) <= 0 || cvar_hget(cvarh_vr_gore_stick) <= 0)
    {
        return;
    }
    g.vr_gib_flingtill = time + 2.5;
    g.vr_gib_lastvel = g.velocity;
    g.think2 = VR_Gib_FlingThink2;
    g.nextthink2 = time + 0.01;
};

// A stuck gib, a few times a second: it falls when its time is up, or when it is struck or shot
// (knocked: it has a velocity); taken by a hand, the hand has it. Armed meanwhile (VR_Gib_Think2's
// job), so that it can be shot off. One that was a rigid body (thrown) settles first: the rigid bodies' last step
// moves it a little after it stuck (vr_gib_stuckpos), and that is not a knock.
void() VR_Gib_StuckThink2 =
{
    if(self.carry_player == world && self.movetype == MOVETYPE_NONE && time < self.vr_gib_stuckat + 0.25)
    {
        self.velocity = '0 0 0';
        self.avelocity = '0 0 0';
        self.vr_rigid = 0;
        if(self.origin != self.vr_gib_stuckpos)
        {
            setorigin(self, self.vr_gib_stuckpos);
        }
    }
    if(self.carry_player != world || self.movetype != MOVETYPE_NONE)
    {
        self.vr_gib_stuck = 0;
    }
    else if(time >= self.vr_gib_stuck || self.velocity != '0 0 0')
    {
        dprint(sprintf("gib: falls at %v (%s)%s", self.origin, self.velocity != '0 0 0' ? vtos(self.velocity) : "its time", "\n"));
        self.vr_gib_stuck = 0;
        self.movetype = MOVETYPE_BOUNCE; // falls; a rigid body again from its landing (VR_Gib_Touch)
        if(self.velocity == '0 0 0')
        {
            self.velocity = '0 0 -20';
        }
        setorigin(self, self.origin);
    }

    if(VR_Gib_IsDestroyable(self) && !self.takedamage && time >= self.vr_gib_armtime && self.carry_player == world)
    {
        self.takedamage = DAMAGE_YES;
    }
    if(self.vr_gib_stuck)
    {
        self.nextthink2 = time + 0.1;
    }
    else
    {
        VR_Gore_Handover();
    }
};

// The damage of a hand or gun striking a gib at `xSpeed` (m/s; vr_melee_speed or more): the base of
// a punch at that speed up to three times it.
float(float xSpeed) VR_Gib_StrikeDamage =
{
    float threshold = max(0.1, cvar_hget(cvarh_vr_melee_speed));
    return 20 * min(3, xSpeed / threshold) * cvar_hget(cvarh_vr_melee_dmg_multiplier);
};

// A melee blow of `xHand` of `self` (vr_melee.qc: its fist or what it holds swept into it, a blow's speed `xSpeed`, m/s)
// landing on a loose gib or head (on the floor, flying): struck as one held in the other hand is (VR_Gib_HeldFrameHand),
// at the blow's speed, weaker short of stamina (VR_Blow_Exhaust, vr_strike_stamina); it bursts.
float(float xHand) VR_Blow_Exhaust;
void(float xHand, entity xGib, float xSpeed, vector xAt, vector xDir) VR_Gib_Blow =
{
    dprint("gib: struck by a blow at "); dprint(ftos(xSpeed)); dprint("\n");
    VRSetMeleeHitSoundPlayed(xHand, TRUE);
    VRMeleeHapticByHand(xHand);
    VR_SoundVaried(self, VRGetGunChannel(xHand), "fisthit.wav", 1.0, ATTN_NORM);
    VR_Blunt_HitLayer(self, VRGetHandChannel(xHand), xGib);
    VR_SetHitKind(QVR_HITKIND_MELEE, xHand, xAt, xDir);
    float sgib = xGib.vr_sgib;
    float sgibId = xGib.vr_sgib_id;
    float grace = VR_SmallGib_BlowSpared(xGib);
    VR_Gib_Hit(xGib, self, VR_Gib_StrikeDamage(xSpeed) * VR_Blow_Exhaust(xHand));
    VR_ClearHitKind();
    if(sgib > 0 && cvar_hget(cvarh_vr_smallgibs_trace))
    {
        bprint(sprintf("sgibtrace: #%g %.3f s: struck by a blow at %.2f m/s%s: %s\n", sgibId, time - sgib, xSpeed,
            grace ? " in its blow grace" : "", xGib.vr_gib ? "not burst" : "burst"));
    }
};

// A hand or a gun striking it at a swing's speed (vr_melee_speed). Not by the hand that just let go
// of it, nor by either hand just after a two-handed throw (VR_Throw_SparedBlows, VR_Throw_HandMayStrike): a throw's
// follow-through is not a blow. Its speed is the hand's against the gib's (at most the hand's own): a hand moving along
// with a gib it just threw, or the other hand alongside, doesn't strike it (NOTES.md vrfiringrange_2026-10-02_15-38-26).
void(entity xPlayer, float xHand, entity xGib) VR_Gib_Struck =
{
    vector handVel = VRGetEntHandVel(xPlayer, xHand);
    float speed = min(vlen(handVel), vlen(handVel - xGib.velocity * (1 / VRMetersToUnits())));
    if(speed < max(0.1, cvar_hget(cvarh_vr_melee_speed)) || !VR_Gib_IsDestroyable(xGib) ||
       (!xGib.takedamage && !VR_SmallGib_BlowBursts(xGib)) ||
       (time < xGib.vr_gib_releasetime + 0.4 && xHand == xGib.vr_gib_releasehand) ||
       (xGib.carry_letgo == xPlayer && time < xGib.carry_letgo_time + cvar_hget(cvarh_vr_gib_letgo_spare)) ||
       VR_Throw_SparedBlows(xGib, xPlayer) || !VR_Throw_HandMayStrike(xPlayer, xHand))
    {
        return;
    }
    dprint(sprintf("gib: struck by the %s hand's touch at %.2f m/s, %.2f s after it was let go of (by the %s hand)\n",
        xHand == cVR_MainHand ? "main" : "off", speed, time - xGib.vr_gib_releasetime,
        xGib.vr_gib_releasehand == cVR_MainHand ? "main" : "off"));
    VR_Gib_Hit(xGib, xPlayer, VR_Gib_StrikeDamage(speed));
};

// ----------------------------------------------------------------------------
// A gib or head held in one hand can be shot by the gun in the other (FireBullets, the lightning
// gun, the player's missiles) or struck by the other hand, its fist or its weapon (VR_Gib_HeldFrame).
// Held, it is not solid, and it is the player's (the player's own traces pass it): hit, it is
// knocked out of the hand first, and takes the hit as a loose one (it bursts, or flies off).

float vr_slab_t0; // VR_SegmentBox's working
float vr_slab_t1;

float(float xA, float xD, float xLo, float xHi) VR_Slab =
{
    if(fabs(xD) < 0.0001)
    {
        return xA >= xLo && xA <= xHi;
    }
    float u = (xLo - xA) / xD;
    float v = (xHi - xA) / xD;
    if(u > v)
    {
        float t = u;
        u = v;
        v = t;
    }
    vr_slab_t0 = max(vr_slab_t0, u);
    vr_slab_t1 = min(vr_slab_t1, v);
    return vr_slab_t0 <= vr_slab_t1;
};

// Where along the line from `xA` to `xB` (0 .. 1) it enters the box `xLo` .. `xHi` (-1: it misses).
float(vector xA, vector xB, vector xLo, vector xHi) VR_SegmentBox =
{
    vector d = xB - xA;
    vr_slab_t0 = 0;
    vr_slab_t1 = 1;
    if(!VR_Slab(xA_x, d_x, xLo_x, xHi_x) || !VR_Slab(xA_y, d_y, xLo_y, xHi_y) || !VR_Slab(xA_z, d_z, xLo_z, xHi_z))
    {
        return -1;
    }
    return vr_slab_t0;
};

// The gib or head `xPlayer` holds in the hand other than `xHand` (the one firing or striking), if it
// can be destroyed; else world.
entity(entity xPlayer, float xHand) VR_Gib_HeldByOther =
{
    entity g = VR_Carry_Held(xPlayer, xHand == cVR_OffHand ? cVR_MainHand : cVR_OffHand);
    if(g == world || !g.vr_gib || g.carry_player != xPlayer || !VR_Gib_IsDestroyable(g) || VR_Carry_Held(xPlayer, xHand) == g)
    {
        return world;
    }
    return g;
};

// Where the line from `xA` to `xB` enters `xGib`'s box grown by `xPad` (0 .. 1; -1: it misses).
float(entity xGib, vector xA, vector xB, float xPad) VR_Gib_OnLine =
{
    vector pad = '1 1 1' * xPad;
    return VR_SegmentBox(xA, xB, xGib.origin + xGib.mins - pad, xGib.origin + xGib.maxs + pad);
};

// Out of the hand, to take a hit: loose, the player's no more, and armed.
entity vr_gib_knocked; // the last knocked out (VR_Gib_KnockOut),
float vr_gib_knocktime; // and when

void(entity xPlayer, entity xGib) VR_Gib_KnockOut =
{
    vr_gib_knocked = xGib;
    vr_gib_knocktime = time;
    if(VR_Carry_Held(xPlayer, cVR_MainHand) == xGib)
    {
        VR_Carry_SetHeld(xPlayer, cVR_MainHand, world);
    }
    if(VR_Carry_Held(xPlayer, cVR_OffHand) == xGib)
    {
        VR_Carry_SetHeld(xPlayer, cVR_OffHand, world);
    }
    VR_Carry_Loose(xGib);
    xGib.velocity = '0 0 0';
    xGib.owner = world;
    xGib.throwhit = QVR_THROWHIT_HIT;
    xGib.takedamage = DAMAGE_YES;
    xGib.vr_gib_releasetime = 0;
};

// After a traceline of `self` (a player) from `xA` to `xB`, firing with vr_hithand: a gib or head held
// in the other hand in the way (before trace_fraction) is what the shot hits, knocked out of the hand
// (the shots after it hit it as a loose one).
void(vector xA, vector xB) VR_Gib_TraceHeld =
{
    if(!(self.flags & FL_CLIENT) || vr_hithand < 0)
    {
        return;
    }
    entity g = VR_Gib_HeldByOther(self, vr_hithand);
    if(g == world && vr_gib_knocked.vr_gib && vr_gib_knocktime == time && vr_gib_knocked.carry_player == world)
    {
        g = vr_gib_knocked; // knocked out by this shot's first pellets: the rest hit it too (they start in it)
    }
    if(g == world)
    {
        return;
    }
    float t = VR_Gib_OnLine(g, xA, xB, 1);
    if(t < 0 || t >= trace_fraction)
    {
        return;
    }
    dprint("gib: shot in the other hand\n");
    VR_Gib_KnockOut(self, g);
    trace_ent = g;
    trace_fraction = t;
    trace_endpos = xA + (xB - xA) * t;
    trace_plane_normal = normalize(xA - xB);
    trace_allsolid = FALSE;
    trace_startsolid = FALSE;
};

// Once a frame (PlayerVRMelee, self is the player): a gib or head held in one hand struck by the
// other -- its fist or its weapon, moving at a swing's speed (vr_melee_speed) towards it -- or hit by
// one of the player's missiles (which pass it: it is the player's).
void(float xHand) VR_Gib_HeldFrameHand =
{
    entity g = VR_Gib_HeldByOther(self, xHand);
    if(g == world)
    {
        return;
    }
    float holder = xHand == cVR_OffHand ? cVR_MainHand : cVR_OffHand;
    float m2u = VRMetersToUnits();

    // The striking hand's sweep this frame (its fist), or its weapon (from the hand to the muzzle).
    // (Not the hand that just let go of it, passing it to this one; nor the hand that let go first of a gib held in
    // both, while the other may yet throw it with both (VR_Throw_HandMayStrike): a two-handed throw's hands never let
    // go in the same frame, and that hand, inside a large gib's box and a little faster or slower than the other, struck
    // it at their difference, 3-4 m/s: it burst in front of him. NOTES.md vrfiringrange_2026-10-03_02-02-13.)
    vector rel = VRGetHandVel(xHand) - VRGetHandVel(holder);
    float speed = vlen(rel);
    if(speed >= max(0.1, cvar_hget(cvarh_vr_melee_speed)) &&
       !(time < g.vr_gib_releasetime + 0.4 && xHand == g.vr_gib_releasehand) && VR_Throw_HandMayStrike(self, xHand))
    {
        vector hand = VRGetHandPos(xHand);
        float t = VR_Gib_OnLine(g, hand - rel * (m2u * max(frametime, 0.011)), hand, 2.5);
        if(t < 0 && VRGetEntWeapon(self, xHand) != WID_FIST)
        {
            t = VR_Gib_OnLine(g, hand, VRGetMuzzlePos(xHand), 1);
        }
        if(t >= 0)
        {
            dprint("gib: struck by the other hand at "); dprint(ftos(speed)); dprint("\n");
            VR_Gib_KnockOut(self, g);
            VRSetMeleeHitSoundPlayed(xHand, TRUE); // the blow landed (no punch as well)
            VRMeleeHapticByHand(xHand);
            VR_SoundVaried(self, VRGetGunChannel(xHand), "fisthit.wav", 1.0, ATTN_NORM);
            VR_Blunt_HitLayer(self, VRGetHandChannel(xHand), g);
            VR_SetHitKind(QVR_HITKIND_MELEE, xHand, (g.absmin + g.absmax) * 0.5, normalize(rel));
            T_Damage(g, self, self, VR_Gib_StrikeDamage(speed));
            VR_ClearHitKind();
            return;
        }
    }

    // The player's missiles: where each went this frame.
    vector mid = g.origin + (g.mins + g.maxs) * 0.5;
    entity e = findradius(mid, 96);
    while(e)
    {
        if(e.owner == self && e.touch && e != g && (e.solid == SOLID_BBOX || e.solid == SOLID_SLIDEBOX) &&
           (e.movetype == MOVETYPE_FLYMISSILE || e.movetype == MOVETYPE_BOUNCEMISSILE || e.movetype == MOVETYPE_FLY))
        {
            vector from = e.origin - e.velocity * max(frametime, 0.011);
            float at = VR_Gib_OnLine(g, from, e.origin, max(1, e.maxs_x));
            if(at >= 0)
            {
                dprint("gib: "); dprint(e.classname); dprint(" through the other hand's\n");
                VR_Gib_KnockOut(self, g);
                setorigin(e, from + (e.origin - from) * at);
                entity oldSelf = self;
                entity oldOther = other;
                self = e;
                other = g;
                e.touch();
                self = oldSelf;
                other = oldOther;
                return;
            }
        }
        e = e.chain;
    }
};

void() VR_Gib_HeldFrame =
{
    if(!cvar_hget(cvarh_vr_gib_destroy) || self.health <= 0)
    {
        return;
    }
    VR_Gib_HeldFrameHand(cVR_MainHand);
    VR_Gib_HeldFrameHand(cVR_OffHand);
};

// From MakeItemGrabbable: the box's pickup moves to carry_use; its hand and gun touches carry.
void(entity xBox) VR_Carry_Setup =
{
    if(xBox.handtouch != VR_Carry_Handtouch)
    {
        xBox.carry_use = xBox.handtouch;
    }
    xBox.handtouch = VR_Carry_Handtouch;
    xBox.vr_wpntouch = VR_Carry_Weapontouch;
    xBox.vr_rigid = 1;
};

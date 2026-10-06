// vr_limbs.qc -- limb gore (docs/vr-port/ROUND21.md, "Limb gore"; the engine's vr_box3d.cpp "Limb gore",
// vr_limbmodel.cpp). Every limb of a monster with a ragdoll rig as its head is (vr_decap.qc): a blade cuts it off, a
// bullet, a blunt blow or a bolt pops it, by the head's chance rules (each weapon's, times vr_limbs_chance_scale;
// Quad Damage always). On the living only the killing blow: no death animation, the ragdoll at once with
// vr_limbs_body_speed of the blow's knock; on corpses and ragdolls every limb on its own (vr_limbs_corpses), down to the
// torso. The limb's joint is the rig's (ragdolllimb: the bone struck, then the nearest of its joint and its children's:
// the elbow takes the forearm and the hand, the shoulder the whole arm). The piece cut flies off as a gib of its own
// (its model made from the monster's: limbmodel; grabbable, rigid, destroyable; at most vr_limbs_max lie about); a pop
// bursts it (blood, small gibs; brains only for the head). The stump bleeds and spurts as the neck does
// (VR_Decap_FountainThink). A zombie or mummy losing a limb dies for good (vr_limbs_zombies). Explosions pop the limbs
// near them by chance (vr_limbs_blast); a body gibbed throws its own limbs (vr_gib_limbs).
//
// The arming is the head's (vr_decap.qc's globals): vr_decap_limb the joint (-1 the head). The public hooks
// (VR_Decap_Blow, _Saw, ...) try the head first, then the limb struck.

float vr_limb_cuts;      // limbs cut off (the tests)
float vr_limb_pops;      // limbs popped (the tests)
entity vr_limb_last;     // the last limb thrown (the tests)
// vr_limb_blast_at, _by, _time (combat.qc): the explosion being dealt (T_RadiusDamage).
.float vr_limb_bone;     // a stump's fountain: its joint (-1 the head); a limb thrown: its joint
.vector vr_limb_end;     // a piece's own fountain (VR_Limb_EndFountain): its spout, the cut end, in the piece's model space
.float vr_limb_born;     // ... the piece's vr_gib_made (it is that piece still)
float vr_limb_endspurts; // the pieces' fountains' drops thrown (the tests)
.float vr_limb_gibbed;   // when it was gibbed throwing its own limbs (VR_Limb_GibAll; ThrowGib's meat gibs not then: vr_gib_limbs 2)
.float vr_limb_bserial;  // the blast its pellets below are of (vr_sgib_blastserial)
.float vr_limb_bdmg[16]; // the blast's damage at each limb (by its joint)
.float vr_limb_btotal;   // the blast's damage on it in all
.vector vr_limb_bat;     // the limb pellets' points summed
.float vr_limb_bn;       // and how many

float VR_LIMB_POP_SIZE = 1.0; // a popped limb's burst (a head's is VR_DECAP_POP_SIZE)

float(float xBone) VR_Limb_Bit =
{
    return 1 << xBone;
};

// A zombie or a mummy: a lost limb kills it for good (vr_limbs_zombies).
float(entity e) VR_Limb_IsZombie =
{
    return e.classname == "monster_zombie" || e.classname == "monster_mummy";
};

// Whether `e` may lose limbs: the option, a monster with a rig and limbs left (not a statue, not a gib).
float(entity e) VR_Limb_Allowed =
{
    if(!cvar_hget(cvarh_vr_limbs) || e.vr_gib || !(e.flags & FL_MONSTER))
    {
        return FALSE;
    }
    if((e.classname == "monster_knight" || e.classname == "monster_hell_knight") && (e.spawnflags & 2))
    {
        return FALSE; // (a statue: stone)
    }
    if(VR_Limb_IsZombie(e) && !cvar_hget(cvarh_vr_limbs_zombies))
    {
        return FALSE;
    }
    return ragdolllimbs(e, 0) != 0;
};

// The limb a hit at `xAt` on `e` strikes: its joint; -1 none (the torso, the head: Decapitation's, no rig).
float(entity e, vector xAt) VR_Limb_At =
{
    float b = ragdolllimb(e, xAt);
    if(b < 0 || b == ragdolllimbs(e, 3))
    {
        return -1;
    }
    return b;
};

// A blow of `xHow` about to strike `targ`'s limb at joint `xBone`: armed, Killed cuts it off (or pops it).
float(entity targ, entity attacker, vector xAt, vector xVel, float xHow, float xBone) VR_Limb_Arm =
{
    if(xBone < 0 || !VR_Limb_Allowed(targ) || !VR_Decap_HowOn(xHow) ||
       (targ.health <= 0 && (!VR_Decap_IsPop(xHow) || !cvar_hget(cvarh_vr_limbs_corpses))))
    {
        return FALSE;
    }
    vr_decap_ent = targ;
    vr_decap_by = attacker;
    vr_decap_time = time;
    vr_decap_vel = xVel;
    vr_decap_at = xAt;
    vr_decap_how = xHow;
    vr_decap_chance = 1;
    vr_decap_limb = xBone;
    vr_decap_why = "";
    return TRUE;
};

// The cut limb thrown: its model `m` at `xAt` turned `xAng`, flying at `xVel`, spinning at `xSpin` (rad/s): a gib (as
// the head's, VR_Decap_ThrowHead), its joint `xBone`. Room made under vr_limbs_max first.
entity(entity c, string m, float xBone, vector xAt, vector xAng, vector xVel, vector xSpin) VR_Limb_Throw =
{
    VR_Limb_MakeRoom();
    precache_model(m); // (made as it loads: the engine's vr_limbmodel.cpp; precached late, as the dummy's enemies)
    entity h = spawn();
    h.classname = "vr_limb";
    setmodel(h, m);
    h.frame = 0;
    h.skin = c.skin;
    h.model_scale = c.model_scale;
    setorigin(h, xAt);
    h.angles = xAng;
    h.movetype = MOVETYPE_BOUNCE;
    h.takedamage = DAMAGE_NO;
    h.solid = SOLID_NOT;
    h.velocity = xVel;
    h.avelocity = '0 0 0';
    h.flags = 0;
    h.think = SUB_Remove;
    h.nextthink = time + 10;
    h.vr_limb_bone = xBone;
    VR_MakeGibGrabbable(h);
    VR_Gib_MakeDestroyable(h, 1.2);
    VR_SmallGib_CarryMults(c, h); // (vr_smallgibs.qc) it carries its enemy's gib counts: bursting it later
    VR_Gore_WatchFling(h); // (it may stick to a ceiling or a wall)
    h.vr_rigid = 1;
    h.vr_spin = xSpin;
    VR_Limb_SetMass(h, c, xBone);
    VR_Limb_EndFountain(h, c, xBone, FALSE);
    particle2(xAt, xVel * 0.3, QVR_PARTICLE_PRESET_BLOOD, 20);
    vr_limb_last = h;
    return h;
};

// Piece `h` (a limb or the head) cut off `c` at joint `xBone` (-1 the head) weighs its share of c's ragdoll's Mass by
// the kind of limb (limbpiece; the engine's limbMassShare: an arm 6.3%, a forearm and hand 2.8, a hand 0.8, a leg 15.5,
// a shin and foot 6, the head 7) times vr_limbs_mass_scale (Box3D's: .vr_prop_mass; held and thrown as that). No rig,
// or vr_limbs_mass_scale 0: its model's hull of flesh, as before.
void(entity h, entity c, float xBone) VR_Limb_SetMass =
{
    float scale = cvar_hget(cvarh_vr_limbs_mass_scale);
    vector piece = limbpiece(c, xBone, 0);
    if(piece_x > 0 && scale > 0)
    {
        h.vr_prop_mass = max(0.05, piece_x * scale);
    }
};

// Piece `h`'s fountain (VR_Limb_EndFountain; self): its cut end spurts blood as it flies, smaller than the stump's,
// pulsing and dying away over vr_limbs_end_fountain s; gone with the piece.
void() VR_Limb_EndFountainThink =
{
    entity h = self.owner;
    float age = time - self.ltime;
    if(age > self.wait || h.model == "" || h.model != self.netname || h.vr_gib_made != self.vr_limb_born)
    {
        remove(self);
        return;
    }
    self.nextthink = time + VR_DECAP_SPURT_TICK;
    vector off = modelpoint(h, h.angles, self.vr_limb_end);
    vector out = normalize(off);
    float fade = 1 - age / max(0.1, self.wait);
    float beat = 0.55 + 0.45 * cos(age * VR_DECAP_BEAT * 2 * 3.14159265);
    float s = fade * beat;
    vector jitter;
    jitter_x = crandom() * 10;
    jitter_y = crandom() * 10;
    jitter_z = crandom() * 6;
    float n = 1 + ceil(2 * s);
    particle2(h.origin + off + out * 0.5, out * (2 + 4 * s) + jitter * 0.03, QVR_PARTICLE_PRESET_BLOODTRAIL, n);
    vr_limb_endspurts = vr_limb_endspurts + n;
};

// Piece `h` just cut off `c` at joint `xBone` (-1 the head; `xHead` it is: its gib's model, not the rig's) spurts blood
// out of its cut end as it flies (vr_limbs_end_fountain s; 0 none). A limb's model is made from the rig: its end is
// the joint (limbpiece 1). The head's gib model has a space of its own: its end is where the neck is as it is cut (the
// stump's), in the gib's turned axes (modelpoint's, undone); far off (no stump), from its middle the rig's way to the
// neck out to its box's side.
void(entity h, entity c, float xBone, float xHead) VR_Limb_EndFountain =
{
    float len = cvar_hget(cvarh_vr_limbs_end_fountain);
    vector end = limbpiece(c, xBone, 1);
    if(len <= 0 || end == '0 0 0' || h.model == "")
    {
        return;
    }
    vector neck = ragdollcut(c, 4, xBone) - h.origin;
    if(xHead && vlen(neck) < 32)
    {
        vector ax = modelpoint(h, h.angles, '1 0 0');
        vector ay = modelpoint(h, h.angles, '0 1 0');
        vector az = modelpoint(h, h.angles, '0 0 1');
        float k2 = max(0.0001, ax * ax);
        end_x = (neck * ax) / k2;
        end_y = (neck * ay) / k2;
        end_z = (neck * az) / k2;
    }
    else if(xHead)
    {
        vector lo = modelbounds(h, 0);
        vector hi = modelbounds(h, 1);
        vector half = (hi - lo) * 0.5;
        vector d = normalize(end);
        float t = 1000;
        if(fabs(d_x) > 0.001) { t = min(t, half_x / fabs(d_x)); }
        if(fabs(d_y) > 0.001) { t = min(t, half_y / fabs(d_y)); }
        if(fabs(d_z) > 0.001) { t = min(t, half_z / fabs(d_z)); }
        end = (lo + hi) * 0.5 + d * t;
    }
    entity f = spawn();
    f.classname = "vr_limb_fountain";
    f.owner = h;
    f.enemy = c;
    f.vr_limb_bone = xBone;
    f.netname = h.model;
    f.vr_limb_born = h.vr_gib_made;
    f.vr_limb_end = end;
    f.ltime = time;
    f.wait = len;
    f.think = VR_Limb_EndFountainThink;
    f.nextthink = time;
    // (The tests: how far its spout is from the stump's joint as it is cut.)
    f.dest = h.origin + modelpoint(h, h.angles, end);
    f.dmg = vlen(f.dest - ragdollcut(c, 4, xBone));
};

// Room for one more limb under vr_limbs_max: the oldest lying about go (not one in a hand).
void() VR_Limb_MakeRoom =
{
    float most = max(1, cvar_hget(cvarh_vr_limbs_max));
    float n = 0;
    entity e;
    for(e = find(world, classname, "vr_limb"); e; e = find(e, classname, "vr_limb"))
    {
        n++;
    }
    while(n >= most)
    {
        entity oldest = world;
        for(e = find(world, classname, "vr_limb"); e; e = find(e, classname, "vr_limb"))
        {
            if(e.carry_player == world && (oldest == world || e.vr_gib_made < oldest.vr_gib_made))
            {
                oldest = e;
            }
        }
        if(oldest == world)
        {
            return;
        }
        dprint(sprintf("limbs: the oldest limb removed (the most is %g)\n", most));
        oldest.classname = "vr_limb_gone";
        oldest.nextthink2 = 0;
        remove(oldest);
        n--;
    }
};

// The stump of `xBone` on `e` (just cut: ragdollcut knows it): blood painted round it, the cut's squish, a burst of
// blood and a few small gibs (not after a pop: its burst is there), then its fountain.
void(entity e, float xBone, vector xVel, float xPop) VR_Limb_Stump =
{
    vector at = ragdollcut(e, 4, xBone);
    vector out = ragdollcut(e, 5, xBone);
    vector side;
    side_x = out_y;
    side_y = -out_x;
    side_z = 0;
    side = (vlen(side) > 0.1) ? normalize(side) : '1 0 0';
    vector side2;
    side2_x = out_y * side_z - out_z * side_y;
    side2_y = out_z * side_x - out_x * side_z;
    side2_z = out_x * side_y - out_y * side_x;
    woundevent(e, at + out * 3, out * -1, QVR_WOUND_MELEE, 50, 0);
    woundevent(e, at + side * 3, side * -1, QVR_WOUND_MELEE, 40, 0);
    woundevent(e, at - side * 3, side, QVR_WOUND_MELEE, 40, 0);
    woundevent(e, at + side2 * 3, side2 * -1, QVR_WOUND_MELEE, 40, 0);
    woundevent(e, at - side2 * 3, side2, QVR_WOUND_MELEE, 40, 0);
    if(!xPop)
    {
        sound(e, CHAN_AUTO, VR_Gib_SquishSound(FALSE), 1, ATTN_NORM);
        VR_BloodMist(at, xVel, 0.5);
        VR_Gore_Burst(at, out, 0.4);
        if(cvar_hget(cvarh_vr_smallgibs))
        {
            for(float i = 0; i < 2; i++)
            {
                vector v = out * (50 + random() * 50) + normalize(xVel) * 40 + side * (crandom() * 40) + '0 0 40';
                VR_SmallGib_Tag(VR_SmallGib_Spawn(at + out * 2, v, e), QVR_SGIB_SIT_MELEE);
            }
        }
    }
    float len = cvar_hget(cvarh_vr_decap_fountain);
    if(len > 0)
    {
        entity f = spawn();
        f.classname = "vr_decap_fountain";
        f.owner = e;
        f.ltime = time;
        f.wait = len * 0.8;
        f.cnt = e.modelindex;
        f.vr_limb_bone = xBone;
        f.think = VR_Decap_FountainThink;
        f.nextthink = time;
    }
};

// A limb popped at `xAt` by a blow going `xVel`: as a gib destroyed (VR_Gib_Burst): blood, the gore's burst, small gibs
// (no brains), a squish.
void(entity e, vector xAt, vector xVel) VR_Limb_PopEffects =
{
    VR_BloodMist(xAt, xVel, VR_LIMB_POP_SIZE);
    VR_Gore_Burst(xAt, xVel, VR_LIMB_POP_SIZE);
    VR_SmallGib_Burst(xAt, xVel, VR_LIMB_POP_SIZE, e);
    sound(e, CHAN_AUTO, VR_Gib_SquishSound(FALSE), 1, ATTN_NORM);
    vr_limb_pops++;
};

// Cuts `e`'s limb at joint `xBone` off (a dead monster: just killed, dying, a corpse or a ragdoll): its ragdoll made now
// if it has none, the piece thrown (a cut) or burst (a pop). `xLive`: the killing blow on a live one (the body keeps
// vr_limbs_body_speed of its knock). FALSE if it can't (ragdolls off, no rig, cut already).
float(entity e, entity by, vector xVel, float xHow, float xBone, float xLive) VR_Limb_Cut =
{
    float oldSolid = e.solid;
    if(e.solid != SOLID_NOT && e.solid != SOLID_NOT_BUT_TOUCHABLE)
    {
        e.solid = SOLID_NOT; // (a dead monster to the engine: its ragdoll)
    }
    float pop = VR_Decap_IsPop(xHow);
    float settle = xLive ? max(0, cvar_hget(cvarh_vr_limbs_body_speed)) : 1;
    if(pop && !xLive)
    {
        settle = max(0, cvar_hget(cvarh_vr_decap_pop_body_speed)) * 0.5 + 0.5;
    }
    if(!ragdollcutlimb(e, xBone, xVel, settle))
    {
        e.solid = oldSolid;
        dprint(sprintf("limbs: no: %s's limb %g can't be cut (Ragdolls off? cut already?)\n", e.classname, xBone));
        return FALSE;
    }
    vector mid = ragdollcut(e, 0);
    if(pop)
    {
        VR_Limb_PopEffects(e, mid, xVel);
        dprint(sprintf("limbs: %s's limb %g popped by %s (%s)\n", e.classname, xBone, by.classname, VR_Decap_HowName(xHow)));
    }
    else
    {
        string m = limbmodel(e, xBone);
        if(m != "")
        {
            VR_Limb_Throw(e, m, xBone, mid, ragdollcut(e, 1), ragdollcut(e, 2), ragdollcut(e, 3));
        }
        vr_limb_cuts++;
        dprint(sprintf("limbs: %s's limb %g cut off by %s (%s at %.1f m/s): %s at %.0f u/s\n", e.classname, xBone, by.classname,
            VR_Decap_HowName(xHow), vlen(xVel) / VRMetersToUnits(), m, vlen(ragdollcut(e, 2))));
    }
    VR_Limb_Stump(e, xBone, xVel, pop);
    return TRUE;
};

// Killed, the armed blow at a limb (VR_Decap_Killed; self is `e`): its roll (times vr_limbs_chance_scale), then cut. It
// dies whole; a zombie or a mummy for good.
void(entity e) VR_Limb_Killed =
{
    if(!VR_Decap_Roll(e) || !VR_Limb_Cut(e, vr_decap_by, vr_decap_vel, vr_decap_how, vr_decap_limb, TRUE))
    {
        return;
    }
    if(e.health < -1)
    {
        e.health = -1;
    }
    if(VR_Limb_IsZombie(e))
    {
        e.th_die = VR_Decap_ZombieDie;
    }
};

// ----------------------------------------------------------------------------
// The blows at a limb (the head's hooks' second half: vr_decap.qc's VR_Decap_Head* first).

// A hand's blow about to deal its damage to live `targ` (T_Damage_VRMelee; self the player, vr_hithand the hand): armed if
// it strikes a limb as a cut (a blade's slash, the chainsaw) or a blunt blow (by chance), or with Quad Damage.
float(entity targ, entity attacker, float xDamage) VR_Limb_Blow =
{
    if(vr_hitkind != QVR_HITKIND_MELEE || vr_hithand < 0 || attacker != self || !(self.flags & FL_CLIENT) ||
       targ.health <= 0 || !VR_Limb_Allowed(targ))
    {
        return FALSE;
    }
    entity b = VR_Blow(vr_hithand);
    float bone = VR_Limb_At(targ, b.mh_c_pos);
    if(bone < 0)
    {
        return FALSE;
    }
    float zombieLight = VR_Limb_IsZombie(targ) && xDamage < VR_DECAP_ZOMBIE_DOWN;
    if(b.mh_kind == VR_MKIND_POMMEL || b.mh_wid == WID_MJOLNIR || b.mh_thing == VR_MTHING_FIST || b.mh_thing == VR_MTHING_GUN ||
       b.mh_thing == VR_MTHING_CROWBAR || b.mh_thing == VR_MTHING_CLUB)
    {
        if(zombieLight || !VR_Limb_Arm(targ, attacker, b.mh_c_pos, VRGetHandVel(vr_hithand) * VRMetersToUnits(), QVR_DECAP_BLUNT, bone))
        {
            return FALSE;
        }
        float bc = VR_Decap_BluntClass(b, b.mh_thing);
        vr_decap_chance = VR_Decap_BluntChance(bc, b.mh_speed, xDamage);
        vr_decap_why = sprintf("%s %.1f m/s %.0f damage at limb %g", VR_Decap_BluntName(bc), b.mh_speed, xDamage, bone);
        return TRUE;
    }
    if(b.mh_thing != VR_MTHING_SWORD && b.mh_thing != VR_MTHING_AXE && b.mh_thing != VR_MTHING_SAW)
    {
        return FALSE;
    }
    vr_decap_quiet = TRUE;
    float how = VR_Decap_Slash(vr_hithand, b.mh_c_pos, TRUE);
    vr_decap_quiet = FALSE;
    if(how && VR_Limb_Arm(targ, attacker, b.mh_c_pos, vr_decap_slashvel, how, bone))
    {
        return TRUE;
    }
    if(!how)
    {
        dprint(sprintf("limbs: no: %s at limb %g of %s\n", vr_decap_no, bone, targ.classname));
    }
    if(VR_Decap_QuadPops(attacker) && !zombieLight)
    {
        return VR_Limb_Arm(targ, attacker, b.mh_c_pos, VRGetHandVel(vr_hithand) * VRMetersToUnits(), QVR_DECAP_QUAD, bone);
    }
    return FALSE;
};

// A hand's blow at corpse `c` (VR_Corpse_StrikeFrame; self the player) at `xAt` on a limb: a blade's slash cuts it off, a
// blunt blow pops it by chance, instead of taking its corpse health. TRUE if it did.
float(entity c, float xHand, vector xAt) VR_Limb_CorpseBlow =
{
    if(!cvar_hget(cvarh_vr_limbs_corpses) || !VR_Limb_Allowed(c))
    {
        return FALSE;
    }
    float bone = VR_Limb_At(c, xAt);
    if(bone < 0)
    {
        return FALSE;
    }
    entity b = VR_Blow(xHand);
    float thing = VR_Melee_Thing(self, xHand);
    vr_decap_limb = bone;
    if((b.mh_thing == thing && (b.mh_kind == VR_MKIND_POMMEL || b.mh_wid == WID_MJOLNIR)) || thing == VR_MTHING_FIST || thing == VR_MTHING_GUN ||
       thing == VR_MTHING_CROWBAR || thing == VR_MTHING_CLUB)
    {
        float speed = vlen(VRGetHandVel(xHand));
        float dmg = VR_Gib_StrikeDamage(speed) * (VRIsHandWeaponless(self, xHand) ? 1 : 1.5);
        float bc = VR_Decap_BluntClass(b, thing);
        vr_decap_by = self;
        vr_decap_chance = VR_Decap_BluntChance(bc, speed, dmg);
        vr_decap_how = QVR_DECAP_BLUNT;
        vr_decap_why = sprintf("%s at a corpse's limb %.1f m/s %.0f damage", VR_Decap_BluntName(bc), speed, dmg);
        if(!VR_Decap_Roll(c))
        {
            return FALSE;
        }
        return VR_Limb_Cut(c, self, VRGetHandVel(xHand) * VRMetersToUnits(), QVR_DECAP_BLUNT, bone, FALSE);
    }
    if(b.mh_thing != VR_MTHING_SWORD && b.mh_thing != VR_MTHING_AXE && b.mh_thing != VR_MTHING_SAW)
    {
        return FALSE;
    }
    vr_decap_quiet = TRUE;
    float how = VR_Decap_Slash(xHand, xAt, FALSE);
    vr_decap_quiet = FALSE;
    if(!how || !VR_Decap_HowOn(how))
    {
        return FALSE;
    }
    vr_decap_by = self;
    vr_decap_chance = 1;
    vr_decap_how = how;
    vr_decap_why = "a slash at a corpse's limb";
    return VR_Decap_Roll(c) && VR_Limb_Cut(c, self, vr_decap_slashvel, how, bone, FALSE);
};

// The running chainsaw's bar in `e` at `xAt` on a limb (VR_Decap_Saw's second half): 2 a corpse's cut off, 1 a live one
// armed, 0 neither.
float(entity e, float xHand, vector xAt) VR_Limb_Saw =
{
    if(!cvar_hget(cvarh_vr_decap_chainsaw) || !VR_Limb_Allowed(e))
    {
        return 0;
    }
    float bone = VR_Limb_At(e, xAt);
    if(bone < 0)
    {
        return 0;
    }
    entity b = VR_Blow(xHand);
    vector vel = VR_Blow_ToWorld(VR_Melee_PointVel(b, 1)) * VRMetersToUnits();
    if(e.health <= 0 || e.vr_corpse == 2 || e.vr_corpse_dying)
    {
        if(!cvar_hget(cvarh_vr_limbs_corpses))
        {
            return 0;
        }
        vr_decap_limb = bone;
        vr_decap_by = self;
        vr_decap_chance = 1;
        vr_decap_how = QVR_DECAP_SAW;
        return (VR_Decap_Roll(e) && VR_Limb_Cut(e, self, vel, QVR_DECAP_SAW, bone, FALSE)) ? 2 : 0;
    }
    return VR_Limb_Arm(e, self, xAt, vel, QVR_DECAP_SAW, bone) ? 1 : 0;
};

// A thrown axe `xAxe` gone into `m` edge first at a limb (VR_Decap_Thrown's second half): armed if fast enough; a monster
// its hit box killed a moment ago cut at once.
float(entity m, entity xAxe) VR_Limb_Thrown =
{
    if(!cvar_hget(cvarh_vr_decap_thrown) || !VR_Limb_Allowed(m))
    {
        return FALSE;
    }
    vector vel = xAxe.velocity;
    vector at = xAxe.vr_stick_at;
    float bone = VR_Limb_At(m, at);
    if(bone < 0 || vlen(vel) / VRMetersToUnits() < cvar_hget(cvarh_vr_decap_speed))
    {
        return FALSE;
    }
    entity by = VR_Throw_Thrower(xAxe);
    if(m.health > 0)
    {
        return VR_Limb_Arm(m, by, at, vel, QVR_DECAP_THROWN, bone);
    }
    if(time - m.vr_decap_died < VR_DECAP_DIED_LATE)
    {
        VR_Limb_Cut(m, by, vel, QVR_DECAP_THROWN, bone, TRUE);
    }
    return FALSE;
};

// A pellet of the player's blast of `xDamage` striking `e` at `xAt` (VR_Decap_Pellet): the blast's damage at each limb.
void(entity e, float xDamage, vector xAt, vector xDir) VR_Limb_Pellet =
{
    if(!vr_decap_weapon || !vr_sgib_blast || !VR_Limb_Allowed(e))
    {
        return;
    }
    if(e.vr_limb_bserial != vr_sgib_blastserial)
    {
        e.vr_limb_bserial = vr_sgib_blastserial;
        for(float i = 0; i < 16; i++)
        {
            e.vr_limb_bdmg[i] = 0;
        }
        e.vr_limb_btotal = 0;
        e.vr_limb_bat = '0 0 0';
        e.vr_limb_bn = 0;
    }
    e.vr_limb_btotal = e.vr_limb_btotal + xDamage;
    float bone = VR_Limb_At(e, xAt);
    if(bone >= 0 && bone < 16)
    {
        e.vr_limb_bdmg[bone] = e.vr_limb_bdmg[bone] + xDamage;
        e.vr_limb_bat = e.vr_limb_bat + xAt;
        e.vr_limb_bn = e.vr_limb_bn + 1;
    }
};

// A blast's damage about to be dealt to `targ` (VR_Decap_BlastArm's second half; self the shooter): armed on the limb with
// most of the blast's pellets if at least vr_decap_head_share of it is there.
float(entity targ) VR_Limb_BlastArm =
{
    if(!vr_decap_weapon || !vr_sgib_blast || !(self.flags & FL_CLIENT) || targ.vr_limb_bserial != vr_sgib_blastserial ||
       targ.vr_limb_bn <= 0)
    {
        return FALSE;
    }
    float best = -1;
    float bestDmg = 0;
    for(float i = 0; i < 16; i++)
    {
        if(targ.vr_limb_bdmg[i] > bestDmg)
        {
            bestDmg = targ.vr_limb_bdmg[i];
            best = i;
        }
    }
    float share = bestDmg / max(0.001, targ.vr_limb_btotal);
    float least = min(1, max(0, cvar_hget(cvarh_vr_decap_head_share)));
    if(best < 0 || (share < least && !VR_Decap_QuadPops(self)))
    {
        return FALSE;
    }
    if(VR_Limb_IsZombie(targ) && targ.vr_limb_btotal < VR_DECAP_ZOMBIE_DOWN)
    {
        return FALSE;
    }
    vector at = targ.vr_limb_bat * (1 / targ.vr_limb_bn);
    vector dir = normalize(at - vr_decap_src);
    if(!VR_Limb_Arm(targ, self, at, dir * VR_DECAP_SHOT_SPEED, vr_decap_weapon, best))
    {
        return FALSE;
    }
    vr_decap_chance = VR_Decap_PopChance(vr_decap_weapon, vlen(at - vr_decap_src), share);
    vr_decap_why = sprintf("%.0f%% of the blast at limb %g", share * 100, best);
    return TRUE;
};

// The player's lightning bolt about to deal `xDamage` to `targ` at `xAt` (VR_Decap_BoltArm's second half): armed if at a
// limb.
float(entity targ, vector xAt, vector xDir, float xDamage) VR_Limb_BoltArm =
{
    if(vr_decap_weapon != QVR_DECAP_LIGHTNING || !(self.flags & FL_CLIENT) || !targ.takedamage || !VR_Limb_Allowed(targ))
    {
        return FALSE;
    }
    float bone = VR_Limb_At(targ, xAt);
    if(bone < 0 || (VR_Limb_IsZombie(targ) && xDamage < VR_DECAP_ZOMBIE_DOWN))
    {
        return FALSE;
    }
    if(!VR_Limb_Arm(targ, self, xAt, normalize(xDir) * VR_DECAP_SHOT_SPEED, QVR_DECAP_LIGHTNING, bone))
    {
        return FALSE;
    }
    vr_decap_chance = VR_Decap_PopChance(QVR_DECAP_LIGHTNING, vlen(xAt - (self.origin + self.view_ofs)), 1);
    vr_decap_why = "a bolt at a limb";
    return TRUE;
};

// A thrown or flung thing about to strike `targ` (VR_Decap_ThrownArm's second half): armed if blunt and at a limb, as the
// head's (its mass).
float(entity targ, entity xThing, entity by, float xDamage) VR_Limb_ThrownArm =
{
    if(!(by.flags & FL_CLIENT) || targ.health <= 0 || !VR_Limb_Allowed(targ) || (vr_decap_ent == targ && vr_decap_time == time))
    {
        return FALSE;
    }
    float quad = VR_Decap_QuadPops(by);
    if(xThing.classname == "thrown_weapon" && !quad &&
       (xThing.weapon == WID_SWORD || xThing.weapon == WID_AXE || xThing.weapon == WID_CHAINSAW))
    {
        return FALSE;
    }
    float bone = VR_Limb_At(targ, xThing.origin);
    if(bone < 0 || (VR_Limb_IsZombie(targ) && xDamage < VR_DECAP_ZOMBIE_DOWN))
    {
        return FALSE;
    }
    float mass = VR_Thrown_Mass(xThing);
    float chance = (mass >= cvar_hget(cvarh_vr_decap_pop_thrown_mass) || quad) ? 1 :
                   min(1, max(0, cvar_hget(cvarh_vr_decap_pop_thrown_light_chance)));
    if(chance <= 0 || !VR_Limb_Arm(targ, by, xThing.origin, xThing.velocity, QVR_DECAP_THROWN_BLUNT, bone))
    {
        return FALSE;
    }
    vr_decap_chance = chance;
    vr_decap_why = sprintf("%.1f kg at limb %g", mass, bone);
    return TRUE;
};

// Any other blow with Quad Damage (VR_Decap_QuadArm's second half): armed if it strikes a limb.
float(entity targ, entity inflictor, entity attacker, float xDamage) VR_Limb_QuadArm =
{
    if(!VR_Decap_QuadPops(attacker) || vr_decap_weapon || targ.health <= 0 || !targ.takedamage || !VR_Limb_Allowed(targ) ||
       (vr_decap_ent == targ && vr_decap_time == time))
    {
        return FALSE;
    }
    vector at;
    vector dir = inflictor.velocity;
    if(vr_hitpart_target == targ && vr_hitpart_time == time)
    {
        at = vr_hitpart_pos;
        if(inflictor == attacker || vlen(dir) < 1)
        {
            dir = at - (attacker.origin + attacker.view_ofs);
        }
    }
    else if(inflictor != attacker && inflictor != world)
    {
        at = inflictor.origin;
    }
    else
    {
        return FALSE;
    }
    float bone = VR_Limb_At(targ, at);
    if(bone < 0 || (VR_Limb_IsZombie(targ) && xDamage * 4 < VR_DECAP_ZOMBIE_DOWN))
    {
        return FALSE;
    }
    if(!VR_Limb_Arm(targ, attacker, at, normalize(dir) * VR_DECAP_SHOT_SPEED, QVR_DECAP_QUAD, bone))
    {
        return FALSE;
    }
    vr_decap_why = sprintf("%s at limb %g", inflictor.classname, bone);
    return TRUE;
};

// ----------------------------------------------------------------------------
// Explosions (vr_limbs_blast) and gibbing (vr_gib_limbs).

// The limb joints of `e` hanging off its torso (or off the head: none), not cut: what a whole body comes apart into.
float(entity e) VR_Limb_TopJoints =
{
    float all = ragdolllimbs(e, 0);
    float top = 0;
    for(float b = 1; b < 16; b++)
    {
        float bit = VR_Limb_Bit(b);
        if((all & bit) && !(all & VR_Limb_Bit(ragdolllimbs(e, 5, b))))
        {
            top = top | bit;
        }
    }
    return top;
};

// An explosion's hit on `e` (dead now: killed by it, or a corpse): its limbs near the blast (vr_limbs_blast_radius) pop by
// chance (vr_limbs_blast_chance at the blast, falling to 0 at the radius; times vr_limbs_chance_scale; Quad Damage
// always). How many popped.
float(entity e, float xLive) VR_Limb_BlastPops =
{
    if(!cvar_hget(cvarh_vr_limbs_blast) || vr_limb_blast_time != time || !VR_Limb_Allowed(e) ||
       (!xLive && !cvar_hget(cvarh_vr_limbs_corpses)))
    {
        return 0;
    }
    float radius = max(1, cvar_hget(cvarh_vr_limbs_blast_radius));
    float scale = max(0, cvar_hget(cvarh_vr_limbs_chance_scale));
    float quad = VR_Decap_QuadPops(vr_limb_blast_by);
    float top = VR_Limb_TopJoints(e);
    float n = 0;
    for(float b = 1; b < 16; b++)
    {
        if(!(top & VR_Limb_Bit(b)))
        {
            continue;
        }
        vector at = limbplace(e, b, 0);
        float d = vlen(at - vr_limb_blast_at);
        float c = quad ? 1 : min(1, cvar_hget(cvarh_vr_limbs_blast_chance) * max(0, 1 - d / radius) * scale);
        if(c <= 0)
        {
            continue;
        }
        float r = cvar_hget(cvarh_vr_decap_pop_roll);
        if(r < 0)
        {
            r = random();
        }
        dprint(sprintf("limbs: blast at %.0f units from %s's limb %g: chance %.2f, rolled %.2f\n", d, e.classname, b, c, r));
        if(r >= c)
        {
            continue;
        }
        vector vel = normalize(at - vr_limb_blast_at) * 300;
        if(VR_Limb_Cut(e, vr_limb_blast_by, vel, QVR_DECAP_BLAST, b, xLive))
        {
            n++;
        }
    }
    return n;
};

// Killed, not by an armed blow (VR_Decap_Killed): by an explosion, its limbs near it may pop; if any did, it dies whole (a
// ragdoll, not gibbed; a zombie or mummy for good).
void(entity e) VR_Limb_BlastKilled =
{
    if(vr_hitkind != QVR_HITKIND_SPLASH || VR_Limb_BlastPops(e, TRUE) <= 0)
    {
        return;
    }
    if(e.health < -1)
    {
        e.health = -1;
    }
    if(VR_Limb_IsZombie(e))
    {
        e.th_die = VR_Decap_ZombieDie;
    }
};

// ThrowHead (player.qc; self the monster gibbed): its own limbs thrown with the gibs (vr_gib_limbs 1 or 2), from where they
// are, flung as gibs are (VelocityForDamage of `dm`).
void(float dm) VR_Limb_GibAll =
{
    if(!cvar_hget(cvarh_vr_gib_limbs) || !cvar_hget(cvarh_vr_limbs) || !(self.flags & FL_MONSTER) || self.vr_gib)
    {
        return;
    }
    float top = VR_Limb_TopJoints(self);
    if(!top)
    {
        return;
    }
    self.vr_limb_gibbed = time;
    for(float b = 1; b < 16; b++)
    {
        if(!(top & VR_Limb_Bit(b)))
        {
            continue;
        }
        string m = limbmodel(self, b);
        if(m == "")
        {
            continue;
        }
        vector vel = VR_Gore_Fling(VelocityForDamage(dm)) + limbplace(self, b, 2);
        vector spin;
        spin_x = crandom() * 8;
        spin_y = crandom() * 8;
        spin_z = crandom() * 8;
        VR_Limb_Throw(self, m, b, limbplace(self, b, 0), limbplace(self, b, 1), vel, spin);
        vr_limb_cuts++;
    }
};

// ThrowGib (player.qc; self the monster gibbed): FALSE when its limbs take the meat gibs' place (vr_gib_limbs 2).
float() VR_Limb_GibsWanted =
{
    if(cvar_hget(cvarh_vr_gib_limbs) != 2 || !cvar_hget(cvarh_vr_limbs))
    {
        return TRUE;
    }
    if(self.vr_limb_gibbed == time)
    {
        return FALSE;
    }
    return !((self.flags & FL_MONSTER) && !self.vr_gib && VR_Limb_TopJoints(self));
};

// ----------------------------------------------------------------------------
// The hooks (vr_decap.qc's prototypes, combat.qc's): the head first (VR_Decap_Head*), then the limb struck.

float(entity targ, entity attacker, float xDamage) VR_Decap_Blow =
{
    if(VR_Decap_HeadBlow(targ, attacker, xDamage))
    {
        return TRUE;
    }
    return VR_Limb_Blow(targ, attacker, xDamage);
};

float(entity c, float xHand, vector xAt) VR_Decap_CorpseBlow =
{
    if(VR_Decap_HeadCorpseBlow(c, xHand, xAt))
    {
        return TRUE;
    }
    return VR_Limb_CorpseBlow(c, xHand, xAt);
};

float(entity e, float xHand, vector xAt) VR_Decap_Saw =
{
    float r = VR_Decap_HeadSaw(e, xHand, xAt);
    if(r)
    {
        return r;
    }
    return VR_Limb_Saw(e, xHand, xAt);
};

float(entity m, entity xAxe) VR_Decap_Thrown =
{
    if(VR_Decap_HeadThrown(m, xAxe))
    {
        return TRUE;
    }
    return VR_Limb_Thrown(m, xAxe);
};

void(entity e, float xDamage, vector xAt, vector xDir) VR_Decap_Pellet =
{
    VR_Decap_HeadPellet(e, xDamage, xAt, xDir);
    VR_Limb_Pellet(e, xDamage, xAt, xDir);
};

float(entity targ) VR_Decap_BlastArm =
{
    if(VR_Decap_HeadBlastArm(targ))
    {
        return TRUE;
    }
    return VR_Limb_BlastArm(targ);
};

float(entity targ, vector xAt, vector xDir, float xDamage) VR_Decap_BoltArm =
{
    if(VR_Decap_HeadBoltArm(targ, xAt, xDir, xDamage))
    {
        return TRUE;
    }
    return VR_Limb_BoltArm(targ, xAt, xDir, xDamage);
};

// An enforcer's laser bolt (an enemy's: Laser_Touch, any monster's at another; or your enforcer's rifle:
// VR_RifleLaser_Touch) about to deal `xDamage` to `targ` for `attacker`, striking it at `xAt` going `xDir`: armed if
// it strikes a living monster's head or a limb (VR_Decap_OnHead, VR_Limb_At: on its model as it is animated), the kill
// popping it at vr_decap_pop_laser (times Head Chance or Limb Chance; a zombie: only a blow that knocks it down).
float(entity targ, entity attacker, vector xAt, vector xDir, float xDamage) VR_Decap_LaserArm =
{
    float chance = min(1, max(0, cvar_hget(cvarh_vr_decap_pop_laser)));
    if(chance <= 0 || targ.health <= 0 || !targ.takedamage || !(targ.flags & FL_MONSTER) || targ == attacker)
    {
        return FALSE;
    }
    xDir = normalize(xDir);
    if(VR_Decap_Allowed(targ) && VR_Decap_OnHead(targ, xAt, xDir))
    {
        if((targ.classname == "monster_zombie" && xDamage < VR_DECAP_ZOMBIE_DOWN) ||
           !VR_Decap_Arm(targ, attacker, xAt, xDir * VR_DECAP_SHOT_SPEED, QVR_DECAP_LASER))
        {
            return FALSE;
        }
        vr_decap_chance = chance;
        vr_decap_why = "a laser bolt";
        return TRUE;
    }
    if(!VR_Limb_Allowed(targ))
    {
        return FALSE;
    }
    float bone = VR_Limb_At(targ, xAt);
    if(bone < 0 || (VR_Limb_IsZombie(targ) && xDamage < VR_DECAP_ZOMBIE_DOWN) ||
       !VR_Limb_Arm(targ, attacker, xAt, xDir * VR_DECAP_SHOT_SPEED, QVR_DECAP_LASER, bone))
    {
        dprint(sprintf("limbs: no: a laser bolt at %s at %v, its limb %g\n", targ.classname, xAt, bone));
        return FALSE;
    }
    vr_decap_chance = chance;
    vr_decap_why = "a laser bolt at a limb";
    return TRUE;
};

float(entity targ, entity xThing, entity by, float xDamage) VR_Decap_ThrownArm =
{
    if(VR_Decap_HeadThrownArm(targ, xThing, by, xDamage))
    {
        return TRUE;
    }
    return VR_Limb_ThrownArm(targ, xThing, by, xDamage);
};

float(entity targ, entity inflictor, entity attacker, float xDamage) VR_Decap_QuadArm =
{
    if(VR_Decap_HeadQuadArm(targ, inflictor, attacker, xDamage))
    {
        return TRUE;
    }
    return VR_Limb_QuadArm(targ, inflictor, attacker, xDamage);
};

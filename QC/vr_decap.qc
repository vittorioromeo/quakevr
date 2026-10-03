// vr_decap.qc -- decapitation (docs/vr-port/ROUND21.md, "Decapitation"; the engine's vr_box3d.cpp, "Decapitation").
//
// A killing blow at a live monster's head (positional damage's head zone: PositionalHead) by a bladed slash cuts its
// head off: a sword's or an axe's blade (not its hilt, handle or pommel) swung across its line (VR_DECAP_ACROSS: a blade
// driven along its line is a stab) at vr_decap_speed or more, an axe thrown into it edge first (vr_axestick.cpp decides
// the edge), or the chainsaw (its running bar at the neck, or a swing of its bar). The monster skips its death animation:
// its ragdoll is made at once from the frame it is in (the engine: ragdolldecap), keeping its motion and the blow's
// knock, without its head; the head flies off as its own gib (VR_Decap_ThrowHead: from where the head was, turned as it
// was, launched by the blade and spun by it); the stump bleeds (a wound round the neck, a squish, small gibs, a burst
// and a fountain of blood: VR_Decap_FountainThink). A headless corpse is a ragdoll as any other; gibbed, it throws no
// head (.vr_headless: VR_Corpse_Gib; kept in a saved game, the engine makes its ragdoll headless again).
// A slash at a ragdoll's head (or a dying body's) beheads it the same way instead of taking its corpse health
// (vr_decap_corpses; VR_Decap_CorpseBlow, from VR_Corpse_StrikeFrame). A zombie's head cut off kills it for good,
// whatever the damage (vr_decap_zombies; its ragdoll: vr_ragdoll.cpp zombieSeeds).
// Only monsters with a ragdoll rig (vr_ragdoll.cpp seedTables: their bone named "head") and a head gib here
// (VR_Decap_HeadModel) are beheaded; the rest die as before. Ragdolls off: nothing.

float VR_DECAP_ACROSS = 0.5;       // cosine: the struck point moving within 60 degrees of the blade's line is a stab, not a slash
float VR_DECAP_BLADE_SWORD = 0.4;  // the blade proper: from this share of the way from the grip to the tip (VR_MELEE_BLADE_FROM)
float VR_DECAP_BLADE_AXE = 0.6;    // an axe's head (25..37 units up its 40)
float VR_DECAP_DIED_LATE = 0.3;    // s: a thrown axe's blade going in this soon after its hit box killed the monster still beheads it
float VR_DECAP_SPURT_TICK = 0.05;  // s between the fountain's spurts
float VR_DECAP_BEAT = 2.2;         // Hz: its pulses

float QVR_DECAP_SWING = 1;  // a sword or an axe swung
float QVR_DECAP_THROWN = 2; // an axe thrown
float QVR_DECAP_SAW = 3;    // the chainsaw
// Head pops (below: "Head pops"; combat.qc: QVR_DECAP_SHOTGUN 4, _SSG 5, _LIGHTNING 6): a shot kills with a headshot.

.float vr_decap_died; // when it was killed (Killed: VR_Decap_Killed)

// The blow that would behead (VR_Decap_Arm), while its damage is dealt: Killed (VR_Decap_Killed) cuts the head off if
// it kills; VR_Decap_After ends it (a zombie: beheaded whatever).
entity vr_decap_ent;
entity vr_decap_by;
float vr_decap_time;
vector vr_decap_vel; // the blade's velocity at the cut (world, units/s)
vector vr_decap_at;
float vr_decap_how;
vector vr_decap_slashvel; // VR_Decap_Slash's answer: the struck point's velocity (world, units/s)
float vr_decap_count;     // heads cut off (the tests)
entity vr_decap_lasthead; // the last head thrown (the tests)
float vr_decap_pops;      // heads popped (the tests)
float VR_DECAP_POP_SIZE = 1.5; // a popped head's burst: a head gib's (VR_Decap_ThrowHead: VR_Gib_MakeDestroyable 1.5)

void() VR_Decap_ZombieDie;
float(entity e, entity by, vector xVel, float xHow) VR_Decap_Cut;

string(float xHow) VR_Decap_HowName =
{
    if(xHow == QVR_DECAP_THROWN) { return "a thrown axe"; }
    if(xHow == QVR_DECAP_SAW) { return "the chainsaw"; }
    if(xHow == QVR_DECAP_SHOTGUN) { return "a shotgun blast"; }
    if(xHow == QVR_DECAP_SSG) { return "a super shotgun blast"; }
    if(xHow == QVR_DECAP_LIGHTNING) { return "a lightning bolt"; }
    return "a slash";
};

// The head gib `e` throws ("": none, it is never beheaded). A new monster with a ragdoll rig: its head here.
string(entity e) VR_Decap_HeadModel =
{
    string c = e.classname;
    if((c == "monster_knight" || c == "monster_hell_knight") && (e.spawnflags & 2))
    {
        return ""; // (a statue: stone, VR_Corpse_Parts)
    }
    if(c == "monster_army")          { return "progs/h_guard.mdl"; }
    if(c == "monster_knight")        { return "progs/h_knight.mdl"; }
    if(c == "monster_ogre" || c == "monster_ogre_marksman") { return "progs/h_ogre.mdl"; }
    if(c == "monster_enforcer")      { return "progs/h_mega.mdl"; }
    if(c == "monster_hell_knight")   { return "progs/h_hellkn.mdl"; }
    if(c == "monster_dog")           { return "progs/h_dog.mdl"; }
    if(c == "monster_wizard")        { return "progs/h_wizard.mdl"; }
    if(c == "monster_zombie")        { return "progs/h_zombie.mdl"; }
    return "";
};

// Whether `e` may lose its head (the option, a monster with a head gib, not headless already; a zombie: vr_decap_zombies).
float(entity e) VR_Decap_Allowed =
{
    if(!cvar_hget(cvarh_vr_decap) || e.vr_headless || e.vr_gib || !(e.flags & FL_MONSTER) || VR_Decap_HeadModel(e) == "")
    {
        return FALSE;
    }
    if(e.classname == "monster_zombie" && !cvar_hget(cvarh_vr_decap_zombies))
    {
        return FALSE;
    }
    return TRUE;
};

// Whether a blow of `xHow` may behead (vr_decap_thrown, vr_decap_chainsaw; the head pops' vr_decap_shotgun,
// vr_decap_super_shotgun, vr_decap_lightning).
float(float xHow) VR_Decap_HowOn =
{
    if(xHow == QVR_DECAP_THROWN) { return cvar_hget(cvarh_vr_decap_thrown); }
    if(xHow == QVR_DECAP_SAW) { return cvar_hget(cvarh_vr_decap_chainsaw); }
    if(xHow == QVR_DECAP_SHOTGUN) { return cvar_hget(cvarh_vr_decap_shotgun); }
    if(xHow == QVR_DECAP_SSG) { return cvar_hget(cvarh_vr_decap_super_shotgun); }
    if(xHow == QVR_DECAP_LIGHTNING) { return cvar_hget(cvarh_vr_decap_lightning); }
    return TRUE;
};

// Whether `xHow` pops the head (a shot's) rather than cutting it off.
float(float xHow) VR_Decap_IsPop =
{
    return xHow == QVR_DECAP_SHOTGUN || xHow == QVR_DECAP_SSG || xHow == QVR_DECAP_LIGHTNING;
};

// Whether a hit at `xAt` going `xDir` on `e` is at its head: positional damage's head zone (PositionalHead, weapons.qc:
// on its model standing, vr_hit_precise; else the ray past its head sphere), whether positional damage is on or not. A
// rottweiler has none for headshots: its head here (its rest pose's, $attack1: lunging).
float(entity e, vector xAt, vector xDir) VR_Decap_OnHead =
{
    if(e.classname == "monster_dog")
    {
        xHeadForward = 23;
        xHeadUp = 1;
        xHeadRadius = 7;
    }
    else if(!PositionalHead(e))
    {
        return FALSE;
    }
    float yaw = e.angles_y * (3.14159265 / 180);
    vector facing;
    facing_x = cos(yaw);
    facing_y = sin(yaw);
    facing_z = 0;
    vector head = e.origin + facing * xHeadForward + '0 0 1' * xHeadUp;
    if(hitmodel_target(e))
    {
        return vlen(hitmodel_rest(e, xAt) - head) < xHeadRadius + VR_HEAD_MARGIN;
    }
    vector d = normalize(xDir);
    if(d == '0 0 0')
    {
        return vlen(xAt - head) < xHeadRadius;
    }
    float t = max(0, (head - xAt) * d);
    return vlen(head - (xAt + d * t)) < xHeadRadius;
};

// Whether what hand `xHand` of `self` holds strikes `xAt` as a beheading cut: a sword's or an axe's blade swung across
// its line at vr_decap_speed (its struck point's speed), or the chainsaw's bar. QVR_DECAP_SWING or _SAW (the struck
// point's velocity in vr_decap_slashvel), 0 if not. `xLive`: the hand's blow (VR_Melee_Decide's kind: a slash, not a
// stab or a pommel strike); else a corpse's (VR_Corpse_StrikeFrame: the blade's place and motion only).
float(float xHand, vector xAt, float xLive) VR_Decap_Slash =
{
    entity b = VR_Blow(xHand);
    float thing = b.mh_thing;
    if(b.mh_free || (thing != VR_MTHING_SWORD && thing != VR_MTHING_AXE && thing != VR_MTHING_SAW) ||
       (thing == VR_MTHING_AXE && b.mh_wid != WID_AXE))
    {
        return 0; // (held off its handle: its pommel, hilt or handle struck with; Mjolnir: a hammer, not a blade)
    }
    vector line = b.mh_far - b.mh_grip;
    float len = vlen(line);
    if(len < 1)
    {
        return 0;
    }
    float f = ((xAt - b.mh_grip) * line) / (len * len); // (the grip 0, the tip 1, the pommel below 0)
    float how = QVR_DECAP_SWING;
    string no = "";
    if(thing == VR_MTHING_SAW)
    {
        how = QVR_DECAP_SAW;
        if(f < VR_SAW_BAR_START || (xLive && b.mh_kind == VR_MKIND_POMMEL))
        {
            no = "the chainsaw's body or handle";
        }
    }
    else if(xLive && b.mh_kind != VR_MKIND_SLASH)
    {
        no = (b.mh_kind == VR_MKIND_STAB) ? "a stab" : "a pommel or hilt";
    }
    else if(f < ((thing == VR_MTHING_AXE) ? VR_DECAP_BLADE_AXE : VR_DECAP_BLADE_SWORD))
    {
        no = (f < 0) ? "the pommel" : "the hilt or handle";
    }
    vector vp = VR_Melee_PointVel(b, min(1, f));
    float speed = vlen(vp);
    if(no == "" && how != QVR_DECAP_SAW)
    {
        vector axis = normalize(b.mh_rfar - b.mh_rgrip);
        if(speed < cvar_hget(cvarh_vr_decap_speed))
        {
            no = sprintf("too slow (%.1f m/s)", speed);
        }
        else if(fabs(vp * axis) > speed * VR_DECAP_ACROSS)
        {
            no = sprintf("along the blade, a stab (%.2f)", fabs(vp * axis) / speed);
        }
    }
    if(no != "")
    {
        dprint(sprintf("decap: no: %s at the head (%.2f along the blade)\n", no, f));
        return 0;
    }
    vr_decap_slashvel = VR_Blow_ToWorld(vp) * VRMetersToUnits();
    return how;
};

// A blow of `xHow` about to strike `targ` (alive) at its head with the blade at `xVel`: armed, Killed beheads it.
float(entity targ, entity attacker, vector xAt, vector xVel, float xHow) VR_Decap_Arm =
{
    if(!VR_Decap_Allowed(targ) || !VR_Decap_HowOn(xHow) || targ.health <= 0)
    {
        return FALSE;
    }
    vr_decap_ent = targ;
    vr_decap_by = attacker;
    vr_decap_time = time;
    vr_decap_vel = xVel;
    vr_decap_at = xAt;
    vr_decap_how = xHow;
    return TRUE;
};

// A hand's blow about to deal its damage to `targ` (T_Damage_VRMelee; self the player, vr_hithand the hand): armed if
// it is a beheading cut at a live monster's head.
float(entity targ, entity attacker) VR_Decap_Blow =
{
    if(vr_hitkind != QVR_HITKIND_MELEE || vr_hithand < 0 || attacker != self || !(self.flags & FL_CLIENT) ||
       targ.health <= 0 || !VR_Decap_Allowed(targ))
    {
        return FALSE;
    }
    entity b = VR_Blow(vr_hithand);
    if(b.mh_thing != VR_MTHING_SWORD && b.mh_thing != VR_MTHING_AXE && b.mh_thing != VR_MTHING_SAW)
    {
        return FALSE;
    }
    if(!VR_Decap_OnHead(targ, b.mh_c_pos, b.mh_c_dir))
    {
        return FALSE;
    }
    float how = VR_Decap_Slash(vr_hithand, b.mh_c_pos, TRUE);
    return how && VR_Decap_Arm(targ, attacker, b.mh_c_pos, vr_decap_slashvel, how);
};

// The armed blow's damage dealt: a zombie it didn't kill is beheaded all the same (it dies for good); the blow is done.
void(entity targ) VR_Decap_After =
{
    if(vr_decap_ent != targ)
    {
        return;
    }
    if(vr_decap_time == time && targ.classname == "monster_zombie" && targ.health > 0 && !targ.vr_gib &&
       (targ.flags & FL_MONSTER))
    {
        // (Cut first: a zombie that can't be beheaded, ragdolls off, isn't killed by it; Killed then cuts no more.)
        entity by = vr_decap_by;
        float oldHealth = targ.health;
        vr_decap_ent = world;
        targ.health = -1;
        if(!VR_Decap_Cut(targ, by, vr_decap_vel, vr_decap_how))
        {
            targ.health = oldHealth;
            return;
        }
        targ.th_die = VR_Decap_ZombieDie;
        Killed(targ, by);
    }
    vr_decap_ent = world;
};

// The head gib: `m` at `xAt` turned `xAng`, flying at `xVel` and spinning at `xSpin` (rad/s), as a thrown head
// (ThrowHead, player.qc) of its own; a rigid body from the start (as a thrown gib's flight: Box3D turns it as it spins,
// at any angle; it passes through its body until clear of it).
entity(entity c, string m, vector xAt, vector xAng, vector xVel, vector xSpin) VR_Decap_ThrowHead =
{
    entity h = spawn();
    h.classname = c.classname; // (as the head a gibbing throws: the monster itself)
    setmodel(h, m);
    h.frame = 0;
    h.model_scale = c.model_scale;
    setorigin(h, xAt);
    h.angles = xAng;
    h.movetype = MOVETYPE_BOUNCE;
    h.takedamage = DAMAGE_NO;
    h.solid = SOLID_NOT;
    h.view_ofs = '0 0 8';
    setsize(h, '-10 -10 0', '10 10 10');
    h.velocity = xVel;
    h.avelocity = '0 0 0';
    h.flags = 0;
    h.gorging = TRUE;
    h.think = HeadThink;
    h.nextthink = time + 1.5;
    VR_MakeGibGrabbable(h);
    VR_Gib_MakeDestroyable(h, 1.5);
    VR_Gore_WatchFling(h); // (it may stick to a ceiling or a wall)
    h.vr_rigid = 1;
    h.vr_spin = xSpin;
    particle2(xAt, xVel * 0.3, QVR_PARTICLE_PRESET_BLOOD, 30);
    vr_decap_lasthead = h;
    return h;
};

// The stump's fountain (self; owner its body): spurts of blood out of the neck, pulsing and dying away over
// vr_decap_fountain s; the drops fall and mark the floor (the particles' decals).
void() VR_Decap_FountainThink =
{
    entity c = self.owner;
    float age = time - self.ltime;
    if(age > self.wait || !c.vr_headless || c.modelindex != self.cnt || !isragdoll(c))
    {
        remove(self);
        return;
    }
    self.nextthink = time + VR_DECAP_SPURT_TICK;
    vector neck = ragdollcut(c, 4);
    vector out = ragdollcut(c, 5);
    float fade = 1 - age / max(0.1, self.wait);
    float beat = 0.55 + 0.45 * cos(age * VR_DECAP_BEAT * 2 * 3.14159265);
    float s = fade * beat;
    vector jitter;
    jitter_x = crandom() * 10;
    jitter_y = crandom() * 10;
    jitter_z = crandom() * 6;
    // The spurt: drops thrown out of the neck (a blood trail's: thrown along `dir`, falling; not the Blood preset, whose
    // mist cloud each time hung over the body as a red haze); and now and then a hit's spray of blood onto the floor
    // and walls round it (the gore's lines: splats and drips).
    particle2(neck + out * 1.5, out * (3 + 7 * s) + jitter * 0.04, QVR_PARTICLE_PRESET_BLOODTRAIL, 2 + ceil(4 * s));
    if(time >= self.pausetime && s > 0.3)
    {
        self.pausetime = time + 0.3;
        vector down = out;
        down_z = 0;
        down = normalize(down + jitter * 0.1 - '0 0 0.7');
        particle2(neck, down * 7, QVR_GORE_HIT, 6 + 3000); // (a blade's hit: kind 3)
    }
};

// A head popped (VR_Decap_IsPop) at `xAt` by a shot going `xVel`: as its head gib destroyed (VR_Gib_Burst: a mist of
// blood, the gore's burst, small gibs, the gib sound) and a squish.
void(entity e, vector xAt, vector xVel) VR_Decap_PopEffects =
{
    VR_BloodMist(xAt, xVel, VR_DECAP_POP_SIZE);
    VR_Gore_Burst(xAt, xVel, VR_DECAP_POP_SIZE);
    VR_SmallGib_Burst(xAt, xVel, VR_DECAP_POP_SIZE);
    sound(e, CHAN_AUTO, VR_Gib_SquishSound(FALSE), 1, ATTN_NORM);
    sound(e, CHAN_BODY, "player/udeath.wav", 0.8, ATTN_NORM);
    vr_decap_pops++;
};

// The cut's look and sound on `e` (just beheaded: ragdollcut knows the head and the stump), the blade (or the shot) at
// `xVel`, `xHow` the blow's: a head pop bursts the head where it was, and no head flies.
void(entity e, vector xVel, float xHow) VR_Decap_Effects =
{
    float pop = VR_Decap_IsPop(xHow);
    string m = VR_Decap_HeadModel(e);
    if(pop)
    {
        VR_Decap_PopEffects(e, ragdollcut(e, 0), xVel);
    }
    else if(m != "")
    {
        VR_Decap_ThrowHead(e, m, ragdollcut(e, 0), ragdollcut(e, 1), ragdollcut(e, 2), ragdollcut(e, 3));
    }

    // The stump: blood painted round the neck (the head's vertices drawn there), the cut's squish, a burst of blood and
    // a few small gibs; then its fountain.
    vector neck = ragdollcut(e, 4);
    vector out = ragdollcut(e, 5);
    vector side;
    side_x = out_y;
    side_y = -out_x;
    side_z = 0;
    side = (vlen(side) > 0.1) ? normalize(side) : '1 0 0';
    vector side2;
    side2_x = out_y * side_z - out_z * side_y;
    side2_y = out_z * side_x - out_x * side_z;
    side2_z = out_x * side_y - out_y * side_x;
    woundevent(e, neck + out * 4, out * -1, QVR_WOUND_MELEE, 60, 0);
    woundevent(e, neck + side * 4, side * -1, QVR_WOUND_MELEE, 50, 0);
    woundevent(e, neck - side * 4, side, QVR_WOUND_MELEE, 50, 0);
    woundevent(e, neck + side2 * 4, side2 * -1, QVR_WOUND_MELEE, 50, 0);
    woundevent(e, neck - side2 * 4, side2, QVR_WOUND_MELEE, 50, 0);
    if(!pop) // (a head pop's burst is there already)
    {
        sound(e, CHAN_AUTO, VR_Gib_SquishSound(FALSE), 1, ATTN_NORM);
        sound(e, CHAN_BODY, "player/udeath.wav", 0.5, ATTN_NORM);
        VR_BloodMist(neck, xVel, 0.6);
        VR_Gore_Burst(neck, out, 0.5);
    }
    if(!pop && cvar_hget(cvarh_vr_smallgibs))
    {
        for(float i = 0; i < 3; i++)
        {
            vector v = out * (60 + random() * 60) + normalize(xVel) * 40 + side * (crandom() * 40) + '0 0 40';
            VR_SmallGib_Tag(VR_SmallGib_Spawn(neck + out * 2, v, e), QVR_SGIB_SIT_MELEE);
        }
    }
    float len = cvar_hget(cvarh_vr_decap_fountain);
    if(len > 0)
    {
        entity f = spawn();
        f.classname = "vr_decap_fountain";
        f.owner = e;
        f.ltime = time;
        f.wait = len;
        f.cnt = e.modelindex;
        f.think = VR_Decap_FountainThink;
        f.nextthink = time;
    }
};

// Cuts `e`'s head off (a dead monster: just killed, dying, a corpse or a ragdoll): its ragdoll headless (made now if it
// has none), its head thrown by the blade at `xVel`. FALSE if it can't (ragdolls off, no rig).
float(entity e, entity by, vector xVel, float xHow) VR_Decap_Cut =
{
    float oldSolid = e.solid;
    if(e.solid != SOLID_NOT && e.solid != SOLID_NOT_BUT_TOUCHABLE)
    {
        e.solid = SOLID_NOT; // (a dead monster to the engine: its ragdoll)
    }
    if(!ragdolldecap(e, xVel))
    {
        e.solid = oldSolid;
        dprint(sprintf("decap: no: %s has no ragdoll to behead (Ragdolls off?)\n", e.classname));
        return FALSE;
    }
    e.vr_headless = 1; // (the engine's too)
    VR_Decap_Effects(e, xVel, xHow);
    vr_decap_count++;
    if(VR_Decap_IsPop(xHow))
    {
        dprint(sprintf("decap: %s's head popped by %s (%s)\n", e.classname, by.classname, VR_Decap_HowName(xHow)));
        return TRUE;
    }
    dprint(sprintf("decap: %s beheaded by %s (%s at %.1f m/s): its head at %.0f u/s\n", e.classname, by.classname,
        VR_Decap_HowName(xHow), vlen(xVel) / VRMetersToUnits(), vlen(ragdollcut(e, 2))));
    return TRUE;
};

// Killed, before the death code (self is `e`): the armed blow beheads it. It dies whole (its death code would gib it);
// a zombie dies for good (VR_Decap_ZombieDie).
void(entity e) VR_Decap_Killed =
{
    e.vr_decap_died = time;
    if(vr_decap_ent != e || vr_decap_time != time)
    {
        return;
    }
    vr_decap_ent = world;
    if(!VR_Decap_Cut(e, vr_decap_by, vr_decap_vel, vr_decap_how))
    {
        return;
    }
    if(e.health < -1)
    {
        e.health = -1;
    }
    if(e.classname == "monster_zombie")
    {
        e.th_die = VR_Decap_ZombieDie;
    }
};

// A beheaded zombie's death: it lies as its ragdoll does, never to get up (zombie_die gibs it).
void() VR_Decap_ZombieDie =
{
    self.think = SUB_Null;
    self.nextthink = 0;
    self.inpain = 0;
    self.solid = SOLID_NOT;
    self.deadflag = DEAD_DEAD;
};

// A hand's blow at corpse `c` (VR_Corpse_StrikeFrame; self the player) landing at `xAt`: a beheading cut at its head
// (its ragdoll's head part, or a dying body's head) cuts it off instead of taking its corpse health. TRUE if it did.
float(entity c, float xHand, vector xAt) VR_Decap_CorpseBlow =
{
    if(!cvar_hget(cvarh_vr_decap) || !cvar_hget(cvarh_vr_decap_corpses) || !VR_Decap_Allowed(c))
    {
        return FALSE;
    }
    entity b = VR_Blow(xHand);
    if(b.mh_thing != VR_MTHING_SWORD && b.mh_thing != VR_MTHING_AXE && b.mh_thing != VR_MTHING_SAW)
    {
        return FALSE;
    }
    float on = ragdollhead(c, xAt);
    if(on < 0)
    {
        on = (hitmodel_target(c) && VR_Decap_OnHead(c, xAt, VR_Blow_ToWorld(b.mh_vfar)));
    }
    if(!on)
    {
        return FALSE;
    }
    float how = VR_Decap_Slash(xHand, xAt, FALSE);
    return how && VR_Decap_HowOn(how) && VR_Decap_Cut(c, self, vr_decap_slashvel, how);
};

// The running chainsaw's bar in `e` at `xAt` (VR_Chainsaw_Cut; self the player), moving at `xVel` (units/s): 2 a
// corpse (or a dying body) beheaded (no tick's damage), 1 a live one armed (its tick's damage beheads if it kills; a
// zombie whatever: VR_Decap_After), 0 neither.
float(entity e, float xHand, vector xAt) VR_Decap_Saw =
{
    if(!cvar_hget(cvarh_vr_decap) || !cvar_hget(cvarh_vr_decap_chainsaw) || !VR_Decap_Allowed(e))
    {
        return 0;
    }
    entity b = VR_Blow(xHand);
    vector vel = VR_Blow_ToWorld(VR_Melee_PointVel(b, 1)) * VRMetersToUnits();
    vector dir = normalize(VRGetMuzzlePos(xHand) - VRGetHandPos(xHand));
    if(e.health <= 0 || e.vr_corpse == 2 || e.vr_corpse_dying)
    {
        if(!cvar_hget(cvarh_vr_decap_corpses))
        {
            return 0;
        }
        float on = ragdollhead(e, xAt);
        if(on < 0)
        {
            on = (hitmodel_target(e) && VR_Decap_OnHead(e, xAt, dir));
        }
        return (on && VR_Decap_Cut(e, self, vel, QVR_DECAP_SAW)) ? 2 : 0;
    }
    if(!VR_Decap_OnHead(e, xAt, dir))
    {
        return 0;
    }
    return VR_Decap_Arm(e, self, xAt, vel, QVR_DECAP_SAW) ? 1 : 0;
};

// A thrown axe `xAxe` gone into `m` edge first (VR_AxeStick_Stuck), before its blow: armed if at its head, fast enough
// (vr_decap_speed); a monster its hit box killed a moment ago (its blade going in after) beheaded at once.
float(entity m, entity xAxe) VR_Decap_Thrown =
{
    if(!cvar_hget(cvarh_vr_decap) || !cvar_hget(cvarh_vr_decap_thrown) || !VR_Decap_Allowed(m))
    {
        return FALSE;
    }
    vector vel = xAxe.velocity;
    vector at = xAxe.vr_stick_at;
    if(!VR_Decap_OnHead(m, at, vel))
    {
        return FALSE;
    }
    float speed = vlen(vel) / VRMetersToUnits();
    if(speed < cvar_hget(cvarh_vr_decap_speed))
    {
        dprint(sprintf("decap: no: a thrown axe too slow (%.1f m/s) at the head\n", speed));
        return FALSE;
    }
    entity by = VR_Throw_Thrower(xAxe);
    if(m.health > 0)
    {
        return VR_Decap_Arm(m, by, at, vel, QVR_DECAP_THROWN);
    }
    if(time - m.vr_decap_died < VR_DECAP_DIED_LATE)
    {
        VR_Decap_Cut(m, by, vel, QVR_DECAP_THROWN);
    }
    return FALSE;
};

// ----------------------------------------------------------------------------
// Head pops (ROUND21.md, "Head pops"): the player's shotgun or super shotgun blast, or lightning gun bolt, that kills a
// monster that can be beheaded with a headshot pops its head: no death animation and no gibbing (an overkill too), its
// ragdoll headless at once as a beheading's (VR_Decap_Cut, the stump's wounds and fountain), but no head flies: the head
// bursts where it was as its head gib destroyed would (VR_Decap_PopEffects). vr_decap_shotgun, vr_decap_super_shotgun,
// vr_decap_lightning.
// A blast counts as a whole: its pellets on him (FireBulletsImpl traces them one by one and deals a run of them on one
// target at once: ApplyMultiDamage), as far as they had struck when it killed him, their damage with the headshot
// multiplier; when at least vr_decap_head_share of it is from pellets at his head (VR_Decap_OnHead: positional damage's
// head zone, whether positional damage is on or not), it is a headshot. A bolt: the one that kills him strikes his head.
// A zombie (vr_decap_zombies): a headshot that would knock it down (VR_DECAP_ZOMBIE_DOWN, zombie_pain's) pops its head
// and it dies for good, as a slash's beheading does (VR_Decap_After).

float VR_DECAP_ZOMBIE_DOWN = 25; // a hit this hard knocks a zombie down (zombie_pain)
float VR_DECAP_SHOT_SPEED = 300; // units/s: a shot's "blade" velocity (the stump's blood, along the shot)

.float vr_decap_bserial; // the blast its pellets below are of (vr_sgib_blastserial)
.float vr_decap_bdmg;    // the blast's damage on it so far (the headshot multiplier in)
.float vr_decap_bhead;   // of which from pellets at its head
.vector vr_decap_bat;    // those pellets' points summed
.float vr_decap_bn;      // and how many
.vector vr_decap_bdir;   // every pellet's direction summed

// A pellet of the player's blast (vr_decap_weapon) of `xDamage` striking `e` at `xAt` going `xDir` (TraceAttack).
void(entity e, float xDamage, vector xAt, vector xDir) VR_Decap_Pellet =
{
    if(!vr_decap_weapon || !vr_sgib_blast || e.health <= 0 || !VR_Decap_Allowed(e))
    {
        return;
    }
    if(e.vr_decap_bserial != vr_sgib_blastserial)
    {
        e.vr_decap_bserial = vr_sgib_blastserial;
        e.vr_decap_bdmg = 0;
        e.vr_decap_bhead = 0;
        e.vr_decap_bat = '0 0 0';
        e.vr_decap_bn = 0;
        e.vr_decap_bdir = '0 0 0';
    }
    e.vr_decap_bdmg = e.vr_decap_bdmg + xDamage;
    e.vr_decap_bdir = e.vr_decap_bdir + normalize(xDir);
    if(VR_Decap_OnHead(e, xAt, xDir))
    {
        e.vr_decap_bhead = e.vr_decap_bhead + xDamage;
        e.vr_decap_bat = e.vr_decap_bat + xAt;
        e.vr_decap_bn = e.vr_decap_bn + 1;
    }
};

// A blast's damage about to be dealt to `targ` (ApplyMultiDamage; self the shooter): armed (Killed pops its head) if
// the blast on it so far is a headshot.
float(entity targ) VR_Decap_BlastArm =
{
    if(!vr_decap_weapon || !vr_sgib_blast || !(self.flags & FL_CLIENT) || targ.vr_decap_bserial != vr_sgib_blastserial ||
       targ.vr_decap_bn <= 0)
    {
        return FALSE;
    }
    float share = targ.vr_decap_bhead / max(0.001, targ.vr_decap_bdmg);
    float least = min(1, max(0, cvar_hget(cvarh_vr_decap_head_share)));
    if(share < least)
    {
        dprint(sprintf("decap: no: %s, %.2f of the blast at the head (Head Share %.2f)\n", targ.classname, share,
            least));
        return FALSE;
    }
    if(targ.classname == "monster_zombie" && targ.vr_decap_bdmg < VR_DECAP_ZOMBIE_DOWN)
    {
        return FALSE;
    }
    vector at = targ.vr_decap_bat * (1 / targ.vr_decap_bn);
    return VR_Decap_Arm(targ, self, at, normalize(targ.vr_decap_bdir) * VR_DECAP_SHOT_SPEED, vr_decap_weapon);
};

// The player's lightning bolt (vr_decap_weapon) about to deal `xDamage` to `targ`, striking it at `xAt` going `xDir`
// (LightningDamage; self the shooter): armed if it strikes its head.
float(entity targ, vector xAt, vector xDir, float xDamage) VR_Decap_BoltArm =
{
    if(vr_decap_weapon != QVR_DECAP_LIGHTNING || !(self.flags & FL_CLIENT) || !targ.takedamage || targ.health <= 0 ||
       !VR_Decap_Allowed(targ) || !VR_Decap_OnHead(targ, xAt, xDir))
    {
        return FALSE;
    }
    if(targ.classname == "monster_zombie" && xDamage < VR_DECAP_ZOMBIE_DOWN)
    {
        return FALSE;
    }
    return VR_Decap_Arm(targ, self, xAt, normalize(xDir) * VR_DECAP_SHOT_SPEED, QVR_DECAP_LIGHTNING);
};

// ----------------------------------------------------------------------------
// Tests: `vr_decap_test <n>` (Debug > Gore Tests), on the monster (or corpse) nearest you, a blow made up as the hand's
// would be (its weapon's line across the head, its motion) and dealt as the melee's (T_Damage_VRMelee) or the corpses'
// (VR_Corpse_StrikeFrame's): 1 a sword's slash (kills: its health 1 first), 2 the same along the blade (a stab), 3 its
// pommel, 4 too slow, 5 at a ragdoll's or corpse's head (a corpse's cut), 6 the chainsaw's running bar at the neck (its
// health 1), 7 a slash at full health (a zombie: beheaded whatever), 8 gibs the nearest corpse (a headless one first;
// the heads thrown counted), 9 an axe's slash, 10 the chainsaw at a corpse's head, 11 its health 1 (no blow: a thrown
// axe's test). Head pops (a shot from your eye, VR_Decap_TestShot): 12 a shotgun blast at the head that just kills, 13
// the super shotgun's, 14 the super shotgun's at health 1 (overkill), 15 a lightning bolt at the head (health 1), 16 a
// shotgun blast at the body (health 1), 17 a shotgun blast at the head at health 500 (not killed), 18 a bolt at the body
// (health 1); a zombie at its 60. Prints `decaptest:` lines: beheaded or not, the heads thrown, popped, gibbed.

// The nearest monster (live: xLive 1; a dead one: 0) to `self` within 400 units.
entity(float xLive) VR_Decap_TestTarget =
{
    entity best = world;
    float bestDist = 400;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || e.vr_gib || e.vr_headless || (xLive ? e.health <= 0 : e.health > 0))
        {
            continue;
        }
        float d = vlen(e.origin - self.origin);
        if(d < bestDist)
        {
            best = e;
            bestDist = d;
        }
    }
    return best;
};

// Hand `cVR_MainHand`'s blow made up: `xThing` held across the line from the player to `xAt` (its grip `xF` of its
// length short of it), moving `xAlong` (1: along its line) at `xSpeed` m/s, as kind `xKind`.
void(vector xAt, float xThing, float xKind, float xF, float xAlong, float xSpeed) VR_Decap_TestBlow =
{
    entity b = VR_Blow(cVR_MainHand);
    vector toward = xAt - self.origin;
    toward_z = 0;
    toward = normalize(toward);
    vector axis;
    axis_x = -toward_y;
    axis_y = toward_x;
    axis_z = 0;
    float len = 30;
    b.mh_thing = xThing;
    b.mh_wid = WID_SWORD;
    if(xThing == VR_MTHING_AXE)
    {
        b.mh_wid = WID_AXE;
    }
    else if(xThing == VR_MTHING_SAW)
    {
        b.mh_wid = WID_CHAINSAW;
    }
    b.mh_kind = xKind;
    b.mh_free = 0;
    b.mh_grip = xAt - axis * (len * xF);
    b.mh_far = b.mh_grip + axis * len;
    b.mh_near = b.mh_grip - axis * 6;
    vector eye = self.origin + self.view_ofs;
    b.mh_rgrip = VR_Blow_ToPlaySpace(b.mh_grip - eye) * (1 / VRMetersToUnits());
    b.mh_rfar = VR_Blow_ToPlaySpace(b.mh_far - eye) * (1 / VRMetersToUnits());
    b.mh_rnear = VR_Blow_ToPlaySpace(b.mh_near - eye) * (1 / VRMetersToUnits());
    vector way = normalize(toward * (1 - xAlong) + axis * xAlong);
    vector v = VR_Blow_ToPlaySpace(way) * xSpeed;
    b.mh_vgrip = v;
    b.mh_vfar = v;
    b.mh_vnear = v;
    b.mh_c_pos = xAt;
    b.mh_c_dir = way;
    b.mh_c_ent = world;
};

// A head pop's test shot from your eye at `xAt`: a blast (`xWeapon` QVR_DECAP_SHOTGUN or _SSG: its pellets, no spread)
// or a bolt (_LIGHTNING), as the weapon's (vr_decap_weapon, its Weapon Damage).
void(float xWeapon, vector xAt) VR_Decap_TestShot =
{
    vector src = self.origin + self.view_ofs;
    vector dir = normalize(xAt - src);
    VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
    vr_decap_weapon = xWeapon;
    if(xWeapon == QVR_DECAP_LIGHTNING)
    {
        self.damage_weapon = IT_LIGHTNING;
        LightningDamage(src, xAt + dir * 40, self, cvar_hget(cvarh_vr_dmg_lightning));
    }
    else
    {
        self.damage_weapon = IT_SHOTGUN;
        vr_pellet_damage = cvar_hget(xWeapon == QVR_DECAP_SSG ? cvarh_vr_dmg_super_shotgun : cvarh_vr_dmg_shotgun);
        vr_tracer_hand = cVR_MainHand;
        vr_tracer_from = src;
        FireBulletsImpl(xWeapon == QVR_DECAP_SSG ? 14 : 6, dir, '0 0 0', src, 2048, FALSE);
        vr_pellet_damage = 0;
    }
    vr_decap_weapon = 0;
    VR_ClearHitKind();
};

// A test blast's damage, all at the head (`xWeapon` as VR_Decap_TestShot's).
float(float xWeapon) VR_Decap_TestBlastDamage =
{
    float head = cvar_hget(cvarh_vr_positional_damage) ? PositionalDamageFactor(1) : 1;
    if(xWeapon == QVR_DECAP_SSG)
    {
        return 14 * cvar_hget(cvarh_vr_dmg_super_shotgun) * head;
    }
    return 6 * cvar_hget(cvarh_vr_dmg_shotgun) * head;
};

// Heads (the head gib models) in the level.
float() VR_Decap_TestHeads =
{
    float n = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_gib && e.gorging) // (a head: ThrowHead's, VR_Decap_ThrowHead's)
        {
            n++;
        }
    }
    return n;
};

void(float xTest) VR_Decap_Test =
{
    float heads = VR_Decap_TestHeads();
    float before = vr_decap_count;
    float pops = vr_decap_pops;
    entity m =VR_Decap_TestTarget((xTest != 5 && xTest != 8 && xTest != 10));
    if(xTest == 8)
    {
        // (A headless corpse first: gibbed, it throws no head.)
        for(entity hl = nextent(world); hl; hl = nextent(hl))
        {
            if(hl.vr_headless && !hl.vr_gib && hl.health <= 0 && vlen(hl.origin - self.origin) < 400)
            {
                m = hl;
            }
        }
    }
    if(m == world)
    {
        bprint(sprintf("decaptest: %g: no %s monster near\n", xTest, (xTest == 5 || xTest == 8 || xTest == 10) ? "dead" : "live"));
        return;
    }
    // Its head: the head zone's middle (a ragdoll: its head part's).
    vector at;
    if(isragdoll(m))
    {
        at = ragdollcut(m, 6);
    }
    else
    {
        if(!PositionalHead(m) && m.classname == "monster_dog")
        {
            xHeadForward = 23;
            xHeadUp = 1;
        }
        else
        {
            PositionalHead(m);
        }
        makevectors('0 1 0' * m.angles_y);
        at = m.origin + v_forward * xHeadForward + '0 0 1' * xHeadUp;
    }
    if(xTest == 11)
    {
        m.health = 1; // (no blow: the next one kills it, a thrown axe's)
        bprint(sprintf("decaptest: 11 %s at health 1\n", m.classname));
        return;
    }
    float dmg = 30;
    string what = "";
    if(xTest == 8)
    {
        what = "gibbed";
        T_Damage(m, self, self, 1000);
    }
    else if(xTest == 5)
    {
        what = "a slash at a corpse's head";
        VR_Decap_TestBlow(at, VR_MTHING_SWORD, VR_MKIND_SLASH, 0.7, 0, 8);
        if(!VR_Decap_CorpseBlow(m, cVR_MainHand, at))
        {
            VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
            T_Damage(m, self, self, dmg);
            VR_ClearHitKind();
        }
    }
    else if(xTest == 6 || xTest == 10)
    {
        what = (xTest == 6) ? "the chainsaw" : "the chainsaw at a corpse's head";
        if(xTest == 6)
        {
            m.health = 1;
        }
        VR_Decap_TestBlow(at, VR_MTHING_SAW, VR_MKIND_SLASH, 0.8, 0, 0.5);
        float saw = VR_Decap_Saw(m, cVR_MainHand, at);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
        self.damage_weapon = IT_AXE;
        if(saw != 2)
        {
            T_DamageImpl(m, self, self, 8, 1);
        }
        VR_ClearHitKind();
        if(saw == 1)
        {
            VR_Decap_After(m);
        }
    }
    else if(xTest >= 12 && xTest <= 18)
    {
        // Head pops: a shot at its head (or its body) from your eye.
        float weapon = QVR_DECAP_SHOTGUN;
        vector aim = at;
        if(xTest == 12) { what = "a shotgun headshot"; m.health = VR_Decap_TestBlastDamage(weapon) - 1; }
        else if(xTest == 13) { weapon = QVR_DECAP_SSG; what = "a super shotgun headshot"; m.health = VR_Decap_TestBlastDamage(weapon) - 1; }
        else if(xTest == 14) { weapon = QVR_DECAP_SSG; what = "a super shotgun headshot, overkill"; m.health = 1; }
        else if(xTest == 15) { weapon = QVR_DECAP_LIGHTNING; what = "a lightning bolt at the head"; m.health = 1; }
        else if(xTest == 16) { what = "a shotgun body shot"; m.health = 1; aim = m.origin + '0 0 4'; }
        else if(xTest == 17) { what = "a shotgun headshot not killing"; m.health = max(m.health, 500); }
        else { weapon = QVR_DECAP_LIGHTNING; what = "a lightning bolt at the body"; m.health = 1; aim = m.origin + '0 0 4'; }
        if(m.classname == "monster_zombie")
        {
            m.health = 60; // (a zombie: its own health, as it always is)
        }
        bprint(sprintf("decaptest: %g at health %g\n", xTest, m.health));
        VR_Decap_TestShot(weapon, aim);
    }
    else
    {
        float thing = (xTest == 9) ? VR_MTHING_AXE : VR_MTHING_SWORD;
        float kind = VR_MKIND_SLASH;
        float f = 0.7;
        float along = 0;
        float speed = 8;
        if(xTest == 2) { what = "a stab"; along = 1; }
        else if(xTest == 3) { what = "a pommel strike"; f = -0.15; kind = VR_MKIND_POMMEL; }
        else if(xTest == 4) { what = "a slow slash"; speed = max(0.2, cvar_hget(cvarh_vr_decap_speed) * 0.5); }
        else if(xTest == 7) { what = "a slash at full health"; dmg = 10; }
        else { what = (xTest == 9) ? "an axe's slash" : "a sword's slash"; f = (xTest == 9) ? 0.85 : 0.7; }
        if(xTest != 7)
        {
            m.health = 1;
        }
        VR_Decap_TestBlow(at, thing, kind, f, along, speed);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
        T_Damage_VRMelee(m, self, self, dmg);
        VR_ClearHitKind();
    }
    float headsNow = VR_Decap_TestHeads(); // (not in the call's arguments: a call there overwrites the ones before it)
    float rag = isragdoll(m);
    // (sprintf takes at most 7 values: QC builtins take 8 parameters)
    bprint(sprintf("decaptest: %g %s on %s: %s, health %g, headless %g", xTest, what, m.classname,
        (vr_decap_count > before) ? "beheaded" : "not beheaded", m.health, m.vr_headless));
    bprint(sprintf(", ragdoll %g, heads %g -> %g, popped %g, gibbed %g\n", rag, heads, headsNow, vr_decap_pops - pops,
        m.vr_gib));
    if(vr_decap_count > before && vr_decap_lasthead != world)
    {
        entity h = vr_decap_lasthead;
        vector neck = ragdollcut(m, 4);
        bprint(sprintf("decaptest: head %s at %v (the neck %v), %.0f u/s %v", h.model, h.origin, neck, vlen(h.velocity),
            h.velocity));
        bprint(sprintf(", spin %.1f rad/s %v (gib %g, rigid %g)\n", vlen(h.vr_spin), h.vr_spin, h.vr_gib, h.vr_rigid));
    }
};

// Every server frame (StartFrame): a test asked for (vr_decap_test).
void() VR_Decap_TestFrame =
{
    float ask = cvar("vr_decap_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_decap_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    VR_Decap_Test(ask);
    self = oself;
};

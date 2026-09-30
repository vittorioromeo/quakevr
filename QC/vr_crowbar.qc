// The crowbar (WID_CROWBAR; docs/vr-port/ROUND21.md, "The crowbar"): a steel wrecking bar, 67 cm, a melee weapon held
// as the knights' swords are, in one hand or two (Weapon Offsets' Two-Handed: Sword; its hotspots: the other hand below
// the first on the tape, or sliding along the bar above the hands, short of the hook). Its model is our own
// (Misc/quakevr/make_crowbar.py: progs/v_crowbar.mdl), its weapon settings slot 24 (Quake/vr/vr_weapons.inc).
//
// Its blows (vr_melee.qc VR_MTHING_CROWBAR): the bar past the hands strikes, blunt, the hook at its end hardest
// (VR_MELEE_CROWBAR_HOOK); its chisel end jabs as a pommel does. No stabs (a hook, not a point). The damage is
// vr_crowbar_damage (25: the axe's 20, a knight's sword's 30) times the blow's speed and the weight's curve (its mass,
// 1.9 kg: Weapon Weights). It parries as any weapon (combat.qc VR_Parry: its line from the chisel to the hook), is thrown
// as a weapon (40 damage) and lands with the metal physics sounds (vr_physsound.cpp: a thrown weapon is metal).
//
// Placing one: a `weapon_crowbar` entity in a map (TrenchBroom: Misc/trenchbroom), `func_weapon_grabbable` with weapon 17,
// or from QC VR_Crowbar_Spawn (a crate breaking open, a monster's drop: it falls and settles where it lands).

float VR_CROWBAR_HIT_SOUNDS = 3;

void() VR_Crowbar_Precache =
{
    precache_model("progs/v_crowbar.mdl");
    precache_sound("vr/crowbar_hit1.wav"); // a blow landing on something that bleeds (Misc/quakevr/make_sounds.py)
    precache_sound("vr/crowbar_hit2.wav");
    precache_sound("vr/crowbar_hit3.wav");
    precache_sound("vr/crowbar_wall1.wav"); // on a wall: steel ringing on stone
    precache_sound("vr/crowbar_wall2.wav");
};

// A crowbar at `xOrg`, moving at `xVel` (units a second; '0 0 0': dropped where it is), turned `xAngles` ('0 0 0': any
// way): a weapon lying ready to be picked up (a thrown weapon at rest: it falls and settles on its drawn shape), as a
// knight's sword dropped. `xFrom`: what it comes from (a crate, a monster; world: none). Returns it.
entity(entity xFrom, vector xOrg, vector xAngles, vector xVel) VR_Crowbar_Spawn =
{
    if(xAngles == '0 0 0')
    {
        xAngles = randomVec(180);
    }
    return CreateThrownWeapon(xFrom, WID_CROWBAR, 0 /* weaponflags */, 0 /* weaponclip */,
        xOrg, xAngles,
        xVel, xVel == '0 0 0' ? '0 0 0' : randomVec(60),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(WID_CROWBAR));
};

/*QUAKED weapon_crowbar (0 .5 .8) (-16 -16 0) (16 16 32)
A crowbar lying ready to be picked up (Quake VR): a melee weapon, one hand or two. It falls from its origin and
settles on the floor. "angle" or "angles": how it lies (none: any way).
*/
void() weapon_crowbar =
{
    VR_Crowbar_Spawn(self, self.origin, self.angles, '0 0 0');
    remove(self);
};

// The blow landing (client.qc PlayerVRMeleeImpl, by vr_melee.qc's contact): VRDoMeleeAttackTracelineFor finds what it
// meets; a thing that takes damage takes the blow, a wall rings.
void(float xHand) W_CrowbarMelee =
{
    float xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_crowbar_damage));
    if(!VRDoMeleeAttackTracelineFor(xHand, 22, 30))
    {
        return;
    }

    vector org = trace_endpos - v_forward * 4;
    if(trace_ent.takedamage)
    {
        trace_ent.axhitme = 1;
        self.damage_weapon = IT_AXE;
        if(T_Damage_VRMelee(trace_ent, self, self, xdmg))
        {
            SpawnBloodSplash(org, xdmg);
        }
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            float k = min(VR_CROWBAR_HIT_SOUNDS, 1 + floor(random() * VR_CROWBAR_HIT_SOUNDS));
            string s = k == 1 ? "vr/crowbar_hit1.wav" : (k == 2 ? "vr/crowbar_hit2.wav" : "vr/crowbar_hit3.wav");
            sound(self, VRGetGunChannel(xHand), VR_Melee_HitSound(s), 1.0, ATTN_NORM);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
        return;
    }

    // A wall: steel ringing and sparks.
    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        VRMeleeHapticByHand(xHand);
        sound(self, VRGetGunChannel(xHand), random() < 0.5 ? "vr/crowbar_wall1.wav" : "vr/crowbar_wall2.wav", 1, ATTN_NORM);
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
    particle2(org, trace_plane_normal * 60, QVR_PARTICLE_PRESET_SPARKS, 10);
};

// Tests (impulse 216): the crowbar lying nearest you into an empty hand, as a hand's touch with its grip held takes it.
void() VR_Crowbar_TakeNearest =
{
    entity best = world;
    float bestDist = 1000;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(e.weapon == WID_CROWBAR && vlen(e.origin - self.origin) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - self.origin);
        }
        e = find(e, classname, "thrown_weapon");
    }
    float hand = VRIsHandEmpty(self, cVR_MainHand) ? cVR_MainHand : VRIsHandEmpty(self, cVR_OffHand) ? cVR_OffHand : -1;
    if(best == world || hand < 0 || !best.handtouch)
    {
        bprint("crowbar: none lying within 1000 units, or no empty hand\n");
        return;
    }
    bprint(sprintf("crowbar: taking the one %g units away into the %s hand\n", bestDist,
        hand == cVR_MainHand ? "main" : "off"));
    entity oldSelf = self;
    entity oldOther = other;
    self.touchinghand = hand;
    best.handtouch_hand = hand;
    best.handtouch_ent = self;
    other = self;
    self = best;
    float oldCatching = forcegrabCatching;
    forcegrabCatching = TRUE;
    best.handtouch();
    forcegrabCatching = oldCatching;
    self = oldSelf;
    other = oldOther;
};

// Tests (impulse 218): each crowbar lying about and in your hands, printed (no new one dropped).
void() VR_Crowbar_Report =
{
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(e.weapon == WID_CROWBAR)
        {
            bprint(sprintf("crowbar: lying at %v, %g units away\n", e.origin, vlen(e.origin - self.origin)));
        }
        e = find(e, classname, "thrown_weapon");
    }
    bprint(sprintf("crowbar: in the main hand %s, in the off hand %s\n",
        VRGetEntWeapon(self, cVR_MainHand) == WID_CROWBAR ? "yes" : "no",
        VRGetEntWeapon(self, cVR_OffHand) == WID_CROWBAR ? "yes" : "no"));
};

// Tests (impulse 217): a crowbar dropped 48 units ahead of you, as VR_Crowbar_Spawn places one; then the report.
void() VR_Crowbar_SpawnAhead =
{
    makevectors(self.v_angle_y * '0 1 0');
    entity c = VR_Crowbar_Spawn(world, self.origin + v_forward * 48 + '0 0 16', '0 0 0', '0 0 0');
    bprint(sprintf("crowbar: dropped at %v\n", c.origin));
    VR_Crowbar_Report();
};

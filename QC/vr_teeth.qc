// vr_teeth.qc -- teeth knocked out (docs/vr-port/ROUND22.md, "More gore"): a hard punch to a humanoid's face (a grunt,
// an enforcer, a knight or a hell knight, a zombie, Arcane Dimensions' humanoids: VR_Teeth_Humanoid) knocks out small
// tooth gibs (make_teeth.py's vr_tooth.mdl), more the harder the blow. They are small gibs (vr_smallgibs.qc,
// VR_SmallGib_SpawnAs's xBrain 2: counted in Most Small Gibs, carried, thrown, fading as the others), but they do not
// bleed, squish or stick: they bounce and click (vr_physsound.cpp: stone).
//
// When: a hand's blow (T_Damage_VRMelee) that lands on the face of a live one (its head zone, VR_Decap_OnHead, struck
// from the front) with a fist (or with vr_teeth_blunt a blunt thing: a gun's butt, a crowbar, a club, a pommel; never a
// slap), as hard as vr_teeth_hardness at least (Decapitation's blunt hardness: the hand's speed and the damage,
// VR_Decap_BluntHardness): up to vr_teeth_per_hit for the hardest, at most vr_teeth_per_enemy from one.
// Test: impulse 198 punches the monster spawned last (impulse 241) in the face, as hard as can be.

.float vr_teeth_lost; // how many it has lost

// Whether `e` has teeth to lose: a humanoid with a head (its head gib).
float(entity e) VR_Teeth_Humanoid =
{
    string h = VR_Decap_HeadModel(e);
    return h == "progs/h_guard.mdl" || h == "progs/h_mega.mdl" || h == "progs/h_knight.mdl" || h == "progs/h_hellkn.mdl" ||
           h == "progs/h_zombie.mdl" || h == "progs/h_zombiek.mdl" || h == "progs/h_dknight.mdl" || h == "progs/h_soldier.mdl" ||
           h == "progs/h_justice.mdl" || h == "progs/h_judge.mdl" || h == "progs/xmas/h_santa.mdl" || h == "progs/xmas/h_elf.mdl";
};

// Its mouth: the front of its head zone, low (FALSE: no head zone).
vector vr_teeth_mouth;
vector vr_teeth_facing;
float(entity e) VR_Teeth_Mouth =
{
    if(!VR_Decap_HeadZone(e))
    {
        return FALSE;
    }
    float yaw = e.angles_y * (3.14159265 / 180);
    vr_teeth_facing_x = cos(yaw);
    vr_teeth_facing_y = sin(yaw);
    vr_teeth_facing_z = 0;
    vector head = e.origin + vr_teeth_facing * xHeadForward + VR_HeadSide(vr_teeth_facing) + '0 0 1' * xHeadUp;
    vr_teeth_mouth = head + vr_teeth_facing * (xHeadRadius * 0.8) - '0 0 1' * (xHeadRadius * 0.35);
    return TRUE;
};

// `n` teeth out of `e`'s mouth, struck going `xDir`: out of the mouth and to the sides, up a little, along the blow.
void(entity e, vector xDir, float n) VR_Teeth_Knock =
{
    if(!VR_Teeth_Mouth(e))
    {
        return;
    }
    n = min(n, max(0, cvar_hget(cvarh_vr_teeth_per_enemy)) - e.vr_teeth_lost);
    vector d = normalize(xDir);
    vector side = '0 0 1' * 0;
    side_x = -vr_teeth_facing_y;
    side_y = vr_teeth_facing_x;
    float speed = max(0, cvar_hget(cvarh_vr_teeth_speed)) * VRMetersToUnits();
    for(float i = 0; i < n; i++)
    {
        vector v = normalize(vr_teeth_facing * 0.6 + d * 0.4 + side * (crandom() * 0.9) + '0 0 1' * (0.3 + random() * 0.5));
        entity g = VR_SmallGib_SpawnAs(vr_teeth_mouth + side * (crandom() * 1.5), v * speed * (0.6 + random() * 0.6), e, 2);
        if(g == world)
        {
            break;
        }
        g.avelocity = '1 0 0' * (crandom() * 900) + '0 1 0' * (crandom() * 900) + '0 0 1' * (crandom() * 900);
        VR_SmallGib_Tag(g, QVR_SGIB_SIT_MELEE);
        e.vr_teeth_lost++;
    }
    if(n > 0)
    {
        dprint(sprintf("teeth: %g out of %s (%g lost)\n", n, e.classname, e.vr_teeth_lost));
    }
};

// A hand's blow about to deal `xDamage` to `targ` (T_Damage_VRMelee; self the player, vr_hithand the hand).
void(entity targ, entity attacker, float xDamage) VR_Teeth_Blow =
{
    if(!cvar_hget(cvarh_vr_teeth) || vr_hitkind != QVR_HITKIND_MELEE || vr_hithand < 0 || attacker != self ||
       !(self.flags & FL_CLIENT) || targ.health <= 0 || !(targ.flags & FL_MONSTER) || !VR_Teeth_Humanoid(targ))
    {
        return;
    }
    entity b = VR_Blow(vr_hithand);
    if(b.mh_kind == VR_MKIND_SLAP)
    {
        return;
    }
    float fist = b.mh_thing == VR_MTHING_FIST;
    float blunt = b.mh_kind == VR_MKIND_POMMEL || b.mh_wid == WID_MJOLNIR || b.mh_thing == VR_MTHING_GUN ||
                  b.mh_thing == VR_MTHING_CROWBAR || b.mh_thing == VR_MTHING_CLUB;
    if(!fist && !(blunt && cvar_hget(cvarh_vr_teeth_blunt)))
    {
        return;
    }
    if(!VR_Decap_OnHead(targ, b.mh_c_pos, '0 0 0') || !VR_Teeth_Mouth(targ))
    {
        return;
    }
    vector d = vr_hitdir;
    d_z = 0;
    if(d != '0 0 0' && normalize(d) * vr_teeth_facing > 0.3)
    {
        return; // (from behind: not its face)
    }
    float h = VR_Decap_BluntHardness(b.mh_speed, xDamage);
    float least = min(0.99, max(0, cvar_hget(cvarh_vr_teeth_hardness)));
    if(h < least)
    {
        return;
    }
    float n = floor((h - least) / (1 - least) * max(0, cvar_hget(cvarh_vr_teeth_per_hit)) + random());
    VR_Teeth_Knock(targ, vr_hitdir, n);
};

// impulse 198: the monster spawned last punched in the face as hard as can be (vr_teeth_per_hit teeth).
void() VR_Teeth_Test =
{
    entity t = world;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e != self && e.health > 0)
        {
            t = e;
        }
        e = nextent(e);
    }
    if(t == world || !VR_Teeth_Humanoid(t))
    {
        bprint("teeth: no humanoid monster\n");
        return;
    }
    VR_Teeth_Mouth(t);
    float before = t.vr_teeth_lost;
    VR_Teeth_Knock(t, vr_teeth_mouth - (self.origin + self.view_ofs), max(1, cvar_hget(cvarh_vr_teeth_per_hit)));
    bprint(sprintf("teeth: %g out of %s\n", t.vr_teeth_lost - before, t.classname));
};

// A tooth's touch: no squish, no blood; a rigid body from its first landing (as a small gib: its grace kept).
void() VR_Tooth_Touch =
{
    if(other.vr_gib && self.throwhit != QVR_THROWHIT_NEVER_HIT)
    {
        return;
    }
    if(other.solid == SOLID_BSP && !self.vr_rigid)
    {
        if(self.vr_sgib_grace)
        {
            self.vr_sgib_rigidlater = TRUE;
        }
        else
        {
            VR_SmallGib_Rigid(self);
        }
        if(vlen(self.velocity) > 60)
        {
            sound(self, CHAN_AUTO, "weapons/tink1.wav", 0.25, ATTN_STATIC);
        }
        return;
    }
    if(other.solid != SOLID_BSP && self.throwhit != QVR_THROWHIT_NEVER_HIT)
    {
        VR_Gib_Touch(); // (thrown into something: a gib's hit)
    }
};

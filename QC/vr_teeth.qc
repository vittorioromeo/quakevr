// vr_teeth.qc -- teeth knocked out (docs/vr-port/ROUND22.md, "More gore"): a hard punch to a humanoid's face (a grunt,
// an enforcer, a knight or a hell knight, a zombie, Arcane Dimensions' humanoids: VR_Teeth_Humanoid) knocks out small
// tooth gibs (make_teeth.py's vr_tooth.mdl), more the harder the blow. They are small gibs (vr_smallgibs.qc,
// VR_SmallGib_SpawnAs's xBrain 2: counted in Most Small Gibs, carried, thrown, fading as the others), but they do not
// bleed, squish or stick: they bounce and click (vr_physsound.cpp: stone).
//
// When: a hand's blow (T_Damage_VRMelee) that lands on the face of a live one (its head zone, VR_Decap_OnHead, struck
// from the front) with a fist (or with vr_teeth_blunt a blunt thing: a gun's butt, a crowbar, a club, a pommel; never a
// slap), as hard as vr_teeth_hardness at least (Decapitation's blunt hardness: the hand's speed and the damage,
// VR_Decap_BluntHardness): up to vr_teeth_per_hit for the hardest, at most vr_teeth_per_enemy from one. A dead one's
// ragdoll as well (ROUND22.md, "Teeth from ragdolls"): a corpse blow (VR_Corpse_Struck: VR_Teeth_CorpseBlow) on its head
// part's face, from the teeth the living one had left (the same entity's .vr_teeth_lost, kept in a saved game).
// Test: impulse 198 punches the monster spawned last (impulse 241) in the face, as hard as can be; dead, its ragdoll
// through a corpse blow of the main hand (vr_teeth_blunt 1 if it holds a gun).

.float vr_teeth_lost; // how many it has lost

// Whether `e` has teeth to lose: a humanoid with a head (its head gib).
float(entity e) VR_Teeth_Humanoid =
{
    string h = VR_Decap_HeadModel(e);
    return h == "progs/h_guard.mdl" || h == "progs/h_mega.mdl" || h == "progs/h_knight.mdl" || h == "progs/h_hellkn.mdl" ||
           h == "progs/h_zombie.mdl" || h == "progs/h_zombiek.mdl" || h == "progs/h_dknight.mdl" || h == "progs/h_soldier.mdl" ||
           h == "progs/h_justice.mdl" || h == "progs/h_judge.mdl" || h == "progs/xmas/h_santa.mdl" || h == "progs/xmas/h_elf.mdl";
};

// Its mouth: the front of its head zone, low (FALSE: no head zone); the way its face looks and its side (its left).
// A ragdoll's (a corpse's, a dying or knocked-down body's): its head part's middle (ragdollcut 6), along the part's own
// forward and up (the rest pose's +x and +z turned with it: the face as the body lies).
vector vr_teeth_mouth;
vector vr_teeth_facing;
vector vr_teeth_side;
float(entity e) VR_Teeth_Mouth =
{
    if(!VR_Decap_HeadZone(e))
    {
        return FALSE;
    }
    if(isragdoll(e))
    {
        float hb = ragdolllimbs(e, 3);
        if(e.vr_headless || hb < 0)
        {
            return FALSE;
        }
        vector mid = ragdollcut(e, 6);
        vector inHead = ragdollpoint(e, hb, mid, 0);
        vr_teeth_facing = normalize(ragdollpoint(e, hb, inHead + '1 0 0', 1) - mid);
        vr_teeth_side = normalize(ragdollpoint(e, hb, inHead + '0 1 0', 1) - mid);
        vector up = normalize(ragdollpoint(e, hb, inHead + '0 0 1', 1) - mid);
        vr_teeth_mouth = mid + vr_teeth_facing * (xHeadRadius * 0.8) - up * (xHeadRadius * 0.35);
        return TRUE;
    }
    float yaw = e.angles_y * (3.14159265 / 180);
    vr_teeth_facing_x = cos(yaw);
    vr_teeth_facing_y = sin(yaw);
    vr_teeth_facing_z = 0;
    vr_teeth_side_x = -vr_teeth_facing_y;
    vr_teeth_side_y = vr_teeth_facing_x;
    vr_teeth_side_z = 0;
    vector head = e.origin + vr_teeth_facing * xHeadForward + VR_HeadSide(vr_teeth_facing) + '0 0 1' * xHeadUp;
    vr_teeth_mouth = head + vr_teeth_facing * (xHeadRadius * 0.8) - '0 0 1' * (xHeadRadius * 0.35);
    return TRUE;
};

// Whether a blow at `xAt` going `xDir` on `e` lands on its face: on its head (a ragdoll's head part: ragdollhead; else
// its head zone, VR_Decap_OnHead) and not from behind it (VR_Teeth_Mouth's facing; standing, level). Sets the mouth.
float(entity e, vector xAt, vector xDir) VR_Teeth_OnFace =
{
    float rag = isragdoll(e);
    if(rag ? ragdollhead(e, xAt, 0) <= 0 : !VR_Decap_OnHead(e, xAt, '0 0 0'))
    {
        return FALSE;
    }
    if(!VR_Teeth_Mouth(e))
    {
        return FALSE;
    }
    vector d = xDir;
    if(!rag)
    {
        d_z = 0;
    }
    return d == '0 0 0' || normalize(d) * vr_teeth_facing <= 0.3; // (from behind: not its face)
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
    vector side = vr_teeth_side;
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

// A blow on `e`'s face (VR_Teeth_OnFace) going `xDir` at hardness `xHard` (0..1, VR_Decap_BluntHardness): from
// vr_teeth_hardness on, up to vr_teeth_per_hit for the hardest.
void(entity e, vector xDir, float xHard) VR_Teeth_Hard =
{
    float least = min(0.99, max(0, cvar_hget(cvarh_vr_teeth_hardness)));
    if(xHard < least)
    {
        return;
    }
    float n = floor((xHard - least) / (1 - least) * max(0, cvar_hget(cvarh_vr_teeth_per_hit)) + random());
    VR_Teeth_Knock(e, xDir, n);
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
    if(!VR_Teeth_OnFace(targ, b.mh_c_pos, vr_hitdir))
    {
        return;
    }
    VR_Teeth_Hard(targ, vr_hitdir, VR_Decap_BluntHardness(b.mh_speed, xDamage));
};

// A hand's blow at a dead humanoid's ragdoll (VR_Corpse_Struck; self the player): a fist (or with vr_teeth_blunt a blunt
// thing) at its head part's face knocks teeth out as a live one's punch does, from what it has left (.vr_teeth_lost is
// the same entity's: the living one's count goes on). Its hardness from the blow's speed (m/s) and its corpse damage.
void(entity c, float xHand, vector xAt, vector xDir, float xSpeed, float xDamage) VR_Teeth_CorpseBlow =
{
    if(!cvar_hget(cvarh_vr_teeth) || !isragdoll(c) || !VR_Teeth_Humanoid(c))
    {
        return;
    }
    entity b = VR_Blow(xHand);
    float thing = VR_Melee_Thing(self, xHand);
    float own = b.mh_thing == thing; // (the hand's blow record is this thing's: its kind holds)
    if(own && b.mh_kind == VR_MKIND_SLAP)
    {
        return;
    }
    float blunt = (own && (b.mh_kind == VR_MKIND_POMMEL || b.mh_wid == WID_MJOLNIR)) || thing == VR_MTHING_GUN ||
                  thing == VR_MTHING_CROWBAR || thing == VR_MTHING_CLUB;
    if(thing != VR_MTHING_FIST && !(blunt && cvar_hget(cvarh_vr_teeth_blunt)))
    {
        return;
    }
    if(!VR_Teeth_OnFace(c, xAt, xDir))
    {
        return;
    }
    VR_Teeth_Hard(c, xDir, VR_Decap_BluntHardness(xSpeed, xDamage));
};

// impulse 198: the monster spawned last punched in the face as hard as can be (vr_teeth_per_hit teeth).
void() VR_Teeth_Test =
{
    entity t = world;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e != self && (e.health > 0 || isragdoll(e)))
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
    if(!VR_Teeth_Mouth(t))
    {
        bprint(sprintf("teeth: %s has no face%s\n", t.classname, t.vr_headless ? " (headless)" : ""));
        return;
    }
    float before = t.vr_teeth_lost;
    if(t.health <= 0 || t.vr_corpse == 2 || t.vr_corpse_dying)
    {
        // Its ragdoll: a corpse blow at its mouth, into its face (as fast as a blow can be: vr_melee_speed x 3).
        vector at = vr_teeth_mouth;
        t.vr_corpse_struck = 0;
        VR_Corpse_Struck(cVR_MainHand, t, at, -vr_teeth_facing, 3 * max(0.1, cvar_hget(cvarh_vr_melee_speed)), FALSE);
        bprint(sprintf("teeth: %g out of %s's ragdoll (%g lost; thing %g)\n", t.vr_teeth_lost - before, t.classname,
            t.vr_teeth_lost, VR_Melee_Thing(self, cVR_MainHand)));
        return;
    }
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

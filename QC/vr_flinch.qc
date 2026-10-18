// vr_flinch.qc -- the flinch (vr_flinch; docs/vr-port/FLINCH.md; Combat > Flinch; the engine's box3d::ragdollFlinch and
// ragactive::beginFlinch): a hit past a threshold makes a humanoid an active ragdoll a moment. Its impulse (N s) goes into
// its body: a share at the point struck (the jolt), the rest along the hit (it steps, its arms and torso balancing). Once
// balanced (the recovery test: its middle over its feet, slow, upright, on firm ground) it is handed back to its animation
// in the nearest stand or run frame and its AI goes on; not balanced in time, or past its balance's edge, it topples into
// the knockdown and gets up as any. It reuses the shove's stagger (VR_Knockdown_Start, VR_Knockdown_Think).
//
// - Impulse by what struck: a shotgun's pellets (vr_flinch_shotgun, _ssg per pellet, less with distance), nails and
//   other missiles (vr_flinch_nail per damage), other hitscan (vr_flinch_shot_other), a melee blow (vr_flinch_melee per
//   damage: its speed, weapon and zone are in its damage), a thrown prop's momentum (vr_flinch_throw), a blast
//   (vr_flinch_blast per damage, away from it). Shoves and bashes stay the knockdown's.
// - Hits add up (fading over VR_FL_FADE s): a burst of nails flinches where one nail does not.
// - Safeguards: below vr_flinch_min (m/s its body is given) only the small hit reaction (vr_hitreact.qc); a cooldown
//   (vr_flinch_cooldown); its attack frames guarded (vr_flinch_attack_guard); a budget and a range (the engine's: the
//   small reaction past them). Its pain's frames never play for a flinch (a ragdoll then); vr_flinch_pain 0 keeps them
//   off for the small hits too.
// - vr_flinch 0: none of this; the reactions, pains and Hits Unbalance exactly as before.

.float vr_fl_next;      // when it may flinch again (vr_flinch_cooldown)
.vector vr_fl_acc;      // the hits' impulses summed (N s, world), fading
.float vr_fl_acc_time;  // when last added to
float vr_fl_after_damage; // VR_Flinch_Try from a blow or a throw (after its damage, its pain played): made in its frame before
float VR_FL_FADE = 0.2; // s: the summed impulse's half-life
float vr_fl_test_damage; // a test's (VR_Flinch_Test): the impulse reckoned from this damage (0: the damage dealt)
float vr_fl_test_weapon; // a test's weapon, plus 1 (WID_* + 1; 0: the attacker's)

float() VR_Flinch_On =
{
    return cvar_hget(cvarh_vr_flinch) != 0;
};

// Whether `e`'s hits are the flinch's (on, a monster of a class that flinches): Hits Unbalance gives way to it.
float(entity e) VR_Flinch_Owns =
{
    return VR_Flinch_On() && (e.flags & FL_MONSTER) && ragdollflinch(e, '0 0 0', '0 0 0', 2) > 0;
};

// With vr_flinch_pain 0, Quake's pain frames stay off for a hit too small to flinch (its sound plays).
float(entity e) VR_Flinch_PainOff =
{
    return VR_Flinch_On() && !cvar_hget(cvarh_vr_flinch_pain) && VR_Flinch_Owns(e);
};

// A shotgun's strength `xDist` units off: 1 within vr_flinch_shot_near, easing to vr_flinch_shot_far_share at _far.
float(float xDist) VR_Flinch_ShotFalloff =
{
    float near = max(0, cvar_hget(cvarh_vr_flinch_shot_near));
    float far = max(near + 1, cvar_hget(cvarh_vr_flinch_shot_far));
    float f = min(1, max(0, (xDist - near) / (far - near)));
    return 1 + (max(0, cvar_hget(cvarh_vr_flinch_shot_far_share)) - 1) * f;
};

// The impulse (N s) of a hit dealing `take` (T_DamageDeal; its kind in vr_hitkind, its point in vr_hr_at): 0 a hit the
// flinch leaves to others (shoves, bashes, headbutts: the knockdown's; a melee blow and a thrown prop: VR_Flinch_Melee
// and _Throw, after the damage).
float(entity targ, entity inflictor, entity attacker, float take) VR_Flinch_Impulse =
{
    float dmg = vr_fl_test_damage > 0 ? vr_fl_test_damage : take;
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        return dmg * max(0, cvar_hget(cvarh_vr_flinch_blast));
    }
    if(vr_hitkind == QVR_HITKIND_MELEE || vr_hitkind == QVR_HITKIND_BASH || vr_hitkind == QVR_HITKIND_SHOVE ||
       vr_hitkind == QVR_HITKIND_HEADBUTT || (vr_fl_thrown != world && inflictor == vr_fl_thrown))
    {
        return 0;
    }
    if(inflictor == attacker && (attacker.flags & FL_CLIENT) && (vr_hitkind == QVR_HITKIND_SHOT || vr_hitpellets > 0))
    {
        float hand = vr_hithand >= 0 ? vr_hithand : cVR_MainHand;
        float w = vr_fl_test_weapon > 0 ? vr_fl_test_weapon - 1 : VRGetEntWeapon(attacker, hand);
        float pellets = vr_hitpellets > 0 ? vr_hitpellets : 1;
        float fall = VR_Flinch_ShotFalloff(vlen(vr_hr_at - (attacker.origin + attacker.view_ofs)));
        if(w == WID_SHOTGUN || w == WID_GRUNTGUN)
        {
            return pellets * max(0, cvar_hget(cvarh_vr_flinch_shotgun)) * fall;
        }
        if(w == WID_SUPER_SHOTGUN)
        {
            return pellets * max(0, cvar_hget(cvarh_vr_flinch_ssg)) * fall;
        }
        return dmg * max(0, cvar_hget(cvarh_vr_flinch_shot_other));
    }
    if(inflictor != attacker && inflictor != world && !(inflictor.flags & (FL_MONSTER | FL_CLIENT)))
    {
        return dmg * max(0, cvar_hget(cvarh_vr_flinch_nail)); // a nail, a missile's own hit
    }
    return dmg * max(0, cvar_hget(cvarh_vr_flinch_shot_other)); // a monster's hitscan or blow
};

// Room for its ragdoll (vr_ragdoll_max): a dead body may go for it, never a living knockdown.
float(entity e) VR_Flinch_Room =
{
    vr_kd_room_dead_only = TRUE;
    float ok = VR_Knockdown_MakeRoom(e);
    vr_kd_room_dead_only = FALSE;
    return ok;
};

// A hit of `xImpulse` N s by `xBy` at `xAt` (units) going along `xDir` on `e` (alive, its damage dealt): it flinches if
// it can and the hits summed give its body vr_flinch_min or more. TRUE if it does (a ragdoll now: vr_knockdown 1, its
// stagger the flinch's).
float(entity e, entity xBy, vector xAt, vector xDir, float xImpulse, string xWhat) VR_Flinch_Try =
{
    if(!VR_Flinch_On() || xImpulse <= 0 || !(e.flags & FL_MONSTER) || e.health <= 0 || e.vr_knockdown || e.vr_corpse ||
       e.vr_corpse_dying || (e.flags & (FL_FLY | FL_SWIM)) || e.solid == SOLID_BSP || e.vr_parry_stagger_until > time ||
       (VR_IsDummy(e) && cvar_hget(cvarh_vr_dummy_anchored)))
    {
        return FALSE;
    }
    vector dir = normalize(xDir);
    if(dir == '0 0 0')
    {
        dir = normalize(e.origin - xBy.origin);
    }
    if(xAt == '0 0 0')
    {
        xAt = (e.absmin + e.absmax) * 0.5;
    }
    float loud = cvar_hget(cvarh_vr_flinch_debug);
    // (Summed with the last hits, fading.)
    float keep = e.vr_fl_acc_time > 0 ? pow(0.5, (time - e.vr_fl_acc_time) / VR_FL_FADE) : 0;
    vector acc = e.vr_fl_acc * keep + dir * xImpulse;
    e.vr_fl_acc = acc;
    e.vr_fl_acc_time = time;
    if(time < e.vr_fl_next)
    {
        return FALSE;
    }
    float dv = ragdollflinch(e, xAt, acc, 0);
    if(dv < 0 && dv > -1.5)
    {
        return FALSE; // (not now: its class, the budget, the range)
    }
    float full = dv <= -2;
    if(full)
    {
        dv = -2 - dv;
    }
    if(dv < max(0, cvar_hget(cvarh_vr_flinch_min)))
    {
        if(loud >= 2)
        {
            dprint(sprintf("flinch: %s by %s: %.0f N s summed, %.2f m/s: under the threshold\n", e.classname, xWhat,
                vlen(acc), dv));
        }
        return FALSE;
    }
    if(full && !VR_Flinch_Room(e))
    {
        return FALSE;
    }
    // (A blow or a throw comes after its damage, its pain's first frame already set: its body is made as it stood when
    // struck. A pain's frame bends it (the enforcer's paind1: its chest 35 degrees over), and it may not straighten.)
    float painFrame = e.frame;
    if(vr_fl_after_damage && e.vr_fl_prepain_time == time)
    {
        e.frame = e.vr_fl_prepain;
    }
    if(!VR_Knockdown_Start(e))
    {
        e.frame = painFrame;
        return FALSE;
    }
    float r = ragdollflinch(e, xAt, acc, 1);
    vector level = dir;
    level_z = 0;
    level = normalize(level);
    if(!r)
    {
        ragdollshovetopple(e, level, 0.5, FALSE, 0); // (as VR_Knockdown_Stagger: it can't, it goes over)
    }
    else
    {
        e.vr_kd_stagger = 1;
        e.vr_kd_balance = 0.5;
        e.vr_kd_dir = level;
        e.nextthink = time + 0.05;
    }
    e.vr_fl_next = time + max(0, cvar_hget(cvarh_vr_flinch_cooldown));
    e.vr_fl_acc = '0 0 0';
    if(loud)
    {
        dprint(sprintf("flinch: %s by %s: %.0f N s, %.2f m/s%s\n", e.classname, xWhat, vlen(acc), dv,
            r == 2 ? ": down at once" : !r ? ": toppled (no stagger)" : ""));
    }
    return TRUE;
};

// T_DamageDeal's (before the small reaction): a shot, a nail, a missile or a blast. TRUE if it flinches.
float(entity targ, entity inflictor, entity attacker, float take) VR_Flinch_Hit =
{
    if(cvar_hget(cvarh_vr_flinch_debug) >= 3)
    {
        dprint(sprintf("flinch hit: %s take %g kind %g pellets %g flags %g health %g down %g\n", targ.classname, take, vr_hitkind,
            vr_hitpellets, targ.flags & FL_MONSTER, targ.health, targ.vr_knockdown));
    }
    if(take <= 0 || !VR_Flinch_On() || !(targ.flags & FL_MONSTER) || targ.health <= 0 || targ.vr_knockdown ||
       vr_hitkind == QVR_HITKIND_BLEED || inflictor.classname == "vr_burn" || inflictor == world || attacker == world)
    {
        return FALSE;
    }
    VR_HitReact_Where(targ, inflictor, attacker);
    float imp = VR_Flinch_Impulse(targ, inflictor, attacker, take);
    if(imp <= 0)
    {
        return FALSE;
    }
    string what = vr_hitkind == QVR_HITKIND_SPLASH ? "a blast" : vr_hitpellets > 0 ? sprintf("%g pellets", vr_hitpellets)
                                                                                    : inflictor.classname;
    return VR_Flinch_Try(targ, attacker, vr_hr_at, vr_hr_dir, imp, what);
};

// A melee blow (VR_HitStagger_Melee, after its damage): its damage (its zone in) times vr_flinch_melee, at the point
// it met (the head's: a head blow jolts the head).
float(entity e, entity xBy, vector xDir, float xDamage, float xHead) VR_Flinch_Melee =
{
    float dmg = vr_fl_test_damage > 0 ? vr_fl_test_damage : xDamage;
    vector at = vr_hitpos;
    if(at == '0 0 0' || vlen(at - (e.absmin + e.absmax) * 0.5) > 96)
    {
        at = (e.absmin + e.absmax) * 0.5;
        if(xHead)
        {
            at_z = e.absmax_z - 4;
        }
    }
    vector dir = vr_hitdir != '0 0 0' ? vr_hitdir : xDir;
    vr_fl_after_damage = TRUE;
    float r = VR_Flinch_Try(e, xBy, at, dir, dmg * max(0, cvar_hget(cvarh_vr_flinch_melee)), xHead ? "a blow to the head" : "a blow");
    vr_fl_after_damage = FALSE;
    return r;
};

// A thrown prop or weapon (VR_HitStagger_Throw, after its damage): its momentum (kg m/s) times vr_flinch_throw.
float(entity e, entity xBy, vector xDir, float xMomentum) VR_Flinch_Throw =
{
    vr_fl_after_damage = TRUE;
    float r = VR_Flinch_Try(e, xBy, (e.absmin + e.absmax) * 0.5, xDir, xMomentum * max(0, cvar_hget(cvarh_vr_flinch_throw)),
        sprintf("a throw (%.0f kg m/s)", xMomentum));
    vr_fl_after_damage = FALSE;
    return r;
};

// ----------------------------------------------------------------------------
// Tests (vr_flinch_test <n>; Debug > Tests > Flinch; Misc/quakevr/flinch/flinch_test.sh): hits through T_Damage as the
// weapons deal them, on the live monster nearest the first player (a damage of 1: never a kill; the impulse reckoned from
// the weapon's), from where he stands:
//   1 a shotgun blast (vr_flinch_test_pellets pellets, 6), 2 a super shotgun's (14), 3 a nail, 4 a super nail, 5 a melee
//   blow (30 damage), 6 a blow to the head (30), 7 a blast 48 units before it (60 damage), 8 a 12 kg crate at 10 m/s;
//   9 a shotgun blast on every live monster within 1024 units (the crowd);
//   20 a grunt put at vr_flinch_test_x, _y, _z facing him; 21 ten grunts in two rows of five 160 units ahead.
// ----------------------------------------------------------------------------

entity(entity pl) VR_Flinch_TestTarget =
{
    entity best = world;
    float bestD = 100000;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e.health > 0 && !VR_IsDummy(e) && vlen(e.origin - pl.origin) < bestD)
        {
            bestD = vlen(e.origin - pl.origin);
            best = e;
        }
        e = nextent(e);
    }
    return best;
};

// A grunt at `at` facing `pl`.
void(entity pl, vector at) VR_Flinch_TestSpawn =
{
    entity oldSelf = self;
    entity m = spawn();
    self = m;
    setorigin(m, at);
    m.angles = '0 1 0' * vectoyaw(pl.origin - at);
    m.classname = "monster_army";
    monster_army();
    if(cvar("vr_flinch_test_health") > 0)
    {
        m.health = cvar("vr_flinch_test_health"); // (a test of real weapons' impulses: it lives through them)
    }
    self = oldSelf;
};

// Hit `best` by player `pl` with test `xMode`.
void(entity pl, entity best, float xMode) VR_Flinch_TestHit =
{
    vector eye = pl.origin + pl.view_ofs;
    vector chest = (best.absmin + best.absmax) * 0.5 + '0 0 6';
    vector dir = chest - eye;
    dir_z = 0; // (level: a test's hit from above or below pushes it along the floor all the same)
    dir = normalize(dir);
    float pellets = cvar("vr_flinch_test_pellets");
    entity oldSelf = self;
    self = pl;
    if(xMode == 1 || xMode == 2 || xMode == 9)
    {
        vr_fl_test_weapon = 1 + (xMode == 2 ? WID_SUPER_SHOTGUN : WID_SHOTGUN);
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, chest, dir);
        vr_hitpellets = pellets > 0 ? pellets : (xMode == 2 ? 14 : 6);
        T_Damage(best, pl, pl, 1);
        vr_hitpellets = 0;
        vr_fl_test_weapon = 0;
    }
    else if(xMode == 3 || xMode == 4)
    {
        entity nail = spawn();
        nail.classname = xMode == 3 ? "spike" : "superspike";
        setorigin(nail, chest - dir * 8);
        nail.velocity = dir * 1000;
        nail.owner = pl;
        vr_fl_test_damage = xMode == 3 ? 9 : 18;
        T_Damage(best, nail, pl, 1);
        remove(nail);
    }
    else if(xMode == 5 || xMode == 6)
    {
        vector at = xMode == 6 ? chest + '0 0 1' * (best.absmax_z - 6 - chest_z) : chest;
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - eye));
        vr_fl_test_damage = 30;
        T_Damage_VRMelee(best, pl, pl, 1);
    }
    else if(xMode == 7)
    {
        entity blast = spawn();
        blast.classname = "vr_flinch_testblast";
        vector flat = best.origin - pl.origin;
        flat_z = 0;
        setorigin(blast, best.origin - normalize(flat) * 48);
        VR_SetHitKind(QVR_HITKIND_SPLASH, -1, '0 0 0', '0 0 0');
        vr_fl_test_damage = 60;
        T_Damage(best, blast, pl, 1);
        remove(blast);
    }
    else if(xMode == 8)
    {
        vr_fl_test_damage = 0;
        VR_HitStagger_Throw(best, pl, dir, 12, 10 * VRMetersToUnits(), 1);
    }
    VR_ClearHitKind();
    vr_fl_test_damage = 0;
    self = oldSelf;
};

// StartFrame: vr_flinch_test.
void() VR_Flinch_Frame =
{
    float want = cvar("vr_flinch_test");
    if(want < 1)
    {
        return;
    }
    cvar_set("vr_flinch_test", "0");
    entity pl = find(world, classname, "player");
    if(!pl)
    {
        return;
    }
    makevectors(pl.angles);
    if(want == 20)
    {
        VR_Flinch_TestSpawn(pl, '1 0 0' * cvar("vr_flinch_test_x") + '0 1 0' * cvar("vr_flinch_test_y") + '0 0 1' * cvar("vr_flinch_test_z"));
        return;
    }
    if(want == 21)
    {
        float i;
        vector flat = v_forward;
        flat_z = 0;
        flat = normalize(flat);
        vector side = '0 0 0';
        side_x = -flat_y;
        side_y = flat_x;
        for(i = 0; i < 10; i = i + 1)
        {
            float row = floor(i / 5);
            float col = i - row * 5;
            VR_Flinch_TestSpawn(pl, pl.origin + flat * (160 + 64 * row) + side * (48 * (col - 2)) + '0 0 8');
        }
        return;
    }
    if(want == 9)
    {
        float n = 0;
        entity e = findradius(pl.origin, 1024);
        while(e)
        {
            if((e.flags & FL_MONSTER) && e.health > 0 && !VR_IsDummy(e))
            {
                VR_Flinch_TestHit(pl, e, 9);
                n = n + 1;
            }
            e = e.chain;
        }
        dprint(sprintf("flinch test: a shotgun blast on %g monsters\n", n));
        return;
    }
    entity best = VR_Flinch_TestTarget(pl);
    if(!best)
    {
        dprint("flinch test: no live monster\n");
        return;
    }
    dprint(sprintf("flinch test %g: %s %.0f units off\n", want, best.classname, vlen(best.origin - pl.origin)));
    VR_Flinch_TestHit(pl, best, want);
};

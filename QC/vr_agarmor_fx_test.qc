// vr_agarmor_fx_test.qc -- armour's ragdolls, cuts, debris and effects (vr_agarmor.qc; ROUND23.md, "Armour: ragdolls,
// dislodging, debris, sparks"). `vr_agarmor_test 5`: four armed grunts ahead of the player: A killed (a dead ragdoll),
// B knocked down (an active ragdoll), C killed (cut, beheaded and gibbed), D standing (shots). Checks: a plate found and
// absorbing on both ragdolls; B's armed cut stopped by its plate as the plate breaks; then (A, B dead) the leg worn by
// hits stays on until its plate breaks, and pops after; C's shoulder cut dislodging its pauldron and bracer, the
// beheading its helmet, the headless gib every plate left, A's gib with its head its cuirass, every plate as debris
// still there (none lost); the debris taken in a hand (not force-grabbable, nothing to wear or put away) has its fade
// put off; TraceAttack on D's plates sparks, with blood only when damage got through; the break's sound is not the
// pickup's. Waits for the ragdolls (a give-up of 5 s: FAIL timed out). Ends on "agarmor fx test: N passed, M failed".

entity vr_aft_a, vr_aft_b, vr_aft_c, vr_aft_d;
// (vr_aft_busy, vr_agarmor_test.qc: 1 waiting for the ragdolls)
float vr_aft_until;
float vr_aft_rule, vr_aft_helm, vr_aft_cuts, vr_aft_dtime; // the settings put back after
float vr_aft_made0; // vr_ar_debris_made at the start
float vr_aft_pb;    // B's plate

// Repeated hits of `d` at piece `i`'s place on ragdoll `e` (at most 60), by T_Damage as a shot's: the hit its plate
// broke at, then the one its limb came off at (-1 never); vr_aft_broke, vr_aft_cut.
float vr_aft_broke, vr_aft_cut;
void(entity e, float i, float d) VR_AFT_HitUntilCut =
{
    float k;
    vr_aft_broke = VR_AgArmor_On(e, i) ? -1 : 0;
    vr_aft_cut = -1;
    for(k = 1; k <= 60 && vr_aft_cut < 0 && (e.vr_corpse == 2 || e.vr_corpse_dying); k++)
    {
        vector at = limbpiece(e, i, 10);
        VR_ClearHitKind();
        VR_ART_Hit(e, at, 3, d);
        if(vr_aft_broke < 0 && !VR_AgArmor_On(e, i))
        {
            vr_aft_broke = k;
        }
        if(ragdolllimbs(e, 2) != 0)
        {
            vr_aft_cut = k;
        }
    }
};

float() VR_AFT_DebrisCount =
{
    float n = 0;
    entity h = find(world, classname, "vr_ar_debris");
    while(h != world)
    {
        n = n + 1;
        h = find(h, classname, "vr_ar_debris");
    }
    return n;
};

// A limb plate of `e` (legs first) whose place, as it lies, reads as its own piece; -1 none.
float(entity e) VR_AFT_LimbPiece =
{
    float i;
    for(i = 9; i >= 2; i--)
    {
        vector at = limbpiece(e, i, 10);
        float j = VR_Limb_At(e, at);
        if(ragdollbone(e, at, QVR_ARMOR_PIECE_AT) == i && j > 0 && j < VR_LIMB_JOINTS) // (a corpse wears every limb joint: VR_Limb_WearPiece)
        {
            return i;
        }
    }
    return -1;
};

// Every limb plate of `e` read as its own piece where a hit meets its outer face: near both ends and at its middle
// (limbpiece 12, 13, 14). The ragdoll rig's nearest vertex read a thigh plate's knee end as the shin's.
void(entity e, string xWhat) VR_AFT_PiecesAt =
{
    float i, k, own = 0;
    string bad = "";
    for(i = 2; i < VR_AR_PIECES; i++)
    {
        for(k = 12; k <= 14; k++)
        {
            float got = ragdollbone(e, limbpiece(e, i, k), QVR_ARMOR_PIECE_AT);
            own = own + (got == i);
            bad = (got == i) ? bad : sprintf("%s %g@%g->%g", bad, i, k, got);
        }
    }
    VR_ART_Check(own == (VR_AR_PIECES - 2) * 3, sprintf("%s: %g of %g points on the limb plates read as their own piece%s", xWhat,
        own, (VR_AR_PIECES - 2) * 3, bad));
};

// Corpse `e`'s right leg (the authored rig's joints 17 and 18: past the old 16 slots) worn as its own piece by a hit
// there, the whole body's health untouched (it wore as the torso's).
void(entity e) VR_AFT_RightLegWear =
{
    float keepType = e.vr_ar_type;
    float keepMask = e.vr_ar_mask;
    float chance = cvar("vr_limbs_chance_scale");
    cvar_set("vr_limbs_chance_scale", "0");
    VR_AgArmor_Give(e, 0, 0);
    float i, ok = 0;
    string got = "";
    for(i = 7; i <= 9; i = i + 2)
    {
        vector at = limbpiece(e, i, 10);
        float j = VR_Limb_WearPiece(e, at);
        float d0 = (j >= 0) ? e.vr_limb_corpse_dmg[j] : 0;
        float hp0 = e.vr_corpse_hp;
        VR_ClearHitKind();
        VR_ART_Hit(e, at, 3, 3);
        float worn = (j >= 0) ? e.vr_limb_corpse_dmg[j] - d0 : 0;
        ok = ok + (j >= 16 && worn > 0 && e.vr_corpse_hp == hp0);
        got = sprintf("%s plate %g: joint %g worn %g, body %g -> %g;", got, i, j, worn, hp0, e.vr_corpse_hp);
    }
    VR_ART_Check(ok == 2, sprintf("corpse's right thigh and shin (joints 16+) worn as their own pieces %g of 2:%s", ok, got));
    VR_AgArmor_Give(e, keepType, keepMask);
    cvar_set("vr_limbs_chance_scale", ftos(chance));
};

void(entity p) VR_AFT_Run =
{
    entity a = vr_aft_a;
    entity b = vr_aft_b;
    entity c = vr_aft_c;
    entity d = vr_aft_d;
    entity oldself;
    float h0, made, s0, b0;
    vector at;

    // A limb plate of each ragdoll found at its own place as it lies (legs first; some plates' middles lie nearer the
    // next bone's skin: a thigh plate's by the knee reads as the shin's).
    VR_AFT_PiecesAt(a, "dead ragdoll");
    VR_AFT_PiecesAt(b, "active ragdoll");
    VR_AFT_PiecesAt(d, "standing");
    VR_AFT_RightLegWear(a);
    float pb = VR_AFT_LimbPiece(b);
    float pa = VR_AFT_LimbPiece(a);
    VR_ART_Check(pa >= 2 && pb >= 2, sprintf("a limb plate found at its place on the dead ragdoll (%g) and the active one (%g)", pa, pb));
    pa = max(2, pa);
    pb = max(2, pb);

    // --- The active ragdoll (B, alive, knocked down): its plate absorbing, its armed killing cut stopped as the plate
    // breaks.
    VR_AgArmor_Give(b, 1, 1 | VR_AgArmor_Bit(pb));
    at = limbpiece(b, pb, 10);
    h0 = b.vr_ar_hp[pb];
    float took = VR_ART_Hit(b, at, 2, 10);
    VR_ART_Check(b.vr_ar_hp[pb] == h0 - 3 && took == 7, sprintf("active ragdoll: 10 on its green plate %g takes %g (7), pool %g -> %g",
        pb, took, h0, b.vr_ar_hp[pb]));
    b.vr_ar_hp[pb] = 2;
    b.health = 5;
    made = vr_ar_debris_made;
    oldself = self;
    self = p;
    float joint = VR_Limb_At(b, at);
    float armed = VR_Limb_Arm(b, p, at, normalize(at - p.origin) * 300, QVR_DECAP_SWING, joint);
    self = oldself;
    VR_ART_Hit(b, at, 2, 20);
    VR_ART_Check(armed && b.health <= 0 && !VR_AgArmor_On(b, pb) && vr_ar_debris_made == made + 1 && ragdolllimbs(b, 2) == 0,
        sprintf("active ragdoll: a killing cut armed at joint %g (armed %g) under the plate: the plate broke (debris %g), no limb cut (cut %g)",
        joint, armed, vr_ar_debris_made - made, ragdolllimbs(b, 2)));
    VR_ART_Check(vr_ar_break_sound_last == "vr/derived/armor_break.wav" && vr_ar_break_sound_last != "items/armor1.wav",
        sprintf("the break's sound %s (not the pickup's items/armor1.wav)", vr_ar_break_sound_last));

    // --- The dead ragdolls (A; B now dead): the limb stays on until its plate breaks, then pops.
    VR_AgArmor_Give(a, 1, 1 | VR_AgArmor_Bit(pa));
    at = limbpiece(a, pa, 10);
    h0 = a.vr_ar_hp[pa];
    VR_ClearHitKind();
    VR_ART_Hit(a, at, 2, 10);
    VR_ART_Check(a.vr_ar_hp[pa] < h0, sprintf("dead ragdoll: 10 on its green plate %g: pool %g -> %g (its share saved)", pa, h0, a.vr_ar_hp[pa]));
    VR_AFT_HitUntilCut(a, pa, 12);
    VR_ART_Check(vr_aft_broke > 0 && vr_aft_cut > vr_aft_broke, sprintf("dead ragdoll: the plate broke at hit %g, the limb off at hit %g (after)",
        vr_aft_broke, vr_aft_cut));
    vr_aft_pb = pb; // (B's limb worn off once it is a corpse: VR_AgArmor_FxTestFrame's last step)

    // --- Dislodging (C, dead): a shoulder cut takes its pauldron and bracer, the beheading its helmet, the gib the rest.
    VR_AgArmor_Give(c, 1, 1023);
    vector vel = normalize(c.origin - p.origin) * 200;
    made = vr_ar_debris_made;
    float shoulder = ragdolllimbs(c, 6, 0);
    float cutOk = VR_Limb_Cut(c, p, vel, QVR_DECAP_SWING, shoulder, FALSE);
    VR_ART_Check(cutOk && vr_ar_debris_made == made + 2 && c.vr_ar_mask == 1023 - 4 - 16,
        sprintf("cut at the left shoulder (joint %g, cut %g): %g plates dislodged (2), pieces left %g (1003)", shoulder, cutOk,
        vr_ar_debris_made - made, c.vr_ar_mask));
    made = vr_ar_debris_made;
    float decOk = VR_Decap_Cut(c, p, vel, QVR_DECAP_SWING);
    VR_ART_Check(decOk && vr_ar_debris_made == made + 1 && !VR_AgArmor_On(c, VR_AR_HELMET),
        sprintf("beheaded (%g): %g plates dislodged (1, the helmet), pieces left %g", decOk, vr_ar_debris_made - made, c.vr_ar_mask));
    made = vr_ar_debris_made;
    float left = VR_ART_Count(c.vr_ar_mask);
    VR_Corpse_Gib(c, p, p, 100);
    VR_ART_Check(vr_ar_debris_made == made + left && left == 7 && c.vr_ar_mask == 0,
        sprintf("the headless corpse gibbed: %g plates shed (%g left on it, 7)", vr_ar_debris_made - made, left));
    made = vr_ar_debris_made;
    left = VR_ART_Count(a.vr_ar_mask);
    VR_Corpse_Gib(a, p, p, 100);
    VR_ART_Check(vr_ar_debris_made == made + left && left == 1 && a.vr_ar_mask == 0,
        sprintf("the corpse gibbed with its head: %g plates shed (%g left on it, 1)", vr_ar_debris_made - made, left));
    VR_ART_Check(VR_AFT_DebrisCount() == vr_ar_debris_made - vr_aft_made0,
        sprintf("every plate off is debris still there: %g of %g", VR_AFT_DebrisCount(), vr_ar_debris_made - vr_aft_made0));

    // --- The debris in a hand.
    entity h = find(world, classname, "vr_ar_debris");
    VR_ART_Check(h != world && !(h.flags & FL_FORCEGRABBABLE) && !(h.flags & FL_ITEM) && h.solid == SOLID_NOT_BUT_TOUCHABLE &&
        h.handtouch == VR_Carry_Handtouch && h.carry_use == SUB_Null && !VR_Carry_IsCollectable(h, TRUE) && h.size != '0 0 0',
        sprintf("debris: taken by a hand (solid %g, size %v), not force-grabbable (flags %g), nothing to wear or put away", h.solid, h.size, h.flags));
    h.wait = time - 0.5;
    h.alpha = 0.5;
    h.nextthink = time + 0.05;
    float g0 = vr_ar_debris_grabs;
    oldself = self;
    self = p;
    VR_Carry_Start(p, cVR_MainHand, h);
    VR_ART_Check(vr_ar_debris_grabs == g0 + 1 && h.alpha == 1 && h.wait == 0 && h.nextthink >= time + VR_AgArmor_DebrisTime() - 0.01,
        sprintf("debris taken mid-fade: alpha %g, its fade in %.2f s (its whole %g again)", h.alpha, h.nextthink - time, VR_AgArmor_DebrisTime()));
    entity fade = h;
    self = fade;
    fade.nextthink = time;
    VR_AgArmor_DebrisFade();
    VR_ART_Check(fade.classname == "vr_ar_debris" && fade.alpha == 1 && fade.nextthink >= time + VR_AgArmor_DebrisTime() - 0.01,
        "debris held when its time is up: kept, its fade put off again");
    self = p;
    VR_Carry_Release(cVR_MainHand, fade);
    self = oldself;
    VR_ART_Check(fade.classname == "vr_ar_debris" && fade.carry_player == world, "debris let go: still there");

    // --- Sparks (D, standing): TraceAttack on its plates.
    vector dir;
    VR_AgArmor_Give(d, 1, 1023);
    at = limbpiece(d, 0, 10);
    s0 = vr_ar_fx_sparks;
    b0 = vr_ar_fx_bled;
    oldself = self;
    self = p;
    dir = normalize(at - (p.origin + p.view_ofs));
    PositionalDamage(d, at, dir);
    ClearMultiDamage();
    trace_ent = d;
    trace_endpos = at;
    trace_plane_normal = dir * -1;
    TraceAttack(10, dir);
    ApplyMultiDamage();
    VR_ART_Check(vr_ar_fx_sparks == s0 + 1 && vr_ar_fx_bled == b0 + 1,
        sprintf("a shot of 10 on a green cuirass: sparks (%g) and blood (%g: 7 got through)", vr_ar_fx_sparks - s0, vr_ar_fx_bled - b0));
    VR_AgArmor_Give(d, 3, 1023);
    s0 = vr_ar_fx_sparks;
    b0 = vr_ar_fx_bled;
    PositionalDamage(d, at, dir);
    ClearMultiDamage();
    trace_ent = d;
    trace_endpos = at;
    trace_plane_normal = dir * -1;
    float hd = d.health;
    TraceAttack(4, dir);
    ApplyMultiDamage();
    self = oldself;
    VR_ART_Check(vr_ar_fx_sparks == s0 + 1 && vr_ar_fx_bled == b0 && d.health == hd,
        sprintf("a pellet of 4 on a red cuirass: sparks (%g), no blood (%g), no damage (%g)", vr_ar_fx_sparks - s0, vr_ar_fx_bled - b0, hd - d.health));
    d.vr_ar_type = 0;
    d.vr_ar_mask = 0;
    s0 = vr_ar_fx_sparks;
    float bl = VR_AgArmor_HitFx(d, at, dir * -1, 10);
    VR_ART_Check(bl == 1 && vr_ar_fx_sparks == s0, sprintf("no armour: all blood (%g), no sparks", bl));
};

void() VR_AgArmor_FxTestEnd =
{
    remove(vr_aft_b);
    remove(vr_aft_d);
    cvar_set("vr_agarmor_rule", ftos(vr_aft_rule));
    cvar_set("vr_agarmor_helmet_head", ftos(vr_aft_helm));
    cvar_set("vr_agarmor_block_cuts", ftos(vr_aft_cuts));
    cvar_set("vr_agarmor_debris_time", ftos(vr_aft_dtime));
    vr_aft_busy = 0;
    dprint(sprintf("agarmor fx test: %g passed, %g failed\n", vr_art_pass, vr_art_fail));
};

// VR_AgArmor_TestFrame's `vr_agarmor_test 5` (start 1: the request just made).
void(float start) VR_AgArmor_FxTestFrame =
{
    entity p = VR_Player();
    if(start)
    {
        vr_art_pass = 0;
        vr_art_fail = 0;
        vr_aft_rule = cvar("vr_agarmor_rule");
        vr_aft_helm = cvar("vr_agarmor_helmet_head");
        vr_aft_cuts = cvar("vr_agarmor_block_cuts");
        vr_aft_dtime = cvar("vr_agarmor_debris_time");
        cvar_set("vr_agarmor_rule", "0");
        cvar_set("vr_agarmor_helmet_head", "1");
        cvar_set("vr_agarmor_block_cuts", "1");
        cvar_set("vr_agarmor_debris_time", "30");
        vr_aft_made0 = vr_ar_debris_made;
        vr_aft_a = VR_ART_SpawnKind(p, -60, 0, 120);
        vr_aft_b = VR_ART_SpawnKind(p, -20, 0, 120);
        vr_aft_c = VR_ART_SpawnKind(p, 20, 0, 120);
        vr_aft_d = VR_ART_SpawnKind(p, 60, 0, 120);
        vr_aft_busy = 1;
        vr_aft_until = time + 5;
        return; // (they start next frame: killed and knocked down then)
    }
    if(vr_aft_busy == 1)
    {
        vr_aft_a.health = 1;
        vr_aft_c.health = 1;
        T_Damage(vr_aft_a, p, p, 5);
        T_Damage(vr_aft_c, p, p, 5);
        if(!canragdoll(vr_aft_b) || !VR_Knockdown_MakeRoom(vr_aft_b))
        {
            VR_ART_Check(FALSE, "B can be knocked down");
        }
        vr_aft_b.velocity = normalize(vr_aft_b.origin - p.origin) * 220 + '0 0 120';
        dprint(sprintf("agarmor fx test: killed A (health %g), C (%g); B down %g\n", vr_aft_a.health, vr_aft_c.health,
            VR_Knockdown_Start(vr_aft_b)));
        vr_aft_busy = 2;
        return;
    }
    if(vr_aft_busy == 3)
    {
        // B, killed by its stopped cut, a corpse now: its limb (the plate broken) wears off.
        entity b = vr_aft_b;
        float dead = isragdoll(b) && (b.vr_corpse == 2 || b.vr_corpse_dying);
        if(!dead && time < vr_aft_until)
        {
            return;
        }
        VR_AFT_HitUntilCut(b, vr_aft_pb, 12);
        VR_ART_Check(dead && vr_aft_cut > 0, sprintf("the active ragdoll dead (%g): its limb (plate broken) off at hit %g", dead, vr_aft_cut));
        VR_AgArmor_FxTestEnd();
        return;
    }
    float ready = isragdoll(vr_aft_a) && (vr_aft_a.vr_corpse == 2 || vr_aft_a.vr_corpse_dying) && isragdoll(vr_aft_c) && (vr_aft_c.vr_corpse == 2 || vr_aft_c.vr_corpse_dying) && isragdoll(vr_aft_b) &&
        vr_aft_b.health > 0;
    if(!ready && time < vr_aft_until)
    {
        return;
    }
    if(!ready)
    {
        VR_ART_Check(FALSE, sprintf("timed out: the ragdolls (A %g corpse %g, B %g health %g, C %g corpse %g)", isragdoll(vr_aft_a),
            vr_aft_a.vr_corpse, isragdoll(vr_aft_b), vr_aft_b.health, isragdoll(vr_aft_c), vr_aft_c.vr_corpse));
        VR_AgArmor_FxTestEnd();
        return;
    }
    VR_AFT_Run(p);
    vr_aft_busy = 3;
    vr_aft_until = time + 5;
};

#ifdef QVR_TESTS
// vr_embed_test.qc -- Embedded blades, stage 1 (ROUND23.md, "Embedded blades"; vr_axestick.qc, vr_axestick.cpp).
// `vr_embed_test 1` (Debug > Tests > Embedded Blades; god, notarget, map vrtesthall, on your feet in the open): a thrown
// sword point first, flat and by the hilt at a wall; its sag; at a live body, and it bleeds; the body dying with it in; a
// corpse (and tugging it out), a knocked down body, an active ragdoll; a gib, a cut limb, a cut head letting it go; a plate
// and the same spot bare; what armed enemies and the drop cap see; an axe through its owner's death; vr_embed 0 as before.
// Ends `embedtest: N passed, M failed`.
// `vr_embed_test 6`: every other blade the game has here at the wall ahead: AD's swords point first, hammer, mace and flail
// not sticking (AD running: -Games), the crowbar and the chainsaw not, Super Axes by their edge (MG3 running): `embedkinds:
// N passed, M failed`.
// `vr_embed_test 2`: a sword in the wall ahead and one in a corpse beside it, for the eyes (`embedshot: ready`): you beside
// the one in the wall; `vr_embed_test 5`: you before the one in the corpse.
// `vr_embed_test 3`: the same plus one in a live grunt, each one's pose noted; then `vr_embed_test 4` after a save's load
// checks them (`embedload: N passed, M failed`).

float vr_et_on;      // the running test's number
float vr_et_case;
float vr_et_s;       // the case's state
float vr_et_t0;      // when the state began
float vr_et_v;       // a case's number kept across states
float vr_et_v2;
entity vr_et_m;      // the case's body
entity vr_et_w;      // the case's blade
entity vr_et_w2;
vector vr_et_at;
float vr_et_flags;   // the blade's weapon flags (AD's kinds)

float VR_ET_SPEED = 14;   // m/s (a hard throw: gravity hardly bends it on the way)
float VR_ET_GIVEUP = 10;  // seconds a state may last (a stuck test: FAIL "timed out")

void(float s) VR_ET_To =
{
    vr_et_s = s;
    vr_et_t0 = time;
};

// Everything the last case left: blades, its body.
void() VR_ET_Clean =
{
    for(entity e = find(world, classname, "thrown_weapon"); e; )
    {
        entity next = find(e, classname, "thrown_weapon");
        VR_AxeStick_EndTug(e);
        remove(e);
        e = next;
    }
    if(vr_et_m != world)
    {
        remove(vr_et_m);
        vr_et_m = world;
    }
    vr_et_w = world;
    vr_et_w2 = world;
};

void() VR_ET_NextCase =
{
    VR_ET_Clean();
    vr_et_case = vr_et_case + 1;
    VR_ET_To(0);
};

// A blade of weapon `xWid` thrown from `xFrom` at VR_ET_SPEED along `xGo`, its line (from the hand to its tip) along `xLine`.
entity(float xWid, vector xFrom, vector xGo, vector xLine, float xHurt) VR_ET_ThrowSword =
{
    entity t = CreateThrownWeapon(self, xWid, vr_et_flags, WeaponInst_New(0), xFrom, '0 0 0', '0 0 0', '0 0 0', FALSE, FALSE, 0);
    if(t == world)
    {
        return world; // (the pack not running)
    }
    vector m0 = normalize(weaponmuzzle(t) - t.origin); // (unturned: its line in its own axes)
    remove(t);
    vector fw = '1 0 0';
    vector up = '0 0 1';
    vector k;
    k_x = m0_y * xLine_z - m0_z * xLine_y;
    k_y = m0_z * xLine_x - m0_x * xLine_z;
    k_z = m0_x * xLine_y - m0_y * xLine_x;
    float deg = atan2(vlen(k), m0 * xLine) * (180 / 3.14159265);
    if(vlen(k) < 0.001)
    {
        k = '0 0 1'; // (along it, or back along it)
    }
    k = normalize(k);
    fw = VR_AxeStick_Turn(fw, k, deg);
    up = VR_AxeStick_Turn(up, k, deg);
    float speed = VR_ET_SPEED * VRMetersToUnits();
    entity w = CreateThrownWeapon(self, xWid, vr_et_flags, WeaponInst_New(0), xFrom, VR_AxeStick_Angles(fw, up), xGo * speed, '0 0 0', FALSE,
        xHurt, WeaponIdToThrowDamage(xWid));
    if(xHurt)
    {
        w.throwdmgspeed = 700 * VRMetersToUnits() / 120;
    }
    return w;
};

// An axe thrown so, its edge ahead (blade first, as vr_test_axe 0).
entity(vector xFrom, vector xGo, float xHurt) VR_ET_ThrowAxe =
{
    vector up = '0 0 1';
    vector fw = xGo * 0.9398 + up * 0.3417;
    vector u2 = up * 0.9398 - xGo * 0.3417;
    float speed = VR_ET_SPEED * VRMetersToUnits();
    return CreateThrownWeapon(self, WID_AXE, 0, WeaponInst_New(0), xFrom, VR_AxeStick_Angles(fw, u2), xGo * speed, '0 0 0', FALSE, xHurt,
        WeaponIdToThrowDamage(WID_AXE));
};

// A sword point first at `xAt`, from `xDist` units off along the way you face it from.
entity(vector xAt, float xDist, float xHurt) VR_ET_SwordAt =
{
    vector go = xAt - (self.origin + '0 0 22');
    go_z = 0;
    go = normalize(go);
    return VR_ET_ThrowSword(WID_SWORD, xAt - go * xDist, go, go, xHurt);
};

float(entity w) VR_ET_Resolved =
{
    return w.vr_stick_kind != 0 || time > vr_et_t0 + 1.2;
};

// A grunt ahead of you, fists only, plenty of health (vr_agarmor_test.qc's).
entity(float xAhead) VR_ET_Grunt =
{
    return VR_ART_SpawnKind(self, 0, 0, xAhead);
};

vector(entity m) VR_ET_Chest =
{
    return limbplace(m, 0, 3) + '0 0 6';
};

// You, moved `xDist` units from the wall ahead and facing it; the place on it level with your eyes.
vector(float xDist) VR_ET_FaceWall =
{
    makevectors('0 1 0' * self.v_viewangle_y);
    vector F = v_forward;
    vector eye = self.origin + '0 0 22';
    traceline(eye, eye + F * 4000, TRUE, self);
    vector at = trace_endpos;
    setorigin(self, self.origin + F * (4000 * trace_fraction - xDist));
    self.velocity = '0 0 0';
    return at;
};

// The sword in the wall `xW`: where it went in plus its depth is its tip, as the engine's QC builtin draws it.
void(entity xW) VR_ET_CheckTip =
{
    float tipOff = vlen(weaponmuzzle(xW) - xW.vr_stick_at) - xW.vr_stick_depth;
    VR_Test_Check(fabs(tipOff) < 1.5, sprintf("its tip %.1f units past where it went in, as deep as it says (%.1f)", tipOff + xW.vr_stick_depth,
        xW.vr_stick_depth));
};

void() VR_ET_Step =
{
    entity p = self;
    float h = cVR_OffHand;
    vector c;
    float t = time - vr_et_t0;

    // ---- 0: the wall, point first; its sag
    if(vr_et_case == 0)
    {
        if(vr_et_s == 0)
        {
            vr_et_at = VR_ET_FaceWall(220);
            vr_et_w = VR_ET_SwordAt(vr_et_at, 80, FALSE);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind == AXESTICK_LEVEL && vr_et_w.vr_stick_embed == AXESTICK_POINT, sprintf("a sword point first sticks in the wall (kind %g, how %g)", vr_et_w.vr_stick_kind, vr_et_w.vr_stick_embed));
            VR_ET_CheckTip(vr_et_w);
            vr_et_v = 0;
            vr_et_at = vr_et_w.vr_stick_at;
            VR_ET_To(2);
        }
        else if(vr_et_s == 2)
        {
            vr_et_v = max(vr_et_v, vr_et_w.vr_stick_sag);
            if(t > 0.5 && fabs(vr_et_w.vr_stick_sagv) < 0.5) // (settled)
            {
                float sagged = vr_et_w.vr_stick_sag;
                VR_Test_Check(sagged > 2 && sagged <= cvar_hget(cvarh_vr_embed_sag_most) + 0.01, sprintf("it sags %.1f degrees (most %g)", sagged, cvar_hget(cvarh_vr_embed_sag_most)));
                VR_Test_Check(vr_et_v > sagged + 0.5, sprintf("springing: it went to %.1f degrees first", vr_et_v));
                VR_Test_Check(vlen(vr_et_w.vr_stick_at - vr_et_at) < 0.5, "sagging about where it went in");
                vector out = vr_et_w.vr_stick_dir * -1;
                VR_Test_Check(out_z < 0, sprintf("its hilt lower than where it went in (%.2f)", out_z));
                VR_ET_CheckTip(vr_et_w);
                // The hilt's handle where it is drawn: a hand or a force grab goes by its origin.
                traceline(vr_et_w.vr_stick_at + out * 2, vr_et_w.origin, TRUE, vr_et_w);
                VR_Test_Check(trace_fraction == 1, "its handle out of the wall");
                VR_ET_NextCase();
            }
        }
        return;
    }

    // ---- 1, 2: the wall, flat and by the hilt
    if(vr_et_case == 1 || vr_et_case == 2)
    {
        if(vr_et_s == 0)
        {
            vr_et_at = VR_ET_FaceWall(220);
            makevectors('0 1 0' * self.v_viewangle_y);
            vector line = vr_et_case == 1 ? v_right : v_forward * -1;
            vr_et_w = VR_ET_ThrowSword(WID_SWORD, vr_et_at - v_forward * 80, v_forward, line, FALSE);
            VR_ET_To(1);
        }
        else if(VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind <= 0, sprintf("a sword %s bounces off the wall (kind %g)", vr_et_case == 1 ? "side on" : "by its hilt", vr_et_w.vr_stick_kind));
            VR_ET_NextCase();
        }
        return;
    }

    // ---- 3: a live body; it bleeds; dies with it in: it stays, rides, no more blood; gibbed: it falls
    if(vr_et_case == 3)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(200);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.1)
        {
            vr_et_w = VR_ET_SwordAt(VR_ET_Chest(vr_et_m), 72, TRUE);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && vr_et_w.vr_stick_ent == vr_et_m, sprintf("it sticks in a live body (kind %g)", vr_et_w.vr_stick_kind));
            VR_Test_Check(vr_et_m.health < 1000, sprintf("the blow on the way in hurt it (health %g)", vr_et_m.health));
            VR_ET_CheckTip(vr_et_w);
            vr_et_v = vr_et_m.health;
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && t >= 1.05)
        {
            float bled = vr_et_v - vr_et_m.health;
            VR_Test_Check(bled >= 3 && bled <= 8, sprintf("it bleeds it (%g in a second, the rate %g)", bled, cvar_hget(cvarh_vr_axestick_bleed)));
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2);
            VR_Test_Check(vr_et_m.health <= 0 && vr_et_m.modelindex != 0, "the body died whole");
            VR_ET_To(4);
        }
        else if(vr_et_s == 4 && t >= 1.2)
        {
            VR_Test_Check(VR_AxeStick_IsStuck(vr_et_w) && vr_et_w.vr_stick_ent == vr_et_m, sprintf("it stays in the dead body (kind %g, solid %g)", vr_et_w.vr_stick_kind, vr_et_m.solid));
            vr_et_v = vr_et_m.health;
            vr_et_at = vr_et_w.vr_stick_at;
            VR_Test_Check(vlen(vr_et_at - (vr_et_m.absmin + vr_et_m.absmax) * 0.5) < 80, sprintf("riding the body %.0f units from its middle", vlen(vr_et_at - (vr_et_m.absmin + vr_et_m.absmax) * 0.5)));
            VR_ET_To(5);
        }
        else if(vr_et_s == 5 && t >= 0.8)
        {
            VR_Test_Check(vr_et_m.health == vr_et_v, sprintf("no more bleeding in a dead body (%g, %g)", vr_et_v, vr_et_m.health));
            T_Damage(vr_et_m, p, p, 5000);
            VR_ET_To(6);
        }
        else if(vr_et_s == 6 && (vr_et_w.vr_stick_kind < 0 || t > 1))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind == AXESTICK_OUT, sprintf("the body gibbed: it falls (kind %g)", vr_et_w.vr_stick_kind));
            vr_et_m = world; // (gone by itself)
            VR_ET_NextCase();
        }
        return;
    }

    // ---- 4: a corpse lying down: thrown in; the grip must tug it (let go: nothing; pulled: the hand's)
    if(vr_et_case == 4)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(200);
            VR_ET_To(10);
        }
        else if(vr_et_s == 10 && t > 0.1)
        {
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && vr_et_m.solid == SOLID_NOT_BUT_TOUCHABLE && t >= 1)
        {
            vr_et_w = VR_ET_SwordAt(VR_ET_Chest(vr_et_m), 72, FALSE);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && vr_et_w.vr_stick_ent == vr_et_m, sprintf("it sticks in a corpse lying down (kind %g)", vr_et_w.vr_stick_kind));
            localcmd("+graboff\n");
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && VR_HandGrabUtil_IsHandGrabbing(p, h))
        {
            VR_Test_Check(VR_AxeStick_HandTug(vr_et_w, p, h), "a grip on it in a corpse starts a tug");
            VR_Test_Check(VR_Hand_Kind(p, h) == VR_HOLD_TUG, "the hand holds it for now");
            localcmd("-graboff\n");
            VR_ET_To(4);
        }
        else if(vr_et_s == 4 && !VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.1)
        {
            VR_Test_Check(VR_AxeStick_IsStuck(vr_et_w) && VR_Hand_Kind(p, h) == VR_HOLD_NONE, "let go: it stays, the hand free");
            localcmd("+graboff\n");
            VR_ET_To(5);
        }
        else if(vr_et_s == 5 && VR_HandGrabUtil_IsHandGrabbing(p, h))
        {
            VR_AxeStick_HandTug(vr_et_w, p, h);
            VR_ET_To(6);
        }
        else if(vr_et_s == 6 && t > 0.15)
        {
            VR_Test_Check(VR_AxeStick_IsStuck(vr_et_w), "a grip alone doesn't take it");
            makevectors('0 1 0' * p.v_viewangle_y);
            setorigin(p, p.origin - v_forward * 40); // (the hand with you: a good pull)
            vr_et_v = VRGetEntWeapon(p, h);
            VR_ET_To(7);
        }
        else if(vr_et_s == 7 && (VRGetEntWeapon(p, h) == WID_SWORD || t > 1))
        {
            VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD, sprintf("pulled: it comes into the hand (holds %g)", VRGetEntWeapon(p, h)));
            localcmd("-graboff\n");
            makevectors('0 1 0' * p.v_viewangle_y);
            setorigin(p, p.origin + v_forward * 40);
            VR_ET_To(8);
        }
        else if(vr_et_s == 8 && t > 0.1)
        {
            VR_ET_NextCase();
        }
        return;
    }

    // ---- 5, 6: a knocked down body, an active ragdoll
    if(vr_et_case == 5 || vr_et_case == 6)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(200);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.1)
        {
            makevectors('0 1 0' * p.v_viewangle_y);
            float started = vr_et_case == 5 ? VR_Knockdown_Start(vr_et_m) : VR_Knockdown_Stagger(vr_et_m, v_forward, 1, 0.5);
            VR_Test_Check(started, vr_et_case == 5 ? "knocked down" : "staggering as a ragdoll");
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && t > (vr_et_case == 5 ? 0.8 : 0.1))
        {
            // (Lying down: from above, the point meeting it first.)
            c = VR_ET_Chest(vr_et_m);
            vector down = c - (p.origin + '0 0 22');
            down_z = 0;
            down = normalize(normalize(down) + (vr_et_case == 5 ? '0 0 -1' : '0 0 0'));
            vr_et_w = VR_ET_ThrowSword(WID_SWORD, c - down * 64, down, down, TRUE);
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && vr_et_w.vr_stick_ent == vr_et_m, sprintf("it sticks in %s (kind %g, solid %g)", vr_et_case == 5 ? "a knocked down body" : "an active ragdoll", vr_et_w.vr_stick_kind, vr_et_m.solid));
            vr_et_v = vr_et_m.health;
            vr_et_at = vr_et_w.vr_stick_at;
            VR_ET_To(4);
        }
        else if(vr_et_s == 4 && t >= 0.6)
        {
            VR_Test_Check(vr_et_m.health < vr_et_v, sprintf("alive: it bleeds (%g to %g)", vr_et_v, vr_et_m.health));
            VR_Test_Check(vlen(vr_et_w.vr_stick_at - (vr_et_m.absmin + vr_et_m.absmax) * 0.5) < 96, "it rides the body");
            VR_ET_NextCase();
        }
        return;
    }

    // ---- 7, 8: a limb, the head: cut off, what is stuck in them falls
    if(vr_et_case == 7 || vr_et_case == 8)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(200);
            vr_et_v2 = 0;
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.1)
        {
            vr_et_v = vr_et_case == 8 ? ragdolllimbs(vr_et_m, 3) : -1;
            if(vr_et_case == 7)
            {
                // (Its arms and legs in turn, as the way to one may be blocked by another: the first the point reaches.)
                float bits = ragdolllimbs(vr_et_m, 0);
                for(float b = vr_et_v2 + 1; b < VR_LIMB_JOINTS && vr_et_v < 0; b++)
                {
                    if(bits & VR_AgArmor_Bit(b))
                    {
                        vr_et_v = b;
                    }
                }
                vr_et_v2 = vr_et_v;
            }
            VR_Test_Check(vr_et_v >= 0, sprintf("a %s to aim at (bone %g)", vr_et_case == 8 ? "head" : "limb", vr_et_v));
            if(vr_et_v < 0)
            {
                VR_ET_NextCase();
                return;
            }
            vr_et_w = VR_ET_SwordAt(limbplace(vr_et_m, vr_et_v, 3), 64, FALSE);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && VR_ET_Resolved(vr_et_w))
        {
            float bone = ragdolllimb(vr_et_m, vr_et_w.vr_stick_at);
            float takes = vr_et_case == 8 ? ragdolllimbs(vr_et_m, 1) : ragdolllimbs(vr_et_m, 4, bone);
            if(vr_et_case == 7 && (!VR_AxeStick_InMonster(vr_et_w) || bone < 0 || !(takes & VR_AgArmor_Bit(bone))) && vr_et_v < VR_LIMB_JOINTS - 1)
            {
                remove(vr_et_w);
                VR_ET_To(1);
                return;
            }
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && bone >= 0 && (takes & VR_AgArmor_Bit(bone)), sprintf("it sticks in that %s (kind %g, bone %g, aimed %g)", vr_et_case == 8 ? "head" : "limb", vr_et_w.vr_stick_kind, bone, vr_et_v));
            // A second blade in the same body, off that limb, stays.
            vr_et_v2 = bone;
            VR_ET_To(3);
        }
        else if(vr_et_s == 3)
        {
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2);
            makevectors('0 1 0' * p.v_viewangle_y);
            float cut = vr_et_case == 8 ? VR_Decap_Cut(vr_et_m, p, v_forward * 200, QVR_DECAP_THROWN) : VR_Limb_Cut(vr_et_m, p, v_forward * 200, QVR_DECAP_THROWN, vr_et_v2, FALSE);
            VR_Test_Check(cut, vr_et_case == 8 ? "the head is cut off" : "the limb is cut off");
            VR_ET_To(4);
        }
        else if(vr_et_s == 4 && (vr_et_w.vr_stick_kind < 0 || t > 0.5))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind == AXESTICK_OUT, sprintf("what was stuck in it falls (kind %g)", vr_et_w.vr_stick_kind));
            VR_ET_NextCase();
        }
        return;
    }

    // ---- 9: a plate: a spark, a bounce; the same place bare: it sticks
    if(vr_et_case == 9)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(200);
            VR_AgArmor_Give(vr_et_m, 1, 1023);
            vr_et_v2 = -1;
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.1)
        {
            // Its plates in turn, as a bare arm may be in the way of one: the first the point meets.
            vr_et_v2 = vr_et_v2 + 1;
            vr_et_v = vr_ar_fx_sparks;
            vr_et_at = limbpiece(vr_et_m, vr_et_v2, 10);
            vr_et_w = VR_ET_SwordAt(vr_et_at, 72, FALSE);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && VR_ET_Resolved(vr_et_w))
        {
            if(vr_ar_fx_sparks == vr_et_v && vr_et_v2 < VR_AR_PIECES - 1)
            {
                remove(vr_et_w);
                VR_ET_To(1);
                return;
            }
            VR_Test_Check(vr_et_w.vr_stick_kind <= 0, sprintf("an unbroken plate stops it (kind %g, plate %g)", vr_et_w.vr_stick_kind, vr_et_v2));
            VR_Test_Check(vr_ar_fx_sparks > vr_et_v, sprintf("with a spark (%g)", vr_ar_fx_sparks - vr_et_v));
            remove(vr_et_w);
            vr_et_m.vr_ar_mask = 0; // (the plates broken)
            vr_et_w = VR_ET_SwordAt(vr_et_at, 72, FALSE);
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w), sprintf("the plate broken: it sticks there (kind %g)", vr_et_w.vr_stick_kind));
            VR_ET_NextCase();
        }
        return;
    }

    // ---- 10: what armed enemies and the drop cap see
    if(vr_et_case == 10)
    {
        if(vr_et_s == 0)
        {
            vr_et_at = VR_ET_FaceWall(220);
            vr_et_w = VR_ET_SwordAt(vr_et_at, 80, FALSE);
            VR_ET_To(1);
        }
        else if(VR_ET_Resolved(vr_et_w))
        {
            vr_et_w.weaponinst.vr_enemy_drop = 1;
            VR_Test_Check(VR_AxeStick_Kept(vr_et_w), "a stuck blade is kept from armed enemies");
            VR_Test_Check(!VR_EnemyWeapons_Counted(vr_et_w), "and from the drop cap");
            vr_et_w.weaponinst.vr_enemy_drop = 0;
            VR_ET_NextCase();
        }
        return;
    }

    // ---- 11: an axe: into a live body, through its death
    if(vr_et_case == 11)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(200);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.1)
        {
            c = VR_ET_Chest(vr_et_m);
            vector go = normalize(c - (p.origin + '0 0 22'));
            vr_et_w = VR_ET_ThrowAxe(c - go * 64, go, FALSE);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && vr_et_w.vr_stick_embed == AXESTICK_EDGE, sprintf("an axe sticks by its edge (kind %g, how %g)", vr_et_w.vr_stick_kind, vr_et_w.vr_stick_embed));
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2);
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && t >= 0.6)
        {
            VR_Test_Check(VR_AxeStick_IsStuck(vr_et_w), sprintf("and stays in the dead body (kind %g)", vr_et_w.vr_stick_kind));
            VR_ET_NextCase();
        }
        return;
    }

    // ---- 15: never: a crowbar's and a chainsaw's point stick in nothing (before vr_embed 0's)
    if(vr_et_case == 12 && vr_et_v2 != 99)
    {
        if(vr_et_s == 0)
        {
            vr_et_at = VR_ET_FaceWall(220);
            makevectors('0 1 0' * self.v_viewangle_y);
            vr_et_w = VR_ET_ThrowSword(WID_CROWBAR, vr_et_at - v_forward * 80, v_forward, v_forward, FALSE);
            vr_et_w2 = VR_ET_ThrowSword(WID_CHAINSAW, vr_et_at + v_right * 40 - v_forward * 80, v_forward, v_forward, FALSE);
            VR_ET_To(1);
        }
        else if(VR_ET_Resolved(vr_et_w) && VR_ET_Resolved(vr_et_w2))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind <= 0 && vr_et_w2.vr_stick_kind <= 0, sprintf("a crowbar and a chainsaw never stick (%g, %g)", vr_et_w.vr_stick_kind, vr_et_w2.vr_stick_kind));
            VR_ET_Clean();
            vr_et_v2 = 99;
            VR_ET_To(0);
        }
        return;
    }

    // ---- 12..14: vr_embed 0: a sword doesn't stick; an axe does and falls at the death, nor in a corpse
    if(vr_et_case == 12)
    {
        if(vr_et_s == 0)
        {
            cvar_set("vr_embed", "0");
            vr_et_at = VR_ET_FaceWall(220);
            vr_et_w = VR_ET_SwordAt(vr_et_at, 80, FALSE);
            VR_ET_To(1);
        }
        else if(VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind == 0, sprintf("vr_embed 0: a sword doesn't stick (kind %g)", vr_et_w.vr_stick_kind));
            VR_ET_NextCase();
        }
        return;
    }
    if(vr_et_case == 13)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(200);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.1)
        {
            c = VR_ET_Chest(vr_et_m);
            vector go2 = normalize(c - (p.origin + '0 0 22'));
            vr_et_w = VR_ET_ThrowAxe(c - go2 * 64, go2, FALSE);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w), sprintf("vr_embed 0: an axe sticks in a body (kind %g)", vr_et_w.vr_stick_kind));
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2);
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && (vr_et_w.vr_stick_kind < 0 || t > 0.5))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind == AXESTICK_OUT, sprintf("and falls out as the body dies (kind %g)", vr_et_w.vr_stick_kind));
            VR_ET_NextCase();
        }
        return;
    }
    if(vr_et_case == 14)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(200);
            VR_ET_To(10);
        }
        else if(vr_et_s == 10 && t > 0.1)
        {
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && vr_et_m.solid == SOLID_NOT_BUT_TOUCHABLE && t >= 1)
        {
            c = VR_ET_Chest(vr_et_m);
            vector go3 = normalize(c - (p.origin + '0 0 22'));
            vr_et_w = VR_ET_ThrowAxe(c - go3 * 64, go3, FALSE);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(!VR_AxeStick_InMonster(vr_et_w), sprintf("vr_embed 0: nothing sticks in a corpse (kind %g)", vr_et_w.vr_stick_kind));
            cvar_set("vr_embed", "1");
            VR_ET_NextCase();
        }
        return;
    }

    VR_Test_Verdict();
    vr_et_on = 0;
};

// vr_embed_test 6: the kinds in turn (vr_et_case: 0 the plain sword's second model, then AD's by their kind, then the Super
// Axe), each at the wall.
void() VR_ET_Kinds =
{
    if(vr_et_s == 0)
    {
        vr_et_case = vr_et_case + 1;
        if(vr_et_case > VR_ADMELEE_LAST + 1)
        {
            VR_Test_Verdict();
            vr_et_on = 0;
            return;
        }
        vr_et_at = VR_ET_FaceWall(220);
        makevectors('0 1 0' * self.v_viewangle_y);
        if(vr_et_case == VR_ADMELEE_LAST + 1)
        {
            vr_et_flags = 0;
            vr_et_w = VR_ET_ThrowAxe(vr_et_at - v_forward * 64, v_forward, FALSE);
            vr_et_w2 = CreateThrownWeapon(self, WID_SUPERAXE, 0, WeaponInst_New(0), vr_et_at - v_forward * 64 + v_right * 60, vr_et_w.angles, v_forward * VR_ET_SPEED * VRMetersToUnits(), '0 0 0', FALSE, FALSE, 0);
            if(vr_et_w2 == world)
            {
                bprint("embedkinds: no Super Axe here (MG3 not running)\n");
                vr_et_on = 0;
                VR_Test_Verdict();
                return;
            }
            vr_et_w2.angles = vr_et_w.angles;
            remove(vr_et_w);
            vr_et_w = vr_et_w2;
            vr_et_w2 = world;
        }
        else
        {
            if(strstrofs(VR_ADMelee_Model(vr_et_case), "#melee", 0) < 0)
            {
                bprint(sprintf("embedkinds: skipped kind %g: its model isn't here (%s)\n", vr_et_case, VR_ADMelee_Model(vr_et_case)));
                return; // (AD not running: the next kind, next frame)
            }
            vr_et_flags = vr_et_case * QVR_WPNFLAG_SWORD_KIND_UNIT;
            vr_et_w = VR_ET_ThrowSword(WID_SWORD, vr_et_at - v_forward * 90, v_forward, v_forward, FALSE);
            vr_et_flags = 0;
        }
        VR_ET_To(1);
    }
    else if(VR_ET_Resolved(vr_et_w))
    {
        float never = vr_et_case == VR_ADMELEE_HAMMER || vr_et_case == VR_ADMELEE_MACE || vr_et_case == VR_ADMELEE_FLAIL;
        string name = vr_et_case == VR_ADMELEE_LAST + 1 ? "the Super Axe" : VR_ADMelee_Model(vr_et_case);
        if(never)
        {
            VR_Test_Check(vr_et_w.vr_stick_kind <= 0, sprintf("%s never sticks (kind %g)", name, vr_et_w.vr_stick_kind));
        }
        else
        {
            VR_Test_Check(vr_et_w.vr_stick_kind == AXESTICK_LEVEL, sprintf("%s sticks in the wall (kind %g, how %g, depth %.1f, sag %.1f)", name, vr_et_w.vr_stick_kind, vr_et_w.vr_stick_embed, vr_et_w.vr_stick_depth, vr_et_w.vr_stick_sag));
            if(vr_et_w.vr_stick_embed == AXESTICK_POINT)
            {
                VR_ET_CheckTip(vr_et_w);
            }
        }
        VR_ET_Clean();
        VR_ET_To(0);
    }
};

// You moved so that `xAt` is `xBack` units ahead of you and `xSide` to your left, your feet where they were in height, facing
// as you were (your head's own).
void(entity p, vector xAt, float xBack, float xSide) VR_ET_View =
{
    makevectors('0 1 0' * p.v_viewangle_y);
    vector to = xAt - v_forward * xBack + v_right * xSide;
    to_z = p.origin_z;
    setorigin(p, to);
    p.velocity = '0 0 0';
};

// vr_embed_test 2, 3: the pictures' and the save's scene: a sword in the wall ahead, one in a corpse beside it (and for 3, one
// in a live grunt too).
void() VR_ET_Scene =
{
    entity p = self;
    float t = time - vr_et_t0;
    if(vr_et_s == 0)
    {
        vr_et_at = VR_ET_FaceWall(160);
        makevectors('0 1 0' * p.v_viewangle_y);
        vector side = v_right * 40;
        vr_et_w = VR_ET_ThrowSword(WID_SWORD, vr_et_at + '0 0 30' - side - v_forward * 80, v_forward, v_forward, FALSE);
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && VR_ET_Resolved(vr_et_w) && t > 0.6)
    {
        vr_et_m = VR_ART_SpawnKind(p, 40, 0, 70);
        VR_ET_To(10);
    }
    else if(vr_et_s == 10 && t > 0.1)
    {
        vr_et_m.health = 1;
        T_Damage(vr_et_m, p, p, 2);
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && vr_et_m.solid == SOLID_NOT_BUT_TOUCHABLE && t >= 1.2)
    {
        vr_et_w2 = VR_ET_SwordAt(VR_ET_Chest(vr_et_m), 44, FALSE);
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && VR_ET_Resolved(vr_et_w2) && t > 0.4)
    {
        if(vr_et_on == 2)
        {
            // Before the sword in the wall, a little to its side: its sag seen.
            VR_ET_View(p, vr_et_w.origin, 44, 26);
            bprint(sprintf("embedshot: ready: wall kind %g sag %.1f, corpse kind %g\n", vr_et_w.vr_stick_kind, vr_et_w.vr_stick_sag, vr_et_w2.vr_stick_kind));
            vr_et_on = 0;
            return;
        }
        entity g = VR_ART_SpawnKind(p, -60, 0, 110);
        vr_et_v = 0;
        vr_et_m = g;
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && t > 0.1)
    {
        vr_et_w = VR_ET_SwordAt(VR_ET_Chest(vr_et_m), 64, FALSE);
        VR_ET_To(5);
    }
    else if(vr_et_s == 5 && VR_ET_Resolved(vr_et_w) && t > 0.3)
    {
        // Each blade's pose noted in its own fields (they survive a save).
        for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
        {
            if(!VR_AxeStick_IsStuck(e))
            {
                continue; // (a map's weapon lying about)
            }
            e.vr_stick_tugfrom = e.origin;
            bprint(sprintf("embedload: before: kind %g in %s, at %v, sag %.1f\n", e.vr_stick_kind, e.vr_stick_ent == world ? "the level" : e.vr_stick_ent.classname, e.origin, e.vr_stick_sag));
        }
        bprint("embedsave: ready\n");
        vr_et_on = 0;
    }
};

// vr_embed_test 4: after the load.
void() VR_ET_AfterLoad =
{
    VR_Test_BeginFlags("embedload", VR_TEST_LOUD);
    float n = 0;
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(e.vr_stick_tugfrom == '0 0 0')
        {
            continue; // (a map's weapon lying about)
        }
        n = n + 1;
        float inBody = VR_AxeStick_InMonster(e);
        bprint(sprintf("embedload: after: kind %g in %s, at %v, sag %.1f, moved %.1f\n", e.vr_stick_kind, e.vr_stick_ent == world ? "the level" : e.vr_stick_ent.classname, e.origin, e.vr_stick_sag, vlen(e.origin - e.vr_stick_tugfrom)));
        VR_Test_Check(VR_AxeStick_IsStuck(e) && e.vr_stick_embed == AXESTICK_POINT, "stuck, by its point, after the load");
        VR_Test_Check(e.movetype == MOVETYPE_NONE, "held there, not a body");
        VR_Test_Check(vlen(e.origin - e.vr_stick_tugfrom) < (inBody ? 48 : 1), sprintf("where it was (%.1f units)", vlen(e.origin - e.vr_stick_tugfrom)));
    }
    VR_Test_Check(n == 3, sprintf("the three blades there (%g)", n));
    VR_Test_Verdict();
};

void() VR_Embed_TestFrame =
{
    float ask = cvar_hget(cvarh_vr_embed_test);
    if(ask)
    {
        cvar_set("vr_embed_test", "0");
        if(ask == 1)
        {
            VR_Test_BeginFlags("embedtest", VR_TEST_LOUD);
            vr_et_on = 1;
            vr_et_case = 0;
            VR_ET_To(0);
        }
        if(ask == 2)
        {
            vr_et_on = 2;
            VR_ET_To(0);
        }
        if(ask == 3)
        {
            vr_et_on = 3;
            VR_ET_To(0);
        }
        if(ask == 4)
        {
            VR_ET_AfterLoad();
        }
        if(ask == 6)
        {
            VR_Test_BeginFlags("embedkinds", VR_TEST_LOUD);
            vr_et_on = 6;
            vr_et_case = 0;
            VR_ET_To(0);
        }
        if(ask == 5)
        {
            VR_ET_View(VR_Player(), vr_et_w2.vr_stick_at, 100, 0); // (far enough back to see it on the floor: the head isn't ours to tilt)
        }
    }
    if(!vr_et_on)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    if(vr_et_on == 1)
    {
        if(time - vr_et_t0 > VR_ET_GIVEUP)
        {
            VR_Test_Check(FALSE, sprintf("timed out: case %g state %g", vr_et_case, vr_et_s));
            cvar_set("vr_embed", "1");
            VR_ET_NextCase();
        }
        VR_ET_Step();
    }
    else if(vr_et_on == 6)
    {
        VR_ET_Kinds();
    }
    else
    {
        VR_ET_Scene();
    }
    self = oself;
};
#endif

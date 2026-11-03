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
// `vr_embed_test 7`: held stabs and swung axes (vr_embedhold.qc), the mock hand's real blows: too slow, off its line, blunt, an axe
// stabbed; a swung axe; a stab, deeper, out, pulled free; off its axis; let go; a grip again; a force grab; a plate; dying, gibbed,
// cut, gone with it held; a weapon switched in; its holder dead; the level's end; two hands; a corpse; a limb; vr_embed 0.
// Ends `embedheld: N passed, M failed`. `vr_embed_test 8`: the sword in the hand, a stab held and that blade let go of, a picture of each, for the eyes
// (`embedshot: done`; nothing behind it in the script: its waits would hold back the test's own commands).
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

.vector vr_et_noted; // (a blade's pose before a save)

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
        VR_Embed_DropHands(e, "test");
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
    float depth = xW.vr_stick_depth + xW.vr_stick_slide;
    float tipOff = vlen(weaponmuzzle(xW) - xW.vr_stick_at) - depth;
    VR_Test_Check(fabs(tipOff) < 1.5, sprintf("its tip %.1f units past where it went in, as deep as it says (%.1f)", tipOff + depth, depth));
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
            VR_Test_Check(VR_Embed_Grip(vr_et_w, p, h), "a grip on it in a corpse takes hold of it");
            VR_Test_Check(VR_Hand_Kind(p, h) == VR_HOLD_EMBED, "the hand holds it for now");
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
            VR_Embed_Grip(vr_et_w, p, h);
            VR_ET_To(6);
        }
        else if(vr_et_s == 6 && t > 0.15)
        {
            VR_Test_Check(VR_AxeStick_IsStuck(vr_et_w), "a grip alone doesn't take it");
            localcmd(sprintf("vr_mock_hand_glide 0.3\nvr_mock_hand_to off %v\n", VRGetEntHandPos(p, h) - vr_et_w.vr_stick_dir * 10)); // (a good pull)
            vr_et_v = VRGetEntWeapon(p, h);
            VR_ET_To(7);
        }
        else if(vr_et_s == 7 && (VRGetEntWeapon(p, h) == WID_SWORD || t > 1))
        {
            VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD, sprintf("pulled: it comes into the hand (holds %g)", VRGetEntWeapon(p, h)));
            localcmd("-graboff\nvr_mock_hand_glide 0\n");
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
            e.vr_et_noted = e.origin;
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
        if(e.vr_et_noted == '0 0 0')
        {
            continue; // (a map's weapon lying about)
        }
        n = n + 1;
        float inBody = VR_AxeStick_InMonster(e);
        bprint(sprintf("embedload: after: kind %g in %s, at %v, sag %.1f, moved %.1f\n", e.vr_stick_kind, e.vr_stick_ent == world ? "the level" : e.vr_stick_ent.classname, e.origin, e.vr_stick_sag, vlen(e.origin - e.vr_et_noted)));
        VR_Test_Check(VR_AxeStick_IsStuck(e) && e.vr_stick_embed == AXESTICK_POINT, "stuck, by its point, after the load");
        VR_Test_Check(e.movetype == MOVETYPE_NONE, "held there, not a body");
        VR_Test_Check(vlen(e.origin - e.vr_et_noted) < (inBody ? 48 : 1), sprintf("where it was (%.1f units)", vlen(e.origin - e.vr_et_noted)));
    }
    VR_Test_Check(n == 3, sprintf("the three blades there (%g)", n));
    VR_Test_Verdict();
};


// ---- vr_embed_test 7: held stabs and swung axes (vr_embedhold.qc), by the mock hand, the real blow.
float vr_eh_for;             // how long it takes
float vr_eh_flags;
vector vr_eh_fw;             // the way you face the body
vector vr_eh_want;           // the way its line is to lie
vector vr_eh_aim;            // the mock hand's aim that lays it so
vector vr_eh_end;
vector vr_eh_home;           // where you stood as the test began

void(float xWid, float xFlags) VR_EH_Give =
{
    VRSetEntWeapon(self, cVR_MainHand, xWid);
    VRSetEntWeaponFlags(self, cVR_MainHand, xFlags);
    VRSetEntWeaponInst(self, cVR_MainHand, WeaponInst_New(0));
    W_SetCurrentAmmoFor(cVR_MainHand);
};

void(vector xAt, float xFor) VR_EH_HandTo =
{
    localcmd(sprintf("vr_mock_hand_glide %g\nvr_mock_hand_to main %v\n", xFor, xAt));
};

entity() VR_EH_Blade =
{
    return VR_Hand_Kind(self, cVR_MainHand) == VR_HOLD_EMBED ? VR_Hand_BusyEnt(self, cVR_MainHand) : world;
};

// A case's blow (the state machine's first states, 0 to 3): a grunt ahead, weapon `xWid` in the gripping main hand, a hand's
// move `xSpeed` m/s at its chest: a stab (its line off the way it goes by `xOff` degrees) or, xSwing, across it, the head at the
// chest, the handle towards you. TRUE: done, whatever came of it (the blade: VR_EH_Blade).
float(float xWid, float xSpeed, float xOff, float xSwing, float t) VR_EH_Blow =
{
    if(vr_et_s == 0)
    {
        setorigin(self, vr_eh_home); // (where every case starts; the last's body may have punched you off it)
        self.velocity = '0 0 0';
        vr_et_m = VR_ET_Grunt(xWid == WID_AXE ? 44 : 54); // (the hand reaches 50 units from you; an axe is short)
        localcmd("+grabmain\n");
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(self, cVR_MainHand) && t > 0.1)
    {
        VR_EH_Give(xWid, vr_eh_flags);
        vector chest = VR_ET_Chest(vr_et_m);
        vector hand = VRGetHandPos(cVR_MainHand);
        vr_eh_fw = chest - hand;
        vr_eh_fw_z = 0;
        vr_eh_fw = normalize(vr_eh_fw);
        // Aimed along the way it goes (so far off as asked): below, by what it is seen to do.
        vr_eh_want = VR_AxeStick_Turn(vr_eh_fw, '0 0 1', xOff);
        vr_eh_aim = vr_eh_want;
        localcmd(sprintf("vr_mock_hand_aim main %v\n", hand + vr_eh_aim * 8000));
        vr_et_v2 = 0;
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && t > 0.15 && vr_et_v2 < 12 && normalize(VR_Blow(cVR_MainHand).mh_far - VR_Blow(cVR_MainHand).mh_grip) * vr_eh_want < 0.9985)
    {
        // Its line isn't its muzzle's aim (a sword is held up from it): turned by what's missing, until it lies so.
        vr_eh_aim = normalize(vr_eh_aim + vr_eh_want - normalize(VR_Blow(cVR_MainHand).mh_far - VR_Blow(cVR_MainHand).mh_grip));
        localcmd(sprintf("vr_mock_hand_aim main %v\n", VRGetHandPos(cVR_MainHand) + vr_eh_aim * 8000));
        vr_et_v2 = vr_et_v2 + 1;
        vr_et_t0 = time;
    }
    else if(vr_et_s == 2 && t > 0.15)
    {
        // Now it lies along the way (or off it): its length known, the hand put back so that the move ends in the body.
        entity b = VR_Blow(cVR_MainHand);
        vector line = b.mh_far - b.mh_grip;
        vector chest2 = VR_ET_Chest(vr_et_m);
        float dist = 36; // (long enough that the hand is up to its speed when it gets there)
        if(xSwing)
        {
            // (Across you: as the mock hand holds it, its edges face that way.)
            vector across = VR_AxeStick_Turn(vr_eh_fw, '0 0 1', 90);
            vr_eh_end = chest2 - line + across * 8;
            VR_EH_HandTo(vr_eh_end - across * dist, 0);
        }
        else
        {
            vr_eh_end = chest2 - line + vr_eh_fw * 10;
            VR_EH_HandTo(vr_eh_end - vr_eh_fw * dist, 0);
        }
        vr_eh_for = dist / VRMetersToUnits() / max(0.1, xSpeed);
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && t > 0.15)
    {
        if(vr_et_on == 8)
        {
            localcmd("screenshot\n"); // (the sword in the hand before: the same hand on the same sword after)
        }
        VR_EH_HandTo(vr_eh_end, vr_eh_for);
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && (VR_EH_Blade() != world || t > vr_eh_for + 0.1))
    {
        return TRUE;
    }
    return FALSE;
};

void() VR_EH_NextCase =
{
    localcmd("-grabmain\nvr_mock_hand_aim main off\nvr_mock_hand_glide 0\n");
    entity w = VR_EH_Blade();
    if(w != world)
    {
        VR_Embed_DropHands(w, "test");
    }
    VRSetHandEmpty(self, cVR_MainHand);
    vr_eh_flags = 0;
    // (A gib lying about would take the next blow.)
    for(entity g = nextent(world); g; )
    {
        entity gn = nextent(g);
        if(g.vr_gib)
        {
            remove(g);
        }
        g = gn;
    }
    VR_ET_NextCase();
};

// (States of the cases from here: 10 on.)
void() VR_EH_Step =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    entity w = VR_EH_Blade();
    // The body stands where it is, its blow's anger put off, in these cases (the hold's numbers the hand's alone; case 21 not).
    if(vr_et_m != world && vr_et_m.health > 0 && vr_et_case != 21)
    {
        vr_et_m.nextthink = time + 0.5;
        vr_et_m.velocity = '0 0 0';
    }

    // ---- 0: too slow: a blow, not stuck
    if(vr_et_case == 0)
    {
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 2.8, 0, FALSE, t))
            {
                VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_SWORD, sprintf("a stab at 2.8 m/s isn't stuck (holds %g)", VRGetEntWeapon(p, h)));
                VR_Test_Check(vr_et_m.health < vr_et_m.max_health, sprintf("it struck all the same (health %g of %g)", vr_et_m.health, vr_et_m.max_health));
                VR_EH_NextCase();
            }
        }
        return;
    }
    // ---- 1: off its line
    if(vr_et_case == 1)
    {
        if(VR_EH_Blow(WID_SWORD, 6, 60, FALSE, t))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_SWORD, sprintf("a blow 60 deg off its line isn't stuck (holds %g)", VRGetEntWeapon(p, h)));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 2: a blunt weapon
    if(vr_et_case == 2)
    {
        if(VR_EH_Blow(WID_CROWBAR, 6, 0, FALSE, t))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_CROWBAR, sprintf("a crowbar isn't stuck (holds %g)", VRGetEntWeapon(p, h)));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 3: an axe driven along its handle
    if(vr_et_case == 3)
    {
        if(VR_EH_Blow(WID_AXE, 8, 0, FALSE, t))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_AXE, sprintf("an axe stabbed isn't stuck (holds %g)", VRGetEntWeapon(p, h)));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 4: a slow axe swing
    if(vr_et_case == 4)
    {
        if(VR_EH_Blow(WID_AXE, 4, 0, TRUE, t))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_AXE, sprintf("an axe swung at 4 m/s isn't stuck (holds %g)", VRGetEntWeapon(p, h)));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 5: a hard axe swing: stuck; it holds, slides 5 cm at most, comes free
    if(vr_et_case == 5)
    {
        if(vr_et_s < 9)
        {
            if(VR_EH_Blow(WID_AXE, 11, 0, TRUE, t))
            {
                VR_Test_Check(w != world && VR_AxeStick_InMonster(w) && w.vr_stick_embed == AXESTICK_EDGE, sprintf("an axe swung hard is stuck by its edge (holds %g)", VRGetEntWeapon(p, h)));
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                vr_et_w = w;
                VR_Test_Check(VRIsHandWeaponless(p, h), "the hand holds it in the body, no weapon");
                VR_ET_To(9);
            }
        }
        else if(vr_et_s == 9 && t > vr_eh_for + 0.1)
        {
            // (Its blow's glide over.)
            vr_et_at = VRGetHandPos(h);
            vr_et_v = vr_et_w.vr_stick_slide;
            VR_EH_HandTo(vr_et_at - vr_et_w.vr_stick_dir * 0.5 * 0.01 * VRMetersToUnits(), 0); // (half a cm: under the stick)
            VR_ET_To(10);
        }
        else if(vr_et_s == 10 && t > 0.2)
        {
            VR_Test_Check(vr_et_w.vr_stick_slide == vr_et_v, sprintf("half a cm doesn't move it (slid %.2f to %.2f)", vr_et_v, vr_et_w.vr_stick_slide));
            VR_EH_HandTo(vr_et_at - vr_et_w.vr_stick_dir * 8 * 0.01 * VRMetersToUnits() * 2, 0.2);
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && (w == world || t > 1))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_AXE, sprintf("pulled a few cm: free, in the hand (holds %g)", VRGetEntWeapon(p, h)));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 6: a stab: stuck, deep by its speed, bleeding; deeper, out, the resistance; pulled out: free, no new strike
    if(vr_et_case == 6)
    {
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                VR_Test_Check(w != world && VR_AxeStick_InMonster(w) && w.vr_stick_embed == AXESTICK_POINT, sprintf("a stab at 6 m/s is stuck by its point (holds %g)", VRGetEntWeapon(p, h)));
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                vr_et_w = w;
                VR_Test_Check(vr_et_m.health < vr_et_m.max_health, sprintf("its blow dealt (health %g)", vr_et_m.health));
                VR_Test_Check(VR_Melee_Thing(p, h) == VR_MTHING_NONE, "the hand strikes nothing while it holds it");
                VR_Test_Check(VR_Hand_HeldOff(p, h), "the hand is held off");
                VR_Test_Check(VR_Hand_Free(p, VRGetOtherHand(h), 0), "the other hand is free (to climb, to grab)");
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10 && (vr_et_w.vr_stick_goal == 0 || t > 1))
        {
            float depth = vr_et_w.vr_stick_depth + vr_et_w.vr_stick_slide;
            float share = depth / vr_et_w.vr_stick_bladelen;
            VR_Test_Check(share >= 0.3 && share <= 0.62, sprintf("in by its speed: %.0f%% of the blade (%.1f of %.1f)", share * 100, depth, vr_et_w.vr_stick_bladelen));
            VR_ET_CheckTip(vr_et_w);
            vr_et_v = vr_et_w.vr_stick_slide;
            vr_et_v2 = vr_et_m.health;
            vr_et_at = VRGetHandPos(h);
            vector a0 = vr_et_w.vr_stick_dir;
            VR_EH_HandTo(vr_et_at + a0 * 6, 0.3); // (6 units deeper)
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && t > 0.6)
        {
            VR_Test_Check(vr_et_w.vr_stick_slide > vr_et_v + 1, sprintf("pushed: it slides deeper (%.1f to %.1f; the hand moved %.1f)", vr_et_v, vr_et_w.vr_stick_slide, vlen(VRGetHandPos(h) - vr_et_at)));
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && VR_EH_Blade() == vr_et_w, "held still");
            VR_Test_Check(vr_et_m.health >= vr_et_v2 - 5, sprintf("sliding deeper deals nothing but the bleeding (%g to %g)", vr_et_v2, vr_et_m.health));
            // Resistance: by how deep.
            float sShallow = VR_Embed_Stick(vr_et_w, 0, FALSE);
            float sDeep = VR_Embed_Stick(vr_et_w, 1, FALSE);
            float rShallow = VR_Embed_Rate(vr_et_w, 0, FALSE);
            float rDeep = VR_Embed_Rate(vr_et_w, 1, FALSE);
            VR_Test_Check(sDeep > sShallow && rDeep < rShallow, sprintf("the deeper the harder: stick %.1f to %.1f cm, slide %.0f to %.0f cm/s", sShallow, sDeep, rShallow, rDeep));
            VR_Test_Check(VR_Embed_Stick(vr_et_w, 0.5, TRUE) < VR_Embed_Stick(vr_et_w, 0.5, FALSE), "less in a dead body");
            vr_et_v = vr_et_w.vr_stick_slide;
            vr_et_at = VRGetHandPos(h);
            VR_EH_HandTo(vr_et_at - vr_et_w.vr_stick_dir * 7, 0.3); // (a little out)
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && t > 0.6)
        {
            VR_Test_Check(vr_et_w.vr_stick_slide < vr_et_v - 1 && VR_EH_Blade() == vr_et_w, sprintf("pulled a little: it slides out, still in (%.1f to %.1f)", vr_et_v, vr_et_w.vr_stick_slide));
            vr_et_v2 = vr_et_m.health;
            vr_et_at = VRGetHandPos(h);
            VR_EH_HandTo(vr_et_at - vr_et_w.vr_stick_dir * 44, 1.6); // (slowly, right out)
            VR_ET_To(13);
        }
        else if(vr_et_s == 13 && (w == world || t > 4))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_SWORD, sprintf("pulled right out: free, in the hand (holds %g)", VRGetEntWeapon(p, h)));
            vr_et_v = vr_et_m.health;
            VR_EH_HandTo(VRGetHandPos(h) - vr_eh_fw * 4, 0.05);
            VR_ET_To(14);
        }
        else if(vr_et_s == 14 && t > 0.12)
        {
            VR_Test_Check(vr_et_m.health == vr_et_v, sprintf("pulling out struck nothing (%g to %g)", vr_et_v, vr_et_m.health));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 7: off its axis: it comes free
    if(vr_et_case == 7)
    {
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                vr_et_w = w;
                VR_Test_Check(w != world, "stuck");
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            vector side = '0 0 1';
            VR_EH_HandTo(VRGetHandPos(h) + side * 3, 0); // (a little: 3 units, under its break)
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && t > 0.15)
        {
            VR_Test_Check(VR_EH_Blade() == vr_et_w, "a little off its axis: still held");
            VR_EH_HandTo(VRGetHandPos(h) + '0 0 1' * 12, 0.05);
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && (w == world || t > 0.5))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_SWORD, sprintf("pulled hard off its axis: free, in the hand (holds %g)", VRGetEntWeapon(p, h)));
            vr_et_v = vr_et_m.health;
            VR_EH_HandTo(VRGetHandPos(h) + vr_eh_fw * 6, 0.1); // (a fast yank back the way it came, within the moment)
            VR_ET_To(13);
        }
        else if(vr_et_s == 13 && t > 0.15)
        {
            VR_Test_Check(vr_et_m.health == vr_et_v, sprintf("a yank just out strikes nothing (%g to %g)", vr_et_v, vr_et_m.health));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 8: let go: it stays, not thrown; a grip takes it again on the same axis; let go; a force grab tugs it out
    if(vr_et_case == 8)
    {
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                vr_et_w = w;
                VR_Test_Check(w != world, "stuck");
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            vr_et_at = vr_et_w.vr_stick_ang;
            localcmd("-grabmain\n");
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && !VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.1)
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && vr_et_w.movetype == MOVETYPE_NONE, sprintf("let go: it stays in the body, not thrown (kind %g)", vr_et_w.vr_stick_kind));
            VR_Test_Check(VR_Hand_Free(p, h, 0) && VRIsHandWeaponless(p, h), "the hand free, empty");
            vr_et_v = vr_et_m.health;
            localcmd("vr_mock_hand_aim main off\n");
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && t > 1.1)
        {
            VR_Test_Check(vr_et_m.health <= vr_et_v - 3, sprintf("it bleeds it left in (%g to %g)", vr_et_v, vr_et_m.health));
            vr_eh_end = vr_et_w.origin;
            VR_EH_HandTo(vr_eh_end, 0);
            vr_et_v2 = 0;
            VR_ET_To(13);
        }
        else if(vr_et_s == 13 && t > 0.1 && vlen(VRGetHandPos(h) - vr_et_w.origin) > 2 && vr_et_v2 < 10)
        {
            // (Where the hand is put isn't quite where it is, turned as it is now: told by how far off.)
            vr_eh_end = vr_eh_end + vr_et_w.origin - VRGetHandPos(h);
            VR_EH_HandTo(vr_eh_end, 0);
            vr_et_v2 = vr_et_v2 + 1;
            vr_et_t0 = time;
        }
        else if(vr_et_s == 13 && t > 0.1)
        {
            localcmd("+grabmain\n");
            VR_ET_To(14);
        }
        else if(vr_et_s == 14 && (VR_EH_Blade() != world || t > 0.5))
        {
            VR_Test_Check(VR_EH_Blade() == vr_et_w, sprintf("a grip on it takes it again (kind %g)", VR_Hand_Kind(p, h)));
            VR_Test_Check(vr_et_w.vr_stick_ang == vr_et_at, "on the same axis (its turn on the limb as it stuck)");
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w), "still in the body");
            localcmd("-grabmain\n");
            VR_ET_To(15);
        }
        else if(vr_et_s == 15 && !VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.1)
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && VR_Hand_Free(p, h, 0), "let go again: it stays");
            VR_AxeStick_Tug(vr_et_w, p, h); // (the force grab's flick)
            VR_ET_To(16);
        }
        else if(vr_et_s == 16 && (vr_et_w.vr_stick_kind == AXESTICK_OUT || t > 1))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind == AXESTICK_OUT, sprintf("a force grab tugs it out (kind %g)", vr_et_w.vr_stick_kind));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 9: a plate: a spark, not stuck (a hand's blow at the place on its body a plate is); the plates broken: stuck there
    if(vr_et_case == 9)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(54);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.1)
        {
            VR_AgArmor_Give(vr_et_m, 1, 1023);
            vector to = normalize(p.origin - vr_et_m.origin);
            float found = FALSE;
            vr_et_v = vr_ar_fx_sparks;
            vector at = '0 0 0';
            for(float z = -14; z <= 26 && !found; z = z + 2)
            {
                at = vr_et_m.origin + to * 10 + '0 0 1' * z;
                entity pw = CreateThrownWeapon(p, WID_SWORD, 0, WeaponInst_New(0), at + to * 20, '0 0 0', '0 0 0', '0 0 0', FALSE, FALSE, 0);
                pw.throwhit = QVR_THROWHIT_HIT;
                vr_embed_stabbing = TRUE;
                float stuck9 = embedstick(pw, vr_et_m, at, to * -200, -1);
                vr_embed_stabbing = FALSE;
                found = !stuck9;
                remove(pw);
            }
            VR_Test_Check(found, sprintf("a plate stops a hand's stab at %v", at));
            VR_Test_Check(vr_ar_fx_sparks > vr_et_v, sprintf("with a spark (%g)", vr_ar_fx_sparks - vr_et_v));
            vr_et_m.vr_ar_mask = 0;
            entity pw2 = CreateThrownWeapon(p, WID_SWORD, 0, WeaponInst_New(0), at + to * 20, '0 0 0', '0 0 0', '0 0 0', FALSE, FALSE, 0);
            pw2.throwhit = QVR_THROWHIT_HIT;
            vr_embed_stabbing = TRUE;
            VR_Test_Check(embedstick(pw2, vr_et_m, at, to * -200, -1), "the plates broken: it stays in, there");
            vr_embed_stabbing = FALSE;
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 10: dies with it held: the hold goes on on the corpse, a light tug takes it
    if(vr_et_case == 10)
    {
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                vr_et_w = w;
                VR_Test_Check(w != world, "stuck");
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10 && t > vr_eh_for + 0.1)
        {
            // (While it falls, a nudge of you by it isn't what's looked at here: a long tug, then its own.)
            cvar_set("vr_embed_tug_corpse", "40");
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2);
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && t > 0.5)
        {
            VR_Test_Check(vr_et_m.health <= 0 && VR_EH_Blade() == vr_et_w && VR_AxeStick_InMonster(vr_et_w), sprintf("dead: held still (health %g, kind %g)", vr_et_m.health, VR_Hand_Kind(p, h)));
            cvar_set("vr_embed_tug_corpse", "8");
            vr_et_w.vr_hold_pulled = 0;
            VR_EH_HandTo(VRGetHandPos(h) - vr_et_w.vr_stick_dir * 8, 0.3); // (about 30 cm of hand)
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && (w == world || t > 1.5))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_SWORD, sprintf("a tug: in the hand (holds %g)", VRGetEntWeapon(p, h)));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 11: gibbed with it held: the hand has it back, nothing stuck
    if(vr_et_case == 11 || vr_et_case == 12 || vr_et_case == 13)
    {
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                vr_et_w = w;
                VR_Test_Check(w != world, "stuck");
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            if(vr_et_case == 11)
            {
                vr_et_m.health = 1;
                T_Damage(vr_et_m, p, p, 500);
            }
            else if(vr_et_case == 12)
            {
                // The limb or head it is in cut off (their mark, VR_Limb_Cut's and VR_Decap_Cut's: case 7, 8 of test 1).
                vr_et_w.vr_stick_cutmark = time;
                VR_AxeStick_CutDone(vr_et_m);
            }
            else
            {
                remove(vr_et_m); // (the body gone)
                vr_et_m = world;
            }
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && (w == world || t > 0.5) && t > 0.05)
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_SWORD, sprintf("%s: the hand has it back (holds %g)", vr_et_case == 11 ? "gibbed" : (vr_et_case == 12 ? "cut off" : "the body gone"), VRGetEntWeapon(p, h)));
            float stuck = 0;
            for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
            {
                stuck = stuck + 1;
            }
            VR_Test_Check(stuck == 0, sprintf("no blade left behind (%g)", stuck));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 14: a weapon put in the hand; its holder dying: it stays; the level ends: the hand has it
    if(vr_et_case == 14 || vr_et_case == 15 || vr_et_case == 16)
    {
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                vr_et_w = w;
                VR_Test_Check(w != world, "stuck");
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            if(vr_et_case == 14)
            {
                VR_EH_Give(WID_SHOTGUN, 0);
            }
            else if(vr_et_case == 15)
            {
                vr_et_v = p.health;
                p.health = -1;
            }
            else
            {
                VR_Embed_LevelEnd(p);
                self = p;
            }
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && t > 0.05)
        {
            if(vr_et_case == 14)
            {
                VR_Test_Check(VRGetEntWeapon(p, h) == WID_SHOTGUN && VR_AxeStick_InMonster(vr_et_w) && !VR_Embed_Held(vr_et_w), "a weapon switched in: the hand has it, the blade stays");
            }
            else if(vr_et_case == 15)
            {
                p.health = vr_et_v;
                VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && !VR_Embed_Held(vr_et_w) && VR_Hand_Free(p, h, 0), "its holder dead: the blade stays, the hand free");
            }
            else
            {
                VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD && !VR_Hand_HeldOff(p, h), sprintf("the level ends: the hand has it (holds %g)", VRGetEntWeapon(p, h)));
            }
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 17: two hands on it: one lets go, it holds; both, it stays
    if(vr_et_case == 17)
    {
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                vr_et_w = w;
                VR_Test_Check(w != world, "stuck");
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                localcmd("+graboff\n");
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10 && VR_HandGrabUtil_IsHandGrabbing(p, cVR_OffHand) && vr_et_w.vr_stick_goal == 0)
        {
            VR_Test_Check(VR_Embed_Grip(vr_et_w, p, cVR_OffHand), "a second hand takes hold");
            localcmd("-grabmain\n");
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && !VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.1)
        {
            VR_Test_Check(VR_Hand_Free(p, h, 0) && VR_Hand_Kind(p, cVR_OffHand) == VR_HOLD_EMBED && VR_Embed_Held(vr_et_w), "one hand opens: the other holds it still");
            localcmd("-graboff\n");
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && !VR_HandGrabUtil_IsHandGrabbing(p, cVR_OffHand) && t > 0.1)
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && !VR_Embed_Held(vr_et_w), "both open: it stays");
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 18: a stab at a corpse; 19: a blade in an arm is loose on it, one in the chest not
    if(vr_et_case == 18)
    {
        if(vr_et_s == 1 && vr_et_m.health > 0)
        {
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2);
        }
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                // (Lying now: the blow went over it, or not; it's the corpse path's, so aimed at where it lies, directly below.)
                float b0 = FALSE;
                if(w == world)
                {
                    vr_embed_host = vr_et_m;
                    vr_embed_hostmodel = vr_et_m.modelindex;
                    vr_embed_at = VR_ET_Chest(vr_et_m);
                    vr_embed_vel = vr_eh_fw * 6 * VRMetersToUnits();
                    VR_Embed_BlowsDone(h);
                    b0 = TRUE;
                }
                w = VR_EH_Blade();
                VR_Test_Check(w != world && VR_AxeStick_InMonster(w), sprintf("a body lying down takes it too (holds %g, by hand %g)", VRGetEntWeapon(p, h), b0));
                VR_EH_NextCase();
            }
        }
        return;
    }
    if(vr_et_case == 19)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(72);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.1)
        {
            // (Stuck where, test 1's cases 7 and 8 check: the rule here.)
            float heads = ragdolllimbs(vr_et_m, 1);
            float head = -1;
            float limb = -1;
            for(float j = 0; j < VR_LIMB_JOINTS; ++j)
            {
                if(heads & VR_AgArmor_Bit(j))
                {
                    head = j;
                }
                else if(limb < 0)
                {
                    limb = j;
                }
            }
            VR_Test_Check(!VR_AxeStick_LooseIn(vr_et_m, -1), "the torso: rigid");
            VR_Test_Check(head >= 0 && !VR_AxeStick_LooseIn(vr_et_m, head), sprintf("the head: rigid (bone %g)", head));
            VR_Test_Check(limb >= 0 && VR_AxeStick_LooseIn(vr_et_m, limb), sprintf("an arm or a leg: loose (bone %g)", limb));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 20: vr_embed 0: the same stab: a blow as ever
    if(vr_et_case == 20)
    {
        if(vr_et_s == 0)
        {
            cvar_set("vr_embed", "0");
        }
        if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_SWORD, sprintf("vr_embed 0: nothing stays in (holds %g)", VRGetEntWeapon(p, h)));
            VR_Test_Check(vr_et_m.health < vr_et_m.max_health, sprintf("its blow all the same (health %g)", vr_et_m.health));
            cvar_set("vr_embed", "1");
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 21: the body taken away from the hand: it comes free, into the hand where it is
    if(vr_et_case == 21)
    {
        if(vr_et_s < 10)
        {
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                vr_et_w = w;
                VR_Test_Check(w != world, "stuck");
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10 && t > vr_eh_for + 0.1)
        {
            vr_et_at = VRGetHandPos(h);
            setorigin(vr_et_m, vr_et_m.origin + vr_eh_fw * 40);
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && (w == world || t > 0.5))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_SWORD, sprintf("the body gone from it: free, in the hand (holds %g)", VRGetEntWeapon(p, h)));
            VR_Test_Check(vlen(VRGetHandPos(h) - vr_et_at) < 3, sprintf("the hand where it was (%.1f)", vlen(VRGetHandPos(h) - vr_et_at)));
            VR_EH_NextCase();
        }
        return;
    }
    localcmd("vr_mock_hand_to_absolute 0\n");
    cvar_set("vr_knockdown", "1");
    vr_et_on = 0;
    VR_Test_Verdict();
};

// vr_embed_test 8: a stab held, for the eyes (`embedshot: held`); vr_embed_test 9: let go (`embedshot: left`).
void() VR_EH_Scene =
{
    float t = time - vr_et_t0;
    entity w = VR_EH_Blade();
    if(vr_et_s < 10)
    {
        if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
        {
            vr_et_w = w;
            VR_ET_To(10);
        }
    }
    else if(vr_et_s == 10 && (w == world || vr_et_w.vr_stick_goal == 0))
    {
        bprint(sprintf("embedshot: held: %s, %.1f units in, its hilt %v, tip %v, where it went in %v\n", w != world ? w.netname : "NOT STUCK", w.vr_stick_depth + w.vr_stick_slide, w.origin, weaponmuzzle(w), w.vr_stick_at));
        bprint(sprintf("embedshot: the hand %v, the body at %v\n", VRGetHandPos(cVR_MainHand), vr_et_m.origin));
        vr_et_m.nextthink = time + 60; // (it stands for the pictures)
        vr_et_on = 0;
        // (Commands here: a script's wait would hold back this test's own, which queue behind it.)
        localcmd("vr_mock_hand head 0.7 1.25 -0.5 25 30 0\nwait10\nscreenshot\nvr_embed_test 9\n");
    }
    if(vr_et_on == 8 && vr_et_m != world)
    {
        vr_et_m.nextthink = time + 0.5;
    }
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
        if(ask == 7)
        {
            VR_Test_BeginFlags("embedheld", VR_TEST_LOUD);
            cvar_set("vr_knockdown", "0"); // (a blow's chance of one takes the body from the hand)
            vr_eh_home = VR_Player().origin;
            localcmd("vr_mock_hand_to_absolute 1\n");
            vr_et_on = 7;
            vr_et_case = 0;
            VR_ET_To(0);
        }
        if(ask == 8)
        {
            vr_eh_home = VR_Player().origin;
            localcmd("vr_mock_hand_to_absolute 1\n");
            vr_et_on = 8;
            VR_ET_To(0);
        }
        if(ask == 9)
        {
            bprint(sprintf("embedshot: going to let go: its hilt %v, tip %v, the hand %v\n", vr_et_w.origin, weaponmuzzle(vr_et_w), VRGetEntHandPos(VR_Player(), cVR_MainHand)));
            localcmd("-grabmain\nvr_mock_hand_aim main off\nwait40\nscreenshot\nwait2\nvr_embed_test 10\n");
        }
        if(ask == 10)
        {
            bprint(sprintf("embedshot: let go: its hilt %v, tip %v, kind %g\n", vr_et_w.origin, weaponmuzzle(vr_et_w), vr_et_w.vr_stick_kind));
            bprint("embedshot: done\n");
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
    else if(vr_et_on == 7)
    {
        if(time - vr_et_t0 > VR_ET_GIVEUP)
        {
            VR_Test_Check(FALSE, sprintf("timed out: case %g state %g", vr_et_case, vr_et_s));
            cvar_set("vr_embed", "1");
            VR_EH_NextCase();
        }
        VR_EH_Step();
    }
    else if(vr_et_on == 8)
    {
        VR_EH_Scene();
    }
    else
    {
        VR_ET_Scene();
    }
    self = oself;
};
#endif

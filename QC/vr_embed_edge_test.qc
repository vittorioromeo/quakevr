#ifdef QVR_TESTS
// vr_embed_edge_test.qc -- Embedded blades, stage 4: the edges (ROUND23.md, "Embedded blades, stage 4"), by the mock hand's real blows.
//
// `vr_embed_test 13` (god, notarget, vrtesthall; ends `embededge: N passed, M failed`): vr_embed off during a hold; a body and its holder
// through a teleporter; armed enemies and the drop cap; a body going; two hands; stabs on a knocked down body and an active ragdoll; the
// other hand grabbing the body; bullet time.
// `vr_embed_test 14`: a stab held (`embedsave: ready`), `16`: staggering, a spring on it, `17`: left in a staggering body. Then a save and
// a load, and `vr_embed_test 15` (`embedafter: N passed, M failed`): nothing lost, it comes out into the hand. Or a level change, and
// `vr_embed_test 18` (held: in the hand) or `20` (left: what you have).
// `vr_embed_test 19`: the map's own Ogre or Shambler or boss (`embedheavy: N passed, M failed`).

float vr_ee_mode;      // the save's scene (14, 16, 17)
float vr_ee_real;      // a slide's way, what a case compares
vector vr_ee_offhome;  // where the off hand is left between cases
entity vr_ee_m2;       // a second body

void() VR_EE_Verdict =
{
    localcmd("vr_mock_hand_to_absolute 0\nvr_slowmo 1\n");
    cvar_set("vr_embed", "1");
    vr_et_on = 0;
    VR_Test_Verdict();
};

void() VR_EE_NextCase =
{
    localcmd(sprintf("-grabmain\n-graboff\nvr_mock_hand_aim main off\nvr_mock_hand_glide 0\nvr_slowmo 1\nvr_mock_hand_to off %v\nvr_mock_walk_to off\nvr_mock_stick off 0 0\n", vr_ee_offhome));
    entity w = VR_EH_Blade();
    if(w != world)
    {
        VR_Embed_DropHands(w, "test");
    }
    VRSetHandEmpty(self, cVR_MainHand);
    VRSetHandEmpty(self, cVR_OffHand);
    vr_eh_down = 0;
    vr_ee_target = world;
    vr_et_v = 0;
    vr_et_v2 = 0;
    localcmd("tune vr_embed_stagger_sword 0.6\n");
    if(vr_ee_m2 != world)
    {
        remove(vr_ee_m2);
        vr_ee_m2 = world;
    }
    VR_EH_ClearGibs();
    VR_ET_NextCase();
};

// A case's stab, to state 10: the blade in vr_et_w. FALSE until then, or no blade (the case over).
void(float xSpeed, float t) VR_EE_Stab =
{
    if(VR_EH_Blow(WID_SWORD, xSpeed, 0, FALSE, t))
    {
        vr_et_w = VR_EH_Blade();
        VR_Test_Check(vr_et_w != world, sprintf("case %g: stuck (holds %g)", vr_et_case, VRGetEntWeapon(self, cVR_MainHand)));
        if(vr_et_w == world)
        {
            VR_EE_NextCase();
            return;
        }
        VR_ET_To(10);
    }
};

// A real teleporter's touch (teleport_touch: not the trigger's box, a mock walks in no straight line) takes `xWho` `xBy` from where it is.
void(entity xWho, vector xBy) VR_EE_Teleport =
{
    entity d = spawn();
    d.classname = "info_teleport_destination";
    d.targetname = "vr_ee_dest";
    setorigin(d, xWho.origin + xBy);
    entity t = spawn();
    t.classname = "trigger_teleport";
    t.target = "vr_ee_dest";
    entity was = self;
    entity wasOther = other;
    self = t;
    other = xWho;
    teleport_touch();
    self = was;
    other = wasOther;
    remove(t);
    remove(d);
};

// Nothing left behind, the sword in the main hand.
void(string xWhat) VR_EE_InHand =
{
    entity p = self;
    float h = cVR_MainHand;
    VR_Test_Check(VR_EH_Blade() == world && VRGetEntWeapon(p, h) == WID_SWORD && !VR_Hand_HeldOff(p, h), sprintf("%s: in the hand (holds %g, kind %g)", xWhat, VRGetEntWeapon(p, h), VR_Hand_Kind(p, h)));
    float stuck = 0;
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        stuck = stuck + 1;
    }
    VR_Test_Check(stuck == 0, sprintf("%s: no blade left behind (%g)", xWhat, stuck));
};

void() VR_EE_Rest;

void() VR_EE_Step =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    float off = cVR_OffHand;
    entity w = VR_EH_Blade();
    entity m = vr_et_m;
    float c = vr_et_case;
    // The body stands where it is unless a case is about it.
    if(m != world && m.health > 0 && c != 8 && c != 9 && c != 12 && !m.vr_knockdown)
    {
        m.nextthink = time + 0.5;
        m.velocity = '0 0 0';
    }

    // ---- 0: vr_embed switched off during a hold: it ends, in the hand
    if(c == 0)
    {
        if(vr_et_s < 10)
        {
            VR_EE_Stab(6, t);
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            cvar_set("vr_embed", "0");
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && (w == world || t > 1))
        {
            VR_EE_InHand("vr_embed off during a hold");
            cvar_set("vr_embed", "1");
            VR_EE_NextCase();
        }
        return;
    }
    // ---- 1: the body through a teleporter during a hold; 2: its holder through one
    if(c == 1 || c == 2)
    {
        if(vr_et_s < 10)
        {
            VR_EE_Stab(6, t);
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            makevectors('0 1 0' * p.angles_y);
            VR_EE_Teleport(c == 1 ? m : p, c == 1 ? v_forward * 300 : v_forward * -300);
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && (w == world || t > 2))
        {
            VR_EE_InHand(c == 1 ? "the body teleported" : "the holder teleported");
            VR_Test_Check(ragdolltug(m, p, h, 1) < 0, "no spring left on it");
            VR_EE_NextCase();
        }
        return;
    }
    // ---- 3: left in a body, the body through a teleporter: it goes with it; a grip there takes it
    if(c == 3)
    {
        if(vr_et_s < 10)
        {
            VR_EE_Stab(6, t);
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            localcmd("-grabmain\n");
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && !VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.1)
        {
            vr_et_v = vlen(vr_et_w.origin - m.origin);
            makevectors('0 1 0' * p.angles_y);
            VR_EE_Teleport(m, v_right * 200);
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && t > 0.3)
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && fabs(vlen(vr_et_w.origin - m.origin) - vr_et_v) < 16, sprintf("left in a body: it goes with it (%.1f from it, was %.1f)", vlen(vr_et_w.origin - m.origin), vr_et_v));
            VR_EE_NextCase();
        }
        return;
    }
    // ---- 4: armed enemies: a blade held, a blade left: not taken, not its seek, not counted; one lying about (the control) is
    if(c == 4)
    {
        if(vr_et_s < 10)
        {
            VR_EE_Stab(6, t);
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            vr_ee_m2 = VR_ART_SpawnKind(p, 40, 0, 70); // (empty handed: it looks for a weapon)
            vr_ee_m2.enemy = world;
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && t > 0.1)
        {
            entity seeker = vr_ee_m2;
            seeker.nextthink = time + 1;
            entity was = self;
            self = seeker;
            entity found = VR_AG_FindWeapon();
            self = was;
            VR_Test_Check(found != vr_et_w, sprintf("an armed enemy doesn't look for one held (found %s)", found.classname));
            VR_Test_Check(!VR_EnemyWeapons_Counted(vr_et_w), "the drop cap doesn't count one held");
            // Left in the body.
            VR_Embed_DropHands(vr_et_w, "test");
            localcmd("-grabmain\n");
            self = seeker;
            found = VR_AG_FindWeapon();
            self = was;
            VR_Test_Check(found != vr_et_w, sprintf("nor for one left (found %s)", found.classname));
            VR_Test_Check(!VR_EnemyWeapons_Counted(vr_et_w), "the drop cap doesn't count one left");
            // The control.
            vr_et_w2 = CreateThrownWeapon(p, WID_SWORD, 0, WeaponInst_New(0), seeker.origin + '12 0 0', '0 0 0', '0 0 0', '0 0 0', FALSE, FALSE, 0);
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && t > 0.6)
        {
            entity was2 = self;
            vr_ee_m2.nextthink = time + 1;
            self = vr_ee_m2;
            entity found2 = VR_AG_FindWeapon();
            self = was2;
            VR_Test_Check(found2 == vr_et_w2, sprintf("the control: one lying at its feet it does (found %s, %g)", found2.classname, vr_et_w2.weapon));
            VR_EE_NextCase();
        }
        return;
    }
    // ---- 5: a dead body with one left in it goes: it falls, a weapon lying there, a grip takes it. (Held: test 7's case 13.)
    if(c == 5)
    {
        if(vr_et_s < 10)
        {
            VR_EE_Stab(6, t);
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            localcmd("-grabmain\n");
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && !VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.1)
        {
            m.health = 1;
            T_Damage(m, p, p, 2);
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && t > 1)
        {
            VR_Test_Check(m.health <= 0 && VR_AxeStick_InMonster(vr_et_w), sprintf("it stays in the corpse (health %g, kind %g)", m.health, vr_et_w.vr_stick_kind));
            remove(m); // (what fading out and every other going comes to: SUB_Remove, the arena's)
            vr_et_m = world;
            VR_ET_To(13);
        }
        else if(vr_et_s == 13 && (vr_et_w.flags & FL_ONGROUND || t > 3))
        {
            VR_Test_Check(vr_et_w.classname == "thrown_weapon" && vr_et_w.vr_stick_kind == AXESTICK_OUT && vr_et_w.weapon == WID_SWORD, sprintf("the body gone: it falls, a weapon (kind %g, on the ground %g)", vr_et_w.vr_stick_kind, vr_et_w.flags & FL_ONGROUND));
            VR_Test_Check(!VR_AxeStick_Kept(vr_et_w), "lying there, as any weapon");
            vr_et_v2 = 0;
            vr_eh_end = vr_et_w.origin;
            VR_EH_HandTo(vr_eh_end, 0);
            VR_ET_To(14);
        }
        else if(vr_et_s == 14 && t > 0.1 && vlen(VRGetHandPos(h) - vr_et_w.origin) > 2 && vr_et_v2 < 10)
        {
            vr_eh_end = vr_eh_end + vr_et_w.origin - VRGetHandPos(h);
            VR_EH_HandTo(vr_eh_end, 0);
            vr_et_v2 = vr_et_v2 + 1;
            vr_et_t0 = time;
        }
        else if(vr_et_s == 14 && t > 0.1)
        {
            localcmd("+grabmain\n");
            VR_ET_To(15);
        }
        else if(vr_et_s == 15 && (VRGetEntWeapon(p, h) == WID_SWORD || t > 0.5))
        {
            VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD, sprintf("a grip on it takes it (holds %g, %.1f from it)", VRGetEntWeapon(p, h), vlen(VRGetHandPos(h) - vr_et_w.origin)));
            VR_EE_NextCase();
        }
        return;
    }
    // ---- 6: a real two-handed stab: both on it; the handle's hand opens, then the other. 7: the helping hand first.
    if(c == 6 || c == 7)
    {
        float keeper = c == 6 ? off : h; // (the hand that stays on longer)
        float first = c == 6 ? h : off;
        if(vr_et_s == 3 && vr_et_v2 != -1)
        {
            // The helping hand on the blade, as the blow begins, a little past the handle; both hands go together.
            vector lineN = normalize(VR_Blow(h).mh_far - VR_Blow(h).mh_grip);
            localcmd(sprintf("vr_mock_hand_glide 0\nvr_mock_hand_to off %v\n+graboff\n", vr_eh_start + lineN * 12));
            vr_et_v2 = -1;
            vr_et_t0 = time; // (time for the grip to close)
            return;
        }
        if(vr_et_s == 3 && t < 0.3)
        {
            return;
        }
        float wasS = vr_et_s;
        if(vr_et_s < 10)
        {
            VR_EE_Stab(6, t);
            if(wasS == 3 && vr_et_s == 4)
            {
                vector lineM = normalize(VR_Blow(h).mh_far - VR_Blow(h).mh_grip);
                bprint(sprintf("embededge: two hands: aiming %g, the other's grip %g\n", VRGet2HAiming(p), weaponanygrip(p, off)));
                localcmd(sprintf("vr_mock_hand_to off %v\n", vr_eh_end + lineM * 12));
            }
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0 && t > 0.2)
        {
            VR_Test_Check(vr_et_w.vr_hold_hands == 3 && vr_et_w.vr_hold_handle == 2, sprintf("a two-handed stab: both on it, the main by its handle (hands %g, handle %g)", vr_et_w.vr_hold_hands, vr_et_w.vr_hold_handle));
            VR_Test_Check(VR_Hand_Kind(p, off) == VR_HOLD_EMBED && VR_Hand_HeldOff(p, off), "the other hand is held off too");
            VR_Test_Check(ragdolltug(m, p, h, 1) >= 0 && ragdolltug(m, p, off, 1) < 0, sprintf("one spring, on the handle's hand (%g, %g)", ragdolltug(m, p, h, 1), ragdolltug(m, p, off, 1)));
            localcmd(first == h ? "-grabmain\n" : "-graboff\n");
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && !VR_HandGrabUtil_IsHandGrabbing(p, first) && t > 0.15)
        {
            VR_Test_Check(VR_Hand_Free(p, first, 0) && VR_Hand_Kind(p, keeper) == VR_HOLD_EMBED && VR_Embed_Held(vr_et_w), "one opens: the other holds on");
            VR_Test_Check(vr_et_w.vr_hold_handle == (keeper == h ? 2 : 1), sprintf("the handle's the hand left (%g)", vr_et_w.vr_hold_handle));
            VR_Test_Check(ragdolltug(m, p, keeper, 1) >= 0 && ragdolltug(m, p, first, 1) < 0, sprintf("the spring goes with it (%g, %g)", ragdolltug(m, p, keeper, 1), ragdolltug(m, p, first, 1)));
            localcmd(keeper == h ? "-grabmain\n" : "-graboff\n");
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && !VR_HandGrabUtil_IsHandGrabbing(p, keeper) && t > 0.15)
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && !VR_Embed_Held(vr_et_w) && VR_Hand_Free(p, keeper, 0), "both open: it stays");
            VR_Test_Check(ragdolltug(m, p, keeper, 1) < 0, "no spring");
            VR_EE_NextCase();
        }
        return;
    }
    // ---- 8: a stab down at a body lying on a real knockdown: held; the spring has it; pulled out, in the hand
    if(c == 8)
    {
        if(vr_et_s == 0 && !vr_et_v)
        {
            vr_et_m = VR_ET_Grunt(54);
            vr_et_v = 1;
            return;
        }
        if(vr_et_s == 0 && m.health > 0 && !m.vr_knockdown)
        {
            VR_Test_Check(VR_Knockdown_Start(m), "knocked down");
            vr_et_t0 = time;
            return;
        }
        if(vr_et_s == 0 && t > 0.7)
        {
            vr_et_v = 0;
            vr_eh_down = 1.2;
            vr_ee_target = m;
            VR_ET_To(1);
            localcmd("+grabmain\n");
            return;
        }
        if(vr_et_s > 0 && vr_et_s < 10)
        {
            VR_EE_Stab(6, t);
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            VR_Test_Check(ragdolltug(m, p, h, 1) >= 0, "a body knocked down: held by the limb it's in");
            VR_Test_Check(VR_EH_Blade() == vr_et_w, "held");
            vr_et_at = VRGetHandPos(h);
            VR_EH_HandTo(vr_et_at - vr_et_w.vr_stick_dir * 50, 1.6);
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && (w == world || t > 4))
        {
            VR_EE_InHand("pulled out of a knocked down body");
            VR_EE_NextCase();
        }
        return;
    }
    // ---- 9: an active ragdoll by a real blow (the stab's own stagger, made long), pulled out, stabbed again while it staggers
    if(c == 9)
    {
        if(vr_et_s == 0 && !vr_et_v)
        {
            localcmd("tune vr_embed_stagger_sword 4\n");
            vr_et_v = 1;
        }
        if(vr_et_s < 10)
        {
            VR_EE_Stab(6.5, t);
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0 && vr_et_v == 2)
        {
            VR_Test_Check(m.vr_knockdown && VR_EH_Blade() == vr_et_w, sprintf("stabbed again while it staggers: held (down %g)", m.vr_knockdown));
            VR_Test_Check(ragdolltug(m, p, h, 1) >= 0, "the spring on it");
            VR_EE_NextCase();
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            VR_Test_Check(m.vr_knockdown && m.vr_kd_stagger, sprintf("a real stab: staggering (%g, %g)", m.vr_knockdown, m.vr_kd_stagger));
            vr_et_at = VRGetHandPos(h);
            VR_EH_HandTo(vr_et_at - vr_et_w.vr_stick_dir * 44, 0.8);
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && (w == world || t > 2))
        {
            VR_EE_InHand("out of a staggering body");
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && t > 0.05)
        {
            vr_ee_target = m;
            vr_eh_down = 0;
            vr_et_s = 1;
            vr_et_t0 = time;
            vr_et_v = 2;
        }
        return;
    }
    VR_EE_Rest();
};

// (The rest of the cases: a QC function's locals have a limit, and these are many.)
void() VR_EE_Rest =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    float off = cVR_OffHand;
    entity w = VR_EH_Blade();
    entity m = vr_et_m;
    float c = vr_et_case;
    // ---- 10: the other hand's fist on the body the blade is in, standing (Holding Enemies): nothing clashes, nothing's lost
    // ---- 11: a staggering body, the other hand's fist on a limb (it may go down: a manhandling): the same
    if(c == 10 || c == 11)
    {
        if(vr_et_s == 0 && c == 11)
        {
            localcmd("tune vr_embed_stagger_sword 4\n");
        }
        if(vr_et_s < 10)
        {
            VR_EE_Stab(c == 11 ? 6.5 : 4.2, t);
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
        {
            VR_Test_Check(c == 10 ? !m.vr_knockdown : m.vr_knockdown, sprintf("standing or staggering as the case asks (%g)", m.vr_knockdown));
            localcmd(sprintf("vr_mock_hand_to off %v\n", VR_ET_Chest(m) + '0 0 10'));
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && t > 0.15)
        {
            localcmd("+graboff\n");
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && t > 0.4)
        {
            bprint(sprintf("embededge: case %g: the other hand's fist on it: its kind %g (foe %g, ragdoll %g), the blade %s, body down %g\n", c, VR_Hand_Kind(p, off), VR_HOLD_FOE, VR_HOLD_RAGDOLL,
                w == world ? "not held" : "held", m.vr_knockdown));
            VR_Test_Check(w == vr_et_w || VRGetEntWeapon(p, h) == WID_SWORD, sprintf("the blade held still, or in the hand (holds %g)", VRGetEntWeapon(p, h)));
            localcmd("-graboff\n");
            VR_ET_To(13);
        }
        else if(vr_et_s == 13 && !VR_HandGrabUtil_IsHandGrabbing(p, off) && t > 0.15)
        {
            VR_Test_Check(VR_Hand_Free(p, off, 0), sprintf("the fist let go (kind %g)", VR_Hand_Kind(p, off)));
            if(w != world)
            {
                vr_et_at = VRGetHandPos(h);
                VR_EH_HandTo(vr_et_at - w.vr_stick_dir * 60, 0.8);
            }
            VR_ET_To(14);
        }
        else if(vr_et_s == 14 && (w == world || t > 3))
        {
            VR_EE_InHand("after the fist let go, pulled");
            VR_EE_NextCase();
        }
        return;
    }
    // ---- 12: a stab under bullet time: it reads the hand's real speed; a real pull is no jump, and slides as at full speed
    if(c == 12)
    {
        if(vr_et_s == 0)
        {
            localcmd("vr_slowmo 0.3\n");
            vr_et_v = vr_hand_time_scale;
        }
        if(vr_et_s < 10)
        {
            VR_EE_Stab(6, t);
        }
        else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0 && t > 0.05)
        {
            bprint(sprintf("embededge: bullet time: hand scale %.2f, the blow read %.1f m/s, %.1f units in\n", vr_hand_time_scale, VR_Blow(h).mh_speed, vr_et_w.vr_stick_depth + vr_et_w.vr_stick_slide));
            VR_Test_Check(vr_hand_time_scale < 0.5, sprintf("it runs (%.2f)", vr_hand_time_scale));
            vr_ee_real = vr_et_w.vr_stick_slide;
            vr_et_at = VRGetHandPos(h);
            VR_EH_HandTo(vr_et_at - vr_et_w.vr_stick_dir * 8, 0.05);
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && t > 0.3)
        {
            bprint(sprintf("embededge: bullet time: a hard pull of 8 units: the blade slid %.1f to %.1f, err %.1f, held %g\n", vr_ee_real, w.vr_stick_slide, w.vr_hold_err, w != world));
            VR_Test_Check(w == world || w.vr_stick_slide < vr_ee_real - 0.5 || fabs(w.vr_hold_err) > 1, sprintf("the pull counts (%.1f, %.1f)", w.vr_stick_slide, w.vr_hold_err));
            localcmd("vr_slowmo 1\n");
            VR_EE_NextCase();
        }
        return;
    }
    VR_EE_Verdict();
};

// vr_embed_test 14, 16, 17: the save's scenes.
void() VR_EE_Scene =
{
    entity p = self;
    float t = time - vr_et_t0;
    if(vr_et_s < 10)
    {
        VR_EE_Stab(vr_ee_mode == 14 ? 4.2 : 6.5, t);
    }
    else if(vr_et_s == 10 && vr_et_w.vr_stick_goal == 0)
    {
        if(vr_ee_mode == 17)
        {
            localcmd("-grabmain\n");
        }
        VR_ET_To(11);
    }
    else if(vr_et_s == 11 && t > 0.2)
    {
        bprint(sprintf("embedsave: blade %s, hands %g, body down %g, spring %g\n", vr_et_w.netname, vr_et_w.vr_hold_hands, vr_et_m.vr_knockdown, ragdolltug(vr_et_m, p, cVR_MainHand, 1)));
        bprint("embedsave: ready\n");
        vr_et_on = 0;
    }
};

// vr_embed_test 15: after the load: the blade's where it was, the hand on it (14, 16), or a grip takes it (17); pulled: in the hand.
void() VR_EE_After =
{
    entity p = self;
    float h = cVR_MainHand;
    float t = time - vr_et_t0;
    entity w = VR_EH_Blade();
    if(vr_et_s == 0)
    {
        vr_et_w = world;
        for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
        {
            if(VR_AxeStick_IsStuck(e))
            {
                vr_et_w = e;
            }
        }
        VR_Test_Check(vr_et_w != world && VR_AxeStick_InMonster(vr_et_w), "the blade's still in the body");
        if(vr_et_w == world)
        {
            VR_EE_Verdict();
            return;
        }
        bprint(sprintf("embedafter: mode %g: hands %g, the hand's kind %g, body down %g, stagger %g, spring %g, holds %g\n", vr_ee_mode, vr_et_w.vr_hold_hands, VR_Hand_Kind(p, h), vr_et_m.vr_knockdown, vr_et_m.vr_kd_stagger, ragdolltug(vr_et_m, p, h, 1), VRGetEntWeapon(p, h)));
        if(vr_ee_mode == 17)
        {
            VR_Test_Check(!VR_Embed_Held(vr_et_w) && VR_Hand_Free(p, h, 0), "left: no hand on it");
            vr_eh_end = vr_et_w.origin;
            VR_EH_HandTo(vr_eh_end, 0);
            vr_et_v2 = 0;
            VR_ET_To(1);
        }
        else
        {
            VR_Test_Check(w == vr_et_w && VR_Hand_HeldOff(p, h), "the hand's on it still");
            VR_ET_To(4);
        }
    }
    else if(vr_et_s == 1 && t > 0.1 && vlen(VRGetHandPos(h) - vr_et_w.origin) > 2 && vr_et_v2 < 10)
    {
        vr_eh_end = vr_eh_end + vr_et_w.origin - VRGetHandPos(h);
        VR_EH_HandTo(vr_eh_end, 0);
        vr_et_v2 = vr_et_v2 + 1;
        vr_et_t0 = time;
    }
    else if(vr_et_s == 1 && t > 0.1)
    {
        localcmd("+grabmain\n");
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && (w != world || t > 0.5))
    {
        VR_Test_Check(w == vr_et_w, sprintf("a grip takes it again (kind %g, %.1f from it)", VR_Hand_Kind(p, h), vlen(VRGetHandPos(h) - vr_et_w.origin)));
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && t > 0.3)
    {
        if(vr_ee_mode == 16 && w != world)
        {
            VR_Test_Check(vr_et_m.vr_knockdown == 0 || ragdolltug(vr_et_m, p, h, 1) >= 0, sprintf("a spring on it again, if it's a ragdoll still (%g, %g)", vr_et_m.vr_knockdown, ragdolltug(vr_et_m, p, h, 1)));
        }
        if(w != world)
        {
            vr_et_at = VRGetHandPos(h);
            VR_EH_HandTo(vr_et_at - w.vr_stick_dir * 60, 1.6);
        }
        VR_ET_To(5);
    }
    else if(vr_et_s == 5 && (w == world || t > 5))
    {
        VR_EE_InHand("pulled after the load");
        VR_EE_Verdict();
    }
};

// vr_embed_test 19: the map's own heavy body: not light, never staggered, nor dragged; the penalty on; out at reach, in the hand.
void() VR_EE_Heavy =
{
    entity p = self;
    float h = cVR_MainHand;
    float t = time - vr_et_t0;
    entity w = VR_EH_Blade();
    entity m = vr_et_m;
    if(vr_et_s == 0 && vr_ee_target == world)
    {
        entity best = world;
        for(entity e = nextent(world); e; e = nextent(e))
        {
            if((e.flags & FL_MONSTER) && e.health > 0 && (e.classname == "monster_ogre" || e.classname == "monster_shambler" || e.classname == "monster_boss" || e.classname == "monster_shalrath" || e.classname == "monster_demon1"))
            {
                if(best == world || vlen(e.origin - p.origin) < vlen(best.origin - p.origin))
                {
                    best = e;
                }
            }
        }
        VR_Test_Check(best != world, "the map has one");
        if(best == world)
        {
            VR_EE_Verdict();
            return;
        }
        vr_ee_target = best;
        makevectors('0 1 0' * best.angles_y);
        vr_eh_home = best.origin + v_forward * 64;
        vr_eh_home_z = best.origin_z + best.mins_z - p.mins_z + 1;
        bprint(sprintf("embedheavy: %s (%g health), hold %.2f, light %g, from %v at %v\n", best.classname, best.health, VR_FoeGrab_Strength(best, 0, 0), VR_Embed_Light(best), vr_eh_home, best.origin));
    }
    if(m != world && m.health > 0)
    {
        m.nextthink = time + 0.5;
        m.velocity = '0 0 0';
    }
    if(vr_et_s < 10)
    {
        VR_EE_Stab(9, t);
        if(vr_et_s == 10)
        {
            VR_Test_Check(!VR_Embed_Light(vr_et_m), sprintf("%s: not light", vr_et_m.classname));
            VR_Test_Check(!vr_et_m.vr_knockdown, sprintf("%s: a very strong stab doesn't stagger it", vr_et_m.classname));
        }
    }
    else if(vr_et_s == 10 && (w == world || w.vr_stick_goal == 0))
    {
        vr_et_at = m.origin;
        vr_em_share = 1;
        VR_EM_Walk(vr_eh_fw * -1);
        VR_ET_To(11);
    }
    else if(vr_et_s == 11)
    {
        vr_em_share = min(vr_em_share, p.vr_heavy_move);
        if(w == world || t > 3)
        {
            VR_EM_Walk('0 0 0');
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) == WID_SWORD, sprintf("%s: walking off, it comes out into the hand (%.2f s, holds %g)", m.classname, t, VRGetEntWeapon(p, h)));
            VR_Test_Check(vlen(m.origin - vr_et_at) < 1, sprintf("%s: never dragged (%.1f units)", m.classname, vlen(m.origin - vr_et_at)));
            VR_Test_Check(!m.vr_knockdown && ragdolltug(m, p, h, 1) < 0, sprintf("%s: no ragdoll, no hold", m.classname));
            VR_Test_Check(vr_em_share < 0.4, sprintf("%s: you slowed while it held (%.2f)", m.classname, vr_em_share));
            VR_EE_Verdict();
        }
    }
};

// The frame of what vr_embed_test asked (VR_Embed_TestFrame): TRUE: one of these runs.
float(entity p) VR_EE_Frame =
{
    if(vr_et_on == 13)
    {
        if(time - vr_et_t0 > VR_ET_GIVEUP)
        {
            VR_Test_Check(FALSE, sprintf("timed out: case %g state %g", vr_et_case, vr_et_s));
            cvar_set("vr_embed", "1");
            VR_EE_NextCase();
        }
        VR_EE_Step();
        return TRUE;
    }
    if(vr_et_on == 14)
    {
        VR_EE_Scene();
        return TRUE;
    }
    if(vr_et_on == 15)
    {
        VR_EE_After();
        return TRUE;
    }
    if(vr_et_on == 19)
    {
        if(time - vr_et_t0 > VR_ET_GIVEUP)
        {
            VR_Test_Check(FALSE, sprintf("timed out: state %g", vr_et_s));
            VR_EE_Verdict();
            return TRUE;
        }
        VR_EE_Heavy();
        return TRUE;
    }
    return FALSE;
};

// What vr_embed_test asks of these.
void(float ask) VR_EE_Ask =
{
    entity p = VR_Player();
    vr_eh_home = p.origin;
    vr_ee_offhome = VRGetEntHandPos(p, cVR_OffHand);
    vr_ee_target = world;
    vr_eh_down = 0;
    vr_et_case = 0;
    VR_ET_To(0);
    if(ask == 13)
    {
        VR_Test_BeginFlags("embededge", VR_TEST_LOUD);
        cvar_set("vr_hit_stagger", "0"); // (a blow's own chances: what's looked at is the blade's)
        cvar_set("vr_kdtrig", "0");
        localcmd("vr_mock_hand_to_absolute 1\n");
        vr_et_on = 13;
    }
    else if(ask == 14 || ask == 16 || ask == 17)
    {
        cvar_set("vr_hit_stagger", "0");
        cvar_set("vr_kdtrig", "0");
        localcmd("vr_mock_hand_to_absolute 1\n");
        if(ask != 14)
        {
            localcmd("tune vr_embed_stagger_sword 4\n");
        }
        vr_ee_mode = ask;
        vr_et_on = 14;
    }
    else if(ask == 15)
    {
        VR_Test_BeginFlags("embedafter", VR_TEST_LOUD);
        vr_et_on = 15;
    }
    else if(ask == 18 || ask == 20)
    {
        VR_Test_BeginFlags("embedlevel", VR_TEST_LOUD);
        float wid = VRGetEntWeapon(p, cVR_MainHand);
        bprint(sprintf("embedlevel: main hand holds %g (%s), kind %g, sword %g; thrown weapons in this level: %g\n", wid, wid == WID_SWORD ? "the sword" : "not the sword", VR_Hand_Kind(p, cVR_MainHand), WID_SWORD, find(world, classname, "thrown_weapon") != world));
        if(ask == 18)
        {
            VR_Test_Check(wid == WID_SWORD && VR_Hand_Free(p, cVR_MainHand, VR_HANDFREE_LOCKED) && !VR_Hand_HeldOff(p, cVR_MainHand), "a blade held as the level ended: in the hand in the next");
        }
        VR_Test_Verdict();
    }
    else if(ask == 19)
    {
        VR_Test_BeginFlags("embedheavy", VR_TEST_LOUD);
        cvar_set("vr_hit_stagger", "0");
        cvar_set("vr_kdtrig", "0");
        localcmd("vr_mock_hand_to_absolute 1\n");
        vr_et_on = 19;
    }
};
#endif

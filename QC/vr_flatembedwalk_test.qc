#ifdef QVR_TESTS
// vr_flatembedwalk_test.qc -- vr_embed_test 64 (embedhold2, his flat notes 2026-11-25, items 4 and 11): a blade stuck by flat
// play's V thrust, then he walks away (+back) or the other hand is checked. With -flatscreenmode only (map vrtesthall, god,
// notarget). Per frame it reads the engine's .vr_embed_armgap (cm each hand is past its stretched arm: the hand off the wrist)
// and keeps the most, from the stick until half a second after the hold ends (the hand's ease back to the arm included).
//   case 0 "crate":     the sword in a small crate, walk away: the hand stays on the wrist; the blade comes out or lets go cleanly.
//   case 1 "ragdoll":   the sword in a grunt killed (a ragdoll with it in), walk away: with vr_flat_embed_arm 1, blade comes out into hand.
//   case 2 "grunt":     the sword in a live grunt, walk away: with vr_flat_embed_arm 1, blade comes out into hand.
//   case 3 "ogre":      the sword in an ogre held still (heavy, anchored), walk away: with vr_flat_embed_arm 1, hand lets go and blade stays in.
//   case 4 "spear":     the spear stuck in a grunt: with vr_flat_embed_arm 1, off hand holds it too, then walking away draws spear into hand;
//                       with vr_flat_embed_arm 0, no undroppable flat off hand is attached.
//   case 5 "sword+off": the sword stuck in a grunt with off hand free (stays off), walk away: with vr_flat_embed_arm 1, blade comes out into hand.
//   case 6 "axe":       the axe swung by its edge (AXESTICK_EDGE) into a grunt: walking away never draws the axe into the hand; with
//                       vr_flat_embed_arm 1, the hand lets go cleanly when arm reach runs out (stays in body, doesn't drag 600 u away).
// Whenever the blade comes out into the hand (vr_fw_end == 1), vr_embed_probe checks the drawn hand stays rigidly locked to the weapon
// throughout vr_embed_free_ease (vr_embed_probe_grip < 0.5 cm).
// Every case: after the stab he stands still half a second (state 6) before walking: the engine's .vr_embed_armpast (cm the drawn
// wrist is past the most its arm reaches, signed) stays under VR_FW_PASTTOL there, the hold is still on, and the blade came back out of
// the body by at most VR_FW_CREEPTOL units on its own (state 6, the body settled: the stab's own half second is checked the same way
// except for the ragdoll, which flops with the blade in it): nothing moves the drawn hand off the blade while nobody moves (his flat note
// 2026-11-25, item 4: "the blade comes out about 0.1 s after the stab"). The depth is watched from the stab on (the go-in only deepens).
// Ends `flatembedwalk: N passed, M failed`. Per frame with scratch1 2: "flatembedwalk: f <n> gap main <cm> off <cm> far <cm>". scratch2 N starts at case N.

float vr_fw_gapmain;  // the most cm the main hand was off its arm in this case
float vr_fw_gapoff;   // and the off hand
float vr_fw_frames;   // frames walked
float vr_fw_end;      // the hold ended: 1 out into the hand, 2 let go (it stays in), 3 still held
float vr_fw_bothmin;  // frames both hands held it (cases 4, 5)
float vr_fw_hold;     // frames looked at
float vr_fw_pastmain; // the most cm the main hand's unclamped blade target was past its arm while standing still (.vr_embed_armpast)
float vr_fw_pastoff;  // and the off hand's
float vr_fw_stillgapm; // the most cm the main hand's drawn wrist was off its arm while standing still (.vr_embed_armgap)
float vr_fw_stillgapo; // and the off hand's
float vr_fw_walkpast; // the most cm the main hand's unclamped blade target was past its arm while walking away (.vr_embed_armpast)
float vr_fw_stillin;  // standing still after the stab: the blade is still stuck (state 6)
float vr_fw_dmax;     // the deepest the blade went in during the stab (units: vr_stick_depth + vr_stick_slide)
float vr_fw_hp;       // the body's health before the V (case 6 prints what the axe's swing did)
float vr_fw_dmax6;    // and during the standing still (from there nothing may push it out)
float vr_fw_dcut;     // the most it came back out by itself while he stood still, state 6 (units)
float vr_fw_dcut3;    // the same in the stab's own half second (printed: a ragdoll flops with the blade in it)
vector vr_fw_from;    // where the walk began
string vr_fw_name;
entity vr_fw_w;       // the stuck blade

float vr_fw_saved_kd;
float vr_fw_saved_stag;
float vr_fw_saved_push;

float VR_FW_LAST = 6;
float VR_FW_TOL = 3; // cm a hand may be off its arm (a frame's walk at the let-go, the ease's first frame)
float VR_FW_CREEPTOL = 2; // units the stuck blade may come back out of the body by itself while he stands still
float VR_FW_EASEGRIPTOL = 0.5; // cm the drawn hand may diverge from the drawn weapon during vr_embed_free_ease

void() VR_FW_Finish =
{
    localcmd("-back\n");
    cvar_set("vr_knockdown", ftos(vr_fw_saved_kd));
    cvar_set("vr_hit_stagger", ftos(vr_fw_saved_stag));
    cvar_set("vr_melee_push", ftos(vr_fw_saved_push));
    cvar_set("vr_embed_probe", "");
    VR_Test_Verdict();
    vr_et_on = 0;
};

void() VR_FW_Begin =
{
    VR_Test_BeginFlags("flatembedwalk", VR_TEST_LOUD);
    vr_fw_saved_kd = cvar_hget(cvarh_vr_knockdown);
    vr_fw_saved_stag = cvar_hget(cvarh_vr_hit_stagger);
    vr_fw_saved_push = cvar_hget(cvarh_vr_melee_push);
    cvar_set("vr_knockdown", "0");
    cvar_set("vr_hit_stagger", "0");
    cvar_set("vr_melee_push", "0"); // (the blow's push would carry the body off the blade before the walk)
    vr_eh_home = VR_Player().origin;
    vr_et_on = 64;
    vr_et_case = max(0, cvar_hget(cvarh_scratch2)); // (scratch2: the first case)
    VR_ET_To(0);
};

void() VR_FW_Next =
{
    localcmd("-back\n");
    VRSetHandEmpty(self, cVR_OffHand);
    VR_EH_NextCase();
};

// What the case's blade is in and what holds it: 0 a crate, 1 a grunt killed, 2 a live grunt, 3 an ogre, 4 a grunt (the spear),
// 5 a grunt (the sword, the off hand free); 6 a grunt with the axe (its V is a swing, an edge embed).
void() VR_FW_Setup =
{
    entity p = self;
    float c = vr_et_case;
    float ahead = 40;
    float flags = 0;
    VRSetHandEmpty(p, cVR_OffHand);
    if(c == 4)
    {
        flags = VR_ADMelee_Flags(VR_SWORDKIND_SPEAR);
        ahead = 42;
    }
    if(c == 6)
    {
        ahead = 28; // (the axe's swing reaches shorter than a thrust: at 40 its V meets nothing, flat play's punch line included)
    }
    VR_EH_Give(c == 6 ? WID_AXE : WID_SWORD, flags);
    makevectors('0 1 0' * p.v_viewangle_y);
    if(c == 0)
    {
        vr_fw_name = "crate";
        vr_et_m = VR_EP_Crate(2, 44);
        return;
    }
    vr_fw_name = c == 1 ? "ragdoll" : (c == 2 ? "grunt" : (c == 3 ? "ogre" : (c == 4 ? "spear" : (c == 5 ? "sword+off" : "axe"))));
    if(c == 3)
    {
        self = spawn();
        self.classname = "monster_ogre";
        setorigin(self, p.origin + v_forward * 52);
        self.angles_y = p.v_viewangle_y + 180;
        monster_ogre();
        vr_et_m = self;
        self = p;
        vr_et_m.health = 1000;
        return;
    }
    vr_et_m = VR_ET_Grunt(ahead);
    setorigin(vr_et_m, p.origin + v_forward * ahead - '0 0 1');
};

// The hold of `w` ended how: 1 out into the hand, 2 let go (it stays in), 3 still held.
float(entity p, entity w) VR_FW_How =
{
    if(VR_EH_Blade() != world)
    {
        return 3;
    }
    if(!VRIsHandWeaponless(p, cVR_MainHand))
    {
        return 1;
    }
    return 2;
};

void() VR_FW_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    entity w = VR_EH_Blade();
    if(!VR_FlatPlay(p))
    {
        VR_Test_Check(FALSE, "flat-screen play only (-flatscreenmode)");
        VR_FW_Finish();
        return;
    }
    if(vr_et_case > VR_FW_LAST)
    {
        VR_FW_Finish();
        return;
    }
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %s state %g", vr_fw_name, vr_et_s));
        VR_FW_Next();
        return;
    }
    // (The blade's own depth: it only deepens as the stab goes in (vr_stick_goal); anything it came back out by after that, while he
    // stands still, is a drawn hand moving the blade (item 4). Watched through state 3 as well: his "0.1 s after the stab".)
    if(w != world && (vr_et_s == 2 || (vr_et_s >= 3 && vr_et_s <= 6)))
    {
        float d = w.vr_stick_depth + w.vr_stick_slide;
        if(vr_et_s <= 3)
        {
            vr_fw_dcut3 = max(vr_fw_dcut3, vr_fw_dmax - d);
            vr_fw_dmax = max(vr_fw_dmax, d);
        }
        else
        {
            // (State 6: from the deepest it stood when he stopped, no further out. What the body's stagger and settling took out
            // during the stab's own half second is the other measure's.)
            vr_fw_dcut = max(vr_fw_dcut, vr_fw_dmax6 - d);
            vr_fw_dmax6 = max(vr_fw_dmax6, d);
        }
        if(cvar_hget(cvarh_scratch1) == 3 && vr_et_s == 6)
        {
            VR_Test_Print(sprintf("f %g in %.1f past main %.1f off %.1f gap main %.1f held %g\n", time, d, p.vr_embed_armpast_y, p.vr_embed_armpast_x, p.vr_embed_armgap_y, w.vr_hold_hands));
        }
    }
    // (Held still while the blade goes in: the ogre the whole case, a heavy body anchored where it stands.)
    if(vr_et_m != world && (vr_et_m.flags & FL_MONSTER) && vr_et_m.health > 0 && vr_et_m.takedamage && (vr_et_s < 3 || vr_et_s == 6 || vr_et_case == 3 || vr_et_case >= 4))
    {
        vr_et_m.nextthink = time + 0.5;
        vr_et_m.velocity = '0 0 0';
    }
    if(vr_et_s >= 3 && vr_et_s <= 6)
    {
        vr_fw_gapmain = max(vr_fw_gapmain, p.vr_embed_armgap_y);
        vr_fw_gapoff = max(vr_fw_gapoff, (vr_et_case >= 4) ? p.vr_embed_armgap_x : 0);
        if(cvar_hget(cvarh_scratch1) == 2)
        {
            dprint(sprintf("flatembedwalk: f %g gap main %.1f off %.1f past %.1f far %.1f held %g in %.1f\n", vr_fw_frames, p.vr_embed_armgap_y,
                p.vr_embed_armgap_x, p.vr_embed_armpast_y,
                vr_fw_w != world ? vr_fw_w.vr_hold_far : -1, w != world, vr_fw_w != world ? vr_fw_w.vr_stick_depth + vr_fw_w.vr_stick_slide : -1));
        }
    }
    if(vr_et_s == 0)
    {
        setorigin(p, vr_eh_home - '0 40 0');
        p.velocity = '0 0 0';
        VR_FW_Setup();
        vr_fw_gapmain = 0;
        vr_fw_gapoff = 0;
        vr_fw_frames = 0;
        vr_fw_end = 0;
        vr_fw_bothmin = 0;
        vr_fw_hold = 0;
        vr_fw_pastmain = -1000;
        vr_fw_pastoff = -1000;
        vr_fw_stillgapm = 0;
        vr_fw_stillgapo = 0;
        vr_fw_walkpast = -1000;
        vr_fw_stillin = 1;
        vr_fw_dmax = 0;
        vr_fw_dmax6 = 0;
        vr_fw_dcut = 0;
        vr_fw_dcut3 = 0;
        vr_fw_w = world;
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && t > 0.4 && VRGetEntWeapon(p, cVR_MainHand) != WID_FIST && (vr_et_case == 0 || vr_et_m.takedamage))
    {
        dprint(sprintf("flatembedwalk: case %s\n", vr_fw_name));
        vr_fw_hp = vr_et_m.health;
        localcmd("vr_flat_punch v\n");
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && (w != world || t > 0.9))
    {
        VR_Test_Check(w != world, sprintf("%s: V's thrust sticks the blade (%s)", vr_fw_name, w != world ? sprintf("%.1f units in", w.vr_stick_depth) : "not stuck"));
        if(w == world)
        {
            VR_FW_Next();
            return;
        }
        vr_fw_w = w;
        // (What the arm's reach leaves it: a point sticks to its goal, an edge to the depth the engine's own; the arm rule stops a
        // stab deeper than its hand's arm reaches (VR_Embed_FlatArm). Printed, both ways: what the rule costs the thrust.)
        VR_Test_Print(sprintf("%s: the blade is %s in (%.1f units of the %.1f it was going to)\n", vr_test_tag, w.vr_stick_embed == AXESTICK_POINT ? "point first" : "by its edge",
            w.vr_stick_depth + w.vr_stick_slide, w.vr_stick_goal));
        if(vr_et_case == 6)
        {
            VR_Test_Print(sprintf("%s: its swing did %g damage\n", vr_test_tag, vr_fw_hp - vr_et_m.health));
            VR_Test_Check(w.vr_stick_embed == AXESTICK_EDGE, sprintf("%s: its V went in by its edge, not its point (how %g)", vr_fw_name, w.vr_stick_embed));
        }
        if(vr_et_case == 1)
        {
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2); // (a ragdoll with the sword in)
        }
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && vr_et_case >= 4)
    {
        // (The other hand: on it the whole second, as the hand on the handle.)
        vr_fw_hold = vr_fw_hold + 1;
        if(w != world && VR_Hand_Kind(p, cVR_OffHand) == VR_HOLD_EMBED && VR_Hand_BusyEnt(p, cVR_OffHand) == w)
        {
            vr_fw_bothmin = vr_fw_bothmin + 1;
        }
        if(t > 1)
        {
            float armOn3 = (cvar_hget(cvarh_vr_flat_embed_arm) != 0);
            // (flatembed3 item 3: every two-handed blade whose grip or shaft is in the free off arm's reach takes the off hand too --
            // the spear's shaft, the sword stabbed one-handed, the axe swung -- only when vr_flat_embed_arm is on, so the off hand
            // can drop cleanly when its arm runs out.)
            if(armOn3)
            {
                VR_Test_Check(vr_fw_bothmin >= vr_fw_hold - 1, sprintf("%s: the other hand holds the stuck blade too (%g of %g frames)", vr_fw_name, vr_fw_bothmin, vr_fw_hold));
            }
            else
            {
                VR_Test_Check(vr_fw_bothmin == 0, sprintf("%s: with vr_flat_embed_arm 0 no undroppable off hand attaches (%g of %g frames)", vr_fw_name, vr_fw_bothmin, vr_fw_hold));
            }
            VR_Test_Check(vr_fw_gapoff <= VR_FW_TOL, sprintf("%s: the other hand on its arm (at most %.1f cm off, %g allowed)", vr_fw_name, vr_fw_gapoff, VR_FW_TOL));
            VR_ET_To(6);
        }
    }
    else if(vr_et_s == 3 && t > 0.5) // (the stretch eased in, the ragdoll fallen)
    {
        VR_ET_To(6);
    }
    else if(vr_et_s == 6) // (standing still: nothing moves the drawn hand off its arm, nothing pulls the blade out)
    {
        vr_fw_pastmain = max(vr_fw_pastmain, p.vr_embed_armpast_y);
        vr_fw_stillgapm = max(vr_fw_stillgapm, p.vr_embed_armgap_y);
        if(vr_et_case >= 4)
        {
            vr_fw_pastoff = max(vr_fw_pastoff, p.vr_embed_armpast_x);
            vr_fw_stillgapo = max(vr_fw_stillgapo, p.vr_embed_armgap_x);
        }
        if(w == world)
        {
            vr_fw_stillin = 0;
        }
        if(t > 0.5)
        {
            VR_Test_Print(strcat(
                sprintf("%s: stood still after the stab: armgap main %.1f off %.1f, armpast main %.1f off %.1f cm; ",
                    vr_test_tag, vr_fw_stillgapm, vr_fw_stillgapo, vr_fw_pastmain, vr_fw_pastoff),
                sprintf("the blade %.1f units in, back out by %.1f on its own; %s\n",
                    vr_fw_dmax6, vr_fw_dcut, vr_fw_stillin ? "still stuck" : "the hold ended")));
            VR_Test_Check(vr_fw_dcut <= VR_FW_CREEPTOL, sprintf("%s: standing still, the blade stays in (came back out by %.1f units on its own, %g allowed)", vr_fw_name, vr_fw_dcut, VR_FW_CREEPTOL));
            // (The stab's own half second: the body settling under it. A ragdoll falls with the blade in it, so only that case is let run.)
            VR_Test_Check(vr_et_case == 1 || vr_fw_dcut3 <= VR_FW_CREEPTOL, sprintf("%s: the stab's own half second, the blade stays in (came back out by %.1f units, %g allowed)", vr_fw_name, vr_fw_dcut3, VR_FW_CREEPTOL));
            VR_Test_Check(vr_fw_stillgapm <= VR_FW_TOL && vr_fw_stillgapo <= VR_FW_TOL,
                sprintf("%s: standing still, the drawn hand stays on its arm (main %.1f, off %.1f cm off, %g allowed)", vr_fw_name, vr_fw_stillgapm, vr_fw_stillgapo, VR_FW_TOL));
            VR_Test_Check(vr_fw_stillin, sprintf("%s: standing still, the blade stays in the body (nothing moves the drawn hand: %s)", vr_fw_name, vr_fw_stillin ? "" : "the hold ended"));
            vr_fw_from = p.origin;
            cvar_set("vr_embed_probe", "reset");
            localcmd("+back\n");
            VR_ET_To(4);
        }
    }
    else if(vr_et_s == 4)
    {
        vr_fw_frames = vr_fw_frames + 1;
        if(w != world)
        {
            vr_fw_walkpast = max(vr_fw_walkpast, p.vr_embed_armpast_y);
        }
        if(w == world || t > 3)
        {
            vr_fw_end = VR_FW_How(p, vr_fw_w);
            localcmd("-back\n");
            VR_ET_To(5);
        }
    }
    else if(vr_et_s == 5 && t > 0.5) // (the hand's ease back to its arm over)
    {
        float armOn = (cvar_hget(cvarh_vr_flat_embed_arm) != 0);
        string how = vr_fw_end == 1 ? "came out into the hand" : (vr_fw_end == 2 ? "let go, it stays in" : "still held");
        float walked = vlen(p.origin - vr_fw_from);
        float stillStuck = (vr_fw_w != world && VR_AxeStick_IsStuck(vr_fw_w));
        VR_Test_Print(sprintf("%s: walked %.0f units in %g frames: %s; the main hand at most %.1f cm off its arm, the off hand %.1f\n",
            vr_test_tag, walked, vr_fw_frames, how, vr_fw_gapmain, vr_fw_gapoff));
        VR_Test_Check(vr_fw_gapmain <= VR_FW_TOL && vr_fw_gapoff <= VR_FW_TOL,
            sprintf("%s: walking away, the hand stays on the wrist (main %.1f, off %.1f cm off its arm at most, %g allowed)", vr_fw_name, vr_fw_gapmain, vr_fw_gapoff, VR_FW_TOL));
        if(vr_fw_end == 1)
        {
            float easeGrip = cvar_hget(cvarh_vr_embed_probe_grip);
            float easedN = cvar_hget(cvarh_vr_embed_probe_eased);
            VR_Test_Check(easedN > 0 && easeGrip >= 0 && easeGrip <= VR_FW_EASEGRIPTOL,
                sprintf("%s: release ease keeps weapon and hand locked together (%.2f cm off over %g frames, %g allowed)", vr_fw_name, easeGrip, easedN, VR_FW_EASEGRIPTOL));
        }
        if(vr_et_case == 6)
        {
            // An axe's V is a swing (edge embed): walking away never draws it into the hand; with vr_flat_embed_arm 1 the hold ends
            // cleanly when arm reach runs out (stays in body, doesn't drag the grunt 600 u away).
            VR_Test_Check(vr_fw_end != 1, sprintf("%s: walking away never draws the swung axe into the hand (%s, %.0f units walked)", vr_fw_name, how, walked));
            if(armOn)
            {
                VR_Test_Check(vr_fw_end == 2 && stillStuck && walked < 150,
                    sprintf("%s: with vr_flat_embed_arm 1 the arm lets go cleanly and axe stays in (%s, stuck %g, %.0f units walked)", vr_fw_name, how, stillStuck, walked));
                VR_Test_Check(vr_fw_walkpast > cvar_hget(cvarh_vr_embed_arm_letgo),
                    sprintf("%s: .vr_embed_armpast reports true unclamped past-reach while clamped (%.1f cm)", vr_fw_name, vr_fw_walkpast));
            }
        }
        else if(armOn)
        {
            if(vr_et_case == 3)
            {
                VR_Test_Check(vr_fw_end == 2 && stillStuck,
                    sprintf("%s: heavy body lets go and blade stays in (%s, stuck %g, %.0f units walked)", vr_fw_name, how, stillStuck, walked));
                VR_Test_Check(vr_fw_walkpast > 0,
                    sprintf("%s: .vr_embed_armpast reports positive unclamped past-reach while clamped (%.1f cm)", vr_fw_name, vr_fw_walkpast));
            }
            else if(vr_et_case == 1 || vr_et_case == 2 || vr_et_case == 4 || vr_et_case == 5)
            {
                VR_Test_Check(vr_fw_end == 1,
                    sprintf("%s: light body draws blade out into hand (%s, %.0f units walked)", vr_fw_name, how, walked));
            }
            else
            {
                VR_Test_Check(vr_fw_end == 1 || vr_fw_end == 2,
                    sprintf("%s: the hold ends as he walks (%s, %.0f units walked)", vr_fw_name, how, walked));
            }
        }
        else
        {
            // vr_flat_embed_arm 0: verify the rule is genuinely off (hands still kept on wrists above; non-edge holds may slide or drag).
            VR_Test_Check(vr_fw_end == 1 || vr_fw_end == 2 || vr_fw_end == 3,
                sprintf("%s: vr_flat_embed_arm 0 ran (%s, %.0f units walked)", vr_fw_name, how, walked));
        }
        VR_FW_Next();
    }
};
#endif

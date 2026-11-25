#ifdef QVR_TESTS
// vr_flatembedwalk_test.qc -- vr_embed_test 64 (embedhold2, his flat notes 2026-11-25, items 4 and 11): a blade stuck by flat
// play's V thrust, then he walks away (+back) or the other hand is checked. With -flatscreenmode only (map vrtesthall, god,
// notarget). Per frame it reads the engine's .vr_embed_armgap (cm each hand is past its stretched arm: the hand off the wrist)
// and keeps the most, from the stick until half a second after the hold ends (the hand's ease back to the arm included).
//   case 0 "crate":   the sword in a small crate, walk away: the hand stays on the wrist; the crate is dragged, the blade
//                     comes out into the hand, or the hand lets go; never a hand off its arm.
//   case 1 "ragdoll": the sword in a grunt, the grunt killed (a ragdoll with it in), walk away: the same.
//   case 2 "grunt":   the sword in a live grunt (left to stand), walk away: the same.
//   case 3 "ogre":    the sword in an ogre held still (heavy, anchored), walk away: the same.
//   case 4 "spear":   the spear (two-handed, the off hand on the pole) stuck: the off hand holds it too, for a second.
//   case 5 "sword+off": the sword stuck, the off hand empty: it takes the sword's handle too, for a second; then walk away.
// Ends `flatembedwalk: N passed, M failed`. Per frame with scratch1 2: "flatembedwalk: f <n> gap main <cm> off <cm> far <cm>". scratch2 N starts at case N.

float vr_fw_gapmain;  // the most cm the main hand was off its arm in this case
float vr_fw_gapoff;   // and the off hand
float vr_fw_frames;   // frames walked
float vr_fw_end;      // the hold ended: 1 out into the hand, 2 let go (it stays in), 3 still held
float vr_fw_bothmin;  // frames both hands held it (cases 4, 5)
float vr_fw_hold;     // frames looked at
vector vr_fw_from;    // where the walk began
string vr_fw_name;
entity vr_fw_w;       // the stuck blade

float VR_FW_LAST = 5;
float VR_FW_TOL = 3; // cm a hand may be off its arm (a frame's walk at the let-go, the ease's first frame)

void() VR_FW_Begin =
{
    VR_Test_BeginFlags("flatembedwalk", VR_TEST_LOUD);
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
// 5 a grunt (the sword, the off hand free).
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
    VR_EH_Give(WID_SWORD, flags);
    makevectors('0 1 0' * p.v_viewangle_y);
    if(c == 0)
    {
        vr_fw_name = "crate";
        vr_et_m = VR_EP_Crate(2, 44);
        return;
    }
    vr_fw_name = c == 1 ? "ragdoll" : (c == 2 ? "grunt" : (c == 3 ? "ogre" : (c == 4 ? "spear" : "sword+off")));
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
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    if(vr_et_case > VR_FW_LAST)
    {
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %s state %g", vr_fw_name, vr_et_s));
        VR_FW_Next();
        return;
    }
    // (Held still while the blade goes in: the ogre the whole case, a heavy body anchored where it stands.)
    if(vr_et_m != world && (vr_et_m.flags & FL_MONSTER) && vr_et_m.health > 0 && vr_et_m.takedamage && (vr_et_s < 3 || vr_et_case == 3 || vr_et_case >= 4))
    {
        vr_et_m.nextthink = time + 0.5;
        vr_et_m.velocity = '0 0 0';
    }
    if(vr_et_s >= 3 && vr_et_s <= 5)
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
        vr_fw_w = world;
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && t > 0.4 && VRGetEntWeapon(p, cVR_MainHand) != WID_FIST && (vr_et_case == 0 || vr_et_m.takedamage))
    {
        dprint(sprintf("flatembedwalk: case %s\n", vr_fw_name));
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
            VR_Test_Check(vr_fw_bothmin >= vr_fw_hold - 1, sprintf("%s: the other hand holds the stuck blade too (%g of %g frames)", vr_fw_name, vr_fw_bothmin, vr_fw_hold));
            VR_Test_Check(vr_fw_gapoff <= VR_FW_TOL, sprintf("%s: the other hand on its arm (at most %.1f cm off, %g allowed)", vr_fw_name, vr_fw_gapoff, VR_FW_TOL));
            vr_fw_from = p.origin;
            localcmd("+back\n");
            VR_ET_To(4);
        }
    }
    else if(vr_et_s == 3 && t > 0.5) // (the stretch eased in, the ragdoll fallen)
    {
        vr_fw_from = p.origin;
        localcmd("+back\n");
        VR_ET_To(4);
    }
    else if(vr_et_s == 4)
    {
        vr_fw_frames = vr_fw_frames + 1;
        if(w == world || t > 3)
        {
            vr_fw_end = VR_FW_How(p, vr_fw_w);
            localcmd("-back\n");
            VR_ET_To(5);
        }
    }
    else if(vr_et_s == 5 && t > 0.5) // (the hand's ease back to its arm over)
    {
        string how = vr_fw_end == 1 ? "came out into the hand" : (vr_fw_end == 2 ? "let go, it stays in" : "still held");
        float walked = vlen(p.origin - vr_fw_from);
        VR_Test_Print(sprintf("%s: walked %.0f units in %g frames: %s; the main hand at most %.1f cm off its arm, the off hand %.1f\n",
            vr_test_tag, walked, vr_fw_frames, how, vr_fw_gapmain, vr_fw_gapoff));
        VR_Test_Check(vr_fw_gapmain <= VR_FW_TOL && vr_fw_gapoff <= VR_FW_TOL,
            sprintf("%s: walking away, the hand stays on the wrist (main %.1f, off %.1f cm off its arm at most, %g allowed)", vr_fw_name, vr_fw_gapmain, vr_fw_gapoff, VR_FW_TOL));
        VR_Test_Check(vr_fw_end == 1 || vr_fw_end == 2 || (vr_fw_end == 3 && walked < 40),
            sprintf("%s: the hold ends as he walks (%s, %.0f units walked)", vr_fw_name, how, walked));
        VR_FW_Next();
    }
};
#endif

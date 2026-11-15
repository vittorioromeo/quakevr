#ifdef QVR_TESTS
// vr_flipgrip_test.qc -- the flip grip (ROUND25.md, "flipgrip"; his notes 2026-11-14): a sword's or dagger's grip, upright or
// reversed (blade down), kept or chosen by the blade's way through every way out of and into a hand. `vr_embed_test 44` (map
// vrtesthall, god, notarget), by the embedded blades' and the reverse grip's test machinery (vr_embed_test.qc, vr_revgrip_test.qc),
// the mock hands:
//   0 the dagger upright in the main hand: its grip hotspot; the off hand closed on it holds it two-handed (the control);
//   1 reversed: the hotspot still there, turned with the blade; the off hand closed on it holds it two-handed, the blade still down
//     (his note 1);
//   2 upright, handed off to the off hand (the main hand let go while the off hand held its hotspot): the carried blade keeps its
//     way; the main hand, turned for the other grip, closes on its handle: taken back reversed, the blade where it was (note 2);
//   3 reversed, let go of: the dagger lying about keeps the way it was held (its blade's line; notes 3 and 5: a drop and a throw
//     leave the hand by the one DropWeaponInHandScaled);
//   4 a dagger lying on a table (held still in the air: MOVETYPE_NONE, as one lying about) taken reversed (the hand at its reversed
//     hand point, weapondrawnpose 48, turned for that grip): held by its handle, reversed, not carried as a prop (note 4); 5 the
//     same upright;
//   6 the knight's sword stabbed down upright into a grunt and let go of; the hand turned for the other grip closes on its handle:
//     taken reversed, and the hand drawn on the handle within a small angle of the real hand (the engine's vr_embed_probe_hctl;
//     note 6: it was drawn twisted).
// Ends `flipgrip: N passed, M failed`.

float vr_fg_spot;   // the dagger's grip hotspot (its index), -1 none
vector vr_fg_hs0;   // case 0: that hotspot from the hand, upright
vector vr_fg_line;  // a case's blade line noted (from the handle to the tip)
entity vr_fg_d;     // a case's dagger lying about
vector vr_fg_hp;    // case 2: the carried dagger's handle
vector vr_fg_bl;    // ... and its blade's line
vector vr_fg_aim;   // the main hand's aim being nudged (VR_FG_MainTakes)
float vr_fg_nudges;
float vr_fg_retried; // case 6: the take tried again once

float() VR_FG_Grip = { return VRGetEntWeaponFlags(self, cVR_MainHand); };

// The main hand's dagger's two-handed hotspot (a grip's or its blade's: -1 none).
float() VR_FG_Spot =
{
    for(float i = 0; i < 4; i++)
    {
        if(weaponhotspotinfo(self, cVR_MainHand, i, 0) >= 1)
        {
            return i;
        }
    }
    return -1;
};

// A dagger lying still at `xAt`, its line along `xLine` (as on a table: MOVETYPE_NONE, a weapon lying about to the grabs).
entity(vector xAt, vector xLine) VR_FG_Lying =
{
    entity d = VR_RG_DaggerAt(xAt, xLine, '0 0 0');
    if(d != world)
    {
        d.vr_rigid = 0;
        d.vr_spin = '0 0 0';
        d.movetype = MOVETYPE_NONE;
        d.velocity = '0 0 0';
    }
    return d;
};

// The off hand on the main hand's grip hotspot, closed: states from `s`; TRUE once it holds it two-handed (or after a second).
float(float s, float t) VR_FG_OffOnSpot =
{
    if(vr_et_s == s)
    {
        localcmd(sprintf("vr_mock_hand_to off %v\n", weaponhotspot(self, cVR_MainHand, vr_fg_spot)));
        VR_ET_To(s + 1);
    }
    else if(vr_et_s == s + 1 && t > 0.2)
    {
        localcmd(sprintf("vr_mock_hand_to off %v\n+graboff\n", weaponhotspot(self, cVR_MainHand, vr_fg_spot)));
        VR_ET_To(s + 2);
    }
    else if(vr_et_s == s + 2 && (VRGet2HAiming(self) || t > 1))
    {
        return TRUE;
    }
    return FALSE;
};

// The main hand at `xAt`, turned so that its upright blade points along `xBlade`, closed: states from `s`; TRUE once it holds a
// sword by its handle or a stuck blade (or after a second). Turned first away from it (`xAway`) with a weapon of `xFlags` in it
// (a sword's or the dagger's), its aim nudged until that weapon's line lies along `xBlade` (its aim is not its blade's), then
// emptied and opened, moved there and closed: the hand turned for that grip.
float(float s, float t, vector xAway, vector xAt, vector xBlade, float xFlags) VR_FG_MainTakes =
{
    if(vr_et_s == s)
    {
        localcmd("-grabmain\n");
        VR_ET_To(s + 1);
    }
    else if(vr_et_s == s + 1 && !VR_HandGrabUtil_IsHandGrabbing(self, cVR_MainHand) && t > 0.15)
    {
        localcmd(sprintf("vr_mock_hand_to main %v\n", xAway)); // (away first: closed where it is, it would take what it is on)
        VR_ET_To(s + 7);
    }
    else if(vr_et_s == s + 7 && t > 0.2)
    {
        localcmd("+grabmain\n");
        VR_ET_To(s + 2);
    }
    else if(vr_et_s == s + 2 && VR_HandGrabUtil_IsHandGrabbing(self, cVR_MainHand) && t > 0.15)
    {
        VR_EH_Give(WID_SWORD, xFlags);
        vr_fg_aim = xBlade;
        vr_fg_nudges = 0;
        localcmd(sprintf("vr_mock_hand_aim main %v\n", VRGetHandPos(cVR_MainHand) + vr_fg_aim * 8000));
        VR_ET_To(s + 3);
    }
    else if(vr_et_s == s + 3 && t > 0.15)
    {
        vector have = VR_RG_Line();
        if(have * xBlade < 0.995 && vr_fg_nudges < 24)
        {
            // (Nudged, then turned by what is missing: as VR_EH_Blow lays a blade.)
            vector axis = VR_Cross(have, xBlade);
            float sn = vlen(axis);
            if(vr_fg_nudges < 8 || sn < 0.0001)
            {
                vr_fg_aim = normalize(vr_fg_aim + xBlade - have);
            }
            else
            {
                vr_fg_aim = normalize(VR_AxeStick_Turn(vr_fg_aim, axis * (1 / sn), atan2(sn, have * xBlade) * (180 / 3.14159265)));
            }
            localcmd(sprintf("vr_mock_hand_aim main %v\n", VRGetHandPos(cVR_MainHand) + vr_fg_aim * 8000));
            vr_fg_nudges = vr_fg_nudges + 1;
            vr_et_t0 = time;
            return FALSE;
        }
        VR_Test_Print(sprintf("flipgrip: the hand turned for the grip (its blade's line cos %.3f, %g nudges)\n", have * xBlade, vr_fg_nudges));
        VRSetHandEmpty(self, cVR_MainHand);
        W_SetCurrentAmmoFor(cVR_MainHand);
        localcmd("-grabmain\n");
        VR_ET_To(s + 4);
    }
    else if(vr_et_s == s + 4 && !VR_HandGrabUtil_IsHandGrabbing(self, cVR_MainHand) && t > 0.15)
    {
        localcmd(sprintf("vr_mock_hand_to main %v\n", xAt));
        VR_ET_To(s + 5);
    }
    else if(vr_et_s == s + 5 && t > 0.3)
    {
        localcmd(sprintf("vr_mock_hand_to main %v\n+grabmain\n", xAt));
        VR_ET_To(s + 6);
    }
    else if(vr_et_s == s + 6 && ((VRGetEntWeapon(self, cVR_MainHand) == WID_SWORD &&
            !(VR_FG_Grip() & QVR_WPNFLAG_FOREGRIP_CARRIED)) || VR_EH_Blade() != world || t > 1))
    {
        return TRUE;
    }
    return FALSE;
};

void() VR_FG_Begin =
{
    VR_Test_BeginFlags("flipgrip", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0");
    cvar_set("vr_hit_stagger", "0");
    cvar_set("vr_embed_probe", "");
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    vr_fg_spot = -1;
    vr_et_on = 44;
    vr_et_case = 0;
    VR_ET_To(0);
};

void() VR_FG_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %g state %g", vr_et_case, vr_et_s));
        localcmd("-graboff\n");
        VR_EH_NextCase();
        return;
    }
    vector eye = VRGetHeadPos(p);
    makevectors(p.v_viewangle);
    vector before = eye + v_forward * 20 + v_right * 4 - v_up * 10; // (the main hand's place, before you, a little low)
    // ---- 0, 1: upright, then reversed: the hotspot and a two-handed hold
    if(vr_et_case <= 1)
    {
        if(vr_et_s == 0)
        {
            localcmd(sprintf("-graboff\nvr_mock_hand_to main %v\nvr_mock_hand_aim main %v\n+grabmain\n", before, before + v_forward * 400));
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.2)
        {
            VR_EH_Give(WID_SWORD, VR_RG_Dagger() | (vr_et_case == 1 ? QVR_WPNFLAG_REVGRIP : 0));
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && t > 0.4)
        {
            float spot = VR_FG_Spot();
            vector hs = weaponhotspot(p, h, spot) - VRGetHandPos(h);
            if(vr_et_case == 0)
            {
                vr_fg_spot = spot;
                vr_fg_hs0 = hs;
                vr_fg_line = VR_RG_Line();
                VR_Test_Check(spot >= 0, sprintf("upright: the dagger's grip hotspot (%g) at %v from the hand", spot, hs));
            }
            else
            {
                VR_Test_Check(spot == vr_fg_spot && spot >= 0, sprintf("reversed: the grip hotspot still there (%g; was %g)", spot, vr_fg_spot));
                VR_Test_Check(vlen(hs - vr_fg_hs0) > 2, sprintf("... turned with the blade: at %v from the hand (%v upright)", hs, vr_fg_hs0));
            }
            if(spot < 0)
            {
                VR_EH_NextCase();
                return;
            }
            VR_ET_To(10);
        }
        else if(vr_et_s >= 10 && vr_et_s < 20 && VR_FG_OffOnSpot(10, t))
        {
            float along = VR_RG_Line() * vr_fg_line;
            string how = vr_et_case == 0 ? "upright" : "reversed";
            VR_Test_Check(VRGet2HAiming(p), sprintf("%s: the off hand closed on its hotspot holds it two-handed", how));
            if(vr_et_case == 1)
            {
                VR_Test_Check(along < -0.7 && VR_RG_Flip() > 0.99, sprintf("... still held blade down (line cos %.2f against upright, flip %.2f)",
                    along, VR_RG_Flip()));
            }
            localcmd("-graboff\n");
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 2: handed off upright, taken back reversed
    if(vr_et_case == 2)
    {
        if(vr_et_s == 0)
        {
            localcmd(sprintf("-graboff\nvr_mock_hand_to main %v\nvr_mock_hand_aim main %v\n+grabmain\n", before, before + v_forward * 400));
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.2)
        {
            VR_EH_Give(WID_SWORD, VR_RG_Dagger());
            VR_ET_To(10);
        }
        else if(vr_et_s >= 10 && vr_et_s < 20 && VR_FG_OffOnSpot(10, t))
        {
            vr_fg_line = VR_RG_Line();
            localcmd("-grabmain\n");
            VR_ET_To(20);
        }
        else if(vr_et_s == 20 && ((VRGetEntWeaponFlags(p, cVR_OffHand) & QVR_WPNFLAG_FOREGRIP_CARRIED) || t > 1) && t > 0.3)
        {
            float carried = VRGetEntWeapon(p, cVR_OffHand) == WID_SWORD && (VRGetEntWeaponFlags(p, cVR_OffHand) & QVR_WPNFLAG_FOREGRIP_CARRIED);
            vector hp = weapondrawnpose(p, cVR_OffHand, 0);
            vector tp = weapondrawnpose(p, cVR_OffHand, 3);
            vr_fg_hp = hp; // (its handle and line noted: the take below)
            vr_fg_bl = normalize(tp - hp);
            float keeps = vr_fg_bl * vr_fg_line;
            VR_Test_Check(carried, "handed off: the off hand carries the dagger by its hotspot");
            VR_Test_Check(keeps > 0.9, sprintf("... its blade the way it was held (cos %.2f)", keeps));
            if(!carried)
            {
                localcmd("-graboff\n");
                VR_EH_NextCase();
                return;
            }
            VR_ET_To(30);
        }
        else if(vr_et_s >= 30 && VR_FG_MainTakes(30, t, before + v_right * 30, vr_fg_hp, vr_fg_bl * -1, VR_RG_Dagger()))
        {
            float rev = (VR_FG_Grip() & QVR_WPNFLAG_REVGRIP) != 0;
            VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD && !(VR_FG_Grip() & QVR_WPNFLAG_FOREGRIP_CARRIED),
                "the main hand turned for the other grip takes the carried dagger back by its handle");
            VR_Test_Check(rev, "... reversed (its blade to the little finger's side)");
            VR_ET_To(40);
        }
        else if(vr_et_s == 40 && t > 0.4)
        {
            float along2 = VR_RG_Line() * vr_fg_bl;
            VR_Test_Check(along2 > 0.8, sprintf("... drawn with its blade where it was (cos %.2f)", along2));
            localcmd("-graboff\n");
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 3: reversed, let go of: the prop keeps the way it was held
    if(vr_et_case == 3)
    {
        if(vr_et_s == 0)
        {
            localcmd(sprintf("-graboff\nvr_mock_hand_to main %v\nvr_mock_hand_aim main %v\n+grabmain\n", before, before + v_forward * 400));
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.2)
        {
            VR_EH_Give(WID_SWORD, VR_RG_Dagger() | QVR_WPNFLAG_REVGRIP);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && t > 0.4)
        {
            vr_fg_line = VR_RG_Line();
            DropWeaponInHand(p, h, WID_SWORD, VR_FG_Grip());
            self = p;
            entity d = world;
            for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
            {
                if(e.vr_wpn_from == p && e.vr_drop_born == time)
                {
                    d = e;
                }
            }
            VR_Test_Check(d != world, "reversed, let go of: lying about");
            if(d != world)
            {
                float keeps = normalize(weaponmuzzle(d) - d.origin) * vr_fg_line;
                VR_Test_Check(keeps > 0.95, sprintf("... its blade the way it was held (cos %.3f)", keeps));
            }
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 4, 5: taken off a table, reversed and upright
    if(vr_et_case == 4 || vr_et_case == 5)
    {
        float revd = vr_et_case == 4;
        if(vr_et_s == 0)
        {
            localcmd("-grabmain\n");
            VRSetHandEmpty(p, h);
            W_SetCurrentAmmoFor(h);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && !VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.15)
        {
            vector line = normalize(v_forward + v_right * 0.5); // (lying level, across you)
            line_z = 0;
            vr_fg_d = VR_FG_Lying(eye + v_forward * 26 + v_right * 6 - v_up * 6, normalize(line)); // (at arm's length: off the holsters' places)
            vr_fg_line = vr_fg_d != world ? normalize(weaponmuzzle(vr_fg_d) - vr_fg_d.origin) : '1 0 0';
            VR_ET_To(10);
        }
        else if(vr_et_s >= 10 && vr_fg_d != world)
        {
            vector at = revd ? weapondrawnpose(vr_fg_d, h, 48) : vr_fg_d.origin;
            if(vr_et_s == 10)
            {
                VR_Test_Print(sprintf("flipgrip: the dagger lying at %v, its reversed hand point %.1f units off it\n", vr_fg_d.origin,
                    vlen(weapondrawnpose(vr_fg_d, h, 48) - vr_fg_d.origin)));
            }
            if(VR_FG_MainTakes(10, t, before + v_right * 30, at, revd ? vr_fg_line * -1 : vr_fg_line, VR_RG_Dagger()))
            {
                string how = revd ? "reversed" : "upright";
                float f = VR_FG_Grip();
                VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD && !(f & QVR_WPNFLAG_FOREGRIP_CARRIED),
                    sprintf("off a table, the hand %s at its handle takes it by its handle, not as a prop (holds %g, carried %g)", how,
                    VRGetEntWeapon(p, h), (f & QVR_WPNFLAG_FOREGRIP_CARRIED) != 0));
                VR_Test_Check(((f & QVR_WPNFLAG_REVGRIP) != 0) == revd, sprintf("... held %s", how));
                VR_EH_NextCase();
            }
        }
        else if(vr_et_s >= 10)
        {
            VR_Test_Check(FALSE, "a dagger to lie on the table");
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 6: stuck upright, taken reversed: the hand drawn on the handle as the real hand
    if(vr_et_case == 6)
    {
        if(vr_et_m != world && vr_et_m.health > 0)
        {
            vr_et_m.nextthink = time + 0.5; // (the body stands where it is, all through: as test 42's)
            vr_et_m.velocity = '0 0 0';
        }
        if(vr_et_s < 10)
        {
            vr_eh_flags = 0;
            vr_eh_down = 1.2;
            if(VR_EH_Blow(WID_SWORD, 9, 0, FALSE, t)) // (9 m/s: deep enough to stay in once let go of)
            {
                vr_eh_down = 0;
                entity w = VR_EH_Blade();
                VR_Test_Check(w != world && !(w.weaponflags & QVR_WPNFLAG_REVGRIP), "an upright sword's downward stab is stuck in the grunt");
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                vr_fg_d = w;
                vr_fg_line = normalize(weaponmuzzle(w) - w.origin);
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10)
        {
            localcmd("-grabmain\n");
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && VR_EH_Blade() == world && t > 0.3)
        {
            cvar_set("vr_embed_probe", "reset");
            VR_ET_To(20);
        }
        else if(vr_et_s >= 20 && vr_et_s < 40 && VR_FG_MainTakes(20, t, VRGetHandPos(h) - v_right * 30, weapondrawnpose(vr_fg_d, h, 48), vr_fg_line * -1, 0))
        {
            if(VR_EH_Blade() != vr_fg_d && !vr_fg_retried && VR_AxeStick_IsStuck(vr_fg_d))
            {
                // (Once more if the closing missed: the grunt's body settles under the blade, a low-impact run's late frames.)
                vr_fg_retried = 1;
                VR_Test_Print("flipgrip: the hand missed the stuck handle: once more\n");
                VR_ET_To(20);
                return;
            }
            VR_Test_Check(VR_EH_Blade() == vr_fg_d, "the hand turned for the other grip closes on the stuck sword's handle: held in the body");
            VR_Test_Check((vr_fg_d.weaponflags & QVR_WPNFLAG_REVGRIP) != 0, "... reversed");
            VR_ET_To(40);
        }
        else if(vr_et_s == 40 && t > 0.6)
        {
            // (The hand drawn on the handle either way round its line, the nearer the real hand: its roll about the handle is not the
            // blade's as it went in. Before, the hand closed the other way round was drawn twisted on it, up to half a turn off.)
            float hctl = cvar_hget(cvarh_vr_embed_probe_hctl);
            VR_Test_Check(hctl >= 0 && hctl < 95, sprintf("... drawn on the handle %.1f degrees from the real hand (under 95: never twisted past a quarter turn)", hctl));
            cvar_set("vr_embed_probe", "");
            VR_EH_NextCase();
        }
        return;
    }
    localcmd("-grabmain\n-graboff\n");
    VR_Test_Verdict();
    vr_et_on = 0;
};
#endif

#ifdef QVR_TESTS
// vr_flatblood_test.qc -- blood at point blank (his note 2026-11-10: "Even setting both vr_particle_near_fade and
// vr_particle_near_cull to 0 still results in some blood particles not spawning if for example I stand really close to a
// ragdoll and shoot it"; ROUND24.md, "Flat-screen: blood at point blank"). Flat-screen play (-flatscreenmode; the
// headset: the mock hand placed as far off and the gun fired from it, printed, not judged); god, notarget. `vr_flatblood_test 1`: for each target (a live grunt, its health kept up; a corpse ragdoll,
// settled) and each gun (the shotgun, the super shotgun, the nailgun), one shot at its middle from 60, 30, 12 and 4 units
// (the eye's level distance to its middle less 16: a grunt's face), a fresh target each: through the real path (+attack,
// the view turned onto it). Each shot's blood: the impacts' blood the game sent (vr_blood_sent), the wounds painted, and
// the client's blood particles made and made where the view draws them (vr_blood_tally, vr_blood_tally_drawn, every
// emitter's). A shot at point blank makes blood as the 60-unit one does: at least a third of its particles (pellets and
// the wounds' spurts vary), all of them where the view draws them as at 60. Ends `flatblood: N passed, M failed`.
// `vr_flatblood_test 4` (test aid): the live grunt's shotgun shots alone (60, 30, 12, 4 units), a picture of each 4 frames
// after it (the blood near the eye: textured drops, not flat squares; vr_particle_retro_min_blocks).

float vr_fbt_on;      // the running test
float vr_fbt_s;       // the state
float vr_fbt_case;    // which shot: target * 12 + gun * 4 + distance
float vr_fbt_frames;  // frames in the state
entity vr_fbt_m;      // the target
vector vr_fbt_home;   // where he stands at the start (the targets spawn ahead of it)
vector vr_fbt_fw;     // ahead, level
float vr_fbt_sent0, vr_fbt_tally0, vr_fbt_drawn0, vr_fbt_clip0;
float vr_fbt_ref_tally, vr_fbt_ref_drawn; // the 60-unit shot's, this target and gun
float vr_fbt_still;   // frames the corpse has kept still
vector vr_fbt_last;
vector vr_fbt_aim;    // the point he shoots at

float VR_FBT_GIVEUP = 1080; // frames (15 s) a state may last: FAIL "timed out"
float VR_FBT_CASES = 28;
float VR_FBT_CROUCH = 24; // cases 24-27: crouched over a corpse, the shotgun (his repro, 2026-11-10)
float vr_fbt_quick;    // vr_flatblood_test 2: case 12, then the crouched ones (3: with pictures); 4: cases 0-3, pictures
float vr_fbt_ref_sg_corpse, vr_fbt_ref_sg_sent; // case 12's particles: the corpse's shotgun shot from 60, the crouched ones' measure

float(float xCase) VR_FBT_Dist =
{
    float d = xCase - floor(xCase / 4) * 4;
    if(xCase >= VR_FBT_CROUCH)
    {
        // Crouched over it: his eye this far off the point level (0 is 3: straight down is past the look's limit).
        if(d == 0) { return 20; }
        if(d == 1) { return 12; }
        if(d == 2) { return 6; }
        return 3;
    }
    if(d == 0) { return 60; }
    if(d == 1) { return 30; }
    if(d == 2) { return 12; }
    return 4;
};

float(float xCase) VR_FBT_Gun =
{
    float g = floor(xCase / 4) - floor(xCase / 12) * 3;
    if(g == 0) { return WID_SHOTGUN; }
    if(g == 1) { return WID_SUPER_SHOTGUN; }
    return WID_NAILGUN;
};

string(float xCase) VR_FBT_Name =
{
    float g = VR_FBT_Gun(xCase);
    return sprintf("%s, %s, %g units", xCase < 12 ? "a live grunt" : (xCase >= VR_FBT_CROUCH ? "crouched over a corpse ragdoll" : "a corpse ragdoll"),
        g == WID_SHOTGUN ? "shotgun" : (g == WID_SUPER_SHOTGUN ? "super shotgun" : "nailgun"), VR_FBT_Dist(xCase));
};

void(float xS) VR_FBT_To =
{
    vr_fbt_s = xS;
    vr_fbt_frames = 0;
};

void() VR_FBT_Finish =
{
    localcmd(VR_FlatPlay(VR_Player()) ? "-attack\n-flatcrouch\n" : "vr_mock_button main trigger 0\nvr_mock_hand_aim main off\nvr_mock_hand_to_absolute 0\n");
    if(vr_fbt_m != world)
    {
        remove(vr_fbt_m);
        vr_fbt_m = world;
    }
    vr_fbt_on = 0;
    VR_Test_Verdict();
};

vector() VR_FBT_Mid =
{
    return (vr_fbt_m.absmin + vr_fbt_m.absmax) * 0.5;
};

// He stands with his eye level `xDist` + 16 off the point he shoots at (a grunt's chest; a corpse's root bone's surface:
// its box's middle may lie between its limbs), back along `vr_fbt_fw`, the view on it.
void(entity p, float xDist) VR_FBT_Stand =
{
    vector mid = VR_FBT_Mid();
    vector onBone = limbplace(vr_fbt_m, 0, 3);
    if(vr_fbt_case >= 12 && onBone != '0 0 0')
    {
        mid = onBone;
    }
    vector at = mid - vr_fbt_fw * (vr_fbt_case >= VR_FBT_CROUCH ? xDist : xDist + 16);
    at_z = vr_fbt_home_z;
    setorigin(p, at);
    p.velocity = '0 0 0';
    vector eye = VRGetHeadPos(p);
    if(vr_fbt_case < 12)
    {
        mid_z = vr_fbt_m.absmin_z + vr_fbt_m.size_z * 0.75; // (a grunt standing: its chest, a little under his eye)
    }
    vr_fbt_aim = mid;
    vector a = vectoangles(mid - eye);
    a_x = -a_x;
    p.angles = a;
    p.v_angle = a;
    p.fixangle = TRUE;
};

void(entity p) VR_FBT_Begin =
{
    VR_Test_BeginFlags("flatblood", VR_TEST_LOUD);
    if(!VR_FlatPlay(p))
    {
        localcmd("vr_mock_hand_to_absolute 1\n");
    }
    vr_fbt_on = 1;
    vr_fbt_case = 0;
    vr_fbt_quick = FALSE;
    vr_fbt_home = p.origin;
    makevectors('0 1 0' * p.v_angle_y);
    vr_fbt_fw = v_forward;
    VR_Test_Print(sprintf("flatblood: flat play %g, near fade %g cull %g, mist near %g\n", VR_FlatPlay(p),
        cvar_hget(cvarh_vr_particle_near_fade), cvar_hget(cvarh_vr_particle_near_cull), cvar_hget(cvarh_vr_gore_mist_near)));
    VR_FBT_To(0);
};

void() VR_FlatBloodTest_Frame =
{
    entity p = VR_Player();
    if(!vr_fbt_on)
    {
        float ask = cvar_hget(cvarh_vr_flatblood_test);
        if(!ask || !p)
        {
            return;
        }
        cvar_set("vr_flatblood_test", "0");
        if(ask >= 1 && ask <= 4)
        {
            VR_FBT_Begin(p);
            if(ask == 4)
            {
                vr_fbt_quick = 3; // (4: the grunt's shotgun shots, a picture of each)
            }
            else if(ask >= 2)
            {
                vr_fbt_case = 12; // (2: the corpse's shotgun shot from 60, then the crouched ones; 3: and a picture of
                vr_fbt_quick = ask - 1; // each crouched shot's blood, 4 frames after it)
            }
        }
        return;
    }
    vr_fbt_frames = vr_fbt_frames + 1;
    if(vr_fbt_frames > VR_FBT_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out in state %g (%s)", vr_fbt_s, VR_FBT_Name(vr_fbt_case)));
        VR_FBT_Finish();
        return;
    }
    entity oldself = self;
    float corpse = vr_fbt_case >= 12;
    if(vr_fbt_s == 0) // a fresh target ahead
    {
        if(vr_fbt_case >= VR_FBT_CASES)
        {
            VR_FBT_Finish();
            return;
        }
        if(vr_fbt_m != world)
        {
            remove(vr_fbt_m);
        }
        setorigin(p, vr_fbt_home);
        vr_fbt_m = VR_Test_Spawn(p, 0, vr_fbt_home + vr_fbt_fw * 96, vectoyaw(vr_fbt_fw * -1));
        if(vr_fbt_m == world)
        {
            VR_Test_Check(FALSE, "a grunt spawned");
            VR_FBT_Finish();
            return;
        }
        self = p;
        VR_WeaponUtil_EntAddWeapon(p, VR_FBT_Gun(vr_fbt_case)); // (owned, and in the hand: as vr_nail_test gives it)
        VRGiveEntWeapon(p, cVR_MainHand, VR_FBT_Gun(vr_fbt_case), 0, 50);
        p.ammo_shells = 100;
        p.ammo_nails = 200;
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = oldself;
        VR_FBT_To(1);
        return;
    }
    if(vr_fbt_s == 1) // its spawn done (a few frames); a corpse killed whole and let settle
    {
        if(vr_fbt_frames < 10)
        {
            return;
        }
        if(!corpse)
        {
            vr_fbt_m.health = 100000;
            VR_FBT_To(3);
            return;
        }
        T_Damage(vr_fbt_m, world, p, vr_fbt_m.health + 1);
        vr_fbt_still = 0;
        vr_fbt_last = VR_FBT_Mid();
        VR_FBT_To(2);
        return;
    }
    if(vr_fbt_s == 2) // the corpse settling: its middle still for 36 frames
    {
        vector mid = VR_FBT_Mid();
        vr_fbt_still = vlen(mid - vr_fbt_last) < 0.6 ? vr_fbt_still + 1 : 0;
        vr_fbt_last = mid;
        if(isragdoll(vr_fbt_m) && vr_fbt_still >= 36 && vr_fbt_frames >= 36)
        {
            VR_FBT_To(3);
        }
        return;
    }
    if(vr_fbt_s == 3) // he stands there, the view on it; the hands settle onto the view (36 frames: the flat hand's lag)
    {
        float crouch = vr_fbt_case >= VR_FBT_CROUCH;
        if(crouch && vr_fbt_frames == 1)
        {
            localcmd("+flatcrouch\n"); // (Left Ctrl's hold, the console's way)
        }
        VR_FBT_Stand(p, VR_FBT_Dist(vr_fbt_case));
        if(!corpse)
        {
            if(vr_fbt_frames == 1)
            {
                vr_fbt_last = vr_fbt_m.origin;
            }
            setorigin(vr_fbt_m, vr_fbt_last); // (kept where it stood: it turns to him, it doesn't walk)
            vr_fbt_m.velocity = '0 0 0';
        }
        if(!VR_FlatPlay(p))
        {
            // The headset: his hand `xDist` + 16 back from the point (a grunt's face `xDist` off the hand), aimed at it:
            // at 4 and 12 units the muzzle is in the body.
            vector way = normalize(vr_fbt_aim - VRGetHeadPos(p));
            localcmd(sprintf("vr_mock_hand_to main %v\nvr_mock_hand_aim main %v\n",
                vr_fbt_aim - way * (VR_FBT_Dist(vr_fbt_case) + 16), vr_fbt_aim + way * 8000));
        }
        if(vr_fbt_frames < (crouch ? 72 : 36)) // (crouched: the crouch's own travel too)
        {
            return;
        }
        p.fixangle = FALSE;
        vr_fbt_sent0 = vr_blood_sent;
        vr_fbt_tally0 = cvar_hget(cvarh_vr_blood_tally);
        vr_fbt_drawn0 = cvar_hget(cvarh_vr_blood_tally_drawn);
        vr_fbt_clip0 = VRGetEntWeaponInst(p, cVR_MainHand).wi_clip + p.ammo_shells * 1000 + p.ammo_nails * 1000000;
        if(VR_FlatPlay(p))
        {
            localcmd("+attack\n"); // (flat: the real input, the view's aim)
            VR_FBT_To(4);
            return;
        }
        // The headset: the gun's own shot from the mock hand (its muzzle, its aim), as vr_nail_test fires it.
        self = p;
        float g = VR_FBT_Gun(vr_fbt_case);
        if(g == WID_SHOTGUN) { W_FireShotgun(cVR_MainHand); }
        else if(g == WID_SUPER_SHOTGUN) { W_FireSuperShotgun(cVR_MainHand); }
        else { W_FireSpikes(cVR_MainHand, 0); }
        self = oldself;
        VR_FBT_To(5);
        return;
    }
    if(vr_fbt_s == 4) // the shot: a tap (one frame of +attack: one shot, the counts its own), then a nail's flight and
    {                 // the client's frames (state 5's 18)
        if(vr_fbt_frames >= 2)
        {
            localcmd("-attack\n");
            VR_FBT_To(5);
        }
        return;
    }
    if(vr_fbt_s == 5)
    {
        if(((vr_fbt_quick == 2 && vr_fbt_case >= VR_FBT_CROUCH) || vr_fbt_quick == 3) && vr_fbt_frames == 4)
        {
            localcmd("screenshot\n");
        }
        if(vr_fbt_frames < 18)
        {
            return;
        }
        float sent = vr_blood_sent - vr_fbt_sent0;
        float made = cvar_hget(cvarh_vr_blood_tally) - vr_fbt_tally0;
        float drawn = cvar_hget(cvarh_vr_blood_tally_drawn) - vr_fbt_drawn0;
        vector eye = VRGetHeadPos(p);
        VR_Test_Print(sprintf("flatblood: %s: eye %v, its box %v .. %v,", VR_FBT_Name(vr_fbt_case), eye, vr_fbt_m.absmin,
            vr_fbt_m.absmax));
        VR_Test_Print(sprintf(" blood sent %g, particles %g, drawn %g, its health %g\n", sent, made, drawn, vr_fbt_m.health));
        float d = VR_FBT_Dist(vr_fbt_case);
        if(vr_fbt_case == 12)
        {
            vr_fbt_ref_sg_corpse = made;
            vr_fbt_ref_sg_sent = sent;
        }
        if(vr_fbt_case >= VR_FBT_CROUCH)
        {
            localcmd("-flatcrouch\n");
            VR_Test_Print(sprintf("flatblood: crouched: eye %.1f over the point, pitch %.0f\n", eye_z - vr_fbt_aim_z, p.v_angle_x));
            if(VR_FlatPlay(p))
            {
                // (Every pellet's blood as from 60: 108-120 particles a pellet there, more when it cuts or gibs, so
                // case 12's whole count is no steady measure.)
                VR_Test_Check(sent >= vr_fbt_ref_sg_sent - 1 && made >= sent * 60 && drawn >= made * 0.9,
                    sprintf("%s: it bleeds as from 60 (%g sent, %g particles, %g drawn; from 60: %g sent, %g particles)",
                    VR_FBT_Name(vr_fbt_case), sent, made, drawn, vr_fbt_ref_sg_sent, vr_fbt_ref_sg_corpse));
            }
        }
        else if(!VR_FlatPlay(p))
        {
            // (The headset: printed, not judged. The mock hand's aim is a few units off at 60 (a corpse's limb missed now
            // and then), and the nailgun fired this way makes no nail; the counts for comparing by eye.)
        }
        else if(d == 60)
        {
            vr_fbt_ref_tally = made;
            vr_fbt_ref_drawn = drawn;
            VR_Test_Check(sent > 0 && made > 0, sprintf("%s: it bleeds (%g sent, %g particles)", VR_FBT_Name(vr_fbt_case),
                sent, made));
        }
        else
        {
            VR_Test_Check(sent > 0 && made >= vr_fbt_ref_tally / 3 && drawn >= made * 0.9,
                sprintf("%s: it bleeds as at 60 (%g sent, %g particles, %g drawn; at 60: %g, %g)", VR_FBT_Name(vr_fbt_case),
                sent, made, drawn, vr_fbt_ref_tally, vr_fbt_ref_drawn));
        }
        vr_fbt_case = vr_fbt_case + 1;
        if(vr_fbt_quick == 3 && vr_fbt_case == 4)
        {
            vr_fbt_case = VR_FBT_CASES; // (4: the grunt's shotgun shots alone)
        }
        if(vr_fbt_quick && vr_fbt_quick != 3 && vr_fbt_case == 13)
        {
            vr_fbt_case = VR_FBT_CROUCH;
        }
        VR_FBT_To(0);
        return;
    }
};
#endif

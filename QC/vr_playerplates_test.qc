#ifdef QVR_TESTS
// vr_playerplates_test.qc -- the player's armour plates (vr_playerplates.qc; ROUND24.md, "The player's armour plates").
// `map vrtesthall; vr_pp_test 1` (no god: real hits): the pieces against the armour for each tier, a hit from a side
// shedding the piece nearest it, the helmet's fall, the cuirass last; a shard, new armour and an upgrade through the morph
// (over time, not at once); the damage taken the same with the plates on and off (and as the old rule has it); the helmet
// never drawn from the eyes (VR and -flatscreenmode). Ends `pptest: N passed, M failed`.
// `vr_pp_test 2` (then 20, 21 by itself): a save keeps the very pieces, a load shows them without a morph (`ppsave: ...`).
// `vr_pp_test 3` (then 30 by itself, after `changelevel e1m1`): so does a level change (`pplevel: ...`).
// `vr_pp_test 5, 6, 7`: full, half, torso only, red, to look at (Debug > Tests). `vr_pp_test 8`: the same each from the front and
// behind, a screenshot each (`ppshot: done`). `vr_pp_test 9`: full red from the eyes, looking down at the hands (`ppshot: done`).

float vr_ppt_on;       // the running test's number
float vr_ppt_s;        // its state
float vr_ppt_wait;     // frames to wait before the state goes on (what a word to the client takes)
float vr_ppt_t0;       // when the state began (a stuck test: FAIL "timed out")
float vr_ppt_begun;    // morphs begun at the mark
float vr_ppt_frames;   // frames a morph has been seen running
float vr_ppt_start;    // when it was first seen
float vr_ppt_prog0;    // its progress first seen
float vr_ppt_prog;     // ... last seen
float vr_ppt_case;     // which pickup
float vr_ppt_mask;     // the pieces noted
float vr_ppt_shot;     // the picture being taken

float VR_PPT_GIVEUP = 10; // seconds a state may last

void(float s, float frames) VR_PPT_To =
{
    vr_ppt_s = s;
    vr_ppt_wait = frames;
    vr_ppt_t0 = time;
};

float(entity p, float what) VR_PPT_Client =
{
    return ragdollbone(p, [what, 0, 0], QVR_ARMOR_CLIENT);
};

// Armour of `tier` (0 none) with `value` left, as armor_touch and T_Damage leave it.
void(entity p, float tier, float value) VR_PPT_Set =
{
    p.armortype = tier == 3 ? 0.8 : tier == 2 ? 0.6 : tier == 1 ? 0.3 : 0;
    p.armorvalue = value;
    VR_ItemUtil_EntDelItem(p, IID_ARMOR1);
    VR_ItemUtil_EntDelItem(p, IID_ARMOR2);
    VR_ItemUtil_EntDelItem(p, IID_ARMOR3);
    if(tier > 0)
    {
        VR_ItemUtil_EntAddItem(p, tier == 3 ? IID_ARMOR3 : tier == 2 ? IID_ARMOR2 : IID_ARMOR1);
    }
};

void() VR_PPT_CleanDebris =
{
    for(entity e = find(world, classname, "vr_ar_debris"); e; )
    {
        entity next = find(e, classname, "vr_ar_debris");
        remove(e);
        e = next;
    }
};

// A hit of `dmg` from `at` (a thing standing there): T_Damage's own, the body left where it is and well.
void(entity p, vector at, float dmg) VR_PPT_Hit =
{
    entity inf = spawn();
    inf.classname = "vr_ppt_inflictor";
    setorigin(inf, at);
    p.health = 10000;
    T_Damage(p, inf, world, dmg);
    p.velocity = '0 0 0';
    remove(inf);
};

// The pieces against the armour, each tier: all at its most, half at half, the cuirass alone at 1, none at 0.
void(entity p) VR_PPT_Counts =
{
    for(float tier = 1; tier <= 3; tier++)
    {
        float most = 50 + 50 * tier;
        VR_PPT_Set(p, tier, most);
        VR_PP_Frame(p);
        VR_Test_Check(p.vr_pp_mask == 1023 && p.vr_pp_type == tier, sprintf("tier %g at %g: every piece (mask %g, colour %g)", tier, most, p.vr_pp_mask, p.vr_pp_type));
        VR_PPT_Set(p, tier, most * 0.5);
        VR_PP_Frame(p);
        VR_Test_Check(VR_PP_CountOf(p.vr_pp_mask) == 5, sprintf("tier %g at half: 5 pieces (%g)", tier, VR_PP_CountOf(p.vr_pp_mask)));
        VR_Test_Check((p.vr_pp_mask & 1) != 0, sprintf("tier %g at half: the cuirass on", tier));
        // All the way down: one fewer at a time, never the cuirass.
        float ok = TRUE;
        float last = 11;
        for(float v = most; v >= 1; v--)
        {
            VR_PPT_Set(p, tier, v);
            VR_PP_Frame(p);
            float n = VR_PP_CountOf(p.vr_pp_mask);
            if(n != VR_PP_Count(tier, v) || !(p.vr_pp_mask & 1) || n > last)
            {
                ok = FALSE;
            }
            last = n;
        }
        VR_Test_Check(ok, sprintf("tier %g: the count follows the armour down, the cuirass stays", tier));
        VR_PPT_Set(p, tier, 1);
        VR_PP_Frame(p);
        VR_Test_Check(p.vr_pp_mask == 1, sprintf("tier %g at 1: the torso only (mask %g)", tier, p.vr_pp_mask));
        VR_PPT_Set(p, 0, 0);
        VR_PP_Frame(p);
        VR_Test_Check(p.vr_pp_mask == 0 && p.vr_pp_type == 0, sprintf("tier %g at 0: none (mask %g)", tier, p.vr_pp_mask));
    }
    float same = TRUE;
    for(float pct = 0.05; pct < 1.01; pct = pct + 0.05)
    {
        float c1 = VR_PP_Count(1, 100 * pct);
        float c2 = VR_PP_Count(2, 150 * pct);
        float c3 = VR_PP_Count(3, 200 * pct);
        if(c1 != c2 || c2 != c3)
        {
            same = FALSE;
        }
    }
    VR_Test_Check(same, "a damaged red, yellow and green lose pieces at the same pace");
    VR_PPT_CleanDebris();
};

// A hit from a side sheds that side's piece; from above, the helmet, with no debris; the cuirass goes last.
void(entity p) VR_PPT_Hits =
{
    makevectors([0, p.angles_y, 0]);
    vector left = p.origin + v_up * 8 - v_right * 400;
    vector right = p.origin + v_up * 8 + v_right * 400;
    float cracks = vr_pp_cracks;
    float pulses = vr_pp_pulses;
    float made = vr_ar_debris_made;

    VR_PPT_Set(p, 1, 100);
    VR_PP_Frame(p);
    VR_PPT_Hit(p, left, 30);
    VR_PP_Frame(p);
    float i = vr_pp_last_pop;
    VR_Test_Check(VR_PP_CountOf(p.vr_pp_mask) == 9 && (i == 2 || i == 4 || i == 6 || i == 8), sprintf("a hit from the left: one piece, on the left (piece %g, armour %g)", i, p.armorvalue));
    VR_Test_Check(vr_pp_cracks == cracks + 1, "... its crack");
    VR_Test_Check(vr_pp_pulses > pulses, "... a pulse in the hands");
    VR_Test_Check(vr_ar_debris_made == made + 1, "... its debris");
    entity d = find(world, classname, "vr_ar_debris");
    VR_Test_Check(d != world && vlen(d.origin - p.origin) < 80, sprintf("... where it was (%g from you)", d != world ? vlen(d.origin - p.origin) : -1));

    VR_PPT_Set(p, 1, 100);
    VR_PP_Frame(p);
    VR_PPT_Hit(p, right, 30);
    VR_PP_Frame(p);
    i = vr_pp_last_pop;
    VR_Test_Check(VR_PP_CountOf(p.vr_pp_mask) == 9 && (i == 3 || i == 5 || i == 7 || i == 9), sprintf("a hit from the right: one piece, on the right (piece %g)", i));

    VR_PPT_Set(p, 1, 100);
    VR_PP_Frame(p);
    VR_PPT_CleanDebris();
    made = vr_ar_debris_made;
    VR_PPT_Hit(p, p.origin + '0 0 400', 30);
    VR_PP_Frame(p);
    VR_Test_Check(vr_pp_last_pop == 1 && vr_ar_debris_made == made, sprintf("a hit from above: the helmet, no debris round your eyes (piece %g, debris %g)", vr_pp_last_pop, vr_ar_debris_made - made));

    // Down to nothing: the cuirass on until the armour's gone.
    VR_PPT_Set(p, 1, 100);
    VR_PP_Frame(p);
    float ok = TRUE;
    while(p.armorvalue > 0)
    {
        VR_PPT_Hit(p, left, 20);
        VR_PP_Frame(p);
        if(p.armorvalue > 0 && !(p.vr_pp_mask & 1))
        {
            ok = FALSE;
        }
    }
    VR_Test_Check(ok, "hit down to nothing: the cuirass on while any armour's left");
    VR_Test_Check(p.vr_pp_mask == 0 && vr_pp_last_pop == 0, sprintf("... and gone last (mask %g, last piece %g)", p.vr_pp_mask, vr_pp_last_pop));
    VR_PPT_CleanDebris();
};

// The same hits on each tier with the plates on or off: what's left of armour and health, against the old rule worked out here.
float(entity p, float on) VR_PPT_Absorb =
{
    cvar_set("vr_player_plates", on ? "1" : "0");
    float sum = 0;
    float bad = 0;
    for(float tier = 1; tier <= 3; tier++)
    {
        float frac = tier == 3 ? 0.8 : tier == 2 ? 0.6 : 0.3;
        float value = 50 + 50 * tier;
        VR_PPT_Set(p, tier, value);
        VR_PP_Frame(p);
        for(float k = 0; k < 8; k++)
        {
            float dmg = k == 0 ? 5 : k == 1 ? 17 : k == 2 ? 33 : k == 3 ? 60 : k == 4 ? 120 : k == 5 ? 3 : k == 6 ? 250 : 40;
            VR_PPT_Hit(p, p.origin + '300 0 0', dmg);
            VR_PP_Frame(p);
            float save = frac > 0 ? ceil(frac * dmg) : 0;
            if(save >= value)
            {
                save = value;
                frac = 0;
            }
            value = value - save;
            if(p.armorvalue != value || p.health != 10000 - ceil(dmg - save))
            {
                bad = bad + 1;
            }
            sum = sum * 3 % 65536 + p.armorvalue + p.health + p.armortype * 100;
        }
    }
    vr_ppt_mask = bad;
    return sum;
};

// A pickup, the game's own touch.
void(entity p, float which) VR_PPT_Pick =
{
    entity it = spawn();
    it.classname = which == 1 ? ITEM_NAME_ARMOR1 : which == 3 ? ITEM_NAME_ARMORINV : "item_armor_shard";
    it.model = "pp";
    entity oldself = self;
    entity oldother = other;
    self = it;
    other = p;
    if(which == 2)
    {
        MG3_armor_shard_touch();
    }
    else
    {
        armor_touch();
    }
    self = oldself;
    other = oldother;
    remove(it);
};

void(entity p) VR_PPT_Main =
{
    float t = time - vr_ppt_t0;
    if(vr_ppt_wait > 0)
    {
        vr_ppt_wait = vr_ppt_wait - 1;
        return;
    }
    if(t > VR_PPT_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out in state %g", vr_ppt_s));
        vr_ppt_s = 100;
    }
    if(vr_ppt_s == 0)
    {
        VR_Test_Check(VR_PPT_Client(p, 17) > 0, sprintf("the plates' model is made on the body (%g bones)", VR_PPT_Client(p, 17)));
        VR_PPT_Counts(p);
        VR_PPT_Hits(p);
        // The word to the client (a tier and pieces as the server has them).
        VR_PPT_Set(p, 2, 75);
        VR_PP_Frame(p);
        vr_ppt_mask = p.vr_pp_mask;
        VR_PPT_To(1, 3);
    }
    else if(vr_ppt_s == 1)
    {
        VR_Test_Check(VR_PPT_Client(p, 0) == 2 && VR_PPT_Client(p, 1) == vr_ppt_mask, sprintf("the client has the word (colour %g, pieces %g)", VR_PPT_Client(p, 0), VR_PPT_Client(p, 1)));
        VR_PPT_Set(p, 0, 0);
        vr_ppt_case = 1;
        VR_PPT_To(10, 3);
    }
    // ---- A pickup through the morph (case 1 new armour, 2 a shard, 3 an upgrade) ----
    else if(vr_ppt_s == 10)
    {
        if(vr_ppt_case == 2)
        {
            VR_PPT_Set(p, 1, 42);
            VR_PPT_To(11, 3 + rint(0.8 * 72)); // (its own morph out of the way)
        }
        else if(vr_ppt_case == 3)
        {
            VR_PPT_Set(p, 1, 42);
            VR_PPT_To(11, 3);
        }
        else
        {
            VR_PPT_To(11, 3);
        }
    }
    else if(vr_ppt_s == 11)
    {
        vr_ppt_mask = p.vr_pp_mask;
        vr_ppt_begun = VR_PPT_Client(p, 14);
        vr_ppt_frames = 0;
        VR_PPT_Pick(p, vr_ppt_case);
        VR_PPT_To(12, 0);
    }
    else if(vr_ppt_s == 12)
    {
        float coming = VR_PPT_Client(p, 11);
        if(coming)
        {
            vr_ppt_start = time;
            vr_ppt_prog0 = VR_PPT_Client(p, 13);
            vr_ppt_prog = vr_ppt_prog0;
            vr_ppt_frames = 1;
            float want = p.vr_pp_mask;
            if(vr_ppt_case == 1)
            {
                VR_Test_Check(coming == want && want == 1023 && VR_PPT_Client(p, 10) == 0, sprintf("new armour: every piece morphing in, none popped in (coming %g, settled %g)", coming, VR_PPT_Client(p, 10)));
            }
            else if(vr_ppt_case == 2)
            {
                VR_Test_Check(VR_PP_CountOf(coming) == 1 && (coming | VR_PPT_Client(p, 10)) == want && p.armorvalue == 47, sprintf("a shard (armour %g): the one piece it restores morphing in (coming %g, settled %g, shown %g)", p.armorvalue, coming, VR_PPT_Client(p, 10), want));
            }
            else
            {
                VR_Test_Check(coming == 1023 && VR_PPT_Client(p, 12) == vr_ppt_mask && VR_PPT_Client(p, 18) == 3, sprintf("an upgrade: every piece in red morphing in, the old green's going out (coming %g, going %g, colour %g)", coming, VR_PPT_Client(p, 12), VR_PPT_Client(p, 18)));
            }
            VR_Test_Check(VR_PPT_Client(p, 14) > vr_ppt_begun, "... a morph begun");
            VR_PPT_To(13, 0);
        }
    }
    else if(vr_ppt_s == 13)
    {
        float now = VR_PPT_Client(p, 11);
        if(now)
        {
            vr_ppt_frames = vr_ppt_frames + 1;
            vr_ppt_prog = VR_PPT_Client(p, 13);
            if(vr_ppt_frames == 10)
            {
                VR_Test_Check((VR_PPT_Client(p, 10) & now) == 0, "... a piece coming in not drawn whole meanwhile");
            }
        }
        else
        {
            float took = time - vr_ppt_start;
            float expect = cvar_hget(cvarh_vr_player_plates_morph_time);
            VR_Test_Check(vr_ppt_frames > 10 && took > expect * 0.8, sprintf("... over time, not a frame: %g frames, %g s (of %g), progress %g to %g per mille", vr_ppt_frames, took, expect, vr_ppt_prog0, vr_ppt_prog));
            VR_Test_Check(vr_ppt_prog > vr_ppt_prog0 + 500, "... its progress climbing");
            VR_Test_Check(VR_PPT_Client(p, 10) == p.vr_pp_mask && VR_PPT_Client(p, 12) == 0, sprintf("... then settled (%g, want %g)", VR_PPT_Client(p, 10), p.vr_pp_mask));
            vr_ppt_case = vr_ppt_case + 1;
            VR_PPT_To(vr_ppt_case > 3 ? 20 : 10, 0);
        }
    }
    // ---- The damage the same, on and off ----
    else if(vr_ppt_s == 20)
    {
        float on = VR_PPT_Absorb(p, TRUE);
        float badOn = vr_ppt_mask;
        float off = VR_PPT_Absorb(p, FALSE);
        float badOff = vr_ppt_mask;
        cvar_set("vr_player_plates", "1");
        VR_PP_Frame(p);
        VR_Test_Check(on == off, sprintf("damage absorbed the same with the plates on and off (%g, %g)", on, off));
        VR_Test_Check(badOn == 0 && badOff == 0, sprintf("... and as the old rule has it (off by %g, %g)", badOn, badOff));
        VR_PPT_CleanDebris();
        VR_PPT_Set(p, 3, 200);
        VR_PPT_To(30, 3);
    }
    // ---- The helmet ----
    else if(vr_ppt_s == 30)
    {
        if(VR_PPT_Client(p, 10) == 1023 && VR_PPT_Client(p, 11) == 0)
        {
            VR_PPT_To(31, 3);
        }
    }
    else if(vr_ppt_s == 31)
    {
        float eyes = VR_PPT_Client(p, 15);
        VR_Test_Check((p.vr_pp_mask & 2) && (eyes & 1) && !(eyes & 2), sprintf("from the eyes (%s): the helmet worn, not drawn (drawn %g)", p.vr_flatscreen ? "flat-screen" : "VR", eyes));
        VR_Test_Check(VR_PPT_Client(p, 16) == 0, "... nothing drawn with the head");
        if(p.vr_flatscreen)
        {
            VR_PPT_To(100, 0);
        }
        else
        {
            localcmd("vr_mock_camera 0 1.3 -1.8 0 180\n");
            VR_PPT_To(32, 4);
        }
    }
    else if(vr_ppt_s == 32)
    {
        VR_Test_Check((VR_PPT_Client(p, 16) & 2) != 0, sprintf("from outside: the helmet with the head (drawn %g)", VR_PPT_Client(p, 16)));
        localcmd("vr_mock_camera\n");
        VR_PPT_To(100, 2);
    }
    else if(vr_ppt_s == 100)
    {
        VR_PPT_Set(p, 0, 0);
        VR_Test_Verdict();
        vr_ppt_on = 0;
    }
};

// A mask no canonical order gives: hits from the left, red down to about half. Noted, with the morphs begun so far.
void(entity p) VR_PPT_Crooked =
{
    makevectors([0, p.angles_y, 0]);
    VR_PPT_Set(p, 3, 200);
    VR_PP_Frame(p);
    while(p.armorvalue > 105)
    {
        VR_PPT_Hit(p, p.origin - v_right * 400, 30);
        VR_PP_Frame(p);
    }
    vr_ppt_mask = p.vr_pp_mask;
    VR_PPT_CleanDebris();
};

// vr_pp_test 8, 9: pictures. Each: the armour, its morph done, a screenshot.
void(entity p) VR_PPT_Shots =
{
    float n = vr_ppt_on == 9 ? 1 : 6;
    if(vr_ppt_wait > 0)
    {
        vr_ppt_wait = vr_ppt_wait - 1;
        return;
    }
    if(vr_ppt_s == 0)
    {
        float level = floor(vr_ppt_shot / 2);
        VR_PPT_Set(p, 3, level == 0 ? 200 : level == 1 ? 100 : 1);
        p.health = 100;
        localcmd(vr_ppt_on == 9 ? "vr_mock_look 55 0\n" : vr_ppt_shot % 2 == 0 ? "vr_mock_camera 0 1.0 -1.7 0 180\n" : "vr_mock_camera 0 1.0 1.7 0 0\n");
        VR_PPT_To(1, rint(0.8 * 72));
    }
    else if(vr_ppt_s == 1)
    {
        dprint(sprintf("ppshot: %g: drawn %g, coming %g, eyes %g, head %g\n", vr_ppt_shot, VR_PPT_Client(p, 10), VR_PPT_Client(p, 11), VR_PPT_Client(p, 15), VR_PPT_Client(p, 16)));
        localcmd("screenshot\n");
        VR_PPT_To(2, 4);
    }
    else if(vr_ppt_s == 2)
    {
        vr_ppt_shot = vr_ppt_shot + 1;
        VR_PPT_To(0, 0);
        if(vr_ppt_shot >= n)
        {
            localcmd("vr_mock_camera\n");
            bprint("ppshot: done\n");
            vr_ppt_on = 0;
        }
    }
};

// vr_pp_test 10: the look's sheet where you stand (in a dark place: `notarget` first): the three tiers on you (from the front) and
// on an armed grunt ahead, then a grunt of each tier 10 m away, a screenshot each (`ppshot: done`). vr_pp_test 11: every
// wearer in red, front and back, then each body build (vr_body_build), for the fit.
entity vr_ppt_g0, vr_ppt_g1, vr_ppt_g2;
vector vr_ppt_at0, vr_ppt_at1, vr_ppt_at2;

void() VR_PPT_LookClean =
{
    if(vr_ppt_g0) { remove(vr_ppt_g0); }
    if(vr_ppt_g1) { remove(vr_ppt_g1); }
    if(vr_ppt_g2) { remove(vr_ppt_g2); }
    vr_ppt_g0 = world;
    vr_ppt_g1 = world;
    vr_ppt_g2 = world;
};

entity(entity p, float kind, float type, float side, float ahead) VR_PPT_LookWearer =
{
    entity g = VR_ART_SpawnKind(p, side, kind, ahead);
    VR_AgArmor_Give(g, type, 1023);
    return g;
};

void(entity p) VR_PPT_Look =
{
    float n = vr_ppt_on == 10 ? 7 : vr_ppt_on == 12 ? 4 : 12;
    float k = vr_ppt_shot;
    // (they stand where they were put, whatever their AI wants: the same picture every run)
    if(vr_ppt_g0) { setorigin(vr_ppt_g0, [vr_ppt_at0_x, vr_ppt_at0_y, vr_ppt_g0.origin_z]); }
    if(vr_ppt_g1) { setorigin(vr_ppt_g1, [vr_ppt_at1_x, vr_ppt_at1_y, vr_ppt_g1.origin_z]); }
    if(vr_ppt_g2) { setorigin(vr_ppt_g2, [vr_ppt_at2_x, vr_ppt_at2_y, vr_ppt_g2.origin_z]); }
    if(vr_ppt_wait > 0)
    {
        vr_ppt_wait = vr_ppt_wait - 1;
        return;
    }
    if(vr_ppt_s == 0)
    {
        VR_PPT_LookClean();
        if(vr_ppt_on == 10)
        {
            VR_PPT_Set(p, k < 3 ? k + 1 : 0, k < 3 ? 50 + 50 * (k + 1) : 0);
            localcmd(k < 3 ? "vr_mock_camera 0 0.95 -1.95 0 180\n" : k < 6 ? "vr_mock_camera 0 0.95 -0.9 0 0\n" : "vr_mock_camera\n");
            if(k >= 3 && k < 6)
            {
                vr_ppt_g0 = VR_PPT_LookWearer(p, 0, k - 2, 0, 100);
            }
            if(k == 6)
            {
                vr_ppt_g0 = VR_PPT_LookWearer(p, 0, 1, -60, 262);
                vr_ppt_g1 = VR_PPT_LookWearer(p, 0, 2, 0, 262);
                vr_ppt_g2 = VR_PPT_LookWearer(p, 0, 3, 60, 262);
            }
        }
        else if(vr_ppt_on == 12)
        {
            // (bent: your hands in at the chest, your head down, knees and elbows showing; a grunt on the move; front, then side, yellow)
            VR_PPT_Set(p, k < 2 ? 2 : 0, k < 2 ? 150 : 0);
            localcmd("vr_mock_hand main 0.18 1.0 -0.25 0 0 0;vr_mock_hand off -0.18 1.0 -0.25 0 0 0;vr_mock_hand head 0 1.15 0.1\n");
            localcmd(k == 0 ? "vr_mock_camera 0 0.8 -1.8 0 180\n" : k == 1 ? "vr_mock_camera 1.8 0.8 0 0 90\n" : k == 2 ? "vr_mock_camera 0 0.95 -0.9 0 0\n" : "vr_mock_camera 2.0 0.95 -3.3 0 90\n");
            if(k >= 2)
            {
                vr_ppt_g0 = VR_PPT_LookWearer(p, 0, 2, 0, 100);
            }
        }
        else if(vr_ppt_on == 13)
        {
            // (the joints: a grunt, green as in his pictures: whole, front and back; an elbow and a knee close to; then each part of a limb
            // worn alone; then yours, bent, from the front and the side)
            VR_PPT_Set(p, k >= 10 ? 2 : 0, k >= 10 ? 150 : 0);
            localcmd("vr_mock_hand main 0.18 1.0 -0.25 0 0 0;vr_mock_hand off -0.18 1.0 -0.25 0 0 0;vr_mock_hand head 0 1.15 0.1\n");
            if(k == 10) { localcmd("vr_mock_camera 0 0.8 -1.3 0 180\n"); }
            else if(k == 11) { localcmd("vr_mock_camera 0.8 0.5 -0.5 0 110\n"); }
            else if(k < 2) { localcmd("vr_mock_camera 0 0.8 -1.3 0 0\n"); }
            else if(k == 2 || k == 4 || k == 5) { localcmd("vr_mock_camera 0.3 0.95 -1.85 0 0\n"); }
            else { localcmd("vr_mock_camera 0.2 0.4 -1.75 0 0\n"); }
            if(k < 10)
            {
                float mask = 1023;
                if(k == 4 || k == 6) { mask = 12; }
                if(k == 5 || k == 7) { mask = 48; }
                if(k == 8) { mask = 192; }
                if(k == 9) { mask = 768; }
                vr_ppt_g0 = VR_ART_SpawnKind(p, 0, 0, 100);
                VR_AgArmor_Give(vr_ppt_g0, 1, mask);
                if(k == 1)
                {
                    vr_ppt_g0.angles_y = vr_ppt_g0.angles_y + 180;
                    vr_ppt_g0.ideal_yaw = vr_ppt_g0.angles_y;
                }
            }
        }
        else if(k < 8)
        {
            // (each kind, facing you, then turned away)
            VR_PPT_Set(p, 0, 0);
            localcmd("vr_mock_camera 0 0.95 -0.9 0 0\n");
            vr_ppt_g0 = VR_PPT_LookWearer(p, floor(k / 2), 3, 0, 100);
            if(k % 2)
            {
                vr_ppt_g0.angles_y = vr_ppt_g0.angles_y + 180;
                vr_ppt_g0.ideal_yaw = vr_ppt_g0.angles_y;
            }
        }
        else
        {
            VR_PPT_Set(p, 3, 200);
            localcmd(sprintf("vr_body_build %g; vr_mock_camera 0.9 0.95 -1.1 0 140\n", k - 8));
        }
        vr_ppt_at0 = vr_ppt_g0.origin;
        vr_ppt_at1 = vr_ppt_g1.origin;
        vr_ppt_at2 = vr_ppt_g2.origin;
        VR_PPT_To(1, rint(1.3 * 72));
    }
    else if(vr_ppt_s == 1)
    {
        localcmd("screenshot\n");
        VR_PPT_To(2, 4);
    }
    else if(vr_ppt_s == 2)
    {
        vr_ppt_shot = vr_ppt_shot + 1;
        VR_PPT_To(0, 0);
        if(vr_ppt_shot >= n)
        {
            VR_PPT_LookClean();
            VR_PPT_Set(p, 0, 0);
            localcmd("vr_mock_camera\n");
            bprint("ppshot: done\n");
            vr_ppt_on = 0;
        }
    }
};

// vr_pp_test 14: each kind in all its plates, coming for you (god mode first) where they stand: the body against the plates in the poses
// they're drawn in (vr_agarmor_pop_sample, a sample every 6 frames, 80 of them), then the sums (`agpop: live report done`).
float vr_ppt_samples;
void(entity p) VR_PPT_Live =
{
    if(vr_ppt_g0) { setorigin(vr_ppt_g0, [vr_ppt_at0_x, vr_ppt_at0_y, vr_ppt_g0.origin_z]); }
    if(vr_ppt_g1) { setorigin(vr_ppt_g1, [vr_ppt_at1_x, vr_ppt_at1_y, vr_ppt_g1.origin_z]); }
    if(vr_ppt_g2) { setorigin(vr_ppt_g2, [vr_ppt_at2_x, vr_ppt_at2_y, vr_ppt_g2.origin_z]); }
    if(vr_ppt_wait > 0)
    {
        vr_ppt_wait = vr_ppt_wait - 1;
        return;
    }
    if(vr_ppt_s == 0)
    {
        vr_ppt_g0 = VR_PPT_LookWearer(p, 0, 1, -80, 200);
        vr_ppt_g1 = VR_PPT_LookWearer(p, 1, 1, 0, 200);
        vr_ppt_g2 = VR_PPT_LookWearer(p, 3, 1, 80, 200);
        vr_ppt_at0 = vr_ppt_g0.origin;
        vr_ppt_at1 = vr_ppt_g1.origin;
        vr_ppt_at2 = vr_ppt_g2.origin;
        vr_ppt_samples = 0;
        VR_PPT_To(1, 72);
        return;
    }
    localcmd("vr_agarmor_pop_sample\n");
    vr_ppt_samples = vr_ppt_samples + 1;
    VR_PPT_To(1, 6);
    if(vr_ppt_samples >= 80)
    {
        VR_PPT_LookClean();
        localcmd("vr_agarmor_pop_report\n");
        vr_ppt_on = 0;
    }
};

void() VR_PP_TestFrame =
{
    float ask = cvar_hget(cvarh_vr_pp_test);
    entity p = VR_Player();
    if(ask && p) // (the request kept for when the player is in: `map x; vr_pp_test n` asks in the level's first frames)
    {
        cvar_set("vr_pp_test", "0");
        if(ask == 1)
        {
            VR_Test_BeginFlags("pptest", VR_TEST_LOUD);
            vr_ppt_on = 1;
            // (the body shown, its shell made, past the new level's settling second: setupPlates)
            VR_PPT_To(0, 110);
        }
        if(ask == 2)
        {
            VR_Test_BeginFlags("ppsave", VR_TEST_LOUD);
            VR_PPT_Crooked(p);
            p.vr_pp_type = 3;
            vr_ppt_begun = VR_PPT_Client(p, 14);
            bprint(sprintf("ppsave: saving mask %g, armour %g\n", vr_ppt_mask, p.armorvalue));
            localcmd("wait; wait; save pp_test; wait; vr_pp_test 20\n");
        }
        if(ask == 20)
        {
            // What's on after the save: another state altogether, for the load to put right.
            VR_PPT_Set(p, 1, 3);
            VR_PP_Frame(p);
            VR_Test_Check(p.vr_pp_mask != vr_ppt_mask, sprintf("after the save the pieces are others (%g)", p.vr_pp_mask));
            localcmd("load pp_test; wait; vr_pp_test 21\n");
        }
        if(ask == 21)
        {
            vr_ppt_on = 21;
            VR_PPT_To(0, 3);
        }
        if(ask == 3)
        {
            VR_Test_BeginFlags("pplevel", VR_TEST_LOUD);
            VR_PPT_Crooked(p);
            cvar_set("scratch1", ftos(vr_ppt_mask));
            cvar_set("scratch2", ftos(VR_PPT_Client(p, 14)));
            bprint(sprintf("pplevel: leaving with mask %g, armour %g\n", vr_ppt_mask, p.armorvalue));
            localcmd("wait; wait; changelevel e1m1; wait; vr_pp_test 30\n");
        }
        if(ask == 30)
        {
            VR_Test_BeginFlags("pplevel", VR_TEST_LOUD);
            vr_ppt_on = 30;
            VR_PPT_To(0, 3);
        }
        if(ask == 5 || ask == 6 || ask == 7)
        {
            VR_PPT_Set(p, 3, ask == 5 ? 200 : ask == 6 ? 100 : 1);
            bprint(sprintf("pp: armour %g (Debug > Tests)\n", p.armorvalue));
        }
        if(ask == 8 || ask == 9 || ask == 10 || ask == 11 || ask == 12 || ask == 13 || ask == 14)
        {
            vr_ppt_on = ask;
            vr_ppt_shot = 0;
            VR_PPT_To(0, 110);
        }
    }
    if(!vr_ppt_on || !p)
    {
        return;
    }
    if(vr_ppt_on == 1)
    {
        VR_PPT_Main(p);
    }
    else if(vr_ppt_on == 14)
    {
        VR_PPT_Live(p);
    }
    else if(vr_ppt_on == 10 || vr_ppt_on == 11 || vr_ppt_on == 12 || vr_ppt_on == 13)
    {
        VR_PPT_Look(p);
    }
    else if(vr_ppt_on == 8 || vr_ppt_on == 9)
    {
        VR_PPT_Shots(p);
    }
    else if(vr_ppt_on == 21 || vr_ppt_on == 30)
    {
        if(vr_ppt_wait > 0)
        {
            vr_ppt_wait = vr_ppt_wait - 1;
            return;
        }
        float mask = vr_ppt_on == 21 ? vr_ppt_mask : cvar_hget(cvarh_scratch1);
        float begun = vr_ppt_on == 21 ? vr_ppt_begun : cvar_hget(cvarh_scratch2);
        float told = VR_PPT_Client(p, 1);
        if(vr_ppt_s == 0 && told != mask && time - vr_ppt_t0 < VR_PPT_GIVEUP)
        {
            return; // (the client's not told yet)
        }
        if(vr_ppt_s == 0)
        {
            VR_PPT_To(1, 10);
            return;
        }
        VR_Test_Check(p.vr_pp_mask == mask, sprintf("the very pieces kept (%g, was %g; armour %g)", p.vr_pp_mask, mask, p.armorvalue));
        VR_Test_Check(p.vr_pp_type == 3 && VR_PPT_Client(p, 0) == 3, sprintf("the colour kept (%g)", p.vr_pp_type));
        VR_Test_Check(told == mask, sprintf("the client told (%g)", told));
        VR_Test_Check(VR_PPT_Client(p, 14) == begun && VR_PPT_Client(p, 11) == 0, sprintf("no morph for it (begun %g, was %g; coming %g)", VR_PPT_Client(p, 14), begun, VR_PPT_Client(p, 11)));
        VR_Test_Check(VR_PPT_Client(p, 10) == mask, sprintf("drawn (%g)", VR_PPT_Client(p, 10)));
        VR_Test_Verdict();
        vr_ppt_on = 0;
    }
};
#endif

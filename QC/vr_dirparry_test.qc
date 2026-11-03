#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_dirparry_test.qc -- `vr_agrunt_test 78` (Debug > Tests > Armed Grunts: Directional Parry): the directional parry
// (vr_dirparry.qc; ROUND23.md, "Directional parry"). In vrtesthall, you unseen; for each case a fresh sword grunt 36
// units ahead, held, then its swing forced (an overhead or a horizontal one, its blade as drawn), you holding a weapon
// still in your main hand (the hand, its turn and its muzzle set each frame: the weapon's own line from its hand to its
// far end, measured in the hand's frame from the tracked pose). Your weapon's middle is put where its blade was at its
// blow in that kind's first swing (cases 0 and 1: your fists, nothing parries: the reference, the blow lands):
//   2 overhead vs your sword held level across: parried (no damage, sparks within 2 units of both lines);
//   3 overhead vs your sword upright: parried (the grunt's overhead is a diagonal cut, ~30 deg off vertical, so an
//   upright sword is across it too; his call 2026-10-31: accepted as a diagonal cut);  4 horizontal vs upright: parried;
//   5 horizontal vs level across: lands: its blade comes in level, along yours, 83 deg off square, gap 15.3. (Up to
//   2026-11-01's runs it was parried, a diagonal cut, and his call of 2026-11-03 on that was "Accept as is": no change to
//   the swing. The parry he accepted isn't there on next: ROUND23.md, "His decisions of 2026-11-03".);
//   6 overhead vs a shotgun's barrel level across: parried;  7 overhead vs the sword level across 40 units to
//   the side (out of reach): lands.  (8: retired with the level-across parry's switch, 2026-11-03.)
// Its debug view and its settings (each case as the one named, its cvars put back after):
//   9 Show Directional Parry on (vr_dirparry_debug), case 2: parried as before; the view takes its live readings and
//   the strike's verdict (the engine prints `dirparryviz: drawn PARRIED ...` as it draws it held; an eye image taken);
//   10 case 7 (the sword out of reach to the side) with vr_dirparry_contact just past its gap: the case flips, parried;
//   11 Enemy Melee Speed 0.5 (vr_agrunt_melee_speed), case 0: the blow lands as hard, at twice the time into the swing,
//   the swing twice as long (its times, VR_AGP_Length; and as measured, to its think's 0.1 s);  12 speed 0.5, case 2:
//   parried as at full speed.
// The weapon's line as the parry reads it (Show Directional Parry draws it every frame; VR_DRP_DebugLine): 13 a sword,
//   14 an axe, 15 an armed knight's sword (vr_agknight), 16 Mjolnir, 17 a chainsaw (and its thrust), 18 AD's crowbar,
//   19 AD's super axe, each swinging its four swings (kinds 0-3) at your fists: every frame from its hold to its last
//   swing's end, the line's ends (VR_AG_WeaponLine) within 1 unit of the drawn weapon's own butt and tip (its model's,
//   by the renderer's matrix: debugdirparryline); case 13's eye image mid-strike. `scratch1 N` (13 on): from case N;
//   `scratch2 1`: each line case's eye image from close up (vr_dirparry_linecheck look), its line printed in its frame.
// A monster's own blow, its line read from its frames (vr_blowline.cpp; ROUND23.md, "Directional parry: the only parry").
// Stock models, 20 on, a monster held ahead of you and its attack forced; each kind's reference (your fists: the blow
// lands, its line and the plane it sweeps taken), then your sword at that place: across (square to its line and its way):
// parried, sparks; along its way: lands:
//   20-22 a knight's chop (vr_agknight 0; the plane upright: a weapon level across);  23-25 an ogre's chainsaw swing (the
//   plane level: a weapon upright);  26-27 an ogre's overhead smash (its first blow while its saw is still going up; the plane upright: level across);
//   28-29 a fiend's claw, 30-31 a shambler's smash (no along cases: an arm's line turns its own way as it comes, its way at
//   the first meeting isn't the reference's);  32-34 a dog's bite (its lunge's line: the whole dog, pitching);  35-36 a
//   knight's blow as it stands (no stroke in its frames: no line, the guard's, as a fish's bite, a leap landing): your
//   fists: lands, no reading; your sword level across ahead: parried, no reading. Your weapon's place follows the monster
//   (its charge varies from run to run). `scratch2 1` with Show Disarm Volumes on: a screenshot of the knight's and the ogre's lines on them.
// Each `agtest: PASS|FAIL dirparry <case>: ...`; ends on `agtest: test 78 over: P passed, F failed`.
// ----------------------------------------------------------------------------

float VR_DRPT_CASES = 37;
float VR_DRPT_BAKED = 20; // the first monster's own blow case
float vr_drpt_case;      // the case running
float vr_drpt_lk;        // a line case's swing (0-3 its kinds, 4 the chainsaw's thrust)
float vr_drpt_keep_lust; // vr_melee_bloodlust before (off during the test)
float vr_drpt_until;     // its give-up time (a safety net: FAIL "timed out")
float vr_drpt_b_mark;    // a monster's own blow: when its line is taken (the reference)
float vr_drpt_b_len;     // ... its attack's whole length (s)
vector vr_drpt_b_a;      // ... its line then
vector vr_drpt_b_b;
vector vr_drpt_b_rel;    // ... its middle's place from the monster's origin
vector vr_drpt_b_way;    // ... its way (unit)
vector vr_drpt_b_sq;     // ... square to its line and its way (unit)
float vr_drpt_b_hurt;    // ... the reference's damage
float vr_drpt_b_n;       // ... how many lines its blow had
float vr_drpt_keep_parry;
float vr_drpt_keep_contact;
float vr_drpt_keep_debug;
float vr_drpt_keep_speed;
float vr_drpt_hurt_at;   // when you were hurt (s into its swing; 0: not)
float vr_drpt_swung;     // ... its swing's end as its think saw it (s)
float vr_drpt_len;       // ... its swing's length (VR_AGP_Length) during it
float vr_drpt_ref_hurt;  // case 0's: the damage, when, the swing's end and length (case 11's reference)
float vr_drpt_ref_at;
float vr_drpt_ref_swung;
float vr_drpt_ref_len;
float vr_drpt_gap7;      // case 7's gap between the lines (case 10's leniency)
float vr_drpt_dbg0;      // Show Directional Parry's counters at the swing's start
float vr_drpt_dbgf0;
float vr_drpt_ln0;       // the line check's frames at the case's start
float vr_drpt_keep_knight;
float vr_drpt_shot;      // case 13's eye image taken
entity vr_drpt_hurt_by;  // what hurt you first in the case (its inflictor)
vector vr_drpt_r0a;      // the reference swings' blade at their blow: overhead
vector vr_drpt_r0b;
vector vr_drpt_r2a;      // ... horizontal
vector vr_drpt_r2b;
vector vr_drpt_lm;       // your weapon's muzzle (its far end) in your hand's frame
vector vr_drpt_lg;       // ... its parry line's near end
vector vr_drpt_lf;       // ... its far end
vector vr_drpt_rot;      // your hand's turn that lays it along the case's axis
vector vr_drpt_center;   // ... its middle's place
vector vr_drpt_axis;     // ... its axis
vector vr_drpt_home;     // where you stand for every case
vector vr_drpt_hand;     // your hand's place as posed
float vr_drpt_posed;     // the pose is held (its turn found)
float vr_drpt_wid;       // the weapon in your main hand
float vr_drpt_refd;      // the reference swing's blade recorded
float vr_drpt_t0;        // the swing's start
float vr_drpt_ev0;       // counters at the swing's start
float vr_drpt_pr0;
float vr_drpt_fx0;

string(float c) VR_DRPT_Name =
{
    if(c == 0) { return "overhead vs your fists (reference)"; }
    if(c == 1) { return "horizontal vs your fists (reference)"; }
    if(c == 2) { return "overhead vs your sword level across"; }
    if(c == 3) { return "overhead (a diagonal cut) vs your sword upright"; }
    if(c == 4) { return "horizontal vs your sword upright"; }
    if(c == 5) { return "horizontal vs your sword level across"; }
    if(c == 6) { return "overhead vs a shotgun's barrel level across"; }
    if(c == 7) { return "overhead vs your sword level across, 40 units to the side"; }
    if(c == 9) { return "Show Directional Parry on: overhead vs your sword level across"; }
    if(c == 10) { return "contact distance past its gap: overhead vs your sword level across, 40 units to the side"; }
    if(c == 11) { return "Enemy Melee Speed 0.5: overhead vs your fists"; }
    if(c == 12) { return "Enemy Melee Speed 0.5: overhead vs your sword level across"; }
    if(c == 13) { return "the line on the drawn weapon: a grunt's sword, swings 0-3"; }
    if(c == 14) { return "the line on the drawn weapon: a grunt's axe, swings 0-3"; }
    if(c == 15) { return "the line on the drawn weapon: an armed knight's sword, swings 0-3"; }
    if(c == 16) { return "the line on the drawn weapon: a grunt's Mjolnir, swings 0-3"; }
    if(c == 17) { return "the line on the drawn weapon: a grunt's chainsaw, swings 0-3 and its thrust"; }
    if(c == 18) { return "the line on the drawn weapon: a grunt's crowbar (AD), swings 0-3"; }
    return "the line on the drawn weapon: a grunt's super axe (AD), swings 0-3";
};

// An armed knight `dist` ahead of you (vr_agknight on: monster_knight makes it an armed humanoid with a sword).
entity(float dist) VR_DRPT_Knight =
{
    entity p = vr_agt_player;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    traceline(p.origin, p.origin + fw * dist, TRUE, p);
    vector at = trace_endpos - fw * 24;
    entity oldself = self;
    self = spawn();
    self.classname = "monster_knight";
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    monster_knight();
    entity g = self;
    self = oldself;
    return g;
};

// The case whose geometry (its swing, your weapon and its pose) case `c` takes.
float(float c) VR_DRPT_Geo = { return ((c == 9) || (c == 12)) ? 2 : (c == 10) ? 7 : (c == 11) ? 0 : (c >= 13) ? 1 : c; };

// A line case's weapon (13 on): a sword, an axe, (15 the armed knight's sword), Mjolnir, a chainsaw, AD's crowbar and
// its super axe.
float(float c) VR_DRPT_LineWid =
{
    return c == 14 ? WID_AXE : c == 16 ? WID_MJOLNIR : c == 17 ? WID_CHAINSAW : c == 18 ? WID_CROWBAR : c == 19 ?
        WID_SUPERAXE : WID_SWORD;
};

float(float c) VR_DRPT_Kind =
{
    if(c >= 13)
    {
        return vr_drpt_lk; // (a line case: each swing kind in turn)
    }
    float g = VR_DRPT_Geo(c);
    return (g == 0 || g == 2 || g == 3 || g == 6 || g == 7) ? 0 : 2;
};
float(float c) VR_DRPT_Wants = { return (c == 2) || (c == 3) || (c == 4) || (c == 6) || (c == 9) || (c == 10) || (c == 12); };

vector(vector d, vector ang) VR_DRPT_Local =
{
    makevectors(ang);
    return [d * v_forward, d * v_right, d * v_up];
};

vector(vector l, vector ang) VR_DRPT_World =
{
    makevectors(ang);
    return v_forward * l_x + v_right * l_y + v_up * l_z;
};

// The hand's turn laying its weapon's line along `axis` (either way): the best of a 15-degree grid, then 3 degrees round it.
vector(vector axis) VR_DRPT_Solve =
{
    vector dl = normalize(vr_drpt_lf - vr_drpt_lg);
    float best = -1;
    vector ba = '0 0 0';
    vector a = '0 0 0';
    float step = 15;
    for(float pass = 0; pass < 2; pass++)
    {
        vector c = ba;
        float span = pass == 0 ? 0 : 12;
        float lo = pass == 0 ? -90 : -span;
        for(float x = lo; x <= (pass == 0 ? 90 : span); x += step)
        {
            for(float y = (pass == 0 ? 0 : -span); y <= (pass == 0 ? 345 : span); y += step)
            {
                for(float z = (pass == 0 ? 0 : -span); z <= (pass == 0 ? 345 : span); z += step)
                {
                    a = pass == 0 ? [x, y, z] : c + [x, y, z];
                    float d = fabs(normalize(VR_DRPT_World(dl, a)) * axis);
                    if(d > best)
                    {
                        best = d;
                        ba = a;
                    }
                }
            }
        }
        step = 3;
    }
    return ba;
};

// Your main hand held in the case's pose this frame (the tracking's put back each frame by the engine).
void(entity p) VR_DRPT_Apply =
{
    vector g = VR_DRPT_World(vr_drpt_lg, vr_drpt_rot);
    vector f = VR_DRPT_World(vr_drpt_lf, vr_drpt_rot);
    vector m = VR_DRPT_World(vr_drpt_lm, vr_drpt_rot);
    p.handrot = vr_drpt_rot;
    p.handpos = vr_drpt_center - (g + f) * 0.5;
    vr_drpt_hand = p.handpos;
    p.muzzlepos = p.handpos + m;
    p.handvel = '0 0 0';
};

void() VR_DRPT_End =
{
    entity p = vr_agt_player;
    cvar_set("vr_agrunt_parry", ftos(vr_drpt_keep_parry));
    localcmd(sprintf("tune vr_dirparry_contact %g\n", vr_drpt_keep_contact)); // (a tunable: set by `tune`)
    cvar_set("vr_dirparry_debug", ftos(vr_drpt_keep_debug));
    cvar_set("vr_agrunt_melee_speed", ftos(vr_drpt_keep_speed));
    cvar_set("vr_agknight", ftos(vr_drpt_keep_knight));
    cvar_set("vr_melee_bloodlust", ftos(vr_drpt_keep_lust));
    localcmd("vr_mock_button main grip 0\n");
    VR_AGPT_Clear();
    VR_AGPT_EmptyHand(p);
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    vr_drpt_case = 0;
    cvar_set("vr_agrunt_test", "0");
};

// The case's outcome after its swing.
void(entity g, float c) VR_DRPT_Judge =
{
    entity p = vr_agt_player;
    float hurt = 1000 - p.health;
    float evals = vr_drp_evals - vr_drpt_ev0;
    float parries = vr_drp_parries - vr_drpt_pr0;
    float fx = vr_drp_fx_n - vr_drpt_fx0;
    float want = VR_DRPT_Wants(c);
    float ok;
    string got = sprintf("hurt %g, directional parries %g (early %g), sparks %g, readings %g", hurt, parries, vr_drp_early,
        fx, evals);
    if(vr_drp_hand >= 0)
    {
        got = sprintf("%s; last: score %.2f, %.0f deg off square, gap %.1f", got, vr_drp_score, vr_drp_off, vr_drp_gap);
        float al = fabs(normalize(vr_drp_pb - vr_drp_pa) * vr_drpt_axis);
        float mid = vlen((vr_drp_pa + vr_drp_pb) * 0.5 - vr_drpt_center);
        got = sprintf("%s; your line along the axis %.3f, its middle %.1f units off (%.1f from you; your hand %.1f off its pose)", got, al, mid,
            vlen(vr_drpt_center - p.origin), vlen(p.handpos - vr_drpt_hand));
        vector tf = p.origin - g.origin;
        tf_z = 0;
        tf = normalize(tf);
        vector tl = VR_Cross('0 0 1', tf); // (its left)
        vector bx = normalize(vr_drp_eb - vr_drp_ea);
        got = sprintf("%s; (towards you, its left, up) its blade [%.2f %.2f %.2f]", got, bx * tf, bx * tl, bx_z);
        got = sprintf("%s, its way [%.2f %.2f %.2f] (%.2f s)", got, vr_drp_sweep * tf, vr_drp_sweep * tl, vr_drp_sweep_z,
            vr_drp_tau);
        vector yl = normalize(vr_drp_pb - vr_drp_pa);
        got = sprintf("%s, your line [%.2f %.2f %.2f]", got, yl * tf, yl * tl, yl_z);
    }
    if(c == 0)
    {
        vr_drpt_ref_hurt = hurt;
        vr_drpt_ref_at = vr_drpt_hurt_at;
        vr_drpt_ref_swung = vr_drpt_swung;
        vr_drpt_ref_len = vr_drpt_len;
    }
    if(c == 7)
    {
        vr_drpt_gap7 = vr_drp_gap;
    }
    float extra = TRUE;
    if(c == 9)
    {
        float dn = vr_drp_dbg_n - vr_drpt_dbg0;
        float df = vr_drp_dbg_final - vr_drpt_dbgf0;
        extra = ((dn >= 2) && (df == 1));
        got = sprintf("%s; the view took %g readings, %g a strike's verdict", got, dn, df);
        cvar_set("vr_eyeshot", "3"); // (its verdict held 3 s from its blow: drawn now)
        VR_AGT_Print("dirparryshot: the strike's verdict held");
    }
    if(c == 10)
    {
        got = sprintf("%s; contact distance %s (case 7: %.1f units apart)", got, ftos(cvar_hget(cvarh_vr_dirparry_contact)),
            vr_drpt_gap7);
    }
    if((c == 11) || (c == 12))
    {
        float lr = vr_drpt_len / max(0.001, vr_drpt_ref_len);
        float sr = vr_drpt_swung / max(0.001, vr_drpt_ref_swung);
        extra = ((fabs(lr - 2) < 0.01) && (sr >= 1.8) && (sr <= 2.2));
        got = sprintf("%s; its length %.3f s (x%.3f), its end %.2f s (x%.2f)", got, vr_drpt_len, lr, vr_drpt_swung, sr);
        if(c == 11)
        {
            float ar = vr_drpt_hurt_at / max(0.001, vr_drpt_ref_at);
            extra = (extra && (fabs(ar - 2) <= 0.05) && (hurt == vr_drpt_ref_hurt));
            got = sprintf("%s; hurt %g at %.3f s (x%.3f; full speed: %g)", got, hurt, vr_drpt_hurt_at, ar, vr_drpt_ref_hurt);
        }
    }
    if(c >= 13)
    {
        float ln = vr_drp_line_n - vr_drpt_ln0;
        extra = ((ln >= 20) && (vr_drp_line_worst <= 1));
        got = sprintf("%s; its line measured on %g frames, its ends at worst %.2f units from the drawn weapon's", got,
            ln, vr_drp_line_worst);
    }
    if(c >= 13)
    {
        ok = extra; // (its blow lands or not: only the line is judged)
    }
    else if(want)
    {
        float dp = VR_AG_SegDist(vr_drp_fx_at, vr_drp_fx_at, vr_drp_pa, vr_drp_pb);
        float de = VR_AG_SegDist(vr_drp_fx_at, vr_drp_fx_at, vr_drp_ea, vr_drp_eb);
        float half = max(0.5, cvar_hget(cvarh_vr_dirparry_contact)) * 0.5 + 0.01; // (the meeting: the middle of the nearest points)
        ok = ((hurt == 0) && (parries == 1) && (fx == 1) && (vlen(vr_drp_fx_at - vr_drp_at) <= 2) && (dp <= half) && (de <= half));
        got = sprintf("%s; sparks %.1f units off your line, %.1f off its blade", got, dp, de);
    }
    else
    {
        ok = ((hurt > 0) && (parries == 0) && (fx == 0) && ((VR_DRPT_Geo(c) < 2) || (evals > 0)));
    }
    ok = (ok && extra);
    VR_AGT_Check(sprintf("dirparry %s: %s; expected %s", VR_DRPT_Name(c), got, want ? "parried, no damage, sparks at "
        "the meeting" : "the blow lands"), ok);
};

// A monster's own blow's case `c`'s kind: 0 a knight's chop, 1 an ogre's swing, 2 an ogre's smash, 3 a fiend's claw, 4 a
// shambler's smash, 5 a dog's bite, 6 a blow from a knight as it stands.
float(float c) VR_DRPT_BKind =
{
    return c < 23 ? 0 : c < 26 ? 1 : c < 28 ? 2 : c < 30 ? 3 : c < 32 ? 4 : c < 35 ? 5 : 6;
};

// ... its role: 0 the reference (your fists), 1 your sword across, 2 along its way, 3 the guard.
float(float c) VR_DRPT_BRole =
{
    if(c == 36)
    {
        return 3;
    }
    if((c == 20) || (c == 23) || (c == 26) || (c == 28) || (c == 30) || (c == 32) || (c == 35))
    {
        return 0;
    }
    return ((c == 22) || (c == 25) || (c == 34)) ? 2 : 1;
};

string(float c) VR_DRPT_BName =
{
    float k = VR_DRPT_BKind(c);
    float r = VR_DRPT_BRole(c);
    string who = k == 0 ? "a knight's chop" : k == 1 ? "an ogre's swing" : k == 2 ? "an ogre's smash" : k == 3 ?
        "a fiend's claw" : k == 4 ? "a shambler's smash" : k == 5 ? "a dog's bite" : "a knight's blow as it stands";
    return sprintf("%s vs %s", who, r == 0 ? "your fists (reference)" : r == 1 ? "your sword across it" : r == 2 ?
        "your sword along its way" : "your sword level across ahead (the guard)");
};

void(entity g, float k) VR_DRPT_BStart =
{
    entity was = self;
    self = g;
    g.enemy = VR_Player();
    g.vr_melee_begun = time;
    if(k == 0) { knight_atk1(); }
    else if(k == 1) { ogre_swing1(); }
    else if(k == 2) { ogre_smash1(); }
    else if(k == 3) { demon1_atta1(); }
    else if(k == 4) { sham_smash1(); }
    else if(k == 5) { dog_atta1(); }
    else { T_Damage(g.enemy, g, g, 10); }
    self = was;
};

// The monster's own blow's cases (20 on): as the armed ones', each case's monster fresh and held, its attack forced.
void() VR_DRPT_BFrame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float c = vr_drpt_case;
    float k = VR_DRPT_BKind(c);
    float r = VR_DRPT_BRole(c);
    if(vr_agt_step == 0)
    {
        setorigin(p, vr_drpt_home);
        p.velocity = '0 0 0';
        cvar_set("vr_agknight", "0");
        localcmd(sprintf("tune vr_dirparry_contact %g\n", vr_drpt_keep_contact));
        cvar_set("vr_dirparry_debug", ftos(vr_drpt_keep_debug));
        cvar_set("vr_agrunt_melee_speed", ftos(vr_drpt_keep_speed));
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        vr_drpt_wid = r == 0 ? WID_FIST : WID_SWORD;
        VR_ItemUtil_EntSetItem(p, IID_SWORD, TRUE);
        p.weapon = vr_drpt_wid;
        vr_drpt_posed = FALSE;
        vr_drpt_until = time + 5;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(r != 0)
        {
            vector h = p.handpos;
            vector hr = p.handrot;
            vr_drpt_lm = '24 0 0';
            p.muzzlepos = h + VR_DRPT_World(vr_drpt_lm, hr);
            if(p.weapon != vr_drpt_wid || !VR_DRP_PlayerSeg(p, cVR_MainHand) || vlen(vr_drp_pb - vr_drp_pa) < 10)
            {
                if(time > vr_drpt_until)
                {
                    VR_AGT_Check(sprintf("dirparry %s: timed out (weapon %g, want %g, its line %.1f)", VR_DRPT_BName(c), p.weapon,
                        vr_drpt_wid, vlen(vr_drp_pb - vr_drp_pa)), FALSE);
                    vr_agt_step = 4;
                }
                return;
            }
            vr_drpt_lg = VR_DRPT_Local(vr_drp_pa - h, hr);
            vr_drpt_lf = VR_DRPT_Local(vr_drp_pb - h, hr);
            makevectors(p.angles);
            vector ahead = v_forward;
            ahead_z = 0;
            ahead = normalize(ahead);
            if(r == 3)
            {
                vr_drpt_axis = normalize(VR_Cross(ahead, '0 0 1'));
                vr_drpt_center = p.origin + ahead * 22 + '0 0 18';
            }
            else
            {
                vr_drpt_axis = r == 1 ? vr_drpt_b_sq : vr_drpt_b_way;
                vr_drpt_center = vr_drpt_b_a + (vr_drpt_b_b - vr_drpt_b_a) * 0.6;
            }
            vr_drpt_rot = VR_DRPT_Solve(vr_drpt_axis);
            vr_drpt_posed = TRUE;
            VR_DRPT_Apply(p);
        }
        float dist = ((k == 0) || (k == 6)) ? 50 : k < 4 ? 70 : k == 4 ? 80 : 45;
        makevectors(p.angles);
        vector fw = v_forward;
        fw_z = 0;
        fw = normalize(fw);
        vector at = p.origin + fw * dist;
        vr_agt_grunt = VR_Test_Spawn(world, ((k == 0) || (k == 6)) ? 5 : k < 3 ? 1 : k == 3 ? 9 : k == 4 ? 3 : 7, at, vectoyaw(fw * -1));
        vr_agt_step = 2;
        return;
    }
    if(vr_drpt_posed)
    {
        if((r == 1) || (r == 2))
        {
            vr_drpt_center = g.origin + vr_drpt_b_rel;
        }
        VR_DRPT_Apply(p);
    }
    if(vr_agt_step == 2)
    {
        if(!g.takedamage)
        {
            if(time > vr_drpt_until)
            {
                VR_AGT_Check(sprintf("dirparry %s: timed out (no monster)", VR_DRPT_BName(c)), FALSE);
                vr_agt_step = 4;
            }
            return;
        }
        p.health = 1000;
        p.armorvalue = 0;
        p.vr_stamina_used = 0;
        vr_drpt_ev0 = vr_drp_evals;
        vr_drpt_pr0 = vr_drp_parries;
        vr_drpt_fx0 = vr_drp_fx_n;
        vr_drpt_hurt_at = 0;
        vr_drp_hand = -1;
        vr_parried = 0;
        vr_drpt_b_len = (k == 0 ? 11 : k < 3 ? 14 : k == 3 ? 15 : k == 4 ? 12 : k == 5 ? 8 : 1) * 0.1;
        vr_drpt_t0 = time;
        // (where its stroke goes, by its frames from now: the knight's and the fiend's after their first blows, the others' at it)
        vr_drpt_b_mark = time + (k == 0 ? 0.75 : k == 1 ? 0.45 : k == 2 ? 0.85 : k == 3 ? 0.55 : k == 4 ? 0.9 : k == 5 ? 0.31 : 0.01);
        VR_DRPT_BStart(g, k);
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        if(!vr_drpt_hurt_at && p.health < 1000)
        {
            vr_drpt_hurt_at = time - vr_drpt_t0;
        }
        if(r == 0 && vr_drpt_b_mark > 0 && time >= vr_drpt_b_mark)
        {
            vr_drpt_b_mark = 0;
            if((cvar_hget(cvarh_scratch2) > 0) && (k < 2))
            {
                localcmd("screenshot\n");
                VR_AGT_Print(sprintf("dirparryshot: %s", VR_DRPT_BName(c)));
            }
            float tt = k == 4 ? 0.05 : 0;
            vr_drpt_b_n = blowline(g, tt - 0.02, 0);
            if(vr_drpt_b_n > 0)
            {
                vector a0 = vr_blow_a;
                vector b0 = vr_blow_b;
                blowline(g, tt, 0);
                vr_drpt_b_a = vr_blow_a;
                vr_drpt_b_b = vr_blow_b;
                vr_drpt_b_rel = vr_blow_a + (vr_blow_b - vr_blow_a) * 0.6 - g.origin;
                blowline(g, tt + 0.02, 0);
                vector ax = normalize(vr_blow_b - vr_blow_a);
                vector way = ax - normalize(b0 - a0); // (its way as VR_DRP_Eval reads it: the turn of its line)
                way = normalize(way - ax * (way * ax));
                vr_drpt_b_way = way;
                vr_drpt_b_sq = normalize(VR_Cross(ax, way));
                vector ptr = normalize(p.origin - g.origin);
                string lt = sprintf("dirparry %s: its line %.1f long, along (to you, up) [%.2f %.2f], way (to you, up) [%.2f %.2f]",
                    VR_DRPT_BName(c), vlen(vr_drpt_b_b - vr_drpt_b_a), ax * ptr, ax_z, way * ptr, way_z);
                VR_AGT_Print(sprintf("%s, square (up) %.2f; reach %.1f, sweep %.0f, %g lines at its blow", lt, vr_drpt_b_sq_z,
                    vr_blow_more_x, vr_blow_more_z, vr_drpt_b_n));
            }
        }
        if(time - vr_drpt_t0 < vr_drpt_b_len)
        {
            return;
        }
        float hurt = 1000 - p.health;
        float evals = vr_drp_evals - vr_drpt_ev0;
        float parries = vr_drp_parries - vr_drpt_pr0;
        float fx = vr_drp_fx_n - vr_drpt_fx0;
        float ok;
        string got = sprintf("hurt %g at %.2f s, directional parries %g, sparks %g, readings %g, vr_parried %g", hurt, vr_drpt_hurt_at,
            parries, fx, evals, vr_parried);
        if(vr_drp_hand >= 0)
        {
            got = sprintf("%s; last: score %.2f, %.0f deg off square, gap %.1f, %.2f s", got, vr_drp_score, vr_drp_off, vr_drp_gap,
                vr_drp_tau);
        }
        if(r == 0)
        {
            vr_drpt_b_hurt = hurt;
            ok = k == 6 ? ((hurt > 0) && (evals == 0)) : ((hurt > 0) && (parries == 0) && (vr_drpt_b_n > 0) && (evals > 0));
            if(k == 0)
            {
                ok = (ok && (fabs(vr_drpt_b_sq_z) < 0.5)); // (a chop: its plane upright: a weapon level across)
            }
            if(k == 1)
            {
                ok = (ok && (fabs(vr_drpt_b_sq_z) > 0.7)); // (a side swing: its plane level: a weapon upright)
            }
            if(k == 2)
            {
                ok = (ok && (fabs(vr_drpt_b_sq_z) < 0.5)); // (a chop, however early its first blow)
            }
        }
        else if(r == 1)
        {
            ok = ((parries == 1) && (fx >= 1)); // (what the parry lets through of a blow of random damage: printed)
        }
        else if(r == 2)
        {
            ok = ((parries == 0) && (fx == 0) && (hurt > 0));
        }
        else
        {
            ok = ((evals == 0) && (vr_parried == 1));
        }
        VR_AGT_Check(sprintf("dirparry %s: %s; reference hurt %g", VR_DRPT_BName(c), got, vr_drpt_b_hurt), ok);
        vr_agt_step = 4;
        return;
    }
    if(g)
    {
        VR_AGT_Remove(g);
    }
    vr_agt_grunt = world;
    vr_drpt_posed = FALSE;
    vr_drpt_case = vr_drpt_case + 1;
    vr_agt_step = 0;
};

void() VR_DRPT_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float c = vr_drpt_case;
    if(c >= VR_DRPT_CASES)
    {
        VR_DRPT_End();
        return;
    }
    if(c == 8)
    {
        c = vr_drpt_case = 9; // (retired: the level-across parry's switch)
    }
    if(c >= VR_DRPT_BAKED)
    {
        VR_DRPT_BFrame();
        return;
    }
    if(vr_agt_step == 0)
    {
        if(c == 0)
        {
            vr_drpt_keep_parry = cvar_hget(cvarh_vr_agrunt_parry);
            vr_drpt_keep_contact = cvar_hget(cvarh_vr_dirparry_contact);
            vr_drpt_keep_debug = cvar_hget(cvarh_vr_dirparry_debug);
            vr_drpt_keep_speed = cvar_hget(cvarh_vr_agrunt_melee_speed);
            vr_drpt_keep_knight = cvar_hget(cvarh_vr_agknight);
            vr_drpt_keep_lust = cvar_hget(cvarh_vr_melee_bloodlust);
            // (your melee's bloodlust off: a hit of yours (case 12's counter, your posed sword's pommel) heals you, and
            // a heal clamps your 1000 test health to the megahealth cap (T_Heal: 250): read as 750 damage)
            cvar_set("vr_melee_bloodlust", ftos(QVR_MELEEBLOODLUST_DISABLED)); // (0 is on: QVR_MELEEBLOODLUST_ENABLED)
            cvar_set("vr_agrunt_parry", "0"); // (its own guard and parries out of it: its swing as forced)
            localcmd("vr_mock_button main grip 1\n"); // (your weapon held: not let go of)
            p.flags = p.flags | FL_NOTARGET;
            vr_drpt_home = p.origin;
            if(rint(cvar_hget(cvarh_scratch1)) >= 13) // (the monster's own blows' references need their kind's first case)
            {
                c = vr_drpt_case = rint(cvar_hget(cvarh_scratch1)); // (scratch1 N >= 13: the line cases from N; they need no reference)
                if(c >= VR_DRPT_BAKED)
                {
                    return;
                }
            }
        }
        setorigin(p, vr_drpt_home); // (where the reference swings found you: a blow's knockback undone)
        p.velocity = '0 0 0';
        cvar_set("vr_dirparry_debug", ((c == 9) || (c >= 13)) ? "1" : ftos(vr_drpt_keep_debug));
        cvar_set("vr_agknight", c == 15 ? "1" : ftos(vr_drpt_keep_knight));
        localcmd(sprintf("tune vr_dirparry_contact %g\n", c == 10 ? ceil(vr_drpt_gap7) + 1 : vr_drpt_keep_contact));
        cvar_set("vr_agrunt_melee_speed", ((c == 11) || (c == 12)) ? "0.5" : ftos(vr_drpt_keep_speed));
        float geo = VR_DRPT_Geo(c);
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        vr_drpt_wid = geo < 2 ? WID_FIST : geo == 6 ? WID_SHOTGUN : WID_SWORD;
        VR_ItemUtil_EntSetItem(p, IID_SWORD, TRUE); // (owned: kept in your hand)
        VR_ItemUtil_EntSetItem(p, IID_SHOTGUN, TRUE);
        p.weapon = vr_drpt_wid;
        vr_drpt_posed = FALSE;
        vr_drpt_until = time + 5;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        // Your weapon's line (VR_DRP_PlayerSeg: VR_Melee_Line's, its Edit Blade Shape ends) measured in your hand's
        // frame, its pose for the case found and held from now on; the grunt spawned. (The headless mock places no
        // weapon's muzzle: its muzzle is put along the hand's forward, a sword's 24 units, a shotgun's 20.)
        if(VR_DRPT_Geo(c) >= 2)
        {
            vector h = p.handpos;
            vector r = p.handrot;
            vr_drpt_lm = vr_drpt_wid == WID_SHOTGUN ? '20 0 0' : '24 0 0';
            p.muzzlepos = h + VR_DRPT_World(vr_drpt_lm, r);
            if(p.weapon != vr_drpt_wid || !VR_DRP_PlayerSeg(p, cVR_MainHand) || vlen(vr_drp_pb - vr_drp_pa) < 10)
            {
                if(time > vr_drpt_until)
                {
                    VR_AGT_Check(sprintf("dirparry %s: timed out (weapon %g, want %g, its line %.1f units)",
                        VR_DRPT_Name(c), p.weapon, vr_drpt_wid, vlen(vr_drp_pb - vr_drp_pa)), FALSE);
                    vr_agt_step = 4;
                }
                return;
            }
            vr_drpt_lg = VR_DRPT_Local(vr_drp_pa - h, r);
            vr_drpt_lf = VR_DRPT_Local(vr_drp_pb - h, r);
            float k = VR_DRPT_Kind(c);
            vector ra = k == 0 ? vr_drpt_r0a : vr_drpt_r2a;
            vector rb = k == 0 ? vr_drpt_r0b : vr_drpt_r2b;
            makevectors(p.angles); // (the grunt spawns ahead of you: VR_AGT_Spawn)
            vector toG = v_forward;
            toG_z = 0;
            vector side = normalize(VR_Cross(normalize(toG), '0 0 1'));
            float gc = VR_DRPT_Geo(c);
            float upright = (gc == 3) || (gc == 4);
            vr_drpt_axis = upright ? '0 0 1' : side;
            vr_drpt_center = ra + (rb - ra) * 0.6 + (gc == 7 ? side * 40 : '0 0 0');
            vr_drpt_rot = VR_DRPT_Solve(vr_drpt_axis);
            dprint(sprintf("dirparry test: weapon %g, its line in the hand %v .. %v, the turn %v, its middle at %v\n",
                p.weapon, vr_drpt_lg, vr_drpt_lf, vr_drpt_rot, vr_drpt_center));
            vr_drpt_posed = TRUE;
            VR_DRPT_Apply(p);
        }
        vr_agt_grunt = c == 15 ? VR_DRPT_Knight(60) : VR_AGT_Spawn(c >= 13 ? VR_DRPT_LineWid(c) : WID_SWORD, 60);
        vr_drpt_lk = 0;
        vr_agt_step = 2;
        return;
    }
    if(vr_drpt_posed)
    {
        VR_DRPT_Apply(p);
    }
    if(vr_agt_step == 2)
    {
        // Its start done, its weapon placed and drawn: held, then its swing forced at you.
        if(!(g.takedamage && g.vr_ag_prop && g.vr_ag_prop.vr_ag_was_n >= 1 && weapondrawnpose(g.vr_ag_prop, 0, 31) != '0 0 0'))
        {
            if(time > vr_drpt_until)
            {
                VR_AGT_Check(sprintf("dirparry %s: timed out (takedamage %g, weapon %g)", VR_DRPT_Name(c), g.takedamage,
                    g.vr_ag_prop != world), FALSE);
                vr_agt_step = 4;
            }
            return;
        }
        VR_AGST_Hold(g, 0);
        p.health = 1000;
        p.armorvalue = 0;
        if(c >= 11)
        {
            p.vr_stamina_used = 0; // (rested: the cases before spent none of this one's; 0-10 as they were)
        }
        if(c >= 13 && vr_drpt_lk > 0)
        {
            // (a line case's next swing: its counts go on)
            entity was2 = self;
            self = g;
            g.enemy = p;
            VR_AGP_Begin(vr_drpt_lk >= 4 ? VR_AGP_SAW : VR_AGP_SWING);
            g.vr_ag_act_kind = min(3, vr_drpt_lk);
            g.vr_ag_act_hand = 0;
            g.vr_ag_act_var = 0.5;
            self = was2;
            vr_drpt_t0 = time;
            vr_agt_step = 3;
            return;
        }
        vr_drpt_ev0 = vr_drp_evals;
        vr_drpt_pr0 = vr_drp_parries;
        vr_drpt_fx0 = vr_drp_fx_n;
        vr_drpt_dbg0 = vr_drp_dbg_n;
        vr_drpt_dbgf0 = vr_drp_dbg_final;
        vr_drpt_ln0 = vr_drp_line_n;
        vr_drp_line_worst = 0;
        vr_drpt_shot = FALSE;
        vr_drpt_hurt_at = 0;
        vr_drpt_hurt_by = world;
        vr_drpt_swung = 0;
        vr_drpt_len = 0;
        vr_drp_hand = -1;
        entity was = self;
        self = g;
        g.enemy = p;
        VR_AGP_Begin(VR_AGP_SWING);
        g.vr_ag_act_kind = VR_DRPT_Kind(c);
        g.vr_ag_act_hand = 0;
        g.vr_ag_act_var = 0.5; // (its swing's variation fixed: a random one moved case 5's blade from run to run)
        self = was;
        vr_drpt_t0 = time;
        vr_drpt_refd = FALSE;
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        // The reference swings: its blade as drawn three quarters into its strike (coming onto you, past its blow).
        VR_AGP_Times(g);
        if(c < 2 && !vr_drpt_refd && time - vr_drpt_t0 >= vr_agp_tw + vr_agp_ts * 0.75)
        {
            vr_drpt_refd = TRUE;
            VR_AG_WeaponLine(g.vr_ag_prop);
            if(c == 0)
            {
                vr_drpt_r0a = vr_ag_wa;
                vr_drpt_r0b = vr_ag_wb;
            }
            else
            {
                vr_drpt_r2a = vr_ag_wa;
                vr_drpt_r2b = vr_ag_wb;
            }
        }
        if(!vr_drpt_hurt_at && p.health < 1000)
        {
            vr_drpt_hurt_by = p.dmg_inflictor;
            vr_drpt_hurt_at = time - vr_drpt_t0;
        }
        if(c >= 13 && !vr_drpt_shot && time - vr_drpt_t0 >= vr_agp_tw + vr_agp_ts * 0.5)
        {
            vr_drpt_shot = TRUE;
            localcmd("vr_dirparry_linecheck dump\n"); // (its line and the drawn weapon's ends in its own frame, printed)
            if(cvar_hget(cvarh_scratch2) > 0)
            {
                // (scratch2 1: the eyes close to its weapon, both sides, then their image)
                localcmd("vr_dirparry_linecheck look\nwait\nwait\nvr_eyeshot 3\n");
                VR_AGT_Print(sprintf("dirparryshot: case %g's line mid-strike, close", c));
            }
            else if(c == 13)
            {
                cvar_set("vr_eyeshot", "3"); // (mid-strike: its line, last frame's and the drawn ends)
                VR_AGT_Print(sprintf("dirparryshot: case %g's line mid-strike", c));
            }
        }
        if(g.vr_ag_act == VR_AGP_SWING)
        {
            vr_drpt_swung = time - vr_drpt_t0;
            vr_drpt_len = VR_AGP_Length(g);
        }
        // Its swing's whole length (wind-up, strike, recovery) gone by: judged.
        if(time - vr_drpt_t0 < VR_AGP_Length(g) + 0.05)
        {
            return;
        }
        if(c >= 13 && vr_drpt_lk < (VR_DRPT_LineWid(c) == WID_CHAINSAW ? 4 : 3))
        {
            vr_drpt_lk = vr_drpt_lk + 1; // (a line case: its next swing kind, then the chainsaw's thrust)
            vr_drpt_until = time + 5;
            vr_agt_step = 2;
            return;
        }
        VR_DRPT_Judge(g, c);
        vr_agt_step = 4;
        return;
    }
    // (step 4) the next case
    if(g)
    {
        VR_AGT_Remove(g);
    }
    vr_agt_grunt = world;
    vr_drpt_posed = FALSE;
    vr_drpt_case = vr_drpt_case + 1;
    vr_agt_step = 0;
};
#endif

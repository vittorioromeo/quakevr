//
//
//
// ----------------------------------------------------------------------------
// Directional parry: the parry of every blow that has a line (ROUND23.md, "Directional parry" and "Directional parry: the
// only parry"). His words (2026-10-31): parrying an
// armed humanoid's melee weapon by "the direction of the enemy weapon and strike": overhead strikes parried by holding
// your weapon horizontally against them, horizontal strikes by holding it vertically; no poses written in: the parrying
// weapon square to the enemy weapon during the strike, physically meeting it; sparks where the two weapons collide.
// (2026-11-03: the only parry; "are we able to reasonably estimate the melee blow direction from their baked melee
// animations?": yes, below.)
//
// - Whose blows. An armed humanoid's melee-weapon swing, and any other monster's blow its frames give a line for
//   (VR_DRP_Baked: vr_blowline.cpp: a knight's or a hell knight's sword, an ogre's chainsaw, a fiend's and a shambler's
//   arms, a mod's monster alike). Those with none are read by the guard (a weapon held across in front, combat.qc
//   VR_Parry_Blocks, the bash's stance too), or crossed arms: a blow with no blade's way to be square to: a leap landing on
//   you (the spawn's, AD's), a fish's bite, an armed humanoid's punch, butt, shove or chainsaw thrust, the training dummy's.
// - A lunge (his words, 2026-11-03: "Dog lunges and fiend lunges should count as "vertical lines""; VR_DRP_Lunge: runs of
//   frames in quakevr/data/monsters.txt: a dog's bite and leap, a fiend's leap): the whole of it comes at you, so its line
//   is vertical, between it and you, at your weapon; its way towards you: a weapon level across parries, read at the blow.
// - Well timed (VR_DRP_BecameValid): your weapon became a valid parry shortly before the contact, moved in or turned to the
//   angle: within vr_dirparry_timed_window s, your line as it was then failed against its line at the contact.
// - What a parry lets through: vr_dirparry_through_armed, _monster (0: nothing).
// - Baked: the line is read as drawn at any moment of the stroke, ahead too (its frames are known). A blow is dealt as its
//   frame starts, the frame before still drawn, up to vr_dirparry_stroke s before its blade gets there; so with your
//   weapon near its way the blow is held back (a follower entity: VR_DRP_HeldThink) until it meets your weapon or that
//   long is over. Its stagger stays the parry's own (the parry balance). One attack, one parry: the rest of its blows go on
//   as the parry's cooldown has them. A blow from further than its limb is drawn reaching (Quake's melee range is longer) reads as coming on
//   as far as your weapon, up to VR_DRP_STRETCH.
// - Its side: while it swings (VR_AGP_SWING, a melee weapon; not the chainsaw's thrust), every frame its blade's line as
//   drawn (VR_AG_WeaponLine: the engine's hitreact::drawnWeapon, held::drawnExtent) and last frame's give its axis and
//   its way (the sweep: the velocity of its points, square to the blade).
// - Yours: each hand's weapon's parry line, from the hand to its far end (VR_Parry_WeaponLine: VR_Melee_Line's, the
//   weapon's own shape: a sword's blade, the axe's handle and head, a gun's barrel to its muzzle). Fists: none (the
//   crossed arms' parry stays as it was).
// - They meet: its blade swept from last frame's line to this frame's (and, at its blow, on along its way for
//   vr_dirparry_window s) comes within vr_dirparry_contact of your line. Your line is best square to both its blade and
//   its way (the normal of the plane it sweeps): overhead (the blade ahead, going down) a weapon held level across,
//   a side swing (going sideways) one held upright. Its angle off that: full marks within vr_dirparry_angle, none at 90.
//   Score: that times the meeting's (1 within the contact distance, falling to 0 at twice it); vr_dirparry_threshold or
//   more: parried.
// - Parried: no damage; sparks at the meeting (the middle of the two lines' nearest points) and the clang; the parry's
//   own sound, haptics, hand knock, stamina, counter window and stagger (VR_Parry); its swing stopped (no blow from it;
//   a flinch of vr_dirparry_opening s if the parry balance gave none: an armed humanoid's). Under the threshold: the blow lands, cut by
//   vr_dirparry_partial x score / threshold (0: off).
// - Met before its blow (VR_DRP_Frame, every frame of the swing's strike): parried there and then. At its blow
//   (VR_Parry): read on along its way (the window: where it would have met your weapon in the swing's rest).
// - Debug > Show Disarm Volumes: both lines, its way and the score (vr_disarmviz.cpp kinds 12-14). Show Directional
//   Parry (vr_dirparry_debug, VR_DRP_Debug: vr_dirparryviz.cpp): each reading's lines, the ideal and its numbers, the
//   last strike's verdict held a moment.
// - The leniencies (tuning constants: Debug > Tunables > Combat > Directional Parry): vr_dirparry_angle (full marks within), _angle_zero (none
//   at), _contact, _falloff (the meeting's marks none at this times the contact), _threshold, _window (divided by the
//   swing's speed: vr_agrunt_melee_speed, VR_AGP_Speed).
// ----------------------------------------------------------------------------

float VR_DV_DIR_YOURS = 12; // Show Disarm Volumes' kinds (vr_disarmviz.cpp Kind: keep in step)
float VR_DV_DIR_BLADE = 13;
float VR_DV_DIR_WAY = 14;
.float vr_drp_pend;      // (an armed humanoid) its blow held back: its damage (0: none), until when
.float vr_drp_pend_until;
float VR_DRP_NEAR = 48; // units: your weapon this near its blade's way at its blow holds the blow back
float vr_drp_hand;      // the last reading's: the hand whose weapon met it best (-1: none)
float vr_drp_score;     // ... its score (0..1)
float vr_drp_gap;       // ... the two lines' least distance on its way (units)
float vr_drp_off;       // ... your line's angle off square to its blade and its way (degrees)
float vr_drp_tau;       // ... when on its way (s from its line now; negative: since last frame)
vector vr_drp_at;       // ... where they met (the middle of their nearest points)
vector vr_drp_pa;       // ... your weapon's parry line (VR_DRP_PlayerSeg's out)
vector vr_drp_pb;
vector vr_drp_ea;       // ... its blade's line then
vector vr_drp_eb;
vector vr_drp_sweep;    // ... its way there (unit)
float vr_drp_evals;     // readings (the tests)
float vr_drp_parries;   // directional parries (the tests)
float vr_drp_early;     // ... of them met before its blow
float vr_drp_fx_n;      // sparks thrown (the tests)
vector vr_drp_fx_at;    // ... where the last were
vector vr_drp_na;       // the last reading's blade line now (the tests' reference swings)
vector vr_drp_nb;
vector vr_drp_sq;       // the last reading's ideal: square to its blade and its way (unit; either way)
vector vr_drp_lims;     // ... its limits: the angle's full marks within, none at (degrees); the contact distance
float vr_drp_limz;      // ... the meeting's marks none at (units)
float vr_drp_amark;     // ... the angle's marks (0..1)
float vr_drp_cmark;     // ... the meeting's marks (0..1)
vector vr_drp_ne;       // ... the two lines' nearest points: on its blade, on yours
vector vr_drp_np;
vector vr_drp_s0a;      // ... its blade where its way's span begins (last frame) and ends (its window's end)
vector vr_drp_s0b;
vector vr_drp_s1a;
vector vr_drp_s1b;
float vr_drp_dbg_n;     // Show Directional Parry's readings sent (the tests)
float vr_drp_dbg_final; // ... of them a strike's verdict
float vr_drp_back;      // VR_DRP_Eval, a baked blow: how far back on its way to read (s)
float VR_DRP_STRETCH = 40;   // units: at most this much further on than its limb is drawn reaching
float VR_DRP_SOON = 0.02;    // s: a baked blow met further ahead than this waits for its blade to get there
float VR_DRP_LOOKBACK = 0.1; // s: at its blow, a baked blow's way read this far back (a blow dealt after its stroke)

// Your hand `h`'s weapon's parry line into vr_drp_pa .. vr_drp_pb: the hand to its far end (FALSE: an empty hand, a fist).
float(entity pl, float h) VR_DRP_PlayerSeg =
{
    entity was = self;
    self = pl; // (VR_Melee_Line's helpers read self: VR_Parry_HandFor)
    float ok = VR_Parry_WeaponLine(pl, h);
    vector grip = vr_mline_grip;
    self = was;
    if(!ok)
    {
        return FALSE;
    }
    vr_drp_pa = grip;
    vr_drp_pb = vr_parry_end;
    if(vlen(vr_drp_pb - vr_drp_pa) < 8)
    {
        vr_drp_pa = vr_parry_butt; // (held near its far end: all of it)
    }
    return TRUE;
};

// Whether a monster's blow now is a lunge (the top; its frame in a run of its block in quakevr/data/monsters.txt).
float(entity g) VR_DRP_Lunge =
{
    if(g.vr_ag || g.vr_eshove_striking || g.vr_dummy_striking || vr_parry_test_force || cvar_hget(cvarh_vr_parry_test_guard))
    {
        return FALSE;
    }
    return blowlunge(g);
};

// A lunge's line, for your weapon `a` .. `b`: vertical, from the lower of your feet and its to the higher of your head and its
// top, where the way from it to you comes nearest your weapon. Into vr_drp_la, vr_drp_lb.
vector vr_drp_la;
vector vr_drp_lb;
void(entity g, entity pl, vector a, vector b) VR_DRP_LungeLine =
{
    vector from = g.origin;
    vector to = pl.origin;
    from_z = to_z = a_z = b_z = 0;
    VR_AG_SegDist(from, to, a, b);
    vector at = from + (to - from) * vr_ag_seg_s;
    vector head = VRGetHeadPos(pl);
    vr_drp_la = [at_x, at_y, min(g.absmin_z, pl.absmin_z)];
    vr_drp_lb = [at_x, at_y, max(g.absmax_z, head_z)];
};

// How many lines a monster's own blow has now (vr_blowline.cpp; 0: read by the guard): one on its feet (or flying,
// swimming): not a leap landing on you, no stroke but the flight. vr_parry_test_guard, a test's forced parry: none.
float(entity g) VR_DRP_Baked =
{
    if((!(g.flags & FL_ONGROUND) && !(g.flags & (FL_FLY | FL_SWIM))) || g.vr_eshove_striking || g.vr_dummy_striking ||
       vr_parry_test_force || cvar_hget(cvarh_vr_parry_test_guard))
    {
        return 0;
    }
    return blowline(g, 0, 0);
};

// Whether `g`'s blow now is judged by its direction: an armed humanoid swinging a melee weapon, a monster's own blow with
// a line.
float(entity g) VR_DRP_Applies =
{
    if(!g.vr_ag)
    {
        return VR_DRP_Lunge(g) || VR_DRP_Baked(g) > 0;
    }
    if(g.vr_ag_act != VR_AGP_SWING || g.vr_eshove_striking)
    {
        return FALSE;
    }
    entity w = vr_drp_w ? vr_drp_w : g.vr_ag_prop;
    return w && VR_AG_Class(w.weapon) == VR_AG_CLASS_MELEE && w.weapon != WID_CHAINSAW;
};

// This swing of `g`'s was stopped by a directional parry already (a monster's own: this attack of its was parried).
float(entity g) VR_DRP_Done =
{
    if(g.vr_drp_swing <= 0)
    {
        return FALSE;
    }
    if(g.vr_ag)
    {
        return g.vr_drp_swing == g.vr_ag_act_at;
    }
    return g.vr_melee_begun > 0 ? g.vr_drp_swing == g.vr_melee_begun : time - g.vr_drp_swing < 0.5;
};

// A baked blow's line (vr_blowline.cpp's, just read) reads this much further on, towards your weapon's middle `m`: from
// further than its limb is drawn reaching, as far as your weapon.
vector(entity g, vector m) VR_DRP_Stretch =
{
    float y = g.angles_y * 0.0174533;
    vector fw = [cos(y), sin(y), 0];
    return fw * min(VR_DRP_STRETCH, max(0, (m - g.origin) * fw - vr_blow_more_x));
};

// The score of your line `px` (unit) `gap` units from its line, `sq` the ideal: the angle's marks times the meeting's. Out:
// vr_drp_m_off (degrees off the ideal), vr_drp_m_angle, vr_drp_m_contact (the marks).
float vr_drp_m_off;
float vr_drp_m_angle;
float vr_drp_m_contact;
float(vector px, vector sq, float gap) VR_DRP_Marks =
{
    float contact = max(0.5, cvar_hget(cvarh_vr_dirparry_contact));
    float lenient = min(89, max(0, cvar_hget(cvarh_vr_dirparry_angle)));
    float zero = min(90, max(lenient + 1, cvar_hget(cvarh_vr_dirparry_angle_zero))); // (the angle's marks none)
    float fall = contact * (max(1, cvar_hget(cvarh_vr_dirparry_falloff)) - 1); // (the meeting's marks none this past it)
    vr_drp_m_off = atan2(vlen(VR_Cross(px, sq)), fabs(px * sq)) * 57.29578;
    vr_drp_m_angle = vr_drp_m_off <= lenient ? 1 : max(0, (zero - vr_drp_m_off) / max(1, zero - lenient));
    vr_drp_m_contact = gap <= contact ? 1 : fall <= 0 ? 0 : max(0, 1 - (gap - contact) / fall);
    return vr_drp_m_angle * vr_drp_m_contact;
};

// One reading: `g`'s weapon `w` swung at `pl`, its way followed on `ahead` s from its line now (a monster's own blow: `w` is
// `g`, its line vr_blowline.cpp's, from vr_drp_back s ago). Out: vr_drp_* (the best hand's). Its score.
float(entity g, entity w, entity pl, float ahead) VR_DRP_Eval =
{
    vr_drp_hand = -1;
    vr_drp_score = 0;
    vr_drp_gap = 9999;
    vr_drp_off = 90;
    vr_drp_tau = 0;
    vr_drp_evals = vr_drp_evals + 1;
    float baked = !g.vr_ag;
    float lunge = baked && VR_DRP_Lunge(g);
    float strikers = 1;
    vector a1 = '0 0 0';
    vector b1 = '0 0 0';
    float dt = 0;
    vector va = '0 0 0';
    vector vb = '0 0 0';
    if(lunge)
    {
        ahead = 0; // (read at the blow)
    }
    else if(baked)
    {
        strikers = VR_DRP_Baked(g);
        if(strikers > 0)
        {
            a1 = vr_blow_a;
            b1 = vr_blow_b;
        }
        dt = max(0, vr_drp_back);
    }
    else
    {
        VR_AG_WeaponLine(w);
        a1 = vr_ag_wa;
        b1 = vr_ag_wb;
        dt = time - w.vr_drp_t0;
        if(w.vr_drp_t0 > 0 && dt > 0.001 && dt < 0.1)
        {
            va = (a1 - w.vr_drp_a0) * (1 / dt);
            vb = (b1 - w.vr_drp_b0) * (1 / dt);
        }
        else
        {
            dt = 0;
        }
    }
    vr_drp_na = a1;
    vr_drp_nb = b1;
    float contact = max(0.5, cvar_hget(cvarh_vr_dirparry_contact));
    float lenient = min(89, max(0, cvar_hget(cvarh_vr_dirparry_angle)));
    float zero = min(90, max(lenient + 1, cvar_hget(cvarh_vr_dirparry_angle_zero))); // (the angle's marks none)
    float fall = contact * (max(1, cvar_hget(cvarh_vr_dirparry_falloff)) - 1); // (the meeting's marks none this past it)
    float span = dt + max(0, ahead);
    float n = 4 + 2 * ceil((baked ? span : max(0, ahead)) * 72); // (samples along its way: two a frame)
    vector keepA = '0 0 0';
    vector keepB = '0 0 0';
    for(float h = 0; h < 2; h++)
    {
        if(!VR_DRP_PlayerSeg(pl, h))
        {
            continue;
        }
        vector pa = vr_drp_pa;
        vector pb = vr_drp_pb;
        for(float st = 0; st < strikers; st++)
        {
            vector on = '0 0 0'; // (a baked line's stretch)
            if(baked && !lunge)
            {
                blowline(g, 0, st);
                on = VR_DRP_Stretch(g, (pa + pb) * 0.5);
            }
            float best = 9999;
            float bt = 0;
            float bs = 0;
            float bp = 0;
            vector ea = '0 0 0';
            vector eb = '0 0 0';
            if(lunge)
            {
                VR_DRP_LungeLine(g, pl, pa, pb);
                ea = vr_drp_la;
                eb = vr_drp_lb;
                best = VR_AG_SegDist(ea, eb, pa, pb);
                bs = vr_ag_seg_s;
                bp = vr_ag_seg_t;
            }
            for(float k = 0; !lunge && k <= n; k++)
            {
                float tau = -dt + span * k / n;
                vector sa = a1 + va * tau;
                vector sb = b1 + vb * tau;
                if(baked)
                {
                    blowline(g, tau, st);
                    sa = vr_blow_a + on;
                    sb = vr_blow_b + on;
                }
                float d = VR_AG_SegDist(sa, sb, pa, pb);
                if(d < best)
                {
                    best = d;
                    bt = tau;
                    bs = vr_ag_seg_s;
                    bp = vr_ag_seg_t;
                    ea = sa;
                    eb = sb;
                }
                if(d <= contact)
                {
                    break; // (the first meeting on its way)
                }
            }
            vector axis = normalize(eb - ea);
            // Its way: the turn of its blade over the last frame (the plane it sweeps, whatever its hand's own motion: a swing
            // is a turn, its blade going along its own length as the hand goes in adds nothing), else its point's motion
            // square to it.
            vector sw = '0 0 0';
            vector v = va + (vb - va) * bs;
            vector turn = '0 0 0';
            if(lunge)
            {
                v = pl.origin - g.origin; // (towards you)
                v_z = 0;
                v = normalize(v) * 1000;
            }
            else if(baked)
            {
                blowline(g, bt - 0.02, st);
                vector ba0 = vr_blow_a;
                vector bb0 = vr_blow_b;
                blowline(g, bt + 0.02, st);
                turn = normalize(vr_blow_b - vr_blow_a) - normalize(bb0 - ba0);
                v = (vr_blow_a + (vr_blow_b - vr_blow_a) * bs - ba0 - (bb0 - ba0) * bs) * 25;
            }
            else if(dt > 0)
            {
                turn = normalize(b1 - a1) - normalize(w.vr_drp_b0 - w.vr_drp_a0);
            }
            sw = turn - axis * (turn * axis);
            sw = vlen(sw) > 0.01 ? sw * 1000 : '0 0 0';
            if(sw == '0 0 0')
            {
                sw = v - axis * (v * axis);
            }
            if(vlen(sw) < 20)
            {
                sw = pl.origin - g.origin; // (its blade still: its way taken as towards you)
                sw = sw - axis * (sw * axis);
            }
            sw = normalize(sw);
            vector sq = normalize(VR_Cross(axis, sw)); // (square to its blade and its way)
            vector px = normalize(pb - pa);
            float score = VR_DRP_Marks(px, sq, best);
            float off = vr_drp_m_off;
            float angleScore = vr_drp_m_angle;
            float cs = vr_drp_m_contact;
            if(vr_drp_hand < 0 || score > vr_drp_score)
            {
                vr_drp_hand = h;
                vr_drp_score = score;
                vr_drp_gap = best;
                vr_drp_off = off;
                vr_drp_tau = bt;
                vr_drp_ea = ea;
                vr_drp_eb = eb;
                vr_drp_sweep = sw;
                vr_drp_ne = ea + (eb - ea) * bs;
                vr_drp_np = pa + (pb - pa) * bp;
                vr_drp_at = (vr_drp_ne + vr_drp_np) * 0.5;
                vr_drp_sq = sq;
                vr_drp_lims = [lenient, zero, contact];
                vr_drp_limz = contact + fall;
                vr_drp_amark = angleScore;
                vr_drp_cmark = cs;
                if(lunge)
                {
                    vr_drp_s0a = vr_drp_s1a = ea;
                    vr_drp_s0b = vr_drp_s1b = eb;
                }
                else if(baked)
                {
                    blowline(g, -dt, st);
                    vr_drp_s0a = vr_blow_a + on;
                    vr_drp_s0b = vr_blow_b + on;
                    blowline(g, span - dt, st);
                    vr_drp_s1a = vr_blow_a + on;
                    vr_drp_s1b = vr_blow_b + on;
                }
                else
                {
                    vr_drp_s0a = a1 - va * dt;
                    vr_drp_s0b = b1 - vb * dt;
                    vr_drp_s1a = a1 + va * (span - dt);
                    vr_drp_s1b = b1 + vb * (span - dt);
                }
                keepA = pa;
                keepB = pb;
            }
        }
    }
    if(vr_drp_hand >= 0)
    {
        vr_drp_pa = keepA;
        vr_drp_pb = keepB;
    }
    return vr_drp_score;
};

// The reading's lines on Show Disarm Volumes (vr_disarmviz.cpp: 12 your line, green parried, red not; 13 its blade then;
// 14 its way), its score by the meeting.
void(entity g, entity w, float parried) VR_DRP_Show =
{
    if(!vr_dv_on || vr_drp_hand < 0)
    {
        return;
    }
    debugdisarm(w, VR_DV_DIR_YOURS, vr_drp_pa, vr_drp_pb, parried ? 1 : 0);
    debugdisarm(w, VR_DV_DIR_BLADE, vr_drp_ea, vr_drp_eb, 0);
    debugdisarm(w, VR_DV_DIR_WAY, vr_drp_at, vr_drp_at + vr_drp_sweep * 12, 0);
    debugdisarmnote(w, 0, vr_drp_at + '0 0 4', sprintf("dirparry %.2f%s: %.0f deg off square, gap %.1f", vr_drp_score,
        parried ? " PARRIED" : "", vr_drp_off, vr_drp_gap));
};

float VR_DRP_SHOW_MISS = 0;    // Show Directional Parry's verdicts (vr_dirparryviz.cpp: keep in step): a strike's, held
float VR_DRP_SHOW_PARTIAL = 1; // ... a moment: the blow lands (cut by vr_dirparry_partial), or
float VR_DRP_SHOW_PARRIED = 2;
float VR_DRP_SHOW_LIVE = 10;   // ... + a reading during its strike (its verdict were it the blow's)
float VR_DRP_SHOW_HELD = 13;   // ... its blow held back for the window

// Show Directional Parry (vr_dirparry_debug; the engine's vr_dirparryviz.cpp): the last reading of `g`'s swing drawn,
// its blade then (and where its way's span began and ends), its way, your weapon's parry line, the ideal through it,
// the gap between them, and its numbers; `verdict` VR_DRP_SHOW_*. Nothing is sent while it is off.
void(entity g, float verdict) VR_DRP_Debug =
{
    if(!cvar_hget(cvarh_vr_dirparry_debug) || vr_drp_hand < 0)
    {
        return;
    }
    debugdirparry(1, vr_drp_ea, vr_drp_eb);
    debugdirparry(2, vr_drp_s0a, vr_drp_s0b);
    debugdirparry(3, vr_drp_s1a, vr_drp_s1b);
    debugdirparry(4, vr_drp_pa, vr_drp_pb);
    debugdirparry(5, vr_drp_at, vr_drp_at + vr_drp_sweep * 12);
    float half = max(6, vlen(vr_drp_pb - vr_drp_pa) * 0.5);
    debugdirparry(6, vr_drp_np - vr_drp_sq * half, vr_drp_np + vr_drp_sq * half);
    debugdirparry(7, vr_drp_ne, vr_drp_np);
    float shown = debugdirparrynote(verdict, vr_drp_at, [vr_drp_score, cvar_hget(cvarh_vr_dirparry_threshold), vr_drp_cut],
        [vr_drp_off, vr_drp_lims_x, vr_drp_lims_y], [vr_drp_gap, vr_drp_lims_z, vr_drp_limz],
        [vr_drp_amark, vr_drp_cmark, 0], [vr_drp_hand == cVR_MainHand ? 1 : 0, vr_drp_tau, VR_AGP_Speed(g)]);
    vr_drp_dbg_n = vr_drp_dbg_n + shown;
    if(verdict < VR_DRP_SHOW_LIVE)
    {
        vr_drp_dbg_final = vr_drp_dbg_final + shown;
    }
};

.vector vr_drp_dbg_a0;  // (a held melee weapon) its line last frame, as Show Directional Parry sent it
.vector vr_drp_dbg_b0;
.float vr_drp_dbg_t;    // ... when (once a frame)
float vr_drp_line_n;    // the frames whose line was measured against the drawn weapon (the tests)
float vr_drp_line_worst; // ... the worst gap between their ends (units)

// Show Directional Parry (vr_dirparry_debug), every frame `w` is held (VR_DRP_Frame): its line as the parry reads it
// (VR_AG_WeaponLine, both ends) and last frame's, drawn; its ends measured against the drawn weapon's own butt and tip.
void(entity w) VR_DRP_DebugLine =
{
    if(!cvar_hget(cvarh_vr_dirparry_debug) || w.vr_drp_dbg_t == time)
    {
        return;
    }
    VR_AG_WeaponLine(w);
    float fresh = w.vr_drp_dbg_t > 0 && time - w.vr_drp_dbg_t < 0.1;
    float gap = debugdirparryline(w, vr_ag_wa, vr_ag_wb, fresh ? w.vr_drp_dbg_a0 : vr_ag_wa,
        fresh ? w.vr_drp_dbg_b0 : vr_ag_wb);
    if(gap >= 0)
    {
        vr_drp_line_n = vr_drp_line_n + 1;
        vr_drp_line_worst = max(vr_drp_line_worst, gap);
    }
    w.vr_drp_dbg_a0 = vr_ag_wa;
    w.vr_drp_dbg_b0 = vr_ag_wb;
    w.vr_drp_dbg_t = time;
};

// VR_AGP_Pose, every frame `g` holds the melee weapon `w` (before its swing's pose and blow): its swing's strike met
// your weapon held across it: parried now (VR_Parry, forced), the swing stopped.
void(entity g, entity w) VR_DRP_Frame =
{
    if(!w || w.vr_drp_t0 == time)
    {
        return; // (once a frame)
    }
    VR_DRP_DebugLine(w);
    entity pl = g.enemy;
    float live = g.vr_ag_act == VR_AGP_SWING && w.weapon != WID_CHAINSAW && g.health > 0 &&
               pl && (pl.flags & FL_CLIENT) && pl.health > 0 && VRHandsTracked(pl);
    if(!live)
    {
        w.vr_drp_t0 = 0;
        g.vr_drp_pend = 0; // (its swing cut short: a held blow dropped)
        return;
    }
    VR_AGP_Times(g);
    float s = time - g.vr_ag_act_at;
    float left = w.vr_ag_left && g.vr_ag_prop2;
    float struck = left ? g.vr_ag_act_hitl : g.vr_ag_act_hit;
    float pending = g.vr_drp_pend > 0;
    float parried = FALSE;
    if((s >= vr_agp_tw) && (((s < vr_agp_tw + vr_agp_ts) && !struck) || pending) && !VR_DRP_Done(g) &&
       (vlen(pl.origin - g.origin) < 150))
    {
        VR_DRP_Eval(g, w, pl, 0);
        parried = ((vr_drp_hand >= 0) && (vr_drp_gap <= max(0.5, cvar_hget(cvarh_vr_dirparry_contact))) &&
                   (vr_drp_score >= cvar_hget(cvarh_vr_dirparry_threshold)));
        VR_DRP_Show(g, w, parried);
        if(!parried)
        {
            float thr = cvar_hget(cvarh_vr_dirparry_threshold);
            VR_DRP_Debug(g, pending ? VR_DRP_SHOW_HELD : VR_DRP_SHOW_LIVE + (vr_drp_score >= thr ? VR_DRP_SHOW_PARRIED :
                vr_drp_score > 0 ? VR_DRP_SHOW_PARTIAL : VR_DRP_SHOW_MISS));
        }
        if(parried)
        {
            vr_drp_forced = TRUE;
            vr_drp_w = w;
            float through = VR_Parry(pl, g, g, pending ? g.vr_drp_pend : 20); // (the parry's events; dealt: vr_dirparry_through_armed's)
            vr_drp_w = world;
            vr_drp_forced = FALSE;
            if(through > 0)
            {
                vr_dmg_dealonly = TRUE;
                T_Damage(pl, g, g, through);
            }
            vr_drp_early = vr_drp_early + 1;
            g.vr_ag_act_hit = 1; // (no blow from this swing)
            g.vr_ag_act_hitl = 1;
            g.vr_drp_pend = 0;
        }
    }
    if(pending && g.vr_drp_pend > 0 && time >= g.vr_drp_pend_until)
    {
        // Its blow held back met no weapon of yours across its way in the window: it lands now.
        float hurt = g.vr_drp_pend;
        g.vr_drp_pend = 0;
        vr_drp_release = TRUE;
        vr_drp_w = w;
        T_Damage(pl, g, g, hurt);
        vr_drp_w = world;
        vr_drp_release = FALSE;
    }
    VR_AG_WeaponLine(w);
    w.vr_drp_a0 = vr_ag_wa;
    w.vr_drp_b0 = vr_ag_wb;
    w.vr_drp_t0 = time;
};

// StartFrame's player think (VR_Parry_GuardFrame; `pl` self): each hand's weapon's parry line this frame, for its timing.
void(entity pl) VR_DRP_HandLines =
{
    vector keepA = vr_drp_pa; // (the last reading's)
    vector keepB = vr_drp_pb;
    for(float h = 0; h < 2; h++)
    {
        if(VR_DRP_PlayerSeg(pl, h))
        {
            handlineadd(pl, h, vr_drp_pa, vr_drp_pb);
        }
        else
        {
            handlineadd(pl, h, '0 0 0', '0 0 0');
        }
    }
    vr_drp_pa = keepA;
    vr_drp_pb = keepB;
};

// The parry just read by your hand `h` against `g`'s blow (vr_drp_*): well timed: your weapon became a valid parry shortly
// before now: as it was within vr_dirparry_timed_window s, it failed against this contact's line (too far from it, or at the
// wrong angle, or not in your hand). Held there all that time: not.
float(entity g, entity pl, float h) VR_DRP_BecameValid =
{
    float window = max(0, cvar_hget(cvarh_vr_dirparry_timed_window));
    float threshold = cvar_hget(cvarh_vr_dirparry_threshold);
    float lunge = !g.vr_ag && VR_DRP_Lunge(g);
    vector ea = vr_drp_ea;
    vector eb = vr_drp_eb;
    vector sq = vr_drp_sq;
    for(float back = 0; back < 128; back++)
    {
        float at = handlineat(pl, h, back);
        if(at < 0 || time - at > window)
        {
            return FALSE;
        }
        vector a = vr_blow_a;
        vector b = vr_blow_b;
        if(a == b)
        {
            return TRUE; // (no weapon in that hand then)
        }
        if(lunge)
        {
            VR_DRP_LungeLine(g, pl, a, b); // (a lunge's line is wherever its way meets your weapon)
            ea = vr_drp_la;
            eb = vr_drp_lb;
        }
        if(VR_DRP_Marks(normalize(b - a), sq, VR_AG_SegDist(ea, eb, a, b)) < threshold)
        {
            return TRUE;
        }
    }
    return FALSE;
};

// A monster's own blow held back (VR_DRP_Blow; self a follower, its owner the monster, its enemy you): each frame its line
// followed as drawn: met across your weapon, parried now; that long over, it lands.
void() VR_DRP_HeldThink =
{
    entity f = self;
    entity g = f.owner;
    entity pl = f.enemy;
    float keep = FALSE;
    if(g && g.modelindex && g.health > 0 && (g.flags & FL_MONSTER) && g.vr_drp_pend > 0 && pl && (pl.flags & FL_CLIENT) && pl.health > 0 && // (modelindex: removed since: a freed entity keeps the rest)
       VRHandsTracked(pl) && g.vr_parry_stagger_until <= time)
    {
        self = g;
        float thr = cvar_hget(cvarh_vr_dirparry_threshold);
        vr_drp_back = max(0.001, frametime);
        VR_DRP_Eval(g, g, pl, 0);
        float parried = ((vr_drp_hand >= 0) && (vr_drp_gap <= max(0.5, cvar_hget(cvarh_vr_dirparry_contact))) && (vr_drp_score >= thr));
        VR_DRP_Show(g, g, parried);
        float hurt = g.vr_drp_pend;
        if(parried)
        {
            g.vr_drp_pend = 0;
            vr_drp_forced = TRUE;
            float rest = VR_Parry(pl, g, g, hurt);
            vr_drp_forced = FALSE;
            vr_drp_early = vr_drp_early + 1;
            if(rest > 0)
            {
                vr_dmg_dealonly = TRUE;
                T_Damage(pl, g, g, rest); // (what a parry lets through: vr_parry_reduction)
            }
        }
        else if(time >= g.vr_drp_pend_until)
        {
            g.vr_drp_pend = 0;
            vr_drp_release = TRUE;
            T_Damage(pl, g, g, hurt);
            vr_drp_release = FALSE;
        }
        else
        {
            VR_DRP_Debug(g, VR_DRP_SHOW_HELD);
            keep = TRUE;
        }
        self = f;
    }
    else if(g)
    {
        g.vr_drp_pend = 0; // (dead, staggered, you gone: a held blow dropped)
    }
    if(keep)
    {
        f.nextthink = time + 0.001; // (the next server frame)
    }
    else
    {
        remove(f);
    }
};

// VR_Parry, `g`'s blow on `pl` (VR_DRP_Applies): the hand whose weapon parried it (-1: none; vr_drp_cut a partial
// parry's share), vr_parry_at the meeting. Met before its blow (VR_DRP_Frame, VR_DRP_HeldThink): that reading's.
float(entity pl, entity g, float damage) VR_DRP_Blow =
{
    float threshold = cvar_hget(cvarh_vr_dirparry_threshold);
    float baked = !g.vr_ag;
    float window = (baked && VR_DRP_Lunge(g)) ? 0 : baked ? max(0, cvar_hget(cvarh_vr_dirparry_stroke)) :
                   max(0, cvar_hget(cvarh_vr_dirparry_window)) / VR_AGP_Speed(g); // (a slowed swing's: as long a way)
    vr_drp_deferred = FALSE;
    vr_drp_cut = 0;
    if(!vr_drp_forced)
    {
        if(baked && g.vr_drp_pend > 0 && !vr_drp_release)
        {
            // (its stroke's next blow, the first still held: all of them dealt or parried together)
            g.vr_drp_pend = g.vr_drp_pend + damage;
            vr_drp_deferred = TRUE;
            return -1;
        }
        entity w = baked ? g : (vr_drp_w ? vr_drp_w : g.vr_ag_prop);
        vr_drp_back = VR_DRP_LOOKBACK;
        VR_DRP_Eval(g, w, pl, (baked && vr_drp_release) ? 0 : window);
        VR_DRP_Show(g, w, vr_drp_score >= threshold);
        float met = ((vr_drp_gap <= max(0.5, cvar_hget(cvarh_vr_dirparry_contact))) && (vr_drp_score >= threshold));
        // Your weapon near its way but not met yet (its blow is dealt before its blade gets there): the blow held back
        // for the window, its blade followed each frame (VR_DRP_Frame, VR_DRP_HeldThink): met across, parried; else it
        // lands then. (A baked blow's meeting ahead is waited for too: its sparks when its blade gets there.)
        if(!vr_drp_release && window > 0 && vr_drp_hand >= 0 && vr_drp_gap < VR_DRP_NEAR &&
           (!met || (baked && vr_drp_tau > VR_DRP_SOON)))
        {
            g.vr_drp_pend = max(g.vr_drp_pend, damage);
            g.vr_drp_pend_until = time + window;
            vr_drp_deferred = TRUE;
            if(baked)
            {
                entity f = spawn();
                f.classname = "vr_drp_held";
                f.owner = g;
                f.enemy = pl;
                f.think = VR_DRP_HeldThink;
                f.nextthink = time + 0.001;
            }
            dprint(sprintf("dirparry: %s's blow held back %.2f s: your weapon %.1f units from its way\n", g.classname,
                window, vr_drp_gap));
            VR_DRP_Debug(g, VR_DRP_SHOW_HELD);
            return -1;
        }
    }
    if(vr_drp_hand < 0 || vr_drp_score < threshold || vr_drp_score <= 0)
    {
        if(vr_drp_score > 0 && threshold > 0)
        {
            vr_drp_cut = max(0, cvar_hget(cvarh_vr_dirparry_partial)) * min(1, vr_drp_score / threshold);
        }
        dprint(sprintf("dirparry: %s's blow lands: score %.2f (%.0f deg off square, gap %.1f units), cut %.2f\n",
            g.classname, vr_drp_score, vr_drp_off, vr_drp_gap, vr_drp_cut));
        VR_DRP_Debug(g, vr_drp_score > 0 ? VR_DRP_SHOW_PARTIAL : VR_DRP_SHOW_MISS);
        return -1;
    }
    g.vr_drp_swing = !baked ? g.vr_ag_act_at : g.vr_melee_begun > 0 ? g.vr_melee_begun : time;
    g.vr_drp_by = vr_drp_hand;
    g.vr_drp_timed = vr_drp_ag_timed = VR_DRP_BecameValid(g, pl, vr_drp_hand);
    vr_drp_parries = vr_drp_parries + 1;
    vr_parry_at = vr_drp_at;
    dprint(sprintf("dirparry: %s's blow parried by hand %g: score %.2f (%.0f deg off square, gap %.1f units, %.3f s %s)\n",
        g.classname, vr_drp_hand, vr_drp_score, vr_drp_off, vr_drp_gap, fabs(vr_drp_tau),
        vr_drp_tau > 0 ? "ahead" : "ago"));
    if(vr_dv_on)
    {
        VR_DV_Mark(baked ? g : (vr_drp_w ? vr_drp_w : g.vr_ag_prop), vr_drp_at, VR_DV_PARRY, sprintf("dirparry %.2f", vr_drp_score));
    }
    VR_DRP_Debug(g, VR_DRP_SHOW_PARRIED);
    return vr_drp_hand;
};

// VR_Parry's directional parry by your hand `h`: sparks where the two weapons met (vr_parry_at), off both, and their
// sound: the weapon clash's table for what met what (vr_ownclash.qc VR_Clash_Row: your blade on its blade sparks, the
// clang and a tink; your gun fewer sparks and the clank).
void(entity pl, entity g, float h) VR_DRP_Effects =
{
    vector x = vr_parry_at;
    vector way = vr_drp_sweep != '0 0 0' ? vr_drp_sweep : normalize(pl.origin - g.origin);
    entity w = vr_drp_w ? vr_drp_w : g.vr_ag ? g.vr_ag_prop : world; // (a monster's own limb: as a blade)
    VR_Clash_Fx(g, VR_Clash_MatOf(VR_Melee_Thing(pl, h)), w ? VR_Clash_MatOfWid(w.weapon) : VR_CLASH_BLADE, x,
        way * -1 + '0 0 0.5', 60, 0.9, 1);
    vr_drp_fx_n = vr_drp_fx_n + 1;
    vr_drp_fx_at = x;
};

// Show Disarm Volumes (StartFrame's player think): each monster near you whose frame now has a line: that line as drawn
// (white; a second limb's yellow).
void(entity pl) VR_DRP_ShowLines =
{
    for(entity e = findradius(pl.origin, 300); e; e = e.chain)
    {
        if((e.flags & FL_MONSTER) && !e.vr_ag && e.health > 0)
        {
            float n = blowline(e, 0, 0);
            for(float st = 0; st < n; st++)
            {
                blowline(e, 0, st);
                debugdisarm(e, st == 0 ? VR_DV_DIR_BLADE : VR_DV_DIR_WAY, vr_blow_a, vr_blow_b, 0); // (the second limb's: its own colour)
            }
        }
    }
};

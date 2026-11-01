#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_disarmhull_test.qc: test 16 (vr_agrunt_test 16), the disarm volumes as hulls of the drawn triangles (ROUND23.md,
// "Disarm volumes as hulls of the drawn triangles"; his notes 2026-10-31 19:36 and 19:38: an axe's and a knight's sword's
// volumes twice the weapon or a near-sphere round half the body, the line upright across a level blade, the hand's
// volume above the drawn hand). For every kind (grunt, enforcer, knight, death knight) and weapon (axe, sword, mjolnir,
// chainsaw, shotgun, rocket launcher, nail pistol), frozen in its Aim (the sword held level), 60 ahead of you, once its
// weapon is drawn and still:
// - the weapon's hull (disarmhull 10) holds every vertex the renderer draws it with (none over 0.05 units out), its
//   box within 3% of theirs; the hand's (11) holds the drawn hand and forearm's vertices;
// - its line (VR_AG_WeaponLine: the parry's, a strike's way) along the hull's long axis (within 3 degrees), as long;
// - a shot (VR_AGS_MeetOne, the volumes as they count) across its blade's far half counts on the weapon, one 2 units off
//   its hull does not (the old capsule, its model's box's x and its larger side's half, took it);
// - a shot across its drawn forearm counts (its weapon or its hand);
// - a body blow (VR_AG_Hit) on its drawn forearm disarms it; one 3 units off its hand's and weapon's hulls does not (the
//   old 10-unit sphere round its grip took it).
// The sword is the Knight's Sword (WID_SWORD, flags 0; his second view), the death knight's row among them. Then a grunt
// dual wielding two nail pistols: the same for the right one, then (a fresh grunt) the left one, its left hand's hull.
// Ends on `agtest: test 16 over: ...` (run.sh -QuitOn on it); each wait is the weapon drawn and still (or its give-up,
// a FAIL).
// ----------------------------------------------------------------------------

float vr_dht_k;      // the kind under test (0..3)
float vr_dht_w;      // the weapon under test (0..VR_DHT_WEAPONS - 1)
float vr_dht_t0;     // the state's start (its give-up's clock)
float vr_dht_still;  // frames its drawn grip moved under 0.05
vector vr_dht_last;  // ... its last place
float vr_dht_kind;   // vr_agrunt_test_kind as it was
float vr_dht_disarm; // vr_agrunt_disarm as it was
float VR_DHT_WEAPONS = 7;
float vr_dht_debug;  // vr_debug_disarm as it was (the hunt's phase)
float vr_dht_n;      // ... frames measured
float vr_dht_bad;    // ... of them its drawn hand hull not from the drawn rig while drawn
float vr_dht_srv;    // ... the server's query not from the drawn rig
float vr_dht_gmax;   // ... the drawn grip's most gap to it
float vr_dht_gsum;   // ... their sum

float(float w) VR_DHT_Wid =
{
    if(w == 0) { return WID_AXE; }
    if(w == 1) { return WID_SWORD; }
    if(w == 2) { return WID_MJOLNIR; }
    if(w == 3) { return WID_CHAINSAW; }
    if(w == 4) { return WID_SHOTGUN; }
    if(w == 5) { return WID_ROCKET_LAUNCHER; }
    return WID_NAILPISTOL;
};

string(float w) VR_DHT_WName =
{
    if(w == 0) { return "axe"; }
    if(w == 1) { return "sword"; }
    if(w == 2) { return "mjolnir"; }
    if(w == 3) { return "chainsaw"; }
    if(w == 4) { return "shotgun"; }
    if(w == 5) { return "rocket launcher"; }
    return "nail pistol";
};

// Whether the shot a..b counts on `g`'s weapon `p` or its hand (the volumes as they count: VR_AGS_MeetOne, no near
// misses); vr_ags_mpart its part.
float(entity g, entity p, vector a, vector b) VR_DHT_Shot =
{
    vr_ags_mp = world;
    vr_ags_ms = 2;
    vr_ags_exact = TRUE; // (the hulls themselves: no grow, no near misses)
    VR_AGS_MeetOne(g, p, a, b);
    vr_ags_exact = FALSE;
    return vr_ags_mp == p;
};

// A body blow by you on `g` at `at` as T_DamageDeal hands it to VR_AG_Hit (test 79's): whether its weapon `p` is kept
// (in either hand).
float(entity g, entity p, vector at) VR_DHT_Blow =
{
    entity pl = vr_agt_player;
    VR_Hit_Begin(QVR_HITKIND_MELEE, cVR_MainHand, '0 0 0', '0 0 0');
    hit_blast = 0;
    VR_Hit_SetZone(g, 0, at);
    VR_AG_Hit(g, pl, pl, 1000);
    VR_Hit_End();
    return (g.vr_ag_prop == p || g.vr_ag_prop2 == p) && p != world;
};

// The path across `p`'s hull at `c`, along `w`, `s` units out along `u`: its gap to the hull (disarmhull 0).
float(entity p, vector c, vector u, vector w, float s) VR_DHT_Gap =
{
    vector v = disarmhull(p, 0, c + u * s - w * 30, c + u * s + w * 30, 0);
    return v_x;
};

// `g`'s weapon `p` in its hand on `side` (0 its main weapon's, 1 the left one's: dual), named `wn`.
void(entity g, entity p, float side, string wn) VR_DHT_Measure =
{
    // The hulls hold what is drawn.
    vector m = disarmhull(p, 10, '0 0 0', '0 0 0', 0);
    VR_AGT_Check(sprintf("%s: its weapon's hull holds its drawn vertices (most %.3f out), its box %.3f of theirs, slack %.2f",
        wn, m_x, m_y, m_z), m_z >= 0 && m_x <= 0.05 && m_y >= 0.97 && m_y <= 1.03);
    // Its hand's volume: two hulls, its drawn hand and its drawn forearm, each holding its own vertices, neither its
    // upper arm (his note 2026-11-01 18:02: the one hull took the biceps).
    vector mh = disarmhull(g, 11 + side, '0 0 0', '0 0 0', 0);
    VR_AGT_Check(sprintf("%s: its hand's hulls hold its drawn hand (%g vertices) and forearm (%g), most %.3f out", wn, mh_y,
        mh_z, mh_x), mh_y > 0 && mh_z > 0 && mh_x <= 0.05);
    vector up = disarmhull(g, 17 + side, '0 0 0', '0 0 0', 0);
    VR_AGT_Check(sprintf("%s: its hand's hulls hold none of its upper arm (%g of its top's vertices in them; the old hull's "
        "region had %g of %g off the hand and forearm)", wn, up_x, up_y, up_z), up_x == 0);
    // Its line along the hull's long axis.
    vector ax = disarmhull(p, 13, '0 0 0', '0 0 0', 0);
    vector ext = disarmhull(p, 14, '0 0 0', '0 0 0', 0);
    VR_AG_WeaponLine(p);
    vector ld = vr_ag_wb - vr_ag_wa;
    float ang = vlen(VR_Cross(normalize(ld), ax)) * 57.2958; // (degrees: sin, near enough under 10)
    float size = cvar("vr_agrunt_shotdisarm_size");
    VR_AGT_Print(sprintf("%s: the hull %.1f long, %.1f off its axis; the line %.1f long, %.1f round (x%g)", wn, ext_x, ext_y,
        vlen(ld), vr_ag_wr, size));
    // (Its line: the model's own longest axis when that is 0.85 of the hull's longest span or more (a gun's barrel), else
    // that span's way (the Knight's Sword's blade: wanted on it); as long as the hull is along it.)
    vector ga = disarmhull(p, 0, vr_ag_wa, vr_ag_wa, 0);
    vector gb = disarmhull(p, 0, vr_ag_wb, vr_ag_wb, 0);
    vector gl = disarmhull(p, 0, vr_ag_wa, vr_ag_wb, 0);
    VR_AGT_Check(sprintf("%s: its line %.1f degrees off the hull's longest span, %.1f long against %.1f, through it (%.2f), "
        "its ends %.2f and %.2f off it", wn, ang, vlen(ld), ext_x, gl_x, ga_x, gb_x),
        vlen(ld) >= 0.85 * ext_x && vlen(ld) <= 1.02 * ext_x && gl_x <= 0 && ga_x <= 3 && gb_x <= 3);
    // A shot across its far half counts on the weapon; one 2 units off its hull does not.
    vector c = (vr_ag_wa + vr_ag_wb) * 0.5 + ax * (ext_x * 0.25);
    vector to = vr_agt_player.origin + vr_agt_player.view_ofs - c;
    vector u = normalize(to - ax * (to * ax));
    vector w = normalize(VR_Cross(ax, u));
    float s = 20;
    float g2 = VR_DHT_Gap(p, c, w, u, s);
    float k;
    for(k = 0; k < 4; k++)
    {
        // (in till its gap is 2: the nearest point may move as it comes in)
        s = s - (g2 - 2);
        g2 = VR_DHT_Gap(p, c, w, u, s);
    }
    float through = VR_DHT_Shot(g, p, c - u * 30, c + u * 30);
    float part = vr_ags_mpart;
    vector gc = disarmhull(p, 0, c - u * 30, c + u * 30, 0);
    VR_AGT_Check(sprintf("%s: a shot across its far half meets its hull (gap %.2f) and counts (part %g: 2 its hand first)",
        wn, gc_x, part), through && gc_x <= 0);
    float off = VR_DHT_Shot(g, p, c + w * s - u * 30, c + w * s + u * 30);
    VR_AGT_Check(sprintf("%s: a shot %.2f units off its hull does not count (part %g)", wn, g2, off ? vr_ags_mpart : 0),
        !off && g2 >= 1.8 && g2 <= 2.2);
    // A shot across its drawn forearm counts.
    vector f = weapondrawnpose(g, side, 37);
    through = VR_DHT_Shot(g, p, f + '0 0 30', f - '0 0 30');
    VR_AGT_Check(sprintf("%s: a shot down through its drawn forearm counts (part %g)", wn, through ? vr_ags_mpart : 0),
        through);
    // A body blow on its drawn forearm disarms it; one 3 units off its hand and weapon does not.
    vector side = normalize(VR_Cross(normalize(to), '0 0 1'));
    float gk = VR_AG_BlowGap(g, p, f);
    vector at = f;
    for(k = 0; k < 4; k++)
    {
        // (out along its side till 3 units off both hulls)
        at = at + side * (3 - gk);
        gk = VR_AG_BlowGap(g, p, at);
    }
    cvar_set("vr_agrunt_freeze", "0"); // (held still: no blow disarms, VR_AG_Still)
    float kept = VR_DHT_Blow(g, p, at);
    VR_AGT_Check(sprintf("%s: a blow %.2f units off its hand and weapon keeps it", wn, gk), kept && gk >= 2.5);
    if(kept)
    {
        float gf = VR_AG_BlowGap(g, p, f);
        float dis = !VR_DHT_Blow(g, p, f);
        VR_AGT_Check(sprintf("%s: a blow on its drawn forearm (gap %.2f; struck at %v, %.1f off it) disarms it", wn, gf,
            vr_hr_at, vlen(vr_hr_at - f)), dis);
    }
};

void() VR_DHT_Frame =
{
    entity pl = vr_agt_player;
    entity g = vr_agt_grunt;
    float t = time - vr_dht_t0;
    pl.flags = pl.flags | FL_GODMODE | FL_NOTARGET;
    if(vr_agt_step == 0)
    {
        vr_dht_kind = cvar("vr_agrunt_test_kind");
        vr_dht_disarm = cvar("vr_agrunt_disarm");
        cvar_set("vr_agrunt_disarm", "1");
        vr_dht_k = 0;
        vr_dht_w = 0;
        vr_agt_step = 1;
        return;
    }
    // (after the kinds: a grunt dual wielding two nail pistols, vr_dht_w its weapon measured: 0 the right one, 1 the left)
    float dual = vr_dht_k >= VR_AM_COUNT;
    if(vr_agt_step == 1)
    {
        cvar_set("vr_agrunt_test_kind", ftos(dual ? 0 : vr_dht_k));
        cvar_set("vr_agrunt_freeze", "1");
        vr_agk_test_hold = VR_AGK_AIM + 1;
        vr_agt_weapon2 = dual ? WID_NAILPISTOL : 0;
        vr_agt_grunt = g = VR_AGT_Spawn(dual ? WID_NAILPISTOL : VR_DHT_Wid(vr_dht_w), 60);
        vr_agt_weapon2 = 0;
        vr_dht_still = 0;
        vr_dht_last = '0 0 0';
        vr_dht_t0 = time;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        entity p = (dual && vr_dht_w == 1) ? g.vr_ag_prop2 : g.vr_ag_prop;
        string wn = dual ? sprintf("disarmhull Grunt dual nail pistols, the %s one", vr_dht_w ? "left" : "right")
                         : sprintf("disarmhull %s %s", VR_SPT_Name(vr_dht_k), VR_DHT_WName(vr_dht_w));
        vector o = p ? weapondrawnpose(p, 0, 30) : '0 0 0';
        vr_dht_still = (o != '0 0 0' && vlen(o - vr_dht_last) < 0.05) ? vr_dht_still + 1 : 0;
        vr_dht_last = o;
        if(vr_dht_still < 10)
        {
            if(t > 10)
            {
                VR_AGT_Check(sprintf("%s: timed out (its weapon never drawn and still)", wn), FALSE);
                vr_agt_step = 3;
            }
            return;
        }
        if(dual)
        {
            VR_AGT_Check(sprintf("%s: held in its %s hand (left %g)", wn, vr_dht_w ? "left" : "right", p.vr_ag_left),
                p.vr_ag_left == vr_dht_w);
        }
        VR_DHT_Measure(g, p, p.vr_ag_left, wn);
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        cvar_set("vr_agrunt_freeze", "0");
        vr_agk_test_hold = 0;
        VR_AGT_Remove(g);
        vr_agt_grunt = world;
        vr_dht_w = vr_dht_w + 1;
        if(vr_dht_w >= (dual ? 2 : VR_DHT_WEAPONS))
        {
            vr_dht_w = 0;
            vr_dht_k = vr_dht_k + 1;
        }
        vr_agt_step = vr_dht_k <= VR_AM_COUNT ? 1 : 5;
        return;
    }
    // The hand hull Show Disarm Volumes draws, on a grunt hunting you (not frozen: its arms posed on its gun as it moves;
    // his note 2026-11-01 02:20, the cyan hull "completely off ... moves with the enemy animation": made while the frame
    // drew it, it was the animation frame's). Each of 72 frames: the hand's hull it drew last (disarmhull 21 / 23) made
    // from the drawn rig (source 1) while drawn (+10), the drawn grip (VR_AG_DrawnFrame) on it.
    if(vr_agt_step == 5)
    {
        vr_dht_debug = cvar("vr_debug_disarm");
        cvar_set("vr_debug_disarm", "1");
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 200);
        vr_dht_n = vr_dht_bad = vr_dht_gmax = vr_dht_srv = vr_dht_gsum = 0;
        vr_dht_t0 = time;
        vr_agt_step = 6;
        return;
    }
    if(vr_agt_step == 6)
    {
        entity hp = g.vr_ag_prop;
        vector r = hp ? disarmhull(g, 23 + hp.vr_ag_left, '0 0 0', '0 0 0', 0) : '0 0 -1';
        if(r_z < 0 || r_z > 2)
        {
            if(t > 10)
            {
                VR_AGT_Check("disarmhull hunt: timed out (no hand hull drawn)", FALSE);
                vr_agt_step = 7;
            }
            return;
        }
        VR_AG_DrawnFrame(hp);
        vector gr = disarmhull(g, 23 + hp.vr_ag_left, vr_ag_df_o, vr_ag_df_o, 0);
        vr_dht_n = vr_dht_n + 1;
        vr_dht_bad = vr_dht_bad + (gr_y != 11);
        vr_dht_gmax = max(vr_dht_gmax, gr_x);
        vr_dht_gsum = vr_dht_gsum + max(0, gr_x);
        if(vr_dht_n < 72)
        {
            return;
        }
        // (the server's own query last: made now, the frame's drawing would reuse it)
        vector sq = disarmhull(g, 15 + hp.vr_ag_left, '0 0 0', '0 0 0', 0);
        vr_dht_srv = sq_x != 1;
        vector gs = disarmhull(g, 1 + hp.vr_ag_left, vr_ag_df_o, vr_ag_df_o, 0);
        VR_AGT_Check(sprintf("disarmhull hunt: the drawn hand hull from the drawn rig while drawn every frame (%g of %g "
            "frames not: source+10 last %g), the server's query from the rig too (source %g)", vr_dht_bad, vr_dht_n, gr_y, sq_x),
            vr_dht_bad == 0 && vr_dht_srv == 0);
        VR_AGT_Check(sprintf("disarmhull hunt: the drawn grip on the drawn hand hull (most %.2f units off, mean %.2f over %g "
            "frames; the server's hull now %.2f; %.0f from you)", vr_dht_gmax, vr_dht_gsum / vr_dht_n, vr_dht_n, gs_x,
            vlen(g.origin - vr_agt_player.origin)), vr_dht_gmax <= 2.5); // (the shotgun's grip point 1.44 out of the palm's hull)
        vr_agt_step = 7;
        return;
    }
    if(vr_agt_step == 7)
    {
        cvar_set("vr_debug_disarm", ftos(vr_dht_debug));
        VR_AGT_Remove(g);
        vr_agt_grunt = world;
        vr_agt_step = 4;
        return;
    }
    cvar_set("vr_agrunt_test_kind", ftos(vr_dht_kind));
    cvar_set("vr_agrunt_disarm", ftos(vr_dht_disarm));
    pl.flags = pl.flags - (pl.flags & (FL_GODMODE | FL_NOTARGET));
    vr_agt_step = 0;
    cvar_set("vr_agrunt_test", "0");
};
#endif

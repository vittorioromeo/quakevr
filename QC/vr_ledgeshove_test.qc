// vr_ledgeshove_test.qc -- vr_agrunt_test 210, on vrclimb: humanoids shoved off a ledge backpedal on their feet to the edge
// and go over it (vr_knockdown.qc VR_Knockdown_LedgeBackpedal; ROUND24.md, "Shoves off ledges"). A real shove
// (VR_Bash_Hit) of a grunt on the long ledge (its lip at x 96, its top at z 48, a 240-unit drop into the trench), its back
// to the lip, at the cells below: one hand, two hands, two hands running into it, each from two distances in the
// backpedal zone (48, 72: within vr_knockdown_ledge_backpedal_reach 80, which the test sets); then each from 24 (within
// vr_knockdown_ledge_topple 32, likewise: toppled over at once, the old launch) and from 128 (too far: an ordinary shove, the ledge
// plays no part); then, from the same spot, shoved the other way (it stays up). `sv_random_seed` picks the seed:
//   sv_random_seed 1;map vrclimb;wait30;god;vr_agrunt_test 210     -QuitOn "test 210 over"
// A trial's numbers:
// - went over: its middle 64 units or more below where it stood, in 6 s (safety net) while it was neither back up nor lying
//   still on top;
// - grounded: units along the shove its middle went before the lip with a foot down and its pelvis up all the while: the
//   lower of its
//   shins' middles (ragdollpoint) within LST_FOOT of how high they stood; a step lifts one, not both;
// - lip speed: its level speed as its middle crosses the lip, each strength's from 48 units (the same way to go).
// PASS: each trial's zone as its distance's (VR_Knockdown_Try's vr_kd_ledge_zone: 2 backpedal, 1 topple, 3 or 0 too far);
// 90% or more of the backpedals and topples over; every strength's grounded mean (backpedals) at least 75% of the way to the lip (the old launch: 59%); the
// strengths' lip speeds in order, 15 u/s or more apart; a strength's lip speeds varying by 10 u/s or more.

#ifdef QVR_TESTS

float LST_REPS = 6;      // trials a cell
float LST_CELLS = 12;    // cells: 6 backpedals, 3 topples, 3 too far (then LST_REPS the other way)
float LST_LIP = 96;      // x of the long ledge's lip
float LST_TOP = 48;      // z of its top
float LST_FOOT = 8;      // units: its lower shin this much higher than it stood: no foot down
float LST_AIRFRAMES = 3; // frames in a row with no foot down: off the ground
float LST_SAG = 0.5;     // of how high its pelvis stood: under this, it is down

float lst_cell, lst_rep, lst_step, lst_t0, lst_z0, lst_shin0, lst_air, lst_grounded, lst_flying, lst_lipv, lst_still;
vector lst_last;
float lst_keep_chance, lst_keep_topple, lst_keep_reach, lst_pelvis0;
float lst_over_n, lst_over_ok;     // all the trials that should go over; how many did
float lst_away_ok;                 // shoved away from the edge: stayed up
float lst_zone_n, lst_zone_ok;     // trials whose zone was its distance's
float lst_n[3];                    // by strength: trials over
float lst_gsum[3];                 // ... grounded units, as a share of the way to the lip
float lst_vsum[3], lst_vn[3], lst_vlo[3], lst_vhi[3]; // ... lip speeds

float(float c) LST_Strength = { return c < 6 ? floor(c / 2) : (c - 6) - 3 * floor((c - 6) / 3); };
// (each strength 48 from the lip, then 72; then 24 each, then 128 each)
float(float c) LST_Dist = { return c < 6 ? ((c & 1) ? 72 : 48) : c < 9 ? 24 : 128; };
// The zone a cell's shove should be in (vr_kd_ledge_zone): 2 backpedal, 1 topple, 3 too far (0 too: no ledge in its reach).
float(float c) LST_Zone = { return c < 6 ? 2 : c < 9 ? 1 : 3; };
string(float s) LST_Name = { return s == 0 ? "one hand" : s == 1 ? "two hands" : "two hands, running"; };

// The lower of its shins' middles (z), its lowest part while it stands; 0 no such parts yet.
float(entity g) LST_ShinZ =
{
    float lo = 0;
    vector a = ragdollpoint(g, ragdolllimbs(g, 6, 6), '0 0 0', 2);
    vector b = ragdollpoint(g, ragdolllimbs(g, 6, 7), '0 0 0', 2);
    if(a != '0 0 0') { lo = a_z; }
    if(b != '0 0 0') { lo = ((lo != 0) && (lo < b_z)) ? lo : b_z; }
    return lo;
};

void() LST_NextTrial =
{
    lst_step = 0;
    lst_rep = lst_rep + 1;
    if(lst_rep >= LST_REPS)
    {
        lst_rep = 0;
        lst_cell = lst_cell + 1;
    }
};

void() LST_Verdict =
{
    VR_AGT_Check(sprintf("each shove's zone its distance's: %g of %g", lst_zone_ok, lst_zone_n), lst_zone_ok == lst_zone_n);
    VR_AGT_Check(sprintf("over the edge (backpedals and topples): %g of %g trials", lst_over_ok, lst_over_n), lst_over_ok >= 0.9 * lst_over_n);
    float s;
    float grounded = TRUE;
    for(s = 0; s < 3; s = s + 1)
    {
        float mean = lst_vn[s] > 0 ? lst_vsum[s] / lst_vn[s] : 0;
        float share = lst_n[s] > 0 ? lst_gsum[s] / lst_n[s] : 0;
        VR_AGT_Print(sprintf("%s: over %g, on its feet %.0f%% of the way to the lip, lip speed %.0f u/s (%.0f to %.0f)",
            LST_Name(s), lst_n[s], share * 100, mean, lst_vlo[s], lst_vhi[s]));
        grounded = (grounded && (share >= 0.75));
        VR_AGT_Check(sprintf("%s: lip speeds vary (%.0f to %.0f u/s)", LST_Name(s), lst_vlo[s], lst_vhi[s]), lst_vhi[s] - lst_vlo[s] >= 10);
    }
    VR_AGT_Check("on its feet at least 75% of the way to the lip, every strength", grounded);
    float m0 = lst_vsum[0] / max(1, lst_vn[0]);
    float m1 = lst_vsum[1] / max(1, lst_vn[1]);
    float m2 = lst_vsum[2] / max(1, lst_vn[2]);
    VR_AGT_Check(sprintf("the launch differs by strength: %.0f, %.0f, %.0f u/s", m0, m1, m2), ((m1 - m0) >= 15) && ((m2 - m1) >= 15));
    VR_AGT_Check(sprintf("shoved away from the edge: %g of %g stayed up", lst_away_ok, LST_REPS), lst_away_ok == LST_REPS);
};

void() VR_LST_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(mapname != "vrclimb")
    {
        VR_AGT_Print(sprintf("ledge shove: NOT RUN on %s: map vrclimb", mapname));
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    float away = lst_cell >= LST_CELLS;
    float d = away ? 96 : LST_Dist(lst_cell);
    if(lst_step == 0)
    {
        if(lst_cell > LST_CELLS)
        {
            VR_AGPT_Clear();
            LST_Verdict();
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            vr_test_shove_run = 0;
            cvar_set("vr_knockdown_chance", ftos(lst_keep_chance));
            localcmd(sprintf("tune vr_knockdown_ledge_topple %g\ntune vr_knockdown_ledge_backpedal_reach %g\n", lst_keep_topple, lst_keep_reach));
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        if(lst_cell == 0 && lst_rep == 0)
        {
            lst_keep_chance = cvar_hget(cvarh_vr_knockdown_chance);
            cvar_set("vr_knockdown_chance", "100");
            // The zones the cells were laid out for (the shipped defaults moved to 56 and 92 on 2026-11-08, putting
            // the 48 cells in the topple zone): the probe reads the edge about 8 units past the lip, so 24 topples,
            // 48 and 72 backpedal, 128 is too far.
            lst_keep_topple = cvar_hget(cvarh_vr_knockdown_ledge_topple);
            lst_keep_reach = cvar_hget(cvarh_vr_knockdown_ledge_backpedal_reach);
            localcmd("tune vr_knockdown_ledge_topple 32\ntune vr_knockdown_ledge_backpedal_reach 80\n"); // (tunables; the first shove is 0.4 s on)
        }
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        VR_AGPT_Clear();
        vector at = '0 0 0';
        at_x = LST_LIP + d;
        at_y = 240 + 60 * lst_rep; // (along the ledge: no lip is quite the same place)
        at_z = LST_TOP + 24;
        setorigin(p, at + (away ? '-48 0 0' : '48 0 0'));
        p.velocity = '0 0 0';
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 40);
        setorigin(g, at);
        g.angles_y = away ? 180 : 0; // (facing the shover)
        lst_t0 = time;
        lst_step = 1;
        return;
    }
    if(lst_step == 1)
    {
        if(!g.takedamage || time < lst_t0 + 0.4)
        {
            return; // (started)
        }
        if(g.vr_wp_pack)
        {
            remove(g.vr_wp_pack); // (as vr_armedgrunt_impact_test.qc)
            g.vr_wp_pack = world;
        }
        g.health = 2000; // (the fall never kills it)
        float s = LST_Strength(lst_cell);
        lst_z0 = g.origin_z;
        lst_last = g.origin;
        lst_air = 0;
        lst_flying = FALSE;
        lst_pelvis0 = 0;
        lst_grounded = 0;
        lst_lipv = -1;
        lst_still = 0;
        entity oldself = self;
        self = p;
        vr_bash_exhaust_dmg = 0;
        vr_bash_exhaust_push = 0;
        vr_test_shove_run = (!away && s == 2) ? 200 : 0;
        float hand = (!away && s == 0) ? 0 : -1;
        vr_kd_ledge_zone = -1;
        VR_Bash_Hit(g, g.origin, away ? '1 0 0' : '-1 0 0', QVR_HITKIND_SHOVE, hand, 1, VR_Shove_HandsPush(hand == -1));
        if(!away)
        {
            float want = LST_Zone(lst_cell);
            float zoneOk = vr_kd_ledge_zone == want || (want == 3 && vr_kd_ledge_zone == 0);
            lst_zone_n = lst_zone_n + 1;
            lst_zone_ok = lst_zone_ok + (zoneOk ? 1 : 0);
            VR_AGT_Print(sprintf("%s, %g from the lip: zone %g (edge %.0f on)%s", LST_Name(s), d, vr_kd_ledge_zone,
                vr_kd_ledge_edge, zoneOk ? "" : sprintf(", WANTED %g", want)));
        }
        self = oldself;
        vr_test_shove_run = 0;
        lst_shin0 = LST_ShinZ(g);
        lst_t0 = time;
        lst_step = 2;
        return;
    }
    // Watching it.
    float x = g.origin_x;
    float gone = x < LST_LIP; // (its middle past the lip)
    if(!gone && !lst_flying)
    {
        float shin = LST_ShinZ(g);
        if(lst_shin0 == 0)
        {
            lst_shin0 = shin;
        }
        // (Its pelvis up too: sitting, sliding along the floor is not backpedalling.)
        vector pelvisAt = ragdollpoint(g, 0, '0 0 0', 2);
        float pelvisUp = pelvisAt_z - LST_TOP;
        lst_pelvis0 = max(lst_pelvis0, pelvisUp);
        float sunk = pelvisUp < LST_SAG * lst_pelvis0;
        lst_air = (((shin != 0) && (shin > lst_shin0 + LST_FOOT)) || sunk) ? lst_air + 1 : 0;
        lst_flying = ((lst_air >= LST_AIRFRAMES) || !isragdoll(g));
        if(!lst_flying)
        {
            lst_grounded = max(lst_grounded, LST_LIP + d - x);
        }
    }
    if(gone && lst_lipv < 0)
    {
        vector step = g.origin - lst_last;
        step_z = 0;
        lst_lipv = vlen(step) * 72; // (fast mode: a frame is 1/72 s of game time)
        if(frametime > 0)
        {
            lst_lipv = vlen(step) / frametime;
        }
    }
    lst_still = vlen(g.origin - lst_last) < 0.5 ? lst_still + 1 : 0;
    lst_last = g.origin;
    float fell = g.origin_z < lst_z0 - 64;
    float back = g.vr_knockdown != 1; // (up again)
    float lying = ((!g.vr_kd_stagger) && (lst_still >= 72));
    float timedout = time > lst_t0 + 6;
    if(!fell && !back && !lying && !timedout)
    {
        return;
    }
    float strength = LST_Strength(lst_cell);
    if(away)
    {
        lst_away_ok = lst_away_ok + (fell ? 0 : 1);
        VR_AGT_Print(sprintf("away %g: %s, at x %.0f", lst_rep, fell ? "FELL" : "stayed up", x));
    }
    else if(lst_cell >= 9)
    {
        VR_AGT_Print(sprintf("%s, %g from the lip (too far), %g: %s", LST_Name(strength), d, lst_rep, fell ? "went over" : "stayed up"));
    }
    else
    {
        lst_over_n = lst_over_n + 1;
        if(fell)
        {
            float share = min(1, lst_grounded / d);
            lst_over_ok = lst_over_ok + 1;
            lst_n[strength] = lst_n[strength] + (lst_cell < 6 ? 1 : 0);
            lst_gsum[strength] = lst_gsum[strength] + (lst_cell < 6 ? share : 0); // (backpedals: the topples are launched)
            if(lst_lipv >= 0 && lst_cell < 6 && (lst_cell & 1) == 0) // (from the same distance: how much of the shove is left)
            {
                lst_vlo[strength] = lst_vn[strength] > 0 ? min(lst_vlo[strength], lst_lipv) : lst_lipv;
                lst_vhi[strength] = lst_vn[strength] > 0 ? max(lst_vhi[strength], lst_lipv) : lst_lipv;
                lst_vsum[strength] = lst_vsum[strength] + lst_lipv;
                lst_vn[strength] = lst_vn[strength] + 1;
            }
        }
        VR_AGT_Print(sprintf("%s, %g from the lip, %g: %s in %.2f s, on its feet %.0f units, lip speed %.0f u/s", LST_Name(strength), d, lst_rep,
            fell ? "over" : back ? "STAYED (up again)" : lying ? "STAYED (lying on top)" : "STAYED (timed out)", time - lst_t0, lst_grounded, lst_lipv));
    }
    LST_NextTrial();
};

#endif // QVR_TESTS

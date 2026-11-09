//
//
//
// ----------------------------------------------------------------------------
// Armed grunts: disarming, drops and weapon reach (ROUND22.md, "Armed grunts: gameplay")
// ----------------------------------------------------------------------------
//
// - The tug-of-war (vr_agrunt_tug), always physical: your empty hand closing on its weapon doesn't simply take it. It
//   stands its ground as an active ragdoll, your hand holding its weapon hand's limb, its arms pulling back
//   (VR_AG_TugRagBegin; a staggering or downed one: VR_AG_TugDownBegin), its torso leaning against the pull and its free
//   arm flailing (vr_agrunt_tug_sway, vr_agrunt_tug_flail). Your pull wins it (VR_AG_TugBreak); letting go, or your
//   hand pulled past vr_agrunt_tug_let_go from its hold, loses it; it wrenches free after vr_agrunt_tug_hold_max s.
// - A grab takes the weapon at once (VR_AG_Shaky, VR_AG_TugEasy): stunned by a parry, down to vr_agrunt_tug_hurt of its
//   health, taken unawares from behind (it isn't fighting you, you are behind it), or no tug can begin (no ragdoll, no
//   room to stand). A staggering or downed one's needs an eased tug (vr_agrunt_tug_downed_scale; 0: at once).
// - Hits (VR_AG_Hit): a blow of vr_agrunt_disarm_damage or more on its gun hand or arm knocks the weapon loose; half
//   that while it is shaky (as above).
// - Parries (VR_AG_Parried): your weapon's parry of its melee blow knocks its weapon away with the chance
//   vr_agrunt_parry_disarm (a well-timed parry 1.5x).
// - Blows and throws on the weapon itself (VR_AG_WeaponStruck): your hand or what it holds (its line from the hand to
//   the weapon's muzzle) meeting the held weapon (its line from behind its grip to its muzzle, 5 units round) at
//   vr_agrunt_disarm_swing m/s or faster, or a thrown weapon, prop or gib (its last frame's path) meeting it with a
//   momentum (its mass x its speed) of vr_agrunt_disarm_throw kg m/s or more: knocked out along the blow or the throw.
// - Every disarm (all of the above, a tug won, a fall) clatters by the weapon's kind (a gun: heavy metal; a blade:
//   metal; Mjolnir, a haft: wood) and the grunt grunts (its pain sounds, at most every 0.6 s): VR_AG_DisarmFeedback.
// - Shoves and falls (VR_AG_FallDrop, vr_agrunt_fall_drop): a stagger never drops the weapon. Knocked down onto the
//   floor, it lets go as it lands (0.4 s on): the weapon falls beside it; getting up, it goes back for it first (its
//   own, within 160 units, whoever is nearer), if you haven't taken it.
// - Reach (VR_AG_Reachable): it only goes for a weapon it can walk to: not in a liquid, resting on a floor, the way
//   there (probed every 12 units) floored all along with no step up or down over 18 units, no liquid, no wall. It gives
//   a weapon up (VR_AG_SeekCheck) when it gets no nearer for 2.5 s or after vr_agrunt_seek_timeout s, and leaves it
//   alone for vr_agrunt_seek_shun s.

// (.vr_ag_tug_pl, the player tugging at its weapon: vr_armedgrunt_defs.qc)
.float vr_ag_tug_hand;   // ... with that hand
.float vr_ag_tug_since;   // ... since then
.float vr_ag_tug_prog;    // the pull's time so far (s)
.float vr_ag_tug_last;    // the tug's last frame (its step)
.float vr_ag_tug_buzz;    // the hand's next jerk
.float vr_ag_tug_cool;    // no new tug before this (it just shook you off)
.float vr_ag_tug_wins;    // the tests: tugs you won,
.float vr_ag_tug_losses;  // ... lost
.entity vr_ag_own;        // its own weapon it dropped falling (to take back)
.float vr_ag_seek_since;  // going for vr_ag_seek since
.float vr_ag_seek_best;   // ... its nearest so far (flat)
.float vr_ag_seek_prog;   // ... when it last got nearer
.float vr_ag_seek_check;  // ... its next reach check
.entity vr_ag_shun;       // a weapon it gave up on
.float vr_ag_shun_until;  // ... until then
string vr_ag_tug_why;     // the last tug's end (the tests)
.float vr_ag_snd_at;      // its next disarm grunt not before
.float vr_ag_knock_at;    // a thrown thing's knock (once), a player's main hand's blow on a held weapon
.float vr_ag_knock_off;   // ... the off hand's
float vr_ag_feedbacks;    // disarms' feedback sounds (the tests)
string vr_ag_feedback;    // ... the last clatter
vector vr_ag_wa;          // VR_AG_WeaponLine's: behind its grip,
vector vr_ag_wb;          // ... its muzzle
string vr_ag_reach_why;   // VR_AG_Reachable's last no (the tests)
.float vr_ag_tug_imp;    // the physical tug: its pull's force times time so far (N s)
.float vr_ag_tug_peak;    // ... its hardest pull (N; the tests)
.float vr_ag_tug_held;    // ... how long you have kept pulling above vr_agrunt_tug_pull_force (s; eased off below)
float vr_ag_tug_test_force; // the tests' pull (N) in place of the hold's (0: the hold's; VR_AG_TugBreak)
float vr_ag_tug_fjoint;   // the last physical tug frame's pulls (N; the tests): the hold's joint force,
float vr_ag_tug_fstretch; // ... its stretch's (vr_agrunt_tug_measure)
float vr_ag_tug_ease;     // VR_AG_TugBreak's thresholds times this (both hands on; 0: 1)
.float vr_ag_tug_two;     // the tug's player's other hand on its weapon too (vr_agrunt_tug_second_grab 0)
.float vr_ag_tug_since2;  // ... since then
.float vr_ag_tug_down;    // the tug began on it staggering or down: its thresholds this share (vr_agrunt_tug_downed_scale)
// Knocked down keeping its weapon (VR_AG_FallDrop, vr_agrunt_fall_keep): its fall measured from its start.
.float vr_ag_fall_at;     // the knockdown measured (its vr_kd_since)
.vector vr_ag_fall_last;  // ... its body's place last frame
.float vr_ag_fall_down;   // ... its body's fastest fall (units/s)
.float vr_ag_fall_fling;  // ... its body's fastest motion any way but down (units/s)
.float vr_ag_fall_kept;   // ... kept its weapon as it fell (the tests)
.vector vr_ag_fall_v;     // ... its body's velocity last frame (units/s; the impact rule)
.float vr_ag_fall_fast;   // ... how long its body has kept up vr_agrunt_fall_impact_speed (s; VR_AG_FallImpact)
.float vr_ag_fall_slow;   // ... how long it has been under it since (s: a moment's dip is the origin's jitter)
.vector vr_ag_fall_ring[VR_AG_FALL_RING];   // ... its velocity at its last few moments moving fast and sustained
.float vr_ag_fall_ring_t[VR_AG_FALL_RING];  // ... and when (0: none yet)
.float vr_ag_fall_ring_n;                   // ... the slot written last
.vector vr_ag_fall_into;      // ... the level's strikes on its parts of this slice of time, summed (VR_AG_FallTouch)
.vector vr_ag_fall_into_was;  // ... of the slice before
.float vr_ag_fall_into_at;    // ... when this slice began
.float vr_ag_fall_swung;  // ... when a hand holding it last swung (vr_ragimpact_drag_speed)
.float vr_ag_fall_hit;    // ... its hardest impact so far (units/s lost on a surface it flew into; your hands' bang: its speed over vr_agrunt_fall_impact_held; vr_agrunt_fall_keep_impact)
.float vr_ag_fall_hit_at; // ... when (the tests)
.float vr_ag_fall_held;   // ... held by your hands last frame (the tests)
float VR_AG_FALL_DRAG = 0.4; // s a holding hand's swing counts after it (the engine's dragWindow, vr_box3d_step.cpp): else you only drag it
float VR_AG_FALL_STRUCK = 1; // s after a blow on it (vr_ag_struck_at) its body's knocks are the blow's (VR_AG_FallImpact)
float vr_ag_tug_force;    // ... the last frame's pull (N; the tests)
float vr_ag_parried_n;    // its blows your weapons parried (VR_AG_Parried; the tests)
float VR_AG_TUG_RECOVER = 0.2;  // s up again as you let go (or it wrenches free)
float VR_AG_DOWN_SWING = 0.8;   // m/s: a blow on its weapon knocks it away while it is down holding it (vr_agrunt_fall_keep)

float VR_AG_TUG_COOL = 0.8;     // s before a new grab after a tug it kept its weapon in
float VR_AG_FALL_LETGO = 0.4;   // s down before it lets go
float VR_AG_OWN_RANGE = 160;    // its own dropped weapon taken back this near
float VR_AG_STEP = 18;          // a step up or down it walks
float VR_AG_PROBE = 12;         // the way's probes, units apart
float VR_AG_STUCK = 2.5;        // s no nearer: given up

// Its share of its health (a grunt's full health 30 when unset).
float(entity g) VR_AG_HealthShare =
{
    return g.health / (g.max_health > 0 ? g.max_health : 30);
};

// Shaky: staggered or down, stunned by a parry, badly hurt ("": steady).
string(entity g) VR_AG_Shaky =
{
    if(g.vr_knockdown)
    {
        return g.vr_kd_stagger ? "staggering" : "knocked down";
    }
    if(time < g.vr_parry_stagger_until)
    {
        return "stunned by a parry";
    }
    if(VR_AG_HealthShare(g) <= cvar_hget(cvarh_vr_agrunt_tug_hurt))
    {
        return "badly hurt";
    }
    return "";
};

// Why a grab by `pl` takes its weapon at once ("": a tug).
string(entity g, entity pl) VR_AG_TugEasy =
{
    if(!cvar_hget(cvarh_vr_agrunt_tug))
    {
        return "grabbed";
    }
    string why = VR_AG_Shaky(g);
    if(why != "")
    {
        return why;
    }
    makevectors('0 1 0' * g.angles_y);
    vector to = pl.origin - g.origin;
    to_z = 0;
    if(g.enemy != pl && v_forward * normalize(to) < 0)
    {
        return "taken unawares";
    }
    return "";
};

string(entity g) VR_AG_TugReadout;
string(string why) VR_AG_TugLostRule;
void(entity g, float won, string why) VR_AG_TugEnd =
{
    entity pl = g.vr_ag_tug_pl;
    if(vr_dv_on)
    {
        // The verdict and the readout as it ended, kept over it a few seconds (debugdisarmnote 6; its console line).
        debugdisarmnote(g, 6, g.origin + '0 0 40', sprintf("TUG %s: %s\n%s", won ? "WON" : "LOST",
            won ? why : sprintf("%s (%s)", VR_AG_TugLostRule(why), why), VR_AG_TugReadout(g)));
    }
    // Your hand's hold let go, its stagger over in a moment (won: a short stumble, its grip broken), then its AI again
    // (vr_knockdown.qc VR_Knockdown_Think: recovered).
    float stumble = won ? max(0, cvar_hget(cvarh_vr_agrunt_tug_won_stumble)) : VR_AG_TUG_RECOVER;
    ragdolltug(g, pl, g.vr_ag_tug_hand, 2, '0 0 0', stumble);
    g.vr_ag_tug_pl = world;
    g.vr_ag_tug_two = 0;
    g.vr_ag_tug_down = 0;
    pl.vr_ag_knock_at = time + 0.5; // (no blow on its weapon from your hands as the struggle ends: a headbutt's or a shove's push)
    pl.vr_ag_knock_off = time + 0.5;
    vr_ag_tug_why = why;
    if(won)
    {
        g.vr_ag_tug_wins = g.vr_ag_tug_wins + 1;
    }
    else
    {
        g.vr_ag_tug_losses = g.vr_ag_tug_losses + 1;
        g.vr_ag_tug_cool = time + VR_AG_TUG_COOL;
    }
    VR_AG_Log(sprintf("tug %s after %.2f s (%s; pulled %.2f s)", won ? "won" : "lost", time - g.vr_ag_tug_since, why,
        g.vr_ag_tug_prog));
    if(pl.flags & FL_CLIENT)
    {
        entity oldself = self;
        self = pl;
        haptic(g.vr_ag_tug_hand, 0, won ? 0.15 : 0.25, won ? 120 : 60, 1);
        self = oldself;
    }
};

// The tug on a standing one: `g` stands its ground as an active ragdoll (a stagger that never goes over, up again only
// when the tug ends: vr_knockdown.qc), `pl`'s hand `h` holding its weapon: the hand holds its hand's limb at the
// weapon's lever (ragdolltug), its arms pull back (vr_agrunt_tug_strength). FALSE: it can't (no ragdoll, no room): the
// grab takes the weapon at once.
vector(entity g, entity p) VR_AG_TugGripAt;
float(entity g, entity pl, float h) VR_AG_TugRagBegin =
{
    if(g.vr_knockdown || !canragdoll(g) || !ragdollcanstagger(g) || !g.vr_ag_prop)
    {
        return FALSE;
    }
    entity p = g.vr_ag_prop;
    vector way = g.origin - pl.origin;
    way_z = 0;
    if(vlen(way) < 1)
    {
        makevectors('0 1 0' * g.angles_y);
        way = v_forward * -1;
    }
    way = normalize(way);
    float cap = max(0, cvar_hget(cvarh_vr_agrunt_tug_hold_max));
    vector wasVel = g.velocity;
    float wasFlags = g.flags;
    g.velocity = '0 0 0';
    g.flags = g.flags | FL_ONGROUND;
    if(!VR_Knockdown_Start(g))
    {
        g.velocity = wasVel;
        g.flags = wasFlags;
        return FALSE;
    }
    // (Sure-footed, steady, not carried: it stands where it stood. Up again when the tug ends: VR_AG_TugEnd.)
    if(!ragdollstagger(g, way, 0, 1, 0, cap > 0 ? cap + 2 : 600, TRUE))
    {
        ragdollshovetopple(g, way, 0.3, FALSE, 0); // (its ragdoll made, no stagger: down; the grab takes it: shaky)
        return FALSE;
    }
    g.vr_kd_stagger = 1;
    g.vr_kd_balance = 1;
    g.vr_kd_dir = way;
    g.nextthink = time + 0.05;
    float strength = cvar_hget(cvarh_vr_agrunt_tug_strength);
    // (Your hold never gives first; at most 6000 N: a stiffer spring shakes the ragdoll apart.)
    float grip = min(6000, max(1, cvar_hget(cvarh_vr_agrunt_tug_break_force)) * 3);
    if(!ragdolltug(g, pl, h, 0, VR_AG_TugGripAt(g, p), strength, grip)) // (its weapon hand's limb)
    {
        ragdolltug(g, pl, h, 2, '0 0 0', 0.1); // (up again at once; the grab takes it: staggering)
        return FALSE;
    }
    return TRUE;
};

// A downed or staggering one's arm pulling its weapon back in the physical tug (N): half the least pull
// (vr_agrunt_tug_pull_force) times its arms' pull (vr_agrunt_tug_strength) times the downed share
// (vr_agrunt_tug_downed_scale): 81 N as shipped, under the least pull that counts (a still hold never wins it).
float(entity g) VR_AG_TugDownBack =
{
    return 0.5 * max(0, cvar_hget(cvarh_vr_agrunt_tug_pull_force)) * max(0, cvar_hget(cvarh_vr_agrunt_tug_strength)) *
        max(0, g.vr_ag_tug_down);
};

// The physical tug on a downed or staggering one (his note 2026-10-31: "doesn't seem to actually physically grab the
// enemy's weapon/hand and physically move it"): `pl`'s hand `h` holds its ragdoll's weapon hand (VR_AG_TugGripAt) by a
// spring, as a limb grab does, its arm pulling it back towards its body (VR_AG_TugDownBack; ragdolltug 6): your pull
// moves weapon, hand and arm (the body if hard enough), its pull felt in the hold. Its thresholds' times eased by
// vr_ag_tug_down (VR_AG_TugBreak). FALSE: no ragdoll (the grab takes the weapon at once).
float(entity g, entity pl, float h) VR_AG_TugDownBegin =
{
    if(!g.vr_ag_prop || !isragdoll(g) || g.vr_ag_tug_down <= 0)
    {
        return FALSE;
    }
    float grip = min(6000, max(1, cvar_hget(cvarh_vr_agrunt_tug_break_force)) * 3);
    return ragdolltug(g, pl, h, 6, VR_AG_TugGripAt(g, g.vr_ag_prop), VR_AG_TugDownBack(g), grip);
};

// Its grip's strength in the physical tug: weakened by its wounds (vr_agrunt_tug_hurt_ease). Not by pain any more: in a
// fight every hit's pain halved it (and its health share down to 0.35 again), and the author's grabs in a fight tore the
// weapon out in 0.1-0.3 s (note vrtesthall_2026-10-24_01-10-14).
float(entity g) VR_AG_TugShare =
{
    float ease = min(1, max(0, cvar_hget(cvarh_vr_agrunt_tug_hurt_ease)));
    return 1 - ease * (1 - min(1, max(0, VR_AG_HealthShare(g))));
};

// The physical tug's pull this frame (`f` N over `dt` s): "" it holds on, else how its weapon came out. A tug-of-war:
// only a pull above vr_agrunt_tug_pull_force counts (eased off below it); kept up vr_agrunt_tug_pull_time s, a pull of
// vr_agrunt_tug_break_force or force x time (counted above the pull force) of vr_agrunt_tug_break_impulse tears it out.
// A yank vr_agrunt_tug_snap x the break force tears it out at once. All of them times its grip (VR_AG_TugShare).
// The physical tug's thresholds now (VR_AG_TugBreak's; the tug readout's), times its grip (VR_AG_TugShare) and `ez` (both
// hands on: vr_agrunt_tug_two_hand_ease; else 1): vr_ag_tl_brk the break force (N), _need the break impulse (N s), _least
// the least pull that counts (N), _hold the least pull's time (s), _snap a yank's (N; 0 none).
float vr_ag_tl_brk, vr_ag_tl_need, vr_ag_tl_least, vr_ag_tl_hold, vr_ag_tl_snap;
void(entity g, float ez) VR_AG_TugLimits =
{
    float share = VR_AG_TugShare(g) * ez;
    vr_ag_tl_brk = max(1, cvar_hget(cvarh_vr_agrunt_tug_break_force)) * share;
    vr_ag_tl_need = max(0.01, cvar_hget(cvarh_vr_agrunt_tug_break_impulse)) * share;
    vr_ag_tl_least = max(0, cvar_hget(cvarh_vr_agrunt_tug_pull_force)) * share;
    vr_ag_tl_hold = max(0, cvar_hget(cvarh_vr_agrunt_tug_pull_time)) * ez;
    if(g.vr_ag_tug_down > 0)
    {
        // (A downed or staggering one's: its times eased, vr_agrunt_tug_downed_scale; the pulls that count as ever.)
        vr_ag_tl_need = vr_ag_tl_need * g.vr_ag_tug_down;
        vr_ag_tl_hold = vr_ag_tl_hold * g.vr_ag_tug_down;
    }
    float snap = cvar_hget(cvarh_vr_agrunt_tug_snap);
    vr_ag_tl_snap = snap > 0 ? vr_ag_tl_brk * snap : 0;
};

// The tug readout (Show Disarm Volumes, over its weapon while a tug is on; with the verdict for a few seconds after it
// ends: VR_AG_TugEnd; his note 2026-11-01 18:35, "understand why sometimes it succeeds, why it fails"): every number
// its rules read, against what each must reach.
string(entity g) VR_AG_TugReadout =
{
    entity pl = g.vr_ag_tug_pl;
    float h = g.vr_ag_tug_hand;
    string s;
    // (its state: a standing one's own stagger is the tug's, not shaky)
    string hurt = (g.vr_ag_tug_down <= 0 && g.vr_kd_stagger) ? "" : VR_AG_Shaky(g);
    if(hurt == "" && g.vr_ag_tug_down <= 0)
    {
        hurt = time < g.vr_parry_stagger_until ? "stunned by a parry"
             : VR_AG_HealthShare(g) <= cvar_hget(cvarh_vr_agrunt_tug_hurt) ? "badly hurt" : "";
    }
    if(hurt == "")
    {
        hurt = "steady";
    }
    float cap = cvar_hget(cvarh_vr_agrunt_tug_hold_max);
    s = sprintf("TUG%s%s: %.2f s%s", g.vr_ag_tug_down > 0 ? ", it downed" : "",
        g.vr_ag_tug_two ? ", both hands" : "", time - g.vr_ag_tug_since,
        cap > 0 ? sprintf(" (it wrenches free at %.1f)", cap) : "");
    VR_AG_TugLimits(g, g.vr_ag_tug_two ? min(1, max(0.05, cvar_hget(cvarh_vr_agrunt_tug_two_hand_ease))) : 1);
    s = sprintf("%s\npull %.0f N (hold %.0f, stretch %.0f) / break %.0f N%s", s, vr_ag_tug_force, vr_ag_tug_fjoint,
        vr_ag_tug_fstretch, vr_ag_tl_brk, vr_ag_tl_snap > 0 ? sprintf(", yank %.0f", vr_ag_tl_snap) : "");
    s = sprintf("%s\nover %.0f N: kept %.2f / %.2f s", s, vr_ag_tl_least, g.vr_ag_tug_held, vr_ag_tl_hold);
    s = sprintf("%s\nimpulse %.0f / %.0f N s; progress %.0f%%", s, g.vr_ag_tug_imp, vr_ag_tl_need,
        min(1, g.vr_ag_tug_prog) * 100);
    float d = pl ? ragdolltug(g, pl, h, 7) : -1;
    float lg = pl ? ragdolltug(g, pl, h, 8) : -1;
    float cm = 100 / VRMetersToUnits();
    s = sprintf("%s\nhand %.1f / let go %.1f cm", s, d >= 0 ? d * cm : -1, lg >= 0 ? lg * cm : -1);
    return sprintf("%s\nits grip x%.2f: health %.0f%% (badly hurt at %.0f%%), %s", s, VR_AG_TugShare(g),
        VR_AG_HealthShare(g) * 100, cvar_hget(cvarh_vr_agrunt_tug_hurt) * 100, hurt);
};

// What a tug's end `why` (lost) means, for the verdict.
string(string why) VR_AG_TugLostRule =
{
    if(why == "let go")
    {
        return "you opened your hand";
    }
    if(why == "torn away")
    {
        return "your hand went past the let-go distance from its hold";
    }
    if(why == "wrenched free")
    {
        return "it held on to vr_agrunt_tug_hold_max";
    }
    return why;
};

string(entity g, float f, float dt) VR_AG_TugBreak =
{
    float ez = vr_ag_tug_ease > 0 ? vr_ag_tug_ease : 1;
    VR_AG_TugLimits(g, ez); // (both hands on: eased)
    float brk = vr_ag_tl_brk;
    float need = vr_ag_tl_need;
    float least = vr_ag_tl_least;
    float hold = vr_ag_tl_hold;
    float snap = cvar_hget(cvarh_vr_agrunt_tug_snap);
    vr_ag_tug_force = f;
    g.vr_ag_tug_peak = max(g.vr_ag_tug_peak, f);
    if(f >= least)
    {
        g.vr_ag_tug_held = g.vr_ag_tug_held + dt;
        g.vr_ag_tug_imp = g.vr_ag_tug_imp + f * dt;
    }
    else
    {
        g.vr_ag_tug_held = max(0, g.vr_ag_tug_held - dt * 2); // (let up: it gets its grip back)
        g.vr_ag_tug_imp = max(0, g.vr_ag_tug_imp - need * 0.5 * dt);
    }
    g.vr_ag_tug_prog = min(g.vr_ag_tug_imp / need, hold > 0 ? g.vr_ag_tug_held / hold : 1);
    if(snap > 0 && f >= brk * snap)
    {
        return sprintf("torn out of its hand by a yank (%.0f N)", f);
    }
    if(g.vr_ag_tug_held < hold)
    {
        return "";
    }
    if(f >= brk)
    {
        return sprintf("torn out of its hand (%.0f N, pulled %.2f s)", f, g.vr_ag_tug_held);
    }
    if(g.vr_ag_tug_imp >= need)
    {
        return sprintf("pulled out of its hand (%.0f N s, pulled %.2f s)", g.vr_ag_tug_imp, g.vr_ag_tug_held);
    }
    return "";
};

// Where on its ragdoll a tug's hold takes it (ragdolltug's `grip`: the engine holds the limb nearest it): its hand
// holding weapon `p` as drawn (weapondrawnpose 36, agarms::ragdollGrip: the drawnpose query), its middle. Not the weapon's
// own place: a two-handed weapon's grip lies nearer its support hand (his note 2026-10-31: the arm pulled was the other
// one). The weapon's place when there is no such hand.
vector(entity g, entity p) VR_AG_TugGripAt =
{
    vector hv = weapondrawnpose(g, p.vr_ag_left ? 1 : 0, 36);
    if(hv_x >= 0)
    {
        // (Its middle, ragdollpoint's 2: the hand's body frame '0 0 0' lies by the spine, and the hold took the spine.
        // '0 0 0' without a ragdoll.)
        vector at = ragdollpoint(g, hv_x, '0 0 0', 2);
        VR_AG_Log(sprintf("tug grip: its hand bone %g at %v (its weapon at %v)", hv_x, at, p.origin));
        if(at != '0 0 0')
        {
            return at;
        }
    }
    return p.origin;
};

// The other hand (cVR_MainHand / cVR_OffHand).
float(float h) VR_AG_OtherHand = { return h == cVR_MainHand ? cVR_OffHand : cVR_MainHand; };

// The hand a won tug's weapon goes into: the last of your hands placed on it (both on: the second, vr_ag_tug_since2; his
// note 2026-10-31: "the one placed last, not the one placed first"), the other when that one is busy (VR_Hand_Free).
// Asked before VR_AG_TugEnd (it clears vr_ag_tug_two).
float(entity g, entity pl) VR_AG_TugWinHand =
{
    float h = g.vr_ag_tug_hand;
    if(!g.vr_ag_tug_two)
    {
        return h;
    }
    float last = g.vr_ag_tug_since2 >= g.vr_ag_tug_since ? VR_AG_OtherHand(h) : h;
    float first = VR_AG_OtherHand(last);
    if(!VR_Hand_Free(pl, last, VR_HANDFREE_LOCKED) && VR_Hand_Free(pl, first, VR_HANDFREE_LOCKED))
    {
        return first;
    }
    return last;
};

// The physical tug's pull on hand `h`'s hold now (N; -1 no hold): its joint's force, its stretch (your controller from
// the held point) times vr_agrunt_tug_stiffness, or the larger (vr_agrunt_tug_measure 0, 1, 2). The stretch reads the
// same however the hold's spring gives or the arm reaches (test 31 read 70-230 N of joint force for a 0.6 m/s pull since
// the posed arms and the authored rig, under the least pull of 270 N: a steady pull never counted).
float(entity g, entity pl, float h) VR_AG_TugPull =
{
    float fj = ragdolltug(g, pl, h, 1);
    if(fj < 0)
    {
        return -1;
    }
    float st = ragdolltug(g, pl, h, 3);
    float fs = st > 0 ? st * max(0, cvar_hget(cvarh_vr_agrunt_tug_stiffness)) : 0;
    vr_ag_tug_fjoint = fj;
    vr_ag_tug_fstretch = fs;
    // (A downed one's: the hold's force only once your hand has drawn its arm 5 cm from its body, else the stretch. Its
    // hold's force carries its own fall and flailing too: a sword grunt still falling tore the weapon out of a still hand
    // in 0.3 s; dragged along, its body follows and the stretch stays small: test 91 never won. Felt still: the haptics.)
    float m = g.vr_ag_tug_down > 0 ? (st > 0.05 ? 2 : 1) : cvar_hget(cvarh_vr_agrunt_tug_measure);
    return m == 0 ? fj : (m == 1 ? fs : max(fj, fs));
};

// The tug's player's hand `hh` lets go of its weapon, the other hand holding on (the second hand: vr_ag_tug_two).
void(entity g, entity pl, float hh, string why) VR_AG_TugLetGoOne =
{
    ragdolltug(g, pl, hh, 4);
    g.vr_ag_tug_two = 0;
    VR_AG_Log(sprintf("tug: hand %g let go (%s), hand %g holds on", hh, why, VR_AG_OtherHand(hh)));
};

// The tug's first hand lets go, the second holding on: the tug goes on with the second.
void(entity g, entity pl, string why) VR_AG_TugPromote =
{
    float h = g.vr_ag_tug_hand;
    float h2 = VR_AG_OtherHand(h);
    VR_AG_TugLetGoOne(g, pl, h, why);
    g.vr_ag_tug_hand = h2;
    g.vr_ag_tug_since = g.vr_ag_tug_since2;
};

// The tug's player's other hand `h` closing on its weapon too: it joins the tug (its hold's pull added, the thresholds
// eased: VR_AG_TugRagFrame), or (vr_agrunt_tug_second_grab 1) tears it free into this hand, the last placed (the first
// when this one is busy). TRUE always (handled).
float(entity g, entity pl, float h) VR_AG_TugJoin =
{
    entity p = g.vr_ag_prop;
    if(!p || g.vr_ag_tug_two || h == g.vr_ag_tug_hand)
    {
        return TRUE;
    }
    if(cvar_hget(cvarh_vr_agrunt_tug_second_grab))
    {
        // (Into the hand placed last, this one, unless it is busy: VR_AG_TugWinHand's rule.)
        float h1 = VR_Hand_Free(pl, h, VR_HANDFREE_LOCKED) ? h : g.vr_ag_tug_hand;
        VR_AG_TugEnd(g, TRUE, "torn free by your other hand's grab");
        VR_AG_Yank(g, pl, h1);
        return TRUE;
    }
    float grip = min(6000, max(1, cvar_hget(cvarh_vr_agrunt_tug_break_force)) * 3);
    float dn = g.vr_ag_tug_down > 0; // (a downed one's hold: ragdolltug 6, its arm's pull in N)
    if(!ragdolltug(g, pl, h, dn ? 6 : 0, VR_AG_TugGripAt(g, p),
        dn ? VR_AG_TugDownBack(g) : cvar_hget(cvarh_vr_agrunt_tug_strength), grip))
    {
        return TRUE; // (no hold for it: swallowed, as before)
    }
    g.vr_ag_tug_two = 1;
    g.vr_ag_tug_since2 = time;
    VR_AG_Log(sprintf("tug: hand %g joins hand %g, %.1f units from its weapon", h, g.vr_ag_tug_hand,
        vlen(VRGetEntHandPos(pl, h) - p.origin)));
    entity oldself = self;
    self = pl;
    haptic(h, 0, 0.1, 80, 0.6);
    self = oldself;
    return TRUE;
};

// The physical tug's frame (VR_AG_TugFrame): its pull on your hand's hold (the force between its hand and the weapon);
// a pull of vr_agrunt_tug_break_force, or force times time (counted above a tenth of it) of vr_agrunt_tug_break_impulse,
// breaks its grip (less hurt, half in pain): the weapon is yours. vr_agrunt_tug_hold_max s on, it wrenches it free.
float(entity g, entity p, entity pl, float h) VR_AG_TugRagFrame =
{
    float f = VR_AG_TugPull(g, pl, h);
    if(f < 0 && g.vr_ag_tug_down > 0 && !isragdoll(g) && g.vr_ag_prop)
    {
        // A downed one up again (its ragdoll and the holds gone), your hand still on its weapon: once it is afoot the tug
        // goes on as a standing one's (its progress kept), your hand holding it again; no room to stand, it went down:
        // the weapon is yours.
        if(g.vr_knockdown)
        {
            return FALSE; // (getting up: its weapon in its hand as posed)
        }
        g.vr_ag_tug_down = 0;
        g.vr_ag_tug_two = 0;
        if(VR_AG_TugRagBegin(g, pl, h))
        {
            VR_AG_Log("tug: it got up: the tug goes on, it standing its ground");
            return TRUE;
        }
        if(g.vr_knockdown)
        {
            float hk = VR_AG_TugWinHand(g, pl);
            VR_AG_TugEnd(g, TRUE, "it went down");
            VR_AG_Yank(g, pl, hk);
            return TRUE;
        }
        VR_AG_TugEnd(g, FALSE, "it got up");
        return FALSE;
    }
    if(f < 0)
    {
        if(g.vr_ag_tug_two)
        {
            VR_AG_TugPromote(g, pl, "its hold lost"); // (the other hand holds on: the tug goes on with it)
            return TRUE;
        }
        VR_AG_TugEnd(g, FALSE, "torn away"); // (the hold lost: the limb left far behind your hand)
        return FALSE;
    }
    float fj = vr_ag_tug_fjoint;
    float fs = vr_ag_tug_fstretch;
    vr_ag_tug_ease = 1;
    if(g.vr_ag_tug_two)
    {
        // Both hands on it: the second's pull added (vr_agrunt_tug_two_hand_mult), every threshold eased.
        float f2 = VR_AG_TugPull(g, pl, VR_AG_OtherHand(h));
        if(f2 < 0)
        {
            VR_AG_TugLetGoOne(g, pl, VR_AG_OtherHand(h), "its hold lost");
        }
        else
        {
            f = f + f2 * max(0, cvar_hget(cvarh_vr_agrunt_tug_two_hand_mult));
            vr_ag_tug_ease = min(1, max(0.05, cvar_hget(cvarh_vr_agrunt_tug_two_hand_ease)));
        }
        vr_ag_tug_fjoint = fj;
        vr_ag_tug_fstretch = fs;
    }
    if(g.vr_knockdown == 1 && !g.vr_kd_stagger && !g.vr_ag_tug_down) // (a downed one's tug: down already)
    {
        float hd = VR_AG_TugWinHand(g, pl);
        VR_AG_TugEnd(g, TRUE, "it went down"); // (no room to stand: down, it lets go)
        VR_AG_Yank(g, pl, hd);
        return TRUE;
    }
    VR_AG_RagProp(g, p);
    // The tug's arm stretch (vr_agrunt_tug_ik_stretch, eased in with the tug: VR_AGK_TugStretch, the posed arms' too) on
    // its ragdoll's drawn arm held by each hand (ragdolltug 9: the drawn pose only, ragdoll::drawnPose).
    float ts = VR_AGK_TugStretch(g);
    ragdolltug(g, pl, h, 9, '0 0 0', ts);
    if(g.vr_ag_tug_two)
    {
        ragdolltug(g, pl, VR_AG_OtherHand(h), 9, '0 0 0', ts);
    }
    float dt = min(0.1, time - g.vr_ag_tug_last);
    g.vr_ag_tug_last = time;
    if(vr_ag_tug_test_force > 0)
    {
        f = vr_ag_tug_test_force;
    }
    string won = VR_AG_TugBreak(g, f, dt);
    float ease = vr_ag_tug_ease;
    vr_ag_tug_ease = 1;
    if(won != "")
    {
        float hw = VR_AG_TugWinHand(g, pl);
        VR_AG_TugEnd(g, TRUE, g.vr_ag_tug_two ? strcat(won, ", both hands") : won);
        VR_AG_Yank(g, pl, hw);
        return TRUE;
    }
    float brk = max(1, cvar_hget(cvarh_vr_agrunt_tug_break_force)) * VR_AG_TugShare(g) * ease;
    // Its body in the struggle (a standing one's: its torso leaning against your pull, its free arm flailing;
    // vr_agrunt_tug_sway, vr_agrunt_tug_flail): the pull's share of what breaks its grip (its way: the hold's).
    ragdolltug(g, pl, h, 10, '0 0 0', min(2, f / brk));
    float cap = cvar_hget(cvarh_vr_agrunt_tug_hold_max);
    if(cap > 0 && time - g.vr_ag_tug_since >= cap)
    {
        VR_AG_TugEnd(g, FALSE, "wrenched free");
        return TRUE;
    }
    // Your hand jerked by its pull, harder as its grip gives.
    if(time >= g.vr_ag_tug_buzz)
    {
        g.vr_ag_tug_buzz = time + 0.1;
        float gain = min(1, max(max(f, vr_ag_tug_fjoint) / brk, g.vr_ag_tug_prog)); // (its arm's pull back felt too)
        if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
        {
            VR_AG_Log(sprintf("tug: pull %.0f N (joint %.0f, stretch %.0f), held %.2f s, %.0f N s%s", f, vr_ag_tug_fjoint,
                vr_ag_tug_fstretch, g.vr_ag_tug_held, g.vr_ag_tug_imp, g.vr_ag_tug_down > 0 ? ", downed" : ""));
        }
        entity oldself = self;
        self = pl;
        haptic(h, 0, 0.1, 40 + 140 * gain, 0.25 + 0.75 * gain);
        if(random() < 0.3 + 0.5 * gain)
        {
            handimpact(h, 0.5 + 2 * gain, normalize(g.origin + '0 0 16' - VRGetEntHandPos(pl, h)));
        }
        self = oldself;
    }
    return TRUE;
};

// A held weapon on its ragdoll (VR_AG_PlaceProp: a stagger, a tug, a fall): riding its hand's limb as it was held when
// the ragdoll began (the limb nearest it then; its forward and up as last posed).
void(entity g, entity p) VR_AG_RagProp =
{
    if(!p)
    {
        return;
    }
    // Drawn on its ragdoll's hand (hitreact::drawnWeapon, the client's ragdollProps: the hand holding it, by its grip
    // offsets): placed there, so the disarm volumes, shots, strikes and grabs meet the weapon where it is seen.
    if(isragdoll(g) && VR_AG_DrawnFrame(p))
    {
        // (the limb it rides: the hand holding it as drawn, not the limb nearest it as the ragdoll began: a two-handed
        // weapon's support hand, his rocket launcher note)
        vector hv = weapondrawnpose(g, p.vr_ag_left ? 1 : 0, 36);
        float hb = hv_x;
        p.vr_ag_rb = hb >= 0 ? hb + 1 : (p.vr_ag_rb ? p.vr_ag_rb : ragdollbone(g, p.origin) + 1);
        vector da = VR_AxeStick_Angles(vr_ag_df_fw, vr_ag_df_up);
        p.v_angle = da;
        p.angles = da;
        p.angles_x = -da_x;
        setorigin(p, vr_ag_df_o);
        p.vr_ag_rbo = '0 0 0'; // (taken again from here if it is ever not drawn)
        p.vr_ag_rbf = '0 0 0';
        return;
    }
    if(!p.vr_ag_rb || p.vr_ag_rbf == '0 0 0')
    {
        float b = p.vr_ag_rb ? p.vr_ag_rb - 1 : ragdollbone(g, p.origin);
        if(b < 0)
        {
            return; // (not a ragdoll: it stays)
        }
        vector fw0 = p.vr_ag_pfw != '0 0 0' ? p.vr_ag_pfw : '1 0 0';
        vector up0 = p.vr_ag_pup != '0 0 0' ? p.vr_ag_pup : '0 0 1';
        p.vr_ag_rb = b + 1;
        p.vr_ag_rbo = ragdollpoint(g, b, p.origin, 0);
        p.vr_ag_rbf = ragdollpoint(g, b, p.origin + fw0 * 16, 0);
        p.vr_ag_rbu = ragdollpoint(g, b, p.origin + up0 * 16, 0);
    }
    float bone = p.vr_ag_rb - 1;
    vector o = ragdollpoint(g, bone, p.vr_ag_rbo, 1);
    vector fw = normalize(ragdollpoint(g, bone, p.vr_ag_rbf, 1) - o);
    vector up = normalize(ragdollpoint(g, bone, p.vr_ag_rbu, 1) - o);
    up = normalize(up - fw * (up * fw));
    if(fw == '0 0 0' || up == '0 0 0')
    {
        return;
    }
    vector a = VR_AxeStick_Angles(fw, up);
    p.v_angle = a;
    p.angles = a;
    p.angles_x = -a_x;
    setorigin(p, o);
};

// The grab: `pl`'s `h` closed on its weapon. TRUE: a tug (begun, or not now); FALSE: take it at once (VR_AG_Yank).
float(entity g, entity pl, float h) VR_AG_TugBegin =
{
    if(g.vr_ag_tug_pl)
    {
        if(g.vr_ag_tug_pl == pl)
        {
            return VR_AG_TugJoin(g, pl, h); // (your other hand: it joins)
        }
        return TRUE; // (one at a time)
    }
    string why = VR_AG_TugEasy(g, pl);
    // A staggering or downed one's weapon still needs a tug (his decision of 2026-10-30), its thresholds eased
    // (vr_agrunt_tug_downed_scale; 0: the grab takes it at once, as before), the knockdown going on.
    float eased = 0;
    if((why == "staggering" || why == "knocked down") && cvar_hget(cvarh_vr_agrunt_tug_downed_scale) > 0)
    {
        eased = min(1, cvar_hget(cvarh_vr_agrunt_tug_downed_scale));
        why = "";
    }
    if(why != "")
    {
        vr_ag_tug_why = why;
        VR_AG_Log(sprintf("grabbed at once (%s)", why));
        return FALSE;
    }
    if(time < g.vr_ag_tug_cool)
    {
        return TRUE; // (it just shook you off)
    }
    g.vr_ag_tug_two = 0;
    g.vr_ag_tug_down = eased;
    // Facing you, its attack given up: standing its ground as a ragdoll, or (staggering, down) your hand holding its
    // weapon hand on its ragdoll, its arm pulling back.
    entity oldself = self;
    self = g;
    if(self.enemy != pl)
    {
        self.enemy = pl;
        FoundTarget();
    }
    self.vr_ag_act = 0;
    self = oldself;
    if(!(eased ? VR_AG_TugDownBegin(g, pl, h) : VR_AG_TugRagBegin(g, pl, h)))
    {
        // (No ragdoll, no room to stand, or it went down instead: the grab takes it.)
        g.vr_ag_tug_down = 0;
        vr_ag_tug_why = g.vr_knockdown ? "knocked down" : "no tug (no ragdoll or no room)";
        VR_AG_Log(sprintf("grabbed at once (%s)", vr_ag_tug_why));
        return FALSE;
    }
    g.vr_ag_tug_pl = pl;
    g.vr_ag_tug_hand = h;
    g.vr_ag_tug_since = time;
    g.vr_ag_tug_last = time;
    g.vr_ag_tug_prog = 0;
    g.vr_ag_tug_imp = 0;
    g.vr_ag_tug_held = 0;
    g.vr_ag_tug_peak = 0;
    g.vr_ag_tug_buzz = time;
    VR_AM_Row(g.vr_ag_kind);
    sound(g, CHAN_VOICE, vr_am_pain2, 1, ATTN_NORM); // (its monster's own pain: vr_armedmon.qc)
    if(eased)
    {
        VR_AG_Log(sprintf("tug on it %s begun by %s (hand %g): its pull back %.0f N, times eased %g", VR_AG_Shaky(g),
            pl.netname, h, VR_AG_TugDownBack(g), eased));
    }
    else
    {
        VR_AG_Log(sprintf("tug begun by %s (hand %g): break %g N or %g N s", pl.netname, h,
            cvar_hget(cvarh_vr_agrunt_tug_break_force), cvar_hget(cvarh_vr_agrunt_tug_break_impulse)));
    }
    return TRUE;
};

// The prop's frame while a tug is on (VR_AG_PropThink): TRUE when it placed the prop (or it is gone: won).
float(entity g, entity p) VR_AG_TugFrame =
{
    entity pl = g.vr_ag_tug_pl;
    if(!pl)
    {
        return FALSE;
    }
    // (Its grip let go: its release's time, kept in the player's own think. The grabbing bit itself reads clear in other
    // entities' thinks.) One of two hands letting go: the tug goes on with the other.
    if(g.vr_ag_tug_two)
    {
        float h2 = VR_AG_OtherHand(g.vr_ag_tug_hand);
        if(!VRIsHandEmpty(pl, h2) || VR_HandGrabUtil_Impl_GetLastReleaseTime(pl, h2) > g.vr_ag_tug_since2)
        {
            VR_AG_TugLetGoOne(g, pl, h2, "let go");
        }
    }
    float h = g.vr_ag_tug_hand;
    if(pl.health <= 0 || !VRIsHandEmpty(pl, h) || VR_HandGrabUtil_Impl_GetLastReleaseTime(pl, h) > g.vr_ag_tug_since)
    {
        if(pl.health > 0 && g.vr_ag_tug_two)
        {
            VR_AG_TugPromote(g, pl, "let go");
            h = g.vr_ag_tug_hand;
        }
        else
        {
            VR_AG_TugEnd(g, FALSE, "let go");
            return FALSE;
        }
    }
    // Your other fist striking it: torn free into the holding hand (vr_agrunt_tug_strike). Here, not only in
    // VR_AG_WeaponStruck: VR_AG_PropThink never reaches that while a tug is on (this returns TRUE), so a real punch went
    // through the gun (his checklist answer 2026-11-01: "it didn't punch it out at all"; test 92 called it directly).
    if(VR_AG_TugStrike(g, p))
    {
        return TRUE;
    }
    if(VR_AG_HealthShare(g) <= cvar_hget(cvarh_vr_agrunt_tug_hurt))
    {
        float hh = VR_AG_TugWinHand(g, pl);
        VR_AG_TugEnd(g, TRUE, "badly hurt");
        VR_AG_Yank(g, pl, hh);
        return TRUE;
    }
    return VR_AG_TugRagFrame(g, p, pl, h);
};

// The grab's way in (VR_AG_PropHandTouch): a tug, or the weapon yours at once.
void(entity g, entity pl, float h) VR_AG_GrabBy =
{
    if(VR_AG_TugBegin(g, pl, h))
    {
        return;
    }
    VR_AG_Yank(g, pl, h);
};

// A blow's least damage on its gun hand to disarm it now (half while it is shaky).
float(entity g) VR_AG_DisarmDamage =
{
    float need = cvar_hget(cvarh_vr_agrunt_disarm_damage);
    return VR_AG_Shaky(g) != "" ? need * 0.5 : need;
};

// Its melee blow under way (a punch, a swing, the chainsaw's thrust: vr_armedgrunt_pose.qc; a gun's butt: its swing
// frames): parried as a melee monster's (combat.qc VR_Parry_MeleeMonster).
float(entity e) VR_AG_Striking =
{
    return e.vr_ag_act != 0 || (e.vr_ag_swing_at > 0 && time - e.vr_ag_swing_at < 0.6);
};

// combat.qc VR_Parry: `targ`'s weapon parried `attacker`'s blow.
void(entity attacker, entity targ) VR_AG_Parried =
{
    if(attacker.vr_ag)
    {
        vr_ag_parried_n = vr_ag_parried_n + 1;
        VR_AG_Log(sprintf("its blow parried (%s)", attacker.vr_ag_prop ? attacker.vr_ag_prop.netname : "its fist"));
    }
    if(!attacker.vr_ag || !attacker.vr_ag_prop || attacker.health <= 0 || attacker.vr_eshove_striking ||
       !VR_AG_Striking(attacker))
    {
        return; // (its weapon's swing only: not its shove)
    }
    float chance = cvar_hget(cvarh_vr_agrunt_parry_disarm) * (vr_parry_welltimed ? 1.5 : 1);
    if(random() < chance)
    {
        // (two weapons: the striking hand's; both swinging, either)
        float left = attacker.vr_ag_act_hand == 1 || (attacker.vr_ag_act_hand == 2 && random() < 0.5);
        if(attacker.vr_ag_prop2 && !attacker.vr_ag_tug_pl && left != attacker.vr_ag_prop.vr_ag_left)
        {
            VR_AG_Swap(attacker);
        }
        VR_AG_DisarmBy(attacker, targ, attacker.origin - targ.origin, 120, "its blow parried");
    }
};

// ----------------------------------------------------------------------------
// Blows and throws on the held weapon; every disarm's sound
// ----------------------------------------------------------------------------

void() VR_AG_PlayPrecache =
{
    precache_sound("zombie/z_hit.wav"); // (its headbutt)
    float i;
    for(i = 1; i <= 4; i++)
    {
        precache_sound(sprintf("vr/phys/metal_h%g.wav", i));
        precache_sound(sprintf("vr/phys/metal_m%g.wav", i));
        precache_sound(sprintf("vr/phys/wood_h%g.wav", i));
    }
};

float vr_ag_tries; // disarm attempts (VR_AG_DisarmTry; the tests)

// Who threw `e` (a weapon, a prop, a gib) at its weapon: its owner when a player (world: not known).
// Who threw `e`: a player or a monster (a grunt's thrown thing disarms another grunt in infighting: the player is not
// special; DYNAMIC_SPAWNS.md, the 2026-11-09 audit's quick wins).
entity(entity e) VR_AG_Thrower = { return (e.owner.flags & (FL_CLIENT | FL_MONSTER)) ? e.owner : world; };

// Every try at its weapon, whatever came of it (his notes 2026-11-01 18:03, 18:37: a shot or a parried strike on one
// that hadn't seen you left it calm). It turns on whoever tried (VR_Provoke: you unseen or with notarget too, it knows
// what was done to it); knocked out of its hand, it staggers on its feet for vr_agrunt_disarm_stagger s (the parry's
// flinch, VR_Parry_InterruptFor; never a ragdoll: a fall drops what the other hand holds, VR_AG_FallDrop). `by` world
// (the tools', the tests' own disarms): nothing.
void(entity g, entity by, float outcome) VR_AG_DisarmTry =
{
    g.vr_ag_struck_at = time; // (lying: the blow's knock on its body is no fall's impact, VR_AG_FallImpact)
    if(!by || by == g || g.health <= 0)
    {
        return;
    }
    vr_ag_tries = vr_ag_tries + 1;
    VR_Provoke(g, by);
    float stagger = min(3, max(0, cvar_hget(cvarh_vr_agrunt_disarm_stagger)));
    if(outcome == VR_AG_TRY_DISARMED && stagger > 0)
    {
        vr_parry_noragdoll = TRUE;
        VR_Parry_InterruptFor(g, by, stagger);
        vr_parry_noragdoll = FALSE;
    }
};

// A disarm's sound: the weapon knocked loose (a gun heavy metal, a blade metal, Mjolnir wood), and its grunt.
void(entity g, float wid) VR_AG_DisarmFeedback =
{
    if(wid < 0 || wid == WID_FIST)
    {
        return;
    }
    string kind = VR_AG_Class(wid) != VR_AG_CLASS_MELEE ? "metal_h" : (wid == WID_MJOLNIR ? "wood_h" : "metal_m");
    vr_ag_feedback = sprintf("vr/phys/%s%g.wav", kind, 1 + min(3, floor(random() * 4)));
    sound(g, CHAN_AUTO, vr_ag_feedback, 1, ATTN_NORM);
    vr_ag_feedbacks = vr_ag_feedbacks + 1;
    if(time >= g.vr_ag_snd_at)
    {
        g.vr_ag_snd_at = time + 0.6;
        sound(g, CHAN_VOICE, VR_AM_Pain(g), 1, ATTN_NORM);
    }
    VR_AG_Log(sprintf("disarm feedback: %s", vr_ag_feedback));
};

// The least distance between the segments a0..a1 and b0..b1 (the nearest points: a0 + (a1 - a0) x vr_ag_seg_s, b0 +
// (b1 - b0) x vr_ag_seg_t).
float vr_ag_seg_s;
float vr_ag_seg_t;
float(vector a0, vector a1, vector b0, vector b1) VR_AG_SegDist =
{
    vector d1 = a1 - a0;
    vector d2 = b1 - b0;
    vector r = a0 - b0;
    float a = d1 * d1;
    float e = d2 * d2;
    float f = d2 * r;
    float s = 0;
    float t = 0;
    vr_ag_seg_s = 0;
    vr_ag_seg_t = 0;
    if(a <= 0.001 && e <= 0.001)
    {
        return vlen(r);
    }
    if(a <= 0.001)
    {
        t = min(1, max(0, f / e));
    }
    else
    {
        float c = d1 * r;
        if(e <= 0.001)
        {
            s = min(1, max(0, -c / a));
        }
        else
        {
            float b = d1 * d2;
            float den = a * e - b * b;
            s = den != 0 ? min(1, max(0, (b * f - c * e) / den)) : 0;
            t = (b * s + f) / e;
            if(t < 0)
            {
                t = 0;
                s = min(1, max(0, -c / a));
            }
            else if(t > 1)
            {
                t = 1;
                s = min(1, max(0, (b - c) / a));
            }
        }
    }
    vr_ag_seg_s = s;
    vr_ag_seg_t = t;
    return vlen(a0 + d1 * s - (b0 + d2 * t));
};

// VR_AG_DrawnFrame's: the held weapon's origin as drawn (vr_ag_df_o: vr_armedgrunt_defs.qc),
vector vr_ag_df_fw;  // ... its forward,
vector vr_ag_df_up;  // ... its up
float vr_ag_wr;      // VR_AG_WeaponLine's: half the weapon's drawn thickness (its shot volume's radius)

// Where the held weapon `p` is drawn now (the engine's one "where is this monster's weapon": hitreact::drawnWeapon, alive
// on its posed hands or on its ragdoll's hand; a listen server's own view): vr_ag_df_o, _fw, _up. FALSE: not drawn
// lately (a dedicated server, out of view): the server's own place and turn.
float(entity p) VR_AG_DrawnFrame =
{
    if(agdrawnframe(p)) // (its origin, forward and up into vr_ag_df_o, _fw, _up)
    {
        return TRUE;
    }
    makevectors(p.v_angle);
    vr_ag_df_o = p.origin;
    vr_ag_df_fw = v_forward;
    vr_ag_df_up = v_up;
    return FALSE;
};

// The held weapon `p`'s line as drawn now (VR_AG_DrawnFrame): its drawn model's two ends along its forward (vr_ag_wa
// the back, vr_ag_wb the front: the weapon's own length, held::drawnExtent), vr_ag_wr half its thickness. No drawn
// surface: 8 units behind its grip to its muzzle (one too near: 24 ahead), 3 thick, as before.
// Its line on its drawn frame is kept for the rest of the server frame (the shots, the parries and the strikes ask for
// each weapon's several times a frame): vr_ag_wl_frame the frame it was worked out in (framecount + 1; 0 none). The
// line from its own origin (not drawn) is worked out each time. A kept line also gives back the drawn frame
// (vr_ag_df_o, _fw, _up) and the extent's ends (vr_ag_wx_back, _front) it was worked out from, so a caller reading those
// after VR_AG_WeaponLine sees this weapon's, not the last one asked.
.float vr_ag_wl_frame;
.vector vr_ag_wl_a;
.vector vr_ag_wl_b;
.float vr_ag_wl_r;
.vector vr_ag_wl_o;
.vector vr_ag_wl_fw;
.vector vr_ag_wl_up;
.vector vr_ag_wl_xb;
.vector vr_ag_wl_xf;
void(entity p) VR_AG_WeaponLine =
{
    if(p.vr_ag_wl_frame == framecount + 1)
    {
        vr_ag_wa = p.vr_ag_wl_a;
        vr_ag_wb = p.vr_ag_wl_b;
        vr_ag_wr = p.vr_ag_wl_r;
        vr_ag_df_o = p.vr_ag_wl_o;
        vr_ag_df_fw = p.vr_ag_wl_fw;
        vr_ag_df_up = p.vr_ag_wl_up;
        vr_ag_wx_back = p.vr_ag_wl_xb;
        vr_ag_wx_front = p.vr_ag_wl_xf;
        return;
    }
    float drawn = VR_AG_DrawnFrame(p);
    vector f = vr_ag_df_fw;
    vector u = vr_ag_df_up;
    vector rt = [f_y * u_z - f_z * u_y, f_z * u_x - f_x * u_z, f_x * u_y - f_y * u_x]; // (forward x up: its right)
    vector size = agweaponextent(p); // (its ends into vr_ag_wx_back, _front)
    if(size_y > 1)
    {
        vector lo = vr_ag_wx_back;
        vector hi = vr_ag_wx_front;
        vr_ag_wa = vr_ag_df_o + vr_ag_df_fw * lo_x - rt * lo_y + vr_ag_df_up * lo_z;
        vr_ag_wb = vr_ag_df_o + vr_ag_df_fw * hi_x - rt * hi_y + vr_ag_df_up * hi_z;
        vr_ag_wr = max(0.5, size_x);
        if(drawn)
        {
            p.vr_ag_wl_frame = framecount + 1;
            p.vr_ag_wl_a = vr_ag_wa;
            p.vr_ag_wl_b = vr_ag_wb;
            p.vr_ag_wl_r = vr_ag_wr;
            p.vr_ag_wl_o = vr_ag_df_o;
            p.vr_ag_wl_fw = vr_ag_df_fw;
            p.vr_ag_wl_up = vr_ag_df_up;
            p.vr_ag_wl_xb = lo;
            p.vr_ag_wl_xf = hi;
        }
        return;
    }
    makevectors(p.v_angle);
    vector m = weaponmuzzle(p);
    if(vlen(m - p.origin) < 8 || vlen(m - p.origin) > 80)
    {
        m = p.origin + v_forward * 24;
    }
    vr_ag_wa = p.origin - normalize(m - p.origin) * 8;
    vr_ag_wb = m;
    vr_ag_wr = 3; // (vr_armedgrunt_shot.qc VR_AGS_W_RADIUS)
};

// `pl`'s hand `h`'s own swing now (m/s): the fastest of its grip and far end in the play space, against the head (the
// melee's tracking: vr_melee.qc VR_Melee_Track, this frame's or the last); 0 untracked or stale.
float(entity pl, float h) VR_AG_SwingSpeed =
{
    entity was = self;
    self = pl;
    entity b = VR_Blow(h);
    self = was;
    if(b.mh_thing == VR_MTHING_NONE || fabs(time - b.mh_time) > 0.2) // (its clock: the player's, a frame or so ahead)
    {
        return 0;
    }
    return b.mh_fast;
};

// ... its swing the frame before (0 untracked or stale): a strike is swung over frames, a jolt is one frame.
float(entity pl, float h) VR_AG_SwingBefore =
{
    entity was = self;
    self = pl;
    entity b = VR_Blow(h);
    self = was;
    if(b.mh_thing == VR_MTHING_NONE || fabs(time - b.mh_time) > 0.2)
    {
        return 0;
    }
    return max(vlen(b.mh_vfar_last), vlen(b.mh_vgrip_last));
};

// Whether `pl`'s hand `h` can strike (vr_agrunt_parry_strikes_only, VR_AG_StrikeFrame's disarms): it holds something
// (a weapon, a carried thing), or it is an empty hand closed into a fist (its grip held). An open hand is a reach, never a
// blow on its weapon (his note vrragtriggers_2026-10-24_23-01-03: it parried his hand reaching for its barrel).
float(entity pl, float h) VR_AG_HandStrikes =
{
    return !VRIsHandEmpty(pl, h) || VR_HandGrabUtil_IsHandGrabbing(pl, h);
};

// A player's hand (or what it holds), met with the held weapon `p` (VR_AG_StrikeMeets' test),
// going at `scale` times vr_agrunt_disarm_swing or more, deliberately: its speed (m/s; 0 none).
float(entity g, entity p, entity pl, float h, float scale) VR_AG_StrikeSpeed =
{
    float last = VR_HAND_FIELD(pl, h, vr_ag_knock_at, vr_ag_knock_off);
    if(time - last < 0.3)
    {
        VR_AG_Log(sprintf("a blow on its weapon: this hand knocked one %.2f s ago (under 0.3)", time - last));
        return FALSE;
    }
    vector hv = VRGetEntHandVelU(pl, h); // (the blow's way: against you, units per second)
    // Its speed: the hand's (or its weapon's far end's) own swing, in your play space against your head (your melee's,
    // vr_melee.qc VR_Melee_Track), not its world velocity: a push (a blow's knockback, a parry's, a shotgun's) or a step
    // carries your idle hands along at 5 m/s and more, and the world velocity less yours (a frame behind) read as a
    // blow (vrtesthall_2026-10-24_03-12-16: hands still against the head, 0.0-0.5 m/s, pushed at 4.9 m/s: dropped).
    float sp = VR_AG_SwingSpeed(pl, h);
    if(p.vr_ag_was_n >= 1 && frametime > 0 && vlen(p.origin - p.vr_ag_was_o1) < 32)
    {
        // A blow is the fist against the weapon: a hand going with a swing (closing on it to catch it, his notes
        // vrragtriggers_2026-10-27_01-41-43) is no blow on it however fast both go; in real time (bullet time's hands
        // are the game's, sped up). (The hand's velocity is metres a second against you: in world units with yours
        // added, as the weapon's; read as units it was 1/38 of the swing, every punch "under" the need: his notes
        // vrtesthall_2026-11-01_22-56-18 .. 23-08-32.)
        vector wv = (p.origin - p.vr_ag_was_o1) * (1 / frametime);
        float rel = vlen(hv + pl.velocity - wv) * (vr_hand_time_scale > 0 ? vr_hand_time_scale : 1) / VRMetersToUnits();
        sp = min(sp, rel);
    }
    float need = cvar_hget(cvarh_vr_agrunt_disarm_swing) * scale;
    float deliberate = cvar_hget(cvarh_vr_agrunt_disarm_deliberate);
    if(g.vr_knockdown == 1 && !g.vr_kd_stagger)
    {
        // (down with it in its hand: it doesn't resist a blow; but a real one, vr_agrunt_down_swing: a hand reaching
        // for it at 0.8 m/s knocked it away, vrragtriggers_2026-10-24_23-04-09)
        need = min(need, deliberate ? max(VR_AG_DOWN_SWING, cvar_hget(cvarh_vr_agrunt_down_swing)) : VR_AG_DOWN_SWING);
    }
    if(sp < need)
    {
        if(sp > need * 0.5 || cvar_hget(cvarh_vr_agrunt_debug) >= 2)
        {
            VR_AG_Log(sprintf("a blow of %.1f m/s on its weapon (%.1f in the world): under %g", sp,
                vlen(hv) / VRMetersToUnits(), need));
        }
        return FALSE;
    }
    if(deliberate)
    {
        // A deliberate strike (vr_agrunt_disarm_deliberate): a closed fist or something held (never an open hand
        // reaching), swung over two frames (a push's or a teleport's one-frame jolt of an idle hand against the head is
        // no swing: its tracker read 11.6 m/s on still mock hands as the shove threw the player), not in a tug.
        string no = "";
        if(!VR_AG_HandStrikes(pl, h))
        {
            no = "an open hand";
        }
        else if(VR_AG_SwingBefore(pl, h) < need * 0.5)
        {
            no = sprintf("a jolt, %.1f m/s the frame before", VR_AG_SwingBefore(pl, h));
        }
        if(no != "")
        {
            VR_AG_Log(sprintf("a blow of %.1f m/s on its weapon: not a strike (%s)", sp, no));
            return FALSE;
        }
    }
    return max(0.01, sp);
};

// A player's hand (or what it holds) striking the held weapon `p` (vr_ag_wa..vr_ag_wb: VR_AG_WeaponLine) at `scale`
// times vr_agrunt_disarm_swing or more, deliberately: its speed (m/s; 0 none). Met: its swept points
// and a carried prop's size against its weapon and its hand (VR_AG_StrikeMeets).
float(entity g, entity p, entity pl, float h, float scale) VR_AG_HandStrikeSpeed =
{
    if(VR_AG_StrikeMeets(g, p, pl, h) < 0)
    {
        return FALSE;
    }
    VR_AG_WeaponLine(p);
    return VR_AG_StrikeSpeed(g, p, pl, h, scale);
};

// `pl`'s hand `h` knocked a weapon out (a strike on it, a punch in a tug): its knock's time (VR_AG_StrikeSpeed's 0.3 s
// before that hand strikes again) and the knock felt in it (the disarm's sound: VR_AG_DisarmFeedback).
void(entity pl, float h) VR_AG_StrikeKnock =
{
    if(h == cVR_MainHand) { pl.vr_ag_knock_at = time; } else { pl.vr_ag_knock_off = time; }
    entity was = self;
    self = pl;
    VR_Haptic_KnockOut(h);
    self = was;
};

// In a tug (vr_agrunt_tug_strike), the tug's player's free hand striking its weapon at vr_agrunt_tug_strike_swing of the
// disarm swing: torn free into the holding hand. TRUE: so.
float(entity g, entity p) VR_AG_TugStrike =
{
    entity pl = g.vr_ag_tug_pl;
    if(!cvar_hget(cvarh_vr_agrunt_tug_strike) || g.vr_ag_prop != p || g.vr_ag_tug_two || !(pl.flags & FL_CLIENT))
    {
        return FALSE;
    }
    float h = g.vr_ag_tug_hand;
    float oh = VR_AG_OtherHand(h);
    VR_AG_WeaponLine(p);
    float sp = VR_AG_HandStrikeSpeed(g, p, pl, oh, max(0, cvar_hget(cvarh_vr_agrunt_tug_strike_swing)));
    if(sp <= 0)
    {
        return FALSE;
    }
    VR_AG_StrikeKnock(pl, oh);
    VR_AG_TugEnd(g, TRUE, sprintf("struck free by your other fist (%.1f m/s)", sp));
    VR_AG_Yank(g, pl, h); // (its sound: VR_AG_DisarmFeedback)
    return TRUE;
};

// VR_AG_Disarm in a tug (vr_agrunt_tug_strike): any blow that knocks it out of its hand (a body blow by its grip, a
// strike) puts it in the tug's holding hand instead of flinging it. TRUE: so.
float(entity g, string why) VR_AG_TugDisarm =
{
    entity pl = g.vr_ag_tug_pl;
    if(!pl || !cvar_hget(cvarh_vr_agrunt_tug_strike))
    {
        return FALSE;
    }
    float h = VR_AG_TugWinHand(g, pl); // (both holding: the last placed)
    VR_AG_TugEnd(g, TRUE, sprintf("%s, into your holding hand", why));
    VR_AG_Yank(g, pl, h);
    return TRUE;
};

// VR_AG_PropThink, every frame: a hard blow or a throw on the held weapon itself (not only its hand). TRUE: disarmed.
float(entity g, entity p) VR_AG_WeaponStruck =
{
    if(VR_AGS_Frame(g, p)) // (shot out of its hand: vr_armedgrunt_shot.qc)
    {
        return TRUE;
    }
    if(g.vr_ag_tug_pl)
    {
        return VR_AG_TugStrike(g, p); // (the other fist's strike: vr_agrunt_tug_strike)
    }
    if(VR_AG_StrikeFrame(g, p)) // (a deliberate strike on its weapon or hand, else the parry: vr_armedgrunt_parry.qc)
    {
        return TRUE;
    }
    if(!cvar_hget(cvarh_vr_agrunt_disarm) || !g.vr_ag_prop)
    {
        return FALSE;
    }
    VR_AG_WeaponLine(p);
    vector mid = (vr_ag_wa + vr_ag_wb) * 0.5;
    entity e = findradius(mid, 48);
    while(e)
    {
        if((e.classname == "thrown_weapon" || e.vr_gib || (e.flags & FL_FORCEGRABBABLE)) && e != p &&
           !(e.flags & (FL_CLIENT | FL_MONSTER)) && time - e.vr_ag_knock_at > 0.5 && vlen(e.velocity) >= 100 &&
           e.velocity * (mid - e.origin) > 0)
        {
            float reach = 5 + min(12, max(4, max(e.maxs_x, e.maxs_z)));
            if(VR_AG_SegDist(e.origin - e.velocity * frametime, e.origin, vr_ag_wa, vr_ag_wb) <= reach)
            {
                float mom = VR_Thrown_Mass(e) * vlen(e.velocity) / VRMetersToUnits();
                e.vr_ag_knock_at = time;
                if(mom >= cvar_hget(cvarh_vr_agrunt_disarm_throw))
                {
                    VR_AG_DisarmBy(g, VR_AG_Thrower(e), e.velocity, 120, sprintf("a %s thrown at its weapon, %.1f kg m/s",
                        e.classname, mom));
                    return TRUE;
                }
                VR_AG_Log(sprintf("a %s thrown at its weapon, %.1f kg m/s: under %g", e.classname, mom,
                    cvar_hget(cvarh_vr_agrunt_disarm_throw)));
                VR_AG_DisarmTry(g, VR_AG_Thrower(e), VR_AG_TRY_HELD);
            }
        }
        e = e.chain;
    }
    return FALSE;
};

// ----------------------------------------------------------------------------
// Falls
// ----------------------------------------------------------------------------

// Whether a player's hand holds `g`'s ragdoll (lifted, judo-thrown and still held: vr_foegrab_throw.qc HandOver).
float(entity g) VR_AG_HeldByHand =
{
    entity pl = VR_Player();
    while(pl)
    {
        if(VR_Carry_Held(pl, cVR_OffHand) == g || VR_Carry_Held(pl, cVR_MainHand) == g)
        {
            return TRUE;
        }
        pl = VR_NextPlayer(pl);
    }
    return FALSE;
};

// Whether a hand holding `g` is swinging now (its own move, not your walk: the engine's dragged(), vr_ragimpact_drag_speed);
// vr_ag_hold_at: where a hand holding it is (the swinging one's if any).
vector vr_ag_hold_at;
float(entity g) VR_AG_HeldSwinging =
{
    float fast = max(0, cvar_hget(cvarh_vr_ragimpact_drag_speed));
    for(entity pl = VR_Player(); pl != world; pl = VR_NextPlayer(pl))
    {
        for(float h = 0; h < 2; h++)
        {
            float hand = h ? cVR_MainHand : cVR_OffHand;
            if(VR_Carry_Held(pl, hand) == g)
            {
                vr_ag_hold_at = VRGetEntHandPos(pl, hand);
                if(vlen(VRGetEntHandVel(pl, hand)) >= fast)
                {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
};

// The level struck ragdoll `m` (the engine's strikes, VR_RagImpact_Hit: vr_box3d_step.cpp noteRagImpacts, the same contact
// the smash sound and blood and the impacts' damage use): xInto the way out of the surface, xSpeed the part's speed into
// it. Kept as two slices of time so the rule sees the last window's whatever the frame rate.
void(entity m, float xSpeed, vector xInto) VR_AG_FallTouch =
{
    if(time - m.vr_ag_fall_into_at >= max(0.001, cvar_hget(cvarh_vr_agrunt_fall_impact_window)))
    {
        m.vr_ag_fall_into_was = m.vr_ag_fall_into;
        m.vr_ag_fall_into = '0 0 0';
        m.vr_ag_fall_into_at = time;
    }
    m.vr_ag_fall_into = m.vr_ag_fall_into + xInto * xSpeed;
};

// The impact rule (vr_agrunt_fall_keep_impact; his notes vrragtriggers_2026-10-27_01-43-08, 01-45-10: lifted, thrown or
// shoulder-charged "they should try to hold on; the impact with the ground should make them drop it"; 2026-11-02: "check
// for the body losing speed after it has been at a certain high enough speed for a while", walls too). Its body's velocity
// each frame from its fall's start. It lets go only when all of these hold:
// - Sustained speed: it kept up vr_agrunt_fall_impact_speed m/s for vr_agrunt_fall_impact_time s (seconds, not frames; a
//   dip no longer than the window is the origin's jitter). A shove's or your body's one-frame spike never gets there.
// - A real stop: within vr_agrunt_fall_impact_window s of such a moment its velocity changed by `most` units/s or more.
// - Real contact: the level struck it within the window (VR_AG_FallTouch) from where it was going, and the change points
//   out of that surface. A hand reversing in mid-air strikes nothing; a floor under a body skidding along it, or a limb
//   brushing the floor as you swing it, doesn't stop it that way.
// - Or your hands' bang: a holding hand swung within VR_AG_FALL_DRAG, your grip driving it at vr_agrunt_fall_impact_held of
//   `most` or more (at most about 5 m/s: the grip's force), and that hand reaches a surface ahead (within
//   vr_agrunt_fall_impact_reach units, facing the way it was going). Your hand stops dead there and the body stops at your
//   hand, softly and without striking anything hard (measured, ROUND23.md: 4.4 m/s to 2.4 over 0.3 s, the only strike on
//   the wall at 1.7 m/s), so the loss can't be read off the body. A hand reversed in mid-air has no surface ahead of it.
// So flung, carried, swung about, falling: it holds on; thrown down, banged on a wall or the floor by your hands too, it
// lets go. Not just after a blow on it or a try at its weapon (`struck`). `most` 0: never. "" kept, else why it lets go.
string(entity g, float most) VR_AG_FallImpact =
{
    float u = VRMetersToUnits();
    float held = VR_AG_HeldByHand(g);
    if(frametime > 0)
    {
        vector v = (g.origin - g.vr_ag_fall_last) * (1 / frametime);
        float speed = vlen(v);
        g.vr_ag_fall_v = v;
        g.vr_ag_fall_down = max(g.vr_ag_fall_down, -v_z);
        vector flat = v;
        flat_z = max(0, flat_z);
        g.vr_ag_fall_fling = max(g.vr_ag_fall_fling, vlen(flat));
        float window = max(0.001, cvar_hget(cvarh_vr_agrunt_fall_impact_window));
        // (A blow on it or a try at its weapon as it lies knocks its body: no fall's impact. A disarm of one hand's weapon
        // lying took the other's too, his note vrragtriggers_2026-11-01_18-21-43: the punch's knock met the floor.)
        float struck = time - g.vr_ag_struck_at < VR_AG_FALL_STRUCK;
        // (The slices older than the window are gone; the older slice's strikes are at most two windows old.)
        vector into = (time - g.vr_ag_fall_into_at < window * 2) ? g.vr_ag_fall_into : '0 0 0';
        if(time - g.vr_ag_fall_into_at < window)
        {
            into = into + g.vr_ag_fall_into_was;
        }
        if(held && VR_AG_HeldSwinging(g))
        {
            g.vr_ag_fall_swung = time;
        }
        float share = max(0.01, cvar_hget(cvarh_vr_agrunt_fall_impact_held));
        float swung = (held && g.vr_ag_fall_swung > 0 && time - g.vr_ag_fall_swung <= VR_AG_FALL_DRAG);
        float lost = 0;
        if(!struck)
        {
            vector out = (into != '0 0 0') ? normalize(into) : '0 0 0';
            for(float k = 0; k < VR_AG_FALL_RING; k++)
            {
                float at = g.vr_ag_fall_ring_t[k];
                if(at > 0 && at < time && time - at <= window + frametime * 0.5)
                {
                    vector was = g.vr_ag_fall_ring[k];
                    vector change = v - was;
                    float big = vlen(change);
                    float along = change * out;
                    // (In your hand only what the surface took counts: a reversal's change along it, a limb brushing the
                    // floor, is your hand's.)
                    float worth = held ? along : big;
                    if(worth > lost && into != '0 0 0' && was * out < 0 && along >= 0.5 * big)
                    {
                        lost = worth;
                    }
                    if(swung && most > 0 && vlen(was) / share > lost && vlen(was) >= most * share)
                    {
                        vector way = normalize(was);
                        traceline(vr_ag_hold_at, vr_ag_hold_at + way * max(0, cvar_hget(cvarh_vr_agrunt_fall_impact_reach)), TRUE, g);
                        // (Your hand already in the wall, the body still driving at it: as good.)
                        if(trace_startsolid || (trace_fraction < 1 && way * trace_plane_normal <= -0.5))
                        {
                            lost = vlen(was) / share; // (counts as a stop this hard: `most` once it goes fast enough)
                        }
                    }
                }
            }
            if(lost > g.vr_ag_fall_hit)
            {
                g.vr_ag_fall_hit = lost;
                g.vr_ag_fall_hit_at = time;
            }
        }
        // (Your grip drives a body slower than a throw: the share of its least speed too.)
        if(speed >= max(0, cvar_hget(cvarh_vr_agrunt_fall_impact_speed)) * u * (held ? share : 1))
        {
            g.vr_ag_fall_fast = g.vr_ag_fall_fast + frametime;
            g.vr_ag_fall_slow = 0;
            if(g.vr_ag_fall_fast >= cvar_hget(cvarh_vr_agrunt_fall_impact_time))
            {
                // (Paced by the window, so four slots span it at any frame rate.)
                if(time - g.vr_ag_fall_ring_t[g.vr_ag_fall_ring_n] >= window / (VR_AG_FALL_RING - 1) - 0.0001)
                {
                    g.vr_ag_fall_ring_n = g.vr_ag_fall_ring_n + 1 >= VR_AG_FALL_RING ? 0 : g.vr_ag_fall_ring_n + 1;
                    g.vr_ag_fall_ring[g.vr_ag_fall_ring_n] = v;
                    g.vr_ag_fall_ring_t[g.vr_ag_fall_ring_n] = time;
                }
            }
        }
        else
        {
            g.vr_ag_fall_slow = g.vr_ag_fall_slow + frametime;
            if(g.vr_ag_fall_slow > window)
            {
                g.vr_ag_fall_fast = 0;
            }
        }
        if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
        {
            VR_AG_Log(sprintf("fall: speed %.1f m/s (fast %.2f s), v %v, into %v, lost %.1f, hardest %.1f%s", speed / u,
                g.vr_ag_fall_fast, v * (1 / u), into, lost / u, g.vr_ag_fall_hit / u,
                strcat(strcat(held ? ", held" : "", swung ? " swung" : ""), struck ? ", struck" : "")));
        }
    }
    g.vr_ag_fall_held = held;
    g.vr_ag_fall_last = g.origin;
    if(most > 0 && g.vr_ag_fall_hit >= most)
    {
        return sprintf("it hit something at %.1f m/s", g.vr_ag_fall_hit / u);
    }
    return "";
};

// Knocked down (VR_AG_PropThink): TRUE when it let go (the prop gone). Staggering, still falling, getting up: it holds.
// Knocked down keeping its weapon (vr_agrunt_fall_keep): its fall measured each frame from its start (a stagger's too:
// it may go over), by the impact rule (VR_AG_FallImpact; the old height, speed and fling rules were retired 2026-11-02).
// "" gentle (it keeps it), else why it lets go.
string(entity g) VR_AG_FallViolent =
{
    if(g.vr_ag_fall_at != g.vr_kd_since)
    {
        g.vr_ag_fall_at = g.vr_kd_since;
        g.vr_ag_fall_last = g.origin;
        g.vr_ag_fall_down = 0;
        g.vr_ag_fall_fling = 0;
        g.vr_ag_fall_v = '0 0 0';
        g.vr_ag_fall_hit = 0;
        g.vr_ag_fall_hit_at = 0;
        g.vr_ag_fall_held = 0;
        g.vr_ag_fall_fast = 0;
        g.vr_ag_fall_slow = 0;
        g.vr_ag_fall_swung = 0;
        for(float k = 0; k < VR_AG_FALL_RING; k++)
        {
            g.vr_ag_fall_ring_t[k] = 0;
        }
        return "";
    }
    return VR_AG_FallImpact(g, max(0, cvar_hget(cvarh_vr_agrunt_fall_keep_impact)) * VRMetersToUnits());
};

// It lets go of what it holds (both hands), in a fall. `why`: the log's.
void(entity g, string why) VR_AG_FallLetGo =
{
    entity p = g.vr_ag_prop;
    vector at = g.origin + '0 0 4';
    if(vlen(p.origin - g.origin) < 48)
    {
        at = p.origin;
    }
    setorigin(p, at);
    makevectors('0 1 0' * random() * 360);
    vector vel = v_forward * (20 + random() * 30) + '0 0 60';
    float wid = g.vr_ag_wid;
    entity w = VR_AG_DropHeld(g, vel);
    g.vr_ag_own = w;
    g.vr_ag_disarms = g.vr_ag_disarms + 1;
    VR_AG_DisarmFeedback(g, wid);
    VR_AG_Log(sprintf("dropped its weapon as it fell (%s)", why));
    if(g.vr_ag_prop) // (its other hand's too)
    {
        makevectors('0 1 0' * random() * 360);
        setorigin(g.vr_ag_prop, vlen(g.vr_ag_prop.origin - g.origin) < 48 ? g.vr_ag_prop.origin : g.origin + '0 0 4');
        VR_AG_DropHeld(g, v_forward * (20 + random() * 30) + '0 0 60');
    }
};

float(entity g) VR_AG_FallDrop =
{
    if(!cvar_hget(cvarh_vr_agrunt_fall_drop) || g.vr_knockdown != 1)
    {
        return FALSE;
    }
    string why = "fell";
    if(cvar_hget(cvarh_vr_agrunt_fall_keep))
    {
        // A gentle fall keeps it in its hand (riding its hand's limb: VR_AG_RagProp); yours for a grab (at once: shaky) or
        // a blow (VR_AG_StrikeFrame) while it is down. A tug's stagger is the tug's.
        if(g.vr_ag_tug_pl)
        {
            return FALSE;
        }
        why = VR_AG_FallViolent(g);
        if(why == "" || g.vr_kd_stagger) // (a stagger never drops it: measured on, it may go over)
        {
            g.vr_ag_fall_kept = g.vr_kd_stagger ? g.vr_ag_fall_kept : 1;
            return FALSE;
        }
        g.vr_ag_fall_kept = 0;
    }
    else if(g.vr_kd_stagger || time - g.vr_kd_since < VR_AG_FALL_LETGO)
    {
        return FALSE;
    }
    VR_AG_FallLetGo(g, why);
    return TRUE;
};

// A landing that hurt it (VR_Monster_Fall: a drop past VR_SHOVE_SAFEFALL, its whole body coming down that fast): it lets go,
// whatever else it was doing. On its feet no ragdoll reads an impact at all: shoved off a ledge without being knocked down, it
// walks away hurt and armed (his note vrragtriggers_2026-11-03_02-18-13: "they never drop their weapon"). `vr_agrunt_fall_keep_impact`
// 0: never.
void(entity m, float xSpeed) VR_AG_FallLanded =
{
    float most = max(0, cvar_hget(cvarh_vr_agrunt_fall_keep_impact)) * VRMetersToUnits();
    if(!m.vr_ag || !m.vr_ag_prop || m.health <= 0 || !cvar_hget(cvarh_vr_agrunt_fall_drop) || !cvar_hget(cvarh_vr_agrunt_disarm) ||
       m.vr_ag_tug_pl || most <= 0 || xSpeed < most)
    {
        return;
    }
    VR_AG_FallLetGo(m, sprintf("it landed at %.1f m/s", xSpeed / VRMetersToUnits()));
};
// ----------------------------------------------------------------------------
// Reach
// ----------------------------------------------------------------------------

// Whether `g` can walk to the weapon `w`.
float(entity g, entity w) VR_AG_Reachable =
{
    if(pointcontents(w.origin) != CONTENT_EMPTY)
    {
        vr_ag_reach_why = "in a liquid";
        return FALSE; // (floating, sunk)
    }
    traceline(w.origin + '0 0 4', w.origin - '0 0 40', TRUE, w);
    if(trace_fraction == 1 || trace_startsolid || pointcontents(trace_endpos + '0 0 2') != CONTENT_EMPTY)
    {
        vr_ag_reach_why = "not on a floor";
        return FALSE;
    }
    vector end = trace_endpos;
    vector from = g.origin;
    from_z = g.origin_z + g.mins_z;
    vector flat = end - from;
    flat_z = 0;
    float n = ceil(vlen(flat) / VR_AG_PROBE);
    vector prev = from;
    float i;
    for(i = 1; i <= n; i++)
    {
        vector s = from + flat * (i / n);
        s_z = prev_z;
        traceline(s + '0 0 1' * (VR_AG_STEP + 12), s - '0 0 1' * (VR_AG_STEP + 22), TRUE, g);
        if(trace_fraction == 1 || trace_startsolid || fabs(trace_endpos_z - prev_z) > VR_AG_STEP ||
           pointcontents(trace_endpos + '0 0 2') != CONTENT_EMPTY)
        {
            vr_ag_reach_why = sprintf("a drop, a ledge or a liquid %.0f units on (%.0f to %.0f)", vlen(s - from), prev_z,
                trace_endpos_z);
            return FALSE;
        }
        vector here = trace_endpos;
        traceline(prev + '0 0 24', here + '0 0 24', TRUE, g);
        if(trace_fraction < 1)
        {
            vr_ag_reach_why = sprintf("a wall %.0f units on (%s)", vlen(here - from), trace_ent.classname);
            return FALSE;
        }
        prev = here;
    }
    vr_ag_reach_why = "a step at its end";
    return fabs(prev_z - end_z) <= VR_AG_STEP;
};

float(entity g, entity w) VR_AG_Shunned = { return g.vr_ag_shun == w && time < g.vr_ag_shun_until; };

// Its own weapon it dropped falling, to take back (world: none, taken, out of reach).
entity(entity g) VR_AG_OwnBack =
{
    entity w = g.vr_ag_own;
    if(!w || w.classname != "thrown_weapon" || VR_EnemyWeapons_Spared(w) || VR_AxeStick_Kept(w) || VR_AG_Shunned(g, w) ||
       !VR_AM_Takes(g, w.weapon) || vlen(w.origin - g.origin) > VR_AG_OWN_RANGE || !VR_AG_Reachable(g, w))
    {
        g.vr_ag_own = world;
        return world;
    }
    return w;
};

// It begins going for vr_ag_seek.
void(entity g) VR_AG_SeekStart =
{
    vector d = g.vr_ag_seek.origin - g.origin;
    d_z = 0;
    g.vr_ag_seek_since = time;
    g.vr_ag_seek_best = vlen(d);
    g.vr_ag_seek_prog = time;
    g.vr_ag_seek_check = time + 0.5;
};

// On its way to vr_ag_seek: FALSE when it gives it up (shunned for vr_agrunt_seek_shun s).
float(entity g) VR_AG_SeekCheck =
{
    entity w = g.vr_ag_seek;
    vector d = w.origin - g.origin;
    d_z = 0;
    if(vlen(d) < g.vr_ag_seek_best - 8)
    {
        g.vr_ag_seek_best = vlen(d);
        g.vr_ag_seek_prog = time;
    }
    string why = "";
    if(time - g.vr_ag_seek_prog > VR_AG_STUCK)
    {
        why = "no nearer";
    }
    else if(time - g.vr_ag_seek_since > cvar_hget(cvarh_vr_agrunt_seek_timeout))
    {
        why = "too long";
    }
    else if(time >= g.vr_ag_seek_check)
    {
        g.vr_ag_seek_check = time + 0.5;
        if(!VR_AG_Reachable(g, w))
        {
            why = "out of reach";
        }
    }
    if(why == "")
    {
        return TRUE;
    }
    g.vr_ag_shun = w;
    g.vr_ag_shun_until = time + cvar_hget(cvarh_vr_agrunt_seek_shun);
    if(g.vr_ag_own == w)
    {
        g.vr_ag_own = world;
    }
    VR_AG_Log(sprintf("gave up on %s (%s, %.1f s)", w.netname, why, time - g.vr_ag_seek_since));
    return FALSE;
};

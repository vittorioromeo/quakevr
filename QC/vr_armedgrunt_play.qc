//
//
//
// ----------------------------------------------------------------------------
// Armed grunts: disarming, drops and weapon reach (ROUND22.md, "Armed grunts: gameplay")
// ----------------------------------------------------------------------------
//
// - The tug-of-war (vr_agrunt_tug): your empty hand closing on its weapon doesn't simply take it. It holds on: the
//   weapon is drawn between its hand and yours, your hand is jerked towards it (haptics) and it fights back about every
//   vr_agrunt_tug_fight s (a gun: fires at you, 40%; a shove that pushes you off, 30%; a headbutt, 30%; a melee weapon
//   or its fists: a shove or a headbutt): a shove or a headbutt breaks your grip (it keeps the weapon; 0.8 s before you
//   can grab again). You win by keeping your hand pulled back vr_agrunt_tug_pull units or more from where you grabbed
//   (away from it) for vr_agrunt_tug_time s in all (easing off loses it half as fast as pulling gains it); hurt, it
//   lets go sooner (that time times its share of health, at least 0.35), and twice as fast while it flinches from a
//   hit. Letting go of the grip, or your hand ending 64 units from the weapon, ends it (lost).
// - A grab takes the weapon at once (VR_AG_Shaky, VR_AG_TugEasy): staggered or knocked down, stunned by a parry, down to
//   vr_agrunt_tug_hurt of its health, or taken unawares from behind (it isn't fighting you, you are behind it).
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

.entity vr_ag_tug_pl;     // the player tugging at its weapon (world: none)
.float vr_ag_tug_hand;    // ... with that hand
.float vr_ag_tug_since;   // ... since then
.float vr_ag_tug_prog;    // the pull's time so far (s)
.float vr_ag_tug_last;    // the tug's last frame (its step)
.vector vr_ag_tug_off;    // the hand's place from its grip as it grabbed
.vector vr_ag_tug_away;   // the way back from it (flat, unit)
.float vr_ag_tug_next;    // when it next fights back
.float vr_ag_tug_buzz;    // the hand's next jerk
.float vr_ag_tug_cool;    // no new tug before this (it just shook you off)
.float vr_ag_tug_wins;    // the tests: tugs you won,
.float vr_ag_tug_losses;  // ... lost
.float vr_ag_tug_fights;  // ... its fights back
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
float vr_ag_tug_stretch;  // the last tug frame's stretch (the tests)
.float vr_ag_tug_imp;     // the physical tug: its pull's force times time so far (N s)
.float vr_ag_tug_peak;    // ... its hardest pull (N; the tests)
.float vr_ag_tug_held;    // ... how long you have kept pulling above vr_agrunt_tug_pull_force (s; eased off below)
float vr_ag_tug_test_force; // the tests' pull (N) in place of the hold's (0: the hold's; VR_AG_TugBreak)
// Knocked down keeping its weapon (VR_AG_FallDrop, vr_agrunt_fall_keep): its fall measured from its start.
.float vr_ag_fall_at;     // the knockdown measured (its vr_kd_since)
.float vr_ag_fall_floor;  // ... the floor it stood on (z)
.vector vr_ag_fall_last;  // ... its body's place last frame
.float vr_ag_fall_drop;   // ... the most the floor under it fell below that (units)
.float vr_ag_fall_down;   // ... its body's fastest fall (units/s)
.float vr_ag_fall_fling;  // ... its body's fastest motion any way but down (units/s)
.float vr_ag_fall_kept;   // ... kept its weapon as it fell (the tests)
.vector vr_ag_fall_v;     // ... its body's velocity last frame (units/s; the impact rule)
.float vr_ag_fall_hit;    // ... its hardest impact so far (units/s lost in a frame, coming down; vr_agrunt_fall_keep_impact)
.float vr_ag_fall_hit_at; // ... when (the tests)
.float vr_ag_fall_held;   // ... held by your hands last frame (the tests)
float vr_ag_tug_force;    // ... the last frame's pull (N; the tests)
float vr_ag_parried_n;    // its blows your weapons parried (VR_AG_Parried; the tests)
float VR_AG_TUG_STAGGER = 0.35; // (was: s its stumble as you tear its weapon away; now vr_agrunt_tug_won_stumble)
float VR_AG_TUG_RECOVER = 0.2;  // s up again as you let go (or it wrenches free)
float VR_AG_DOWN_SWING = 0.8;   // m/s: a blow on its weapon knocks it away while it is down holding it (vr_agrunt_fall_keep)

float VR_AG_TUG_SLIP = 64;      // the hand this far from the weapon: the tug is over
float VR_AG_TUG_COOL = 0.8;     // s before a new grab after it shook you off
float VR_AG_TUG_BUTT = 6;       // a headbutt's damage (times vr_agrunt_damage_scale)
float VR_AG_FALL_LETGO = 0.4;   // s down before it lets go
float VR_AG_OWN_RANGE = 160;    // its own dropped weapon taken back this near
float VR_AG_STEP = 18;          // a step up or down it walks
float VR_AG_PROBE = 12;         // the way's probes, units apart
float VR_AG_STUCK = 2.5;        // s no nearer: given up

void() vr_ag_tughold;

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

// How long you must pull now (s).
float(entity g) VR_AG_TugNeed =
{
    float need = max(0, cvar_hget(cvarh_vr_agrunt_tug_time)) * min(1, max(0.35, VR_AG_HealthShare(g)));
    if(time < g.pain_finished)
    {
        need = need * 0.5;
    }
    return need;
};

void(entity g, float won, string why) VR_AG_TugEnd =
{
    entity pl = g.vr_ag_tug_pl;
    if(g.vr_ag_tug_rag)
    {
        // The physical tug's: your hand's hold let go, its stagger over in a moment (won: a short stumble, its grip
        // broken), then its AI again (vr_knockdown.qc VR_Knockdown_Think: recovered).
        g.vr_ag_tug_rag = 0;
        float stumble = won ? max(0, cvar_hget(cvarh_vr_agrunt_tug_won_stumble)) : VR_AG_TUG_RECOVER;
        ragdolltug(g, pl, g.vr_ag_tug_hand, 2, '0 0 0', stumble);
    }
    g.vr_ag_tug_pl = world;
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
    if(g.think == vr_ag_tughold && g.health > 0)
    {
        g.think = g.th_run;
        g.nextthink = time + 0.05;
    }
};

// The physical tug (vr_agrunt_tug_ragdoll): `g` stands its ground as an active ragdoll (a stagger that never goes over,
// up again only when the tug ends: vr_knockdown.qc), `pl`'s hand `h` holding its weapon: the hand holds its hand's limb
// at the weapon's lever (ragdolltug), its arms pull back (vr_agrunt_tug_strength). FALSE: it can't (no ragdoll, no
// room): the old tug.
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
    if(!ragdolltug(g, pl, h, 0, p.origin, strength, grip))
    {
        ragdolltug(g, pl, h, 2, '0 0 0', 0.1); // (up again at once; the grab takes it: staggering)
        return FALSE;
    }
    g.vr_ag_tug_rag = 1;
    return TRUE;
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
string(entity g, float f, float dt) VR_AG_TugBreak =
{
    float share = VR_AG_TugShare(g);
    float brk = max(1, cvar_hget(cvarh_vr_agrunt_tug_break_force)) * share;
    float need = max(0.01, cvar_hget(cvarh_vr_agrunt_tug_break_impulse)) * share;
    float least = max(0, cvar_hget(cvarh_vr_agrunt_tug_pull_force)) * share;
    float hold = max(0, cvar_hget(cvarh_vr_agrunt_tug_pull_time));
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

// The physical tug's frame (VR_AG_TugFrame): its pull on your hand's hold (the force between its hand and the weapon);
// a pull of vr_agrunt_tug_break_force, or force times time (counted above a tenth of it) of vr_agrunt_tug_break_impulse,
// breaks its grip (less hurt, half in pain): the weapon is yours. vr_agrunt_tug_hold_max s on, it wrenches it free.
float(entity g, entity p, entity pl, float h) VR_AG_TugRagFrame =
{
    float f = ragdolltug(g, pl, h, 1);
    if(f < 0)
    {
        VR_AG_TugEnd(g, FALSE, "torn away"); // (the hold lost: the limb left far behind your hand)
        return FALSE;
    }
    if(g.vr_knockdown == 1 && !g.vr_kd_stagger)
    {
        VR_AG_TugEnd(g, TRUE, "it went down"); // (no room to stand: down, it lets go)
        VR_AG_Yank(g, pl, h);
        return TRUE;
    }
    VR_AG_RagProp(g, p);
    float dt = min(0.1, time - g.vr_ag_tug_last);
    g.vr_ag_tug_last = time;
    if(vr_ag_tug_test_force > 0)
    {
        f = vr_ag_tug_test_force;
    }
    string won = VR_AG_TugBreak(g, f, dt);
    if(won != "")
    {
        VR_AG_TugEnd(g, TRUE, won);
        VR_AG_Yank(g, pl, h);
        return TRUE;
    }
    float brk = max(1, cvar_hget(cvarh_vr_agrunt_tug_break_force)) * VR_AG_TugShare(g);
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
        float gain = min(1, max(f / brk, g.vr_ag_tug_prog));
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
    if(!p.vr_ag_rb)
    {
        float b = ragdollbone(g, p.origin);
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
        return TRUE; // (one at a time)
    }
    string why = VR_AG_TugEasy(g, pl);
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
    if(cvar_hget(cvarh_vr_agrunt_tug_ragdoll))
    {
        // The physical tug: facing you, its attack given up, standing its ground as a ragdoll.
        entity oldself0 = self;
        self = g;
        if(self.enemy != pl)
        {
            self.enemy = pl;
            FoundTarget();
        }
        self.vr_ag_act = 0;
        self = oldself0;
        if(VR_AG_TugRagBegin(g, pl, h))
        {
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
            sound(g, CHAN_VOICE, vr_am_pain2, 1, ATTN_NORM);
            VR_AG_Log(sprintf("physical tug begun by %s (hand %g): break %g N or %g N s", pl.netname, h,
                cvar_hget(cvarh_vr_agrunt_tug_break_force), cvar_hget(cvarh_vr_agrunt_tug_break_impulse)));
            return TRUE;
        }
        if(g.vr_knockdown)
        {
            vr_ag_tug_why = "knocked down";
            return FALSE; // (it went down instead: the grab takes it)
        }
    }
    entity p = g.vr_ag_prop;
    vector hand = VRGetEntHandPos(pl, h);
    g.vr_ag_tug_pl = pl;
    g.vr_ag_tug_hand = h;
    g.vr_ag_tug_since = time;
    g.vr_ag_tug_last = time;
    g.vr_ag_tug_prog = 0;
    g.vr_ag_tug_off = hand - p.origin;
    vector away = hand - g.origin;
    away_z = 0;
    if(vlen(away) < 1)
    {
        away = pl.origin - g.origin;
        away_z = 0;
    }
    g.vr_ag_tug_away = normalize(away);
    float fight = cvar_hget(cvarh_vr_agrunt_tug_fight);
    g.vr_ag_tug_next = time + fight * (0.5 + random() * 0.7);
    g.vr_ag_tug_buzz = time;
    entity oldself = self;
    self = g;
    if(self.enemy != pl)
    {
        self.enemy = pl;
        FoundTarget();
    }
    VR_AM_Row(self.vr_ag_kind);
    sound(self, CHAN_VOICE, vr_am_pain2, 1, ATTN_NORM); // (its monster's own pain: vr_armedmon.qc)
    self.vr_ag_act = 0; // (an attack under way: given up for the struggle)
    self.think = vr_ag_tughold;
    self.nextthink = time + 0.05;
    self = oldself;
    VR_AG_Log(sprintf("tug begun by %s (hand %g): pull %g units for %.2f s", pl.netname, h,
        cvar_hget(cvarh_vr_agrunt_tug_pull), VR_AG_TugNeed(g)));
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
    float h = g.vr_ag_tug_hand;
    // (Its grip let go: its release's time, kept in the player's own think. The grabbing bit itself reads clear in other
    // entities' thinks.)
    if(pl.health <= 0 || !VRIsHandEmpty(pl, h) || VR_HandGrabUtil_Impl_GetLastReleaseTime(pl, h) > g.vr_ag_tug_since)
    {
        VR_AG_TugEnd(g, FALSE, "let go");
        return FALSE;
    }
    if(g.vr_ag_tug_rag)
    {
        if(VR_AG_HealthShare(g) <= cvar_hget(cvarh_vr_agrunt_tug_hurt))
        {
            VR_AG_TugEnd(g, TRUE, "badly hurt");
            VR_AG_Yank(g, pl, h);
            return TRUE;
        }
        return VR_AG_TugRagFrame(g, p, pl, h);
    }
    string easy = VR_AG_Shaky(g);
    if(easy != "")
    {
        VR_AG_TugEnd(g, TRUE, easy);
        VR_AG_Yank(g, pl, h);
        return TRUE;
    }
    VR_AG_PlaceProp(g, p);
    vector grip = p.origin;
    vector hand = VRGetEntHandPos(pl, h);
    vector rel = hand - grip - g.vr_ag_tug_off;
    if(vlen(rel) > VR_AG_TUG_SLIP)
    {
        VR_AG_TugEnd(g, FALSE, "torn away");
        return TRUE;
    }
    float stretch = rel * g.vr_ag_tug_away;
    vr_ag_tug_stretch = stretch;
    float dt = min(0.1, time - g.vr_ag_tug_last);
    g.vr_ag_tug_last = time;
    float need = VR_AG_TugNeed(g);
    if(stretch >= cvar_hget(cvarh_vr_agrunt_tug_pull))
    {
        g.vr_ag_tug_prog = g.vr_ag_tug_prog + dt;
    }
    else
    {
        g.vr_ag_tug_prog = max(0, g.vr_ag_tug_prog - dt * 0.5);
    }
    if(g.vr_ag_tug_prog >= need)
    {
        VR_AG_TugEnd(g, TRUE, "pulled out of its hands");
        VR_AG_Yank(g, pl, h);
        return TRUE;
    }
    // Your hand jerked towards it, buzzing harder as you gain (the weapon stays in its hand: drawn from its body).
    if(time >= g.vr_ag_tug_buzz)
    {
        g.vr_ag_tug_buzz = time + 0.2 + random() * 0.15;
        float gain = need > 0 ? min(1, g.vr_ag_tug_prog / need) : 1;
        entity oldself = self;
        self = pl;
        haptic(h, 0, 0.12, 50 + 110 * gain, 0.5 + 0.5 * gain);
        handimpact(h, 1 + random() * 1.5, normalize(g.origin + '0 0 16' - hand));
        self = oldself;
    }
    return TRUE;
};

// It fights back (self: the grunt, in its tug's hold).
void() VR_AG_TugFight =
{
    entity pl = self.vr_ag_tug_pl;
    float fight = cvar_hget(cvarh_vr_agrunt_tug_fight);
    if(fight <= 0 || time < self.vr_ag_tug_next)
    {
        return;
    }
    self.vr_ag_tug_next = time + fight * (0.7 + random() * 0.6);
    self.vr_ag_tug_fights = self.vr_ag_tug_fights + 1;
    float c = VR_AG_Class(self.vr_ag_wid);
    float gun = c != VR_AG_CLASS_FISTS && c != VR_AG_CLASS_MELEE;
    float r = random();
    makevectors('0 1 0' * self.angles_y);
    if(gun && r < 0.4 && self.vr_ag_prop.weaponinst.wi_clip > 0)
    {
        VR_AG_Log("tug: fires at you");
        self.enemy = pl;
        self.th_missile(); // (its attack frames; back to the hold after: VR_AG_TugResume)
        return;
    }
    if(gun && r < 0.7 && self.th_vrshove) // (fists and melee weapons never shove: a headbutt)
    {
        VR_AG_TugEnd(self, FALSE, "shoved off");
        self.vr_eshove_target = pl;
        self.vr_eshove_next = time + cvar_hget(cvarh_vr_enemy_shove_cooldown);
        VR_EnemyShove_Play(); // (posed: vr_agrunt_shove)
        return;
    }
    VR_AG_TugEnd(self, FALSE, "headbutted");
    sound(self, CHAN_WEAPON, "zombie/z_hit.wav", 1, ATTN_NORM);
    T_Damage(pl, self, self, VR_AG_TUG_BUTT);
    pl.punchangle_x = -10;
    vector dir = pl.origin - self.origin;
    dir_z = 0;
    VR_Push(pl, normalize(dir), 220, 60);
};

// Its hold while you tug (the aiming frame: the weapon held out), facing you.
void() vr_ag_tughold =
{
    VR_AM_Anim(VR_AM_A_ATK, 0, vr_ag_tughold);
    entity pl = self.vr_ag_tug_pl;
    if(!pl || !self.vr_ag_prop || self.vr_ag_tug_rag)
    {
        // (No tug, or no weapon to tug at: back to its run, the tug over. It used to keep the tug on with no weapon, its
        // run sending it back here every frame: frozen.)
        if(pl)
        {
            VR_AG_TugEnd(self, FALSE, "no weapon");
        }
        self.think = self.th_run;
        self.nextthink = time + 0.05;
        return;
    }
    self.ideal_yaw = vectoyaw(pl.origin - self.origin);
    ChangeYaw();
    VR_AG_TugFight();
};

// Its run frames (VR_AG_Run) while a tug is on: back to the hold (after a pain or a shot). TRUE: it is.
float() VR_AG_TugResume =
{
    if(!self.vr_ag_tug_pl || self.vr_ag_tug_rag)
    {
        return FALSE;
    }
    if(!self.vr_ag_prop || self.vr_ag_tug_pl.health <= 0)
    {
        VR_AG_TugEnd(self, FALSE, "no weapon"); // (never left on: frozen in the hold, unarmed)
        return FALSE;
    }
    vr_ag_tughold();
    return TRUE;
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
        VR_AG_Disarm(attacker, attacker.origin - targ.origin, 120, "its blow parried");
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

// The least distance between the segments a0..a1 and b0..b1.
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
    return vlen(a0 + d1 * s - (b0 + d2 * t));
};

// The held weapon `p`'s line (vr_ag_wa: 8 units behind its grip, vr_ag_wb: its muzzle; one too near: 24 ahead).
void(entity p) VR_AG_WeaponLine =
{
    makevectors(p.v_angle);
    vector m = weaponmuzzle(p);
    if(vlen(m - p.origin) < 8 || vlen(m - p.origin) > 80)
    {
        m = p.origin + v_forward * 24;
    }
    vr_ag_wa = p.origin - normalize(m - p.origin) * 8;
    vr_ag_wb = m;
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

// Whether `pl`'s hand `h` can strike (vr_agrunt_parry_strikes_only, VR_AG_HandStrike's disarms): it holds something
// (a weapon, a carried thing), or it is an empty hand closed into a fist (its grip held). An open hand is a reach, never a
// blow on its weapon (his note vrragtriggers_2026-10-24_23-01-03: it parried his hand reaching for its barrel).
float(entity pl, float h) VR_AG_HandStrikes =
{
    return !VRIsHandEmpty(pl, h) || VR_HandGrabUtil_IsHandGrabbing(pl, h);
};

// A player's hand (or what it holds) striking the held weapon hard enough: knocked out along the blow. TRUE: disarmed.
float(entity g, entity p, entity pl, float h) VR_AG_HandStrike =
{
    vector hp = VRGetEntHandPos(pl, h);
    vector mz = h == cVR_MainHand ? pl.muzzlepos : pl.offmuzzlepos;
    if(VRIsHandEmpty(pl, h) || vlen(mz - hp) > 48)
    {
        mz = hp;
    }
    if(VR_AG_SegDist(hp, mz, vr_ag_wa, vr_ag_wb) > 5)
    {
        return FALSE;
    }
    float last = h == cVR_MainHand ? pl.vr_ag_knock_at : pl.vr_ag_knock_off;
    if(time - last < 0.3)
    {
        return FALSE;
    }
    vector hv = VRGetEntHandVel(pl, h) - pl.velocity; // (the blow's way)
    // Its speed: the hand's (or its weapon's far end's) own swing, in your play space against your head (your melee's,
    // vr_melee.qc VR_Melee_Track), not its world velocity: a push (a blow's knockback, a parry's, a shotgun's) or a step
    // carries your idle hands along at 5 m/s and more, and the world velocity less yours (a frame behind) read as a
    // blow (vrtesthall_2026-10-24_03-12-16: hands still against the head, 0.0-0.5 m/s, pushed at 4.9 m/s: dropped).
    float sp = VR_AG_SwingSpeed(pl, h);
    if(p.vr_ag_was_n >= 1 && frametime > 0 && vlen(p.origin - p.vr_ag_was_o1) < 32)
    {
        // A blow is the fist against the weapon: a hand going with a swing (closing on it to catch it, his notes
        // vrragtriggers_2026-10-27_01-41-43) is no blow on it however fast both go; in real time (bullet time's hands
        // are the game's, sped up).
        vector wv = (p.origin - p.vr_ag_was_o1) * (1 / frametime);
        float rel = vlen(VRGetEntHandVel(pl, h) - wv) * (vr_hand_time_scale > 0 ? vr_hand_time_scale : 1) / VRMetersToUnits();
        sp = min(sp, rel);
    }
    float need = cvar_hget(cvarh_vr_agrunt_disarm_swing);
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
    if(h == cVR_MainHand) { pl.vr_ag_knock_at = time; } else { pl.vr_ag_knock_off = time; }
    VR_AG_Disarm(g, hv, 100 + sp * 15, sprintf("a blow of %.1f m/s on its weapon", sp));
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
        return FALSE;
    }
    if(g.vr_ag_prop == p && VR_AGPar_WeaponFrame(g)) // (your blow parried, its last strength spent: vr_armedgrunt_parry.qc)
    {
        return TRUE;
    }
    if(!cvar_hget(cvarh_vr_agrunt_disarm) || !g.vr_ag_prop)
    {
        return FALSE;
    }
    VR_AG_WeaponLine(p);
    vector mid = (vr_ag_wa + vr_ag_wb) * 0.5;
    entity pl = VR_Player();
    while(pl)
    {
        if(pl.health > 0 && vlen(pl.origin - mid) < 128 &&
           (VR_AG_HandStrike(g, p, pl, cVR_MainHand) || VR_AG_HandStrike(g, p, pl, cVR_OffHand)))
        {
            return TRUE;
        }
        pl = VR_NextPlayer(pl);
    }
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
                    VR_AG_Disarm(g, e.velocity, 120, sprintf("a %s thrown at its weapon, %.1f kg m/s", e.classname, mom));
                    return TRUE;
                }
                VR_AG_Log(sprintf("a %s thrown at its weapon, %.1f kg m/s: under %g", e.classname, mom,
                    cvar_hget(cvarh_vr_agrunt_disarm_throw)));
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

// The impact rule (vr_agrunt_fall_keep_impact; his notes vrragtriggers_2026-10-27_01-43-08, 01-45-10: lifted, thrown or
// shoulder-charged "they should try to hold on; the impact with the ground should make them drop it"): its body's place
// each frame from its fall's start; it lets go only as it meets the ground (near the floor, coming down) losing `most`
// units/s or more in a frame (its velocity's change: a slam, a hard landing; a topple's fall onto its back is gentle).
// Flung through the air, carried in your hands, swung about, falling: it holds on. "" kept, else why it lets go.
string(entity g, float most) VR_AG_FallImpact =
{
    float u = VRMetersToUnits();
    float held = VR_AG_HeldByHand(g);
    if(frametime > 0)
    {
        vector v = (g.origin - g.vr_ag_fall_last) * (1 / frametime);
        vector was = g.vr_ag_fall_v;
        g.vr_ag_fall_v = v;
        g.vr_ag_fall_down = max(g.vr_ag_fall_down, -v_z);
        vector flat = v;
        flat_z = max(0, flat_z);
        g.vr_ag_fall_fling = max(g.vr_ag_fall_fling, vlen(flat));
        if(!held && !g.vr_ag_fall_held && (was_z < 0 || vlen(was) > 3 * u))
        {
            // (coming down, or flung along the floor: a judo throw's skid into it)
            traceline(g.origin, g.origin - '0 0 24', TRUE, g);
            if(trace_fraction < 1)
            {
                float hit = vlen(was - v);
                if(hit > g.vr_ag_fall_hit)
                {
                    g.vr_ag_fall_hit = hit;
                    g.vr_ag_fall_hit_at = time;
                }
            }
        }
    }
    g.vr_ag_fall_held = held;
    g.vr_ag_fall_last = g.origin;
    if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
    {
        VR_AG_Log(sprintf("fall: hardest impact %.1f m/s, falling %.1f m/s, moving %.1f m/s%s", g.vr_ag_fall_hit / u,
            g.vr_ag_fall_down / u, g.vr_ag_fall_fling / u, held ? ", held" : ""));
    }
    if(g.vr_ag_fall_hit >= most)
    {
        return sprintf("it met the ground at %.1f m/s", g.vr_ag_fall_hit / u);
    }
    return "";
};

// Knocked down (VR_AG_PropThink): TRUE when it let go (the prop gone). Staggering, still falling, getting up: it holds.
// Knocked down keeping its weapon (vr_agrunt_fall_keep): its fall measured each frame from its start (a stagger's too:
// it may go over). Violent: the floor under it fell vr_agrunt_fall_keep_height below the one it stood on (a ledge), its
// body fell faster than vr_agrunt_fall_keep_speed, or moved faster than vr_agrunt_fall_keep_fling any way but down
// (thrown, held and flung, blown away). "" gentle (it keeps it), else why it lets go.
string(entity g) VR_AG_FallViolent =
{
    float u = VRMetersToUnits();
    if(g.vr_ag_fall_at != g.vr_kd_since)
    {
        g.vr_ag_fall_at = g.vr_kd_since;
        traceline(g.origin, g.origin - '0 0 160', TRUE, g);
        g.vr_ag_fall_floor = trace_endpos_z;
        g.vr_ag_fall_last = g.origin;
        g.vr_ag_fall_drop = 0;
        g.vr_ag_fall_down = 0;
        g.vr_ag_fall_fling = 0;
        g.vr_ag_fall_v = '0 0 0';
        g.vr_ag_fall_hit = 0;
        g.vr_ag_fall_hit_at = 0;
        g.vr_ag_fall_held = 0;
        return "";
    }
    float impact = cvar_hget(cvarh_vr_agrunt_fall_keep_impact);
    if(impact > 0)
    {
        return VR_AG_FallImpact(g, impact * u);
    }
    if(frametime > 0)
    {
        vector v = (g.origin - g.vr_ag_fall_last) * (1 / frametime);
        g.vr_ag_fall_down = max(g.vr_ag_fall_down, -v_z);
        v_z = max(0, v_z);
        // (Flung: in its fall's first vr_agrunt_fall_keep_window s only. Lying, your body or hand nudging it as you come
        // to take its weapon is no fling: vrragtriggers_2026-10-24_23-04-09, it lost it lying as he walked up.)
        float win = cvar_hget(cvarh_vr_agrunt_fall_keep_window);
        if(win <= 0 || time - g.vr_kd_since <= win)
        {
            g.vr_ag_fall_fling = max(g.vr_ag_fall_fling, vlen(v));
        }
    }
    g.vr_ag_fall_last = g.origin;
    traceline(g.origin, g.origin - '0 0 160', TRUE, g);
    if(trace_fraction < 1)
    {
        g.vr_ag_fall_drop = max(g.vr_ag_fall_drop, g.vr_ag_fall_floor - trace_endpos_z);
    }
    if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
    {
        VR_AG_Log(sprintf("fall: floor %.2f m below, falling %.1f m/s, moving %.1f m/s", g.vr_ag_fall_drop / u,
            g.vr_ag_fall_down / u, g.vr_ag_fall_fling / u));
    }
    if(g.vr_ag_fall_drop > cvar_hget(cvarh_vr_agrunt_fall_keep_height) * u)
    {
        return sprintf("a fall of %.2f m", g.vr_ag_fall_drop / u);
    }
    if(g.vr_ag_fall_down > cvar_hget(cvarh_vr_agrunt_fall_keep_speed) * u)
    {
        return sprintf("falling at %.1f m/s", g.vr_ag_fall_down / u);
    }
    if(g.vr_ag_fall_fling > cvar_hget(cvarh_vr_agrunt_fall_keep_fling) * u)
    {
        return sprintf("flung at %.1f m/s", g.vr_ag_fall_fling / u);
    }
    return "";
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
        // a blow (VR_AG_HandStrike) while it is down. A tug's stagger is the tug's.
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
    return TRUE;
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
    if(!w || w.classname != "thrown_weapon" || VR_EnemyWeapons_Spared(w) || VR_AG_Shunned(g, w) ||
       vlen(w.origin - g.origin) > VR_AG_OWN_RANGE || !VR_AG_Reachable(g, w))
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

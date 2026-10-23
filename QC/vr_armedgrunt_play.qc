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
string vr_ag_reach_why;   // VR_AG_Reachable's last no (the tests)
float vr_ag_tug_stretch;  // the last tug frame's stretch (the tests)

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
    g.vr_ag_tug_pl = world;
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
    sound(self, CHAN_VOICE, "soldier/pain2.wav", 1, ATTN_NORM);
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
        self.th_vrshove();
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
void() vr_ag_tughold = [81, vr_ag_tughold]
{
    entity pl = self.vr_ag_tug_pl;
    if(!pl || !self.vr_ag_prop)
    {
        self.think = self.th_run;
        return;
    }
    self.ideal_yaw = vectoyaw(pl.origin - self.origin);
    ChangeYaw();
    VR_AG_TugFight();
};

// Its run frames (VR_AG_Run) while a tug is on: back to the hold (after a pain or a shot). TRUE: it is.
float() VR_AG_TugResume =
{
    if(!self.vr_ag_tug_pl)
    {
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
    if(!attacker.vr_ag || !attacker.vr_ag_prop || attacker.health <= 0 || attacker.vr_eshove_striking ||
       !VR_AG_Striking(attacker))
    {
        return; // (its weapon's swing only: not its shove)
    }
    float chance = cvar_hget(cvarh_vr_agrunt_parry_disarm) * (vr_parry_welltimed ? 1.5 : 1);
    if(random() < chance)
    {
        VR_AG_Disarm(attacker, attacker.origin - targ.origin, 120, "its blow parried");
    }
};

// ----------------------------------------------------------------------------
// Falls
// ----------------------------------------------------------------------------

// Knocked down (VR_AG_PropThink): TRUE when it let go (the prop gone). Staggering, still falling, getting up: it holds.
float(entity g) VR_AG_FallDrop =
{
    if(!cvar_hget(cvarh_vr_agrunt_fall_drop) || g.vr_knockdown != 1 || g.vr_kd_stagger ||
       time - g.vr_kd_since < VR_AG_FALL_LETGO)
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
    entity w = VR_AG_DropHeld(g, vel);
    g.vr_ag_own = w;
    g.vr_ag_disarms = g.vr_ag_disarms + 1;
    sound(g, CHAN_AUTO, "weapons/tink1.wav", 1, ATTN_NORM);
    VR_AG_Log("dropped its weapon as it fell");
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

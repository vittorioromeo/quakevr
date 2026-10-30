//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_guard.qc -- armed humanoids' guard: parries raised before any blow (ROUND23.md, "Preventive and
// wind-up parries"). His words (2026-10-30): an option (on by default) for armed humanoids to parry preventively when you
// get close and their shove is on cooldown or they wield a melee weapon, for a while, so you wait it out or bait it out
// and strike in the opening; and one to parry when you raise a weapon (or anything you can hit with) overhead or to the
// side as if to strike. Chance-based, with random delays: not wholly predictable. Every value a tunable.
// - Preventive (vr_agrunt_preparry): every vr_agrunt_preparry_interval s, you within vr_agrunt_preparry_range of it, it
//   able to parry you (VR_AGPar_Able) and its shove cooling down (or off) or a melee weapon in its hand: a roll
//   (vr_agrunt_preparry_chance); won, its guard rises after a delay (_delay_min.._max).
// - Wind-up (vr_agrunt_windup_parry): your hand (holding a weapon, a gun, a prop or a closed fist) raised over your
//   shoulders (vr_agrunt_windup_high), out to your side (_side) or drawn back behind your head (_back), held so for
//   _read s, within _range of it: one roll per raise (_chance); won, its guard rises after _delay_min.._max. A raise held
//   is read once (_rearm 1: lower it to be read again): raise, let it parry, wait it out, strike.
// - The guard: its parry pose raised towards your hand for _guard_min.._max s (VR_AGPar_Frame: its weapon put in the
//   blow's way; a deliberate strike on it rolls vr_agrunt_strike_beat), then dropped for _recover_min.._max s, when it
//   can't parry at all (VR_AGPar_Able: "its guard dropped"): a strike on its weapon disarms it, a blow lands in full.
//   Guarding it holds its fire (vr_agrunt_guard_holdfire).
// - By what you hold (VR_AGG_Cat: a melee weapon, a gun, a carried prop, your fists): vr_agrunt_pcat_<cat>_* multiply
//   every parry's chance (VR_AGG_Odds: 1 - (1 - c)^m, so 0 stays never and 1 always), its delays (the reactive parry's
//   reaction too), the guard's time, the ranges and the wind-up's thresholds (above 1: a plainer raise needed); _pre the
//   preventive parry's chance besides (a gun's holder: it waits for your wind-up).
// Class-agnostic: any armed humanoid holding a weapon with vr_agrunt_parry on (VR_AGPar_Guards).
// ----------------------------------------------------------------------------

float VR_AGG_IDLE = 0;
float VR_AGG_REACTING = 1;
float VR_AGG_RAISED = 2;
float VR_AGG_RECOVERING = 3;
float VR_AGG_KIND_PRE = 1;
float VR_AGG_KIND_WINDUP = 2;
float VR_AGG_CAT_MELEE = 0;
float VR_AGG_CAT_GUN = 1;
float VR_AGG_CAT_PROP = 2;
float VR_AGG_CAT_FIST = 3;
float VR_AGG_M_CHANCE = 0;   // VR_AGG_Mult's kinds
float VR_AGG_M_DELAY = 1;
float VR_AGG_M_GUARD = 2;
float VR_AGG_M_RANGE = 3;
float VR_AGG_M_WINDUP = 4;
float VR_AGG_M_PRE = 5;
float VR_AGG_SHOULDER = 0.2; // m: your shoulders this far under your eyes

float vr_agg_raised_n;   // guards raised (the tests)
float vr_agg_rolls;      // rolls made, and won (the tests)
float vr_agg_won;
float vr_agg_last_delay; // the last won roll's delay, guard and recovery (s; the tests)
float vr_agg_last_guard;
float vr_agg_last_recover;
float vr_agg_force_ok;   // VR_AGG_Roll's won roll (the tests' bare rolls)

// What `pl`'s hand `h` holds: VR_AGG_CAT_*, -1 nothing it can strike with (an open hand, a climbing one).
float(entity pl, float h) VR_AGG_Cat =
{
    float t = VR_Melee_Thing(pl, h);
    if(t == VR_MTHING_NONE)
    {
        return -1;
    }
    if(t == VR_MTHING_GUN)
    {
        return VR_AGG_CAT_GUN;
    }
    if(t == VR_MTHING_CLUB)
    {
        return VR_AGG_CAT_PROP;
    }
    if(t == VR_MTHING_FIST)
    {
        if(!VRIsHandEmpty(pl, h))
        {
            return VR_AGG_CAT_PROP; // (a carried thing)
        }
        return VR_AG_HandStrikes(pl, h) ? VR_AGG_CAT_FIST : -1;
    }
    return VR_AGG_CAT_MELEE;
};

// `pl`'s likeliest striking hand: a melee weapon first, then a gun, a prop, a fist (+1; 0 neither strikes).
float(entity pl) VR_AGG_ThreatHand =
{
    float best = 0;
    float bestRank = 100;
    float h;
    for(h = 0; h < 2; h++)
    {
        float c = VR_AGG_Cat(pl, h);
        if(c >= 0 && c < bestRank)
        {
            bestRank = c;
            best = h + 1;
        }
    }
    return best;
};

// The category `cat` (-1: an open hand: the fists') multiplier of kind `k` (VR_AGG_M_*).
float(float cat, float k) VR_AGG_Mult =
{
    float v;
    if(cat == VR_AGG_CAT_MELEE)
    {
        if(k == VR_AGG_M_CHANCE) { v = cvar_hget(cvarh_vr_agrunt_pcat_melee_chance); }
        else if(k == VR_AGG_M_DELAY) { v = cvar_hget(cvarh_vr_agrunt_pcat_melee_delay); }
        else if(k == VR_AGG_M_GUARD) { v = cvar_hget(cvarh_vr_agrunt_pcat_melee_guard); }
        else if(k == VR_AGG_M_RANGE) { v = cvar_hget(cvarh_vr_agrunt_pcat_melee_range); }
        else if(k == VR_AGG_M_WINDUP) { v = cvar_hget(cvarh_vr_agrunt_pcat_melee_windup); }
        else { v = cvar_hget(cvarh_vr_agrunt_pcat_melee_pre); }
    }
    else if(cat == VR_AGG_CAT_GUN)
    {
        if(k == VR_AGG_M_CHANCE) { v = cvar_hget(cvarh_vr_agrunt_pcat_gun_chance); }
        else if(k == VR_AGG_M_DELAY) { v = cvar_hget(cvarh_vr_agrunt_pcat_gun_delay); }
        else if(k == VR_AGG_M_GUARD) { v = cvar_hget(cvarh_vr_agrunt_pcat_gun_guard); }
        else if(k == VR_AGG_M_RANGE) { v = cvar_hget(cvarh_vr_agrunt_pcat_gun_range); }
        else if(k == VR_AGG_M_WINDUP) { v = cvar_hget(cvarh_vr_agrunt_pcat_gun_windup); }
        else { v = cvar_hget(cvarh_vr_agrunt_pcat_gun_pre); }
    }
    else if(cat == VR_AGG_CAT_PROP)
    {
        if(k == VR_AGG_M_CHANCE) { v = cvar_hget(cvarh_vr_agrunt_pcat_prop_chance); }
        else if(k == VR_AGG_M_DELAY) { v = cvar_hget(cvarh_vr_agrunt_pcat_prop_delay); }
        else if(k == VR_AGG_M_GUARD) { v = cvar_hget(cvarh_vr_agrunt_pcat_prop_guard); }
        else if(k == VR_AGG_M_RANGE) { v = cvar_hget(cvarh_vr_agrunt_pcat_prop_range); }
        else if(k == VR_AGG_M_WINDUP) { v = cvar_hget(cvarh_vr_agrunt_pcat_prop_windup); }
        else { v = cvar_hget(cvarh_vr_agrunt_pcat_prop_pre); }
    }
    else
    {
        if(k == VR_AGG_M_CHANCE) { v = cvar_hget(cvarh_vr_agrunt_pcat_fist_chance); }
        else if(k == VR_AGG_M_DELAY) { v = cvar_hget(cvarh_vr_agrunt_pcat_fist_delay); }
        else if(k == VR_AGG_M_GUARD) { v = cvar_hget(cvarh_vr_agrunt_pcat_fist_guard); }
        else if(k == VR_AGG_M_RANGE) { v = cvar_hget(cvarh_vr_agrunt_pcat_fist_range); }
        else if(k == VR_AGG_M_WINDUP) { v = cvar_hget(cvarh_vr_agrunt_pcat_fist_windup); }
        else { v = cvar_hget(cvarh_vr_agrunt_pcat_fist_pre); }
    }
    return max(0, v);
};

// A chance `c` scaled by `m` as odds: 1 - (1 - c)^m (0 stays 0, 1 stays 1; m above 1 likelier).
float(float c, float m) VR_AGG_Odds =
{
    c = min(1, max(0, c));
    if(c >= 1 || c <= 0)
    {
        return c;
    }
    return 1 - pow(1 - c, max(0, m));
};

// A random time in lo..hi (hi under lo: lo).
float(float lo, float hi) VR_AGG_Rand = { return max(0, lo) + random() * max(0, hi - lo); };

// `pl`'s hand `h` raised as if to strike `g` (overhead, out to the side, drawn back), its thresholds times `m`.
float(entity g, entity pl, float h, float m) VR_AGG_RaisedHand =
{
    float mu = VRMetersToUnits();
    vector head = VRGetHeadPos(pl);
    vector hp = VRGetEntHandPos(pl, h);
    vector fw = g.origin - head;
    fw_z = 0;
    fw = vlen(fw) > 0.1 ? normalize(fw) : '1 0 0';
    vector rt = VR_Cross(fw, '0 0 1');
    vector rel = hp - head;
    float shoulders = head_z - VR_AGG_SHOULDER * mu;
    if(hp_z - shoulders >= cvar_hget(cvarh_vr_agrunt_windup_high) * m * mu)
    {
        return TRUE; // (overhead)
    }
    if(rel_z < -cvar_hget(cvarh_vr_agrunt_windup_low) * mu)
    {
        return FALSE; // (low: at rest)
    }
    float ahead = rel * fw;
    float side = fabs(rel * rt);
    if(side >= cvar_hget(cvarh_vr_agrunt_windup_side) * m * mu && ahead <= cvar_hget(cvarh_vr_agrunt_windup_ahead) * mu)
    {
        return TRUE; // (cocked to the side)
    }
    return ahead <= -cvar_hget(cvarh_vr_agrunt_windup_back) * m * mu; // (drawn back)
};

// One roll for `g`'s guard of kind `kind` against `pl` (its hand `h`'s category; h -1: the likeliest): won, its guard
// set to rise after the delay (VR_AGG_REACTING). TRUE: won.
float(entity g, entity pl, float h, float kind) VR_AGG_Roll =
{
    float cat = h >= 0 ? VR_AGG_Cat(pl, h) : -1;
    if(h < 0)
    {
        float th = VR_AGG_ThreatHand(pl);
        cat = th ? VR_AGG_Cat(pl, th - 1) : -1;
    }
    float pre = kind == VR_AGG_KIND_PRE;
    float c = pre ? cvar_hget(cvarh_vr_agrunt_preparry_chance) : cvar_hget(cvarh_vr_agrunt_windup_chance);
    float m = VR_AGG_Mult(cat, VR_AGG_M_CHANCE) * (pre ? VR_AGG_Mult(cat, VR_AGG_M_PRE) : 1);
    float chance = VR_AGG_Odds(min(1, max(0, c) * VR_AGPar_SkillChance()), m);
    float dm = VR_AGG_Mult(cat, VR_AGG_M_DELAY) * VR_AGPar_SkillReact();
    float delay = dm * (pre ? VR_AGG_Rand(cvar_hget(cvarh_vr_agrunt_preparry_delay_min), cvar_hget(cvarh_vr_agrunt_preparry_delay_max))
                            : VR_AGG_Rand(cvar_hget(cvarh_vr_agrunt_windup_delay_min), cvar_hget(cvarh_vr_agrunt_windup_delay_max)));
    vr_agg_rolls = vr_agg_rolls + 1;
    float roll = random();
    string what = pre ? "you up close" : "your wind-up";
    if(roll >= chance)
    {
        VR_AG_Log(sprintf("guard: %s: not raised (chance %.2f, rolled %.2f)", what, chance, roll));
        vr_agg_force_ok = FALSE;
        return FALSE;
    }
    vr_agg_won = vr_agg_won + 1;
    vr_agg_last_delay = delay;
    vr_agg_force_ok = TRUE;
    g.vr_agg_state = VR_AGG_REACTING;
    g.vr_agg_kind = kind;
    g.vr_agg_until = time + delay;
    g.vr_agg_pl = pl;
    g.vr_agg_hand = h >= 0 ? h + 1 : VR_AGG_ThreatHand(pl);
    VR_AG_Log(sprintf("guard: %s: raised in %.2f s (chance %.2f, rolled %.2f)", what, delay, chance, roll));
    return TRUE;
};

// Its guard dropped: the recovery (an opening) for a random time.
void(entity g) VR_AGG_Drop =
{
    entity pl = g.vr_agg_pl;
    float th = pl ? VR_AGG_ThreatHand(pl) : 0;
    float cat = th ? VR_AGG_Cat(pl, th - 1) : -1;
    float pre = g.vr_agg_kind == VR_AGG_KIND_PRE;
    float r = pre ? VR_AGG_Rand(cvar_hget(cvarh_vr_agrunt_preparry_recover_min), cvar_hget(cvarh_vr_agrunt_preparry_recover_max))
                  : VR_AGG_Rand(cvar_hget(cvarh_vr_agrunt_windup_recover_min), cvar_hget(cvarh_vr_agrunt_windup_recover_max));
    vr_agg_last_recover = r;
    g.vr_agg_state = VR_AGG_RECOVERING;
    g.vr_agg_until = time + r;
    g.vr_agpar_ready = -1;
    g.vr_agpar_since = 0;
    VR_AG_Log(sprintf("guard: dropped, recovering %.2f s", r));
};

// Its guard's state each frame (VR_AGPar_Frame, its weapon's think): the timers, the wind-up read, the preventive
// check. Its state now (VR_AGG_RAISED: its parry up).
float(entity g) VR_AGG_Frame =
{
    float pre = cvar_hget(cvarh_vr_agrunt_preparry);
    float wup = cvar_hget(cvarh_vr_agrunt_windup_parry);
    if(!VR_AGPar_Guards(g) || g.health <= 0 || (!pre && !wup))
    {
        if(g.vr_agg_state == VR_AGG_RAISED)
        {
            g.vr_agpar_ready = -1;
        }
        g.vr_agg_state = VR_AGG_IDLE;
        return VR_AGG_IDLE;
    }
    // The nearest player (alive) within reach of any check.
    entity pl = VR_Player();
    entity best = world;
    float bestD = 100000;
    while(pl)
    {
        vector d = pl.origin - g.origin;
        d_z = 0;
        if(pl.health > 0 && vlen(d) < bestD)
        {
            bestD = vlen(d);
            best = pl;
        }
        pl = VR_NextPlayer(pl);
    }
    pl = best;
    // The timers.
    if(g.vr_agg_state == VR_AGG_REACTING && time >= g.vr_agg_until)
    {
        if(g.vr_agg_pl && VR_AGPar_Able(g, g.vr_agg_pl))
        {
            entity gp = g.vr_agg_pl;
            float th = g.vr_agg_hand ? g.vr_agg_hand : VR_AGG_ThreatHand(gp);
            float cat = th ? VR_AGG_Cat(gp, th - 1) : -1;
            float gm = VR_AGG_Mult(cat, VR_AGG_M_GUARD);
            float dur = gm * (g.vr_agg_kind == VR_AGG_KIND_PRE ?
                VR_AGG_Rand(cvar_hget(cvarh_vr_agrunt_preparry_guard_min), cvar_hget(cvarh_vr_agrunt_preparry_guard_max)) :
                VR_AGG_Rand(cvar_hget(cvarh_vr_agrunt_windup_guard_min), cvar_hget(cvarh_vr_agrunt_windup_guard_max)));
            vr_agg_last_guard = dur;
            vr_agg_raised_n = vr_agg_raised_n + 1;
            g.vr_agg_state = VR_AGG_RAISED;
            g.vr_agg_from = time;
            g.vr_agg_until = time + dur;
            g.vr_agpar_ready = time;
            g.vr_agpar_since = time;
            VR_AG_Log(sprintf("guard: raised for %.2f s", dur));
        }
        else
        {
            g.vr_agg_state = VR_AGG_IDLE; // (it can't now: in pain, unaware)
        }
    }
    else if(g.vr_agg_state == VR_AGG_RAISED && (time >= g.vr_agg_until || !g.vr_agg_pl || !VR_AGPar_Able(g, g.vr_agg_pl)))
    {
        VR_AGG_Drop(g);
    }
    else if(g.vr_agg_state == VR_AGG_RECOVERING && time >= g.vr_agg_until)
    {
        g.vr_agg_state = VR_AGG_IDLE;
        g.vr_agg_next = time;
    }
    if(!pl)
    {
        return g.vr_agg_state;
    }
    // Your raises read (in any state: a raise held through its guard is read once, vr_agrunt_windup_rearm).
    float h;
    for(h = 0; h < 2; h++)
    {
        float hc = VR_AGG_Cat(pl, h);
        float raised = FALSE;
        if(wup && hc >= 0 && bestD <= cvar_hget(cvarh_vr_agrunt_windup_range) * VR_AGG_Mult(hc, VR_AGG_M_RANGE))
        {
            raised = VR_AGG_RaisedHand(g, pl, h, VR_AGG_Mult(hc, VR_AGG_M_WINDUP));
        }
        float since = h == 0 ? g.vr_agg_wup0 : g.vr_agg_wup1;
        float read = h == 0 ? g.vr_agg_wread0 : g.vr_agg_wread1;
        if(!raised)
        {
            since = 0;
            read = 0;
        }
        else
        {
            if(!since)
            {
                since = time;
            }
            float need = max(0, cvar_hget(cvarh_vr_agrunt_windup_read)) * VR_AGG_Mult(hc, VR_AGG_M_WINDUP);
            if(!read && time - since >= need && g.vr_agg_state == VR_AGG_IDLE && VR_AGPar_Able(g, pl))
            {
                read = 1;
                VR_AGG_Roll(g, pl, h, VR_AGG_KIND_WINDUP);
            }
            else if(read && !cvar_hget(cvarh_vr_agrunt_windup_rearm) && g.vr_agg_state == VR_AGG_IDLE)
            {
                read = 0; // (held: read again once its recovery is over)
                since = time;
            }
        }
        if(h == 0) { g.vr_agg_wup0 = since; g.vr_agg_wread0 = read; }
        else { g.vr_agg_wup1 = since; g.vr_agg_wread1 = read; }
    }
    // The preventive check: you up close, its shove cooling down (or none) or a melee weapon in its hand.
    if(pre && g.vr_agg_state == VR_AGG_IDLE && time >= g.vr_agg_next)
    {
        g.vr_agg_next = time + max(0.05, cvar_hget(cvarh_vr_agrunt_preparry_interval));
        float ph = VR_AGG_ThreatHand(pl);
        float pc = ph ? VR_AGG_Cat(pl, ph - 1) : -1;
        float shoveDown = !cvar_hget(cvarh_vr_enemy_shove) || !g.th_vrshove || time < g.vr_eshove_next;
        float melee = VR_AG_Class(g.vr_ag_prop.weapon) == VR_AG_CLASS_MELEE;
        float why = (shoveDown && cvar_hget(cvarh_vr_agrunt_preparry_shove)) || (melee && cvar_hget(cvarh_vr_agrunt_preparry_melee));
        if(why && bestD <= cvar_hget(cvarh_vr_agrunt_preparry_range) * VR_AGG_Mult(pc, VR_AGG_M_RANGE) &&
           VR_AGPar_Able(g, pl) && visible(pl))
        {
            VR_AGG_Roll(g, pl, -1, VR_AGG_KIND_PRE);
        }
    }
    return g.vr_agg_state;
};

// VR_AG_CheckAttack: guarding (or about to), it holds its attack (vr_agrunt_guard_holdfire).
float(entity g) VR_AGG_HoldFire =
{
    return (g.vr_agg_state == VR_AGG_RAISED || g.vr_agg_state == VR_AGG_REACTING) &&
           cvar_hget(cvarh_vr_agrunt_guard_holdfire);
};

// Show Disarm Volumes' note: its guard's state ("" idle).
string(entity g) VR_AGG_Label =
{
    string k = g.vr_agg_kind == VR_AGG_KIND_PRE ? "preparry" : "windup";
    if(g.vr_agg_state == VR_AGG_REACTING)
    {
        return sprintf("%s in %.2f s", k, g.vr_agg_until - time);
    }
    if(g.vr_agg_state == VR_AGG_RAISED)
    {
        return sprintf("%s %.2f s", k, g.vr_agg_until - time);
    }
    if(g.vr_agg_state == VR_AGG_RECOVERING)
    {
        return sprintf("recovering %.2f s", g.vr_agg_until - time);
    }
    return "";
};

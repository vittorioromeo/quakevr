
/*

A monster is in fight mode if it thinks it can effectively attack its
enemy.

When it decides it can't attack, it goes into hunt mode.

*/


float    enemy_vis, enemy_infront, enemy_range;
float    enemy_yaw;


void() knight_attack =
{
    float        len;

// decide if now is a good swing time
    len = vlen(self.enemy.origin+self.enemy.view_ofs - (self.origin+self.view_ofs));

    if(len<80)
        knight_atk1();
    else
        knight_runatk1();
};

//=============================================================================

/*
===========
CheckAttack

The player is in view, so decide to move or launch an attack
Returns FALSE if movement should continue
============
*/
float() CheckAttack =
{
    vector    spot1, spot2;
    entity    targ;
    float        chance;

    targ = self.enemy;

// see if any entities are in the way of the shot
    spot1 = self.origin + self.view_ofs;
    spot2 = VR_AI_Origin(targ) + targ.view_ofs;

    traceline(spot1, spot2, self.vr_ai_gate > 0 ? MOVE_PORTALS : FALSE, self);

//MED 10/17/96 added charmed stuff
    if((trace_ent != targ) && (!self.charmed))
        return FALSE;     // don't have a clear shot

    if(trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents

    if(enemy_range == RANGE_MELEE)
    {    // melee attack
        if(self.th_melee)
        {
            if(!VR_MeleeArc_Launch())
            {
                self.attack_state = AS_MELEE; // QVR: facing away: it turns first (ai_run_melee; melee arc)
                return TRUE;
            }
            self.vr_melee_begun = time; // VR: one attack's blows, one parry (combat.qc VR_Parry)
            if(self.classname == "monster_knight")
                knight_attack();
            else
                self.th_melee();
            return TRUE;
        }
    }

// missile attack
    if(!self.th_missile)
        return FALSE;

    if(time < self.attack_finished)
        return FALSE;

    if(enemy_range == RANGE_FAR)
        return FALSE;

    if(enemy_range == RANGE_MELEE)
    {
        chance = 0.9;
        self.attack_finished = 0;
    }
    else if(enemy_range == RANGE_NEAR)
    {
        if(self.th_melee)
            chance = 0.2;
        else
            chance = 0.4;
    }
    else if(enemy_range == RANGE_MID)
    {
        if(self.th_melee)
            chance = 0.05;
        else
            chance = 0.1;
    }
    else
        chance = 0;

    if(random () < chance)
    {
        self.th_missile();
        SUB_AttackFinished (2*random());
        return TRUE;
    }

    return FALSE;
};


/*
=============
ai_face

Stay facing the enemy
=============
*/
void() ai_face =
{
    self.ideal_yaw = vectoyaw(VR_AI_Origin(self.enemy) - self.origin);
    ChangeYaw();
};

/*
=============
ai_charge

The monster is in a melee attack, so get as close as possible to .enemy
=============
*/
float (entity targ) visible;
float(entity targ) infront;
float(entity targ) range;

// QVR: melee space (vr_enemy_melee_space; ROUND22.md, "Melee space"). The player's body is narrow (vr_hull_ent_width,
// vr_body_monster_inset) and a melee step goes until the bodies meet, or past him (the knight's flyby): the monster's
// model then fills his head. A melee step (ai_charge, ai_charge_side; AD's ad_ai_charge and kin) is cut short where its
// box would come nearer his head (level) than the space and nearer than it was; the engine (vr_bodycontact.cpp) pushes
// back smoothly one that is nearer all the same (he stepped or leaned in, a leap landed). Its blows still reach him.

// How far the level point h is from self's box at o (level; < 0: inside it), squared with its sign (no sqrt builtin
// needed: only compared).
float(float d) VR_MeleeSpace_Sq = { return d < 0 ? -d * d : d * d; };
float(vector h, vector o) VR_MeleeSpace_Gap =
{
    float dx, dy, lo, hi;
    lo = o_x + self.mins_x - h_x;
    hi = h_x - (o_x + self.maxs_x);
    dx = lo > hi ? lo : hi;
    lo = o_y + self.mins_y - h_y;
    hi = h_y - (o_y + self.maxs_y);
    dy = lo > hi ? lo : hi;
    if(dx > 0 || dy > 0)
    {
        if(dx < 0) { dx = 0; }
        if(dy < 0) { dy = 0; }
        return dx * dx + dy * dy;
    }
    return VR_MeleeSpace_Sq(dx > dy ? dx : dy);
};

// After a melee step from `before`: back along it to where it keeps the space (or no nearer than it was).
void(vector before) VR_MeleeSpace_Step =
{
    entity p;
    vector h, way;
    float space, keep, f, lo, hi, i;
    p = self.enemy;
    space = cvar("vr_enemy_melee_space");
    if(space <= 0 || !p || !(p.flags & FL_CLIENT) || self.origin == before)
        return;
    h = p.headpos;
    if(h == '0 0 0')
        h = p.origin + p.view_ofs;
    if(h_z < self.absmin_z || p.absmin_z > self.absmax_z)
        return; // (over it or under it)
    keep = VR_MeleeSpace_Gap(h, before);
    if(keep > space * space)
        keep = space * space;
    if(VR_MeleeSpace_Gap(h, self.origin) >= keep)
        return;
    way = self.origin - before;
    lo = 0; // the most of the step that keeps it (bisected)
    hi = 1;
    for(i = 0; i < 6; i++)
    {
        f = (lo + hi) * 0.5;
        if(VR_MeleeSpace_Gap(h, before + way * f) >= keep)
            lo = f;
        else
            hi = f;
    }
    setorigin(self, before + way * lo);
};

// QVR: melee arc (vr_enemy_melee_arc; ROUND22.md, "Melee arc"). id's blows (ai_melee, a bite, a claw) and AD's
// (ad_ai_checkmelee) check only reach, so a knight's sword struck a player behind it or beside it. Against a player a
// blow now lands only inside the arc ahead of the monster's facing on its hit frame (vr_enemy_melee_arc_angle each
// side; sweeping blows vr_enemy_melee_arc_wide more); a melee attack starts only facing him (else it turns first:
// ai_run_melee, AD's ad_ai_run_melee); and while it swings (ai_charge and kin) it turns after him at most
// vr_enemy_melee_turn degrees a frame, so stepping aside or behind it dodges the swing. Leaps (a fiend's, a dog's) hit
// by touch and are left alone. Monsters fighting each other keep Quake's blows.
// Size and kind (ROUND22.md, "Enemy Melee menu"): the arc and the reach grow with the monster's width against a grunt's
// 24 units (vr_enemy_melee_arc_size, vr_enemy_melee_reach_size: the share more per grunt width more; 0: the same for
// all), times its kind's multipliers (VR_MeleeArc_Kind; vr_enemy_melee_kind). A grunt-sized monster's are as before.
// Show Melee Arcs (vr_enemy_melee_arc_show): each frame VR_MeleeArc_ShowFrame sets the arc and reach of each melee
// monster after a player near him on .vr_melee_arc and kin; the engine draws them (vr_bodycontact.cpp).
float vr_meleearc_yaw0; // (VR_MeleeArc_TurnBegin, VR_MeleeArc_TurnEnd: self's facing before the frame's turns)
float vr_meleearc_last_off; // the last blow's angle off its facing (the test, vr_meleearc_test.qc)
float vr_meleearc_last_time; // ... and when (time)
float vr_meleearc_last_hit; // ... and whether it was inside the arc
float vr_meleearc_reach_base; // the last reach check's own reach (VR_MeleeReach: Quake's or AD's, before the growth)

float() VR_MeleeArc_On =
{
    return cvar("vr_enemy_melee_arc") && self.enemy && (self.enemy.flags & FL_CLIENT);
};

// Self's kind's multipliers ('arc reach 0'; vr_enemy_melee_kind 0: '1 1 0'). Short list, by classname: a knight's
// sword is a light, quick thrust (its arc 0.85: easier to sidestep than a grunt-sized blow). Every other kind 1 (its
// size alone: an ogre's and a shambler's grow by the size sliders).
vector() VR_MeleeArc_Kind =
{
    if(!cvar("vr_enemy_melee_kind"))
    {
        return '1 1 0';
    }
    if(self.classname == "monster_knight")
    {
        return '0.85 1 0';
    }
    return '1 1 0';
};

// Self's size against a grunt's (its level width over a grunt's 24: a knight's, a dog's 1, a fiend's 1.33, an ogre's
// 1.67, a shambler's 2.67), less 1.
float() VR_MeleeArc_SizeMore =
{
    float s = self.size_x / 24;
    return (s < 0.5 ? 0.5 : (s > 4 ? 4 : s)) - 1;
};

// Self's arc (degrees each side; wide: a sweeping blow's) and its reach from Quake's own (units).
float(float wide) VR_MeleeArc_Angle =
{
    vector kind = VR_MeleeArc_Kind();
    float a = cvar("vr_enemy_melee_arc_angle") * (1 + cvar("vr_enemy_melee_arc_size") * VR_MeleeArc_SizeMore());
    a = a * kind_x + (wide ? cvar("vr_enemy_melee_arc_wide") : 0);
    return a < 0 ? 0 : (a > 180 ? 180 : a);
};
float(float base) VR_MeleeArc_Reach =
{
    vector kind = VR_MeleeArc_Kind();
    float r = base * (1 + cvar("vr_enemy_melee_reach_size") * VR_MeleeArc_SizeMore()) * kind_y;
    return r < 0 ? 0 : r;
};

// A blow's reach check (id's `vlen(delta) > 60`, AD's ad_ai_checkmelee): Quake's reach `base` grown by self's size and
// kind against a player with the melee arc on; else `base` as ever.
float(float base) VR_MeleeReach =
{
    vr_meleearc_reach_base = base;
    if(!self.vr_melee_reach_base)
    {
        self.vr_melee_reach_base = base; // (until its first blow, for Show Melee Arcs)
    }
    return VR_MeleeArc_On() ? VR_MeleeArc_Reach(base) : base;
};

// Degrees (0..180, level) between self's facing and the way to its enemy.
float() VR_MeleeArc_Off =
{
    vector d = VR_AI_Origin(self.enemy) - self.origin;
    d_z = 0;
    if(d == '0 0 0')
    {
        return 0;
    }
    float a = anglemod(vectoyaw(d) - self.angles_y);
    return a > 180 ? 360 - a : a;
};

// On a blow's hit frame, after its reach check: TRUE if it lands (wide: a sweeping blow).
float(float wide) VR_MeleeArc_Hits =
{
    if(!self.enemy)
    {
        return TRUE;
    }
    float on = VR_MeleeArc_On();
    float off = VR_MeleeArc_Off();
    float arc = VR_MeleeArc_Angle(wide);
    vr_meleearc_last_off = off;
    vr_meleearc_last_time = time;
    vr_meleearc_last_hit = on ? (off <= arc) : TRUE;
    self.vr_melee_reach_base = vr_meleearc_reach_base; // (its blows' reach, for Show Melee Arcs)
    if(wide)
    {
        self.vr_melee_wide_seen = TRUE;
    }
    if(cvar("vr_enemy_melee_arc_debug") && (self.enemy.flags & FL_CLIENT))
    {
        bprint(sprintf("meleearc: %s blow %.0f deg off (arc %.0f, reach %.0f): %s\n", self.classname, off, arc,
            on ? VR_MeleeArc_Reach(vr_meleearc_reach_base) : vr_meleearc_reach_base,
            vr_meleearc_last_hit ? "lands" : "misses"));
    }
    return vr_meleearc_last_hit;
};

// Before a melee attack starts: TRUE if self faces its enemy enough to start it now (else it turns first).
float() VR_MeleeArc_Launch =
{
    return !VR_MeleeArc_On() || VR_MeleeArc_Off() <= VR_MeleeArc_Angle(FALSE);
};

// Show Melee Arcs (vr_enemy_melee_arc_show), from StartFrame: each live melee monster within 600 units of a player and
// after him gets its arc and reach now (.vr_melee_arc and kin, drawn by the engine). Its reach until its first blow: its
// first reach check's, else Quake's usual (60 a grunt-sized one, 100 a wider one). With 2 also printed once a second
// (meleearc: show), for the logs.
float vr_meleearc_show_print; // (when it last printed)
void() VR_MeleeArc_ShowFrame =
{
    if(!cvar("vr_enemy_melee_arc_show"))
    {
        return;
    }
    float print = cvar("vr_enemy_melee_arc_show") >= 2 && time >= vr_meleearc_show_print + 1;
    if(print)
    {
        vr_meleearc_show_print = time;
    }
    entity oself = self;
    entity p = find(world, classname, "player");
    while(p)
    {
        entity m = findradius(p.origin, 600);
        while(m)
        {
            if((m.flags & FL_MONSTER) && m.health > 0 && m.enemy == p && (m.th_melee || AD_HasMelee(m)))
            {
                self = m;
                float base = self.vr_melee_reach_base ? self.vr_melee_reach_base : (self.size_x > 24 ? 100 : 60);
                float on = cvar("vr_enemy_melee_arc");
                self.vr_melee_arc = on ? VR_MeleeArc_Angle(FALSE) : 180;
                self.vr_melee_arc_wide = self.vr_melee_wide_seen ? (on ? VR_MeleeArc_Angle(TRUE) : 180) : 0;
                self.vr_melee_reach = on ? VR_MeleeArc_Reach(base) : base;
                self.vr_melee_shown = time;
                if(print)
                {
                    bprint(sprintf("meleearc: show %s width %g arc %.1f wide %.1f reach %.1f (base %g)\n",
                        self.classname, self.size_x, self.vr_melee_arc, VR_MeleeArc_Angle(TRUE), self.vr_melee_reach,
                        base));
                }
                self = oself;
            }
            m = m.chain;
        }
        p = find(p, classname, "player");
    }
};

// Around a swing frame's turns after its enemy (ai_face's, and movetogoal's step's: a second ChangeYaw): all of them
// together held to vr_enemy_melee_turn degrees.
void() VR_MeleeArc_TurnBegin = { vr_meleearc_yaw0 = self.angles_y; };
void() VR_MeleeArc_TurnEnd =
{
    float cap = cvar("vr_enemy_melee_turn");
    if(cap <= 0 || !VR_MeleeArc_On())
    {
        return;
    }
    float turn = anglemod(self.angles_y - vr_meleearc_yaw0);
    turn = turn > 180 ? turn - 360 : turn;
    if(turn > cap)
    {
        self.angles_y = anglemod(vr_meleearc_yaw0 + cap);
    }
    else if(turn < -cap)
    {
        self.angles_y = anglemod(vr_meleearc_yaw0 - cap);
    }
};
void() VR_MeleeArc_Face = { VR_MeleeArc_TurnBegin(); ai_face(); VR_MeleeArc_TurnEnd(); };

void(float d) ai_charge =
{
    vector before;
    self.vr_melee_time = time; // QVR: kept out of the player's head meanwhile (vr_enemy_melee_space)
    VR_MeleeArc_TurnBegin(); // QVR: (melee arc: its turn held, ai_face's and movetogoal's step's)
    ai_face();
    before = self.origin;
    movetogoal (d);        // done in C code...
    VR_MeleeArc_TurnEnd();
    VR_MeleeSpace_Step(before);
};

void() ai_charge_side =
{
    vector    dtemp;
    float    heading;

// aim to the left of the enemy for a flyby

    self.vr_melee_time = time; // QVR: (as ai_charge)
    self.ideal_yaw = vectoyaw(VR_AI_Origin(self.enemy) - self.origin);
    VR_MeleeArc_TurnBegin(); // QVR: (melee arc)
    ChangeYaw();
    VR_MeleeArc_TurnEnd();

    makevectors(self.angles);
    dtemp = self.enemy.origin - 30*v_right;
    heading = vectoyaw(dtemp - self.origin);

    dtemp = self.origin;
    walkmove(heading, 20);
    VR_MeleeSpace_Step(dtemp);
};


/*
=============
ai_melee

=============
*/
void() ai_melee =
{
    vector    delta;
    float     ldmg;

    if(!self.enemy)
        return;        // removed before stroke

    self.vr_melee_time = time; // QVR: (as ai_charge)
    delta = self.enemy.origin - self.origin;

    if(vlen(delta) > VR_MeleeReach(60))
        return;
    if(!VR_MeleeArc_Hits(FALSE))
        return; // QVR: (melee arc)

    ldmg = (random() + random() + random()) * 3;
    T_Damage(self.enemy, self, self, ldmg);
};


void() ai_melee_side =
{
    vector    delta;
    float     ldmg;

    if(!self.enemy)
        return;        // removed before stroke

    ai_charge_side();

    delta = self.enemy.origin - self.origin;

    if(vlen(delta) > VR_MeleeReach(60))
        return;
    if(!CanDamage(self.enemy, self))
        return;
    if(!VR_MeleeArc_Hits(FALSE))
        return; // QVR: (melee arc)
    ldmg = (random() + random() + random()) * 3;
    T_Damage(self.enemy, self, self, ldmg);
};


//=============================================================================

/*
===========
SoldierCheckAttack

The player is in view, so decide to move or launch an attack
Returns FALSE if movement should continue
============
*/
float() SoldierCheckAttack =
{
    vector    spot1, spot2;
    entity    targ;
    float        chance;

    targ = self.enemy;

// see if any entities are in the way of the shot
    spot1 = self.origin + self.view_ofs;
    spot2 = VR_AI_Origin(targ) + targ.view_ofs;

    traceline(spot1, spot2, self.vr_ai_gate > 0 ? MOVE_PORTALS : FALSE, self);

    if(trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents

    if(trace_ent != targ)
        return FALSE;    // don't have a clear shot


// missile attack
    if(time < self.attack_finished)
        return FALSE;

    if(enemy_range == RANGE_FAR)
        return FALSE;

    if(enemy_range == RANGE_MELEE)
        chance = 0.9;
    else if(enemy_range == RANGE_NEAR)
        chance = 0.4;
    else if(enemy_range == RANGE_MID)
        chance = 0.05;
    else
        chance = 0;

    if(random () < chance)
    {
        self.th_missile();
        SUB_AttackFinished (1 + random());
        if(random() < 0.3)
            self.lefty = !self.lefty;

        return TRUE;
    }

    return FALSE;
};
//=============================================================================

/*
===========
ShamCheckAttack

The player is in view, so decide to move or launch an attack
Returns FALSE if movement should continue
============
*/
float() ShamCheckAttack =
{
    vector    spot1, spot2;
    entity    targ;

    if(enemy_range == RANGE_MELEE)
    {
        if(CanDamage(self.enemy, self))
        {
            self.attack_state = AS_MELEE;
            return TRUE;
        }
    }

    if(time < self.attack_finished)
        return FALSE;

    if(!enemy_vis)
        return FALSE;

    targ = self.enemy;

// see if any entities are in the way of the shot
    spot1 = self.origin + self.view_ofs;
    spot2 = VR_AI_Origin(targ) + targ.view_ofs;

    if(vlen(spot1 - spot2) > 600)
        return FALSE;

    traceline(spot1, spot2, self.vr_ai_gate > 0 ? MOVE_PORTALS : FALSE, self);

    if(trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents

    if(trace_ent != targ)
    {
        return FALSE;    // don't have a clear shot
    }

// missile attack
    if(enemy_range == RANGE_FAR)
        return FALSE;

    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (2 + 2*random());
    return TRUE;
};

//============================================================================

/*
===========
OgreCheckAttack

The player is in view, so decide to move or launch an attack
Returns FALSE if movement should continue
============
*/
float() OgreCheckAttack =
{
    vector    spot1, spot2;
    entity    targ;
    float        chance;

    if(enemy_range == RANGE_MELEE)
    {
        if(CanDamage(self.enemy, self))
        {
            self.attack_state = AS_MELEE;
            return TRUE;
        }
    }

    if(time < self.attack_finished)
        return FALSE;

    if(!enemy_vis)
        return FALSE;

    targ = self.enemy;

// see if any entities are in the way of the shot
    spot1 = self.origin + self.view_ofs;
    spot2 = VR_AI_Origin(targ) + targ.view_ofs;

    traceline(spot1, spot2, self.vr_ai_gate > 0 ? MOVE_PORTALS : FALSE, self);

    if(trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents

    if(trace_ent != targ)
    {
        return FALSE;    // don't have a clear shot
    }

// missile attack
    if(time < self.attack_finished)
        return FALSE;

    if(enemy_range == RANGE_FAR)
        return FALSE;

    else if(enemy_range == RANGE_NEAR)
        chance = 0.10;
    else if(enemy_range == RANGE_MID)
        chance = 0.05;
    else
        chance = 0;

    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (1 + 2*random());
    return TRUE;
};

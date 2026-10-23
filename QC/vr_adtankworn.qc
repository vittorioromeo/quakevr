//
//
//
// ----------------------------------------------------------------------------
// Armed monsters' back tanks (ROUND22.md, "Fuel tanks: the burst's field, the pickup, armed monsters' tanks")
// ----------------------------------------------------------------------------
//
// Vittorio, 2026-10-23 (vrfiringrange_2026-10-23_21-24-32, 21-25-15): an armed monster holding a flamethrower or poison
// sprayer has the tank on its back and the hose to its gun, as you do; shot enough, the tank bursts on it (it is hurt,
// set on fire or poisoned, the burst's field left round it) and its gun is spent: it lets go of it and fights with its
// fists. Dying, its gun falls with its tank lying about (VR_FuelTank_Attach, as before).
//
// The tank (classname vr_fueltank_worn, the lying tank's model; .owner its wearer, .vr_fuellink its weapon as held,
// vr_ag_weapon) is placed each frame by the weapon's think (VR_AG_PropThink calls VR_FuelTank_Wear): between its row's
// back vertices (vr_armedmon.qc vr_am_back_lo, _hi), upright along them, just behind. The engine draws it on the drawn
// body and the hose from it to the gun (vr_adtank.cpp relinkWorn, setupWorld). It takes damage as a tank lying about does
// (vr_adgun_tank_health; its wearer's own shots pass it: .owner) and bursts the same way (VR_FuelTank_BurstNow).
// vr_adgun_tank_worn 0: none.

.vector vr_tw_lo;  // its wearer's back vertices (vr_am_back_lo, _hi as made)
.vector vr_tw_hi;

void() VR_FuelTank_WornThink =
{
    entity m = self.owner;
    if(!VR_FuelTank_WornGun(self) || m.health <= 0 || !cvar("vr_adgun_tank_worn"))
    {
        if(cvar("developer"))
        {
            dprint(sprintf("adgun: %s's worn tank goes (its gun gone or spent, or it died)\n", m.classname));
        }
        VR_FuelTank_Hush(self);
        self.vr_fuellink = world;
        remove(self);
        return;
    }
    if(self.vr_fueltank_burstat && time >= self.vr_fueltank_burstat)
    {
        VR_FuelTank_BurstNow(self, self.enemy); // (set off by a neighbour's burst, or its field's flames)
        return;
    }
    self.nextthink = time + 0.05;
};

// Its place on `m`'s back now: between its back vertices, upright along them, just behind them (its own radius and an
// inch); its model turned with `m` (its yaw) and leaning as the back does.
void(entity t, entity m) VR_FuelTank_WornPlace =
{
    makevectors('0 1 0' * m.angles_y);
    vector fw = v_forward;
    vector lo = (modelvertex(m, t.vr_tw_lo_x) + modelvertex(m, t.vr_tw_lo_y)) * 0.5;
    float ok = (trace_fraction > 0);
    vector hi = (modelvertex(m, t.vr_tw_hi_x) + modelvertex(m, t.vr_tw_hi_y)) * 0.5;
    ok = (ok && trace_fraction > 0);
    if(!ok)
    {
        lo = m.origin - fw * 10 + '0 0 5'; // (no such vertices: about a grunt's back)
        hi = lo + '0 0 10';
    }
    vector up = normalize(hi - lo);
    if(up_z < 0.3)
    {
        up = '0 0 1';
    }
    vector f = normalize(fw - up * (fw * up));
    setorigin(t, (lo + hi) * 0.5 - fw * (0.06 * VRMetersToUnits() + 1));
    t.angles = vectoangles(f); // (an alias model: its pitch as vectoangles gives it; its up then along the back)
};

entity(entity p, entity m) VR_FuelTank_WornMake =
{
    entity t = spawn();
    t.classname = "vr_fueltank_worn";
    t.netname = "fuel tank";
    t.movetype = MOVETYPE_NOCLIP; // (placed each frame; not NONE: Killed then skips .enemy, who burst it)
    t.solid = SOLID_NOT_BUT_TOUCHABLE;
    setmodel(t, p.weapon == WID_ADPYRO ? "progs/vr_fueltank.mdl#6" : "progs/vr_fueltank.mdl#7");
    setsize(t, modelbounds(t, 0), modelbounds(t, 1));
    t.owner = m; // (its wearer's shots and moves pass it)
    t.vr_fuellink = p;
    p.vr_fuellink = t;
    VR_AM_Row(m.vr_ag_kind);
    t.vr_tw_lo = vr_am_back_lo;
    t.vr_tw_hi = vr_am_back_hi;
    t.touch = SUB_Null;
    t.think = VR_FuelTank_WornThink;
    t.nextthink = time + 0.05;
    float hp = cvar("vr_adgun_tank_health");
    if(hp > 0)
    {
        t.takedamage = DAMAGE_YES;
        t.health = hp;
        t.max_health = hp;
        t.th_die = VR_FuelTank_Burst;
        t.th_pain = VR_FuelTank_Pain;
    }
    vr_fueltank_worn_made++;
    if(cvar("developer"))
    {
        dprint(sprintf("adgun: %s wears %s's tank\n", m.classname, p.netname));
    }
    return t;
};

// `xProp`'s think (VR_AG_PropThink, each frame it is held): its tank on its wearer's back, made if it has none.
void(entity xProp) VR_FuelTank_Wear =
{
    entity m = xProp.owner;
    if(!cvar("vr_adgun_tank_worn") || !VR_FuelTank_Fuelled(xProp.weapon) || (xProp.weaponflags & QVR_WPNFLAG_TANK_BURST) ||
       m.vr_ag_prop != xProp || m.health <= 0)
    {
        return; // (a tank it had goes by its own think)
    }
    entity t = xProp.vr_fuellink;
    if(!t || t.classname != "vr_fueltank_worn" || t.vr_fuellink != xProp || !t.modelindex)
    {
        t = VR_FuelTank_WornMake(xProp, m);
    }
    VR_FuelTank_WornPlace(t, m);
};

// The tests (vr_ad_test 167..169: vr_ad_enemyguns_test.qc). An armed grunt with a flamethrower (`xWid`) `xDist` ahead of
// `p`, facing away from it (its back tank toward you).
entity(entity p, float xWid, float xDist) VR_FuelTank_TestWearer =
{
    makevectors('0 1 0' * p.angles_y);
    vector fw = v_forward;
    traceline(p.origin, p.origin + fw * xDist, TRUE, p);
    entity oldself = self;
    self = spawn();
    setorigin(self, trace_endpos - fw * 24);
    self.angles_y = p.angles_y; // (its back to you)
    self.weapon = xWid;
    VR_AM_Spawn(VR_AM_GRUNT);
    entity g = self;
    self = oldself;
    return g;
};

// The nearest worn tank to `p` (world: none).
entity(entity p) VR_FuelTank_TestWorn =
{
    entity best = world;
    for(entity t = find(world, classname, "vr_fueltank_worn"); t; t = find(t, classname, "vr_fueltank_worn"))
    {
        if(!best || vlen(t.origin - p.origin) < vlen(best.origin - p.origin))
        {
            best = t;
        }
    }
    return best;
};

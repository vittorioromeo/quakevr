// vr_burning.qc -- things set on fire (docs/vr-port/ROUND21.md, "Burning"; Combat > Burning).
//
// A lit torch's blow, a thrown lit torch, a lava nail (the nailguns' lava ammo: vr_burn_lava_nails) and, with
// vr_burn_touch, a lit torch held or thrown touching it set a monster, a player or a corpse (vr_burn_corpses) on fire.
// The fire (a vr_burn) is one flame where it was struck (id's small flame, progs/flame2.mdl, which lights it), spreading
// over the body: a smaller flame every vr_burn_spread s, turned a little but upright, up to vr_burn_flames; more hits
// add a flame each, up to vr_burn_flames_max. Its damage never stacks: vr_burn_damage a second, whatever the flames, for
// vr_burn_time s from the last hit (vr_burn_corpse_time for a corpse; vr_burn_corpse_damage of it hurts a corpse:
// vr_corpse_gib's health). Burns are painted under every flame as it burns (the wounds' QVR_WOUND_BURN, no wound under
// them: extra 1). The flames sit on the model as drawn (hitmodel_any), fitted again a few times a second as it moves,
// dies and falls. Out when its time is up (the flames shrink away), in water, gibbed or gone.
// `vr_burn_test <n>` (Debug > Burning Tests): 1 a torch's blow, 2 a lava nail, 3 a touch, on the nearest monster or
// corpse; `developer 1` prints `burning: ...`.

// On what burns:
.entity burn_fire;        // its fire (a vr_burn): world, not burning
.float burn_add_next;     // when a hit can next add a flame
// On a vr_burn (enemy: what burns; owner: who lit it):
.entity burn_flames;      // its flames (a chain through burn_next; the first is lit, the rest cast no light)
.entity burn_next;
.float burn_count;        // its flames
.float burn_end;          // when it goes out (the flames shrink away over VR_BURN_FADE s after)
.float burn_tick;         // its next damage
.float burn_spread_next;  // its next flame spreading
.float burn_fit_next;     // when its flames are next fitted to the body
.float burn_model;        // the body's modelindex when lit (gibbed: another)
.float burn_dead;         // the body was dead at the last fit
// On a flame (owner: its fire):
.vector burn_off;         // where on the body: from its origin, in its yaw's axes
.float burn_size;         // its full scale
.float burn_born;         // when it came (it grows in)

float VR_BURN_FADE = 0.5;    // s the flames take to shrink away
float VR_BURN_GROW = 0.3;    // s a new flame takes to grow
float VR_BURN_ADD_GAP = 0.3; // s between flames added by hits (a super nailgun's stream adds one a gap)
float VR_BURN_TICK = 0.5;    // s between its damages

float VR_BURN_BLOW = 0;      // how it was lit (VR_Burn_Ignite): a blow (a mark where it struck),
float VR_BURN_TOUCH = 1;     // a touch (no mark but its flame's),
float VR_BURN_NAIL = 2;      // a missile (its own hit marks it)

// What fire doesn't hurt (VR_Liquid_Immune's: what lives in lava or water, the bosses), but the training dummy burns.
float(entity e) VR_Burn_Fireproof =
{
    return e.classname != "vr_dummy" && VR_Liquid_Immune(e);
};

// Whether `e` can be set on fire: a monster or a player alive, or a monster's corpse (dying or lying: vr_corpse_gib).
float(entity e) VR_Burn_Can =
{
    if(e == world || !e.modelindex || e.vr_gib || VR_Burn_Fireproof(e))
    {
        return FALSE;
    }
    if(e.health > 0)
    {
        return (e.flags & (FL_MONSTER | FL_CLIENT)) && e.takedamage;
    }
    return cvar_hget(cvarh_vr_burn_corpses) && (e.flags & FL_MONSTER) && (e.vr_corpse || e.vr_corpse_dying);
};

// A point from the body's origin in its yaw's axes, and back (no makevectors: callers' v_forward is kept).
vector(entity v, vector p) VR_Burn_ToLocal =
{
    float a = v.angles_y * 0.0174533;
    vector d = p - v.origin;
    vector r;
    r_x = d_x * cos(a) + d_y * sin(a);
    r_y = d_y * cos(a) - d_x * sin(a);
    r_z = d_z;
    return r;
};

vector(entity v, vector o) VR_Burn_ToWorld =
{
    float a = v.angles_y * 0.0174533;
    vector r;
    r_x = o_x * cos(a) - o_y * sin(a);
    r_y = o_x * sin(a) + o_y * cos(a);
    r_z = o_z;
    return v.origin + r;
};

// The point on the body's model as drawn nearest `p` as a flame would sit on it: standing, met from outside level with
// it, towards its middle; lying (dead), met from above. Missed (between its legs, past a lying body's end), tried again
// nearer its drawn middle, a fatter line (it sits a few units off it). Not found (a model the engine doesn't test: its box): `p` kept in the box.
// vr_burn_onbody: whether it was found.
float vr_burn_onbody;
vector(entity v, vector p) VR_Burn_OnBody =
{
    vector mc = modelcentre(v);
    float f = -1;
    float i = 0;
    while(i <= 4)
    {
        vector q = p + (mc - p) * (i / 4);
        vector a;
        vector b;
        if(v.health <= 0)
        {
            a = q;
            a_z = max(v.absmax_z, mc_z) + 48;
            b = q;
            b_z = v.absmin_z - 8;
        }
        else
        {
            b = mc;
            b_z = q_z;
            vector d = q - b;
            d_z = 0;
            if(vlen(d) < 1)
            {
                d = '1 0 0';
            }
            a = b + normalize(d) * 64;
        }
        f = hitmodel_any(v, a, b, i == 0 ? 1 : 4);
        if(f >= 0)
        {
            vr_burn_onbody = TRUE;
            return a + (b - a) * f;
        }
        if(f == -2)
        {
            break;
        }
        i = i + 1;
    }
    vr_burn_onbody = f == -2;
    p_x = min(v.absmax_x, max(v.absmin_x, p_x));
    p_y = min(v.absmax_y, max(v.absmin_y, p_y));
    p_z = min(v.absmax_z, max(v.absmin_z, p_z));
    return p;
};

// A burn painted under a flame (no wound: extra 1), met from outside the body.
void(entity v, vector at, float xAmount) VR_Burn_Scorch =
{
    if(!VR_Wound_Takes(v))
    {
        return;
    }
    vector mid = (v.absmin + v.absmax) * 0.5;
    vector out = at - mid;
    if(v.health <= 0)
    {
        out = '0 0 1'; // lying: from above
    }
    else
    {
        out_z = 0;
    }
    out = vlen(out) < 1 ? '0 0 1' : normalize(out);
    woundevent(v, at + out * 8, out * -1, QVR_WOUND_BURN, xAmount, 1);
};

// A flame on the fire's body at `at` (fitted onto it); xMain: the first, full size and lit.
void(entity xFire, vector at, float xMain) VR_Burn_AddFlame =
{
    entity v = xFire.enemy;
    vector p = VR_Burn_OnBody(v, at);
    entity fl = spawn();
    fl.classname = "vr_burn_flame";
    fl.owner = xFire;
    fl.movetype = MOVETYPE_NONE;
    fl.solid = SOLID_NOT;
    setmodel(fl, "progs/flame2.mdl"); // id's small flame (the first flickers its own light: vr_emissive.cpp)
    fl.frame = 0;
    float size = max(0.05, cvar_hget(cvarh_vr_burn_flame_size));
    if(!xMain)
    {
        size = size * max(0.05, cvar_hget(cvarh_vr_burn_flame_small)) * (0.8 + random() * 0.4);
        fl.alpha = 1; // drawn as ever, but no light of its own: the first flame's lights the body (vr_emissive.cpp)
    }
    fl.burn_size = size;
    fl.burn_born = time;
    fl.scale = size * 0.3;
    float tilt = cvar_hget(cvarh_vr_burn_flame_tilt);
    fl.angles_x = xMain ? 0 : crandom() * tilt;
    fl.angles_y = random() * 360;
    fl.angles_z = xMain ? 0 : crandom() * tilt;
    fl.burn_off = VR_Burn_ToLocal(v, p);
    setorigin(fl, p);
    fl.burn_next = xFire.burn_flames;
    if(xMain || xFire.burn_flames == world)
    {
        xFire.burn_flames = fl;
        fl.alpha = 0;
    }
    else
    {
        // After the first (the lit one stays first).
        fl.burn_next = xFire.burn_flames.burn_next;
        xFire.burn_flames.burn_next = fl;
    }
    xFire.burn_count = xFire.burn_count + 1;
    VR_Burn_Scorch(v, p, 20);
    dprint(sprintf("burning: %s: flame %g at %v (%s, scale %.2f)\n", v.classname, xFire.burn_count, p,
                   vr_burn_onbody ? "on it" : "off it", size));
};

// A flame spread from one of the fire's: a hand's width from it, over the body.
void(entity xFire) VR_Burn_Spread =
{
    entity v = xFire.enemy;
    float pick = floor(random() * xFire.burn_count);
    entity from = xFire.burn_flames;
    while(pick > 0 && from.burn_next != world)
    {
        from = from.burn_next;
        pick = pick - 1;
    }
    float tries = 0;
    while(tries < 3)
    {
        float a = random() * 360 * 0.0174533;
        float r = 8 + random() * 10;
        vector p = from.origin;
        p_x = p_x + cos(a) * r;
        p_y = p_y + sin(a) * r;
        if(v.health > 0)
        {
            p_z = p_z + (random() * 16 - 6); // (fire climbs)
            p_z = min(v.absmax_z - 4, max(v.absmin_z + (v.absmax_z - v.absmin_z) * 0.2, p_z));
        }
        VR_Burn_OnBody(v, p);
        if(vr_burn_onbody)
        {
            VR_Burn_AddFlame(xFire, p, FALSE);
            return;
        }
        tries = tries + 1;
    }
};

// Out: its flames gone, the body no longer burning.
void(entity xFire, string xWhy) VR_Burn_Out =
{
    entity v = xFire.enemy;
    entity fl = xFire.burn_flames;
    while(fl != world)
    {
        entity n = fl.burn_next;
        remove(fl);
        fl = n;
    }
    xFire.burn_flames = world;
    if(v.burn_fire == xFire)
    {
        v.burn_fire = world;
    }
    dprint(sprintf("burning: %s out (%s)\n", v.classname, xWhy));
    remove(xFire);
};

// The fire, every 0.05 s: its flames kept on the body (fitted to it a few times a second), grown in and shrunk away;
// a flame spreading; its damage every VR_BURN_TICK s (one fire's, whatever its flames), burns under its flames.
void() VR_Burn_Think =
{
    entity v = self.enemy;
    if(v == world || v.burn_fire != self || !v.modelindex || v.modelindex != self.burn_model || v.vr_gib)
    {
        VR_Burn_Out(self, "gone or gibbed");
        return;
    }
    if(VR_Wound_Level(v) >= 2)
    {
        particle2((v.absmin + v.absmax) * 0.5, '0 0 30', QVR_PARTICLE_PRESET_BIGSMOKE, 6);
        VR_Burn_Out(self, "in a liquid");
        return;
    }
    float dead = v.health <= 0;
    if(dead && !self.burn_dead)
    {
        // Died burning: it burns on as a corpse, or goes out.
        if(!cvar_hget(cvarh_vr_burn_corpses) || !(v.flags & FL_MONSTER) || VR_Burn_Fireproof(v))
        {
            VR_Burn_Out(self, "died");
            return;
        }
        self.burn_end = max(self.burn_end, time + cvar_hget(cvarh_vr_burn_corpse_time));
        self.burn_fit_next = 0;
        dprint(sprintf("burning: %s died burning: burns as a corpse till %.1f\n", v.classname, self.burn_end));
    }
    if(time >= self.burn_end + VR_BURN_FADE)
    {
        VR_Burn_Out(self, "burnt out");
        return;
    }
    self.nextthink = time + 0.05;

    // Fitted to the body (it moves, dies, falls): each flame onto the model again where it is.
    float fit = time >= self.burn_fit_next || dead != self.burn_dead;
    self.burn_dead = dead;
    if(fit)
    {
        self.burn_fit_next = time + (dead ? 0.15 : 0.25);
    }
    float fade = min(1, max(0, (self.burn_end + VR_BURN_FADE - time) / VR_BURN_FADE));
    entity fl = self.burn_flames;
    while(fl != world)
    {
        vector p = VR_Burn_ToWorld(v, fl.burn_off);
        if(fit)
        {
            vector q = VR_Burn_OnBody(v, p);
            if(vr_burn_onbody)
            {
                p = q;
                fl.burn_off = VR_Burn_ToLocal(v, p);
            }
        }
        setorigin(fl, p);
        float grow = min(1, 0.3 + 0.7 * (time - fl.burn_born) / VR_BURN_GROW);
        fl.scale = fl.burn_size * min(grow, fade);
        fl = fl.burn_next;
    }

    if(time >= self.burn_end)
    {
        return; // (shrinking away: no more spreading or damage)
    }
    if(time >= self.burn_spread_next)
    {
        self.burn_spread_next = time + max(0.05, cvar_hget(cvarh_vr_burn_spread));
        if(self.burn_count < min(cvar_hget(cvarh_vr_burn_flames), cvar_hget(cvarh_vr_burn_flames_max)))
        {
            VR_Burn_Spread(self);
        }
    }
    if(time < self.burn_tick)
    {
        return;
    }
    self.burn_tick = time + VR_BURN_TICK;
    float dmg = cvar_hget(cvarh_vr_burn_damage) * VR_BURN_TICK;
    if(dead)
    {
        dmg = dmg * cvar_hget(cvarh_vr_burn_corpse_damage);
    }
    fl = self.burn_flames;
    while(fl != world)
    {
        particle2(fl.origin, '0 0 40', QVR_PARTICLE_PRESET_LAVASPIKE, 2); // embers
        vector jit;
        jit_x = crandom() * 3; // (the burns spread round it, tick by tick)
        jit_y = crandom() * 3;
        jit_z = crandom() * 3;
        VR_Burn_Scorch(v, fl.origin + jit, min(60, 16 + 4 * (time - fl.burn_born))); // (wider as it burns on)
        fl = fl.burn_next;
    }
    particle2(self.burn_flames.origin, '0 0 20', QVR_PARTICLE_PRESET_SMOKE, 2);
    if(dmg > 0 && v.takedamage)
    {
        dprint(sprintf("burning: %s burns for %g (%g flames, health %g)\n", v.classname, dmg, self.burn_count, v.health));
        VR_SetHitKind(QVR_HITKIND_NONE, -1, self.burn_flames.origin, '0 0 -1');
        T_Damage(v, self, self.owner, dmg); // (vr_debug_shots logs it: "by vr_burn")
        VR_ClearHitKind();
    }
};

// Sets `xVictim` on fire (or keeps it burning: its time from now again, a flame more), lit by `xAttacker` at `xAt`
// going `xDir`, by xHow (VR_BURN_BLOW, VR_BURN_TOUCH, VR_BURN_NAIL). Its damage is one fire's however often it is lit.
void(entity xVictim, entity xAttacker, vector xAt, vector xDir, float xHow) VR_Burn_Ignite =
{
    if(!VR_Burn_Can(xVictim) || xVictim == xAttacker)
    {
        return;
    }
    float dead = xVictim.health <= 0;
    float len = cvar_hget(dead ? cvarh_vr_burn_corpse_time : cvarh_vr_burn_time);
    if(len <= 0 || (!dead && cvar_hget(cvarh_vr_burn_damage) <= 0))
    {
        return;
    }
    if(xDir == '0 0 0')
    {
        xDir = '0 0 -1';
    }
    if(xHow == VR_BURN_BLOW && !dead)
    {
        woundevent(xVictim, xAt, normalize(xDir), QVR_WOUND_BURN, 14, 0); // scorched where the head struck
    }
    entity f = xVictim.burn_fire;
    float fresh = f == world || f.classname != "vr_burn" || f.enemy != xVictim;
    if(fresh)
    {
        f = spawn();
        f.classname = "vr_burn";
        f.enemy = xVictim;
        f.movetype = MOVETYPE_NONE;
        f.solid = SOLID_NOT;
        f.burn_model = xVictim.modelindex;
        f.burn_dead = dead;
        f.burn_tick = time + VR_BURN_TICK;
        f.burn_spread_next = time + max(0.05, cvar_hget(cvarh_vr_burn_spread));
        f.think = VR_Burn_Think;
        f.nextthink = time;
        setorigin(f, xVictim.origin);
        xVictim.burn_fire = f;
        sound(xVictim, CHAN_AUTO, "vr/torch_light.wav", 0.6, ATTN_NORM);
        VR_Burn_AddFlame(f, xAt, TRUE);
        xVictim.burn_add_next = time + VR_BURN_ADD_GAP;
    }
    else if(time >= xVictim.burn_add_next && f.burn_count < cvar_hget(cvarh_vr_burn_flames_max))
    {
        xAt_x = xAt_x + crandom() * 5; // (hits on one spot: flames side by side)
        xAt_y = xAt_y + crandom() * 5;
        VR_Burn_AddFlame(f, xAt, FALSE);
        xVictim.burn_add_next = time + VR_BURN_ADD_GAP;
    }
    f.owner = xAttacker;
    f.burn_end = max(f.burn_end, time + len); // (never longer than one lighting's time from the last)
    dprint(sprintf("burning: %s lit (%s) by %s: burns till %.1f, %g flames\n", xVictim.classname,
                   xHow == VR_BURN_BLOW ? "a blow" : (xHow == VR_BURN_TOUCH ? "a touch" : "a missile"),
                   xAttacker.classname, f.burn_end, f.burn_count));
};

// A lava nail struck `other` (lavaspike_touch, superlavaspike_touch; self the nail): yours set it on fire.
void() VR_Burn_LavaNail =
{
    if(cvar_hget(cvarh_vr_burn_lava_nails) && (self.owner.flags & FL_CLIENT))
    {
        VR_Burn_Ignite(other, self.owner, self.origin, self.velocity, VR_BURN_NAIL);
    }
};

// Whether a torch's head at `xHead` touches `e`: its model as drawn (a ball of 8 round the head), else its box.
float(entity e, vector xHead) VR_Burn_Touches =
{
    float r = 8;
    if(xHead_x < e.absmin_x - r || xHead_x > e.absmax_x + r || xHead_y < e.absmin_y - r || xHead_y > e.absmax_y + r ||
       xHead_z < e.absmin_z - r || xHead_z > e.absmax_z + r)
    {
        return FALSE;
    }
    float f = hitmodel_any(e, xHead, xHead + '0 0 1', r);
    return f >= 0 || f == -2;
};

// A lit torch, held or thrown or lying, every 0.1 s (VR_WallTorch_Think): with vr_burn_touch, what its head touches
// catches fire. xBy: who holds it or threw it.
void(entity t, vector xHead, entity xBy) VR_Burn_TorchTouch =
{
    if(!cvar_hget(cvarh_vr_burn_touch))
    {
        return;
    }
    entity e = findradius(xHead, 96);
    while(e)
    {
        if(e != xBy && e != t && e.burn_add_next <= time && VR_Burn_Can(e) && VR_Burn_Touches(e, xHead))
        {
            VR_Burn_Ignite(e, xBy == world ? t : xBy, xHead, t.velocity, VR_BURN_TOUCH);
        }
        e = e.chain;
    }
};

// ----------------------------------------------------------------------------
// Tests (vr_burn_test, Debug > Burning Tests): set on fire the monster or corpse nearest the player, as a torch's
// blow (1), a lava nail (2) or a touch (3) would, at the point on it facing the player; 4: how it burns (the console);
// 5: lava nails loaded in the main hand's nailgun.

void() VR_Burn_TestFrame =
{
    float ask = cvar("vr_burn_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_burn_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    if(ask == 5)
    {
        // The main hand's nailgun or super nailgun loaded with lava nails (no reload needed).
        float wid = VRGetEntWeapon(p, cVR_MainHand);
        if(wid != WID_NAILGUN && wid != WID_SUPER_NAILGUN)
        {
            dprint("burning: test 5: no nailgun in the main hand\n");
            return;
        }
        p.ammo_lava_nails = max(p.ammo_lava_nails, 100);
        VRSetEntWeaponFlags(p, cVR_MainHand, VRGetEntWeaponFlags(p, cVR_MainHand) | QVR_WPNFLAG_USE_SECONDARY_AMMO);
        VRSetEntWeaponClip(p, cVR_MainHand, WeaponIdToBaseClipSize(wid));
        entity oself = self;
        self = p;
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = oself;
        dprint("burning: test 5: lava nails loaded in the main hand\n");
        return;
    }
    entity best = world;
    float bestd = 100000;
    entity e = findradius(p.origin, 1024);
    while(e)
    {
        if(e != p && (e.flags & FL_MONSTER) && e.modelindex && vlen(e.origin - p.origin) < bestd)
        {
            best = e;
            bestd = vlen(e.origin - p.origin);
        }
        e = e.chain;
    }
    if(best == world)
    {
        dprint("burning: test: no monster or corpse near\n");
        return;
    }
    if(ask == 4)
    {
        entity f = best.burn_fire;
        string how = "not burning";
        if(f != world)
        {
            how = sprintf("%g flames, %.1f s left", f.burn_count, f.burn_end - time);
        }
        dprint(sprintf("burning: %s at %v, health %g, corpse %g: %s\n", best.classname, best.origin, best.health,
                      best.vr_corpse, how));
        return;
    }
    vector mid = (best.absmin + best.absmax) * 0.5;
    vector at = mid + normalize(p.origin - mid) * 64;
    at_z = mid_z + (best.health > 0 ? (best.absmax_z - mid_z) * 0.3 : 0);
    at = VR_Burn_OnBody(best, at);
    dprint(sprintf("burning: test %g on %s (health %g, corpse %g)\n", ask, best.classname, best.health, best.vr_corpse));
    VR_Burn_Ignite(best, p, at, mid - p.origin, ask == 2 ? VR_BURN_NAIL : (ask == 3 ? VR_BURN_TOUCH : VR_BURN_BLOW));
};

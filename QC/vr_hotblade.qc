//
//
//
// ----------------------------------------------------------------------------
// Incandescent blades
// ----------------------------------------------------------------------------
//
// A melee weapon's blade (the axe and AD's Shadow Axe, a knight's sword, the crowbar, Dawn of the Machine's Super Axe,
// the ogres' chainsaw; never Mjolnir) kept in a flame for vr_hotblade_heat_time becomes incandescent: it glows orange-red
// (the engine's glow on what the hand holds or a holster shows: vr_meleehud.cpp, from .vr_hot_hud), and for
// vr_hotblade_time every blow of it deals vr_hotblade_damage more base damage and sets what it strikes on fire, alive
// (vr_hotblade_ignite) or dead (vr_hotblade_ignite_corpses; vr_burning.qc). The flames are those a dying torch lights
// again in (VR_WallTorch_FireAt: a torch on its wall, a lit torch held or lying about, the map's flames and braziers,
// a lava ball, lava). While it builds up its glow rises and it sizzles; a flame again while it glows sets it back to
// full; its last vr_hotblade_fade seconds the glow fades out. Water or slime quenches it at once (vr_hotblade_quench).
// The heat is the weapon's record's (vr_weaponinst.qc: wi_heat, wi_heat_at, wi_hot_end): it cools on its own clock
// held, holstered or dropped, and is kept in a save. A level change starts the weapons cold.

float VR_HOT_TICK = 0.1;     // s between two looks at the blades (as a taken torch's think)
float VR_HOT_GRACE = 0.3;    // s out of the flame before the build-up starts falling off (a tick's gap is not out)
float VR_HOT_BUILD_GLOW = 0.6; // the glow of a blade fully built up but not yet incandescent (the glow while hot: 1)

.float vr_hot_next;    // on a player: the next look at the blades
.float vr_hot_last;    // the last
.float vr_hot_sizzle0; // and each hand's next sizzle (the off hand's, the main hand's)
.float vr_hot_sizzle1;
.float vr_hot_hud;     // the hands' glow for the engine (STAT_QVR_HOTBLADE): 0..255 each, the off hand's in bits 0-7,
                       // the main hand's in 8-15
.float vr_hot_hud2;    // the holsters' (STAT_QVR_HOTBLADEHOLSTER): 0..15 each, holster i in bits 4i..4i+3

// Whether weapon `xWid` heats: a blade or a bar (not Mjolnir: its head is a hammer of the gods, not iron).
float(float xWid) VR_HotBlade_Heatable =
{
    return xWid == WID_AXE || xWid == WID_SWORD || xWid == WID_CROWBAR || xWid == WID_SUPERAXE || xWid == WID_CHAINSAW;
};

// Record `r`'s build-up now, 0..1 (falling off out of the flame as fast as it rose).
float(entity r) VR_HotBlade_BuildUp =
{
    if(r == world || r.wi_heat <= 0)
    {
        return 0;
    }
    float out = max(0, time - r.wi_heat_at - VR_HOT_GRACE);
    return max(0, r.wi_heat - out / max(0.1, cvar_hget(cvarh_vr_hotblade_heat_time)));
};

// Whether record `r` is incandescent now.
float(entity r) VR_HotBlade_IsHot =
{
    return r != world && time < r.wi_hot_end && cvar_hget(cvarh_vr_hotblade) != 0;
};

// How record `r` glows now, 0..1: 1 incandescent, fading over its last vr_hotblade_fade; building up, up to
// VR_HOT_BUILD_GLOW.
float(entity r) VR_HotBlade_Glow =
{
    if(r == world || !cvar_hget(cvarh_vr_hotblade))
    {
        return 0;
    }
    if(time < r.wi_hot_end)
    {
        float left = r.wi_hot_end - time;
        float fade = max(0, cvar_hget(cvarh_vr_hotblade_fade));
        return (fade > 0 && left < fade) ? left / fade : 1;
    }
    return VR_HOT_BUILD_GLOW * VR_HotBlade_BuildUp(r);
};

// The weapon `xHand` of `p` holds, if it is an incandescent blade (world: not).
entity(entity p, float xHand) VR_HotBlade_HotIn =
{
    entity r = VRGetEntWeaponInst(p, xHand);
    if(!VR_HotBlade_Heatable(VRGetEntWeapon(p, xHand)) || !VR_HotBlade_IsHot(r))
    {
        return world;
    }
    return r;
};

// The base damage an incandescent blade in `xHand` of `p` adds to its blow (VRMeleeDmg); 0 for anything else.
float(entity p, float xHand) VR_HotBlade_Extra =
{
    if(VR_HotBlade_HotIn(p, xHand) == world)
    {
        return 0;
    }
    return max(0, cvar_hget(cvarh_vr_hotblade_damage));
};

// A blow (or a running chain's cut) of `xHand` of player `p` landed on `xTarg` at `xAt` going `xDir`; `xAlive`: it
// was alive before it. An incandescent blade sets it on fire (T_Damage_VRMelee, VR_Corpse_StrikeFrame, VR_Chainsaw_Cut).
void(entity p, float xHand, entity xTarg, float xAlive, vector xAt, vector xDir) VR_HotBlade_Struck =
{
    if(xTarg == world || xTarg == p || !(p.flags & FL_CLIENT) || VR_HotBlade_HotIn(p, xHand) == world)
    {
        return;
    }
    if(!cvar_hget(xAlive ? cvarh_vr_hotblade_ignite : cvarh_vr_hotblade_ignite_corpses))
    {
        return;
    }
    if(xAt == '0 0 0')
    {
        xAt = (xTarg.absmin + xTarg.absmax) * 0.5;
    }
    dprint(sprintf("hotblade: hand %g struck %s (%s): set on fire\n", xHand, xTarg.classname, xAlive ? "alive" : "a corpse"));
    VR_Burn_Ignite(xTarg, p, xAt, xDir, VR_BURN_BLOW);
};

// Whether `xThing` is a weapon prop (thrown, dropped) whose blade is incandescent.
float(entity xThing) VR_HotBlade_ThingHot =
{
    return xThing.classname == "thrown_weapon" && VR_HotBlade_Heatable(xThing.weapon) && VR_HotBlade_IsHot(xThing.weaponinst);
};

// The base damage an incandescent blade thrown adds to its hit (VR_Thrown_Damage: as its blows', times the throw's speed
// and the rest); 0 for anything else.
float(entity xThing) VR_HotBlade_ThrownExtra =
{
    return VR_HotBlade_ThingHot(xThing) ? max(0, cvar_hget(cvarh_vr_hotblade_damage)) : 0;
};

// A thrown weapon `xThing` (thrown by `xBy`) hit `xTarg` at `xAt` going `xDir` (forcegrabbable_touch); `xAlive`: it was
// alive before. An incandescent blade sets it on fire, as its blows do.
void(entity xThing, entity xTarg, entity xBy, float xAlive, vector xAt, vector xDir) VR_HotBlade_ThrownStruck =
{
    if(xTarg == world || xTarg == xBy || !VR_HotBlade_ThingHot(xThing))
    {
        return;
    }
    if(!cvar_hget(xAlive ? cvarh_vr_hotblade_ignite : cvarh_vr_hotblade_ignite_corpses))
    {
        return;
    }
    dprint(sprintf("hotblade: a thrown weapon %g struck %s (%s): set on fire\n", xThing.weapon, xTarg.classname,
        xAlive ? "alive" : "a corpse"));
    VR_Burn_Ignite(xTarg, xBy != world ? xBy : xThing, xAt, xDir, VR_BURN_BLOW);
};

// The fire point `p` is in (VR_WallTorch_FireAt's: lava, a flame, a lit torch), "" none.
string(vector p) VR_HotBlade_FireAt =
{
    return VR_WallTorch_FireAt(world, p);
};

// Until when some weapon may still be incandescent (the lying weapons are looked at only until then).
// (vr_hot_any_until: vr_weaponinst.qc)
float vr_hot_lying_next;

// Record `r` becomes (or stays) incandescent: full again.
void(entity r) VR_HotBlade_Heat =
{
    r.wi_hot_end = time + max(0.1, cvar_hget(cvarh_vr_hotblade_time));
    r.wi_heat = 0;
    vr_hot_any_until = max(vr_hot_any_until, r.wi_hot_end);
};

// Every VR_HOT_TICK while a weapon may be hot (and a tick after, to set the last glows to 0): the weapons lying about or
// flying (thrown, dropped, placed: thrown_weapon props) tell the engine their glow (.vr_hot_glow, sent with the prop:
// U_QVR_WEAPONUID; it draws them glowing, lit and shedding embers as in a hand), and one that lies in water or slime is
// quenched. Their heat is their record's: it ticks down there as anywhere.
void() VR_HotBlade_Lying =
{
    if(time < vr_hot_lying_next || (time > vr_hot_any_until + 2 * VR_HOT_TICK && vr_hot_any_until > 0))
    {
        return;
    }
    vr_hot_lying_next = time + VR_HOT_TICK;
    if(vr_hot_any_until <= 0)
    {
        vr_hot_any_until = time + max(0.1, cvar_hget(cvarh_vr_hotblade_time)); // (after a load: look a while)
    }
    float vol = min(1, max(0, cvar_hget(cvarh_vr_hotblade_sound)));
    entity t;
    for(t = find(world, classname, "thrown_weapon"); t; t = find(t, classname, "thrown_weapon"))
    {
        entity r = t.weaponinst;
        float g = VR_HotBlade_Heatable(t.weapon) ? VR_HotBlade_Glow(r) : 0;
        t.vr_hot_glow = g;
        if(g <= 0)
        {
            continue;
        }
        vector mid = (t.absmin + t.absmax) * 0.5;
        float c = pointcontents(mid);
        if((c == CONTENT_WATER || c == CONTENT_SLIME) && cvar_hget(cvarh_vr_hotblade_quench))
        {
            t.vr_hot_glow = 0;
            dprint(sprintf("hotblade: a lying weapon %g quenched\n", t.weapon));
            VR_HotBlade_Cool(r);
            if(vol > 0)
            {
                sound(t, CHAN_AUTO, "vr/blade_quench.wav", vol, ATTN_NORM);
            }
            particle2(mid + '0 0 4', '0 0 40', QVR_PARTICLE_PRESET_BIGSMOKE, 6);
        }
    }
};

// Record `r` cooled at once (the water's).
void(entity r) VR_HotBlade_Cool =
{
    r.wi_hot_end = 0;
    r.wi_heat = 0;
};

// Every VR_HOT_TICK for `xHand` of self: its blade's points (the melee's, as last tracked: VR_Melee_Layout's striking
// points, not the hilt, the handle or the engine) in a flame heat it, in water quench it.
void(float xHand, float dt) VR_HotBlade_HandTick =
{
    float wid = VRGetEntWeapon(self, xHand);
    entity r = VRGetEntWeaponInst(self, xHand);
    if(r == world || !VR_HotBlade_Heatable(wid))
    {
        return;
    }
    entity b = VR_Blow(xHand);
    if(b.mh_n <= 0 || b.mh_wid != wid)
    {
        return;
    }
    VR_Melee_LayoutOf(b);
    float n = min(b.mh_n, vr_mpt_n);
    string fire = "";
    float wet = FALSE;
    vector at = '0 0 0';
    float i;
    for(i = 0; i < n; ++i)
    {
        if(vr_mpt_kind[i] != VR_MPT_EDGE)
        {
            continue;
        }
        vector p = b.mh_cp[i];
        float c = pointcontents(p);
        if(c == CONTENT_WATER || c == CONTENT_SLIME)
        {
            wet = TRUE;
            at = p;
        }
        else if(fire == "")
        {
            fire = VR_HotBlade_FireAt(p);
            if(fire != "")
            {
                at = p;
            }
        }
    }
    float vol = min(1, max(0, cvar_hget(cvarh_vr_hotblade_sound)));
    if(wet)
    {
        if(cvar_hget(cvarh_vr_hotblade_quench) && (time < r.wi_hot_end || VR_HotBlade_BuildUp(r) > 0))
        {
            dprint(sprintf("hotblade: hand %g weapon %g quenched (glow %.2f)\n", xHand, wid, VR_HotBlade_Glow(r)));
            VR_HotBlade_Cool(r);
            if(vol > 0)
            {
                sound(self, CHAN_AUTO, "vr/blade_quench.wav", vol, ATTN_NORM);
            }
            particle2(at + '0 0 4', '0 0 40', QVR_PARTICLE_PRESET_BIGSMOKE, 6); // steam
            particle2(at + '0 0 2', '0 0 20', QVR_PARTICLE_PRESET_SMOKE, 4);
            haptic(xHand, 0, 0.25, 90, 0.35);
        }
        return;
    }
    if(fire == "")
    {
        return;
    }
    if(time < r.wi_hot_end)
    {
        // Glowing: a flame again sets it back to full, and the hand feels it heat (the author: reheating a blade not yet
        // cold must be felt too), a steady buzz as the build-up's at its top.
        if(r.wi_hot_end - time < max(0.1, cvar_hget(cvarh_vr_hotblade_time)) - 1)
        {
            dprint(sprintf("hotblade: hand %g weapon %g full again (%s): reheating, a buzz\n", xHand, wid, fire));
        }
        VR_HotBlade_Heat(r);
        haptic(xHand, 0, VR_HOT_TICK + 0.02, 120, 0.4);
    }
    else
    {
        float h = VR_HotBlade_BuildUp(r) + dt / max(0.1, cvar_hget(cvarh_vr_hotblade_heat_time));
        if(h >= 1)
        {
            VR_HotBlade_Heat(r);
            dprint(sprintf("hotblade: hand %g weapon %g incandescent (%s)\n", xHand, wid, fire));
            if(vol > 0)
            {
                sound(self, CHAN_AUTO, "vr/blade_hot.wav", vol, ATTN_NORM);
            }
            particle2(at, '0 0 30', QVR_PARTICLE_PRESET_SPARKS, 10);
            haptic(xHand, 0, 0.2, 70, 0.6);
        }
        else
        {
            if(r.wi_heat <= 0 || h < 0.15)
            {
                dprint(sprintf("hotblade: hand %g weapon %g heating (%s)\n", xHand, wid, fire));
            }
            r.wi_heat = h;
            r.wi_heat_at = time;
            vr_hot_any_until = max(vr_hot_any_until, time + max(0.1, cvar_hget(cvarh_vr_hotblade_heat_time)) + VR_HOT_GRACE);
            // A buzz rising with the build-up (felt from the first tick: it was 0.1 at first, hard to tell).
            haptic(xHand, 0, VR_HOT_TICK + 0.02, 120, 0.2 + 0.3 * h);
        }
    }
    // Sizzling in the flame (twice a second).
    float due = xHand == cVR_OffHand ? self.vr_hot_sizzle0 : self.vr_hot_sizzle1;
    if(time >= due)
    {
        if(vol > 0)
        {
            sound(self, CHAN_AUTO, "vr/blade_sizzle.wav", 0.7 * vol, ATTN_IDLE);
        }
        if(xHand == cVR_OffHand)
        {
            self.vr_hot_sizzle0 = time + 0.45;
        }
        else
        {
            self.vr_hot_sizzle1 = time + 0.45;
        }
    }
};

// The glow of record `r` in `xLevels` steps (0 .. xLevels).
float(entity r, float xLevels) VR_HotBlade_Level =
{
    return rint(xLevels * min(1, max(0, VR_HotBlade_Glow(r))));
};

// What the engine draws (.vr_hot_hud, .vr_hot_hud2: STAT_QVR_HOTBLADE*): the hands' weapons' glow and the holsters'.
void() VR_HotBlade_Hud =
{
    float hud = 0;
    if(VR_HotBlade_Heatable(self.weapon2))
    {
        hud = VR_HotBlade_Level(self.weaponinst2, 255);
    }
    if(VR_HotBlade_Heatable(self.weapon))
    {
        hud = hud + 256 * VR_HotBlade_Level(self.weaponinst, 255);
    }
    self.vr_hot_hud = hud;
    self.vr_hot_hud2 = VR_HotBlade_Level(self.holsterweaponinst0, 15) + 16 * VR_HotBlade_Level(self.holsterweaponinst1, 15) +
        256 * VR_HotBlade_Level(self.holsterweaponinst2, 15) + 4096 * VR_HotBlade_Level(self.holsterweaponinst3, 15) +
        65536 * VR_HotBlade_Level(self.holsterweaponinst4, 15) + 1048576 * VR_HotBlade_Level(self.holsterweaponinst5, 15);
};

// The option turned off: the weapons lying about or flying told once their glow is gone (the engine draws one by its
// .vr_hot_glow alone: it would keep glowing until the option was on again).
float vr_hot_off_cleared;

// Each player frame (PlayerVRMelee, after VR_Melee_Frame: the blades' points as tracked this frame).
void() VR_HotBlade_Frame =
{
    if(!cvar_hget(cvarh_vr_hotblade))
    {
        self.vr_hot_hud = 0;
        self.vr_hot_hud2 = 0;
        if(!vr_hot_off_cleared)
        {
            vr_hot_off_cleared = TRUE;
            entity t;
            for(t = find(world, classname, "thrown_weapon"); t; t = find(t, classname, "thrown_weapon"))
            {
                t.vr_hot_glow = 0;
            }
        }
        return;
    }
    vr_hot_off_cleared = FALSE;
    if(time >= self.vr_hot_next || self.vr_hot_next > time + 1)
    {
        float dt = min(0.2, max(0, time - self.vr_hot_last));
        self.vr_hot_last = time;
        self.vr_hot_next = time + VR_HOT_TICK;
        VR_HotBlade_HandTick(cVR_OffHand, dt);
        VR_HotBlade_HandTick(cVR_MainHand, dt);
    }
    VR_HotBlade_Lying();
    VR_HotBlade_Hud();
};

// ----------------------------------------------------------------------------
// Tests (vr_hotblade_test <n>, Debug > Tests > Incandescent Blades; developer 1 prints "hotblade: ..." lines)

// For the tests' report: record `r` of weapon `xWid`.
string(entity r, float xWid) VR_HotBlade_Desc =
{
    if(r == world)
    {
        return "none";
    }
    float left = max(0, r.wi_hot_end - time);
    return sprintf("wid %g #%g heat %.2f glow %.2f hot %g left %.1f", xWid, r.wi_uid, VR_HotBlade_BuildUp(r),
        VR_HotBlade_Glow(r), VR_HotBlade_IsHot(r), left);
};

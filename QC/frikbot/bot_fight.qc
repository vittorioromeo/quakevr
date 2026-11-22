/***********************************************
*                                              *
*             FrikBot Fight Code               *
*      "Because I ain't no Ghandi code"        *
*                                              *
***********************************************/

/*

This program is in the Public Domain. My crack legal
team would like to add:

RYAN "FRIKAC" SMITH IS PROVIDING THIS SOFTWARE "AS IS"
AND MAKES NO WARRANTY, EXPRESS OR IMPLIED, AS TO THE
ACCURACY, CAPABILITY, EFFICIENCY, MERCHANTABILITY, OR
FUNCTIONING OF THIS SOFTWARE AND/OR DOCUMENTATION. IN
NO EVENT WILL RYAN "FRIKAC" SMITH BE LIABLE FOR ANY
GENERAL, CONSEQUENTIAL, INDIRECT, INCIDENTAL,
EXEMPLARY, OR SPECIAL DAMAGES, EVEN IF RYAN "FRIKAC"
SMITH HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES, IRRESPECTIVE OF THE CAUSE OF SUCH DAMAGES.

You accept this software on the condition that you
indemnify and hold harmless Ryan "FrikaC" Smith from
any and all liability or damages to third parties,
including attorney fees, court costs, and other
related costs and expenses, arising out of your use
of this software irrespective of the cause of said
liability.

The export from the United States or the subsequent
reexport of this software is subject to compliance
with United States export control and munitions
control restrictions. You agree that in the event you
seek to export this software, you assume full
responsibility for obtaining all necessary export
licenses and approvals and for assuring compliance
with applicable reexport restrictions.

Any reproduction of this software must contain
this notice in its entirety.

*/

.entity avoid;

float(entity e, float wid) ent_is_holding =
{
    return e.weapon == wid || e.weapon2 == wid;
}

float(entity e) bot_size_player =
{
    float sz;

    sz = e.health + e.armorvalue * e.armortype;

    if(ent_is_holding(e, WID_ROCKET_LAUNCHER))
        sz = sz + 60;
    else if(ent_is_holding(e, WID_LIGHTNING))
        sz = sz + 60;
    else if(ent_is_holding(e, WID_GRENADE_LAUNCHER))
        sz = sz + 50;
    else if(ent_is_holding(e, WID_SUPER_NAILGUN))
        sz = sz + 50;
    else if(ent_is_holding(e, WID_NAILGUN))
        sz = sz + 40;
    else if(ent_is_holding(e, WID_SUPER_SHOTGUN))
        sz = sz + 40;
    else if(ent_is_holding(e, WID_SHOTGUN))
        sz = sz + 10;
    else if(ent_is_holding(e, WID_AXE))
        sz = sz - 50;

    if(VR_ItemUtil_EntHasItem(e, IID_QUAD))
        sz = sz + 200;

    if(VR_ItemUtil_EntHasItem(e, IID_INVULNERABILITY))
        sz = sz + 300;

    if(VR_ItemUtil_EntHasItem(e, IID_INVISIBILITY))
        sz = sz + 250;

    return sz;
};

void() bot_dodge_stuff =
{
    entity foe;
    float avdist, foesz, flen, tsz;
    vector v;

    if(waypoint_mode > WM_LOADED)
        return;

    self.avoid = world;


    if(self.enemy)
    {
        v = self.origin - realorigin(self.enemy);

        foesz = bot_size_player(self.enemy);
        foesz = foesz + vlen(v) * 0.5;
    }
    else
        foesz = 9999999;
    avdist = 256;

    foe = find(world, classname, "grenade");
    while(foe)
    {
        flen = vlen(foe.origin - self.origin);
        if(flen < avdist)
        {
            avdist = flen;
            self.avoid = foe;
        }
        foe = find(foe, classname, "grenade");
    }
    if(!self.avoid)
    {
        foe = find(world, classname, "missile");
        while(foe)
        {
            if(foe.owner != self)
            {
                flen = vlen(foe.origin - self.origin);
                if(flen < avdist)
                {
                    avdist = flen;
                    self.avoid = foe;
                }
            }
            foe = find(foe, classname, "missile");
        }
        if(!self.avoid)
        {
            foe = find(world, classname, "spike");
            while(foe)
            {
                if(foe.owner != self)
                {
                    flen = vlen(foe.origin - self.origin);
                    if(flen < avdist)
                    {
                        avdist = flen;
                        self.avoid = foe;
                    }
                }
                foe = find(foe, classname, "spike");
            }
        }
    }
    if(coop)
    {
        if(!self.enemy)
        {
            foe = findradius(self.origin, 9999);
            while(foe)
            {
                if(foe.flags & FL_MONSTER)
                {
                    if(foe.health > 0)
                    {
                        tsz = bot_size_player(foe) + vlen(foe.origin - self.origin) * 0.5;
                        if(tsz < foesz)
                        {
                            if(fisible(foe))
                            {
                                self.enemy = foe;
                                foesz = tsz;
                            }
                        }
                    }
                }
                foe = foe.chain;
            }
        }
    }
    else
    {
        foe = player_head;
        while(foe)
        {
            if(foe != self)
            {
                if(foe.modelindex != 0)
                {
                    if(foe.health > 0)
                    {
                        if(!(teamplay && self.team == foe.team))
                        {
                            tsz = bot_size_player(foe) + vlen(foe.origin - self.origin) * 0.5;
                            if(tsz < foesz)
                            {
                                // (QVR: and near enough for its skill to notice, bot_notice: heard, anywhere)
                                if((fov(foe) && vlen(foe.origin - self.origin) <= bot_notice(self)) ||
                                   foe.b_sound > time || self.b_skill == 3)
                                {
                                    if(fisible(foe))
                                    {
                                        self.enemy = foe;
                                        foesz = tsz;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            foe = foe._next;
        }
    }
};




/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

weapon_range

_x "sweet spot range" - try to maintain this range if possible
_y minimum range bot can be to be effective (rl/gl) (move away)
_z maximum range bot can be to be effective (lg/axe) (move in)
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

vector(float wid) weapon_range =
{
    if(wid == WID_AXE)              { return '48 0 64'; }
    if(wid == WID_MJOLNIR)          { return '48 0 64'; }
    if(wid == WID_SWORD || wid == WID_CROWBAR || wid == WID_SUPERAXE) { return '48 0 64'; } // QVR: its melee weapon
    if(wid == WID_SHOTGUN)        { return '128 8 3000'; }
    if(wid == WID_SUPER_SHOTGUN)    { return '128 0 3000'; }
    if(wid == WID_NAILGUN)          { return '180 0 3000'; }
    if(wid == WID_SUPER_NAILGUN)    { return '180 0 3000'; }
    if(wid == WID_GRENADE_LAUNCHER) { return '180 48 3000'; }
    if(wid == WID_PROXIMITY_GUN)    { return '180 48 3000'; }
    if(wid == WID_ROCKET_LAUNCHER)  { return '180 48 3000'; }
    if(wid == WID_LIGHTNING)        { return '350 0 512'; }
    if(wid == WID_LASER_CANNON)     { return '350 0 512'; }

    // QVR: every other weapon by its kind (the armed humanoids' classes, VR_AG_Class): Quake VR's own (the revolver, the
    // nail pistol, the flare gun), the enemies' guns, AD's, the chainsaw (a bprint for each before: his revolver run's bot
    // never chose it)
    float c = VR_AG_Class(wid);
    if(c == VR_AG_CLASS_MELEE || c == VR_AG_CLASS_FISTS) { return '48 0 64'; }
    if(c == VR_AG_CLASS_EXPLOSIVE)                         { return '180 48 3000'; }
    if(c == VR_AG_CLASS_AUTO)                              { return '180 0 3000'; }
    return '128 8 3000';
};
/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bot_weapon_switch

Pick a weapon based on range / ammo

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

// QVR: a gun the bot holds and the run lets it fire (Melee Only refuses every gun: VR_Run_RefusesFire, vr_run.qc).
float(float wid) bot_has_usable =
{
    return VR_WeaponUtil_EntHasWeapon(self, wid) && !VR_Run_RefusesFire(wid);
};

// QVR: the bot's melee weapon, the one it swings when no gun suits: its melee holster's (the run's Starting melee
// weapon, vr_run_startgun.qc: the axe or the dagger), else the axe (bare fists: a bot cannot punch, so the axe as ever).
float() bot_melee_weapon =
{
    float m = self.holsterweapon2;
    if(m == WID_SWORD || m == WID_CROWBAR || m == WID_SUPERAXE) { return m; }
    return WID_AXE;
};

// QVR: the guns a bot chooses from, the best first (FrikBot's order, with Quake VR's and the mission packs' own in it:
// his voice note 2026-11-21, a bot with the revolver loadout only ever swung its dagger, the revolver not in the list).
// The single-use guns (the enemies', AD's, the flare gun: their only ammo their clip) are not drawn from its
// inventory: its off hand takes them up and fires them until spent (bot_offhand_ai).
float BOT_PICKS = 11;
float(float i) bot_pick_nth =
{
    return i == 0 ? WID_ROCKET_LAUNCHER : i == 1 ? WID_LIGHTNING : i == 2 ? WID_LASER_CANNON : i == 3 ? WID_SUPER_NAILGUN :
        i == 4 ? WID_GRENADE_LAUNCHER : i == 5 ? WID_PROXIMITY_GUN : i == 6 ? WID_SUPER_SHOTGUN : i == 7 ? WID_NAILGUN :
        i == 8 ? WID_REVOLVER : i == 9 ? WID_SHOTGUN : WID_NAILPISTOL;
};

// QVR: a bot never reloads, so its guns fire from its ammo, never a clip (VRIsWeaponReloadingEnabledFor: humans only).
// A loaded gun in its hands (taken from a player by a disarm, picked up, the revolver loadout's) has its clip's rounds
// put with its ammo, each AI frame (his voice note 2026-11-22: a bot holding a revolver it took aimed and never shot:
// its own shells spent, it clicked on the six in the clip, or dropped it for its axe). vr_dm_test 14.
void() bot_clips_to_ammo =
{
    for(float hand = 0; hand < 2; hand++)
    {
        float xHand = hand == 0 ? cVR_MainHand : cVR_OffHand;
        float n = VRGetEntWeaponClip(self, xHand);
        float wid = VRGetEntWeapon(self, xHand);
        if(n > 0 && WeaponIdToSupportsReload(wid))
        {
            float aid = WeaponIdToAmmoId(wid, VRGetEntWeaponFlags(self, xHand));
            if(aid != AID_NONE)
            {
                VR_AmmoUtil_SetAmmoCountByAmmoId(self, aid, VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid) + n);
            }
            VRSetEntWeaponClip(self, xHand, 0);
        }
    }
};

// QVR: whether the bot has `wid` and the ammo for a shot of it (two for the double-barrelled guns' two shells and the
// super nailgun's two nails; the lightning gun not under water). Its guns' clips are its ammo (bot_clips_to_ammo).
float(float wid) bot_can_fire =
{
    if(!bot_has_usable(wid) || (wid == WID_LIGHTNING && self.waterlevel > 1))
    {
        return FALSE;
    }
    float aid = WeaponIdToAmmoId(wid, 0);
    if(aid == AID_NONE)
    {
        return TRUE;
    }
    float need = (wid == WID_SUPER_SHOTGUN || wid == WID_SUPER_NAILGUN) ? 2 : 1;
    return VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid) >= need;
};

float(float brange) bot_pick_weapon =
{
    for(float i = 0; i < BOT_PICKS; i++)
    {
        float wid = bot_pick_nth(i);
        if(bot_can_fire(wid))
        {
            vector v = weapon_range(wid);
            if(brange == -1 || (brange >= v_y && brange <= v_z))
            {
                return wid;
            }
        }
    }
    return bot_melee_weapon();
};

void(float brange) bot_weapon_switch =
{
    float wid = bot_pick_weapon(brange);
    if(wid != self.weapon && wid == self.holsterweapon2)
    {
        self.weaponflags = self.holsterweaponflags2; // (the dagger's kind: VR_ADMelee_Flags)
    }
    else if(wid != self.weapon && self.weapon == self.holsterweapon2 && self.weapon == WID_SWORD)
    {
        self.weaponflags = 0; // (the dagger's kind not carried onto a gun)
    }
    self.weapon = wid;
    W_SetCurrentAmmo();
};

// QVR: a bot toned down by its skill (his voice notes 2026-11-21: "relentless, even with the easiest difficulty mode on
// ... super precise, super fast, and super aggressive ... toned down quite a lot, especially at the lower
// difficulties"). Before, its guns were turned dead on its enemy's middle every frame and fired the moment it saw him.
// Now, by its skill (b_skill 0-3, the `skill` it was added at):
// - its aim (bot_aim_point): off by an error drawn every BOT_AIM_REDRAW seconds (bot_aim_err degrees at most) and
//   behind a moving enemy (where he was bot_aim_lag seconds ago), both times vr_bot_aim_error;
// - its reaction (bot_fire_ready): an enemy just seen is fired at only after bot_react seconds, times vr_bot_reaction;
// - its fire discipline (bot_fire_ready): bursts of bot_burst seconds with pauses of bot_pause between them;
// - its turn (bot_phys.qc CL_KeyMove: skills 0 and 1 turn at bot_turn of a keyboard player's speed);
// - its aggression (bot_dodge_stuff): it notices players no further than bot_notice units (unless it heard them);
// - its melee swing's telegraph (bot_windup): held raised vr_bot_swing_windup seconds at skill 0, less the higher.
float(float sk, float a, float b, float c, float d) bot_by_skill =
{
    sk = max(0, min(3, rint(sk)));
    return sk == 0 ? a : sk == 1 ? b : sk == 2 ? c : d;
};
float(entity b) bot_aim_err = { return bot_by_skill(b.b_skill, 6, 4, 2.5, 1.2); };       // degrees
float(entity b) bot_aim_lag = { return bot_by_skill(b.b_skill, 0.12, 0.08, 0.05, 0.02); }; // s
float(entity b) bot_react   = { return bot_by_skill(b.b_skill, 0.8, 0.55, 0.35, 0.2); };  // s
float(entity b) bot_burst   = { return bot_by_skill(b.b_skill, 0.9, 1.2, 1.8, 3); };      // s
float(entity b) bot_pause   = { return bot_by_skill(b.b_skill, 0.7, 0.5, 0.3, 0.15); };   // s
float(entity b) bot_turn    = { return bot_by_skill(b.b_skill, 0.6, 0.8, 1, 1); };         // of the keys' turn speed
float(entity b) bot_notice  = { return bot_by_skill(b.b_skill, 800, 1200, 1800, 100000); }; // units
float BOT_AIM_REDRAW = 0.35; // s

// Where the bot (self) aims at `e` this frame: his middle, behind him as he moves, off by its error.
vector(entity e) bot_aim_point =
{
    vector mid = (e.absmin + e.absmax) * 0.5;
    float scale = max(0, cvar_hget(cvarh_vr_bot_aim_error));
    if(scale <= 0)
    {
        return mid;
    }
    if(time >= self.vr_botaim_next)
    {
        // (a point in a disc: its share of the distance, up to tan(bot_aim_err))
        float ang = bot_aim_err(self) * scale * (3.14159265 / 180);
        float r = (sin(ang) / cos(ang)) * sqrt(random());
        float a = random() * 2 * 3.14159265;
        self.vr_botaim_err = '1 0 0' * (r * cos(a)) + '0 1 0' * (r * sin(a));
        self.vr_botaim_next = time + BOT_AIM_REDRAW * (0.75 + random() * 0.5);
    }
    vector at = mid - e.velocity * (bot_aim_lag(self) * scale);
    vector eye = self.origin + self.view_ofs;
    float dist = vlen(at - eye);
    makevectors(vectoangles(at - eye));
    return at + (v_right * self.vr_botaim_err_x + v_up * self.vr_botaim_err_y) * dist;
};

// Whether the bot (self) may fire at its enemy now (bot_shoot: he is in its sight and its weapon's range): its reaction
// to him done, its burst on (not a pause).
float() bot_fire_ready =
{
    // (a new enemy, or the last one again after a second with no shot at him: out of sight, out of range)
    if(self.enemy != self.vr_botreact_of || time - self.vr_botreact_seen > 1)
    {
        self.vr_botreact_of = self.enemy;
        self.vr_botreact_at = time + bot_react(self) * max(0, cvar_hget(cvarh_vr_bot_reaction));
        self.vr_botburst_until = 0;
    }
    self.vr_botreact_seen = time;
    if(time < self.vr_botreact_at)
    {
        return FALSE;
    }
    if(time >= self.vr_botburst_until)
    {
        if(self.vr_botburst_until > 0 &&
           time < self.vr_botburst_until + bot_pause(self) * max(0, cvar_hget(cvarh_vr_bot_reaction)))
        {
            return FALSE; // (its pause)
        }
        self.vr_botburst_until = time + bot_burst(self);
    }
    return TRUE;
};

// How long bot `b`'s melee swing is held raised at its top beyond a player's (its telegraph).
float(entity b) bot_windup =
{
    return max(0, cvar_hget(cvarh_vr_bot_swing_windup)) * bot_by_skill(b.b_skill, 1, 0.75, 0.5, 0.25);
};

// QVR (MP bots phase 1): its melee weapon's guard, as a flat-screen player's (the right button held: vr_flatplay.cpp's
// parry pose, botflatpose; any blow from the front parried, VR_Parry_FlatGuard). The AI decides: a blow seen coming at it
// -- a player's or a bot's swing, a monster's melee -- raises it, by its skill's chance (vr_bot_parry_chance, -1: 25, 50,
// 75, 90 percent for skills 0-3) after its reaction's delay (0.3, 0.2, 0.12, 0.06 s), held BOT_PARRY_HOLD, one roll a
// blow. Only with a melee weapon in its main hand (the flat guard's rule); no swing while it is up.
float BOT_PARRY_NEAR = 110;   // units: a blow from this near (origin to origin)
float BOT_PARRY_HOLD = 0.6;   // s: its guard up
float BOT_PARRY_SWING = 0.45; // s: another's swing seen coming for this long from its start (its strike at 0.36)
float BOT_PARRY_HANDV = 2;    // m/s: a headset player's melee hand moving this fast swings

// Whether `a` is starting a blow at the bot (self).
float(entity a) bot_blow_coming =
{
    if(!a || a == self || a.health <= 0 || vlen(a.origin - self.origin) > BOT_PARRY_NEAR)
    {
        return FALSE;
    }
    if(a.flags & FL_CLIENT)
    {
        if(!a.ishuman)
        {
            return a.vr_botswing > 0 && time - a.vr_botswing < BOT_PARRY_SWING + a.vr_botwindup;
        }
        if(VR_AG_Class(VRGetEntWeapon(a, cVR_MainHand)) == VR_AG_CLASS_MELEE && VRHandsTracked(a) && !VR_FlatPlay(a) &&
           vlen(a.handvel) > BOT_PARRY_HANDV)
        {
            return TRUE; // (a headset's swing)
        }
        return VR_HandMotionAction(a, cVR_MainHand) || VR_HandMotionAction(a, cVR_OffHand); // (a flat player's button)
    }
    return (a.flags & FL_MONSTER) && a.enemy == self && VR_Parry_MeleeMonster(a) && a.attack_state == AS_MELEE;
};

// Once a frame (bot_setvrfields): a blow looked for, its guard eased up or down.
void() bot_parry_frame =
{
    float melee = VR_AG_Class(VRGetEntWeapon(self, cVR_MainHand)) == VR_AG_CLASS_MELEE;
    if(self.health <= 0 || !melee)
    {
        self.vr_botparry = 0;
        self.vr_botparry_until = 0;
        return;
    }
    if(time >= self.vr_botparry_next && self.vr_botparry_until < time)
    {
        entity a = world;
        if(bot_blow_coming(self.enemy))
        {
            a = self.enemy;
        }
        else
        {
            for(entity p = findradius(self.origin, BOT_PARRY_NEAR); p && !a; p = p.chain)
            {
                if(((p.flags & FL_CLIENT) && (deathmatch || teamplay)) || (p.flags & FL_MONSTER))
                {
                    a = bot_blow_coming(p) ? p : world;
                }
            }
        }
        if(a)
        {
            float sk = max(0, min(3, self.b_skill));
            float chance = cvar_hget(cvarh_vr_bot_parry_chance);
            if(chance < 0)
            {
                chance = sk == 0 ? 25 : sk == 1 ? 50 : sk == 2 ? 75 : 90;
            }
            self.vr_botparry_next = time + BOT_PARRY_SWING; // (this blow rolled once)
            if(random() * 100 < chance)
            {
                float delay = sk == 0 ? 0.3 : sk == 1 ? 0.2 : sk == 2 ? 0.12 : 0.06;
                self.vr_botparry_at = time + delay;
                self.vr_botparry_until = time + delay + BOT_PARRY_HOLD;
                dprint(sprintf("botparry: %s sees %s's blow coming: its guard up in %.2f s (skill %g, chance %g)\n",
                    self.netname, a.classname == "player" ? a.netname : a.classname, delay, sk, chance));
            }
        }
    }
    float want = time >= self.vr_botparry_at && time < self.vr_botparry_until;
    float ease = cvar_hget(want ? cvarh_vr_flat_parry_in_time : cvarh_vr_flat_parry_out_time);
    float was = self.vr_botparry;
    self.vr_botparry = ease > 0 ? min(1, max(0, self.vr_botparry + (want ? frametime : -frametime) / ease)) : (want ? 1 : 0);
    if(was < 0.5 && self.vr_botparry >= 0.5)
    {
        self.vr_botparries = self.vr_botparries + 1;
    }
};

// QVR (MP B11 part 4): its off hand as a player's. It punches (its fist thrust at an enemy close by, struck by the
// melee's sweep as a player's punch: vr_melee.qc), reaches for what lies near (a dropped gun, a backpack: taken by its
// hand's touch and grip, wpnthrow_handtouch, BackpackTouch) and, with vr_player_disarm_pvp, for a player's held weapon
// (closed on it, then pulled away: vr_playerdisarm_grab.qc's take). The AI only decides (bot_offhand_ai); the hand's
// place, grip and velocity go through the players' code.
float BOT_PUNCH_BACK = 0.08;  // s: the fist drawn back from its rest
float BOT_PUNCH_SPEED = 6;    // m/s: its thrust
float BOT_PUNCH_HOLD = 0.06;  // s: at full reach
float BOT_PUNCH_REST = 0.15;  // s: back at rest
float BOT_PUNCH_RANGE = 60;   // units: an enemy this near (origin to origin) is punched
float BOT_PUNCH_REACH = 36;   // units: the fist at most this far from its chest
float BOT_PUNCH_DEPTH = 10;   // units: aimed this far into the body
float BOT_PUNCH_WAIT = 0.5;   // s: from one punch's end to the next
float BOT_REACH_SPEED = 1;    // m/s: the open hand's way to it (a grab takes hold below vr_agrunt_grab_speed)
float BOT_REACH_FAR = 48;     // units: from its chest, what it reaches for
float BOT_REACH_OPEN = 0.15;  // s: open at least (a grip's press counts 0.125 s after a let-go: StartedHandGrabbing)
float BOT_REACH_GIVEUP = 1.5; // s: not there by then, given up
float BOT_REACH_AGAIN = 1;    // s: after a reach given up or a grip that held nothing
float BOT_GRIP_WAIT = 0.3;    // s: closed on it, taken or not
float BOT_PULL_SPEED = 2.5;   // m/s: a player's weapon pulled away from him (vr_player_disarm_pull: 1.5)

// Where its off hand closes on `e`: a player's weapon held in hand `vh` on its drawn line's middle (the line the grab
// measures by: VR_PDG_Gap), a thing lying there its middle; '0 0 0' none.
vector(entity e, float vh) bot_reach_point =
{
    if(!(e.flags & FL_CLIENT))
    {
        return (e.absmin + e.absmax) * 0.5;
    }
    entity was = self;
    self = e; // (VR_Melee_Line's helpers read self)
    float ok = VR_Parry_WeaponLine(e, vh);
    self = was;
    return ok ? (vr_parry_butt + vr_parry_end) * 0.5 : '0 0 0';
};

void(float xAgain) bot_reach_end =
{
    self.vr_botreach = world;
    self.vr_botreach_s = 0;
    self.vr_botreach_t = time;
    self.vr_botreach_next = time + xAgain;
};

void(entity e, float vh, vector xFrom) bot_reach_start =
{
    self.vr_botreach = e;
    self.vr_botreach_h = vh;
    self.vr_botreach_s = 1;
    self.vr_botreach_t = time;
    self.vr_botreach_at = xFrom;
};

void(entity e) bot_punch_start =
{
    self.vr_botpunch = time;
    self.vr_botpunch_of = e;
};

// Whether `e` is still what its reach wants: a player alive, the weapon still in his hand (and PvP disarms on); a thing
// lying there still (taken by another, removed: its touch gone).
float(entity e, float vh) bot_reach_valid =
{
    if(e.flags & FL_CLIENT)
    {
        return e.health > 0 && !VRIsHandWeaponless(e, vh) && cvar_hget(cvarh_vr_player_disarm_pvp) &&
               cvar_hget(cvarh_vr_player_disarm_grab);
    }
    return e.handtouch != __NULL__ && e.modelindex; // (removed: ED_Free clears its model, not its handtouch: mpreview D6)
};

// Its off hand's reach, once a frame (bot_setvrfields): open on its way, closed on it, pulling a player's weapon away;
// its grip.
void(vector xChest) bot_reach_frame =
{
    self.vr_botoffgrip = TRUE; // (a fist: closed, as a flat player's guard)
    entity e = self.vr_botreach;
    if(!e)
    {
        return;
    }
    float st = time - self.vr_botreach_t;
    float s = self.vr_botreach_s;
    if(!VRIsHandWeaponless(self, cVR_OffHand))
    {
        bot_reach_end(0); // (taken: a thing's touch, a player's weapon pulled out of his hand)
        return;
    }
    if(self.health <= 0 || !bot_reach_valid(e, self.vr_botreach_h))
    {
        bot_reach_end(BOT_REACH_AGAIN);
        return;
    }
    vector p = bot_reach_point(e, self.vr_botreach_h);
    if(p == '0 0 0' || vlen(p - xChest) > BOT_REACH_FAR * 1.25)
    {
        bot_reach_end(BOT_REACH_AGAIN); // (out of its reach now)
        return;
    }
    float m2u = VRMetersToUnits();
    if(s == 1)
    {
        self.vr_botoffgrip = FALSE;
        vector d = p - self.vr_botreach_at;
        float step = BOT_REACH_SPEED * m2u * frametime;
        self.vr_botreach_at = vlen(d) <= step ? p : self.vr_botreach_at + normalize(d) * step;
        if(self.vr_botreach_at == p && st >= BOT_REACH_OPEN)
        {
            self.vr_botreach_s = 2;
            self.vr_botreach_t = time;
            self.vr_botoffgrip = TRUE; // (closes on it: the grip's press)
        }
        else if(st > BOT_REACH_GIVEUP)
        {
            bot_reach_end(BOT_REACH_AGAIN);
        }
        return;
    }
    if(s == 2)
    {
        self.vr_botreach_at = p; // (closed on it, going with it)
        if(self.vr_pdg_of == e && (e.flags & FL_CLIENT))
        {
            self.vr_botreach_s = 3; // (taken hold of: now the pull)
            self.vr_botreach_t = time;
        }
        else if(st > BOT_GRIP_WAIT)
        {
            bot_reach_end(BOT_REACH_AGAIN);
        }
        return;
    }
    // (3) pulled away from his body, as the grab measures the pull (VR_PDG_Hold: away from his chest)
    vector chest = e.origin + e.view_ofs * 0.6;
    self.vr_botreach_at = self.vr_botreach_at + normalize(self.vr_botreach_at - chest) * (BOT_PULL_SPEED * m2u * frametime);
    if(self.vr_pdg_of != e || st > BOT_GRIP_WAIT)
    {
        bot_reach_end(BOT_REACH_AGAIN); // (taken, then the hand holds it: above; slipped off or let go)
    }
};

// Its punch's fist this moment (`xChest`, its rest `xRest`, the aim's vectors made): its place into vr_botfist, its way
// into vr_botfistdir; FALSE: no punch now.
vector vr_botfist;
vector vr_botfistdir;
float(vector xChest, vector xRest) bot_punch_pose =
{
    float st = time - self.vr_botpunch;
    entity e = self.vr_botpunch_of;
    if(self.vr_botpunch <= 0 || st < 0 || !e)
    {
        return FALSE;
    }
    vector back = xRest - v_forward * 4;
    vector mid = (e.absmin + e.absmax) * 0.5 + '0 0 6';
    vector aim = mid + normalize(mid - back) * BOT_PUNCH_DEPTH;
    if(vlen(aim - xChest) > BOT_PUNCH_REACH)
    {
        aim = xChest + normalize(aim - xChest) * BOT_PUNCH_REACH;
    }
    float thrust = max(0.02, vlen(aim - back) / (BOT_PUNCH_SPEED * VRMetersToUnits()));
    vr_botfistdir = normalize(aim - back);
    if(st < BOT_PUNCH_BACK)
    {
        vr_botfist = xRest + (back - xRest) * (st / BOT_PUNCH_BACK);
    }
    else if(st < BOT_PUNCH_BACK + thrust)
    {
        vr_botfist = back + (aim - back) * ((st - BOT_PUNCH_BACK) / thrust);
    }
    else if(st < BOT_PUNCH_BACK + thrust + BOT_PUNCH_HOLD)
    {
        vr_botfist = aim;
    }
    else if(st < BOT_PUNCH_BACK + thrust + BOT_PUNCH_HOLD + BOT_PUNCH_REST)
    {
        vr_botfist = aim + (xRest - aim) * ((st - BOT_PUNCH_BACK - thrust - BOT_PUNCH_HOLD) / BOT_PUNCH_REST);
    }
    else
    {
        return FALSE;
    }
    return TRUE;
};

void() bot_setvrfields =
{
    if(self.ishuman)
        return;

    // (v_forward left the aim's, as makeforward left it; the callers' v_right and v_up kept, as it kept them)
    vector wasR = v_right, wasU = v_up;
    float fresh = self.vr_botvr_t != time;
    if(fresh)
    {
        bot_parry_frame(); // (its guard: up when a blow comes, by its skill)
    }

    // Its hands as a flat-screen player's (MP bots phase 1, his words 2026-11-18: the flat player's code for the weapon
    // placement and the melee): flat play's own poses (botflatpose: vr_flatplay.cpp poseBot) with its AI's decisions for
    // the mouse and the keys -- a gun or two off its shoulders, each turned so its shot goes at its enemy; a melee
    // weapon's overhead swing (W_Fire_Melee's button), whose blow is the melee's sweep as a flat player's is (the
    // windup strikes nothing: .handsynth, as flat play's move sends it); its guard. The clients draw them (its
    // controllers), its weapons' lines each frame (VR_Parry_GuardFrame) are what a player's directional parry reads.
    self.v_viewangle = self.v_angle; // (its view as a client's move sends his: the flat guard's cone, VR_Parry_FlatGuard)
    vector aimAt = '0 0 0';
    if(self.enemy && self.enemy.health > 0 && self.enemy != self)
    {
        aimAt = bot_aim_point(self.enemy); // (off by its skill's error: bot_aim_point)
    }
    self.vr_botswing_ph = botflatpose(self, aimAt, self.vr_botswing, self.vr_botparry, self.vr_botwindup);
    float synth = 0;
    if(self.vr_botswing_ph == 1)
    {
        synth = VR_HANDSYNTH_MAIN;
    }
    else if(self.vr_botswing_ph == 2)
    {
        synth = VR_HANDACTION_MAIN;
    }
    if(self.vr_botparry >= 0.5 && VR_AG_Class(VRGetEntWeapon(self, cVR_MainHand)) == VR_AG_CLASS_MELEE)
    {
        synth = synth | VR_HANDGUARD_MAIN; // (the flat guard's: any blow from the front parried, VR_Parry_FlatGuard)
    }
    self.handsynth = synth;
    // Its muzzles where every client draws its weapons (bots3: the engine places them by the weapons' offsets, as each
    // client places its own; the disarm grab's and the parry's line runs from the hand to it); an empty hand or a fist:
    // 24 units ahead of the hand.
    self.muzzlepos = weapondrawnpose(self, cVR_MainHand, 53);
    if(self.muzzlepos == '0 0 0')
    {
        makevectors(self.handrot);
        self.muzzlepos = self.handpos + (v_forward * 24);
    }
    self.offmuzzlepos = weapondrawnpose(self, cVR_OffHand, 53);
    if(self.offmuzzlepos == '0 0 0')
    {
        makevectors(self.offhandrot);
        self.offmuzzlepos = self.offhandpos + (v_forward * 24);
    }
    makevectors(self.v_angle);
    vector chest = self.origin + '0 0 16' + (v_forward * 8);

    // Its off hand (MP B11 part 4): once a frame its reach goes on (bot_reach_frame: its grip with it) and a punch over
    // ends; each call places the hand as the state says.
    if(fresh)
    {
        self.vr_botvr_t = time;
        bot_reach_frame(chest);
        if(self.vr_botpunch > 0 && !bot_punch_pose(chest, self.offhandpos))
        {
            self.vr_botpunch = 0;
            self.vr_botpunch_end = time;
        }
    }
    if(self.vr_botreach)
    {
        vector rd = self.vr_botreach_at - chest;
        self.offhandpos = self.vr_botreach_at;
        self.offhandrot = vectoangles(rd);
        self.offmuzzlepos = self.offhandpos + normalize(rd) * 24;
    }
    else if(bot_punch_pose(chest, self.offhandpos))
    {
        self.offhandpos = vr_botfist;
        self.offhandrot = vectoangles(vr_botfistdir);
        self.offmuzzlepos = self.offhandpos + vr_botfistdir * 24;
    }

    // Its hands' velocity (.handvel, .offhandvel: m/s against its body, as a client's controllers send theirs; the grab's
    // speed and pull read them), from the last frame's places; a jump (a respawn, a teleport) none. Its poses have no
    // clock of their own (.vr_posetime -1: the melee reads them over the server's frames).
    if(fresh)
    {
        float m2u = VRMetersToUnits();
        vector rm = self.handpos - self.origin, ro = self.offhandpos - self.origin;
        vector vm = '0 0 0', vo = '0 0 0';
        if(frametime > 0)
        {
            vm = (rm - self.vr_bothand_rel) * (1 / (frametime * m2u));
            vo = (ro - self.vr_botoffhand_rel) * (1 / (frametime * m2u));
        }
        self.handvel = vlen(vm) < 20 ? vm : '0 0 0';
        self.offhandvel = vlen(vo) < 20 ? vo : '0 0 0';
        self.vr_bothand_rel = rm;
        self.vr_botoffhand_rel = ro;
        self.vr_posetime = -1;
        // (its grips' last frame, then this one's: the grab's press and let-go, VR_HandGrabUtil_UpdateState)
        VRSetOffHandPrevGrabbingBit(self, VRGetOffHandGrabbingBit(self));
        VRSetOffHandGrabbingBit(self, self.vr_botoffgrip);
    }
    v_right = wasR;
    v_up = wasU;

    VRSetTeleporting(self, FALSE);
    VRSetMainHandGrabbingBit(self, TRUE);
    VRSetMainHandPrevGrabbingBit(self, TRUE);
    VRSet2HAiming(self, FALSE);
    VRSetOffHandReloadFlickingBit(self, TRUE);
    VRSetOffHandPrevReloadFlickingBit(self, TRUE);
    VRSetMainHandReloadFlickingBit(self, TRUE);
    VRSetMainHandPrevReloadFlickingBit(self, TRUE);
}

void() bot_shoot =
{
    // quick little function to stop making him shoot the wrong way ! Argh
    float g;

    g = angcomp(self.v_angle_x, self.b_angle_x);
    if(fabs(g) > 30)
        return; // argh, too far away

    g = angcomp(self.v_angle_y, self.b_angle_y);
    if(fabs(g) > 30)
        return; // not again!

    if(!bot_fire_ready())
        return; // QVR: its reaction to a new enemy, its pause between bursts (its skill's)

    bot_setvrfields();
    self.button0 = TRUE;
    // QVR (MP B11 part 4): a gun its off hand took fires with it
    float ow = VRGetEntWeapon(self, cVR_OffHand);
    self.button3 = ((ow != WID_FIST) && (VR_AG_Class(ow) != VR_AG_CLASS_MELEE));
};

// QVR (MP bots phase 1): the gun its off hand would hold beside `mw` (dual wielding), the best first: one of its own, not
// `mw` itself, that pairs with it (VR_AG_Pairs) and has ammo; WID_FIST: none.
float(float mw, float mf) bot_dual_pick =
{
    float w;
    for(float i = 0; i < 8; i++)
    {
        w = i == 0 ? WID_SUPER_NAILGUN : i == 1 ? WID_SUPER_SHOTGUN : i == 2 ? WID_NAILGUN : i == 3 ? WID_LIGHTNING :
            i == 4 ? WID_SHOTGUN : i == 5 ? WID_GRENADE_LAUNCHER : i == 6 ? WID_NAILPISTOL : WID_REVOLVER;
        if(w != mw && VR_WeaponUtil_EntHasWeapon(self, w) && VR_AG_Pairs(mw, mf, w, 0) && PlayerHasAmmoForWeapon(self, w, 0))
        {
            return w;
        }
    }
    return WID_FIST;
};

// QVR (MP B11 part 4): what its off hand does next, when it does nothing (BotAI, after its fight's choices): an enemy
// close by punched -- or, a player with a weapon in his hand and PvP disarms on, his weapon taken hold of and pulled
// away; else a gun or a backpack lying in its reach taken. A gun its off hand holds is let go of once spent.
void() bot_offhand_ai =
{
    if(self.health <= 0 || self.vr_botreach || self.vr_botpunch > 0)
    {
        return;
    }
    float ow = VRGetEntWeapon(self, cVR_OffHand);
    float mw = VRGetEntWeapon(self, cVR_MainHand);
    float mf = VRGetEntWeaponFlags(self, cVR_MainHand);
    float dual = cvar_hget(cvarh_vr_bot_dual_wield) != 0;
    if(ow != WID_FIST)
    {
        // QVR (MP bots phase 1): a second gun held as a player dual wields: kept while it pairs with its main weapon
        // (VR_AG_Pairs: the players' and armed humanoids' rule) and has ammo, both fired (bot_shoot); one its hand took
        // off the floor is its own (its inventory, as a player's pickup). Else let go of: its own put away (the hand
        // emptied), one not its own dropped. vr_bot_dual_wield 0: MP B11 part 4's (fired until spent, then dropped).
        float of = VRGetEntWeaponFlags(self, cVR_OffHand);
        if(dual && VR_AG_Class(ow) != VR_AG_CLASS_MELEE)
        {
            if(!VR_WeaponUtil_EntHasWeapon(self, ow))
            {
                VR_WeaponUtil_EntAddWeapon(self, ow);
            }
            if(ow != mw && VR_AG_Pairs(mw, mf, ow, of) && PlayerHasAmmoForWeapon(self, ow, of))
            {
                return;
            }
            VRGiveEntWeapon(self, cVR_OffHand, WID_FIST, 0, 0);
            return;
        }
        entity os = self;
        if(VR_AG_Class(ow) != VR_AG_CLASS_MELEE && !W_CheckNoAmmo(cVR_OffHand))
        {
            DropWeaponInHand(self, cVR_OffHand, ow, VRGetEntWeaponFlags(self, cVR_OffHand)); // (spent: its hand free again)
        }
        self = os;
        return;
    }
    if(!VR_Hand_Free(self, cVR_OffHand, VR_HANDFREE_LOCKED))
    {
        return;
    }
    // (MP bots phase 1) a gun in its main hand: a second of its own that pairs with it, into the off hand (dual wielding)
    if(dual && self.health > 0 && VR_AG_DualGun(mw, mf))
    {
        float pick = bot_dual_pick(mw, mf);
        if(pick != WID_FIST)
        {
            VRGiveEntWeapon(self, cVR_OffHand, pick, 0, 0);
            dprint(sprintf("botdual: %s holds its %s beside its %s\n", self.netname, WeaponIdToWeaponName(pick),
                WeaponIdToWeaponName(mw)));
            return;
        }
    }
    makevectors(self.v_angle);
    vector chest = self.origin + '0 0 16' + (v_forward * 8);
    entity en = self.enemy;
    if(en && en.health > 0 && en.takedamage && vlen(en.origin - self.origin) < BOT_PUNCH_RANGE)
    {
        if((en.flags & FL_CLIENT) && time >= self.vr_botreach_next && cvar_hget(cvarh_vr_player_disarm_pvp) &&
           cvar_hget(cvarh_vr_player_disarm_grab))
        {
            float vh = VRIsHandWeaponless(en, cVR_MainHand) ? cVR_OffHand : cVR_MainHand;
            vector p = bot_reach_point(en, vh);
            if(!VRIsHandWeaponless(en, vh) && p != '0 0 0' && vlen(p - chest) <= BOT_REACH_FAR)
            {
                bot_reach_start(en, vh, self.offhandpos);
                return;
            }
        }
        if(time >= self.vr_botpunch_end + BOT_PUNCH_WAIT)
        {
            bot_punch_start(en);
        }
        return;
    }
    if(time < self.vr_botreach_next)
    {
        return;
    }
    entity best = world;
    float bestd = BOT_REACH_FAR;
    for(entity e = findradius(chest, BOT_REACH_FAR); e; e = e.chain)
    {
        // (not the weapon its own hand just let go of: it chose another)
        float gun = (e.handtouch == wpnthrow_handtouch) && (e.vr_wpn_from != self) &&
                    (VR_AG_Class(e.weapon) != VR_AG_CLASS_MELEE);
        if((gun || e.handtouch == BackpackTouch) && !VR_Run_RefusesEnt(e))
        {
            float d = vlen((e.absmin + e.absmax) * 0.5 - chest);
            if(d < bestd)
            {
                best = e;
                bestd = d;
            }
        }
    }
    if(best)
    {
        bot_reach_start(best, 0, self.offhandpos);
    }
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Bot_fight_style

This is the core of the bot's thinking when
attacking an enemy.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() bot_fight_style =
{
    vector v1 = '0 0 0';
    vector v2 = '0 0 0';
    float mysz, foesz;

    if(self.enemy.health <= 0)
    {
        self.enemy = world;
        return;
    }

    if(!self.enemy.takedamage)
    {
        self.enemy = world;
        return;
    }

    if(!fisible(self.enemy))
    {
        self.enemy = world;
        return;
    }

    vector org = realorigin(self.enemy);
    makevectors(self.v_angle);

    // decide if I should shoot

    float foedist = vlen(org - self.origin);

    vector v = weapon_range(self.weapon);

    if(foedist > v_y && foedist < v_z)
    {
        traceline(self.origin + self.view_ofs, self.origin + self.view_ofs + v_forward * v_z, FALSE, self);
        if(vlen(trace_endpos - (self.origin + self.view_ofs)) >= v_y)
        {
            // try to avoid shooting teammates
            if(trace_ent.classname == "player")
                if((trace_ent.team == self.team && teamplay) || (coop))
                    return;
            bot_shoot();
        }
    }
    else
        bot_weapon_switch(foedist);

    if(!(self.b_aiflags & (AI_PRECISION | AI_BLIND | AI_OBSTRUCTED)))
    {
        foesz = bot_size_player(self.enemy);
        mysz = bot_size_player(self) + 5;

        if(foesz > mysz)
        {
            if(teamplay)
            {
                if(random() < 0.02)
                {
                    bot_start_topic(5);
                    self.b_chattime = 1;
                }
            }

            return;
        }
        else if(mysz < 140)
            return;
        else if(self.avoid)
        {
            if(self.avoid.velocity)
                v = self.avoid.velocity;
            else
                v = normalize(self.avoid.origin - self.origin);
            v1_x = v_y * -1;
            v1_y = v_x;
            v2_x = v_y;
            v2_y = v_x * -1;
            foedist = vlen(self.avoid.origin - (self.origin + v1));
            if(foedist < vlen(self.avoid.origin - (self.origin + v2)))
                frik_walkmove(v2);
            else
                frik_walkmove(v1);
        }
        else if(!(self.enemy.flags & FL_MONSTER))
        {
            if(foedist + 32 <  v_x)
            frik_walkmove(self.origin - org);
            else if(foedist - 32 >  v_x)
                frik_walkmove(org - self.origin);
            else if(self.wallhug)
                frik_walkmove(v_right);
            else
                frik_walkmove(v_right * -1);
        }
    }
    else
    {
        foesz = bot_size_player(self.enemy);
        mysz = bot_size_player(self) + 5;

        if(foesz > mysz)
            return;
        else if(mysz < 140)
            return;
        self.keys = self.keys & 960;
    }
};



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
                                if(fov(foe) || foe.b_sound > time || self.b_skill == 3)
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

    bprint("Invalid wid in bot weapon_range: '");
    bprint(ftos(wid));
    bprint("'\n");

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

float(float brange) bot_pick_weapon =
{
    float wid;
    vector v;

    if( (bot_has_usable(WID_ROCKET_LAUNCHER)) &&
        (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_ROCKETS) >= 1))
    {
        wid = WID_ROCKET_LAUNCHER;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if(self.waterlevel <= 1 &&
       (bot_has_usable(WID_LIGHTNING)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_CELLS) >= 1))
    {
        wid = WID_LIGHTNING;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((bot_has_usable(WID_SUPER_NAILGUN)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_NAILS) >= 2))
    {
        wid = WID_SUPER_NAILGUN;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((bot_has_usable(WID_GRENADE_LAUNCHER)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_ROCKETS) >= 1))
    {
        wid = WID_GRENADE_LAUNCHER;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((bot_has_usable(WID_SUPER_SHOTGUN)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_SHELLS) >= 2))
    {
        wid = WID_SUPER_SHOTGUN;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((bot_has_usable(WID_NAILGUN)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_NAILS) >= 1))
    {
        wid = WID_NAILGUN;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((bot_has_usable(WID_SHOTGUN)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_SHELLS) >= 1))
    {
        wid = WID_SHOTGUN;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
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

// Its melee swing's shape (bot_setvrfields): wound up from the swing's start, across from FROM to TO (s), back at rest
// at END; the arc's degrees, its hand's reach from the body's middle, its height over the origin and its drop across.
float BOT_SWING_FROM = 0.1;
float BOT_SWING_TO = 0.3;
float BOT_SWING_END = 0.45; // (vr_util.qc VR_BUTTONSWING_LEN)
float BOT_SWING_ARC = 160;
float BOT_SWING_REACH = 18;
float BOT_SWING_Z = 12;
float BOT_SWING_DROP = 8;

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
    return e.handtouch != __NULL__;
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

    self.handrot = self.v_angle;
    self.offhandrot = self.v_angle;

    // Its hands apart, as a player holds his (MP B11 part 3): the main hand on its right, the off hand on its left, at
    // the chest, each weapon pointing along its aim from there (the disarms' and the grab's lines of its weapons, its
    // shots' start).
    // (v_forward left the aim's, as makeforward left it; the callers' v_right and v_up kept, as it kept them)
    vector wasR = v_right, wasU = v_up;
    makevectors(self.v_angle);
    vector chest = self.origin + '0 0 16' + (v_forward * 8);

    self.handpos = chest + (v_right * 7);
    self.offhandpos = chest - (v_right * 7);

    self.muzzlepos = self.handpos + (v_forward * 24);
    self.offmuzzlepos = self.offhandpos + (v_forward * 24);

    // Its melee swing (MP B11 part 3, part 3; W_Fire_Melee's button swing, player_axe*: its blow at the third frame,
    // 0.2 s in): the main hand raised on its right, then swung level across its front to its left, the weapon pointing
    // out along the arc, straight ahead at the blow -- as a player swings his. The clients draw it so (its controllers),
    // and its weapon's line each frame (VR_Parry_GuardFrame) is what a player's directional parry reads its blow by.
    float st = time - self.vr_botswing;
    if(self.vr_botswing > 0 && st >= 0 && st < BOT_SWING_END)
    {
        float k = min(1, max(0, (st - BOT_SWING_FROM) / (BOT_SWING_TO - BOT_SWING_FROM)));
        float ang = BOT_SWING_ARC * (0.5 - k) * 0.0174533; // (radians: its right, ahead at the blow, its left)
        makevectors(self.v_angle_y * '0 1 0');
        vector out = (v_forward * cos(ang)) + (v_right * sin(ang));
        self.handpos = self.origin + '0 0 1' * (BOT_SWING_Z + BOT_SWING_DROP * (0.5 - k)) + (out * BOT_SWING_REACH);
        self.handrot = vectoangles(out);
        self.muzzlepos = self.handpos + (out * 24);
        makevectors(self.v_angle);
    }

    // Its off hand (MP B11 part 4): once a frame its reach goes on (bot_reach_frame: its grip with it) and a punch over
    // ends; each call places the hand as the state says.
    float fresh = self.vr_botvr_t != time;
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

    bot_setvrfields();
    self.button0 = TRUE;
    // QVR (MP B11 part 4): a gun its off hand took fires with it
    float ow = VRGetEntWeapon(self, cVR_OffHand);
    self.button3 = ((ow != WID_FIST) && (VR_AG_Class(ow) != VR_AG_CLASS_MELEE));
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
    if(ow != WID_FIST)
    {
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



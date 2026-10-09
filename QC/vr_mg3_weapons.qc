/*  Copyright (C) 1996-2026 id Software LLC

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/
// Dawn of the Machine (MG3) weapons, adapted from the official quakec_mg3 (mg3_weapons.qc HIP_FireMjolnir, mg3_items.qc
// weapon_mjolnir; docs/vr-port/OFFICIAL_QC_SOURCE.md) for Quake VR's hands. Plan: docs/vr-port/MG3.md, M3-11..14.
//
// The Super Axe (WID_SUPERAXE): MG3's `weapon_mjolnir` is not Hipnotic's hammer but a great axe of its own. Here it is a
// weapon of its own beside Mjolnir (Vittorio's decision 2): `weapon_superaxe`; an MG3 map's `weapon_mjolnir` becomes one
// as the map loads (campaign 5 only: hip_items.qc). Its models are MG3's, read from the owned pack in place
// ("owned/mg3/progs/v_hammer.mdl": vr_gamedir.cpp VR_OwnedFile), so it is there in any campaign when the Dawn of the
// Machine data is (the agglomeration principle), and nowhere without it (VR_Pack_WeaponAvailable). The engine takes the
// first-person arm off the model and lays it as Quake VR's axe lies (vr_monstermods.cpp superAxePoses), and holds it with
// the axe's settings (weapon settings slot 24, its glow slot 25: vr_weapons.inc). Its blows are an axe's (vr_melee.qc
// VR_MTHING_AXE: the head an edge; it beheads as the axe does).
//
// Official behaviour kept: a blow does 40 (vr_dmg_superaxe; Quake VR's axe 20, as id's), a zombie takes three times it,
// a blow that kills twice it (so it gibs); a second blow on the same monster soon after the first fires Mjolnir's
// lightning burst (15 cells, above water: MG3 never discharges it under water) instead of the blow's damage; a blow on
// another monster in between, a wall or the air ends the chain. The head glows while the burst is ready (MG3's
// v_hammer_glow.mdl, 15 cells or more). The window is vr_superaxe_burst_window (1.5 s; MG3's 0.5 s is a key press's
// rhythm, too short for two real swings). A pickup with spawnflag 128 drops its taker through the map's silent
// teleport (MG3's map2: the axe lies in a secret above its copy of the room); its targets fire as any pickup's (itemTouch).

float MG3_SUPERAXE_BURST_CELLS = 15;

.entity vr_superaxe_mark;      // the thing the first blow of a chain struck (MG3's last_mjolnir_hit)
.float vr_superaxe_until;      // until when a second blow on it fires the burst (0: no chain)
.float vr_superaxe_hand;       // the hand whose axe struck it (its head glows)
float vr_superaxe_bursts;      // bursts fired since the map started (tests)

void() VR_MG3_Weapons_Precache =
{
    if(!VR_Pack_WeaponAvailable(WID_SUPERAXE))
    {
        return;
    }
    precache_model("owned/mg3/progs/v_hammer.mdl");
    precache_model("owned/mg3/progs/v_hammer_glow.mdl");
    if(fileexists("owned/mg3/progs/g_hammer.mdl"))
    {
        precache_model("owned/mg3/progs/g_hammer.mdl"); // (the classic pickup: weapon_superaxe)
    }
    precache_sound("hipweap/mjolslap.wav");
    precache_sound("hipweap/mjoltink.wav");
    precache_sound("hipweap/mjolhit.wav");
};

// The model the hand's Super Axe is drawn with: glowing or not.
void(entity p, float xHand, float xGlow) MG3_SuperAxe_Glow =
{
    string m = xGlow ? "owned/mg3/progs/v_hammer_glow.mdl" : "owned/mg3/progs/v_hammer.mdl";
    if(xHand == cVR_MainHand && p.weapon == WID_SUPERAXE)
    {
        p.weaponmodel = m;
    }
    else if(xHand == cVR_OffHand && p.weapon2 == WID_SUPERAXE)
    {
        p.weaponmodel2 = m;
    }
};

void(entity p) MG3_SuperAxe_EndChain =
{
    if(p.vr_superaxe_until)
    {
        MG3_SuperAxe_Glow(p, p.vr_superaxe_hand, FALSE);
    }
    p.vr_superaxe_until = 0;
    p.vr_superaxe_mark = world;
};

// A blow of `xHand`'s Super Axe (self: the player) landing on `xVictim` at `xAt`, `xDmg` its damage before the official
// zombie and finishing-blow multipliers. Returns TRUE when it fired the burst.
float(float xHand, entity xVictim, vector xAt, float xDmg) MG3_SuperAxe_Strike =
{
    if(xVictim.classname == "monster_zombie" || xVictim.classname == "monster_szombie")
    {
        xDmg *= 3; // MG3: 120 for 40
    }
    else if(xVictim.health - xDmg < 0)
    {
        xDmg *= 2; // make sure to gib on an axe kill
    }

    float second = self.vr_superaxe_until > time;
    if(second && self.vr_superaxe_mark == xVictim && self.waterlevel < 2 && self.ammo_cells >= MG3_SUPERAXE_BURST_CELLS)
    {
        // The burst: Mjolnir's lightning from where it struck, its 15 cells; no blow's damage (the lightning's instead).
        MG3_SuperAxe_EndChain(self);
        delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, MG3_SUPERAXE_BURST_CELLS);
        trace_endpos = xAt;
        HIP_SpawnMjolnirBase();
        vr_superaxe_bursts++;
        if(cvar("developer"))
        {
            dprint(sprintf("superaxe: burst on %s (hand %g), %g cells left\n", xVictim.classname, xHand, self.ammo_cells));
        }
        return TRUE;
    }

    if(second)
    {
        MG3_SuperAxe_EndChain(self); // a second blow: on another, or with no cells, or under water: only its damage
    }
    else
    {
        self.vr_superaxe_mark = xVictim;
        self.vr_superaxe_until = time + max(0.05, cvar_hget(cvarh_vr_superaxe_burst_window));
        self.vr_superaxe_hand = xHand;
        if(self.ammo_cells >= MG3_SUPERAXE_BURST_CELLS)
        {
            MG3_SuperAxe_Glow(self, xHand, TRUE);
        }
    }

    xVictim.axhitme = 1;
    self.damage_weapon = IT_AXE;
    VR_HitBlood(xVictim, xAt, v_forward, xDmg);
    T_Damage_VRMelee(xVictim, self, self, xDmg);
    if(cvar("developer"))
    {
        dprint(sprintf("superaxe: %s blow on %s: %.1f (hand %g)\n", second ? "second" : "first", xVictim.classname, xDmg, xHand));
    }
    return FALSE;
};

// The blow landing (client.qc PlayerVRMeleeImpl, by vr_melee.qc's contact): VRDoMeleeAttackTracelineFor finds what it
// meets, as Mjolnir's.
void(float xHand) W_SuperAxeMelee =
{
    float xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_dmg_superaxe));
    if(!VRDoMeleeAttackTracelineFor(xHand, 22, 12))
    {
        return;
    }

    vector org = trace_endpos - v_forward * 4;
    if(trace_ent.takedamage)
    {
        entity victim = trace_ent; // (T_Damage may trace: an explosive box blowing up)
        VRMeleeHapticByHand(xHand);
        MG3_SuperAxe_Strike(xHand, victim, org, xdmg);
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            // MG3's slap on a hit (its sound for the axe landing), or the blow's own on metal.
            VR_SoundVaried(self, VRGetGunChannel(xHand), VR_Melee_HitSoundOn(victim, "hipweap/mjolslap.wav", TRUE), 1, ATTN_NORM);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
        return;
    }

    MG3_SuperAxe_EndChain(self); // a wall or the air: the chain ends (MG3: last_mjolnir_hit = world)
    if(trace_fraction != 1.0 && VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        VRMeleeHapticByHand(xHand);
        sound(self, VRGetGunChannel(xHand), "hipweap/mjoltink.wav", 1, ATTN_NORM);
        BroadcastGunshotEffect(org);
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
};

// Each frame, each player: the glow out when the window closes or the axe left the hand; a Super Axe held without the
// data (a save from an install that had it) becomes an axe.
void(entity p) MG3_SuperAxe_PlayerFrame =
{
    if(p.vr_superaxe_until && (time >= p.vr_superaxe_until || VRGetEntWeapon(p, p.vr_superaxe_hand) != WID_SUPERAXE ||
                               p.ammo_cells < MG3_SUPERAXE_BURST_CELLS))
    {
        if(time >= p.vr_superaxe_until || VRGetEntWeapon(p, p.vr_superaxe_hand) != WID_SUPERAXE)
        {
            MG3_SuperAxe_EndChain(p);
        }
        else
        {
            MG3_SuperAxe_Glow(p, p.vr_superaxe_hand, FALSE); // spent below 15: no glow, the chain goes on (no burst)
        }
    }
    if(!VR_Pack_WeaponAvailable(WID_SUPERAXE))
    {
        entity oldself = self;
        self = p;
        for(float h = cVR_MainHand; h <= cVR_OffHand; h++)
        {
            if(VRGetEntWeapon(p, h) == WID_SUPERAXE)
            {
                VR_WeaponUtil_EntAddWeapon(p, WID_AXE);
                VRGiveEntWeapon(p, h, WID_AXE, 0, 0);
                W_SetCurrentAmmoFor(h);
            }
        }
        self = oldself;
    }
};

// Its pickup taken (weapon_touch, self the pickup, other the taker; itemTouch fires its targets after): with spawnflag
// 128, MG3's silent teleport (mg3_triggers.qc touch_teleport_silent: straight down by `height`, -2048 when unset, the
// carried things along).
void() MG3_SuperAxe_Touch =
{
    entity taker = other;
    weapon_touch();
    if(self.model != string_null || taker.classname != "player")
    {
        return; // not taken
    }
    if(self.spawnflags & 128)
    {
        vector from = taker.origin;
        vector org = taker.origin;
        org_z = taker.origin_z + self.height;
        if(self.height < 0)
        {
            org_z += 1;
        }
        spawn_tdeath(org, taker);
        setorigin(taker, org);
        VR_Carry_Teleported(taker, from); // what the hands carry comes along (vr_carry.qc)
        dprint(sprintf("superaxe: pickup teleported its taker %g units\n", org_z - from_z));
    }
};

/*QUAKED weapon_superaxe (0 .5 .8) (-16 -16 0) (16 16 32) X X X X X X X SILENT_TELEPORT
Dawn of the Machine's Super Axe (Quake VR; needs the owned Dawn of the Machine data, else nothing spawns): a great axe,
twice the axe's blow; a second blow on the same monster within Super Axe Burst Window fires a lightning burst (15 cells).
SILENT_TELEPORT (128): taking it drops the taker straight down by "height" (-2048 when unset), as MG3's map2.
"target": fired when it is taken.
*/
void() weapon_superaxe =
{
    self.classname = "weapon_superaxe"; // (an MG3 map's weapon_mjolnir too: weapon_touch knows it by its class)
    if(self.spawnflags & 128 && !self.height)
    {
        self.height = -2048;
    }
    impl_weapon_item("owned/mg3/progs/g_hammer.mdl", WID_SUPERAXE);
    if(self.handtouch == weapon_touch)
    {
        self.handtouch = MG3_SuperAxe_Touch;
    }
};

// Tests (vr_mg3_wtest; vr_mg3_weapons_test.qc) and Debug > Tests: a Super Axe pickup ahead of the player.
void(entity p, float xDist) MG3_SuperAxe_SpawnAhead =
{
    makevectors(p.v_angle);
    vector fwd = v_forward;
    fwd_z = 0;
    entity e = spawn();
    entity oldself = self;
    self = e;
    setorigin(e, p.origin + normalize(fwd) * xDist);
    weapon_superaxe();
    self = oldself;
};

void() VR_MG3_Weapons_Frame =
{
    entity p = find(world, classname, "player");
    while(p)
    {
        MG3_SuperAxe_PlayerFrame(p);
        p = find(p, classname, "player");
    }
};

// ----------------------------------------------------------------------------
// Axe buttons (M3-12; official buttons.qc func_axe_button, combat.qc's check). Upstream lets only the axe or Mjolnir
// selected open one. Here any melee blow does (Vittorio's decision 1): an axe, the Super Axe, a fist, a sword, a gun's
// butt, a headbutt, a bash or shove, a held prop swung, a thrown weapon, prop or gib; never a shot, a missile or a blast.
// Anything else says MG3's "Use the axe" (spawnflag 1 NOMESSAGE: nothing), to a player only, at most twice a second.

.float vr_axebtn_msg; // when the button may say it again

// A blow: a hand's melee (T_Damage_VRMelee, or what PlayerVRMeleeImpl strikes), a headbutt, a bash, a shove, or a thing
// thrown or flung (a weapon, a prop, a gib: what presses a wall button by being thrown at it, buttons.qc).
float(entity xInflictor, entity xAttacker, float xIsVRMelee) VR_IsMeleeBlow =
{
    if(xIsVRMelee || vr_hitkind == QVR_HITKIND_MELEE || vr_hitkind == QVR_HITKIND_HEADBUTT ||
       vr_hitkind == QVR_HITKIND_BASH || vr_hitkind == QVR_HITKIND_SHOVE)
    {
        return TRUE;
    }
    if(vr_hitkind == QVR_HITKIND_SHOT || vr_hitkind == QVR_HITKIND_SPLASH || vr_hitkind == QVR_HITKIND_BLEED)
    {
        return FALSE;
    }
    return xInflictor != xAttacker && xInflictor != world && !xInflictor.vr_grenade && VR_Button_Thrown(xInflictor);
};

// T_DamageImpl's gate for an axe button `targ`: TRUE lets the damage through (it opens: health 1).
float(entity targ, entity inflictor, entity attacker, float isVRMelee) MG3_AxeButton_Struck =
{
    if(VR_IsMeleeBlow(inflictor, attacker, isVRMelee))
    {
        vr_button_how = "struck";
        return TRUE;
    }
    if(!(targ.spawnflags & 1) && attacker.classname == "player" && time >= targ.vr_axebtn_msg)
    {
        targ.vr_axebtn_msg = time + 0.5;
        centerprint(attacker, "$mg3_qc_axe_button");
    }
    return FALSE;
};

// Its touch: a thing thrown at it presses it as a wall button's (button_touch_any's); never a player's body.
void() MG3_AxeButton_Touch =
{
    if(other.classname == "player")
    {
        return;
    }
    button_touch_any();
};

/*QUAKED func_axe_button (0 .5 .8) ? NOMESSAGE
Dawn of the Machine's axe button: a func_button that only a melee blow opens (Quake VR: any blow: an axe, the Super
Axe, a fist, a sword, a gun's butt, a headbutt or bash, a weapon or prop thrown at it); a shot or a blast makes it say
"Use the axe" (NOMESSAGE: nothing). Keys as func_button's: "target", "angle", "wait", "lip", "speed", "sounds".
*/
void() func_axe_button =
{
    self.health = 1;
    func_button();
    if(self.th_die == button_killed)
    {
        self.touch = MG3_AxeButton_Touch;
    }
};

// ----------------------------------------------------------------------------
// The laser cannon (M3-13). Dawn of the Machine's weapon_laser_gun is Hipnotic's laser cannon taken whole (official
// mg3_weapons.qc "Taken directly from the hipnotic qc"): the same bouncing bolts (a bounce off a wall keeps 0.9 of the
// damage; the third touch, or one in seven, stops it; a second hit on the same monster halves it), the same 1 cell a
// shot and cadence. So it stays one weapon, WID_LASER_CANNON, with campaign-specific numbers: MG3 lowered a bolt's
// damage from 18 to 15 and a lit bolt's from 25 to 20. Its models need no Hipnotic install: Quake VR ships
// progs/v_laserg.mdl (the one held, its own remodel; MG3's own is shadowed by it) and progs/lasrspik.mdl, and in
// campaign 5 MG3's pak gives progs/g_laserg.mdl (the classic pickup; without one, the held model is the pickup).
// Not ported: MG3's respawn_ammo (an out-of-cells laser respawns the map's SAFETY_RESPAWN cell boxes): an ammo-items
// feature for every weapon (client.qc), left to the items' task.
float(float xLit) MG3_LaserScale =
{
    if(!MG3_Campaign())
    {
        return 1;
    }
    return xLit ? 20 / 25 : 15 / 18;
};

// ----------------------------------------------------------------------------
// The bloody shotguns (M3-14; official mg3_items.qc weapon_bloody_sg/ssg, bloody_weapon_touch; weapons.qc). Found
// only in a Bloody Nightmare new game (serverflags 256; else the pickup goes 0.5 s after the map starts, as upstream),
// map1's shotgun and secret4's super shotgun. Taken, it sets its bit for the rest of the game (MG3_bloody: parm56,
// M3-02: kept through changelevel, death and saves, cleared by a new game), gives the weapon and 30 shells and says
// MG3's "You found a secret weapon!". The bit changes every shotgun of that kind the player fires, as upstream's does:
// the bloody shotgun refires in 0.28 s instead of 0.5 (Quake VR's: its trigger's next shot; reloading as ever), the
// bloody super shotgun fires 28 pellets, spread wider across (upstream's '0.3 0.08' for '0.14 0.08'), for the same 2
// shells. Drawn as Quake VR's shotguns for now (backlog: their own bloody skins; MG3's v_bloodshot models are first-
// person models with an arm, made for the view). Outside Dawn of the Machine a map's bloody shotgun simply spawns.

float MG3_SF_BN_NEWGAME = 256; // upstream BLOODY_NIGHTMARE_NEWGAME (serverflags)

.float vr_bloody_bit;

// The bloody shotgun's refire for `xNormal`'s (0.5).
float(float xNormal) MG3_BloodyShotgunRefire =
{
    if(MG3_BloodyBits() & MG3_BLOODY_SG)
    {
        return xNormal * 0.28 / 0.5;
    }
    return xNormal;
};

// Its pickup taken (weapon_touch; itemTouch fires its targets).
void() MG3_Bloody_Touch =
{
    entity taker = other;
    weapon_touch();
    if(self.model != string_null || taker.classname != "player")
    {
        return; // not taken
    }
    MG3_SetBloodyBits(MG3_BloodyBits() | self.vr_bloody_bit);
    // Upstream gives 30 shells (weapon_touch gave the weapon's own few).
    taker.ammo_shells = taker.ammo_shells + max(0, 30 - WeaponIdToInitialAmmoCount(self.weapon));
    entity oldother = other;
    other = taker;
    bound_other_ammo();
    other = oldother;
    sprint(taker, "$mg3_map2_secret_weapon");
    sprint(taker, "\n"); // (the string has none)
    dprint(sprintf("bloody: %s taken, bloody bits %g, shells %g\n", self.netname, MG3_BloodyBits(), taker.ammo_shells));
};

// Dawn of the Machine: gone 0.5 s after the map starts unless this is a Bloody Nightmare new game (upstream's
// bloody_weapon_check).
void() MG3_Bloody_Check =
{
    entity item = self.owner;
    if(!(serverflags & MG3_SF_BN_NEWGAME) && item.model != string_null && item.vr_bloody_bit)
    {
        dprint(sprintf("bloody: %s removed (not a Bloody Nightmare new game)\n", item.netname));
        remove(item);
    }
    remove(self);
};

void(float xWid, float xBit, string xName) MG3_Bloody_Spawn =
{
    self.classname = xWid == WID_SHOTGUN ? "weapon_shotgun" : "weapon_supershotgun"; // (weapon_touch knows it by its class)
    impl_weapon_item(xWid == WID_SHOTGUN ? "progs/g_shot0.mdl" : "progs/g_shot.mdl", xWid);
    if(self.weapon != xWid)
    {
        return; // (removed)
    }
    self.netname = xName;
    self.vr_bloody_bit = xBit;
    if(self.handtouch == weapon_touch)
    {
        self.handtouch = MG3_Bloody_Touch;
    }
    if(MG3_Campaign())
    {
        entity check = spawn();
        check.owner = self;
        check.think = MG3_Bloody_Check;
        check.nextthink = time + 0.5;
    }
};

/*QUAKED weapon_bloody_sg (0 .5 .8) (-16 -16 0) (16 16 32)
Dawn of the Machine's bloody shotgun: in Dawn of the Machine only in a Bloody Nightmare new game (else gone at once).
Taken: the shotgun and 30 shells, and every shotgun refires in 0.28 s for the rest of the game. "target": fired when taken.
*/
void() weapon_bloody_sg = { MG3_Bloody_Spawn(WID_SHOTGUN, MG3_BLOODY_SG, "Bloody Shotgun"); };

/*QUAKED weapon_bloody_ssg (0 .5 .8) (-16 -16 0) (16 16 32)
Dawn of the Machine's bloody super shotgun: in Dawn of the Machine only in a Bloody Nightmare new game (else gone at
once). Taken: the super shotgun and 30 shells, and every super shotgun fires 28 pellets for the rest of the game.
"target": fired when taken.
*/
void() weapon_bloody_ssg = { MG3_Bloody_Spawn(WID_SUPER_SHOTGUN, MG3_BLOODY_SSG, "Bloody Super Shotgun"); };

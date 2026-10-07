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
// weapon_mjolnir; docs/vr-port/OFFICIAL_QC_SOURCE.md) for Quake VR's hands. Plan: docs/vr-port/MG3_PLAN.md, M3-11..14.
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

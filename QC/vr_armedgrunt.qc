//
//
//
// ----------------------------------------------------------------------------
// Armed grunts (experimental; docs/vr-port/ROUND22.md, "Armed grunts")
// ----------------------------------------------------------------------------
//
// monster_vr_grunt: a grunt holding one of your weapons, drawn as the weapon lying about is (its model at the grunt's
// right hand: classname vr_ag_weapon, its record in .weaponinst), fired with your weapons' own code (W_FireShotgun,
// W_FireRocket, ... with `self` the grunt and its hand fields set from the held weapon: the same pellets, projectiles,
// sounds, smoke and damage path), its damage to players times vr_agrunt_damage_scale. Its classname is monster_army
// (.vr_ag 1): to everything else it is a grunt (ragdolls, knockdowns, hit reactions, decapitation, holding, shoves).
//
// - The model: progs/vrgrunt.mdl, the grunt without its gun (Misc/quakevr/make_vrgrunt.py: the gun's triangles dropped,
//   its vertices kept). Each server frame the weapon goes to the grunt's right hand (VR_AG_HAND_VERTS, as drawn now:
//   modelvertex), along the line the grunt's own gun had in that frame (its rear to its muzzle; its top for its roll):
//   the hands its animations close round the rifle hold it there. Aiming, it points at its target; a shot kicks it back;
//   a melee blow swings it.
// - Its weapon: the map's or the toolgun's "weapon" key (WID_*; -1 its fists; 0, unset: one from the pool, each weapon's
//   weight vr_agrunt_w_*), "weaponflags" its flags.
// - Ranged weapons: the grunt's own decision (SoldierCheckAttack's sight line and attack_finished), each weapon's chances
//   by range (VR_AG_Chance), its shots in the grunt's fire frames (shoot1..9: one shot, a burst, or one a frame for the
//   nail guns and the thunderbolt), aimed a little behind a moving target and up to vr_agrunt_aim_error degrees off.
//   With an explosive weapon it backs away from a target nearer than VR_AG_MinRange (its own blast) and clubs one at
//   arm's length.
// - Melee weapons and fists: it closes in and swings (the shove's frames; W_FireAxe, W_ChainsawMelee, HIP_FireMjolnir,
//   W_FistMelee from its hand towards the target).
// - Ammo: the weapon's clip, full (its record's wi_clip; its kind's clip size, VR_AG_ClipSize), and spare magazines
//   (VR_AG_SpawnReserve: 2, more for the guns whose magazine is a shot or two, times vr_agrunt_reserve / 2; the single-use
//   enemy guns too: its own spare magazines). Empty, it reloads (the load frames, vr_agrunt_reload_time s); with nothing
//   left it throws the gun away. A weapon out of its hands leaves its spare rounds in its pockets (VR_AG_Pocket: taken
//   up again, it has them back). Dying, it drops the weapon with what is left in it (gibbed too) and a backpack with its
//   weapons' spare rounds of a real ammo kind (none: its monster's own loot, a grunt's 5 shells).
// - Disarming: a blow of at least vr_agrunt_disarm_damage on its gun hand (vr_agrunt_disarm), your empty hand grabbing
//   its weapon (vr_agrunt_disarm_yank: it is yours), a knockdown: the weapon falls (a lying weapon, its clip with it).
// - Re-arming (vr_agrunt_rearm): disarmed it fights with its fists and goes for a weapon lying within
//   vr_agrunt_rearm_range when its target is not nearer to that weapon than it is (one at its feet, within 64 units:
//   always), then fights with that one.

float VR_AG_CLASS_FISTS = 0;     // its fists
float VR_AG_CLASS_MELEE = 1;     // a melee weapon
float VR_AG_CLASS_GUN = 2;       // one shot an attack (the shotguns, the laser rifle)
float VR_AG_CLASS_EXPLOSIVE = 3; // one round an attack, kept away from its target (the launchers)
float VR_AG_CLASS_AUTO = 4;      // one a frame (the nail guns, the thunderbolt, the laser cannon)
float VR_AG_CLASS_BURST = 5;     // the grunts' burst rifle: three rounds

float VR_AG_GRIP_REACH = 52;     // a melee attack starts this near (the boxes' middles)
float VR_AG_PICKUP_REACH = 40;   // it takes a weapon this near
float VR_AG_DISARM_HAND = 10;    // a blow this near its gun hand (units) disarms it
float VR_AG_SWING_TIME = 0.4;    // a melee swing's drawn arc (seconds)
float VR_AG_EF_HELD = 128;       // the held weapon's effects bit: a weapon in a monster's hand (the engine's vr_armik.cpp,
                                 // vr_weaponfx.cpp: the arms' grips, its muzzle flash); no other effect uses it

// Its right hand's vertices (their middle: where it holds the weapon) and its hidden gun's (the rear, the muzzle, the
// top and the bottom) are its row's in the armed monsters' table (vr_armedmon.qc: VR_AM_Hand, VR_AM_GunRear, ...).

// Where vertex `v` of e's model is as drawn now (trace_fraction 0: none), and the muzzle of the weapon model e is drawn
// with (the engine's modelanchorpoint and weapondrawnpose: builtins.qc).
vector(entity e, float v) modelvertex =
{
    vector a = '0 0 0';
    a_x = -2 - v;
    return modelanchorpoint(e, a);
};
vector(entity e) weaponmuzzle = { return weapondrawnpose(e, 0, 6); };

.float vr_ag_code;      // its "weapon" key as spawned (a weapon id, or VR_AG_CODE_*: the tests' names)
.vector vr_ag_aim;      // the way it aims now (a unit vector; '0 0 0': as its animation holds the gun)
.float vr_ag_myrun;     // when its own run frames last ran (VR_AG_CheckAttack sends it back to them)
.float vr_ag_lastwid;   // the last weapon it held
void(entity g) VR_AG_DrawMain;            // (a flare gun as its second weapon: below, before VR_AG_SoldierDrop)
void(entity g, vector vel) VR_AG_DropMain;

// The pool's pick (VR_AG_PoolAdd: one pass, each weapon kept with its weight's share of the weights so far).
float vr_ag_pool_total;
float vr_ag_pool_pick;
float vr_ag_shots_fired; // every armed grunt's shots (the tests)
float vr_ag_player_dmg;  // the damage armed grunts dealt players, scaled (the tests)

void(string s) VR_AG_Log =
{
    if(cvar_hget(cvarh_vr_agrunt_debug))
    {
        dprint(sprintf("agrunt: %s\n", s));
    }
};

float(float wid) VR_AG_Class =
{
    if(wid < 0 || wid == WID_FIST) { return VR_AG_CLASS_FISTS; }
    if(wid == WID_AXE || wid == WID_SWORD || wid == WID_CROWBAR || wid == WID_CHAINSAW || wid == WID_MJOLNIR ||
       wid == WID_SUPERAXE)
    {
        return VR_AG_CLASS_MELEE;
    }
    if(wid == WID_GRENADE_LAUNCHER || wid == WID_ROCKET_LAUNCHER || wid == WID_PROXIMITY_GUN || wid == WID_ADROCKET ||
       wid == WID_ADGRENADE)
    {
        return VR_AG_CLASS_EXPLOSIVE;
    }
    if(wid == WID_NAILGUN || wid == WID_SUPER_NAILGUN || wid == WID_LIGHTNING || wid == WID_LASER_CANNON ||
       wid == WID_ADPYRO || wid == WID_ADFUMIGATOR)
    {
        return VR_AG_CLASS_AUTO;
    }
    if((wid == WID_GRUNTGUN && cvar_hget(cvarh_vr_grunt_burst)) || wid == WID_ADPLASMA || wid == WID_ADCENTURION)
    {
        return VR_AG_CLASS_BURST; // (AD's plasma guns: three bolts an attack)
    }
    return VR_AG_CLASS_GUN;
};

// A flamethrower's stream (AD's pyro's and fumigator's: two flames a fire frame, near only).
float(float wid) VR_AG_Stream = { return wid == WID_ADPYRO || wid == WID_ADFUMIGATOR; };

// Whether it can use `wid` (what it can pick up, what the pool and the keys may give it): your weapons, the enemy guns
// it knows, Arcane Dimensions' soldiers' guns while AD's data is loaded; not the grapple or the toolgun.
float(float wid) VR_AG_Usable =
{
    if(wid == WID_FIST) { return TRUE; }
    if(VR_IsADGun(wid)) { return wid != WID_ADELIMINATOR && VR_ADGun_Available(); }
    if(wid == WID_NAILPISTOL) { return TRUE; } // (Quake VR's own: every campaign)
    if(wid == WID_FLAREGUN) { return TRUE; }   // (Quake VR's own: every campaign)
    if(wid == WID_NAILPISTOL || wid == WID_REVOLVER) { return TRUE; } // (Quake VR's own: every campaign)
    if(wid < WID_AXE || wid > WID_SUPERAXE || wid == WID_TOOLGUN) { return FALSE; }
    return VR_Pack_WeaponAvailable(wid);
};

// A full clip of `wid` (your guns' reloading clip; one that has none: 20; AD's guns' rounds or fuel).
float(float wid) VR_AG_ClipSize =
{
    if(wid == WID_CHAINSAW) { return VR_CHAINSAW_TANK; } // (its fuel: a full tank; its reserve's tanks refuel it)
    if(VR_AG_Class(wid) == VR_AG_CLASS_MELEE || VR_AG_Class(wid) == VR_AG_CLASS_FISTS) { return 0; }
    if(VR_IsADGun(wid)) { return VR_ADGun_ClipSize(wid); }
    float c = WeaponIdToBaseClipSize(wid);
    return c > 0 ? c : 20;
};

// ... of the weapon `wid` whose record is `inst` (the Widowmaker's: its whole tube).
float(float wid, entity inst) VR_AG_ClipOf =
{
    if(wid == WID_SUPER_SHOTGUN && inst && AD_Widowmaker(inst))
    {
        return VR_ClipSizeOf(wid, inst);
    }
    return VR_AG_ClipSize(wid);
};

// Its ammo is a clip of its own only (single use): the enemy guns. (In your hands; an armed monster carries spare
// magazines of it too: VR_AG_SpawnReserve.)
float(float wid) VR_AG_SingleUse = { return VR_IsEnemyGun(wid) || wid == WID_FLAREGUN; }; // (the flare gun: its two flares)

// The spare magazines an armed monster spawns with for `wid` at vr_agrunt_reserve's default (2; it scales them all):
// a fight of 20 to 40 s is some 10-20 attacks (a shot, a burst of 3, a stream of 5 or more); two magazines more cover
// that for most guns. The ones whose magazine is a shot or two carry more: the double-barrelled shotgun (2 shells, one
// shot: 2 + 16 shells, 9 shots), the flare gun (2 flares: 2 + 8). The super nailgun (2 nails a shot, 5 shots an
// attack): 3. Melee weapons and fists: none (no magazine); the chainsaw: 2 tanks of fuel.
float(float wid, entity inst) VR_AG_SpareClips =
{
    if(wid == WID_SUPER_SHOTGUN && VR_AG_ClipOf(wid, inst) <= 2) { return 8; } // (not AD's Widowmaker: its tube's 16)
    if(wid == WID_FLAREGUN) { return 4; }
    if(wid == WID_SUPER_NAILGUN) { return 3; }
    return 2;
};

// The rounds beyond its magazine an armed monster spawns with for `wid` (record `inst`): VR_AG_SpareClips magazines,
// times vr_agrunt_reserve / 2. Single-use guns too: it reloads them from its own spare magazines (dropped, the gun has
// only its clip: your ammo never fills it).
float(float wid, entity inst) VR_AG_SpawnReserve =
{
    float clip = VR_AG_ClipOf(wid, inst);
    if(wid < 0 || clip <= 0)
    {
        return 0;
    }
    return rint(clip * VR_AG_SpareClips(wid, inst) * max(0, cvar_hget(cvarh_vr_agrunt_reserve)) / 2);
};

// Nearer than this, it doesn't fire an explosive weapon (its own blast).
float(float wid) VR_AG_MinRange =
{
    if(wid == WID_ROCKET_LAUNCHER || wid == WID_ADROCKET) { return 190; }
    if(wid == WID_GRENADE_LAUNCHER || wid == WID_PROXIMITY_GUN || wid == WID_ADGRENADE)
    {
        // (a grenade bursting on its target there still reaches it: further with vr_agrunt_blast_safe)
        return cvar_hget(cvarh_vr_agrunt_blast_safe) ? 210 : 160;
    }
    return 0;
};

// The chance it fires this frame at a target in sight at range `r` (RANGE_*): each weapon's best ranges.
float(float wid, float r) VR_AG_Chance =
{
    float c = VR_AG_Class(wid);
    if(c == VR_AG_CLASS_EXPLOSIVE)
    {
        if(r == RANGE_NEAR) { return 0.35; }
        if(r == RANGE_MID) { return 0.25; }
        return (wid == WID_ROCKET_LAUNCHER && r == RANGE_FAR) ? 0.05 : 0;
    }
    if(wid == WID_LIGHTNING || VR_AG_Stream(wid))
    {
        return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.5 : 0;
    }
    if(wid == WID_SUPER_SHOTGUN)
    {
        return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.5 : r == RANGE_MID ? 0.03 : 0;
    }
    if(c == VR_AG_CLASS_AUTO)
    {
        return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.45 : r == RANGE_MID ? 0.15 : 0;
    }
    return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.4 : r == RANGE_MID ? 0.08 : 0; // (the grunt's own)
};

// ----------------------------------------------------------------------------
// Its fighting distance (vr_agrunt_range; note 2026-10-26 22:19:49; ROUND22.md, "Armed humanoids' fighting distance")
// ----------------------------------------------------------------------------
// A gun's holder keeps its weapon's distance from its enemy, not the grunt's run into his reach: nearer than the least
// it backs off facing him (along a wall when the way back is shut; as before when hemmed in), about its best it
// strafes round him, beyond it it comes on at a slant, beyond the most as before (its path). It walks as it fires
// (vr_agrunt_range_walkfire) and steps back as it reloads. You holding a melee weapon or charging it: its least and
// best further (its row's vr_am_range_threat), it backs off faster; hurt badly: a little further. Melee weapons and
// fists close in as before. Each step is walkmove's: never off a ledge nor into a wall (it fails, nothing moves).

float vr_ag_rmin;    // VR_AG_Ranges': nearer than this it backs off,
float vr_ag_rpref;   // ... about this it strafes (up to 40 beyond),
float vr_ag_rmax;    // ... beyond this it comes on as before
float vr_ag_rthreat; // ... its enemy threatens it up close (a melee weapon in his hand, charging it)
.float vr_ag_rback;       // backing off (until it is halfway from its least to its best)
.float vr_ag_rflip;       // its strafe's turn round next then
.float vr_ag_rthreat_until; // its enemy charged it: wary until then
.float vr_ag_hp0;         // its health at its fullest (seen)
.float vr_ag_rwalk_at;    // its last step walking as it fired (VR_AG_WalkFire)
float vr_ag_range_backs;  // steps it took backing off (the tests)

// The fighting distances of weapon `wid` (vr_ag_rmin, vr_ag_rpref, vr_ag_rmax; FALSE: a melee weapon or its fists).
float(float wid) VR_AG_RangeOf =
{
    float c = VR_AG_Class(wid);
    if(c == VR_AG_CLASS_FISTS || c == VR_AG_CLASS_MELEE) { return FALSE; }
    vr_ag_rmin = 128; vr_ag_rpref = 220; vr_ag_rmax = 380; // (a shotgun, the grunt's gun, a pistol: near)
    if(wid == WID_SUPER_SHOTGUN) { vr_ag_rmin = 96; vr_ag_rpref = 150; vr_ag_rmax = 260; } // (close, out of reach)
    else if(VR_AG_Stream(wid))
    {
        float reach = VR_ADGun_Value(wid, "range"); // (a flamethrower: well inside its stream's reach)
        vr_ag_rmin = 80; vr_ag_rpref = max(110, reach * 0.45); vr_ag_rmax = max(160, reach * 0.75);
    }
    else if(wid == WID_LIGHTNING) { vr_ag_rmin = 140; vr_ag_rpref = 240; vr_ag_rmax = 400; } // (its beam's 600)
    else if(c == VR_AG_CLASS_EXPLOSIVE)
    {
        float m = VR_AG_MinRange(wid); // (its blast's: VR_AG_Run backs off nearer still)
        vr_ag_rmin = m + 40; vr_ag_rpref = m + 190; vr_ag_rmax = m + 440;
    }
    else if(c == VR_AG_CLASS_AUTO) { vr_ag_rmin = 224; vr_ag_rpref = 380; vr_ag_rmax = 600; } // (nail guns: far)
    else if(c == VR_AG_CLASS_BURST) { vr_ag_rmin = 160; vr_ag_rpref = 300; vr_ag_rmax = 480; }
    else if(wid == WID_ENFRIFLE || wid == WID_ADCROSSBOW) { vr_ag_rmin = 192; vr_ag_rpref = 340; vr_ag_rmax = 560; }
    return TRUE;
};

// Its enemy `e` threatens `g` up close: a melee weapon in either hand, or coming at it fast (wary a while after).
float(entity g, entity e) VR_AG_Threat =
{
    if(!(e.flags & FL_CLIENT))
    {
        return FALSE;
    }
    vector to = g.origin - e.origin;
    to_z = 0;
    vector v = e.velocity;
    v_z = 0;
    if(vlen(to) > 1 && v * normalize(to) > 180)
    {
        g.vr_ag_rthreat_until = time + 1.5;
    }
    return VR_AG_Class(e.weapon) == VR_AG_CLASS_MELEE || VR_AG_Class(e.weapon2) == VR_AG_CLASS_MELEE ||
           time < g.vr_ag_rthreat_until;
};

// `g`'s fighting distances now (vr_ag_r*: its weapon's, its row's, you, its health); FALSE: it closes in as before.
float(entity g) VR_AG_Ranges =
{
    if(!cvar_hget(cvarh_vr_agrunt_range) || !g.vr_ag_prop || !g.enemy || g.enemy.health <= 0 || !VR_AG_RangeOf(g.vr_ag_wid))
    {
        return FALSE;
    }
    VR_AM_Row(g.vr_ag_kind);
    float s = max(0, cvar_hget(cvarh_vr_agrunt_range_scale)) * vr_am_range;
    float wary = s; // (its least and best's scale)
    vr_ag_rthreat = VR_AG_Threat(g, g.enemy);
    if(vr_ag_rthreat)
    {
        wary = wary * vr_am_range_threat;
    }
    g.vr_ag_hp0 = max(g.vr_ag_hp0, g.health);
    if(g.health < 0.35 * g.vr_ag_hp0)
    {
        wary = wary * 1.2; // (badly hurt: warier)
    }
    vr_ag_rmin = max(vr_ag_rmin * wary, VR_AG_MinRange(g.vr_ag_wid) + 10);
    vr_ag_rpref = max(vr_ag_rpref * wary, vr_ag_rmin + 40);
    vr_ag_rmax = max(vr_ag_rmax * s, vr_ag_rpref + 80);
    return TRUE;
};

// A step of `d` along `yaw` (TRUE: taken; walkmove's checks: no ledge, no wall).
float(float yaw, float d) VR_AG_RangeStep =
{
    if(d <= 0)
    {
        return FALSE;
    }
    return walkmove(yaw, d);
};

// ai_run (its moves between its attacks, `dist` its run frame's): keeping its distance; FALSE: as before.
float(float dist) VR_AG_RangeMove =
{
    if(!enemy_vis || !VR_AG_Ranges(self))
    {
        self.vr_ag_rback = 0;
        return FALSE;
    }
    float d = vlen(VR_AI_Origin(self.enemy) - self.origin);
    if(d > vr_ag_rmax)
    {
        self.vr_ag_rback = 0;
        return FALSE; // (far: on along its path, as before)
    }
    float yaw = enemy_yaw;
    self.ideal_yaw = yaw;
    ChangeYaw(); // (facing him all the while)
    float side = self.lefty ? 1 : -1;
    if(d < vr_ag_rmin || (self.vr_ag_rback && d < (vr_ag_rmin + vr_ag_rpref) * 0.5))
    {
        // Backing off, facing him (faster from a blade or a charge); the way back shut: along the wall.
        self.vr_ag_rback = 1;
        float sp = dist * (vr_ag_rthreat ? 1 : 0.85);
        if(VR_AG_RangeStep(yaw + 180 + 30 * side, sp) || VR_AG_RangeStep(yaw + 180 - 30 * side, sp) ||
           VR_AG_RangeStep(yaw + 90 * side, sp))
        {
            vr_ag_range_backs = vr_ag_range_backs + 1;
            return TRUE;
        }
        self.lefty = !self.lefty;
        if(VR_AG_RangeStep(yaw - 90 * side, sp))
        {
            vr_ag_range_backs = vr_ag_range_backs + 1;
            return TRUE;
        }
        self.vr_ag_rback = 0;
        return FALSE; // (hemmed in: as before, its shove and its butt up close)
    }
    self.vr_ag_rback = 0;
    if(d > vr_ag_rpref + 40)
    {
        // Gaining ground at a slant (a harder target than straight on); blocked: as before.
        return VR_AG_RangeStep(yaw + 35 * side, dist);
    }
    // About its best: strafing round him, turning round now and then and where it is blocked; boxed in: it holds.
    if(time > self.vr_ag_rflip)
    {
        self.vr_ag_rflip = time + 1.5 + random() * 2;
        if(random() < 0.5)
        {
            self.lefty = !self.lefty;
            side = -side;
        }
    }
    if(!VR_AG_RangeStep(yaw + 90 * side, dist * 0.6))
    {
        self.lefty = !self.lefty;
        VR_AG_RangeStep(yaw - 90 * side, dist * 0.6);
    }
    return TRUE;
};

// An attack's or a reload's frame lasting `dt`: its step by its fighting distance as it fires (vr_agrunt_range_walkfire;
// `firing` FALSE: a reload's, only away when nearer than its best). TRUE: it moved.
float(float dt, float firing) VR_AG_WalkFire =
{
    float v = cvar_hget(cvarh_vr_agrunt_range_walkfire);
    if(v <= 0 || self.health <= 0 || time - self.vr_ag_rwalk_at < 0.09 || !VR_AG_Ranges(self) || !visible(self.enemy))
    {
        return FALSE; // (once a frame: an aim cone's wait calls its frame again)
    }
    self.vr_ag_rwalk_at = time;
    float d = vlen(VR_AI_Origin(self.enemy) - self.origin);
    float yaw = vectoyaw(VR_AI_Origin(self.enemy) - self.origin);
    float step = v * dt;
    float side = self.lefty ? 1 : -1;
    if(d < vr_ag_rmin || (!firing && d < vr_ag_rpref))
    {
        // (faster nearer than its least, a reload's too: you with a blade gain on it otherwise)
        float back = d < vr_ag_rmin ? (vr_ag_rthreat ? 1.6 : 1.3) : 1;
        if(VR_AG_RangeStep(yaw + 180, step * back))
        {
            vr_ag_range_backs = vr_ag_range_backs + 1;
            return TRUE;
        }
        return VR_AG_RangeStep(yaw + 90 * side, step * 0.6);
    }
    if(!firing)
    {
        return FALSE;
    }
    if(d > vr_ag_rpref + 40)
    {
        return VR_AG_Class(self.vr_ag_wid) != VR_AG_CLASS_EXPLOSIVE && VR_AG_RangeStep(yaw + 20 * side, step);
    }
    return VR_AG_RangeStep(yaw + 90 * side, step * 0.6);
};

// ----------------------------------------------------------------------------
// Its weapon: the pool, the held prop
// ----------------------------------------------------------------------------

// The "weapon" keys beyond the weapon ids (the toolgun's and the pool's): AD's upgrades (the weapon it upgrades, its
// record's upgrade) and AD's melee weapons (the knight's sword of that kind: VR_ADMelee_Flags).
float VR_AG_CODE_SHADOWAXE = 101;
float VR_AG_CODE_WIDOWMAKER = 102;
float VR_AG_CODE_PLASMAGUN = 103;
float VR_AG_CODE_ADMELEE = 110; // 111..120: kind 1..10 (VR_ADMELEE_*; not the flail, 11: its head on its chain is only
                                // a player's hand's, vr_box3d_flail.cpp: a grunt would swing a bare handle)

float vr_ag_dec_wid;   // VR_AG_Decode's: the weapon,
float vr_ag_dec_flags; // ... its flags,
float vr_ag_dec_up;    // ... its record's AD upgrade (0: none)

// A "weapon" key `code` as a weapon, its flags and its upgrade (vr_ag_dec_*); FALSE: none it can hold.
float(float code) VR_AG_Decode =
{
    vr_ag_dec_wid = code;
    vr_ag_dec_flags = 0;
    vr_ag_dec_up = 0;
    if(code == VR_AG_CODE_SHADOWAXE || code == VR_AG_CODE_WIDOWMAKER || code == VR_AG_CODE_PLASMAGUN)
    {
        vr_ag_dec_wid = code == VR_AG_CODE_SHADOWAXE ? WID_AXE : code == VR_AG_CODE_WIDOWMAKER ? WID_SUPER_SHOTGUN : WID_LIGHTNING;
        vr_ag_dec_up = AD_UpgradeFor(vr_ag_dec_wid);
        return vr_ad_data && VR_AD_FeatureScope(AD_UpgradeScope()); // (elsewhere a plain one: not this)
    }
    if(code > VR_AG_CODE_ADMELEE && code <= VR_AG_CODE_ADMELEE + VR_ADMELEE_JUDGE) // (the flail, 11: not a grunt's)
    {
        float kind = code - VR_AG_CODE_ADMELEE;
        vr_ag_dec_wid = WID_SWORD;
        vr_ag_dec_flags = VR_ADMelee_Flags(kind);
        if(kind == VR_ADMELEE_PHANTOM)
        {
            return VR_Has("progs/sword.mdl"); // (the Phantom Swordsman's: Rogue's data, AD's or not)
        }
        return cvar("vr_ad_mounted") != 0;
    }
    return VR_AG_Usable(code);
};

// Its name (the tests, the log).
string(float wid, float flags, entity inst) VR_AG_NameOf =
{
    if(wid < 0) { return "fists"; }
    if(inst && inst.wi_adup && vr_ad_data)
    {
        if(wid == WID_AXE) { return "Shadow Axe"; }
        if(wid == WID_SUPER_SHOTGUN) { return "Widowmaker"; }
        if(wid == WID_LIGHTNING) { return "Plasma Gun"; }
    }
    if(VR_IsADGun(wid)) { return VR_ADGun_Name(wid); }
    return WeaponNameOf(wid, flags);
};

void(float code, float w) VR_AG_PoolAdd =
{
    if(w <= 0 || !VR_AG_Decode(code))
    {
        return;
    }
    vr_ag_pool_total = vr_ag_pool_total + w;
    if(random() * vr_ag_pool_total < w)
    {
        vr_ag_pool_pick = code;
    }
};

// A weapon from the pool (-1: its fists).
float() VR_AG_PickWeapon =
{
    vr_ag_pool_total = 0;
    vr_ag_pool_pick = -1;
    VR_AG_PoolAdd(WID_FIST, cvar_hget(cvarh_vr_agrunt_w_fists));
    VR_AG_PoolAdd(WID_AXE, cvar_hget(cvarh_vr_agrunt_w_axe));
    VR_AG_PoolAdd(WID_SWORD, cvar_hget(cvarh_vr_agrunt_w_sword));
    VR_AG_PoolAdd(WID_CROWBAR, cvar_hget(cvarh_vr_agrunt_w_crowbar));
    VR_AG_PoolAdd(WID_CHAINSAW, cvar_hget(cvarh_vr_agrunt_w_chainsaw));
    VR_AG_PoolAdd(WID_SHOTGUN, cvar_hget(cvarh_vr_agrunt_w_shotgun));
    VR_AG_PoolAdd(WID_SUPER_SHOTGUN, cvar_hget(cvarh_vr_agrunt_w_ssg));
    VR_AG_PoolAdd(WID_NAILGUN, cvar_hget(cvarh_vr_agrunt_w_nailgun));
    VR_AG_PoolAdd(WID_SUPER_NAILGUN, cvar_hget(cvarh_vr_agrunt_w_sng));
    VR_AG_PoolAdd(WID_GRENADE_LAUNCHER, cvar_hget(cvarh_vr_agrunt_w_gl));
    VR_AG_PoolAdd(WID_ROCKET_LAUNCHER, cvar_hget(cvarh_vr_agrunt_w_rl));
    VR_AG_PoolAdd(WID_LIGHTNING, cvar_hget(cvarh_vr_agrunt_w_lg));
    VR_AG_PoolAdd(WID_GRUNTGUN, cvar_hget(cvarh_vr_agrunt_w_gruntgun));
    VR_AG_PoolAdd(WID_ENFRIFLE, cvar_hget(cvarh_vr_agrunt_w_enfrifle));
    VR_AG_PoolAdd(WID_NAILPISTOL, cvar_hget(cvarh_vr_agrunt_w_nailpistol));
    VR_AG_PoolAdd(WID_REVOLVER, cvar_hget(cvarh_vr_agrunt_w_revolver)); // (rare: vr_revolver.qc)
    VR_AG_PoolAdd(WID_MJOLNIR, cvar_hget(cvarh_vr_agrunt_w_packs));
    VR_AG_PoolAdd(WID_LASER_CANNON, cvar_hget(cvarh_vr_agrunt_w_packs));
    VR_AG_PoolAdd(WID_PROXIMITY_GUN, cvar_hget(cvarh_vr_agrunt_w_packs));
    // Arcane Dimensions' (VR_AG_Decode: only with its data; the upgrades where AD's upgrades play).
    VR_AG_PoolAdd(WID_ADROCKET, cvar("vr_agrunt_w_adrocket"));
    VR_AG_PoolAdd(WID_ADGRENADE, cvar("vr_agrunt_w_adgrenade"));
    VR_AG_PoolAdd(WID_ADPLASMA, cvar("vr_agrunt_w_adplasma"));
    VR_AG_PoolAdd(WID_ADDEFENDER, cvar("vr_agrunt_w_addefender"));
    VR_AG_PoolAdd(WID_ADCENTURION, cvar("vr_agrunt_w_adcenturion"));
    VR_AG_PoolAdd(WID_ADPYRO, cvar("vr_agrunt_w_adpyro"));
    VR_AG_PoolAdd(WID_ADFUMIGATOR, cvar("vr_agrunt_w_adfumigator"));
    VR_AG_PoolAdd(WID_ADCROSSBOW, cvar("vr_agrunt_w_adcrossbow"));
    VR_AG_PoolAdd(VR_AG_CODE_SHADOWAXE, cvar("vr_agrunt_w_shadowaxe"));
    VR_AG_PoolAdd(VR_AG_CODE_WIDOWMAKER, cvar("vr_agrunt_w_widowmaker"));
    VR_AG_PoolAdd(VR_AG_CODE_PLASMAGUN, cvar("vr_agrunt_w_plasmagun"));
    for(float k = 1; k <= VR_ADMELEE_MACE; k++)
    {
        VR_AG_PoolAdd(VR_AG_CODE_ADMELEE + k, cvar(VR_ADMelee_Heavy(k) ? "vr_agrunt_w_adheavy" : "vr_agrunt_w_adsword"));
    }
    // The monster swords (rare: low weights): the Phantom Swordsman's (with Rogue's data), the Dark Prince's, the justice
    // knight's, the judicator's.
    VR_AG_PoolAdd(VR_AG_CODE_ADMELEE + VR_ADMELEE_PHANTOM, cvar("vr_agrunt_w_phantom"));
    VR_AG_PoolAdd(VR_AG_CODE_ADMELEE + VR_ADMELEE_DPRINCE, cvar("vr_agrunt_w_dprince"));
    VR_AG_PoolAdd(VR_AG_CODE_ADMELEE + VR_ADMELEE_JUSTICE, cvar("vr_agrunt_w_justice"));
    VR_AG_PoolAdd(VR_AG_CODE_ADMELEE + VR_ADMELEE_JUDGE, cvar("vr_agrunt_w_judge"));
    if(vr_ag_pool_pick == WID_FIST)
    {
        return -1;
    }
    return vr_ag_pool_pick;
};

// `v` rotated about the unit axis `k` by `deg` degrees (Rodrigues; vr_axestick.qc has the same).
vector(vector v, vector k, float deg) VR_AG_Rotate =
{
    float c = cos(deg * (3.14159265 / 180));
    float s = sin(deg * (3.14159265 / 180));
    vector kv;
    kv_x = k_y * v_z - k_z * v_y;
    kv_y = k_z * v_x - k_x * v_z;
    kv_z = k_x * v_y - k_y * v_x;
    return v * c + kv * s + k * ((k * v) * (1 - c));
};

// The held weapon to the grunt's right hand as drawn now, along its hidden gun's line (a melee weapon upright),
// pointing where it aims, kicked back by a shot, swung by a blow. Its view angles (VR_AxeStick_Angles') in .v_angle:
// the shots' and a drop's.
void(entity g, entity p) VR_AGP_Pose; // (vr_armedgrunt_pose.qc: the pose, its attacks, sent to the clients)
void() VR_AGP_Attack;
void(entity g) VR_AGP_Spawn;

float(entity g) VR_AG_Taking;
void() VR_AG_TakeGrab;

void(entity g, entity p) VR_AG_PlaceProp =
{
    // A weapon being taken up from the floor, its reach cut short (a pain, a shove, a death) before its hand closed on
    // it: it stays on the floor where it lay (VR_AG_TakeBegin).
    if(g.vr_ag_took_at > 0 && time < g.vr_ag_took_grab && !VR_AG_Taking(g))
    {
        g.vr_ag_took_at = 0;
        if(g.vr_ag_prop2 && !g.vr_ag_prop.vr_ag_left)
        {
            VR_AG_Swap(g); // (its second, its left hand's, was the one it reached for)
        }
        entity t = g.vr_ag_prop;
        if(t)
        {
            setorigin(t, g.vr_ag_took_org);
            VR_AG_DropHeld(g, '0 0 0');
            VR_AG_Log("its reach for a weapon cut short: it stays on the floor");
            return;
        }
    }
    if(g.vr_ag_took_at > 0 && time >= g.vr_ag_took_grab && !g.vr_ag_took_done)
    {
        // (Its hand closed on it: by time, not its take frame: a sighting's FoundTarget may send it on to its run.)
        g.vr_ag_took_done = 1;
        entity oldself = self;
        self = g;
        VR_AG_TakeGrab();
        self = oldself;
    }
    if(isragdoll(g))
    {
        // A ragdoll now (a stagger, a tug, falling): its weapons ride its hands' limbs as they were held when it began
        // (the client draws them where the server has them: vr_hitreact.cpp relinkArms leaves a ragdoll's alone).
        VR_AG_RagProp(g, p);
        if(g.vr_ag_prop2 && g.vr_ag_prop2 != p)
        {
            VR_AG_RagProp(g, g.vr_ag_prop2);
        }
        return;
    }
    p.vr_ag_rb = 0;
    if(g.vr_ag_prop2)
    {
        g.vr_ag_prop2.vr_ag_rb = 0;
    }
    VR_AGP_Pose(g, p);
};

// The old carry (before vr_armedgrunt_pose.qc): kept for reference, unused.
void(entity g, entity p) VR_AG_PlacePropOld =
{
    vector hand = VR_AM_Hand(g);
    if(trace_fraction == 0)
    {
        return; // (no such model now: a ragdoll's, another's; it stays where it was)
    }
    vector rear = VR_AM_GunRear(g);
    vector muzzle = VR_AM_GunMuzzle(g);
    vector up = VR_AM_GunUp(g);
    vector fw = normalize(muzzle - rear);
    if(VR_AG_Class(g.vr_ag_wid) == VR_AG_CLASS_MELEE && g.vr_ag_wid != WID_CHAINSAW)
    {
        // A blade or a club over its shoulder (the rifle's line the other way: its rear rests there), its edge ahead.
        fw = -fw;
        up = -up;
    }
    if(g.vr_ag_aim != '0 0 0')
    {
        fw = g.vr_ag_aim;
    }
    up = normalize(up - fw * (up * fw));
    if(up == '0 0 0')
    {
        up = '0 0 1';
    }
    vector right;
    right_x = fw_y * up_z - fw_z * up_y;
    right_y = fw_z * up_x - fw_x * up_z;
    right_z = fw_x * up_y - fw_y * up_x;

    // A melee blow's arc: raised back, then down through the target.
    float s = (time - g.vr_ag_swing_at) / VR_AG_SWING_TIME;
    if(g.vr_ag_swing_at > 0 && s >= 0 && s < 1)
    {
        float deg = 60 - 150 * (s * s * (3 - 2 * s));
        fw = VR_AG_Rotate(fw, right, deg);
        up = VR_AG_Rotate(up, right, deg);
        // The hand goes with it (the client's arm follows the grip: vr_agrunt_ik): round its right shoulder (the rig's
        // upperarm_r joint, about), most of the blade's turn.
        makevectors([0, g.angles_y, 0]);
        vector shoulder = g.origin + v_up * 12 + v_right * 9.7;
        hand = shoulder + VR_AG_Rotate(hand - shoulder, v_right, deg * 0.6);
    }

    // A shot's kick: back along it and its muzzle up, dying out in 0.15 s.
    float k = 1 - (time - g.vr_ag_kick_at) / 0.15;
    if(g.vr_ag_kick_at > 0 && k > 0)
    {
        hand = hand - fw * (2.5 * k);
        fw = VR_AG_Rotate(fw, right, 9 * k);
        up = VR_AG_Rotate(up, right, 9 * k);
    }

    vector a = VR_AxeStick_Angles(fw, up);
    p.v_angle = a;
    p.angles = a;
    p.angles_x = -a_x; // (a model's pitch)
    setorigin(p, hand);
};

void() VR_AG_PropHandTouch;

// A held weapon's box (its model just set: its bounds): a cube round its origin (its grip) that holds the model however
// it is turned, 2 units more, at least the old 10. The hands' touches find it by this box (vr_physics.cpp handTouches),
// then a grab takes it only with the fist on its drawn surface (handOn): a grab anywhere on it, its muzzle and its stock
// too (a 10-unit box left a shotgun's last 1-2 units and a long gun's barrel out of reach).
void(entity p) VR_AG_PropSize =
{
    vector a;
    a_x = max(fabs(p.mins_x), fabs(p.maxs_x));
    a_y = max(fabs(p.mins_y), fabs(p.maxs_y));
    a_z = max(fabs(p.mins_z), fabs(p.maxs_z));
    float r = max(10, vlen(a) + 2);
    setsize(p, '-1 -1 -1' * r, '1 1 1' * r);
};

// Its place and turn kept a frame or two (the grab's sweep: the engine's handOn tests the fist against them too, the
// weapon as drawn a frame behind the server's: vr_agrunt_grab_sweep). Each think, before it is placed again.
void(entity p) VR_AG_PropWas =
{
    p.vr_ag_was_o2 = p.vr_ag_was_o1;
    p.vr_ag_was_a2 = p.vr_ag_was_a1;
    p.vr_ag_was_o1 = p.origin;
    p.vr_ag_was_a1 = p.angles;
    p.vr_ag_was_n = min(2, p.vr_ag_was_n + 1);
};

// Every server frame: to its hand; gone with its grunt.
void() VR_AG_PropThink =
{
    VR_AG_PropWas(self);
    entity g = self.owner;
    if(g.MG_deferred && g.vr_ag && g.health > 0 && (g.vr_ag_prop == self || g.vr_ag_prop2 == self))
    {
        // (its holder waits unseen for its trigger, MG_MonsterPrepare: unseen with it, back when it comes)
        if(self.model != "")
        {
            self.vr_agr_mdl = self.model;
            setmodel(self, "");
        }
        self.nextthink = time + 0.05;
        return;
    }
    if(self.model == "" && self.vr_agr_mdl != "")
    {
        setmodel(self, self.vr_agr_mdl);
        VR_AG_PropSize(self);
        self.vr_agr_mdl = "";
    }
    if(g.vr_ag_prop2 == self && g.vr_ag && g.health > 0)
    {
        // The second weapon: placed with the first (VR_AGP_Pose: both hands); a blow or a throw on it, the first slot's
        // rules (swapped there and back).
        self.nextthink = time;
        if(!g.vr_ag_tug_pl)
        {
            VR_AG_Swap(g);
            float struck = VR_AG_WeaponStruck(g, self);
            if(!struck && g.vr_ag_prop2 && g.vr_ag_prop == self)
            {
                VR_AG_Swap(g);
            }
        }
        return;
    }
    if(g.vr_ag_prop != self || !g.vr_ag || g.health <= 0)
    {
        if(g.vr_ag_tug_pl && (g.vr_ag_prop == self || !g.vr_ag_prop))
        {
            VR_AG_TugEnd(g, FALSE, "its weapon gone"); // (never left on: its hold, its stagger)
        }
        if(self.weapon == WID_CHAINSAW)
        {
            sound(self, CHAN_WEAPON, "misc/null.wav", 1, ATTN_NORM); // (its engine's loop)
        }
        if(VR_AG_Stream(self.weapon) && (g.vr_ag_prop == self || !g.vr_ag_prop))
        {
            VR_ADGun_MonsterStreamEnd(g, self.weapon, TRUE); // (its wielder dead or gone while firing: its loop)
        }
        self.weaponinst = world; // (its record goes with a drop, if any: never freed here)
        remove(self);
        return;
    }
    // (held still, vr_agrunt_freeze: the Grip Editor, the Pose Studio, hands reaching into its weapons: no tug, no blow)
    float still = VR_AG_Still(g);
    if(!still && VR_AG_TugFrame(g, self)) // (a tug-of-war over it: vr_armedgrunt_play.qc)
    {
        if(g.vr_ag_prop == self)
        {
            self.nextthink = time;
        }
        return;
    }
    if(!still && VR_AG_WeaponStruck(g, self)) // (a hard blow or a throw on the weapon itself: vr_armedgrunt_play.qc)
    {
        return;
    }
    if(g.vr_knockdown && cvar_hget(cvarh_vr_agrunt_disarm) && VR_AG_FallDrop(g)) // (on the floor: it lets go)
    {
        return;
    }
    VR_AGPar_Frame(g); // (a melee blow coming: its parry raised: vr_armedgrunt_parry.qc)
    VR_AG_PlaceProp(g, self);
    if(g.vr_adgun_stream && time - g.vr_adgun_stream > 0.4)
    {
        VR_ADGun_MonsterStreamEnd(g, self.weapon, FALSE); // (no flame for a while: its attack cut short, a pain)
    }
    VR_FuelTank_Wear(self); // (a flamethrower's or sprayer's tank on its back: vr_adtankworn.qc)
    self.nextthink = time;
};

// A weapon one hand swings (dual wielding pairs these: VR_AG_ArmLeft): a sword (not AD's hammer and mace), the crowbar,
// the axe (two-handed alone: vr_armedgrunt_pose.qc VR_AGP_TwoHanded). No gun: they are long guns its animations hold in
// both hands (the grunt's and the enforcer's rifle lines).
float(float wid, float flags) VR_AG_OneHanded =
{
    if(wid == WID_SWORD)
    {
        return !VR_ADMelee_Heavy(VR_ADMelee_Kind(flags));
    }
    return wid == WID_CROWBAR || wid == WID_AXE;
};

// The armed table's "too big to dual wield" flag, per weapon (Vittorio's list, ROUND22.md "Armed monsters: dual-wielded
// guns"): the hammers (Mjolnir, AD's hammer and mace), the rocket launchers, the Laser Cannon, the chainsaw; the two-handed
// melee weapons besides (VR_AG_OneHanded) and the flamethrowers (their tank on its back: one back, one tank).
float(float wid, float flags) VR_AG_Big =
{
    if(wid == WID_SWORD)
    {
        return VR_ADMelee_Heavy(VR_ADMelee_Kind(flags));
    }
    return wid == WID_MJOLNIR || wid == WID_ROCKET_LAUNCHER || wid == WID_ADROCKET || wid == WID_LASER_CANNON ||
           wid == WID_CHAINSAW || wid == WID_SUPERAXE || VR_AG_Stream(wid);
};

// A gun one hand holds in a pair (dual wielding: a two-handed gun in each hand, as you can): any gun not too big.
float(float wid, float flags) VR_AG_DualGun =
{
    float c = VR_AG_Class(wid);
    return c != VR_AG_CLASS_MELEE && c != VR_AG_CLASS_FISTS && !VR_AG_Big(wid, flags) &&
           wid != WID_FLAREGUN; // (the flare gun: fired first and thrown away, never one of a pair)
};

// It may go in a pair at all (VR_AG_Pairs: with one of its own kind).
float(float wid, float flags) VR_AG_Pairable = { return VR_AG_OneHanded(wid, flags) || VR_AG_DualGun(wid, flags); };

// Two weapons held one in each hand: two one-handed melee weapons, or two guns not too big (never a blade and a gun:
// the poses are the melee pair's or the guns' pair's).
float(float w1, float f1, float w2, float f2) VR_AG_Pairs =
{
    if(VR_AG_OneHanded(w1, f1))
    {
        return VR_AG_OneHanded(w2, f2);
    }
    return VR_AG_DualGun(w1, f1) && VR_AG_DualGun(w2, f2);
};

// Holding two guns (dual wielding them: the penalties, both fired).
// (a gun and an axe, vr_ag_gunaxe: not two guns)
float(entity g) VR_AG_DualGuns =
{
    return g.vr_ag_prop2 && VR_AG_Class(g.vr_ag_wid) != VR_AG_CLASS_MELEE && VR_AG_Class(g.vr_ag_wid2) != VR_AG_CLASS_MELEE;
};

// VR_AG_Pairs for `g`: one with a gun and an axe (vr_ag_gunaxe) pairs a gun with a one-handed blade too.
float(entity g, float w1, float f1, float w2, float f2) VR_AG_PairsFor =
{
    if(VR_AG_Pairs(w1, f1, w2, f2))
    {
        return TRUE;
    }
    return g.vr_ag_gunaxe && ((VR_AG_DualGun(w1, f1) && VR_AG_OneHanded(w2, f2)) ||
                              (VR_AG_OneHanded(w1, f1) && VR_AG_DualGun(w2, f2)));
};

// The weapon slots swapped (the layer acts on the first: a hand's event on the second's weapon swaps them first). Which
// hand holds each stays its prop's (.vr_ag_left).
void(entity g) VR_AG_Swap =
{
    float w = g.vr_ag_wid;       g.vr_ag_wid = g.vr_ag_wid2;         g.vr_ag_wid2 = w;
    float f = g.vr_ag_wflags;    g.vr_ag_wflags = g.vr_ag_wflags2;   g.vr_ag_wflags2 = f;
    entity p = g.vr_ag_prop;     g.vr_ag_prop = g.vr_ag_prop2;       g.vr_ag_prop2 = p;
    float r = g.vr_ag_reserve;   g.vr_ag_reserve = g.vr_ag_reserve2; g.vr_ag_reserve2 = r;
};

// After a weapon left a hand: the other (if any) to the first slot, in its right hand (passed over: its poses are the
// right hand's alone).
void(entity g) VR_AG_Settle =
{
    if(!g.vr_ag_prop && g.vr_ag_prop2)
    {
        VR_AG_Swap(g);
    }
    if(g.vr_ag_prop && !g.vr_ag_prop2)
    {
        g.vr_ag_prop.vr_ag_left = 0;
    }
    if(!g.vr_ag_prop2)
    {
        g.vr_ag_wid2 = -1;
        g.vr_ag_wflags2 = 0;
    }
};

entity(entity g, float wid, float flags, entity inst) VR_AG_MakeProp;

// `g` takes up a weapon (its record `inst`: its magazine).
void(entity g, float wid, float flags, entity inst) VR_AG_Arm =
{
    g.vr_ag_wid = wid;
    g.vr_ag_wflags = flags;
    g.vr_ag_lastwid = wid;
    g.vr_ag_aim = '0 0 0';
    g.vr_ag_seek = world;
    if(wid < 0 || wid == WID_FIST)
    {
        g.vr_ag_wid = -1;
        g.vr_ag_prop = world;
        return;
    }
    g.vr_ag_prop = VR_AG_MakeProp(g, wid, flags, inst);
    VR_AG_PlaceProp(g, g.vr_ag_prop);
};

// ... a second weapon, in its left hand (a pair: VR_AG_Pairs; FALSE: not taken).
float(entity g, float wid, float flags, entity inst) VR_AG_ArmLeft =
{
    if(!g.vr_ag_prop || g.vr_ag_prop2 || !VR_AG_PairsFor(g, g.vr_ag_wid, g.vr_ag_wflags, wid, flags))
    {
        return FALSE;
    }
    g.vr_ag_wid2 = wid;
    g.vr_ag_wflags2 = flags;
    g.vr_ag_reserve2 = 0;
    g.vr_ag_prop2 = VR_AG_MakeProp(g, wid, flags, inst);
    g.vr_ag_prop2.vr_ag_left = 1;
    g.vr_ag_prop.vr_ag_left = 0;
    if(g.vr_ag_gunaxe && VR_AG_Class(wid) != VR_AG_CLASS_MELEE && VR_AG_Class(g.vr_ag_wid) == VR_AG_CLASS_MELEE)
    {
        // (a gun and an axe: the gun in its right hand, the blade in its left, whichever came first)
        g.vr_ag_prop2.vr_ag_left = 0;
        g.vr_ag_prop.vr_ag_left = 1;
    }
    g.vr_ag_dual = 1;
    VR_AG_PlaceProp(g, g.vr_ag_prop);
    return TRUE;
};

// The held weapon's entity (classname vr_ag_weapon), its record `inst` (world: a new one, full).
entity(entity g, float wid, float flags, entity inst) VR_AG_MakeProp =
{
    entity p = spawn();
    p.classname = "vr_ag_weapon";
    p.owner = g;
    p.weapon = wid;
    p.weaponflags = flags;
    p.weaponinst = inst ? inst : WeaponInst_New(VR_AG_ClipSize(wid));
    p.netname = VR_AG_NameOf(wid, flags, p.weaponinst);
    if(wid == WID_FLAREGUN)
    {
        p.skin = VR_FlareGun_Skin(p.weaponinst.wi_clip); // (its barrels fired: vr_flaregun.qc)
    }
    p.movetype = MOVETYPE_NONE;
    p.solid = SOLID_TRIGGER;
    p.effects = VR_AG_EF_HELD; // (the client finds it so: its muzzle flash, the arms' grips)
    VR_ADGun_Placed(wid); // (AD's guns' and melee weapons' models made, their sounds precached: any map)
    VR_ADMelee_Placed(wid, flags);
    if(p.weaponinst.wi_adup)
    {
        AD_WeaponsOnDemand(); // (AD's upgrades' shots' sounds, any map: the Plasma Gun's was not precached outside AD's)
    }
    string m = AD_WeaponModel(wid, flags, p.weaponinst);
    precache_model(m);
    setmodel(p, m);
    VR_AG_PropSize(p);
    p.handtouch = VR_AG_PropHandTouch;
    p.think = VR_AG_PropThink;
    p.nextthink = time;
    return p;
};

// Its pockets: the spare rounds of a weapon no longer in its hands (knocked out of them, yanked, thrown away, dropped as
// it dies; a flare gun's stowed weapon as it dies), kept for its backpack (VR_AG_SoldierDrop) and for the same weapon
// taken up again (VR_AG_Unpocket): a real ammo kind's in its .ammo_* fields (ShootProp keeps them round a shot), a
// single-use gun's (its own magazines) here, one gun's.
.float vr_ag_pocket_wid;   // the single-use gun whose magazines it carries (0: none)
.float vr_ag_pocket_n;     // ... their rounds

void(entity g) VR_AG_PocketClear =
{
    g.ammo_shells = 0;
    g.ammo_nails = 0;
    g.ammo_rockets = 0;
    g.ammo_cells = 0;
    g.ammo_lava_nails = 0;
    g.ammo_multi_rockets = 0;
    g.ammo_plasma = 0;
    g.vr_ag_pocket_wid = 0;
    g.vr_ag_pocket_n = 0;
};

// `n` spare rounds of `wid` (flags `flags`) into its pockets.
void(entity g, float wid, float flags, float n) VR_AG_Pocket =
{
    if(wid <= 0 || n <= 0)
    {
        return;
    }
    float aid = WeaponIdToAmmoId(wid, flags);
    if(aid != AID_NONE)
    {
        VR_AmmoUtil_AddAmmoCountByAmmoId(g, aid, n);
        return;
    }
    if(g.vr_ag_pocket_wid != wid)
    {
        g.vr_ag_pocket_n = 0; // (another single-use gun's magazines: no use to it now, its backpack has none of them)
    }
    g.vr_ag_pocket_wid = wid;
    g.vr_ag_pocket_n = g.vr_ag_pocket_n + n;
};

// Its spare rounds for `wid` (flags `flags`) out of its pockets, as it takes that weapon up (0: none of them).
float(entity g, float wid, float flags) VR_AG_Unpocket =
{
    if(wid <= 0)
    {
        return 0;
    }
    float aid = WeaponIdToAmmoId(wid, flags);
    float n = 0;
    if(aid != AID_NONE)
    {
        n = VR_AmmoUtil_GetAmmoCountByAmmoId(g, aid);
        VR_AmmoUtil_SetAmmoCountByAmmoId(g, aid, 0);
        return max(0, n);
    }
    if(g.vr_ag_pocket_wid == wid && VR_AG_ClipSize(wid) > 0)
    {
        n = g.vr_ag_pocket_n;
        g.vr_ag_pocket_wid = 0;
        g.vr_ag_pocket_n = 0;
    }
    return n;
};

// Its weapon leaves its hand as a weapon lying about (its record and magazine with it), flying at `vel`; world if it
// held none. Its spare rounds for it go into its pockets.
entity(entity g, vector vel) VR_AG_DropHeld =
{
    entity p = g.vr_ag_prop;
    if(!p)
    {
        return world;
    }
    if(g.vr_ag_tug_pl)
    {
        // (A tug over it still on: over now, lost. Left on, its run went back to the tug's hold every frame with no
        // weapon: frozen, unarmed. A tug won ends it first: VR_AG_Yank.)
        VR_AG_TugEnd(g, FALSE, "disarmed otherwise");
    }
    g.vr_ag_took_at = 0;
    VR_ADGun_MonsterStreamEnd(g, p.weapon, TRUE); // (its flamethrower's loop: never on after it)
    entity inst = p.weaponinst;
    if(p.weapon == WID_CHAINSAW)
    {
        sound(p, CHAN_WEAPON, "misc/null.wav", 1, ATTN_NORM); // (its engine's loop: vr_armedgrunt_pose.qc VR_AGP_Saw)
    }
    entity w = CreateThrownWeapon(g, p.weapon, p.weaponflags, inst, p.origin, p.v_angle, vel, randomVec(90),
        FALSE /* disappear on hit */, FALSE /* deal damage */, WeaponIdToThrowDamage(p.weapon));
    float fought = g.vr_ag_saw_fought || (g.health > 0 && g.enemy && (g.enemy.flags & FL_CLIENT)); // (alive: after you now)
    if(w && p.weapon == WID_CHAINSAW && inst.wi_clip > 0 && fought && cvar("vr_agrunt_saw_drop_running"))
    {
        // (It was fighting with it, its engine running: dropped running (dead, disarmed, a tug lost), as yours runs on
        // as it lies, vr_chainsaw_drop_run; taken up meanwhile, no cord to pull. His note vrragtriggers_2026-10-27_01-35-21.)
        VR_Saw_PropRuns(w);
    }
    VR_AG_Log(sprintf("%s dropped (%g in its clip, %g in reserve)%s", p.netname, inst.wi_clip, g.vr_ag_reserve,
        (w.weaponflags & QVR_WPNFLAG_SAW_RUNNING) ? ", running" : ""));
    VR_AG_Pocket(g, p.weapon, p.weaponflags, g.vr_ag_reserve);
    g.vr_ag_reserve = 0;
    p.weaponinst = world;
    remove(p);
    g.vr_ag_prop = world;
    g.vr_ag_wid = -1;
    g.vr_ag_wflags = 0;
    g.vr_ag_aim = '0 0 0';
    g.vr_ag_seek_next = time + 0.5 + random() * 0.5;
    VR_AG_Settle(g); // (two held: the other to the first slot)
    return w;
};

// Disarmed: the weapon knocked out of its hand (`dir`: the blow's way).
void(entity g, vector dir, float speed, string why) VR_AG_Disarm =
{
    if(!g.vr_ag_prop || VR_AG_Still(g))
    {
        return; // (held still: the Grip Editor, the Pose Studio, where hands reach into its weapons)
    }
    dir_z = 0;
    vector vel = normalize(dir) * (speed * (0.8 + random() * 0.4)) + '0 0 1' * (speed * 0.9);
    float wid = g.vr_ag_wid;
    VR_AG_DropHeld(g, vel);
    g.vr_ag_disarms = g.vr_ag_disarms + 1; // (its spare rounds: in its pockets, VR_AG_DropHeld)
    VR_AG_DisarmFeedback(g, wid); // (its clatter, its grunt: vr_armedgrunt_play.qc)
    VR_AG_Log(sprintf("disarmed (%s)", why));
};

// Its weapon yours, in `pl`'s hand `h` (a grab: at once, or a tug won).
void(entity g, entity pl, float h) VR_AG_Yank =
{
    float wid = g.vr_ag_wid;
    if(g.vr_ag_prop)
    {
        setorigin(g.vr_ag_prop, VRGetEntHandPos(pl, h)); // (into your hand: a tug's pulled it a little behind it)
    }
    entity w = VR_AG_DropHeld(g, '0 0 0');
    if(!w)
    {
        return;
    }
    VR_AG_DisarmFeedback(g, wid);
    g.vr_ag_disarms = g.vr_ag_disarms + 1;
    w.handtouch_ent = pl;
    w.handtouch_hand = h;
    float catching = forcegrabCatching;
    forcegrabCatching = TRUE; // (caught however long ago the grab began: a tug won)
    wpnthrow_handtouch_ent(w);
    forcegrabCatching = catching;
    VR_AG_Log(sprintf("yanked out of its hand by %s (hand %g)", pl.netname, h));
};

// Your hand grabbing its weapon (the grab started now): a tug-of-war, or it is yours (VR_AG_GrabBy).
void() VR_AG_PropHandTouch =
{
    entity pl = self.handtouch_ent;
    if(!cvar_hget(cvarh_vr_agrunt_disarm_yank) || !(pl.flags & FL_CLIENT) || pl.health <= 0)
    {
        return;
    }
    float h = VRGetHandtouchHand(self);
    if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
    {
        VR_AG_Log(sprintf("a hand (%g) on its weapon: empty %g, grabbing %g", h, VRIsHandEmpty(pl, h),
            VR_HandGrabUtil_StartedHandGrabbing(pl, h)));
    }
    if(!VRIsHandEmpty(pl, h) || !VR_HandGrabUtil_StartedHandGrabbing(pl, h))
    {
        return;
    }
    // (A deliberate grab: the fist on the weapon itself (the engine's handOn), and not a punch or a swipe through it.)
    // Its speed: against you (a hand still as the blade comes into it) or against the weapon (a hand going with a swing
    // to catch it), the lesser; in real time (bullet time's hands are the game's, sped up: they read 3 times as fast at
    // 0.3, and a careful catch in it was refused as a punch).
    float most = cvar_hget(cvarh_vr_agrunt_grab_speed);
    vector hv = VRGetEntHandVel(pl, h);
    vector wv = '0 0 0';
    if(self.vr_ag_was_n >= 1 && frametime > 0 && vlen(self.origin - self.vr_ag_was_o1) < 32)
    {
        wv = (self.origin - self.vr_ag_was_o1) * (1 / frametime);
    }
    float sp = min(vlen(hv - pl.velocity), vlen(hv - wv)) * (vr_hand_time_scale > 0 ? vr_hand_time_scale : 1) / VRMetersToUnits();
    if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
    {
        VR_AG_Log(sprintf("a hand (%g) takes hold of its weapon at %.2f m/s, %.1f units from its grip", h, sp,
            vlen(VRGetEntHandPos(pl, h) - self.origin)));
    }
    if(most > 0 && sp > most)
    {
        vr_ag_grab_fast = vr_ag_grab_fast + 1;
        if(cvar_hget(cvarh_vr_agrunt_debug))
        {
            VR_AG_Log(sprintf("a hand (%g) through its weapon at %.1f m/s: too fast to take hold (vr_agrunt_grab_speed %g)", h,
                sp, most));
        }
        return;
    }
    entity g = self.owner;
    if(g.vr_ag_prop2 == self)
    {
        if(g.vr_ag_tug_pl)
        {
            return; // (a tug over its other weapon: one at a time)
        }
        VR_AG_Swap(g); // (its left hand's weapon grabbed: the first slot's, for the tug and the yank)
    }
    VR_AG_GrabBy(g, pl, h);
};

// ----------------------------------------------------------------------------
// Firing and swinging: your weapons' own functions, `self` the grunt
// ----------------------------------------------------------------------------

// Dual-wielded guns' penalties (vr_agrunt_dual_penalty; ROUND22.md "Armed monsters: dual-wielded guns"): as yours in one
// hand. A shot's spread and aim error lose the two-hand spread reduction (times 1 / vr_2h_spread_reduction: x2), a heavy
// gun's aim is kicked up a shot (its heavy recoil: weaponheft(model, 1), deg/s, over VR_AG_KICK_TIME) and comes back over
// vr_heavy_recoil_time (a later shot from that gun before then: still up by what is left).
float VR_AG_KICK_TIME = 0.1;    // s: a kick's speed (deg/s) to the aim it throws up (the wrist's give, about)
.float vr_ag_hk;                // a held gun's prop: its aim's kick up (degrees) at its last shot,
.float vr_ag_hk_at;             // ... then
float vr_ag_aim_mult;           // VR_AG_AimDir's error times 1 + this (the shot under way's dual penalty)
float vr_ag_last_aimdev;        // the last shot's aim off its target (degrees; the tests)
float vr_ag_last_kick;          // ... its kick up then

// The penalties' share now (0: one gun, or vr_agrunt_dual_penalty 0).
float(entity g) VR_AG_DualPenalty =
{
    return VR_AG_DualGuns(g) ? max(0, cvar("vr_agrunt_dual_penalty")) : 0;
};

// What is left of the gun `p`'s kick now (degrees up).
float(entity p) VR_AG_KickLeft =
{
    float t = max(0.01, cvar("vr_heavy_recoil_time"));
    return p.vr_ag_hk * max(0, 1 - (time - p.vr_ag_hk_at) / t);
};

// Where it aims: at its target's middle, a little behind it as it moves (the grunt's), up to vr_agrunt_aim_error off.
// A direction's angles as vectoangles gives them (pitch up), exact: the builtin truncates its yaw and pitch to whole
// degrees (PF_vectoangles), up to a degree off: its shots a few units to one side of you at 300, 15 at 1000 (test 49).
vector(vector d) VR_AG_Angles =
{
    vector a;
    a_x = atan2(d_z, sqrt(d_x * d_x + d_y * d_y)) * 57.29578;
    a_y = atan2(d_y, d_x) * 57.29578;
    a_z = 0;
    return a;
};

vector() VR_AG_AimPoint =
{
    entity t = self.enemy;
    vector at = VR_AI_Origin(t) - t.velocity * 0.15;
    if(t.flags & FL_CLIENT)
    {
        at_z = at_z + 6; // (a player's chest, not his origin's waist)
    }
    return at;
};
vector(vector from) VR_AG_AimDir =
{
    vector dir = normalize(VR_AG_AimPoint() - from);
    float err = cvar_hget(cvarh_vr_agrunt_aim_error) * (1 + vr_ag_aim_mult);
    if(err > 0)
    {
        vector a = VR_AG_Angles(dir);
        a_x = -a_x + crandom() * err; // (a view's pitch for makevectors)
        a_y = a_y + crandom() * err;
        makevectors(a);
        dir = v_forward;
    }
    return dir;
};

float vr_ag_drawn_shots; // (vr_agrunt_shot_drawn: shots turned onto the drawn line, their degrees, all shots: test 46)
float vr_ag_drawn_deg;
float vr_ag_all_shots;
// Its hand fields as the player's would be holding the weapon `p` aimed along `dir` (VRGetHandPos, VRGetMuzzlePos,
// VRGetWeaponFireRot, VRGetWeapon, VRGetEntWeaponInst read them).
float vr_ag_shot_on;       // VR_AG_ShootProp's shot under way: its straight line (no error) from the grip,
vector vr_ag_shot_straight;
float vr_ag_miss_n;        // shots measured (the tests: test 49), each its line's miss at its target's point:
float vr_ag_miss_d;        // ... the muzzle to that point,
float vr_ag_miss_raw;      // ... to the right of it (units; - left) as aimed from the grip (before vr_agrunt_aim_converge),
float vr_ag_miss_lat;      // ... as fired,
float vr_ag_miss_up;       // ... above it as fired

// The line from `m` along `dir`: its miss at `at`, to its right (vr_ag_miss_up: above).
float(vector m, vector dir, vector at) VR_AG_MissRight =
{
    vector off = m + dir * ((at - m) * dir) - at;
    vector right;
    right_x = dir_y; // (dir x up)
    right_y = -dir_x;
    right_z = 0;
    right = normalize(right);
    vr_ag_miss_up = off_z;
    return off * right;
};

// (an angle's difference, -180..180)
float(float a) VR_AG_Wrap180 = { return a - 360 * floor((a + 180) / 360); };

void(entity p, vector dir) VR_AG_SetHand =
{
    vector dir0 = dir; // (as aimed: from its grip, its error and kick in it)
    self.vr_ag_aim = dir;
    VR_AG_PlaceProp(self, p);
    self.handpos = p.origin;
    vector a = VR_AG_Angles(dir);
    a_x = -a_x; // (a view's pitch, as the hands')
    self.handrot = a;
    self.shotrot = '0 0 0';
    self.handvel = '0 0 0';
    vector m = weaponmuzzle(p);
    if(m == p.origin)
    {
        m = p.origin + dir * 16;
    }
    if(cvar_hget(cvarh_vr_agrunt_shot_drawn))
    {
        // (vr_agrunt_shot_drawn: the gun as the arms drew it, a few degrees off this line where an arm can't turn its
        // wrist so far or its hand eases into a jump: its shots leave its drawn muzzle along its drawn line, as you see
        // it; up to 30 degrees, a listen server's own view)
        vector turn = weapondrawnpose(p, 0, 21);
        float ang = vlen(turn);
        if(ang > 0.01 && ang < 30)
        {
            vector ax = turn * (1 / ang);
            vector off = weapondrawnpose(p, 0, 22);
            dir = normalize(VR_AG_Rotate(dir, ax, ang));
            m = p.origin + off + VR_AG_Rotate(m - p.origin, ax, ang);
            a = VR_AG_Angles(dir);
            a_x = -a_x;
            self.handrot = a;
            vr_ag_drawn_shots = vr_ag_drawn_shots + 1;
            vr_ag_drawn_deg = vr_ag_drawn_deg + ang;
        }
        vr_ag_all_shots = vr_ag_all_shots + 1;
    }
    if(vr_ag_shot_on && self.enemy)
    {
        // Converging on its target (vr_agrunt_aim_converge): aimed from its grip, its shot left the muzzle beside that
        // line along it (or along the drawn gun's, turned off it): a miss to one side at any distance. Now its line
        // from the muzzle it fires from to its target's point, turned by its aim's error and kick as before (unbiased).
        vector at = VR_AG_AimPoint();
        vector old = vectoangles(dir); // (the old way, measured: this line, its angles in whole degrees)
        old_x = -old_x;
        makevectors(old);
        vr_ag_miss_raw = VR_AG_MissRight(m, v_forward, at);
        if(cvar_hget(cvarh_vr_agrunt_aim_converge) && vlen(at - m) > 8)
        {
            vector e = VR_AG_Angles(dir0) - VR_AG_Angles(vr_ag_shot_straight);
            vector c = VR_AG_Angles(normalize(at - m));
            c_x = -(c_x + VR_AG_Wrap180(e_x)); // (a view's pitch for makevectors)
            c_y = c_y + VR_AG_Wrap180(e_y);
            c_z = 0;
            makevectors(c);
            dir = v_forward;
            self.handrot = c;
        }
        makevectors(self.handrot); // (as fired: its hand's angles, the shot's line)
        vr_ag_miss_lat = VR_AG_MissRight(m, v_forward, at);
        vr_ag_miss_d = vlen(at - m);
        vr_ag_miss_n = vr_ag_miss_n + 1;
    }
    self.muzzlepos = m;
    self.weapon = p.weapon;
    self.weaponflags = p.weaponflags;
    self.weaponinst = p.weaponinst;
    makevectors(self.handrot);
};

// ----------------------------------------------------------------------------
// Its friends not shot (vr_agrunt_friendly_care): a friend in its line of fire, it holds and sidesteps
// ----------------------------------------------------------------------------

.float vr_ag_ff_until;    // stepping aside (a friend in its line of fire) until then (VR_AG_Run)
.float vr_ag_ff_since;    // held for friends since then (0: its line clear): vr_agrunt_friendly_giveup
.float vr_ag_ff_last;     // ... the last time it held
float vr_ag_ff_held;      // armed humanoids' shots and blows held for friends (the tests)
float vr_ag_ff_hits;      // ... their hits on other monsters (VR_AG_DamageScale: the tests)
float vr_ag_ff_dmg;       // ... those hits' damage

// The half-angle (degrees) of the lines it checks about its line of fire: a shotgun's spread, a burst's error, a swing's
// arc.
float(float wid) VR_AG_FriendSpread =
{
    if(wid == WID_SUPER_SHOTGUN) { return 6; }
    if(wid == WID_SHOTGUN) { return 3.5; }
    if(VR_AG_Class(wid) == VR_AG_CLASS_MELEE || VR_AG_Class(wid) == VR_AG_CLASS_FISTS) { return 15; }
    return 2;
};

// A friend in its line of fire: a living monster (not its enemy, not fighting it) that the line from `from` to its
// enemy meets first, or one of four more lines `deg` degrees about it (left, right, up, down). world: clear.
entity(vector from, float deg) VR_AG_FriendInLine =
{
    entity targ = self.enemy;
    vector to = VR_AI_Origin(targ) + targ.view_ofs * 0.5; // (its chest)
    float dist = vlen(to - from);
    if(dist < 1)
    {
        return world;
    }
    // (A friend against it, its muzzle at or in it: the line from its own middle out to the muzzle first.)
    traceline(self.origin + '0 0 8', from, FALSE, self);
    if(trace_ent && trace_ent != targ && (trace_ent.flags & FL_MONSTER) && trace_ent.health > 0 && trace_ent.enemy != self)
    {
        return trace_ent;
    }
    vector dir = (to - from) * (1 / dist);
    vector side;
    side_x = dir_y;
    side_y = -dir_x;
    side_z = 0;
    side = vlen(side) > 0.01 ? normalize(side) : '1 0 0';
    float i;
    for(i = 0; i < (deg > 0 ? 5 : 1); i++)
    {
        vector r = dir;
        if(i == 1) { r = VR_AG_Rotate(dir, '0 0 1', deg); }
        else if(i == 2) { r = VR_AG_Rotate(dir, '0 0 1', -deg); }
        else if(i == 3) { r = VR_AG_Rotate(dir, side, deg); }
        else if(i == 4) { r = VR_AG_Rotate(dir, side, -deg); }
        traceline(from, from + r * dist, self.vr_ai_gate > 0 ? MOVE_PORTALS : FALSE, self);
        entity e = trace_ent;
        if(e && e != targ && e != self && (e.flags & FL_MONSTER) && e.health > 0 && e.enemy != self)
        {
            return e;
        }
    }
    return world;
};

// Before it fires (each shot) or swings, from `from`: TRUE when it holds for a friend in its line of fire (by
// vr_agrunt_friendly_care's chance) and steps aside, away from it, for a clear line (VR_AG_Run); held so for
// vr_agrunt_friendly_giveup, it fires anyway (once, then it is careful again).
float(vector from, float deg) VR_AG_FriendHold =
{
    float care = cvar_hget(cvarh_vr_agrunt_friendly_care);
    if(care <= 0 || !self.enemy)
    {
        return FALSE;
    }
    entity f = VR_AG_FriendInLine(from, deg);
    if(!f)
    {
        self.vr_ag_ff_since = 0;
        return FALSE;
    }
    if(self.vr_ag_ff_since <= 0 || time - self.vr_ag_ff_last > 1.5)
    {
        self.vr_ag_ff_since = time;
    }
    self.vr_ag_ff_last = time;
    float giveup = cvar_hget(cvarh_vr_agrunt_friendly_giveup);
    if(giveup > 0 && time - self.vr_ag_ff_since >= giveup)
    {
        VR_AG_Log(sprintf("its %s in the way %.1f s: fires anyway", f.classname, time - self.vr_ag_ff_since));
        self.vr_ag_ff_since = time;
        return FALSE;
    }
    if(random() >= care)
    {
        return FALSE;
    }
    vr_ag_ff_held = vr_ag_ff_held + 1;
    // Its step: away from the friend's side of its line (on the line itself: either way).
    vector toEnemy = self.enemy.origin - self.origin;
    makevectors([0, vectoyaw(toEnemy), 0]);
    float off = (f.origin - self.origin) * v_right;
    if(fabs(off) > 4)
    {
        self.lefty = off > 0; // (lefty: VR_AG_Run's step to its left)
    }
    if(time >= self.vr_ag_ff_until)
    {
        VR_AG_Log(sprintf("holds: its %s in the way, %.0f off its line; stepping %s", f.classname, off,
            self.lefty ? "left" : "right"));
    }
    self.vr_ag_ff_until = time + 0.4 + random() * 0.4;
    return TRUE;
};

// ----------------------------------------------------------------------------
// Its launchers fired safely (vr_agrunt_blast_safe): the round's burst predicted, kept clear of itself
// ----------------------------------------------------------------------------

.float vr_ag_dodge_until; // stepping aside (a launcher's shot not clear) until then
.float vr_ag_held;        // the launcher shots it held back (the tests)
float vr_ag_self_kills;   // armed grunts killed by their own weapon (the tests)
float vr_ag_self_dmg;     // ... the damage they dealt themselves
float vr_ag_held_fire;    // ... the launcher shots held back
vector vr_ag_blast_at;    // VR_AG_BlastPoint's last

// Where a round of `wid` fired from `from` along `dir` (unit) bursts, as the launchers' code flies it: a rocket at what
// its line meets; a grenade along its arc (600 ahead, 200 up, gravity; bounced as MOVETYPE_BOUNCE, 1.5 off a surface,
// resting on a floor once slow) at its fuse or at the first thing it may hurt; a mine where it first lands (it sits there
// and blows at anything near).
vector(float wid, vector from, vector dir) VR_AG_BlastPoint =
{
    if(wid == WID_ROCKET_LAUNCHER || wid == WID_ADROCKET)
    {
        traceline(from, from + dir * 4096, FALSE, self);
        return trace_endpos;
    }
    vector a = VR_AG_Angles(dir);
    a_x = -a_x;
    makevectors(a);
    vector vel = v_forward * 600 + v_up * 200;
    vector pos = from;
    float g = cvar("sv_gravity");
    float mine = (wid == WID_PROXIMITY_GUN);
    float dt = 0.05;
    float t;
    for(t = 0; t < 2.5; t = t + dt)
    {
        vector next = pos + vel * dt;
        vel_z = vel_z - g * dt;
        // (a box about it: a near miss of its target's body, which may be bigger than its box, bursts there too)
        tracebox(pos, '-10 -10 -10', '10 10 10', next, FALSE, self);
        if(trace_startsolid)
        {
            traceline(pos, next, FALSE, self);
        }
        if(trace_fraction == 1)
        {
            pos = next;
            continue;
        }
        pos = trace_endpos + trace_plane_normal * 0.5;
        if(mine || (trace_ent != world && trace_ent.takedamage == DAMAGE_AIM))
        {
            return trace_endpos; // (a mine lands; a grenade bursts on what it may hurt)
        }
        vel = vel - trace_plane_normal * ((vel * trace_plane_normal) * 1.5);
        if(trace_plane_normal_z > 0.7 && vel_z < 60)
        {
            return pos; // (at rest on the floor till its fuse)
        }
    }
    return pos;
};

// The least distance from itself its round of `wid` may burst at: a blast reaches 160 units (T_RadiusDamage's 120 + 40),
// and a margin (a mine's more: it blows at anything within 140 of it, the grunt too when it comes back by).
float(float wid) VR_AG_SafeDist =
{
    if(wid == WID_PROXIMITY_GUN) { return 230; }
    return 180;
};

// Whether its launcher's round fired along `dir` from the held weapon `p` would burst far enough from it (TRUE for any
// other weapon, or vr_agrunt_blast_safe 0).
float(entity p, vector dir) VR_AG_BlastSafe =
{
    float wid = p.weapon;
    if(VR_AG_Class(wid) != VR_AG_CLASS_EXPLOSIVE || !cvar_hget(cvarh_vr_agrunt_blast_safe))
    {
        return TRUE;
    }
    vector from = weaponmuzzle(p);
    vr_ag_blast_at = VR_AG_BlastPoint(wid, from, dir);
    return vlen(vr_ag_blast_at - self.origin) >= VR_AG_SafeDist(wid);
};

// Its own (or any) proximity mine near it (within 200 units, in sight), to keep away from (world: none).
entity() VR_AG_NearMine =
{
    entity e = findradius(self.origin, 200);
    while(e)
    {
        if(e.classname == "proximity_grenade")
        {
            traceline(self.origin, e.origin, TRUE, self);
            if(trace_fraction == 1)
            {
                return e;
            }
        }
        e = e.chain;
    }
    return world;
};

// One shot (or round, or nail, or bolt) of the gun `p` it holds (either hand's), from its clip; two guns held, with the
// dual penalties.
void(entity p) VR_AG_ShootProp =
{
    if(!p || !self.enemy)
    {
        return;
    }
    entity inst = p.weaponinst;
    if(p.vr_ag_native)
    {
        VR_AGR_NativeShot(p); // (a replacement's own gun: its monster's own shot, never spent: vr_armedrepl.qc)
        return;
    }
    if(inst.wi_clip < 1)
    {
        return;
    }
    float wid = p.weapon;
    float pen = VR_AG_DualPenalty(self);
    vr_ag_aim_mult = pen * (1 / max(0.05, cvar_hget(cvarh_vr_2h_spread_reduction)) - 1);
    vector dir = VR_AG_AimDir(p.origin);
    vr_ag_aim_mult = -1; // (no error: the straight line, the tests' measure)
    vector straight = VR_AG_AimDir(p.origin);
    vr_ag_aim_mult = 0;
    float kick = pen > 0 ? VR_AG_KickLeft(p) : 0;
    if(kick > 0)
    {
        // Its aim still up from its last shot (a heavy gun in one hand): turned up about its own right.
        vector side;
        side_x = dir_y;
        side_y = -dir_x;
        side_z = 0;
        side = normalize(side);
        if(side != '0 0 0')
        {
            dir = VR_AG_Rotate(dir, side, kick);
        }
    }
    vr_ag_last_kick = kick;
    vr_ag_last_aimdev = atan2(vlen(dir - straight * (dir * straight)), dir * straight) * 57.29578;
    vr_ag_shot_on = 1;
    vr_ag_shot_straight = straight;
    VR_AG_SetHand(p, dir);
    vr_ag_shot_on = 0;
    if(!VR_AG_BlastSafe(p, dir))
    {
        // Its own blast would reach it (the target came near, a wall ahead): held back, a step aside.
        self.vr_ag_held = self.vr_ag_held + 1;
        vr_ag_held_fire = vr_ag_held_fire + 1;
        self.vr_ag_dodge_until = time + 0.6 + random() * 0.6;
        self.lefty = !self.lefty;
        self.weaponinst = world;
        VR_AG_Log(sprintf("%s held back: it would burst %.0f from it", p.netname, vlen(vr_ag_blast_at - self.origin)));
        return;
    }
    if(VR_AG_FriendHold(self.muzzlepos, VR_AG_FriendSpread(wid))) // (its muzzle: VR_AG_SetHand)
    {
        self.weaponinst = world; // (a friend stepped into its line: this shot held, a step aside)
        return;
    }
    makevectors(self.handrot); // (VR_AG_BlastSafe's arc used the globals)
    float aid = WeaponIdToAmmoId(wid, p.weaponflags);
    float single = VR_AG_SingleUse(wid) || aid == AID_NONE;
    float pocket = 0; // (its .ammo_* fields hold the spare rounds of weapons no longer in its hands: VR_AG_Pocket)
    if(!single)
    {
        pocket = VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid);
        VR_AmmoUtil_SetAmmoCountByAmmoId(self, aid, inst.wi_clip); // (the fire code spends its clip as your ammo)
    }

    float h = cVR_MainHand;
    float rounds = 1;
    self.button0 = 1; // (its trigger held: the laser cannon fires only so)
    vr_fire_spread_extra = pen * (1 / max(0.05, cvar_hget(cvarh_vr_2h_spread_reduction)) - 1); // (FireBullets: x2 at 1)
    if(wid == WID_SHOTGUN)              { W_FireShotgun(h); }
    else if(wid == WID_SUPER_SHOTGUN && AD_Widowmaker(inst)) { AD_FireWidowmaker(h); } // (AD's upgrades: vr_ad_bridge.qc)
    else if(wid == WID_LIGHTNING && AD_PlasmaGun(h, p.weaponflags)) { AD_FirePlasma(h); }
    else if(wid == WID_ADROCKET)        { VR_ADGun_FireRocket(h); } // (AD's soldiers' guns: vr_ad_enemyguns.qc)
    else if(wid == WID_ADGRENADE)       { VR_ADGun_FireGrenade(h); }
    else if(wid == WID_ADDEFENDER)      { VR_ADGun_FireDefender(h); }
    else if(wid == WID_ADCROSSBOW)      { VR_ADGun_FireCrossbow(h); }
    else if(wid == WID_ADPLASMA || wid == WID_ADCENTURION) { VR_ADGun_FirePlasma(h, wid); }
    else if(VR_AG_Stream(wid))
    {
        // Two flames a fire frame (the player's stream is one a tick), from its fuel.
        rounds = min(2, inst.wi_clip);
        VR_ADGun_FireFlame(h, wid);
        if(rounds > 1)
        {
            VR_ADGun_FireFlame(h, wid);
        }
    }
    else if(wid == WID_SUPER_SHOTGUN)   { W_FireSuperShotgun(h); }
    else if(wid == WID_NAILGUN)         { W_FireSpikes(h, (self.vr_ag_shots & 1) ? -2.35 : 2.35); }
    else if(wid == WID_SUPER_NAILGUN)   { W_FireSuperSpikes(h); }
    else if(wid == WID_NAILPISTOL)      { W_FireNailPistol(h); } // (vr_nailpistol.qc)
    else if(wid == WID_FLAREGUN)        { W_FireFlareGun(h); } // (vr_flaregun.qc: the clip below)
    else if(wid == WID_REVOLVER)        { W_FireRevolver(h); } // (vr_revolver.qc: a slug a shot)
    else if(wid == WID_GRENADE_LAUNCHER)
    {
        if(p.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO) { W_FireMultiGrenade(h); } else { W_FireGrenade(h); }
    }
    else if(wid == WID_ROCKET_LAUNCHER)
    {
        if(p.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO) { W_FireMultiRocket(h); } else { W_FireRocket(h); }
    }
    else if(wid == WID_PROXIMITY_GUN)   { W_FireProximityGrenade(h); }
    else if(wid == WID_LASER_CANNON)    { HIP_FireLaser(h, self.vr_ag_shots & 1); }
    else if(wid == WID_LIGHTNING)
    {
        if(self.waterlevel > 1)
        {
            self.button0 = 0;
            self.weaponinst = world;
            vr_fire_spread_extra = 0;
            VR_AmmoUtil_SetAmmoCountByAmmoId(self, aid, pocket); // (its pocket's cells back)
            return; // (it would discharge: never under water)
        }
        W_FireLightning(h);
        // The beam: the client draws yours from your gun; a monster's is Quake's temporary beam.
        traceline(self.muzzlepos, self.muzzlepos + dir * 600, TRUE, self);
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
        WriteEntity(MSG_BROADCAST, p);
        WriteByte(MSG_BROADCAST, 0); // (the disambiguator: no hand's)
        WriteVec3(MSG_BROADCAST, self.muzzlepos);
        WriteVec3(MSG_BROADCAST, trace_endpos);
    }
    else if(wid == WID_GRUNTGUN)
    {
        if(cvar_hget(cvarh_vr_grunt_burst)) { VR_FireBurstRound(h); } else { VR_FireGruntGun(h); }
    }
    else if(wid == WID_ENFRIFLE)        { VR_FireEnforcerRifle(h); }
    self.button0 = 0;
    self.weaponinst = world; // (the record is its weapon's: nothing else may take the grunt for its holder)
    vr_fire_spread_extra = 0;
    if(pen > 0)
    {
        // A heavy gun in one hand: its aim kicked up (what is left of the last kick and this one's).
        float up = weaponheft(WeaponIdToModel(wid, p.weaponflags), 1) * VR_AG_KICK_TIME * pen;
        p.vr_ag_hk = VR_AG_KickLeft(p) + up;
        p.vr_ag_hk_at = time;
    }
    p.vr_ag_kick_at = time; // (its own kick drawn: two guns fire in turn)

    if(VR_AG_Class(wid) != VR_AG_CLASS_MELEE && !VR_AG_Stream(wid))
    {
        weaponfired(p, -1); // its muzzle flash on the held weapon, as yours (the engine's vr_weaponfx.cpp)
    }
    if(single)
    {
        inst.wi_clip = max(0, inst.wi_clip - rounds);
    }
    else
    {
        // (Only a gun of a real ammo kind: a single-use one's was read as AID_NONE's, an assertion and an empty clip
        // after its first shot: the enforcer's rifle's one shot, the burst rifle's one round.)
        inst.wi_clip = max(0, VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid));
        VR_AmmoUtil_SetAmmoCountByAmmoId(self, aid, pocket);
    }
    if(wid == WID_FLAREGUN)
    {
        p.skin = VR_FlareGun_Skin(inst.wi_clip); // (the barrel just fired sooted)
    }
    self.vr_ag_shots = self.vr_ag_shots + 1;
    self.vr_ag_total_shots = self.vr_ag_total_shots + 1;
    p.vr_ag_total_shots = p.vr_ag_total_shots + 1; // (each gun's: the tests)
    vr_ag_shots_fired = vr_ag_shots_fired + 1;
    self.vr_ag_kick_at = time;
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.show_hostile = time + 1; // wakes monsters up, as your shots do
    VR_AG_Log(sprintf("%s fired at %s (%g left in the clip), muzzle %v", p.netname, self.enemy.classname, inst.wi_clip,
        self.muzzlepos));
    if(pen > 0)
    {
        VR_AG_Log(sprintf("  dual: %s hand, aim %.2f deg off, kicked up %.2f (now %.2f)", p.vr_ag_left ? "left" : "right",
            vr_ag_last_aimdev, vr_ag_last_kick, p.vr_ag_hk));
    }
};

// Its first slot's gun fired (the single-gun callers).
void() VR_AG_Shoot = { VR_AG_ShootProp(self.vr_ag_prop); };

// A melee blow (its weapon's, or a punch) from its hand towards its target.
void() VR_AG_Strike =
{
    float h = cVR_MainHand;
    entity p = self.vr_ag_prop;
    vector from = p ? p.origin : self.origin + '0 0 16';
    if(!p)
    {
        // A punch from its right hand.
        VR_AM_Row(self.vr_ag_kind);
        from = (modelvertex(self, vr_am_hand_x) + modelvertex(self, vr_am_hand_y)) * 0.5;
        if(trace_fraction == 0) { from = self.origin + '0 0 16'; }
    }
    vector dir = normalize(VR_AI_Origin(self.enemy) + '0 0 8' - from);
    if(p)
    {
        VR_AG_SetHand(p, dir);
    }
    else
    {
        self.handpos = from;
        vector a = VR_AG_Angles(dir);
        a_x = -a_x;
        self.handrot = a;
        self.shotrot = '0 0 0';
        self.handvel = '0 0 0';
        self.muzzlepos = from + dir * 4;
        self.weapon = WID_FIST;
        self.weaponflags = 0;
        self.weaponinst = world;
        makevectors(a);
    }
    VRSetMeleeHitSoundPlayed(h, FALSE);
    float wid = p ? p.weapon : WID_FIST;
    float cells = self.ammo_cells;
    self.ammo_cells = 0; // (Mjolnir as the hammer: no lightning)
    if(wid == WID_CHAINSAW)            { W_ChainsawMelee(h); }
    else if(wid == WID_MJOLNIR)        { HIP_FireMjolnir(h); }
    else if(wid == WID_FIST)           { W_FistMelee(h); }
    else if(VR_AG_Class(wid) == VR_AG_CLASS_MELEE) { W_FireAxe(h); }
    else                               { W_GunMelee(h); } // (a gun's butt: an explosive one at arm's length)
    self.ammo_cells = cells;
    self.weaponinst = world;
    self.vr_ag_aim = '0 0 0';
    self.vr_ag_strikes = self.vr_ag_strikes + 1;
    VR_AG_Log(sprintf("struck at %s with %s", self.enemy.classname, p ? p.netname : "its fists"));
};

// ----------------------------------------------------------------------------
// Animations: its own run, attacks, reload and pickup (its row's frames: vr_armedmon.qc VR_AM_Anim); stand, walk, pain,
// death are its monster's own
// ----------------------------------------------------------------------------

void(float d) VR_AG_Run;
void() vr_ag_reload1;
void() vr_ag_take1;

void() vr_ag_run1 = { VR_AM_Anim(VR_AM_A_RUN, 0, vr_ag_run2); VR_AG_Run(VR_AM_RunStep(0)); };
void() vr_ag_run2 = { VR_AM_Anim(VR_AM_A_RUN, 1, vr_ag_run3); VR_AG_Run(VR_AM_RunStep(1)); };
void() vr_ag_run3 = { VR_AM_Anim(VR_AM_A_RUN, 2, vr_ag_run4); VR_AG_Run(VR_AM_RunStep(2)); };
void() vr_ag_run4 = { VR_AM_Anim(VR_AM_A_RUN, 3, vr_ag_run5); VR_AG_Run(VR_AM_RunStep(3)); };
void() vr_ag_run5 = { VR_AM_Anim(VR_AM_A_RUN, 4, vr_ag_run6); VR_AG_Run(VR_AM_RunStep(4)); };
void() vr_ag_run6 = { VR_AM_Anim(VR_AM_A_RUN, 5, vr_ag_run7); VR_AG_Run(VR_AM_RunStep(5)); };
void() vr_ag_run7 = { VR_AM_Anim(VR_AM_A_RUN, 6, vr_ag_run8); VR_AG_Run(VR_AM_RunStep(6)); };
void() vr_ag_run8 = { VR_AM_Anim(VR_AM_A_RUN, 7, vr_ag_run1); VR_AG_Run(VR_AM_RunStep(7)); };

// The taunt (vr_agrunt_taunt, off by default; the Rig Studio's proof: docs/vr-port/RIGSTUDIO.md): an armed humanoid seeing
// a player for the first time stands and plays its model's "taunt" sequence (quakevr/anims/<model>/taunt.anim.txt,
// vr_rigseq.qc) facing him, its "shout" event its sight sound, then comes for him. No such sequence for its model
// (vr_rigseq_len -1), a pain (its frame changed: "stopped") or 4 seconds: on at once.
.float vr_ag_taunted;     // it has taunted (once a life)
.float vr_ag_taunt_until; // the wait's end at the latest
void() vr_ag_taunt_wait;
void(string seq, string ev, float frame) VR_AG_SeqEvent =
{
    if(ev == "shout")
    {
        sound(self, CHAN_VOICE, self.vr_ag_kind == 1 ? "enforcer/sight1.wav" : "soldier/sight1.wav", 1, ATTN_NORM);
    }
    else if((ev == "end" || ev == "stopped") && self.think == vr_ag_taunt_wait && self.health > 0)
    {
        self.think = vr_ag_run1;
        self.nextthink = time;
    }
};
void() vr_ag_taunt_wait =
{
    if(self.vr_rigseq_len < 0 || time > self.vr_ag_taunt_until || !self.enemy || self.enemy.health <= 0)
    {
        if(self.vr_rigseq_len > 0)
        {
            VR_RigSeq_Stop(self);
        }
        vr_ag_run1();
        return;
    }
    ai_face();
    self.think = vr_ag_taunt_wait;
    self.nextthink = time + 0.1;
};
// Its th_run: the taunt first (once), else its run.
void() vr_ag_hunt =
{
    if(!self.vr_ag_taunted && self.health > 0 && self.enemy && self.enemy.classname == "player" && !VR_AG_Still(self) &&
       cvar("vr_agrunt_taunt"))
    {
        self.vr_ag_taunted = 1;
        self.vr_rigseq_on = VR_AG_SeqEvent;
        VR_RigSeq_Play(self, "taunt");
        self.vr_ag_taunt_until = time + 4;
        self.velocity = '0 0 0';
        self.think = vr_ag_taunt_wait;
        self.nextthink = time + 0.1;
        VR_AG_Log("taunting");
        return;
    }
    vr_ag_run1();
};

// After an attack: a reload if its clip is empty, else on running.
void() VR_AG_AfterAttack =
{
    self.vr_ag_aim = '0 0 0';
    entity p = self.vr_ag_prop;
    VR_ADGun_MonsterStreamEnd(self, self.vr_ag_wid, FALSE); // (a flamethrower's stream ends with its sound: the player's VR_ADGun_Frame)
    float reload = FALSE;
    entity q = self.vr_ag_prop2;
    if(VR_AG_Spent(q))
    {
        // Its second gun (dual wielding) spent: reloaded with the first, or thrown away (it fights on with the other).
        if(self.vr_ag_reserve2 > 0)
        {
            reload = TRUE;
        }
        else
        {
            float sideQ = q.vr_ag_left ? -1 : 1;
            VR_AG_Swap(self);
            makevectors(self.angles);
            VR_AG_DropHeld(self, v_right * (90 * sideQ) + v_forward * 40 + '0 0 120');
            VR_AG_Log("its second gun spent: thrown away");
            p = self.vr_ag_prop;
        }
    }
    if(VR_AG_Spent(p))
    {
        if(self.vr_ag_reserve > 0)
        {
            reload = TRUE;
        }
        else
        {
            // Spent, nothing to reload: thrown away; it fights on with its fists (or its other gun).
            float sideP = p.vr_ag_left ? -1 : 1;
            makevectors(self.angles);
            VR_AG_DropHeld(self, v_right * (90 * sideP) + v_forward * 40 + '0 0 120');
            VR_AG_Log("its weapon spent: thrown away");
            VR_AG_DrawMain(self); // (its flare gun: its own weapon now)
        }
    }
    if(reload)
    {
        vr_ag_reload1();
        return;
    }
    vr_ag_run1();
};

// A fire frame `n` (1..9: shoot1..9) of its attack.
void(float n) VR_AG_AtkFrame =
{
    ai_face();
    entity p = self.vr_ag_prop;
    if(!p || !self.enemy)
    {
        return;
    }
    VR_AG_WalkFire(0.1, TRUE); // (walking as it fires: its fighting distance, vr_agrunt_range_walkfire)
    float c = VR_AG_Class(p.weapon);
    if(n >= 3)
    {
        self.vr_ag_aim = VR_AG_AimDir(p.origin); // (pointing at its target from the frame it raises the gun)
    }
    if(c == VR_AG_CLASS_AUTO && n >= 4 && n <= 8)
    {
        VR_AG_Shoot();
    }
    else if(c == VR_AG_CLASS_BURST && n >= 5 && n <= 7)
    {
        VR_AG_Shoot();
    }
    else if(n == 5 && (c == VR_AG_CLASS_GUN || c == VR_AG_CLASS_EXPLOSIVE))
    {
        VR_AG_Shoot();
    }
    // Two guns: the second fires too, a frame behind the first's (a shot, a round, a burst: in turn; the nail guns' and
    // the thunderbolt's streams: both at once).
    entity q = self.vr_ag_prop2;
    if(q && VR_AG_DualGuns(self))
    {
        float c2 = VR_AG_Class(q.weapon);
        if((c2 == VR_AG_CLASS_AUTO && n >= 4 && n <= 8) || (c2 == VR_AG_CLASS_BURST && n >= 6 && n <= 8) ||
           (n == 7 && (c2 == VR_AG_CLASS_GUN || c2 == VR_AG_CLASS_EXPLOSIVE)))
        {
            VR_AG_ShootProp(q);
        }
    }
};

// A gun of its out of rounds (its clip empty; not a melee weapon).
float(entity p) VR_AG_Spent = { return p && p.weaponinst.wi_clip < 1 && VR_AG_ClipOf(p.weapon, p.weaponinst) > 0; };

// Either gun it holds can fire on (the nail guns' stream goes on).
float() VR_AG_StreamOn =
{
    entity p = self.vr_ag_prop;
    entity q = self.vr_ag_prop2;
    return (p && p.weaponinst.wi_clip > 0 && VR_AG_Class(p.weapon) == VR_AG_CLASS_AUTO) ||
           (q && VR_AG_DualGuns(self) && q.weaponinst.wi_clip > 0 && VR_AG_Class(q.weapon) == VR_AG_CLASS_AUTO);
};

void() vr_ag_atk1 = { VR_AM_Anim(VR_AM_A_ATK, 0, vr_ag_atk2); self.vr_ag_shots = 0; VR_AG_AtkFrame(1); };
void() vr_ag_atk2 = { VR_AM_Anim(VR_AM_A_ATK, 1, vr_ag_atk3); VR_AG_AtkFrame(2); };
void() vr_ag_atk3 = { VR_AM_Anim(VR_AM_A_ATK, 2, vr_ag_atk4); VR_AG_AtkFrame(3); };
void() vr_ag_atk4 = { VR_AM_Anim(VR_AM_A_ATK, 3, vr_ag_atk5); VR_AG_AtkFrame(4); };
void() vr_ag_atk5 = { VR_AM_Anim(VR_AM_A_ATK, 4, vr_ag_atk6); VR_AG_AtkFrame(5); };
void() vr_ag_atk6 = { VR_AM_Anim(VR_AM_A_ATK, 5, vr_ag_atk7); VR_AG_AtkFrame(6); };
void() vr_ag_atk7 =
{
    VR_AM_Anim(VR_AM_A_ATK, 6, vr_ag_atk8);
    VR_AG_AtkFrame(7);
    if(VR_AG_StreamOn())
    {
        SUB_CheckRefire(vr_ag_atk4); // (a nail gun's or the thunderbolt's stream goes on while it sees its target)
    }
};
void() vr_ag_atk8 = { VR_AM_Anim(VR_AM_A_ATK, 7, vr_ag_atk9); VR_AG_AtkFrame(8); };
void() vr_ag_atk9 = { VR_AM_Anim(VR_AM_A_ATK, 8, vr_ag_atk9); VR_AG_AtkFrame(9); VR_AG_AfterAttack(); };

// A melee blow (the shove's frames: a step in, the arm out); its swing drawn from the second.
void() vr_ag_swing1 = { VR_AM_Anim(VR_AM_A_SHOVE, 0, vr_ag_swing2); ai_face(); ai_charge(4); };
void() vr_ag_swing2 = { VR_AM_Anim(VR_AM_A_SHOVE, 1, vr_ag_swing3); ai_face(); ai_charge(4); self.vr_ag_swing_at = time;
    VR_SoundVaried(self, CHAN_WEAPON, self.vr_ag_prop ? "weapons/ax1.wav" : "knight/sword1.wav", 1, ATTN_NORM); };
void() vr_ag_swing3 = { VR_AM_Anim(VR_AM_A_SHOVE, 2, vr_ag_swing4); ai_face(); ai_charge(2); };
void() vr_ag_swing4 = { VR_AM_Anim(VR_AM_A_SHOVE, 3, vr_ag_swing5); ai_face(); VR_AG_Strike(); };
void() vr_ag_swing5 = { VR_AM_Anim(VR_AM_A_SHOVE, 4, vr_ag_swing6); };
void() vr_ag_swing6 = { VR_AM_Anim(VR_AM_A_SHOVE, 5, vr_ag_swing6); VR_AG_AfterAttack(); };

// The reload: the grunt's load frames (unused by id's), over vr_agrunt_reload_time; a clip out of its reserve at the end.
void() VR_AG_ReloadFrame =
{
    self.nextthink = time + max(0.02, cvar_hget(cvarh_vr_agrunt_reload_time)) / 11;
    if(self.enemy)
    {
        VR_AG_WalkFire(self.nextthink - time, FALSE); // (stepping back as it reloads, nearer than its best)
    }
};
void() vr_ag_reload1 = { VR_AM_Anim(VR_AM_A_RELOAD, 0, vr_ag_reload2); VR_AG_ReloadFrame(); self.vr_agp_rl_at = time; VR_AG_Log("reloading"); };
void() vr_ag_reload2 = { VR_AM_Anim(VR_AM_A_RELOAD, 1, vr_ag_reload3); VR_AG_ReloadFrame(); };
void() vr_ag_reload3 = { VR_AM_Anim(VR_AM_A_RELOAD, 2, vr_ag_reload4); VR_AG_ReloadFrame(); };
void() vr_ag_reload4 = { VR_AM_Anim(VR_AM_A_RELOAD, 3, vr_ag_reload5); VR_AG_ReloadFrame(); };
void() vr_ag_reload5 = { VR_AM_Anim(VR_AM_A_RELOAD, 4, vr_ag_reload6); VR_AG_ReloadFrame(); sound(self, CHAN_WEAPON, "weapons/lock4.wav", 1, ATTN_NORM); };
void() vr_ag_reload6 = { VR_AM_Anim(VR_AM_A_RELOAD, 5, vr_ag_reload7); VR_AG_ReloadFrame(); };
void() vr_ag_reload7 = { VR_AM_Anim(VR_AM_A_RELOAD, 6, vr_ag_reload8); VR_AG_ReloadFrame(); };
void() vr_ag_reload8 = { VR_AM_Anim(VR_AM_A_RELOAD, 7, vr_ag_reload9); VR_AG_ReloadFrame(); };
void() vr_ag_reload9 = { VR_AM_Anim(VR_AM_A_RELOAD, 8, vr_ag_reload10); VR_AG_ReloadFrame(); };
void() vr_ag_reload10 = { VR_AM_Anim(VR_AM_A_RELOAD, 9, vr_ag_reload11); VR_AG_ReloadFrame(); };
void() vr_ag_reload11 =
{
    VR_AM_Anim(VR_AM_A_RELOAD, 10, vr_ag_run1);
    VR_AG_ReloadFrame();
    entity p = self.vr_ag_prop;
    if(p)
    {
        float n = min(VR_AG_ClipOf(p.weapon, p.weaponinst) - p.weaponinst.wi_clip, self.vr_ag_reserve);
        p.weaponinst.wi_clip = p.weaponinst.wi_clip + n;
        self.vr_ag_reserve = self.vr_ag_reserve - n;
        self.vr_ag_reloads = self.vr_ag_reloads + 1;
        VR_AG_Log(sprintf("reloaded: %g in the clip, %g in reserve", p.weaponinst.wi_clip, self.vr_ag_reserve));
    }
    entity q = self.vr_ag_prop2;
    if(q && VR_AG_ClipOf(q.weapon, q.weaponinst) > 0) // (two guns; a gun and an axe, the axe in hand: the gun too)
    {
        float n2 = min(VR_AG_ClipOf(q.weapon, q.weaponinst) - q.weaponinst.wi_clip, self.vr_ag_reserve2);
        q.weaponinst.wi_clip = q.weaponinst.wi_clip + n2;
        self.vr_ag_reserve2 = self.vr_ag_reserve2 - n2;
        VR_AG_Log(sprintf("reloaded its second: %g in the clip, %g in reserve", q.weaponinst.wi_clip, self.vr_ag_reserve2));
    }
};

// Taking up a weapon lying at its feet (the row's take frames: down on a knee, a crouch): it bends down, its hand
// reaching for the weapon where it lies (VR_AG_TakeBegin: the weapon its own at once, drawn where it lay, its hand's IK
// on it: vr_armedgrunt_pose.qc VR_AGP_Pose), its hand closes on it at the deepest frame (VR_AG_TakeGrab), and it comes
// up with it, the weapon carried from the floor to its hold over VR_AG_LIFT_TIME. Cut short before its hand closes (a
// pain, a shove: VR_AG_PlaceProp), the weapon stays on the floor.
float VR_AG_TAKE_FRAME = 0.1;   // s each take frame down (was 0.06: down and up in 0.36 s, the hand on it at once)
float VR_AG_TAKE_UP = 0.07;     // s each frame back up
float VR_AG_LIFT_TIME = 0.3;    // s from the floor to its hold
.float vr_ag_take_i;            // its take frames: the next step

void() VR_AG_TakeBegin =
{
    entity w = self.vr_ag_seek;
    self.vr_ag_seek = world;
    if(!w || w.classname != "thrown_weapon" || vlen(w.origin - self.origin) > VR_AG_PICKUP_REACH * 1.5 ||
       (self.vr_ag_prop && (self.vr_ag_prop2 || !VR_AG_PairsFor(self, self.vr_ag_wid, self.vr_ag_wflags, w.weapon, w.weaponflags))))
    {
        return;
    }
    entity inst = w.weaponinst;
    float wid = w.weapon;
    float flags = w.weaponflags;
    vector a = w.angles;
    a_x = -a_x; // (a model's pitch: back to its forward and up)
    makevectors(a);
    self.vr_ag_took_org = w.origin;
    self.vr_ag_took_fw = v_forward;
    self.vr_ag_took_up = v_up;
    w.weaponinst = world;
    remove(w);
    self.vr_ag_took_at = time;
    VR_AM_Row(self.vr_ag_kind);
    self.vr_ag_took_grab = time + max(1, vr_am_n_take) * VR_AG_TAKE_FRAME; // (its lowest frame drawn in full)
    self.vr_ag_took_done = 0;
    if(self.vr_ag_prop)
    {
        if(VR_AG_ArmLeft(self, wid, flags, inst)) // (its second hand's again: VR_AG_WantsSecond)
        {
            self.vr_ag_reserve2 = VR_AG_Unpocket(self, wid, flags); // (its spare rounds for it, if it has any)
        }
    }
    else
    {
        VR_AG_Arm(self, wid, flags, inst);
        self.vr_ag_reserve = VR_AG_Unpocket(self, wid, flags);
    }
    self.vr_ag_took_at = time; // (VR_AG_Arm's prop placed: still where it lay)
    VR_AG_Log(sprintf("reaching down for %s", wid == self.vr_ag_wid ? self.vr_ag_prop.netname : self.vr_ag_prop2.netname));
};

// Its hand closed on it: taken up.
void() VR_AG_TakeGrab =
{
    if(self.vr_ag_took_at <= 0 || !self.vr_ag_prop)
    {
        return;
    }
    self.vr_ag_pickups = self.vr_ag_pickups + 1;
    sound(self, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
    entity p = self.vr_ag_prop2 ? self.vr_ag_prop2 : self.vr_ag_prop;
    VR_AG_Log(sprintf("picked up %s (%g in its clip)%s", p.netname, p.weaponinst.wi_clip,
        self.vr_ag_prop2 ? " in its left hand" : ""));
};
// Its take frames (the row's vr_am_n_take down, VR_AG_TAKE_FRAME each, its lowest last), held there while its hand
// closes (VR_AG_TakeGrab at vr_ag_took_grab: the lowest frame drawn in full), then back up (VR_AG_TAKE_UP each).
void() vr_ag_takestep =
{
    VR_AM_Row(self.vr_ag_kind);
    float n = max(1, vr_am_n_take);
    float i = self.vr_ag_take_i;
    self.vr_ag_take_i = i + 1;
    if(i <= n)
    {
        VR_AM_Anim(VR_AM_A_TAKE, min(i, n - 1), vr_ag_takestep); // (down; then held at its lowest a frame)
        self.nextthink = time + VR_AG_TAKE_FRAME;
        return;
    }
    float k = n - 2 - (i - n - 1);
    if(k >= 0)
    {
        VR_AM_Anim(VR_AM_A_TAKE, k, vr_ag_takestep); // (back up)
        self.nextthink = time + VR_AG_TAKE_UP;
        return;
    }
    vr_ag_run1();
};
void() vr_ag_take1 =
{
    self.vr_ag_take_i = 0;
    self.think = vr_ag_takestep; // (taking: VR_AG_Taking, as VR_AG_TakeBegin arms it and places the weapon)
    VR_AG_TakeBegin();
    vr_ag_takestep();
};
// (The old take frames' names, kept for saved games: on from where the step was.)
void() vr_ag_take2 = { vr_ag_takestep(); };
void() vr_ag_take3 = { vr_ag_takestep(); };
void() vr_ag_take4 = { vr_ag_takestep(); };
void() vr_ag_take5 = { vr_ag_takestep(); };
void() vr_ag_take6 = { vr_ag_takestep(); };

// Its take frames on (its reach for a weapon not cut short).
float(entity g) VR_AG_Taking =
{
    return g.think == vr_ag_takestep || g.think == vr_ag_take2 || g.think == vr_ag_take3 || g.think == vr_ag_take4 ||
           g.think == vr_ag_take5 || g.think == vr_ag_take6 || g.think == vr_ag_run1 || g.think == vr_ag_hunt;
};

// ----------------------------------------------------------------------------
// Its AI
// ----------------------------------------------------------------------------

// A weapon lying about it would go for: usable, not spent, not in a hand or flying, within vr_agrunt_rearm_range and in
// sight, and its target (if any) not nearer to it than it is. world: none.
entity() VR_AG_FindWeapon =
{
    float range = cvar_hget(cvarh_vr_agrunt_rearm_range);
    entity best = world;
    float bestDist = range + 1;
    entity e = findradius(self.origin, range);
    while(e)
    {
        if(e.classname == "thrown_weapon" && VR_AG_Usable(e.weapon) && !e.vr_depleted && !VR_EnemyWeapons_Spared(e) &&
           (VR_AG_Class(e.weapon) == VR_AG_CLASS_MELEE || e.weaponinst.wi_clip > 0) &&
           (!self.vr_ag_prop || VR_AG_PairsFor(self, self.vr_ag_wid, self.vr_ag_wflags, e.weapon, e.weaponflags))) // (armed: its pair's second only)
        {
            float d = vlen(e.origin - self.origin);
            float safe = TRUE;
            if(self.enemy && self.enemy.health > 0 && d > 64)
            {
                safe = vlen(e.origin - self.enemy.origin) > d; // (one at its feet it takes up whatever)
            }
            if(safe && d < bestDist && fabs(e.origin_z - self.origin_z) < 64 && !VR_AG_Shunned(self, e))
            {
                traceline(self.origin + '0 0 16', e.origin, TRUE, self);
                if((trace_fraction == 1 || trace_ent == e) && VR_AG_Reachable(self, e)) // (walkable: vr_armedgrunt_play.qc)
                {
                    best = e;
                    bestDist = d;
                }
            }
        }
        e = e.chain;
    }
    return best;
};

// Disarmed: whether to go for a weapon (it looks now and then).
// Dual wielding: one of its two weapons lost (or, with vr_agrunt_dual_pickup, holding one that pairs), it goes for a
// second that pairs with it (VR_AG_Pairs; out of its target's reach).
float(entity g) VR_AG_WantsSecond =
{
    return (g.vr_ag_dual || cvar("vr_agrunt_dual_pickup")) && g.vr_ag_prop && !g.vr_ag_prop2 &&
           VR_AG_Pairable(g.vr_ag_wid, g.vr_ag_wflags) &&
           (!g.enemy || g.enemy.health <= 0 || vlen(VR_AI_Origin(g.enemy) - g.origin) > 160);
};

float() VR_AG_Seeking =
{
    if((self.vr_ag_prop && !VR_AG_WantsSecond(self)) || !cvar_hget(cvarh_vr_agrunt_rearm))
    {
        self.vr_ag_seek = world;
        return FALSE;
    }
    entity w = self.vr_ag_seek;
    if(w && (w.classname != "thrown_weapon" || VR_EnemyWeapons_Spared(w)))
    {
        self.vr_ag_seek = world; // (taken by someone else, or flying)
    }
    if(self.vr_ag_seek && !VR_AG_SeekCheck(self))
    {
        self.vr_ag_seek = world; // (out of its reach after all: given up for a while)
        self.vr_ag_seek_next = time + 0.5;
    }
    if(!self.vr_ag_seek && (time >= self.vr_ag_seek_next || self.vr_ag_own))
    {
        self.vr_ag_seek_next = time + 0.5;
        self.vr_ag_seek = VR_AG_OwnBack(self); // (its own, dropped as it fell: first)
        if(!self.vr_ag_seek)
        {
            self.vr_ag_seek = VR_AG_FindWeapon();
        }
        if(self.vr_ag_seek)
        {
            VR_AG_SeekStart(self);
            VR_AG_Log(sprintf("going for %s, %.0f away", self.vr_ag_seek.netname, vlen(self.vr_ag_seek.origin - self.origin)));
        }
    }
    return self.vr_ag_seek != world;
};

// Its run frames: to a weapon it goes for, away from a target too near its explosive weapon, else the grunt's run.
void(float d) VR_AG_Run =
{
    if(VR_AG_TugResume())
    {
        return; // (a tug-of-war over its weapon: its hold, vr_armedgrunt_play.qc)
    }
    self.vr_ag_myrun = time;
    VR_AG_DrawMain(self); // (its flare gun spent and gone: its own weapon)
    if(VR_AG_Seeking())
    {
        entity w = self.vr_ag_seek;
        if(vlen(w.origin - self.origin) <= VR_AG_PICKUP_REACH)
        {
            vr_ag_take1();
            return;
        }
        self.ideal_yaw = vectoyaw(w.origin - self.origin);
        ChangeYaw();
        if(!walkmove(self.ideal_yaw, d))
        {
            entity oldGoal = self.goalentity;
            self.goalentity = w;
            movetogoal(d);
            self.goalentity = oldGoal;
        }
        return;
    }
    if(!self.enemy)
    {
        // Out of a fight (it went for a weapon: VR_AG_Idle): looking about as it stands.
        if(!FindTarget())
        {
            self.th_stand();
        }
        return;
    }
    float safe = cvar_hget(cvarh_vr_agrunt_blast_safe);
    entity mine = safe ? VR_AG_NearMine() : world;
    if(mine)
    {
        // Away from a mine (its own blows at anything near).
        self.ideal_yaw = vectoyaw(self.origin - mine.origin);
        ChangeYaw();
        if(!walkmove(self.ideal_yaw, d))
        {
            walkmove(self.ideal_yaw + (self.lefty ? 60 : -60), d);
        }
        return;
    }
    if(time < self.vr_ag_ff_until)
    {
        // A friend in its line of fire: a step aside for a clear one, facing its target (VR_AG_FriendHold); blocked, the
        // other way.
        ai_face();
        if(!walkmove(self.angles_y + (self.lefty ? 90 : -90), d))
        {
            self.lefty = !self.lefty;
            walkmove(self.angles_y + (self.lefty ? 90 : -90), d);
        }
        return;
    }
    float explosive = self.vr_ag_prop && VR_AG_MinRange(self.vr_ag_wid) > 0;
    if(explosive && safe && time < self.vr_ag_dodge_until)
    {
        // A step aside (its last shot wasn't clear), facing its target.
        ai_face();
        if(!walkmove(self.angles_y + (self.lefty ? 90 : -90), d))
        {
            self.lefty = !self.lefty;
            walkmove(self.angles_y + 180, d * 0.5);
        }
        return;
    }
    if(explosive && visible(self.enemy) && vlen(self.enemy.origin - self.origin) < VR_AG_MinRange(self.vr_ag_wid))
    {
        // Backing off: its own blast would reach it (cornered: along the wall, either way).
        ai_face();
        if(!walkmove(self.angles_y + 180 + (self.lefty ? 30 : -30), d) && safe &&
           !walkmove(self.angles_y + 180 + (self.lefty ? -30 : 30), d))
        {
            self.lefty = !self.lefty;
            walkmove(self.angles_y + (self.lefty ? 90 : -90), d);
        }
        if(vlen(self.enemy.origin - self.origin) < 60 && time >= self.attack_finished)
        {
            self.th_melee();
            SUB_AttackFinished(1);
        }
        return;
    }
    ai_run(d);
};

// ai_stand, ai_walk (vr_agrunt_rearm_idle): unarmed and out of a fight, off to a weapon lying near (its run frames:
// VR_AG_Run walks it there and takes it up; then back to standing).
float() VR_AG_Idle =
{
    if((self.vr_ag_prop && !VR_AG_WantsSecond(self)) || self.health <= 0 || !cvar_hget(cvarh_vr_agrunt_rearm_idle) ||
       !VR_AG_Seeking())
    {
        return FALSE;
    }
    vr_ag_run1();
    return TRUE;
};

// CheckAnyAttack (ai.qc): an armed grunt's attack by its weapon; its own run frames for going for a weapon or backing
// off (the grunt's pains end in the grunt's run).
float() VR_AG_CheckAttack =
{
    entity targ = self.enemy;
    float d = vlen(VR_AI_Origin(targ) - self.origin);
    if(self.vr_ag_gunaxe)
    {
        VR_AGR_GunAxe(d); // (a gun and an axe: the gun, the axe up close or with the gun spent: vr_armedrepl.qc)
    }
    float c = VR_AG_Class(self.vr_ag_wid);
    float mine = time - self.vr_ag_myrun < 0.15;
    float explosive = VR_AG_MinRange(self.vr_ag_wid) > 0;
    float blastSafe = cvar_hget(cvarh_vr_agrunt_blast_safe);
    if(!mine && (VR_AG_Seeking() || (explosive && d < VR_AG_MinRange(self.vr_ag_wid)) || time < self.vr_ag_ff_until ||
                 (explosive && blastSafe && (time < self.vr_ag_dodge_until || VR_AG_NearMine()))))
    {
        vr_ag_run1();
        return TRUE;
    }
    if(explosive && blastSafe && (time < self.vr_ag_dodge_until || VR_AG_NearMine()))
    {
        return FALSE; // (stepping aside, or away from a mine: VR_AG_Run)
    }
    if(VR_AG_Stream(self.vr_ag_wid) && d > 0.8 * VR_ADGun_Value(self.vr_ag_wid, "range"))
    {
        return FALSE; // (a flamethrower's stream reaches so far)
    }
    if(self.vr_ag_seek)
    {
        return FALSE; // (on its way to a weapon)
    }
    if(time < self.attack_finished)
    {
        return FALSE;
    }
    if(c == VR_AG_CLASS_FISTS || c == VR_AG_CLASS_MELEE)
    {
        // (the chainsaw's thrust reaches less far than a swing: it closes in first, vr_agrunt_saw_start)
        float reach = self.vr_ag_wid == WID_CHAINSAW ? cvar_hget(cvarh_vr_agrunt_saw_start) :
                      VR_AG_GRIP_REACH + (c == VR_AG_CLASS_MELEE ? 10 : 0);
        if(d < reach)
        {
            if(VR_AG_FriendHold(self.origin + self.view_ofs * 0.5, VR_AG_FriendSpread(self.vr_ag_wid)))
            {
                return FALSE; // (a friend between: no swing through it)
            }
            self.th_melee();
            SUB_AttackFinished(0.6 + random() * 0.4);
            return TRUE;
        }
        return FALSE;
    }

    // The grunt's: a clear shot.
    traceline(self.origin + self.view_ofs, VR_AI_Origin(targ) + targ.view_ofs, self.vr_ai_gate > 0 ? MOVE_PORTALS : FALSE, self);
    if(trace_inopen && trace_inwater)
    {
        return FALSE;
    }
    if(trace_ent != targ)
    {
        if((trace_ent.flags & FL_MONSTER) && trace_ent.health > 0 && trace_ent.enemy != self)
        {
            VR_AG_FriendHold(self.origin + self.view_ofs, 0); // (a friend in its sight line: a step aside)
        }
        return FALSE;
    }
    if(VR_AG_MinRange(self.vr_ag_wid) > 0 && d < VR_AG_MinRange(self.vr_ag_wid))
    {
        return FALSE; // (backing off: VR_AG_Run)
    }
    if(self.vr_ag_prop && VR_AG_FriendHold(weaponmuzzle(self.vr_ag_prop), VR_AG_FriendSpread(self.vr_ag_wid)))
    {
        return FALSE; // (a friend in its line of fire: a step aside, VR_AG_Run)
    }
    if(explosive && blastSafe && self.vr_ag_prop &&
       !VR_AG_BlastSafe(self.vr_ag_prop, normalize(VR_AI_Origin(targ) - weaponmuzzle(self.vr_ag_prop))))
    {
        // Its round wouldn't burst clear of it from here (a wall, a ledge ahead): a step aside first.
        self.vr_ag_dodge_until = time + 0.5 + random() * 0.5;
        self.lefty = !self.lefty;
        VR_AG_Log(sprintf("not clear to fire: it would burst %.0f from it (%s at %v)", vlen(vr_ag_blast_at - self.origin),
            trace_ent.classname, vr_ag_blast_at));
        return FALSE;
    }
    float native = VR_AGR_NativeAlone(self); // (its own gun alone: its monster's chances and pauses, vr_armedrepl.qc)
    float chance = native ? VR_AGR_NativeChance(enemy_range) : VR_AG_Chance(self.vr_ag_wid, enemy_range);
    if(!explosive && VR_AG_Ranges(self) && d < vr_ag_rmin)
    {
        // Too near for its gun: backing off mostly (VR_AG_RangeMove), firing now and then as it goes (a launcher's
        // retreat never fires: above). You with a blade or charging: rarer still.
        chance = chance * (vr_ag_rthreat ? 0.3 : 0.5);
    }
    if(random() < chance)
    {
        self.th_missile();
        if(native)
        {
            VR_AGR_NativeFinished();
        }
        else
        {
            SUB_AttackFinished(1 + random());
        }
        if(!(native && self.vr_ag_kind == VR_AM_ENFORCER) && random() < 0.3) // (the enforcer's CheckAttack: never)
        {
            self.lefty = !self.lefty;
        }
        return TRUE;
    }
    return FALSE;
};

void() VR_AG_Missile =
{
    if(VR_AGR_NativeAlone(self))
    {
        VR_AGR_NativeAttack(); // (its own gun: its monster's own attack frames and shots, vr_armedrepl.qc)
        return;
    }
    vr_ag_atk1();
};
void() VR_AG_Melee = { VR_AGP_Attack(); }; // (punches, swings, the chainsaw's thrust: vr_armedgrunt_pose.qc)

// T_DamageDeal: its weapon's damage to a player.
float(entity targ, entity inflictor, entity attacker, float damage) VR_AG_DamageScale =
{
    // (a replacement's own gun: its monster's own shot, its own damage; its shoves its monster's: vr_armedrepl.qc)
    float native = inflictor.vr_ag_native || (vr_agr_native_fire && inflictor == attacker) ||
                   (attacker.vr_agr && attacker.vr_eshove_striking);
    if((targ.flags & FL_CLIENT) && (attacker.flags & FL_MONSTER))
    {
        vr_agr_pl_hits = vr_agr_pl_hits + 1; // (the tests: vr_armedrepl_test.qc; the armed ones' scale below)
        vr_agr_pl_raw = vr_agr_pl_raw + damage;
        vr_agr_pl_dmg = vr_agr_pl_dmg + damage * ((attacker.vr_ag && !native) ? max(0, cvar_hget(cvarh_vr_agrunt_damage_scale)) : 1);
    }
    if(native)
    {
        return damage;
    }
    if(attacker == targ && targ.vr_ag && targ.health > 0)
    {
        vr_ag_self_dmg = vr_ag_self_dmg + damage; // (the tests: its own blast)
    }
    if(attacker.vr_ag && (targ.flags & FL_MONSTER) && attacker != targ && targ.health > 0)
    {
        vr_ag_ff_hits = vr_ag_ff_hits + 1; // (the tests: friendly fire)
        vr_ag_ff_dmg = vr_ag_ff_dmg + damage;
        VR_AG_Log(sprintf("friendly hit: %s's %s on %s, %g damage, %.0f from it (held for friends %.1f s ago)",
            attacker.classname, attacker.vr_ag_prop ? attacker.vr_ag_prop.netname : "fists", targ.classname, damage,
            vlen(targ.origin - attacker.origin), time - attacker.vr_ag_ff_last));
    }
    if(attacker.vr_ag && (targ.flags & FL_CLIENT) && attacker != targ)
    {
        damage = damage * max(0, cvar_hget(cvarh_vr_agrunt_damage_scale));
        vr_ag_player_dmg = vr_ag_player_dmg + damage;
        return damage;
    }
    return damage;
};

// T_DamageDeal: a blow on it, near its gun hand and hard enough, knocks its weapon out of it.
void(entity targ, entity inflictor, entity attacker, float take) VR_AG_Hit =
{
    if(!targ.vr_ag || !targ.vr_ag_prop || targ.health <= 0 || !cvar_hget(cvarh_vr_agrunt_disarm) || vr_agpar_now ||
       take < VR_AG_DisarmDamage(targ) || inflictor == world || inflictor.classname == "vr_burn")
    {
        return;
    }
    VR_HitReact_Where(targ, inflictor, attacker);
    if(vr_hr_at == (targ.absmin + targ.absmax) * 0.5 || vr_hitkind == QVR_HITKIND_SPLASH)
    {
        return; // (where it struck isn't known, or a blast's: its box's nearest point)
    }
    if(targ.vr_ag_prop2 && !targ.vr_ag_tug_pl && vlen(vr_hr_at - targ.vr_ag_prop2.origin) < vlen(vr_hr_at - targ.vr_ag_prop.origin))
    {
        VR_AG_Swap(targ); // (nearer its other hand's weapon: that one's the one at stake)
    }
    float near = vlen(vr_hr_at - targ.vr_ag_prop.origin);
    if(cvar_hget(cvarh_vr_agrunt_debug))
    {
        VR_AG_Log(sprintf("hit %g at %.1f from its gun hand (part %g)", take, near, vr_hr_part));
    }
    if(near <= VR_AG_DISARM_HAND || (vr_hr_part == 2 && near <= VR_AG_DISARM_HAND * 1.6))
    {
        VR_AG_Disarm(targ, vr_hr_dir, 150, sprintf("a blow of %g on its gun hand", take));
    }
};

// SoldierDrop: an armed grunt's backpack holds its weapons' spare rounds (none of a real ammo kind: a grunt's 5 shells).
// ----------------------------------------------------------------------------
// A flare gun as its second weapon (vr_agrunt_flaregun; vr_flaregun.qc)
// ----------------------------------------------------------------------------
//
// One from the pool (or a replacement, from its seed) with one weapon may carry a flare gun too: it spawns with the flare
// gun in its hand and its own weapon stowed (no prop: its record and its reserve kept here), fires its two flares (a
// gun's shot an attack), throws the spent flare gun away (VR_AG_AfterAttack) and takes its own weapon (VR_AG_DrawMain;
// also when the flare gun is knocked out of its hand or yanked away: its next run frame). Dying with it stowed, it drops
// that too (VR_AG_DropMain).
.float vr_ag_main_wid;     // its own weapon stowed behind the flare gun (0: none)
.float vr_ag_main_wflags;  // ... its flags
.entity vr_ag_main_inst;   // ... its record (its clip)
.float vr_ag_main_reserve; // ... its reserve
float vr_ag_arm_rolled; // VR_AM_ArmAll's: a roll for one made (not for a map's or the toolgun's own weapons)
float vr_ag_arm_roll;   // ... the roll, 0..1 (no initialiser: a QC global with one is a constant)
float vr_ag_flare_draws;   // armed grunts that took their own weapon after their flare gun (the tests)

// `g`'s flare gun (its roll `r`, 0..1, under vr_agrunt_flaregun): its weapon stowed, the flare gun in its hand.
void(entity g, float r) VR_AG_MaybeFlareGun =
{
    entity p = g.vr_ag_prop;
    if(r >= cvar_hget(cvarh_vr_agrunt_flaregun) || !p || g.vr_ag_prop2 || p.vr_ag_native || g.vr_ag_wid == WID_FLAREGUN ||
       g.vr_ag_main_wid > 0 || !VR_AM_Holds(g, VR_AG_CLASS_GUN))
    {
        return;
    }
    g.vr_ag_main_wid = g.vr_ag_wid;
    g.vr_ag_main_wflags = g.vr_ag_wflags;
    g.vr_ag_main_inst = p.weaponinst;
    g.vr_ag_main_reserve = g.vr_ag_reserve;
    p.weaponinst = world;
    remove(p);
    g.vr_ag_prop = world;
    VR_AG_Arm(g, WID_FLAREGUN, 0, world);
    g.vr_ag_reserve = 0;
    VR_AG_Log(sprintf("a flare gun first, its %s stowed", WeaponIdToWeaponName(g.vr_ag_main_wid)));
};

// Its own weapon from where it was stowed, into its empty hand (its flare gun gone).
void(entity g) VR_AG_DrawMain =
{
    if(g.vr_ag_main_wid <= 0 || g.vr_ag_prop || g.health <= 0)
    {
        return;
    }
    float wid = g.vr_ag_main_wid;
    g.vr_ag_main_wid = 0;
    VR_AG_Arm(g, wid, g.vr_ag_main_wflags, g.vr_ag_main_inst);
    g.vr_ag_reserve = g.vr_ag_main_reserve;
    g.vr_ag_main_inst = world;
    g.vr_ag_main_reserve = 0;
    vr_ag_flare_draws = vr_ag_flare_draws + 1;
    sound(g, CHAN_WEAPON, "weapons/lock4.wav", 0.7, ATTN_NORM);
    VR_AG_Log(sprintf("its flare gun gone: takes its %s (%g in reserve)", g.vr_ag_prop ? g.vr_ag_prop.netname : "fists",
        g.vr_ag_reserve));
};

// Dying with its own weapon still stowed: it falls too (its record with it), flying at `vel`.
void(entity g, vector vel) VR_AG_DropMain =
{
    if(g.vr_ag_main_wid <= 0)
    {
        return;
    }
    float wid = g.vr_ag_main_wid;
    g.vr_ag_main_wid = 0;
    VR_AG_Pocket(g, wid, g.vr_ag_main_wflags, g.vr_ag_main_reserve); // (its spare rounds: its backpack's)
    g.vr_ag_main_reserve = 0;
    entity inst = g.vr_ag_main_inst ? g.vr_ag_main_inst : WeaponInst_New(VR_AG_ClipSize(wid));
    g.vr_ag_main_inst = world;
    CreateThrownWeapon(g, wid, g.vr_ag_main_wflags, inst, g.origin + '0 0 12', randomVec(180), vel, randomVec(90),
        FALSE /* disappear on hit */, FALSE /* deal damage */, WeaponIdToThrowDamage(wid));
    VR_AG_Log(sprintf("its stowed %s dropped", WeaponIdToWeaponName(wid)));
};

float() VR_AG_SoldierDrop =
{
    if(!self.vr_ag)
    {
        return FALSE;
    }
    // Its backpack: the spare rounds of its weapons, of their real ammo kinds: in its hands (if it still holds them),
    // stowed (its flare gun's), in its pockets (VR_AG_Dying dropped the held ones: their spare rounds pocketed). A
    // single-use gun's spare magazines (no ammo kind of yours) are not in it.
    VR_AG_Pocket(self, self.vr_ag_wid, self.vr_ag_wflags, self.vr_ag_reserve);
    self.vr_ag_reserve = 0;
    if(self.vr_ag_prop2)
    {
        VR_AG_Pocket(self, self.vr_ag_wid2, self.vr_ag_wflags2, self.vr_ag_reserve2);
    }
    self.vr_ag_reserve2 = 0;
    if(self.vr_ag_main_wid > 0)
    {
        VR_AG_Pocket(self, self.vr_ag_main_wid, self.vr_ag_main_wflags, self.vr_ag_main_reserve);
        self.vr_ag_main_reserve = 0;
    }
    self.vr_ag_pocket_wid = 0;
    self.vr_ag_pocket_n = 0;
    if(!(self.ammo_shells + self.ammo_nails + self.ammo_rockets + self.ammo_cells + self.ammo_lava_nails +
         self.ammo_multi_rockets + self.ammo_plasma))
    {
        if(self.vr_agr)
        {
            return FALSE; // (a replacement with none: the loot of the monster it replaced, its chance of a weapon too)
        }
        // None of a real ammo kind (fists, melee weapons, single-use guns, all spent): its monster's own loot (a grunt's
        // 5 shells, an enforcer's 5 cells).
        VR_AM_Row(self.vr_ag_kind);
        VR_AmmoUtil_SetAmmoCountByAmmoId(self, vr_am_loot_aid, vr_am_loot_n);
    }
    DropBackpack();
    return TRUE;
};

// army_die, an armed grunt's (its death is the grunt's own: its th_die army_die, so its corpse, its ragdoll, its gibs,
// limbs and blood are the grunt's everywhere they're keyed on it): its weapon falls from its hand with what is left in
// it (gibbed too). FALSE: not an armed grunt (the grunt's gun drops).
float() VR_AG_Dying =
{
    if(!self.vr_ag)
    {
        return FALSE;
    }
    if(damage_attacker == self)
    {
        vr_ag_self_kills = vr_ag_self_kills + 1; // (the tests)
        VR_AG_Log(sprintf("killed by its own %s", self.vr_ag_prop ? self.vr_ag_prop.netname : "weapon"));
    }
    makevectors(self.angles);
    VR_AG_DropHeld(self, v_forward * 25 - v_right * 20 + v_right * (crandom() * 15) + '0 0 70');
    VR_AG_DropHeld(self, v_forward * 25 + v_right * 20 + v_right * (crandom() * 15) + '0 0 70'); // (its other, if two)
    VR_AG_DropMain(self, v_forward * 15 + v_right * (crandom() * 25) + '0 0 60'); // (its own, stowed behind a flare gun)
    return TRUE;
};
void() VR_AG_Die = { army_die(); }; // (a save from before: its th_die)

/*QUAKED monster_vr_grunt (1 0 0) (-16 -16 -24) (16 16 40) Ambush
An armed grunt (experimental): a grunt holding one of your weapons. "weapon": its weapon (WID_*: 2 axe, 4 shotgun,
5 super shotgun, 6 nailgun, 7 super nailgun, 8 grenade launcher, 10 rocket launcher, 11 thunderbolt, 13 sword,
14 chainsaw, 15 grunt's rifle, 16 laser rifle, 17 crowbar; with Arcane Dimensions' data 20 AD rocket launcher, 21 AD
grenade launcher, 22 plasma rifle, 23 defender's shotgun, 25 centurion's plasma gun, 26 flamethrower, 27 poison sprayer,
28 crossbow, 101 Shadow Axe, 102 Widowmaker, 103 Plasma Gun, 111..116 AD's swords, hammer and mace, 118 Dark Prince's
sword, 119 justice knight's, 120 judicator's; with Rogue's data 117 Phantom sword; -1 its fists; 0 or unset: one from
the pool). "weaponflags": that weapon's flags. "weapon2", "weaponflags2": a second weapon in its left hand (both
one-handed: 2 axe, 13 sword, 17 crowbar, 101 Shadow Axe, 111..114 and 117..120 AD's swords; else one weapon).
*/
void() monster_vr_grunt = { VR_AM_Spawn(VR_AM_GRUNT); };

/*QUAKED monster_vr_enforcer (1 0 0) (-16 -16 -24) (16 16 40) Ambush
An armed enforcer (experimental): an enforcer holding one of your weapons (its laser rifle's place). "weapon": its weapon
(as monster_vr_grunt's: WID_*, -1 its fists, 0 or unset: one from the pool). "weaponflags": that weapon's flags.
"weapon2", "weaponflags2": a second one-handed weapon in its left hand (as monster_vr_grunt's).
*/
void() monster_vr_enforcer = { VR_AM_Spawn(VR_AM_ENFORCER); };

// An armed monster of the table's row `kind` (vr_armedmon.qc): its weapon picked, then its monster's own spawn (its
// classname, health, sounds, pains, deaths: that monster to the rest of the game), its gunless model, the layer's run,
// attacks and weapon.
void(float want, float flags, entity inst, float want2, float flags2) VR_AM_ArmAll;
void(float kind) VR_AM_Spawn =
{
    if(MG_MonsterInhibited()) return;
    self.vr_ag_kind = kind;
    if(deathmatch)
    {
        remove(self);
        return;
    }
    float want = self.weapon;
    float flags = self.weaponflags;
    float want2 = self.weapon2; // (its left hand's: "weapon2", "weaponflags2"; one-handed both, VR_AG_OneHanded)
    float flags2 = self.weaponflags2;
    self.weapon = 0;
    self.weaponflags = 0;
    self.weapon2 = 0;
    self.weaponflags2 = 0;
    float pooled = want == 0;
    if(want == 0)
    {
        want = VR_AG_PickWeapon();
        flags = 0;
    }
    if(want2 == 0 && pooled && random() < cvar("vr_agrunt_dual_chance") && VR_AG_Decode(want) &&
       VR_AG_Pairable(vr_ag_dec_wid, vr_ag_dec_flags))
    {
        // A second weapon that pairs with it from the pool (its weights; a few tries: none, one alone).
        float w1 = vr_ag_dec_wid;
        float f1 = vr_ag_dec_flags;
        float tries;
        for(tries = 0; tries < 12 && want2 == 0; tries++)
        {
            float c2 = VR_AG_PickWeapon();
            if(c2 > 0 && VR_AG_Decode(c2) && VR_AG_Pairs(w1, f1, vr_ag_dec_wid, vr_ag_dec_flags))
            {
                want2 = c2;
            }
        }
    }
    entity inst = world;
    self.vr_ag_code = want;
    if(want > 0)
    {
        if(!VR_AG_Decode(want))
        {
            dprint(sprintf("armed monster: weapon %g can't be held here: its fists\n", want));
            want = -1;
        }
        else
        {
            // (AD's upgrades: on its record; its melee weapons: the sword's kind in its flags)
            want = vr_ag_dec_wid;
            flags = flags | vr_ag_dec_flags;
            if(vr_ag_dec_up)
            {
                inst = WeaponInst_New(0);
                inst.wi_adup = vr_ag_dec_up;
                inst.wi_clip = VR_AG_ClipOf(want, inst);
            }
        }
    }
    if(!VR_OtherFlags() && !self.vr_agr)
    {
        // (a replacement keeps its map's: VR_AGR_Replace never replaces those trigger-spawned copies' maker)
        self.spawnflags = self.spawnflags - (self.spawnflags & (SPAWNFLAG_TRIGGER_SPAWN | SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE));
    }

    if(want > 0 && !VR_AM_Holds(self, VR_AG_Class(want)))
    {
        want = -1; // (not a weapon its row holds: its fists)
    }

    VR_AM_Row(kind);
    string mdl = vr_am_model;
    self.classname = vr_am_classname;
    vr_am_spawning = 1; // (its monster's own spawn: never replaced again, VR_AGR_Replace)
    vr_am_spawn();
    vr_am_spawning = 0;
    precache_model(mdl);
    precache_sound("weapons/lock4.wav");
    precache_sound("weapons/pkup.wav");
    precache_sound("weapons/ax1.wav");
    precache_sound("weapons/tink1.wav");
    VR_AG_PlayPrecache(); // (its headbutt, its disarms' clatters: vr_armedgrunt_play.qc)
    vector mn = self.mins;
    vector mx = self.maxs;
    if(self.MG_deferred)
    {
        self.MG_monster_model = mdl; // (a map's monster waiting for its trigger, unseen: its model when it comes)
    }
    else
    {
        setmodel(self, mdl);
        setsize(self, mn, mx); // (its monster's own box)
    }
    self.vr_ag = 1;
    self.vr_ag_kind = kind;
    self.th_run = vr_ag_hunt; // (its run; first the taunt, vr_agrunt_taunt)
    self.th_missile = VR_AG_Missile;
    self.th_melee = VR_AG_Melee;
    vr_ag_arm_rolled = (pooled && want2 == 0); // (a flare gun too: one from the pool, one weapon)
    vr_ag_arm_roll = random();
    VR_AM_ArmAll(want, flags, inst, want2, flags2);
};

// Its weapons from its keys (as VR_AM_Spawn's: "weapon" decoded to `want`, `flags`, `inst`; "weapon2" `want2`,
// `flags2`), their reserves; a replacement's loadout too (vr_armedrepl.qc VR_AGR_Apply: its props gone first).
void(float want, float flags, entity inst, float want2, float flags2) VR_AM_ArmAll =
{
    VR_AG_PocketClear(self); // (its pockets: empty)
    VR_AG_Arm(self, want, flags, inst);
    self.vr_ag_wid2 = -1;
    if(want2 > 0 && VR_AG_Decode(want2) && VR_AM_Holds(self, VR_AG_Class(vr_ag_dec_wid)))
    {
        entity inst2 = world;
        if(vr_ag_dec_up)
        {
            inst2 = WeaponInst_New(0);
            inst2.wi_adup = vr_ag_dec_up;
            inst2.wi_clip = VR_AG_ClipOf(vr_ag_dec_wid, inst2); // (the Plasma Gun's, the Widowmaker's: full)
        }
        if(!VR_AG_ArmLeft(self, vr_ag_dec_wid, flags2 | vr_ag_dec_flags, inst2))
        {
            dprint(sprintf("armed monster: weapon2 %g with weapon %g: not a pair (VR_AG_Pairs), one weapon\n", want2, want));
        }
    }
    VR_AGP_Spawn(self); // (its fists' pose sender)
    self.vr_ag_reserve = self.vr_ag_prop ? VR_AG_SpawnReserve(self.vr_ag_wid, self.vr_ag_prop.weaponinst) : 0;
    self.vr_ag_reserve2 = 0;
    if(self.vr_ag_prop2)
    {
        self.vr_ag_reserve2 = VR_AG_SpawnReserve(self.vr_ag_wid2, self.vr_ag_prop2.weaponinst);
    }
    if(vr_ag_arm_rolled)
    {
        vr_ag_arm_rolled = FALSE;
        VR_AG_MaybeFlareGun(self, vr_ag_arm_roll);
    }
    VR_AG_Log(sprintf("spawned with %s%s%s (%g in reserve)", self.vr_ag_prop ? self.vr_ag_prop.netname : "its fists",
        self.vr_ag_prop2 ? " and in its left hand " : "", self.vr_ag_prop2 ? self.vr_ag_prop2.netname : "", self.vr_ag_reserve));
};

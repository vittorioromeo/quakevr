// Arcane Dimensions' artifacts and custom pickups in every map (docs/vr-port/ARCANE_DIMENSIONS.md, section 16;
// decision 16). After vr_ad_bridge.qc: it uses AD's names (QC/ad/); its functions Quake VR's files call are declared in
// vr_ad_defs.qc, each call there marked "AD support".
//
// While AD's data is installed (VR_Pack_AD, and its folder mounted: vr_ad_mounted), the AD-only classnames of the
// engine's list (vr_ad.cpp everywhereClasses: the seven new artifacts and their Quoth aliases, the custom keys, custom
// items, the ladders) spawn in any map as AD's own entities (their spawn, their touch; a hand's touch too:
// VR_AD_BackpackRestore), after AD's reduced world setup (VR_AD_WorldOnDemand); the health vials are Quake VR's
// (item_healthvial). AD's other pickups that would override one of Quake VR's or its rules
// (the backpacks: Quake's backpack) only in AD maps (vr_ad_everywhere 0: all of them only there).
//
// Their effects are Quake VR's player's, wherever they were taken (AD's player code never runs on him):
// - Sharp Shooter: the shotguns' spread as AD's sharp one (FireBullets: AD_SharpSpread);
// - Nail Piercer: the nailguns' nails go on through bodies (spike_touch, superspike_touch: AD_NailPierce);
// - Blast Belt: no damage from blasts, the armour kept, the push given (rocket jumps); with Quad, half of any other
//   damage (T_DamageDeal: AD_ArtifactDamage);
// - Lava Shield: no lava damage, and lava sets you on no fire (WaterMove, VR_Burn_LavaPlayers: AD_LavaShield);
// - Air Tank: no drowning, AD's breathing; half the poison (AD's own, in its debuff);
// - Jump Boots: AD's air jumps, 1-4 or endless, no comfort limits (decision "no comfort limits"; PlayerJump);
// - Tome of Power: an event item (AD's own: it fires its targets).
// Their timers, their fading warnings and their end are ticked here (AD_ArtifactsTick, AD's ClientPowerups for them),
// and their icons and the soonest timer go to the wrist gadget (.vr_ad_hud*, the engine's STAT_QVR_AD*).

// ---------------------------------------------------------------------------------------------------------------------
// AD's world outside AD maps

// AD_WorldSpawn, a map not played by AD's rules while AD's data is there: AD's defaults for what its artifacts,
// pickups, ladders and debuffs read of AD's world (AD's worldspawn sets them in its own maps). Nothing of AD's world
// itself (its gravity, fog, particles, light styles): the map is Quake VR's.
void() AD_WorldDefaults =
{
    ad_skill = skill;
    ad_developer = cvar_hget(cvarh_developer);
    ad_engine = ad_ENG_FITZ;
    ad_player_fallspeed_reset(); // (the jump speed the boots start from; the fall rules stay Quake VR's here)
    ad_map_bleedingdmg = ad_DAMAGE_BLEEDING;
    ad_map_hazarddmg_x = ad_WATER_DAMAGE;
    ad_map_hazarddmg_y = ad_SLIME_DAMAGE;
    ad_map_hazarddmg_z = ad_LAVA_DAMAGE;
    ad_map_gravity = cvar_hget(cvarh_sv_gravity);
    ad_configflag = 0; // (AD's sprite particles off: its emitters, bursts and respawn rings)
};

// The engine, before the first AD entity of a map outside AD's rules spawns (vr_ad.cpp VR_AD_SpawnsOutside; also for
// one spawned later, by the toolgun or a test): what its code plays besides its own precaches.
void() VR_AD_WorldOnDemand =
{
    AD_WorldDefaults();
    precache_sound(ad_SOUND_HEARTBEAT);
    precache_sound(ad_SOUND_ARTJUMPBOOTS3A);
    precache_sound(ad_SOUND_ARTJUMPBOOTS3B);
    precache_sound(ad_SOUND_ARTJUMPBOOTS3C);
    dprint("VR: Arcane Dimensions' entities in this map: AD's reduced world setup\n");
};

// ---------------------------------------------------------------------------------------------------------------------
// AD's health vials (and gems, their old name): Quake VR's health pickups with AD's look, as Dawn of the Machine's armour
// shards are Quake VR's armour (vr_mg3_items.qc). AD's own entity floated untouched by the hands and the force grab, and
// its heal (ad_T_Heal, capped by an AD field our player lacked) left the player at 0 health, alive but unable to act. As
// a health box (item_health): placed by StartItem (an AD map's STARTOFF kept: AD_ItemStartOff), a force-grabbable
// object at the boxes' scale (AfterPlacementHealthBox: vr_forcegrabbable_health_boxes), taken by a hand or by walking
// into it (health_touch: Quake VR's T_Heal). 2-5 health as AD's (a map's "healamount" else), up to the maximum. Only
// with AD's model on the search path; outside AD's maps only with vr_ad_everywhere (decision 16).
/*QUAKED item_healthvial (0.3 0.3 1) (-8 -8 0) (8 8 32)
2-5 health (Arcane Dimensions; needs its data).
*/
void() item_healthvial =
{
    if(!AD_Rules() && cvar("vr_ad_everywhere") == 0)
    {
        remove(self);
        return;
    }
    if(!VR_Pack_RequireSpawn(VR_Has(ad_MODEL_HEAL5), "Arcane Dimensions")) return;
    self.noise = VR_Has("sound/items/health0.wav") ? ad_SOUND_HEAL05 : "items/r_item1.wav"; // (AD's own sound)
    precache_model(ad_MODEL_HEAL5);
    precache_sound(self.noise);
    setmodel(self, ad_MODEL_HEAL5);
    if(self.healamount < 1) self.healamount = rint(2 + random() * 3);
    self.healtype = 0; // (not a megahealth: up to the maximum)
    self.handtouch = health_touch;
    // Drawn vr_ad_healthvial_scale times AD's tiny model (author's note 2026-10-22: 2.5); as an object, its box with it
    // (MakeItemGrabbable). Its touch box as ever.
    self.vr_pickup_scale = max(0.1, cvar("vr_ad_healthvial_scale"));
    VR_ScaleUtil_SetScale(self, VR_ScaleUtil_GetUniformScaleVector(self.vr_pickup_scale));
    setsize(self, '-8 -8 0', '8 8 32');
    StartItem();
    self.think2 = AfterPlacementHealthBox;
    self.nextthink2 = time + 0.75;
};
/*QUAKED item_healthgem (0.3 0.3 1) (-8 -8 0) (8 8 32)
The health vial's old name (Arcane Dimensions).
*/
void() item_healthgem = { item_healthvial(); };

// ---------------------------------------------------------------------------------------------------------------------
// The artifacts' effects

// AD's artifact touch (a reviewed patch, ad_import.py), the item flags it gives: the Sharp Shooter's and the Nail
// Piercer's Quad bit (AD's shared HUD icon) would be Quake VR's Quad itself (its tint, icon, glow): none.
float(entity xItem) VR_AD_ArtifactItems =
{
    if(xItem.ad_classtype == ad_CT_ARTSHARP || xItem.ad_classtype == ad_CT_ARTPIERCE)
    {
        return xItem.items - (xItem.items & IT_QUAD);
    }
    return xItem.items;
};

// FireBullets (self: the player): the shotguns' spread with AD's Sharp Shooter, as AD's (the shotgun's 0.04 square to
// 0.01, the super shotgun's 0.14 x 0.08 to 0.04 x 0.04).
vector(vector spread) AD_SharpSpread =
{
    if(!(self.ad_moditems & ad_IT_ARTSHARP))
    {
        return spread;
    }
    if(spread_x >= 0.1)
    {
        spread_x = spread_x * (ad_SPREAD_SSG2_x / ad_SPREAD_SSG_x);
        spread_y = spread_y * (ad_SPREAD_SSG2_y / ad_SPREAD_SSG_y);
        return spread;
    }
    return spread * (ad_SPREAD_SG2_x / ad_SPREAD_SG_x);
};

.float vr_ad_pierced; // a nail of the Nail Piercer's: the bodies it went through (AD_NailPierce)

// spike_touch, superspike_touch (self: the nail), its damage done to `xBody`: with its shooter's Nail Piercer, the nail
// goes on through a body (a monster's, a player's), as AD's: the same nail out the other side (within 128 units), at
// its speed, a little gore now and then. The one that hit is removed as ever.
void(entity xBody) AD_NailPierce =
{
    entity p = self.owner;
    if(!(p.flags & FL_CLIENT) || !(p.ad_moditems & ad_IT_ARTPIERCE) || !(xBody.flags & (FL_MONSTER | FL_CLIENT)) ||
       xBody == p || xBody.solid == SOLID_BSP)
    {
        return;
    }
    if(xBody.ad_resist_nails > 0 && (AD_Rules() || AD_Run(xBody)))
    {
        return; // (a body that resists nails stops them, as AD's)
    }
    vector dir = normalize(self.velocity);
    if(dir == '0 0 0')
    {
        return;
    }
    vector at = self.origin;
    float steps = 0;
    while(steps < 8)
    {
        traceline(at, at + dir * 512, FALSE, self);
        if(trace_ent != xBody)
        {
            break;
        }
        at = at + dir * 16;
        steps = steps + 1;
    }
    if(steps >= 8 || pointcontents(at) != CONTENT_EMPTY)
    {
        return;
    }
    entity nail = self;
    self = p;
    entity n = launch_spike(at, dir);
    self = nail;
    n.classname = nail.classname;
    n.touch = nail.touch;
    setmodel(n, nail.model);
    setsize(n, VEC_ORIGIN, VEC_ORIGIN);
    n.skin = nail.skin;
    n.effects = nail.effects;
    n.velocity = nail.velocity;
    n.angles = nail.angles;
    n.nextthink = nail.nextthink;
    n.vr_ad_pierced = nail.vr_ad_pierced + 1;
    if(random() < 0.2)
    {
        SpawnMeatSpray(nail.origin, dir * 100);
    }
};

// T_DamageDeal (after the multipliers, before the armour) on a player with AD's artifacts: the Blast Belt takes a
// blast's damage whole, the armour kept, the push given (rocket jumps), with its reminder; with Quad too, any other
// damage halved (AD's combination). -1: none taken; else the damage.
float(entity targ, entity inflictor, float damage) AD_ArtifactDamage =
{
    if(!(targ.flags & FL_CLIENT) || !(targ.ad_moditems & ad_IT_ARTBLASTBELT))
    {
        return damage;
    }
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        if(inflictor != world && targ.movetype == MOVETYPE_WALK)
        {
            vector dir = normalize(targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5);
            targ.velocity = targ.velocity + dir * damage * 8;
        }
        ad_PlayBlastBeltSound(targ);
        return -1;
    }
    if(targ.super_damage_finished > time)
    {
        return ceil(damage * 0.5);
    }
    return damage;
};

// Whether AD's Lava Shield keeps lava off player `p` (no damage, no fire).
float(entity p) AD_LavaShieldOn =
{
    return (p.ad_moditems & ad_IT_ARTLAVASHIELD) != 0;
};

// WaterMove in lava: TRUE with AD's Lava Shield (no damage; its reminder every 2 s).
float(entity p) AD_LavaShield =
{
    if(!AD_LavaShieldOn(p))
    {
        return FALSE;
    }
    ad_PlayLavaShieldSound(p);
    return TRUE;
};

// PlayerJump (self: the player, out of deep water), with AD's Jump Boots: AD's jumps (its JCR boots): from the ground
// Quake's jump (the map's jump speed in AD maps); in the air, while jumps are left (the boots' count, 1-4, or endless),
// a jump straight to the boots' height (360 u/s up) each press, with AD's sounds; falling off a ledge gives them all
// back; the boots' forward push (their `distance`) up to that speed along the view. TRUE: taken (Quake VR's jump not).
float() AD_JumpBoots =
{
    if(!(self.ad_moditems & ad_IT_ARTJUMPBOOTS))
    {
        return FALSE;
    }
    if(!(self.flags & FL_JUMPRELEASED))
    {
        return TRUE; // (a press a jump)
    }
    if(self.flags & FL_ONGROUND)
    {
        self.flags = self.flags - (self.flags & FL_JUMPRELEASED);
        self.flags = self.flags - FL_ONGROUND;
        self.button2 = 0;
        self.ad_jumpboots_sound = FALSE;
        self.ad_jumpboots_onground = FALSE;
        self.ad_jumpboots_airlvl = self.ad_jumpboots_airmax;
        sound(self, CHAN_BODY, ad_SOUND_ARTJUMPBOOTS3A, 1, ATTN_NORM);
        self.velocity_z = self.velocity_z + (ad_map_jumpheight > 0 ? ad_map_jumpheight : 270);
    }
    else if(self.ad_jumpboots_airmax == -1 || self.ad_jumpboots_airlvl >= 1 || self.ad_jumpboots_onground)
    {
        self.flags = self.flags - (self.flags & FL_JUMPRELEASED);
        self.button2 = 0;
        self.ad_jumpboots_sound = time + 0.4;
        if(self.ad_jumpboots_onground)
        {
            self.ad_jumpboots_airlvl = self.ad_jumpboots_airmax;
        }
        self.ad_jumpboots_onground = FALSE;
        self.ad_jumpboots_airlvl = self.ad_jumpboots_airlvl - 1;
        sound(self, CHAN_BODY, ad_SOUND_ARTJUMPBOOTS3B, 1, ATTN_NORM);
        self.velocity_z = self.ad_jumpboots_height > 0 ? self.ad_jumpboots_height : ad_ART_JUMPHEIGHT;
    }
    else
    {
        if(self.ad_jumpboots_sound < time)
        {
            self.ad_jumpboots_sound = time + 0.2;
            sound(self, CHAN_BODY, ad_SOUND_ARTJUMPBOOTS3C, 1, ATTN_NORM);
        }
        return TRUE;
    }
    if(self.ad_jumpboots_forward > 0)
    {
        vector flat = self.velocity;
        flat_z = 0;
        float speed = vlen(flat);
        if(speed < self.ad_jumpboots_forward)
        {
            makevectors(self.v_angle);
            self.velocity = self.velocity + normalize(v_forward) * (self.ad_jumpboots_forward - speed);
        }
    }
    return TRUE;
};

// ---------------------------------------------------------------------------------------------------------------------
// The artifacts' timers and the wrist gadget

// One artifact's time on player `p` (fields xFinished, xTime: AD's): its fading warning (a message, a sound and a flash
// 3 s before, a flash a second after) as AD's. TRUE: it has just run out. AD's endless ones (-1) never do.
float(entity p, .float xFinished, .float xTime, string xFade, string xMessage) AD_ArtifactFade =
{
    if(p.xFinished <= 0)
    {
        return FALSE;
    }
    if(p.xFinished < time + ad_PWR_FADING)
    {
        if(p.xTime == 1)
        {
            sprint(p, xMessage);
            stuffcmd(p, "bf\n");
            sound(p, CHAN_AUTO, xFade, 1, ATTN_NORM);
            p.xTime = time + 1;
        }
        if(p.xTime < time)
        {
            p.xTime = time + 1;
            stuffcmd(p, "bf\n");
        }
    }
    return p.xFinished < time;
};

// The wrist gadget's AD row (the engine's STAT_QVR_ADHUD, ADKEYS, ADTIME): the artifacts held (bits 0-5: Lava Shield,
// Air Tank, Blast Belt, Jump Boots, Sharp Shooter, Nail Piercer), the debuffs (6 burning, 7 poisoned, 8 bleeding), the
// custom keys (9-12); each custom key's HUD skin (4 bits each); the soonest artifact's seconds left (0: none ends).
.float vr_ad_hud, vr_ad_hudkeys, vr_ad_hudtime;
// The gadget's Arcane Dimensions page (its outer button pages to it; the engine's STAT_QVR_ADART1, ADART2, ADDEBUFF,
// ADUPGRADES): each artifact's seconds left, 8 bits each (255: no end; the Lava Shield, Air Tank, Blast Belt in ART1,
// the Jump Boots, Sharp Shooter, Nail Piercer in ART2); the debuffs' poison and bleeding still to come (damage, up to
// 255) and the burning's seconds left; the carried weapons' upgrades (AD_PackWeaponUpgrades: 3 bits a weapon, the hands'
// then the holsters'; 0 where the upgrades don't play).
.float vr_ad_hudart1, vr_ad_hudart2, vr_ad_huddebuff, vr_ad_hudup;

// An artifact's seconds left for the page: 0 none (not held), 255 held without an end (-1: AD's "for good").
float(float xFinished) AD_HudSeconds =
{
    if(xFinished == -1)
    {
        return 255;
    }
    return xFinished > time ? min(254, ceil(xFinished - time)) : 0;
};

// Whether player `p` burns: AD's burning is Quake VR's (decision 24: vr_ad_player.qc), his fire lit.
float(entity p) AD_Burning =
{
    return p.burn_fire != world && p.burn_fire.enemy == p && p.burn_fire.burn_end > time;
};

// The soonest of the timed artifacts' ends (AD_ArtifactsTick).
float vr_ad_soonest;

void(float xFinished) AD_HudSoonest =
{
    if(xFinished > time && (vr_ad_soonest == 0 || xFinished < vr_ad_soonest))
    {
        vr_ad_soonest = xFinished;
    }
};

// Player `p`'s HUD readings (AD_ArtifactsTick, every frame while AD's data is there).
void(entity p) AD_HudUpdate =
{
    float m = p.ad_moditems;
    float bits = 0;
    if(m & ad_IT_ARTLAVASHIELD) bits = bits | 1;
    if(m & ad_IT_ARTAIRTANK) bits = bits | 2;
    if(m & ad_IT_ARTBLASTBELT) bits = bits | 4;
    if(m & ad_IT_ARTJUMPBOOTS) bits = bits | 8;
    if(m & ad_IT_ARTSHARP) bits = bits | 16;
    if(m & ad_IT_ARTPIERCE) bits = bits | 32;
    if(AD_Burning(p)) bits = bits | 64;
    if(p.ad_poisoned) bits = bits | 128;
    if(p.ad_bleeding) bits = bits | 256;
    float keys = 0;
    if(m & ad_IT_CKEY1) { bits = bits | 512; keys = keys + (p.ad_ckeyskin1 & 15); }
    if(m & ad_IT_CKEY2) { bits = bits | 1024; keys = keys + (p.ad_ckeyskin2 & 15) * 16; }
    if(m & ad_IT_CKEY3) { bits = bits | 2048; keys = keys + (p.ad_ckeyskin3 & 15) * 256; }
    if(m & ad_IT_CKEY4) { bits = bits | 4096; keys = keys + (p.ad_ckeyskin4 & 15) * 4096; }
    p.vr_ad_hud = bits;
    p.vr_ad_hudkeys = keys;
    p.vr_ad_hudtime = vr_ad_soonest > time ? ceil(vr_ad_soonest - time) : 0;
    // The page's: each artifact's own time (only the held ones'), the debuffs' amounts, the weapons' upgrades.
    p.vr_ad_hudart1 = ((bits & 1) ? AD_HudSeconds(p.ad_lavashield_finished) : 0) +
                      ((bits & 2) ? AD_HudSeconds(p.ad_airtank_finished) : 0) * 256 +
                      ((bits & 4) ? AD_HudSeconds(p.ad_blastbelt_finished) : 0) * 65536;
    p.vr_ad_hudart2 = ((bits & 8) ? AD_HudSeconds(p.ad_jumpboots_finished) : 0) +
                      ((bits & 16) ? AD_HudSeconds(p.ad_sharpshoot_finished) : 0) * 256 +
                      ((bits & 32) ? AD_HudSeconds(p.ad_nailpiercer_finished) : 0) * 65536;
    float burn = AD_Burning(p) ? min(255, ceil(p.burn_fire.burn_end - time)) : 0;
    p.vr_ad_huddebuff = (p.ad_poisoned ? min(255, max(1, ceil(p.ad_poisoned_qty))) : 0) +
                        (p.ad_bleeding ? min(255, max(1, ceil(p.ad_bleeding_qty))) : 0) * 256 + burn * 65536;
    p.vr_ad_hudup = VR_AD_FeatureScope(AD_UpgradeScope()) ? AD_PackWeaponUpgrades(p) : 0;
};

// PlayerPostThink, in any map while AD's data is there (AD_PlayerPostThink): AD's new artifacts' time on player self,
// as AD's ClientPowerups for them (their warnings, their end; the Air Tank's air and breathing; nothing while dead, in
// an intermission or a cinematic), and the HUD's readings.
void() AD_ArtifactsTick =
{
    vr_ad_soonest = 0;
    if(self.health > 0 && !intermission_running && ad_cinematic_running <= 0)
    {
        if(AD_ArtifactFade(self, ad_lavashield_finished, ad_lavashield_time, ad_SOUND_ARTLSHIELD2, "Aegis is falling apart!\n"))
            ad_ResetPowerLavaShield(self, 0);
        if(self.ad_airtank_finished > 0 || self.ad_airtank_finished == -1)
        {
            self.air_finished = time + VR_AirSupply(); // (no drowning)
            if(self.ad_airtank_level != self.waterlevel)
            {
                if((self.ad_airtank_level > 2) != (self.waterlevel > 2)) self.ad_airtank_sound = 0;
                self.ad_airtank_level = self.waterlevel;
            }
            if(self.ad_airtank_sound < time && !self.ad_airtank_silent && self.ad_airtank_volume > 0)
            {
                if(self.waterlevel == 3)
                {
                    sound(self, CHAN_ITEM, ad_SOUND_ARTAIRTANK3, min(1, (0.5 + random() * 0.5) * self.ad_airtank_volume), ATTN_IDLE);
                    self.ad_airtank_sound = time + 4;
                }
                else if(!self.ad_airtank_outwater)
                {
                    sound(self, CHAN_ITEM, ad_SOUND_ARTAIRTANK3B, min(1, (0.3 + random() * 0.3) * self.ad_airtank_volume), ATTN_IDLE);
                    self.ad_airtank_sound = time + 3 + random();
                }
            }
            if(self.ad_airtank_finished > 0 && self.ad_airtank_finished < time + ad_PWR_FADING)
            {
                self.ad_airtank_sound = time + 4;
                self.ad_airtank_silent = FALSE;
            }
            if(AD_ArtifactFade(self, ad_airtank_finished, ad_airtank_time, ad_SOUND_ARTAIRTANK2, "Air supply in Tanks is expiring\n"))
                ad_ResetPowerAirTank(self, 0);
        }
        if(AD_ArtifactFade(self, ad_blastbelt_finished, ad_blastbelt_time, ad_SOUND_ARTBLASTBELT2, "Blast belt is failing\n"))
            ad_ResetPowerBlastbelt(self, 0);
        if(AD_ArtifactFade(self, ad_jumpboots_finished, ad_jumpboots_time, ad_SOUND_ARTJUMPBOOTS2, "Jump Boots are wearing off\n"))
            ad_ResetPowerJumpBoots(self, 0);
        if(AD_ArtifactFade(self, ad_sharpshoot_finished, ad_sharpshoot_time, ad_SOUND_ARTSHARP2, "Sharpshooter has almost gone\n"))
            ad_ResetPowerSharp(self, 0);
        if(AD_ArtifactFade(self, ad_nailpiercer_finished, ad_nailpiercer_time, ad_SOUND_ARTNAILP2, "Nail Piercer is wearing off\n"))
            ad_ResetPowerPiercer(self, 0);
        // (Boots on the ground: their jumps back once off it, a ledge walked off too; AD's PlayerPreThink.)
        if((self.ad_moditems & ad_IT_ARTJUMPBOOTS) && (self.flags & FL_ONGROUND))
        {
            self.ad_jumpboots_onground = TRUE;
        }
    }
    AD_HudSoonest(self.ad_lavashield_finished);
    AD_HudSoonest(self.ad_airtank_finished);
    AD_HudSoonest(self.ad_blastbelt_finished);
    AD_HudSoonest(self.ad_jumpboots_finished);
    AD_HudSoonest(self.ad_sharpshoot_finished);
    AD_HudSoonest(self.ad_nailpiercer_finished);
    AD_HudUpdate(self);
};

// AD support: touch buttons share Quake VR's hand, whole-gun, carried/thrown prop paths.
float(entity b) VR_AD_ButtonEnabled =
{
    return b.ad_classtype != ad_CT_FUNCBUTTON || !(b.ad_estate & ad_ESTATE_BLOCK);
};
float() VR_AD_ButtonFire =
{
    if(self.ad_classtype != ad_CT_FUNCBUTTON) return FALSE;
    ad_func_button_fire();
    vr_button_how = NULLSTR;
    return TRUE;
};
float() VR_AD_ButtonTouch =
{
    if(self.ad_classtype != ad_CT_FUNCBUTTON) return FALSE;
    if(other.classname != "player" || !VR_AD_ButtonEnabled(self) || self.state == STATE_UP || self.state == STATE_TOP) return TRUE;
    VRButtonPressHaptic(other);
    self.enemy = other;
    VR_AD_ButtonFire();
    return TRUE;
};
void() VR_AD_ButtonSpawned =
{
    self.handtouch = button_handtouch;
    self.vr_wpntouch = button_wpntouch;
    self.touch = button_touch_any;
};

// AD skins/inventory stay AD's; match vanilla bag dimensions/density, carrying and holster consumption.
void(entity bag) VR_AD_BackpackSetup =
{
    entity os = self;
    self = bag;
    vector lo = modelbounds(bag, 0), hi = modelbounds(bag, 1);
    entity ref = spawn();
    setmodel(ref, "progs/backpack.mdl");
    vector target = (modelbounds(ref, 1) - modelbounds(ref, 0)) * 0.75;
    remove(ref);
    vector size = hi - lo;
    vector scale;
    scale_x = target_x / max(1, size_x);
    scale_y = target_y / max(1, size_y);
    scale_z = target_z / max(1, size_z);
    VR_ScaleUtil_SetScale(bag, scale);
    bag.model_scale_origin = '0 0 0';
    bag.model_offset = -(hi + lo) * 0.5;
    setsize(bag, -target * 0.5, target * 0.5);
    bag.handtouch = ad_item_touch;
    MakeGrabbable(bag);
    bag.flags |= FL_FORCEGRABBABLE;
    bag.solid = SOLID_NOT_BUT_TOUCHABLE;
    bag.touch = forcegrabbable_touch;
    bag.owner = world;
    bag.vr_pickup_obj = QVR_PICKUPOBJ_HOLSTER;
    SetThrownWeaponDealDamage(bag, FALSE);
    SetThrownWeaponDisappearOnHit(bag, FALSE);
    VR_Carry_Setup(bag);
    bag.throwhit = QVR_THROWHIT_HIT;
    bag.ad_item_flrcheck = 0; // physics owns movement; AD's broad floor checks must not teleport a held bag
    bag.think = backpack_think;
    bag.nextthink = time + 0.02;
    if(bag.ad_item_expired > 0)
    {
        bag.think2 = SUB_Remove;
        bag.nextthink2 = bag.ad_item_expired;
    }
    self = os;
};
// AD's ad_item_restore (a reviewed patch, ad_import.py), each time one of AD's own items is placed or comes back: a
// backpack becomes a Quake VR object (above); AD's other pickups that run on AD's code (the artifacts, the Tome of Power,
// the custom keys and items) are taken by a hand too, as Quake VR's powerups are (their handtouch; AD's touch already
// takes them by walking into them). TRUE: a backpack (AD's own restore skipped).
float() VR_AD_BackpackRestore =
{
    if(self.ad_touch2 != ad_BackpackTouch)
    {
        if((self.flags & ad_FL_ITEM) && self.ad_classgroup != ad_CG_WEAPON && self.ad_touch2)
        {
            self.handtouch = ad_item_touch;
        }
        return FALSE;
    }
    VR_AD_BackpackSetup(self);
    return TRUE;
};

// The toolgun's live spawn of one of AD's items (QC VR_SpawnLive_Finish, vr_toolgun.qc): AD's own delayed start run now
// (a backpack's setup, then the start AD spreads over 0.1-0.6 s so that a map's items do not all drop at once), so
// it is drawn where it lands from its first frame, not half a second later.
void() AD_SpawnLive_Finish =
{
    VR_AD_LiveBossStart(); // (a scripted boss, the judicator's or the ice golem's: named and woken, vr_ad_monsters.qc)
    float n = 0;
    while(n < 3) // (a removed one: its think is cleared)
    {
        if(self.think == ad_item_backpack_setup) { ad_item_backpack_setup(); }
        else if((self.flags & ad_FL_ITEM) && self.ad_estate_use && self.think == self.ad_estate_use) { self.think(); }
        else { return; }
        n = n + 1;
    }
};

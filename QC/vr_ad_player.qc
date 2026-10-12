// Arcane Dimensions' player rules that travel with its content (docs/vr-port/ARCANE_DIMENSIONS.md, section 16): its
// debuffs in every map, its burning merged into Quake VR's (decision 24). After vr_ad_items.qc; the functions Quake
// VR's files call are declared in vr_ad_defs.qc, each call there marked "AD support".

// ---------------------------------------------------------------------------------------------------------------------
// Debuffs (decision 24: everywhere)
//
// AD's monsters and traps poison (spiders, scorpions, fumigators... AD's PoisonDeBuff), make bleed (scorpions,
// steelclaws, wraiths: BleedDeBuff, health taken at once and given back over time) and set on fire (pyros, wraiths, fire
// traps: BurnDeBuff) the player, in any map they are in. Poison and bleeding are AD's own, ticked here for Quake VR's
// player in every map while AD's data is there (AD's ClientDeBuff; its screen tints are VR_AD_Tint's, below; the wrist
// gadget shows them too). Burning is Quake VR's (vr_burning.qc): AD's BurnDeBuff (a reviewed patch) lights
// the player's fire for AD's time, which burns at AD's damage (a point a second, no armour) with our flames, burns,
// smoke, crackle, and goes out in water as ours.

.float vr_ad_burn;     // a fire (vr_burn) lit by AD's burning debuff: AD's damage (AD_BurnHurt)
.float vr_ad_burnnext; // its next point of damage

// PlayerPostThink, any map while AD's data is there (AD_PlayerPostThink): AD's poison and bleeding on player self
// (its burning is our fire's).
void() AD_DebuffsTick =
{
    ad_ClientDeBuff();
};

// ---------------------------------------------------------------------------------------------------------------------
// Screen tints (ARCANE_DIMENSIONS.md 16.14)
//
// AD's v_cshift, which AD stuffs into the client's console (its debuffs' colours, its books' dark background, its
// cinematic camera's fades), Quake VR's way: the player's fields below, which the engine sends as stats and draws as one
// more colour shift, eased, times VR Settings' Screen Tints (vr_ad_tints; the camera's fades are not scaled). Reviewed
// patches (ad_import.py) call it where AD stuffed the command; in a map, a save and a loaded game it is the player's.

.float vr_ad_tint;    // the colour: 65536 red + 256 green + blue, 0-255 each (the engine's STAT_QVR_ADTINT)
.float vr_ad_tintden; // the density 0-255, plus 256 for a cinematic camera's fade (STAT_QVR_ADTINTDEN)

// AD's "v_cshift r g b density" for `targ` (a fade: the cinematic camera's). TRUE: done (AD's stuffcmd not sent).
float(entity targ, vector colour, float density, float fade) VR_AD_Tint =
{
    if(!(targ.flags & FL_CLIENT))
    {
        return TRUE;
    }
    float r = rint(min(255, max(0, colour_x))), g = rint(min(255, max(0, colour_y))), b = rint(min(255, max(0, colour_z)));
    targ.vr_ad_tint = r * 65536 + g * 256 + b;
    targ.vr_ad_tintden = rint(min(255, max(0, density))) + (fade ? 256 : 0);
    return TRUE;
};

// AD's BurnDeBuff on `targ` (the reviewed patch): Quake VR's fire on the player for AD's time: AD's own rules for who
// burns (a player out of deep water, without the pentagram or the biosuit), AD's time (3-5 s plus 5 a skill level,
// added to what is left, at most 10), AD's damage (AD_BurnHurt). TRUE: done (AD's own debuff not started). FALSE (AD's
// own then): our burning off (Burn Damage 0).
float(entity targ) VR_AD_BurnDebuff =
{
    if(!(targ.flags & FL_CLIENT))
    {
        return FALSE;
    }
    if(targ.waterlevel >= 3 || (targ.items & (IT_INVULNERABILITY | IT_SUIT)) || targ.health <= 0)
    {
        return TRUE;
    }
    float left = AD_Burning(targ) ? max(0, targ.burn_fire.burn_end - time) : 0;
    if(left >= ad_DEBUFF_BURN_MAX)
    {
        return TRUE;
    }
    float len = min(ad_DEBUFF_BURN_MAX, left + rint(5 * ad_skill + 3 + 2 * random()));
    float a = random() * 360;
    vector out = '0 0 0';
    out_x = cos(a * (3.14159265 / 180));
    out_y = sin(a * (3.14159265 / 180));
    vector at = targ.origin + out * 8;
    at_z = targ.origin_z + random() * 16 - 8;
    vr_burn_part = 0;
    vr_burn_out = out;
    vr_burn_len = len;
    VR_Burn_Ignite(targ, self, at, out * -1, VR_BURN_TOUCH);
    vr_burn_len = 0;
    vr_burn_part = 0;
    vr_burn_out = '0 0 0';
    if(!AD_Burning(targ))
    {
        return FALSE;
    }
    if(!targ.burn_fire.vr_ad_burn)
    {
        targ.burn_fire.vr_ad_burn = TRUE;
        targ.burn_fire.vr_ad_burnnext = time;
    }
    dprint(sprintf("burning: AD's burning on %s: our fire for %g s (AD's damage)\n", targ.classname, len));
    return TRUE;
};

// VR_Burn_Think's damage tick (every 0.5 s) of fire `xFire` on `xVictim`: lit by AD's burning, AD's damage (a point a
// second, no armour, as AD's NOARMOR). TRUE: dealt (ours not).
float(entity xFire, entity xVictim) AD_BurnHurt =
{
    if(!xFire.vr_ad_burn)
    {
        return FALSE;
    }
    if(time >= xFire.vr_ad_burnnext && xVictim.takedamage && xVictim.health > 0)
    {
        xFire.vr_ad_burnnext = time + 1;
        float type = xVictim.armortype;
        xVictim.armortype = 0;
        VR_SetHitKind(QVR_HITKIND_NONE, -1, xFire.burn_flames.origin, '0 0 -1');
        T_Damage(xVictim, xFire, xFire.owner, ad_DEBUFF_BURN_DMG);
        VR_ClearHitKind();
        if(xVictim.armortype == 0)
        {
            xVictim.armortype = type;
        }
    }
    return TRUE;
};

// ---------------------------------------------------------------------------------------------------------------------
// Ammo resistance (decision 18)
//
// AD's monsters resist kinds of ammunition (their resist_shells/nails/rockets/cells: 0 none, 1 immune, below 0 extra
// damage), with a visual tell. AD's weapons and monsters apply it in their own code; here Quake VR's weapons (and its
// monsters' missiles) do too, in AD maps (on any target with the keys: AD's monsters, ours given them by a mapper) and,
// elsewhere, on AD's own monsters only. The kind is the inflictor's: shotgun pellets (hitscan) shells; nails nails;
// rockets, grenades (thrown or launched) and any blast rockets; lightning, plasma and lasers cells. Melee (fists, the
// axe, guns used as clubs, thrown weapons, a stuck axe's bleeding) none: AD's axe ignores it too.

// The kind of ammunition (AD_AMMO_*; 0 none) of the damage now dealt by `inflictor` for `attacker`.
float(entity inflictor, entity attacker, float isVRMelee) AD_AmmoOf =
{
    if(vr_ad_ammo)
    {
        return vr_ad_ammo;
    }
    if(isVRMelee || vr_hitkind == QVR_HITKIND_MELEE || vr_hitkind == QVR_HITKIND_HEADBUTT || vr_hitkind == QVR_HITKIND_BASH ||
       vr_hitkind == QVR_HITKIND_SHOVE || vr_hitkind == QVR_HITKIND_BLEED)
    {
        return 0;
    }
    if(discharged || vr_hitweapon == "Mjolnir's lightning")
    {
        return AD_AMMO_CELLS;
    }
    if(vr_hitkind == QVR_HITKIND_SHOT)
    {
        return AD_AMMO_SHELLS; // (lightning tags itself: VR_LightningHit)
    }
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        return AD_AMMO_ROCKETS;
    }
    string c = inflictor.classname;
    if(c == "spike" || c == "lava_spike" || c == "knightspike" || c == "wizspike")
    {
        return AD_AMMO_NAILS;
    }
    if(c == "missile" || c == "grenade" || c == "MultiGrenade" || c == "MiniGrenade" || c == "MultiRocket" ||
       c == "proximity_grenade" || c == "ogre_grenade" || c == "ogre_missile")
    {
        return AD_AMMO_ROCKETS;
    }
    if(c == "plasma" || c == "laser" || c == "hiplaser" || c == "rifle_laser" || c == "enforcer_laser")
    {
        return AD_AMMO_CELLS;
    }
    return 0;
};

// Whether `targ` resists by AD's rules here: in an AD map, anything with the keys; elsewhere AD's own entities.
float(entity targ) AD_Resists =
{
    if(!VR_AD_FeatureScope(cvar("vr_ad_resistance"))) return FALSE;
    if(!targ.ad_resist_shells && !targ.ad_resist_nails && !targ.ad_resist_rockets && !targ.ad_resist_cells)
    {
        return FALSE;
    }
    return AD_Rules() || AD_Run(targ);
};

// Outside AD maps, before a resist tell: what AD's tells use that AD's world precaches in its own maps (the smoke
// burst's sprite and sound, the smoke and flesh chunks). The monster's pain sound and blood are its own. A precache
// already made is found at once; a new one is late (the engine allows it), once a map (and after a loaded game).
void() AD_ResistTellsPrecache =
{
    precache_model(ad_SBURST_SMOKE);
    precache_model(ad_MODEL_PROJ_SMOKE);
    precache_model(ad_MODEL_PROJ_FLESH);
    precache_model(ad_MODEL_PROJ_FLESHP);
    precache_sound(ad_SOUND_RESIST_ROCKET);
};

// T_DamageImpl: `damage` after AD's ammo resistance of `targ` (as AD's Resist_Damage), with AD's tell (sparks and
// smoke, a pain sound) wherever it resists (decision 13: AD maps, and AD's monsters in any map). Not twice: AD's own
// code resisted what it deals to ours (vr_ad_ourdamage).
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) AD_Resist =
{
    if(vr_ad_ourdamage || !AD_Resists(targ))
    {
        return damage;
    }
    float ammo = AD_AmmoOf(inflictor, attacker, isVRMelee);
    float r = ammo == AD_AMMO_SHELLS ? targ.ad_resist_shells : ammo == AD_AMMO_NAILS ? targ.ad_resist_nails :
              ammo == AD_AMMO_ROCKETS ? targ.ad_resist_rockets : ammo == AD_AMMO_CELLS ? targ.ad_resist_cells : 0;
    if(r == 0)
    {
        return damage;
    }
    float left = damage * (1 - r);
    if(r > 0)
    {
        if(!AD_Rules())
        {
            AD_ResistTellsPrecache();
        }
        vector at = vr_hitpos != '0 0 0' ? vr_hitpos : inflictor != attacker ? inflictor.origin : (targ.absmin + targ.absmax) * 0.5;
        entity oself = self;
        self = attacker;
        if(ammo == AD_AMMO_SHELLS) ad_Resist_Shells(targ, at, vr_hitdir * 300, left);
        else if(ammo == AD_AMMO_NAILS) ad_Resist_Nails(targ, at);
        else if(ammo == AD_AMMO_ROCKETS) ad_Resist_Rockets(targ, at);
        else ad_Resist_Lightning(targ, at);
        self = oself;
    }
    if(cvar("developer"))
    {
        dprint(sprintf("AD resistance: %s takes %g of %g (ammo %g, resists %g)\n", targ.classname, left, damage, ammo, r));
    }
    return left;
};

// ---------------------------------------------------------------------------------------------------------------------
// Ladders (decision 26: working nicely with Quake VR's hand climbing)
//
// AD's trigger_ladder (in any map while its data is there) keeps AD's own climb (holding jump climbs at the ladder's
// speed: AD_PlayerPreThink) and is a climbable volume for Quake VR's hands (the engine's vr_climb.cpp ladderHold,
// vr_climb_ladders): an empty hand gripping in it (or touching it) holds where it is, a rung under every hand, and the
// body hangs and is pulled up hand over hand as on ledges, mantling at the top where there is room. While AD's own
// state turns the ladder off (not solid), no hand holds on it.

// AD's trigger_ladder's spawn's end (a reviewed patch).
void() VR_AD_LadderSpawned =
{
    self.vr_climb_ladder = 1;
};

// ---------------------------------------------------------------------------------------------------------------------
// Crouching (decision 20)
//
// AD's monsters are height-aware: they look for their enemy at its eyes (origin + view_ofs: its sight and attack
// checks, visible/visblocked_wide), lob grenades at its height (its Z-aware arcs) and melee only within 64 units of its
// height. Quake VR's crouch is the player's real head going down and a lower box (the engine's crouched hull), with his
// origin and view_ofs as a standing player's; so AD's sight (reviewed patches) and ours (ai.qc visible, every mode:
// vr_crouch_sight) look for a crouched player at his real eyes: behind cover lower than he stands he is out of sight.
// AD's aim at his origin (his box's middle) and its melee height check already fit the crouched box (its top 36 units
// over the feet, the origin 24); our monsters' bullets aim at what shows of him (vr_crates.qc VR_Crate_ShotAim).

// AD's sight's eyes of `e` (Quake VR's: VR_SightOfs).
vector(entity e) VR_AD_ViewOfs =
{
    return VR_SightOfs(e);
};

// ---------------------------------------------------------------------------------------------------------------------
// Evil (decision 30: in the skill choice, in any campaign while AD's data is installed)
//
// AD's fifth skill, Evil (its skill 4), is Quake VR's Nightmare (skill 3: the engine and Quake VR's monsters know 0-3)
// with vr_ad_evil: chosen in the menu (VR Settings > Combat > Arcane Dimensions: Evil Skill) with the start map's
// Nightmare, or by AD's own skill choices (its hub's Evil: the reviewed patches call VR_AD_SetSkill). It plays AD's
// Evil rules for Quake VR's player in any map: 50 health at most (VR_BaseHealth, VR_MaxHealth), megahealth to 100
// (VR_MegaHealthCap), the Shadow Axe's blows (AD_AxeScale); AD's code reads its skill 4 (its monsters' hardest). A
// game's choice like the skill: saved with the game (vr_ad_evil_on), a loaded game's restored.

float() AD_Evil =
{
    return vr_ad_evil_on && vr_ad_data && skill >= 3 && !deathmatch;
};

// AD_WorldSpawn: this level plays the choice (vr_ad_evil), AD's own flag (scratch4) agreeing, as AD's code reads it.
void() AD_EvilStart =
{
    vr_ad_evil_on = cvar("vr_ad_evil") != 0;
    cvar_set("scratch4", vr_ad_evil_on ? "1" : "0");
};

// The players' health capacity after Evil came or went (AD's SetupEvilmode / ResetEvilmode): at most it.
void() AD_EvilHealth =
{
    for(entity p = find(world, classname, "player"); p; p = find(p, classname, "player"))
    {
        p.max_health = VR_MaxHealth(p);
        if(p.health > p.max_health)
        {
            p.health = p.max_health;
        }
    }
};

// OnLoadGame (its globals read): the loaded game's choice is the cvar's again, for its next levels.
void() AD_LoadedGame =
{
    cvar_set("vr_ad_evil", vr_ad_evil_on ? "1" : "0");
    cvar_set("scratch4", vr_ad_evil_on ? "1" : "0");
};

// AD_Frame: a change of the choice mid-game played now; AD's code at its skill 4 while Evil plays.
void() AD_EvilFrame =
{
    float chosen = cvar("vr_ad_evil") != 0;
    if(chosen != vr_ad_evil_on)
    {
        vr_ad_evil_on = chosen;
        cvar_set("scratch4", chosen ? "1" : "0");
        AD_EvilHealth();
        dprint(sprintf("VR: Arcane Dimensions' Evil skill %s (skill %g)\n", chosen ? "on" : "off", skill));
    }
    if(AD_Evil())
    {
        ad_skill = ad_SKILL_EVIL;
    }
};

// AD's skill choices (its hub's, trigger_setskill, its Evil flag; reviewed patches): skill 4 (Evil) is Nightmare with
// vr_ad_evil; 0-3 those skills, Evil off.
void(float s) VR_AD_SetSkill =
{
    s = rint(s);
    cvar_set("skill", ftos(max(0, min(s, 3))));
    cvar_set("vr_ad_evil", s >= ad_SKILL_EVIL ? "1" : "0");
    cvar_set("scratch4", s >= ad_SKILL_EVIL ? "1" : "0");
};

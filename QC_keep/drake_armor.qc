// armor.qc

//==========================================================================
//  Checks

// Note:  Arf = ARmor Float, for the functions that use a float input.

// Checks if entity wears a custom armor.
float(float exotic) Arf_Exotic =
{
    if (exotic > SUIT_ULTRA)
        return 0;   // Undefined.
    return exotic;
};

// Checks if armor has infinite durability.
float(float exotic) Arf_Infinite =
{
    return ( (exotic == SUIT_ONYX) || (exotic == SUIT_ONYX_POWER)
        || (exotic == SUIT_BERSERKER));
};

// Checks if armor is indestructible.  What this means is the armor stays
// even if durability gets reduced to 0.  Onyx armor has no durability
// (infinite), spirit armor can reform out of nothing, and chaos armor
// stays until used.
float(float exotic) Arf_Indestructible =
{
    if (exotic == SUIT_SPIRIT)
        return TRUE;    // Powered by intangible spirits!
    if (exotic == SUIT_CHAOS)
        return TRUE;    // Indestructible until teleportation is used.
    if (exotic == SUIT_ULTRA)
        return TRUE;    // Don't remove ultra enforcers' armor.
    if (Arf_Infinite (exotic))
        return TRUE;    // Infinite durability.
    return FALSE;
};

// Checks if armor can/not be repaired.  Armor that cannot be repaired
// include infinite armors (don't need repairs) and the spirit armor
// (protection granted by intangible forces).
float(float exotic) Arf_NoPatch =
{
    if (exotic == SUIT_SPIRIT)
        return TRUE;    // Powered by intangible spirits!
    if (exotic == SUIT_ULTRA)
        return TRUE;    // Force field armor.
    if (Arf_Infinite (exotic))
        return TRUE;
    return FALSE;
};

// Checks if armor has a usable power.
float(float exotic) Arf_SuperPower =
{
    return ( (exotic == SUIT_DRAGON) || (exotic == SUIT_ONYX_POWER)
        || (exotic == SUIT_BERSERKER) || (exotic == SUIT_SPIRIT)
        || (exotic == SUIT_CHAOS) );
};

//- - - - - - - - -
float(entity ent) Armor_Exotic =
    {return  Arf_Exotic ((ent.perms & POWx_ARMOR) / POW_ARMOR1);};

float(entity ent) Armor_Infinite =
    {return  Arf_Infinite ((ent.perms & POWx_ARMOR) / POW_ARMOR1);};

float(entity ent) Armor_Indestructible =
    {return  Arf_Indestructible ((ent.perms & POWx_ARMOR) / POW_ARMOR1);};

float(entity ent) Armor_NoPatch =
    {return  Arf_NoPatch ((ent.perms & POWx_ARMOR) / POW_ARMOR1);};

// Checks if armor allows the player to morph into something else.
float(entity ent) Armor_Morphic =
{
// Currently, only the Dragonscale armor qualifies.
    return ( (ent.perms & POWx_ARMOR) / POW_ARMOR1 == SUIT_DRAGON );
};

// Returns current armorvalue for an entity's chaos armor.
float(entity ent) Armor_ChaosRating =
{
    if (ent.health > 0)
        if (ent.armorvalue > 0)
            return ent.armorvalue / (ent.armorvalue + ent.health);
    return 0;
};

// Returns a rating based on armor stats.  The higher it is the better.
// The old 'type*value' doesn't cut it when infinite armor gets involved.
float(float hp, float type, float value, float exotic) Defense_Rating =
{
    local   float   full, combo, inf;

    inf = FALSE;
    exotic = exotic & SUITx_ALL;    //(ent.perms & POWx_ARMOR) / POW_ARMOR1;
    if (exotic)
    {
        if (exotic == SUIT_DRAGON)
            type = 1;   // Morphing into monster uses armor as health.
        else if (exotic == SUIT_BERSERKER)
            {type = -0.3;  value = 0;}
        else if (exotic == SUIT_CHAOS)
            type = value / (value + hp);

        inf = Arf_Infinite (exotic);
    }

    if (!type)
        return 0;

    if (type < 1)
        full = hp / (1 - type);
    else
        full = INFINITY;

    if (inf)
        return full;

    combo = hp + value;
    if (full > combo)
        full = combo;
    return full;
};


//==========================================================================
//  Power Shield

//- - - - - - - - -
// How power armor worked in Quake2...
// + Power armor is applied before regular armor.
// - Power Screen
//   * Protected against frontal attacks only.
//   * Absorbed up to 33% incoming damage.
//   * Consumes 1 cell per 1 damage absorbed.
// - Power Shield
//   * Protected against all attacks that do not bypass armor.
//   * Absorbed up to 66% incoming damage.
//   * Consumes 1 cell per 2 damage absorbed!
//
// The power shield in Quake2 is overpowered because it made players
// very hard to kill, especially if the player wore red armor too.
//
// Drake's custom power armor is DoomRL's Onyx armor and Quake2's
// power shield rolled into one.  Here is how it works...
// * Protects against all attacks that do not bypass armor.
// * Absorbs half incoming damage
// * Consumes 1 cell per 2 damage absorbed.
// * Remaining damage (now 50%) is reduced by half again (as onyx armor).
//  Onyx armor plus power shield provide up to 75% damage reduction.
//  Not quite, but close to red armor class, and much sturdier.
//- - - - - - - - -

// Updates armorvalue based on power armor status.
void(entity ent) Update_PowerArmor =
{
    if (Armor_Exotic(ent) == SUIT_ONYX_POWER)
    {
        if (ent.perms & POW_POWER_ARMOR)
        {
            ent.armorvalue = ent.ammo_cells;
            if (ent.flags & FL_CLIENT)
                if (ent.items & IT_CELLS)
                    ent.currentammo = ent.ammo_cells;
            if (ent.ammo_cells < 1)
            {
                sound (self, CHAN_AUTO, "me/me_soff.wav", 1, ATTN_NORM);
                ent.perms = ent.perms - POW_POWER_ARMOR;
            }
        }
        else
            ent.armorvalue = 0;
    }
};

// Sets the power shield switch.
void(entity ent, float turn_on) PowerArmor_Switch =
{
    local   float   was_on;

    was_on = ent.perms & POW_POWER_ARMOR;
    if (turn_on)
    {   if (!was_on)
        {   ent.perms = ent.perms | POW_POWER_ARMOR;
            sound (ent, CHAN_AUTO, "me/me_son.wav", 1, ATTN_NORM);
        }
    }
    else
    {   if (was_on)
        {   ent.perms = ent.perms - POW_POWER_ARMOR;
            sound (ent, CHAN_AUTO, "me/me_soff.wav", 1, ATTN_NORM);
        }
    }
};

// Returns the damage absorbed by power armor.
// Called by 'T_Damage' in combat.qc.
float(entity targ, float damage, float drain) PowerArmor_Absorb =
{
    local   float   save, fuel;

    save = 0;
    if (targ.perms & POW_POWER_ARMOR)
    {
        save = ceil (damage / 2);   // Absorb half damage, round up.
        fuel = floor(targ.ammo_cells * DAMAGE_PER_CELL);
        if (save > fuel)
            save = fuel;
        if (drain)
        {
            targ.ammo_cells = targ.ammo_cells - ceil(save / DAMAGE_PER_CELL);
            Update_PowerArmor (targ);
            Particle_Cube (targ, 241, 241);
            // Was "weapons/lashit.wav", but I need to recycle sounds to save slots.
            sound (self, CHAN_AUTO, "weapons/zap1.wav", 1, ATTN_NORM);
        }
        // ELSE... We only wanted to crunch numbers without affecting stuff.
    }
    return save;
};


//==========================================================================
//  Suit Up

// Gives instructions to player.  Called on first pickup of usable armor.
void(entity ent) Armor_Awaken =
{
    if ((ent.flags & FLx_CREATURE) == FL_CLIENT)
        if (!(ent.perms & POW_HINT_ARMOR))
        {   // First time taking special armor?
            ent.perms = ent.perms | POW_HINT_ARMOR;
            sound (ent, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
            centerprint (ent, "Double-press JUMP to use armor.");
        }
};

// Sets an entity's armor stats.
void(entity ent, vector vstat) Armor_Set =
{
// Local vars defined for readability's sake.
    local   float   type, value, bit, exotic;

    type    = vstat_x;
    value   = vstat_y;
    bit     = vstat_z & ITx_ALL_ARMOR;
    exotic  = vstat_z & SUITx_ALL;

    ent.armortype = type;
    ent.armorvalue = value;
    ent.items = (ent.items - (ent.items & ITx_ALL_ARMOR)) | bit;
    ent.perms = (ent.perms - (ent.perms & POWx_ARMOR)) | (exotic * POW_ARMOR1);

// Special effects caused by exotic armor.
    if (Arf_SuperPower (exotic))
        Armor_Awaken (ent);     // Dragonscale or power armor.
    if (exotic != SUIT_DRAGON)
    {   // Lost morphing power, so change back to human.
        if (ent.form_active)
            War_Transform (ent);    // This should set baha_time to 0.
    }
    if (exotic == SUIT_ONYX_POWER)
    {   // Give player free large cells to fuel the armor.
        Update_Cells (ent, ent.ammo_cells + LARGE_CELLS);
    }
    else
    {   // No more power armor, so switch it off if necessary.
        PowerArmor_Switch (ent, FALSE);
    }
    Regen_ClientReset (ent);    // Demonhide affects regeneration.
};

// Strips armor from an entity.
void(entity ent) Armor_Remove = {Armor_Set (ent, '0 0 0');};

// PM:  This tries to put armor on 'ent'.  Returns non-zero if successful.
float(entity ent, vector vstat) Armor_Add =
{
// Local vars defined for readability's sake.
    local   float   type, value, bit, exotic, rating;
    local   float   ar, px;

    px = Armor_Exotic (ent);
    if ((px == SUIT_BERSERKER) || (px == SUIT_ULTRA))
        return 0;       // Wearing cursed armor that cannot be removed.

// Now to see if the new armor is worth taking.
    type    = vstat_x;
    value   = vstat_y;
    bit     = vstat_z & ITx_ALL_ARMOR;
    exotic  = vstat_z & SUITx_ALL;

    if (exotic && (exotic != SUIT_ONYX) && (exotic != SUIT_COURAGE))
    {
        if (px == exotic)
            if (ent.armorvalue >= value)
                return 0;   // Already wearing same armor type that's better.

        // Still here?  Take new armor for its special power at least.
    }
    else
    {   // Basic armor, or exotic armor with minor powers (courage/onyx).
        if (exotic == SUIT_ONYX)
            if (px == SUIT_ONYX_POWER)
                return 0;   // Don't replace power armor with shieldless.

        rating = Defense_Rating (ent.health, type, value, exotic);
        ar = Defense_Rating (ent.health, ent.armortype, ent.armorvalue, px);

        if (ar > rating)
            return 0;   // Current armor is still better than new armor.
        if (ar == rating)
        {
            if (ent.armortype*ent.armorvalue >= type*value)
                return 0;   // Current armor is still better than new armor.
            if (Arf_Indestructible (px))
                if (!Arf_Indestructible (exotic))
                    return 0;   // Don't trash current indestructible armor.
        }
    }

// Armor pickup confirmed!
    Armor_Set (ent, vstat);
    return 1;
};


//==========================================================================
//  Client Armor Use

// Checks if 'who' can use chaos armor passively.
// Doing so destroys the armor, and reduces current health by half.
// This is usually called by things that would instakill otherwise,
// Similar to the powered up firemace shots in Heretic.
float(entity who) Use_ChaosArmor =
{
    if (Armor_Exotic (who) == SUIT_CHAOS)
    {   // Auto use chaos device.
        if (Use_ChaosDevice (who))
        {   // Saved!
            who.hit_points = who.health = ceil(who.health / 2);
            Armor_Remove (who);
            return TRUE;
        }
    }
    return FALSE;
};

// Lets player attempt to activate armor.
void() W_UseArmor =
{
    local   float   exotic;

    if (cutscene)
        return;     // Ignore if in cutscene so messages won't be displayed.

    // Update:  No messages if armor serverflag is off.
    exotic = Armor_Exotic (self);
    if (exotic == SUIT_DRAGON)
    {   // The first armor with usable powers in Drake.
        if (mindex_dragon)
        {
            //- - - - -
            // Morphing to dragon form always sets ammo to max.  To prevent
            // recharge shortcuts via double morph, require ammo to fully
            // recharge before allowing player to morph out of dragon form.
            //- - - - -
            if (self.form_active)
            {
                if (self.form_active == FORM_DRAGON)
                    if (self.baha_ammo < MAX_BAHA_AMMO)
                        {sprint (self, "not at full power\n");  return;}
            }
            if (Transform_Ok (self))
            {
                War_Transform (self);
                // PM:  See the sprint messages below?  Ripped from Nethack!
                if (self.form_active)
                {   //sprint (self, "you merge with your scaly armor\n");
                    sprint (self, "you turn into a dragon\n");
                }
                else
                {   //sprint (self, "your skin returns to its original form\n");
                    sprint (self, "you return to human form\n");
                }
                // Enforce a cool down period to prevent spamming.
                // Assumes armor use was triggered by impulse or double jump.
                self.b2_done = self.attack_finished = time + 0.5;
            }
            else
            {
                if (!self.form_active)
                    self.baha_time = 0;     // Override automorph.
                sprint (self, "not enough room\n");
            }
        }
        else
        {   // The files were not loaded, so it fails!
            sprint (self, "nothing happens\n");
        }
        return;
    }
    if (exotic == SUIT_ONYX_POWER)
    {
        if (self.ammo_cells < 1)
            sprint (self, "no power\n");
        else
        {   // Toggle power shield.
            if (self.perms & POW_POWER_ARMOR)
                PowerArmor_Switch (self, FALSE);  // Off
            else
                PowerArmor_Switch (self, TRUE);   // On
            Update_PowerArmor (self);
        }
        return;
    }
    if (exotic == SUIT_BERSERKER)
    {
        if (self.armorvalue < ARMOR_STATS_DEMON_y / 2)
            sprint (self, "not enough power\n");
        else if (self.super_damage_finished)
            sprint (self, "nothing happens\n");
        else
        {   // Rage!
            sprint (self, "you feel like a killing machine!\n");
            exotic = self.armorvalue / 10;
            // damage3.wav is similar to damage.wav, and damage3.wav
            // is always precached, so use that for our short buff.
            sound (self, CHAN_VOICE, "items/damage3.wav", 1, ATTN_NORM);
            self.super_damage_finished = time + exotic;
            self.super_time = self.super_damage_finished - 3;
            self.items = self.items | IT_QUAD;
            self.armorvalue = 0;    // Used up the power.
        }
        return;
    }
    if (exotic == SUIT_SPIRIT)
    {
        if (self.armorvalue < ARMOR_STATS_SPIRIT_y)     //10
            sprint (self, "not at full power\n");       //"not enough power\n"
        else if (self.invisible_finished)   // Don't use if already invisible.
            sprint (self, "nothing happens\n");
        else
        {   // Poof!
            sprint (self, "you vanish in a puff of smoke\n");
            spawn_tfog (self.origin);
            self.invisible_finished = time + 10;    //time + (self.armorvalue / 2);
            self.invisible_time = self.invisible_finished - 3;
            self.items = self.items | IT_INVISO;
            self.armorvalue = 0;
        }
        return;
    }
    if (exotic == SUIT_CHAOS)
    {   // Activate teleportation, then destroy armor.
        if (Use_ChaosDevice (self))
            Armor_Remove (self);
        else
            sprint (self, "nothing happens\n");
        return;
    }

    if (self.perms & POW_HINT_ARMOR)
    {   if (!self.armorvalue && !exotic)    // Onyx has 0 value.
            sprint (self, "no armor\n");
        else
            sprint (self, "no effect\n");
    }
};


//===========================/  END OF FILE  /===========================//

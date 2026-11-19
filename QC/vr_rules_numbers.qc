// vr_rules_numbers.qc -- the rulesets' numbers (DWELL.md S6): one row a ruleset, read by our own pickup and monster
// code (items.qc's touches, monsters.qc's start through vr_rules_monsters.qc, the enforcer's laser). A ruleset whose
// numbers differ from Quake's (Copper's) is a row here, not a branch in each touch. Rows in VR_RULES_* order (ID, MG,
// AD, COPPER); MG's and AD's are Quake's (AD's own pickups route to ours, its megahealth rot is AD's: AD_MegaRot).
// Campaign overrides stay where they were (VR_MegaHealthCap: MG3's 500, AD's Evil skill; VR_MaxHealth).

typedef struct
{
    float armorGreen, armorYellow, armorRed; // the protection (armortype) of item_armor1, item_armor2, item_armorInv
    float armorBreaks;  // 1: Copper's pickup rule (a weaker armour refused over 40/60/100, part of a bigger store kept
                        // when a stronger one is taken); 0: Quake's (taken when type x value is more than yours)
    float armorMax;     // the most armour a pickup leaves you (0: no cap but the pickup's own value)
    float rotOne;       // 1: one rot a player, from 5 s after the last megahealth, a point a second down to the
                        // maximum; 0: Quake's, one rot a megahealth (two rot twice as fast)
    float keyStack;     // keys of one colour a player carries (Copper's 16, each door taking one; Quake's 1)
    float hpArmy, hpDog, hpFish, hpKnight; // our vanilla monsters' health, where it differs by ruleset
    float laserDmg;     // an enforcer's laser
} vr_rules_nums;

vr_rules_nums VR_RN[] =
{
    // green yellow red  breaks max  rotOne keys  army dog fish knight laser
    { 0.3,  0.6,   0.8,  0,     0,   0,     1,    30,  25, 25,  75,    15 }, // ID (Hipnotic's, Rogue's)
    { 0.3,  0.6,   0.8,  0,     0,   0,     1,    30,  25, 25,  75,    15 }, // MG
    { 0.3,  0.6,   0.8,  0,     0,   0,     1,    30,  25, 25,  75,    15 }, // AD
    { 0.3,  0.5,   0.7,  1,     500, 1,     16,   35,  35, 35,  72,    12 }, // COPPER (its item_health_armor.qc,
                                                                            // item_keys_runes.qc, m_*.qc)
};

// The map's row (a ruleset past the table's rows plays Quake's).
float() VR_RN_Row =
{
    float r = VR_Ruleset();
    return (r >= 0 && r <= VR_RULES_COPPER) ? r : VR_RULES_ID;
};

// ---------------------------------------------------------------------------------------------------------------------
// Armour

// The protection of armour tier `xTier` (0 green, 1 yellow, 2 red) under the map's ruleset.
float(float xTier) VR_RN_ArmorType =
{
    vr_rules_nums n = VR_RN[VR_RN_Row()];
    return xTier == 2 ? n.armorRed : xTier == 1 ? n.armorYellow : n.armorGreen;
};

// The armour `xPlayer` would wear after taking `xValue` points of protection `xType`: 0 when he would not take it
// (armor_touch, and VR_Armor_IsBetter for armour worn by hand).
float(entity xPlayer, float xType, float xValue) VR_RN_ArmorTake =
{
    vr_rules_nums n = VR_RN[VR_RN_Row()];
    float have = xPlayer.armorvalue;
    float type = have > 0 ? xPlayer.armortype : 0;
    if(!n.armorBreaks)
    {
        return ((type * have) >= (xType * xValue)) ? 0 : xValue;
    }
    // Copper's armor_give: the breakpoints round Quake's type x value break-evens (42, 60, 107) to 40, 60, 100.
    if(xType < type)
    {
        if(xType == n.armorGreen && type == n.armorRed && have > 40) return 0;
        if(xType == n.armorGreen && type == n.armorYellow && have > 60) return 0;
        if(xType == n.armorYellow && type == n.armorRed && have > 100) return 0;
    }
    else if(xType == type)
    {
        if(xValue <= have) return 0;
    }
    else if(xValue < have)
    {
        // a stronger armour over a bigger store of a weaker one (backpacks' armour): a part of the excess kept
        xValue = (xType == n.armorRed && type == n.armorGreen) ? floor((xValue * 3 + have) / 4) : floor((xValue + have) / 2);
    }
    return n.armorMax > 0 ? min(xValue, n.armorMax) : xValue;
};

// ---------------------------------------------------------------------------------------------------------------------
// Megahealth rot, one a player (Copper's rule)

.float vr_rn_rot;       // when the player's one rot ticks next (0: not rotting)
void(entity e, float iid) VR_ItemUtil_EntAddItem;
void(entity e, float iid) VR_ItemUtil_EntDelItem;

// health_touch, a megahealth taken: under a ruleset with one rot a player, it starts (or restarts) 5 s from now.
// TRUE: taken (the megahealth's own rot not started).
float(entity xPlayer) VR_RN_MegaRot =
{
    if(!VR_RN[VR_RN_Row()].rotOne)
    {
        return FALSE;
    }
    xPlayer.vr_rn_rot = time + 5;
    VR_ItemUtil_EntAddItem(xPlayer, IID_SUPERHEALTH);
    return TRUE;
};

// PlayerPostThink: the player's one rot, a point a second down to his maximum.
void() VR_RN_PlayerFrame =
{
    if(!self.vr_rn_rot || time < self.vr_rn_rot)
    {
        return;
    }
    if(self.health > self.max_health)
    {
        self.health = self.health - 1;
        self.vr_rn_rot = time + 1;
        return;
    }
    self.vr_rn_rot = 0;
    VR_ItemUtil_EntDelItem(self, IID_SUPERHEALTH);
};

// ---------------------------------------------------------------------------------------------------------------------
// Keys: a stack of one colour (Copper's: counted in the player's .worldtype, its first 4 bits silver, the next 4 gold,
// one less than the keys held; Copper's doors, its world logic, take them one at a time)

// key_touch: `xPlayer` takes a key of `xIid` (IID_KEY1 silver, IID_KEY2 gold), `xHad` one already. FALSE: he cannot
// carry another (Quake's rule: one of a colour).
float(entity xPlayer, float xIid, float xHad) VR_RN_KeyTake =
{
    if(xIid != IID_KEY1 && xIid != IID_KEY2)
    {
        return !xHad;
    }
    float unit = xIid == IID_KEY2 ? 16 : 1;
    float extra = floor(xPlayer.worldtype / unit) & 15;
    if(!xHad)
    {
        xPlayer.worldtype = xPlayer.worldtype - extra * unit; // (a stale count from keys lost otherwise)
        return TRUE;
    }
    if(extra + 2 > VR_RN[VR_RN_Row()].keyStack)
    {
        return FALSE;
    }
    xPlayer.worldtype = xPlayer.worldtype + unit;
    return TRUE;
};

// How many keys of `xIid` `xPlayer` carries.
float(entity xPlayer, float xIid, float xHas) VR_RN_KeyCount =
{
    if(!xHas)
    {
        return 0;
    }
    return (floor(xPlayer.worldtype / (xIid == IID_KEY2 ? 16 : 1)) & 15) + 1;
};

// ---------------------------------------------------------------------------------------------------------------------
// Monsters

// walk/fly/swimmonster_start (VR_SF_MonsterPrepare): our vanilla monster's health by the map's row, where its spawn
// function left Quake's (an armed grunt's, a variant's own health stays theirs).
void(entity e) VR_RN_MonsterNumbers =
{
    float row = VR_RN_Row();
    if(row == VR_RULES_ID)
    {
        return;
    }
    vr_rules_nums q = VR_RN[VR_RULES_ID], n = VR_RN[row];
    if(e.classname == "monster_army" && e.health == q.hpArmy) e.health = n.hpArmy;
    else if(e.classname == "monster_dog" && e.health == q.hpDog) e.health = n.hpDog;
    else if(e.classname == "monster_fish" && e.health == q.hpFish) e.health = n.hpFish;
    else if(e.classname == "monster_knight" && e.health == q.hpKnight) e.health = n.hpKnight;
};

// An enforcer's laser's damage under the map's ruleset.
float() VR_RN_LaserDamage =
{
    return VR_RN[VR_RN_Row()].laserDmg;
};

// Arcane Dimensions' player rules that travel with its content (docs/vr-port/ARCANE_DIMENSIONS.md, section 16): its
// debuffs in every map, its burning merged into Quake VR's (decision 24). After vr_ad_items.qc; the functions Quake
// VR's files call are declared in vr_ad_defs.qc, each call there marked "AD support".

// ---------------------------------------------------------------------------------------------------------------------
// Debuffs (decision 24: everywhere)
//
// AD's monsters and traps poison (spiders, scorpions, fumigators... AD's PoisonDeBuff), make bleed (scorpions,
// steelclaws, wraiths: BleedDeBuff, health taken at once and given back over time) and set on fire (pyros, wraiths, fire
// traps: BurnDeBuff) the player, in any map they are in. Poison and bleeding are AD's own, ticked here for Quake VR's
// player in every map while AD's data is there (AD's ClientDeBuff; its screen tints are stuffed commands, filtered: the
// wrist gadget shows them instead). Burning is Quake VR's (vr_burning.qc): AD's BurnDeBuff (a reviewed patch) lights
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

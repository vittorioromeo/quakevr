/*======================================================================
    T_EELZap
    Ignores entities outside of the water and other eels
    inflictor -(self) = projectile
    attacker - (self.owner) = monster/player OR (self) = direct attack
    ignore   - (other) = Original target OR (world) = no exceptions
======================================================================*/
void(entity inflictor, entity attacker, float damage, entity ignore, float checkclass) T_EELZap = {
    local float points;
    local entity head;
    local vector org, imporg;

    // Cannot hurt the world or enemy targets (fake flying markers)
    if (attacker.noradiusdmg) return;
    if (attacker.classtype == CT_WORLD) return;
    if (attacker.classtype == CT_ENEMYTARGET) return;

    // check if inflictor is a bmodel (different origin location)
    if (inflictor.bsporigin) imporg = bmodel_origin(inflictor);
    else imporg = inflictor.origin;
        
    head = findradius(imporg, damage+40);
    while (head) {
        if (head != ignore) {
            // This is used mostly for wraiths so when they die they don't
            // kill each other, can't get the code logic to work so its
            // setup as a dead exception path
            if (checkclass == IGNORECLASS && head.classtype == ignore.classtype) { }
            else {
                // Check for any breakable which is prone to explosive damage
                if (ai_foundbreakable(attacker, head,TRUE) && head.brktrigmissile !=0) {
                    trigger_ent(head, attacker);
                }
                else {
                    // Can be damaged and NOT immune to radius (splash) damage
                    if (head.takedamage > 0 && head.noradiusdmg == 0) {
                        org = head.origin + (head.mins + head.maxs)*0.5;
                        points = 0.5*vlen (imporg - org);
                        if (points < 0) points = 0;
                        points = damage - points;
                        if (head == attacker) points = points * 0.5;
                        if (points > 0 && head.health > 0) {
                            // Need CanDamage to check for anything blocking LoS
                            // It will do several traces (source->target) to check
                            // for any blocking entities (lifts, doors, walls etc)
                            if (CanDamage (head, inflictor)) {
                                if (head.resist_rockets > 0) 
                                    points = points * head.resist_rockets;
                                if ((head.classtype != CT_MONEEL) && (head.flags & FL_INWATER)) {
                                    T_Damage (head, inflictor, attacker, points, DAMARMOR);
                                }
                            }
                        }
                    }
                }
            }
        }
        head = head.chain;
    }
};

// From Quoth
void() ai_shotcheck = { if (visible(self.enemy)) self.oldenemyorigin = self.enemy.origin; };

/* DRAKE DAMAGE CODE

// Knockback!
void(entity targ, entity inflictor, float damage, float dflags) T_KnockBack =
{
    local   vector  dir;

    if (!targ)
        return;     // Can't move the world!

    dir = '0 0 0';
    if (inflictor)
        if (!(dflags & DF_NO_KNOCK))
    {
        dir = targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5;
        dir = normalize(dir);
        dir = dir*damage*8;
        // If target is a player, move him now.  Do not check for
        // movetype_walk because the player may use flight.
        if (targ.classname == "player")   // Was 'movetype == MOVETYPE_WALK'.
            targ.velocity = targ.velocity + dir;
        else if (dflags & DF_PUSH_NPC)
        {
            if (targ.mass && (targ.mass <= MASS_SUPER))
            if (targ.solid != SOLID_BSP)
            {
                if (targ.mass == MASS_SUPER)
                    dir = dir * 0.5;    // Harder to shove.
                targ.velocity = targ.velocity + dir;
                if (targ.flags & FL_ONGROUND)
                {   // Engine does not update velocity unless falling.
                    targ.flags = targ.flags - FL_ONGROUND;
                    targ.velocity = dir;    // Set to new velocity.
                    if (targ.velocity_z < 150)
                        targ.velocity_z = 150;
                }
                else
                    targ.velocity = targ.velocity + dir;
            }
        }
    }
    damage_velocity = dir;
};

//============
// T_Damage
//
// The damage is coming from inflictor, but get mad at attacker
// This should be the only function that ever reduces health.
//
// FIXME:  Pass impact point and velocity.
//============
void(entity targ, entity inflictor, entity attacker, float damage,
    float dflags, string dtype) T_NewDamage =
{
    if (!targ.takedamage)
    {
        if (!(dflags & DF_NO_SHIELDS) || !(targ.xfl & XFL_SHIELDS))
            return;
        // ELSE... penetrate the mega-enforcers' shields.
    }
    if (dflags & DF_BLESSED)
        if (Ally_Check (targ, attacker))
            return;     // A 'blessed' attack ignores allies.

//    local   vector  dir;
    local   entity  oldself;
    local   float   save;
    local   float   take;
    local   float   burn;       // PM:  Burn damage.
    local   float   misc;       // PM:  Miscellaneous float.
    local   float   exotic, adamant;    // Update 5/13/10:  Exotic armors.

// Used by buttons and triggers to set activator for target firing.
// If minion did it, give credit to master, so messages can be displayed.
// FIXME:  This may cause weird things for other code that use damage_attacker.
//         Example:  Gremlin uses it to determine choice of death animation.
    damage_attacker = Patriarch (attacker);

// check for quad damage powerup on the attacker
// PM:  Yes, I know Quake2 does it differently and better, but this is simple
// and works well enough.
    if (!(dflags & DF_PROXY))
    {
        if (attacker.curse_type == CURSE_WEAKEN)
            damage = damage / 2;    // FIXME:  What if percentile damage?
        if (attacker.perms & POW_VAMPIRE)
            if (dflags & DF_MELEE)
                damage = damage * 1.25;
        if (attacker.super_damage_finished)     // IT_QUAD
        {
            if (dflags & DF_PERCENT)
                damage = FourZaps (damage);
            else
                damage = damage * 4;
            dflags = dflags | DF_SCAR;  // Let quad anything blast zombies.
        }
    }

// Check for health percentage mod.  Works like Static Field from Diablo2.
    if (dflags & DF_PERCENT)
        damage = targ.health * damage / 100;

// - - - - - - - - - - - - - - - - - - -
// check for other damage modifiers
//
// PM:  Unlike Diablo II, apply resistance to armor too.
// We (generally) don't know how the target gets his resistance.
// That is, barrier (always full damage) vs. innate/buffs (use resists).
// - - - - - - - - - - - - - - - - - - -
    burn = 0;
    misc = 0;
    damage = ResistDamage (targ, damage, dflags);

// Empathy shields - - - - - - - - - - -
// PM:  This is DANGEROUS!!!  Prone to bugs if not careful.
//  Check the comments in some of the monster files, namely those with
//  jump attacks, to see the changes I made to prevent thorns-related bugs.
// - - - - - - - - - - - - - - - - - - -
    if (targ.thorns_finished)
        if (!(dflags & DF_NO_ARMOR))
    {
        take = dflags & DF_PROXY;
        if (take || !attacker.thorns_finished)
        {
            damage = damage * 0.5;
            if (!take)
                T_NewDamage (attacker, targ, targ, damage, dflags, "thorns");
            // ELSE... damage by proxy.
            // Damage did not really come from the attacker, even though
            // all the credit goes to him.

            // Was placed between invulnerability and teamplay checks.
            if (targ.thorns_sound < time)
            {
                if (powcache & PCH_THORNS_USE)
                    sound (targ, CHAN_ITEM, "hipitems/empathy2.wav", 1, ATTN_NORM);
                targ.thorns_sound = time + 0.5;
            }
        }
        // ELSE both sides have thorns, and they cancel each other out.
    }
// - - - - - - - - - - - - - - - - - - -
// Final damage
    damage = ceil(damage);

// figure momentum add
    T_KnockBack (targ, inflictor, damage, dflags);

// Check for invulnerability.  In classic Doom, invulnerability did not
// red flash or otherwise change the player's view.  Some other Quake mods
// harken back to that style.
    if (targ.flags & FL_GODMODE)
    {
        if (skill < 4)
            return;
        if (!(targ.flags & FL_CLIENT))
            return;
    // Players can't cheat in Hell.
        targ.flags = targ.flags - FL_GODMODE;
        if (Kill_Cheater (targ, inflictor, "fell from grace"))
            return;
    }
    if (targ.invincible_finished)
    {
        ProtectSound (targ);
        return;
    }
// Cross of Deflection - - - - - - - - -
    if (targ.cross_finished)
    {   // Note:  Cross reduces damage only, not knockback.
        CrossSound (targ);
        if (dflags & DF_CROSS_BLOCK)
            return;     // Negate all damage from this attack.
        damage = ceil(damage * 0.5);    // Even armor-piercers get affected.
    }
// - - - - - - - - - - - - - - - - - - -
// Friendly fire.  Placed after shielding items.
    if (teamplay == 1)
        if (Team_Match (targ, attacker))
            return;     // team play damage avoidance
// FIXME:  Too messy, rewrite.
    if (targ.xfl & XFL_BOSS)
    {
        // SoE:  Bane clones.
        if (targ.classname == attacker.classname)
            if ((targ.classname == "monster_bane") || (targ.classname == "monster_morph"))
                if (Ally_Check (targ, attacker))
                    return;     // Bane can't hurt himself and his clones.
        // Minion immunity.
        if (targ == attacker.master)
            if (Ally_Check (targ, attacker))
                return;     // Immune to minions' attacks.
    }
// - - - - - - - - - - - - - - - - - - -

// Damage vs. protection
    take = damage;
    save = 0;
    exotic = Armor_Exotic (targ);
    adamant = Armor_Infinite (targ);

    // Save damage based on target's armor level.
    // Note:  Treat armorvalue of morphed players more like health by
    //  absorbing all damage, even armor-piercing damage.
    if (!(dflags & DF_NO_ARMOR) || targ.form_active)
    {
        damage = damage - PowerArmor_Absorb (targ, damage, TRUE);

//        take = 1;
        if (targ.form_active)
        {   // Absorb all damage while in monster form.
//            if (targ.form_active == FORM_SCORPIO)
//                take = 2;   // Centroid has 300 hp.  150 * 2 = 300 hp.
//            save = damage / take;
            save = damage;
        }
        else if (targ.armortype)
        {   // Target has armor.
            if ((exotic != SUIT_COURAGE)  ||  UseCourage (targ, inflictor.origin))
                save = ceil (targ.armortype * damage);
        }
        else if (exotic == SUIT_CHAOS)
        {
            if (targ.health > 0)
            {
                save = Armor_ChaosRating (targ);
                save = ceil (save * damage);
                // Chance to confuse attacker.  Must delay confusion
                // in case T_Damage was called within T_RadiusDamage.
// DISABLED -- I need to precache a sound to use this, and there's never
// enough slots!  Variable absorption and teleportation should be chaotic enough.
//                if (!(dflags & DF_PROXY))
//                    if (random() < (save + 1) * 0.01)
//                        Confusion_Delayed (attacker, targ);
            }
        }

        if (save)   // Non-zero only with armor, and most are unarmored.
        {
            if (targ.curse_type == CURSE_ARMOR)
                save = ceil(save / 2);      // Armor is not as effective.
            if (!adamant)
                if (save > targ.armorvalue)
                    save = targ.armorvalue;
            take = ceil (damage - save);
    //        take = ceil (damage - save * take);
        }
    }

// Light up the screen if target is a living client.
    if (targ.flags & FL_CLIENT)
    {
        if (!targ.deadflag)
        {
            //- - - - - - - - -
            // Timed, continuous, or persistant damage hurts the target each
            // frame (or every other frame) it is active, and flashing the
            // hud each time will eventually blind the client with red flash.
            //
            // Leave the hud alone if damage is timed and the client is
            // already reeling in pain.
            //- - - - - - - - -
            if ( !(dflags & DF_TIMED) || (targ.pain_finished <= time) )
            {
                if (damage < 0)
                    stuffcmd (targ, "bf\n");    // Use yellow flash.
                else
                {
                    targ.dmg_take = targ.dmg_take + take;
                    targ.dmg_save = targ.dmg_save + save;
                }
            }
            targ.dmg_inflictor = inflictor;
        }
    }

    // Now check for armor damage.
    if (save)
    if (!adamant)
    {
        misc = targ.armorvalue;     // Remember armor before hit.
        targ.armorvalue = targ.armorvalue - save;
        if ((targ.armorvalue <= 0) && !Armor_Indestructible (targ))
        {   // Normal procedure.
            Armor_Remove (targ);    // Lost all armor.
            exotic = 0;
        }
        else if (damage < 0)
        {   // Negative damage can increase armor value.  For clients,
            // do not let it rollover the max of 255 (8-bit).
            if (targ.flags & FL_CLIENT)
                if (targ.armorvalue > 250)
                {   // Move unused armor into health.
                    take = take - (targ.armorvalue - 250);
                    targ.armorvalue = 250;
                    if (misc >= 250)
                        misc = 0;
                }
            if (misc)
                ProtectSound (targ);
        }
        else if (!take)
        {
            if ((exotic == SUIT_SPIRIT) || (exotic == SUIT_ULTRA))
            {   // Spirit armor absorbed all damage this time.
                ProtectSound (targ);
                return;     // Yes, even if a monster wouldn't notice.
            }
        }
    }

// Check for post-armor resistance, like CTF's Earth rune.
// Protection that is applied after armor damage and momentum.
// Note:  Multipliers first, then additions.
//    if (targ.tech == TECH_ARMOR)
//        take = take / 2;
    if (targ.curse_type == CURSE_DAMAGE)
        take = take * 2;
    if (exotic == SUIT_BERSERKER)   // Demonhide increases damage taken.
        take = take * 1.3;          // Green armor reduces 30%, reverse it!
    take = floor(take);
// DISABLED -- Phaser/Wand doesn't reduce damage anymore.
// Keep the code handy in case I want to add a protection artifact.
//    // Holding the wand reduces damage by one -- think ring of protection +1.
//    if (!targ.form_active)
//        if (targ.war == WAR_ZAPPER)
//            if (targ.arsenal & WAR_ZAPPER)
//                if (take > 0)
//                    take = take - 1;

// Negative damage check.
    if (take < 0)
    {   // Negative damage?  Heal the target.  One of czg's maps has a
        // fountain that heals the player by causing negative damage.
        // Since negative damage could add health over the normal limit
        // in the standard progs.dat, do so here too.
        if (T_Heal (targ, 0 - take, 2))     // 2 = Heal even dead things.
        {
            if (targ.health >= 1)
            {
                if (targ.deadflag == DEAD_DEAD)
                    monster_revive (targ, targ);
// DISABLED -- No sound please.
//                else if (!targ.deadflag)
//                    ProtectSound (targ);
            }
        }
        return;
    }

// Check for burn damage.  Works only if player did not heal from damage.
// Warning:  Ugly hack alert -- the big burn hack!
    if (dflags & DF_BURN)
    {   // Okay, here's the deal.  The ignition stuff is tailored specially
        // for dragons' fire attacks.  Anything that lights stuff on fire
        // is set rigidly to the same standard.  I could change the code to
        // allow more variable burning stats, but I would need to add yet
        // another variable to be passed to T_Damage, and I don't want to
        // just for the dragons.  Hence, the big burn hack.
        if (dtype == "rabies")  // Named after Diablo2 druid skill.
        {   // 50% chance to infect a new target with fire.
            // Assumes inflictor is a big fire burning on targ.
            if (random() < 0.5)
                Burn_Add (targ, attacker, inflictor.dmg - BURN_PER_SECOND);
            dtype = "fire";     // Change it back.
        }
        else
        {   // Chance to burn equals (damage + 1)%.  E.g., 9 damage = 10%.
            // Yes, use 'damage' and not 'take'.
            if (random() < (damage+1)*0.01)
                Burn_Add (targ, attacker, MAX_BURN_DAMAGE);
        }
    }

// Check if the damage is allowed to hurt the target.  In case of
// zombies/corpses, remove health only on one-hit kill or scar damage.
// Note:  Boss monsters always cause scar damage to other monsters
// because we don't want a lowly zombie to overpower the boss.
//
// Now that 'burn' served its original purpose (or would have), use it as
// a boolean such that TRUE = hurt the target or FALSE = ignore the damage.
    burn = FALSE;
    if (dflags & DF_SCAR)   // This type of damage hurts even the dead.
        burn = TRUE;
    else if ((targ.curse_type == CURSE_ARMOR) || (attacker.xfl & XFL_BOSS))
    {   // Negate zombies' damage immunity.
        if (targ.flags & FLx_CREATURE)
            burn = TRUE;
    }
    else if (targ.deadflag)
    {   // Dead targets are like zombies, damage only if it gibs on one hit.
        if (take > (0 - targ.gib_health))
            if (!(dflags & DF_NO_GIB))
                burn = TRUE;
    }
    else
    {   // Live target -- zombies are hurt only by one-hit kill.
        // Anything else is hurt normally.
        if (!(targ.xfl & XFL_ZOMBIE) || (take >= targ.max_health))
            burn = TRUE;
    }

// Do the damage.
    if (burn)
    {
        // Check for life leech before applying the damage.
        // Only living attackers can leech from living targets.
        // Update:  Healing is done after damage.
        // Note:  It is possible for attacker to leech himself.  Silly,
        //  but I do not see why it would not work as long as leech is
        //  kept below 100%.
        // FIXME:  Make leech ineffective against non-living such as undead.
        local   float   ignore, hi;

        ignore = FALSE;
        save = misc = 0;
        if (attacker.health > 0)
        if (attacker.flags & FLx_CREATURE)
        if (targ.flags & FLx_CREATURE)
        {
            if (dflags & DF_LEECH)
            {   if (attacker.family & FAM_WRATH)
                {
                    save = save + 0.5;      // Assume life drain attack.
                    ignore = TRUE;          // Exceed normal health limit.
                }
                else
                    save = save + 0.1;
            }
// DISABLED -- Removed lifetap curse.
//            if (targ.curse_type == CURSE_LIFETAP)
//                save = save + 0.5;
            if (save > 1)
                save = 1;   // Cannot heal more than damage dealt.

            if (take > targ.health)
                hi = targ.health;
            else
                hi = take;
            if (save)
            {
                misc = ceil (hi * save);
            }

            if (dflags & DF_MELEE)
            if (attacker.perms & POW_VAMPIRE)
            if (!(targ.family & FAMx_NONLIVING))
            {
                misc = misc + 1;
                if (misc > hi)
                    misc = hi;
            }
        }
        // Now inflict the damage for real.
        targ.hit_points = targ.health = targ.health - take;
        if (misc > 0)
            T_Heal (attacker, misc, ignore);
    // Update 5/14/2010:  Demonhide lifesaver check.
        if (exotic == SUIT_BERSERKER)
        {   // Demonhide will try to save player from a fatal wound.
            if (targ.health < 1)
            {   // Armor will heal the player then break.
                targ.health = targ.armorvalue + targ.health;
                if (targ.health > targ.max_health)
                    targ.health = targ.max_health;
                targ.hit_points = targ.health;
                if (targ.flags & FL_CLIENT)
                {
                    if (targ.health > 0)
                    {
//                        sound (targ, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
                        ProtectSound (targ);
                        centerprint (targ, "You absorb your armor!");
                    }
                    // ELSE... Damage was too much to overcome -- tough luck!
                }
                // Saved the player's ass, so go away now.
                Armor_Remove (targ);
            }
        }
    //- - - - - - - - -
// DISABLED -- Regen code.
//        targ.healtime = time + 1;   // Suppress regeneration.
    }
    if (targ.health <= 0)
    {
        // Target was slain!
        if (!targ.deadflag)
        {
            // Shotgun hack:  Knockback on kill!
            if (!q1shot)
            if (dtype == "ssg")
            if (targ.flags & FL_MONSTER)
            if (take >= 25)
            {
                if (targ.classname == "monster_zombie")
                    take = take * 0.5;      // Offset 2x damage from headshot.
                else if (targ.family & FAM_WRATH)
                    take = take * 0.2;      // Give a slight push
                T_KnockBack (targ, inflictor, take, DF_PUSH_NPC | dflags);
            }

            if (dflags & DF_GIB)
                targ.hit_points = targ.health = -99;
            else if (dflags & DF_NO_GIB)
                targ.hit_points = targ.health = 0;
            Killed (targ, attacker, dtype);
        }
        else if (targ.th_gib)
        {
            if (burn)
            {
                oldself = self;
                self = targ;
                if (self.th_gib (0 - take))       // Gibbed if TRUE.
                    self.th_gib = world.null_func;
                self = oldself;
            }
        }
        return;
    }

// React to the damage.
    oldself = self;
    self = targ;

    if (self.flags & FL_MONSTER)
    {
    //- - - - - - - - -
    // Notes:
    // 1) Never react to 'timed' damage because it becomes too easy for
    //  multiple attackers to paralyze the target.
    // 2) AFAIK, doors are both inflictor and attacker.  Also, since doors
    //  are not creatures, monsters can't get mad at it.  So no need for
    //  'if (inflictor.classname != "door")'
    // 3) Normally, DoE dragon does not react.  In this mod, it reacts if
    //  not bound to a path.
    //- - - - - - - - -
        if (attacker)   // != world)
            if (!(dflags & DF_NO_REACT))
                if (!(dflags & DF_TIMED) || !self.enemy)
                    if (!(self.ryuflags & RYU_PATH))    // For DoE dragon.
                        if (Enemy_Check (self, attacker))
                            NewEnemy (attacker);
    }

    if (self.th_pain)
    {
        if (dflags & DF_TIMED)
            take = 0;   // Minimize pain.
        self.th_pain (attacker, take);

        // Do Nightmare stuff on a case-by-case basis like in Quake2.
        // Quake2 has Quake's pain_finished = T+5, but was made moot by
        // the no pain anims in nightmare override most monsters have.
    }

    self = oldself;
};

/============
// T_RadiusDamage
//============
void(entity inflictor, entity attacker, float damage, entity ignore,
    float dflags, string dtype, float radius, float attn) T_RadiusDamage_II =
{
// Do some sanity checking.
    if (damage < 1)
        return;     // No damage, don't waste time.

// Now do the radius damage routine.
    local   float   points;
    local   entity  head;
    local   vector  org;
    local   float   no_water;
    local   float   ldmg;

    dflags = (dflags | DF_CROSS_BLOCK) - (dflags & DF_NO_SHIELDS);
    ldmg = 0;
    no_water = !(dflags & DF_WATER_ONLY);
//    head = QC_FindRadius(inflictor.origin, radius);     // Hits SOLID_NOT.
    head = findradius(inflictor.origin, radius);
    while (head)
    {
        if (head != ignore)
        if (head.takedamage)
        if (!(head.xfl & XFL_FIREANGEL))
        if (no_water || head.h2olevel)
        if (head.solid > SOLID_NOT)     // So DarkPlaces won't gib the fallen.
        {
            // T_RadiusDamage may be called during WorldScan, and the less
            // code ran and functions called, the less likely FitzQuake will
            // crash when a huge mob gets nuked by Mjolnir.  Here, do the
            // math from Midpoint instead of calling the function.
            org = head.origin + (head.mins + head.maxs)*0.5;    //Midpoint (head);
            points = attn * vlen(inflictor.origin - org);
            points = damage - points;
            if (points > 0)
                if (CanDamage (head, inflictor))
                {
                    if (head == attacker)
                    {   // Ignore attacker until after everyone else is hit.
                        if (dflags & DF_PROXY)
                            ldmg = points;
                        else if (!(dflags & DF_SAFE))
                            ldmg = points * 0.5;
                    }
                    else
                        T_NewDamage (head, inflictor, attacker, points, dflags, dtype);
                }
        }
        head = head.chain;
    }

// Damage attacker last.  Why?  Since frags do not go less than zero,
// build up the frags from kills first before applying the suicide penalty.
// FIXME:  Does not account for teammate kills, which reduces frags too.
    if (ldmg)
        T_NewDamage (attacker, inflictor, attacker, ldmg, dflags, dtype);
};
//============
void(entity inflictor, entity attacker, float damage, entity ignore,
    float dflags, string dtype) T_NewRadiusDamage =
{
    T_RadiusDamage_II (inflictor, attacker, damage, ignore, dflags, dtype, damage+40, ATTN_DAMAGE);
};
*/
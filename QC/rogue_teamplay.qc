// TODO VR: (P1) most of the item management and weapon management here needs to
// be rewritten for VR

// Rogue Teamplay Variants
// Jan'97 by ZOID <zoid@threewave.com>
// Under contract to id software for Rogue Entertainment


// New entity fields

.float steam;        // selected team
.float suicide_count; // how many times has this player done something silly?
.float ctf_flags; // various flags for CTF
.float ctf_killed;    // used for base spawning
.float ctf_flagsince;    // when we picked up the flag
.float ctf_lasthurtcarrier; // when we last hurt the carrier
.float ctf_lastreturnedflag; // when we returned the flag
.float ctf_lastfraggedcarrier; // when we last fragged the carrier

float TEAM1 = 5;  // color of team1 (red), note that this is self.team which
                //is one more than the color setting, so 'color 4' is red.
float TEAM2 = 14;  // color of team2 (blue), note that this is self.team which
                //is one more than the color setting, so 'color 13' is blue.
float TEAM3 = 1; // color of team3 (grey) in alt CTF

entity team1_lastspawn;  // last spawn spot for team1
entity team2_lastspawn;  // last spawn spot for team2
float nextteamupdtime; // next time for a broadcast update

// TEAMPLAY VALUES

float TEAM_NORMAL_NODAMAGE    =    1;    // id's normal teamplay, can't hurt self
float TEAM_NORMAL_DAMAGE    =    2;    // id's alt teamplay, can hurt self/team
float TEAM_DMATCH_TAG        =    3;    // deathmatch tag, no 'team' stuff
float TEAM_CTF                =    4;    // regular CTF, locked colors
float TEAM_CTF_ONEFLAG        =    5;    // one flag CTF
float TEAM_CTF_ALT            =    6;    // alternate CTF (three teams)

// gamecfg cvar fields

float GAMECFG_ENABLE_RUNES        =    1; // enable runes (default is disabled)
float GAMECFG_TEAM_ARMOR_DMG    =    2; // 2 team damage affects armor (default is not)
float GAMECFG_TEAM_HEALTH_DMG    =    4; // 4 team damage affects health (default is not)
float GAMECFG_USE_COLOR            =    8; // use color player joins as (if you join and start with blue, you will go
//    to blue team, same with red/green.  If you are an illegal color, you'll
//    get assigned).
float GAMECFG_ALLOW_CHG            =    16; // allow people to change teams

float CTF_CAPTURE_BONUS = 15; // what you get for capture
float CTF_TEAM_BONUS = 10; // what your team gets for capture
float CTF_ALT_CAPTURE_BONUS = 8; // what you get for capture
float CTF_ALT_TEAM_BONUS = 4; // what your team gets for capture
float CTF_RECOVERY_BONUS = 1; // what you get for recovery
float CTF_FLAG_BONUS = 0; // what you get for picking up enemy flag
float CTF_FRAG_CARRIER_BONUS = 2; // what you get for fragging enemy flag carrier
float CTF_FLAG_RETURN_TIME = 40; // seconds until auto return
float CTF_CARRIER_DANGER_PROTECT_BONUS = 2; // bonus for fraggin someone
    // who has recently hurt your flag carrier
float CTF_CARRIER_PROTECT_BONUS = 1; // bonus for fraggin someone while
    // either you or your target are near your flag carrier
float CTF_FLAG_DEFENSE_BONUS = 1; // bonus for fraggin someone while
    // either you or your target are near your flag
float CTF_RETURN_FLAG_ASSIST_BONUS = 1; // awarded for returning a flag that causes a
    // capture to happen almost immediately
float CTF_FRAG_CARRIER_ASSIST_BONUS = 2; // award for fragging a flag carrier if a
    // capture happens almost immediately
float CTF_TARGET_PROTECT_RADIUS = 400; // the radius around an object being
    // defended where a target will be worth extra frags
float CTF_ATTACKER_PROTECT_RADIUS = 400; // the radius around an object being
    // defended where an attacker will get extra frags when making kills
float CTF_CARRIER_DANGER_PROTECT_TIMEOUT = 4;
float CTF_CARRIER_FLAG_SINCE_TIMEOUT = 2;
float CTF_FRAG_CARRIER_ASSIST_TIMEOUT = 6;
float CTF_RETURN_FLAG_ASSIST_TIMEOUT = 4;
float CTF_UPDATE_TIME = 120;

// CTF flags
float CTF_FLAG_FLAG            =    1;    // palyer has flag in one flag mode
float CTF_FLAG_TEAM1        =    1;    // player has team1's flag
float CTF_FLAG_TEAM2        =    2;    // player has team2's flag
float CTF_FLAG_STUFF_COLOR    =    4;    // gotta stuff his color

// flag status used in cnt field of flag
float FLAG_AT_BASE = 0;
float FLAG_CARRIED = 1;
float FLAG_DROPPED = 2;

// Return a name for the color of a team
string(float Team) GetTeamColor =
{
    if(Team == 1) return("White");
    else if(Team == 2) return("Brown");
    else if(Team == 3) return("Light blue");
    else if(Team == 4) return("Green");
    else if(Team == 5) return("Red");
    else if(Team == 6) return("Olive");
    else if(Team == 7) return("Orange");
    else if(Team == 8) return("Peech");
    else if(Team == 9) return("Purple");
    else if(Team == 10) return("Majenta");
    else if(Team == 11) return("Grey");
    else if(Team == 12) return("Aqua");
    else if(Team == 13) return("Yellow");
    else if(Team == 14) return("Blue");
    return "Unknown";
};

string(float Team) GetCTFTeam =
{
    if(Team == TEAM1) return "���";
    else if(Team == TEAM2) return "����";
    else if(Team == TEAM3) return "����";
    return "UNKNOWN";

};

/*
================
TeamArmorDam

Return TRUE if the target's armor can take damage from this attacker.
================
*/

float(entity targ, entity inflictor, entity attacker, float damage) TeamArmorDam =
{
    if(teamplay <= 0)
        return TRUE;    // PGM bug? fix

    // teamplay modes 4,5,6 protect armor
    if((teamplay == TEAM_CTF ||
        teamplay == TEAM_CTF_ONEFLAG ||
        teamplay == TEAM_CTF_ALT) &&
        attacker.steam == targ.steam &&
        attacker != targ &&
        !(cvar("gamecfg") & GAMECFG_TEAM_ARMOR_DMG)) {
        // Armor is protected
        return FALSE;
    }
    return TRUE;
};

/*
================
TeamHealthDam

Return TRUE if the target can take health damage from this attacker.
================
*/

float(entity targ, entity inflictor, entity attacker, float damage) TeamHealthDam =
{
    if(teamplay <= 0)
        return TRUE;

    if(teamplay == TEAM_NORMAL_NODAMAGE && attacker.steam == targ.steam)
        return FALSE;

    // teamplay modes 4,5,6 protect health
    if((teamplay == TEAM_CTF ||
        teamplay == TEAM_CTF_ONEFLAG ||
        teamplay == TEAM_CTF_ALT) &&
        attacker.steam == targ.steam &&
        attacker != targ &&
        !(cvar("gamecfg") & GAMECFG_TEAM_HEALTH_DMG)) {
        // Health is protected
        return FALSE;
    }
    return TRUE;
};

void(entity who) TeamResetCarrier =
{
    local entity head;

    // When the flag carrier dies, reset the last_hurt_carrier field in
    // all players on the opposite team from the flag carrier.  The carrier
    // has been killed, so there is no longer a reason to award points for
    // killing off his assailants
    if(teamplay >= TEAM_CTF &&
        (who.ctf_flags & (CTF_FLAG_TEAM1 | CTF_FLAG_TEAM2))) {

        head = find(world, classname, "player");

        while (head != world) {
            if(teamplay == TEAM_CTF_ONEFLAG ||
                ((who.ctf_flags & CTF_FLAG_TEAM1) && head.steam == TEAM1) ||
                ((who.ctf_flags & CTF_FLAG_TEAM2) && head.steam == TEAM2))
                head.ctf_lasthurtcarrier = -10;
            head = find(head, classname, "player");
        }
    }
};

void(entity targ, entity attacker) TeamAssists =
{
    local float flag_radius;
    local float flag_carrier_radius;
    local string s;
    local entity head;

    if((targ.ctf_flags & (CTF_FLAG_TEAM1 | CTF_FLAG_TEAM2)) &&
        targ.steam != attacker.steam) {
        //ZOID: one team fragged the other team's flag carrier

        // Mark the attacker with the time at which he killed the flag
        // carrier, for awarding assist points
        attacker.ctf_lastfraggedcarrier = time;

        // give player only the normal amount of frags
        // if the carrier has only had the flag for a few seconds, to
        // prevent ppl intentionally allowing enemies to grab the flag,
        // then immediately fragging them
        if(targ.ctf_flagsince + CTF_CARRIER_FLAG_SINCE_TIMEOUT > time) {
            sprint(attacker, "Enemy flag carrier killed, no bonus\n");
        } else {
            attacker.frags = attacker.frags + CTF_FRAG_CARRIER_BONUS;
            sprint(attacker, "Enemy flag carrier killed: ");
            s = ftos(CTF_FRAG_CARRIER_BONUS);
            sprint(attacker, s);
            sprint(attacker, " bonus frags\n");
        }
    }

    // This code checks for all game-critical kills OTHER THAN fragging the enemy
    // flag carrier, like killing players who are trying to kill your flag carrier
    // or trying to grab your flag, and hands out bonus frags.

    // The two variables below track whether special bonus frags have already
    // been awarded for the attacker or target being near the flag or flag carrier.

    flag_radius = 0;
    flag_carrier_radius = 0;

    // get a string for the attacker's team now, for later announcements
    s = GetCTFTeam(attacker.steam);

    if((targ.ctf_lasthurtcarrier + CTF_CARRIER_DANGER_PROTECT_TIMEOUT > time) &&
        !(attacker.ctf_flags & (CTF_FLAG_TEAM1 | CTF_FLAG_TEAM2))) {
        // a player on the same team as the flag carrier killed
        // someone who recently shot the flag carrier
        attacker.frags = attacker.frags + CTF_CARRIER_DANGER_PROTECT_BONUS;
        flag_carrier_radius = 1;
        // NOTE: getting CARRIER_DANGER_PROTECT_BONUS precludes getting
        // other kinds of bonuses for defending the flag carrier, since
        // it's worth more points
        bprint(attacker.netname);
        bprint(" defends ");
        bprint(s);
        bprint("'s flag carrier against an agressive enemy\n");
    }

    // Bonusus for defending the flag carrier or the flag itself.
    // Extra frags are awarded if either the attacker or the target are
    // 1. within 40 feet of a flag carrier on the same team as the attacker
    // 2. within 40 feet of the attacker's flag
    // These bonuses are cumulative with respect to defending both the
    // flag and the flag carrier at the same time, but not cumulative with
    // respect to both the target and attacker being near the object being defended

    // find flags or flag carriers within a radius of the attacker
    head = findradius(attacker.origin, CTF_ATTACKER_PROTECT_RADIUS);

    while (head) {
        if(head.classname == "player") {
            if( (head.steam == attacker.steam) &&
                 (head.ctf_flags & (CTF_FLAG_TEAM1 | CTF_FLAG_TEAM2)) &&
                 (head != attacker) && // self defense
                 (!flag_carrier_radius) ) {
                // attacker was near his own flag carrier
                attacker.frags = attacker.frags + CTF_CARRIER_PROTECT_BONUS;
                flag_carrier_radius = 1;
                bprint(attacker.netname);
                bprint(" defends ");
                bprint(s);
                bprint("'s flag carrier\n");
            }
        }
        if((attacker.steam == TEAM1 && head.classname == "item_flag_team1") ||
            (attacker.steam == TEAM2 && head.classname == "item_flag_team2") ||
            head.classname == "item_flag") {  // one flag mode
            // attacker was near his own flag
            attacker.frags = attacker.frags + CTF_FLAG_DEFENSE_BONUS;
            flag_radius = 1;
            bprint(attacker.netname);
            bprint(" defends the");
            if(teamplay != TEAM_CTF_ONEFLAG) {
                bprint(" ");
                bprint(s);
            }
            bprint(" flag\n");
        }
        head = head.chain;
    }

    // find flags or flag carriers within a radius from the target
    head = findradius(targ.origin, CTF_TARGET_PROTECT_RADIUS);
    while (head) {
        if(head.classname == "player") {
            if( (head.steam == attacker.steam) &&
                 (head.ctf_flags & (CTF_FLAG_TEAM1 | CTF_FLAG_TEAM2)) &&
                 (head != attacker) &&
                 (!flag_carrier_radius)) { // prevents redundant points awarded
                // target was near attacker's flag carrier
                attacker.frags = attacker.frags + CTF_CARRIER_PROTECT_BONUS;
                flag_carrier_radius = 1;
                bprint(attacker.netname);
                bprint(" defends ");
                bprint(s);
                bprint("'s flag carrier\n");
            }
        }
        if((attacker.steam == TEAM1 && head.classname == "item_flag_team1") ||
            (attacker.steam == TEAM2 && head.classname == "item_flag_team2") ||
            head.classname == "item_flag" &&  // one flag mode
            (!flag_radius)) { // prevents redundant points awarded
            // target was near attacker's flag
            attacker.frags = attacker.frags + CTF_FLAG_DEFENSE_BONUS;
            flag_radius = 1;
            bprint(attacker.netname);
            bprint(" defends the");
            if(teamplay != TEAM_CTF_ONEFLAG) {
                bprint(" ");
                bprint(s);
            }
            bprint(" flag\n");
        }
        head = head.chain;
    }
};

/*
==================
TeamColorIsLegal

Return TRUE if the indicated color is legal
==================
*/
float(float color) TeamColorIsLegal =
{
    // All colors are legal if teamplay is not CTF
    if(teamplay < TEAM_CTF) {
        if(color > 0)
            return TRUE;
        return FALSE;
    }

    // In regular CTF and CTF_ONEFLAG, only two colors are legal
    if(teamplay == TEAM_CTF || teamplay == TEAM_CTF_ONEFLAG)
        if(color == TEAM1 || color == TEAM2)
            return TRUE;
        else
            return FALSE;

    // In ALT CTF, three colors are legal
    if(teamplay == TEAM_CTF_ALT)
        if(color == TEAM1 || color == TEAM2 || color == TEAM3)
            return TRUE;
        else
            return FALSE;

    // dunno what teamplay we're in, let'em all be ok
    return TRUE;
};

/* TeamSetSkin - set the skin of the player model to the
   apropriate skin based on team and teamplay settings.
*/
void() TeamSetSkin =
{
    self.skin = 0;
    if(deathmatch)
    {
        if(teamplay >= TEAM_CTF)
            self.skin = 1;
    }
/*
    if( deathmatch )
    {
        if( teamplay >= TEAM_CTF )
        {
//            if(self.team == TEAM3)
//                self.skin = 2;
//            else
                self.skin = 1;
        }
        if( teamplay < TEAM_CTF )
            self.skin = 1;
        else
        {
            if(self.team == TEAM1)
                self.skin = 2;
            else if(self.team == TEAM2)
                self.skin = 3;
            else if(self.team == TEAM3)
                self.skin = 4;
        }
    }
*/
};

/*
==================
TeamCheckTeam

Check if the team self is on is legal, and put self in a legal team if not.
==================
*/
void() TeamCheckTeam =
{
    local float team1count;
    local float team2count;
    local float team3count;
    local float newcolor;
    local float t;
    local entity p;
    local string n;

    if(self.steam >= 0 || teamplay < TEAM_CTF) {
        if(TeamColorIsLegal(self.team)) {
            self.steam = self.team;
            TeamSetSkin();
            return;
        }
    }

    // Assign the player to a team.

    // Sum the players on all the teams.
    team1count = 0;
    team2count = 0;
    team3count = 0;

    p = find (world, classname, "player");

    while(p) {
        if(p != self) {
            if(p.steam == TEAM1)
                team1count = team1count + 1;
            else if(p.steam == TEAM2)
                team2count = team2count + 1;
            else if(p.steam == TEAM3)
                team3count = team3count + 1;
        }
        p = find(p, classname, "player");
    }

    // Find the team with the least players.
    newcolor = TEAM1;
    t = team1count;

    if(team2count < t || (team2count == t && random() < 0.5)) {
        newcolor = TEAM2;
        t = team2count;
    }

    // in CTF_ALT, there's three teams
    team3count = team3count * 2; // grey team only gets half as many players
    if(teamplay == TEAM_CTF_ALT && team3count < t) {
        newcolor = TEAM3;
        t = team3count;
    }

    // Put the player on a the new team.
    self.ctf_flags = self.ctf_flags | CTF_FLAG_STUFF_COLOR;

    n = GetCTFTeam(newcolor);
    sprint(self, "You have been assigned to ");
    sprint(self, n);
    sprint(self, " team.\n");

    self.steam = newcolor;      // Remember what team we're on
    self.team = newcolor;
    TeamSetSkin();
};

/* Check for team changing and perform whatever actions are neccessary.  */
void() TeamCheckLock =
{
    local   float   n;
    local   string  s;

    if(!deathmatch || teamplay < TEAM_CTF) {
        // all colors are legal, no force
        self.steam = self.team;
        return;
    }

    if(self.ctf_flags & CTF_FLAG_STUFF_COLOR) {
        self.ctf_flags = self.ctf_flags - CTF_FLAG_STUFF_COLOR;
        stuffcmd(self, "color ");
        n = self.steam - 1;
        s = ftos(n);
        stuffcmd(self, s);
        stuffcmd(self, "\n");
        TeamSetSkin();
        return;
    }

    if(!TeamColorIsLegal(self.team) && self.team == self.steam)
        self.steam = -1; // full reset

    // Check to see if the player has changed colors
    if(self.team != self.steam) {
        if(self.steam >= 0) {
            if(TeamColorIsLegal(self.steam)) {

                if(!(cvar("gamecfg") & GAMECFG_ALLOW_CHG)) {
                    // changing teams sucks, kill him

                    // if he has tried to change teams several
                    // times, kick him off the server.
                    if(self.suicide_count > 3) {
                        sprint(self, "You were told you can't change teams.\nGo play color games somewhere else.\n");
                        stuffcmd(self, "disconnect\n");
                        bprint(self.netname);
                        bprint(" has bad color sense\n");
                    }
                    // case base respawn
                    if(self.ctf_killed != 1)
                        self.ctf_killed = 2;
                    T_Damage(self,self,self,1000);  // Kill the player
                    // trying to change teams counts as a suicide
                    self.suicide_count = self.suicide_count + 1;

                    sprint(self, "You cannot change teams.\n");
                    stuffcmd(self, "color ");
                    n = self.steam - 1;
                    s = ftos(n);
                    stuffcmd(self, s);
                    stuffcmd(self, "\n");
                    self.team = self.steam;
                    return;
                }
            } else {
                // If we're on an illegal team, force a change.
                self.steam = -50;
            }
        }

        if(self.steam > 0) {
            // case base respawn
            if(self.ctf_killed != 1)
                self.ctf_killed = 2;
            T_Damage(self,self,self,1000);  // Kill the player
        }
        self.frags = 0;                 // Zero out frags
        TeamCheckTeam(); // re-assignment
    }
};

/* Toss out a backpack containing some ammo from your current weapon,
and any weapons you don't have.
*/
void() TossBackpack =
{
    local entity     item;

    // If we don't have any ammo, return
    if(self.currentammo <= 0)
        return;

    // only valid in teamplay modes
    if(teamplay < 1)
        return;

    item = spawn();

    // TODO VR: (P1) rewrite for VR
    /*

    // See if you have the Shotgun or Super Shotgun on
    if((self.weapon == IT_SHOTGUN) || (self.weapon == IT_SUPER_SHOTGUN)) {
        if(self.ammo_shells >= 20 ) {
            item.ammo_shells = 20;
            self.ammo_shells = self.ammo_shells - 20;
        }
        else
        {
            item.ammo_shells = self.ammo_shells;
            self.ammo_shells = 0;
        }
    }

    // See if you have neither the Shotgun or Super Shotgun
    if( !(self.items & IT_SHOTGUN) && !(self.items & IT_SUPER_SHOTGUN)) {
        if(self.ammo_shells >= 20 ) {
            item.ammo_shells = 20;
            self.ammo_shells = self.ammo_shells - 20;
        } else {
            item.ammo_shells = self.ammo_shells;
            self.ammo_shells = 0;
        }
    }

    // See if we are using a nailgun
    if((self.weapon == IT_NAILGUN) || (self.weapon == IT_SUPER_NAILGUN) ) {
        if(self.ammo_nails >= 20 ) {
            item.ammo_nails = 20;
            self.ammo_nails = self.ammo_nails - 20;
        } else {
            item.ammo_nails = self.ammo_nails;
            self.ammo_nails = 0;
        }
    }

    // See if we are using the lava nailguns
    if((self.weapon == IT_LAVA_NAILGUN) || (self.weapon == IT_LAVA_SUPER_NAILGUN) ) {
        if(self.ammo_lava_nails >= 20 ) {
            item.ammo_lava_nails = 20;
            self.ammo_lava_nails = self.ammo_lava_nails - 20;
        } else {
            item.ammo_lava_nails = self.ammo_lava_nails;
            self.ammo_lava_nails = 0;
        }
    }

    // Check to see if we have neither nailgun
    if(!(self.items & IT_NAILGUN) && !(self.items & IT_SUPER_NAILGUN)) {
        // put both regular and lava nails in
        if(self.ammo_nails >= 20) {
            item.ammo_nails = 20;
            self.ammo_nails = self.ammo_nails - 20;
        } else {
            item.ammo_nails = self.ammo_nails;
            self.ammo_nails = 0;
        }
        if(self.ammo_lava_nails >= 20 ) {
            item.ammo_lava_nails = 20;
            self.ammo_lava_nails = self.ammo_lava_nails - 20;
        } else {
            item.ammo_lava_nails = self.ammo_lava_nails;
            self.ammo_lava_nails = 0;
        }
    }

    // See if we are using a grenade or rocket launcher
    if((self.weapon == IT_GRENADE_LAUNCHER) || (self.weapon == IT_ROCKET_LAUNCHER)) {
        if(self.ammo_rockets >= 10 ) {
            item.ammo_rockets = 10;
            self.ammo_rockets = self.ammo_rockets - 10;
        } else {
            item.ammo_rockets = self.ammo_rockets;
            self.ammo_rockets = 0;
        }
    }

    // See if we are using the multi grenade or rocket launcher
    if((self.weapon == IT_MULTI_GRENADE) || (self.weapon == IT_MULTI_ROCKET)) {
        if(self.ammo_multi_rockets >= 10 ) {
            item.ammo_multi_rockets = 10;
            self.ammo_multi_rockets = self.ammo_multi_rockets - 10;
        } else {
            item.ammo_multi_rockets = self.ammo_multi_rockets;
            self.ammo_multi_rockets = 0;
        }
    }

    // See if we have neither the Grenade or rocket launcher
    if( !(self.items & IT_GRENADE_LAUNCHER) && !(self.items & IT_ROCKET_LAUNCHER) ) {
        if(self.ammo_rockets >= 10 ) {
            item.ammo_rockets = 10;
            self.ammo_rockets = self.ammo_rockets - 10;
        } else {
            item.ammo_rockets = self.ammo_rockets;
            self.ammo_rockets = 0;
        }
        if(self.ammo_multi_rockets >= 10 ) {
            item.ammo_multi_rockets = 10;
            self.ammo_multi_rockets = self.ammo_multi_rockets - 10;
        } else {
            item.ammo_multi_rockets = self.ammo_multi_rockets;
            self.ammo_multi_rockets = 0;
        }
    }

    // See if we're using the lightning gun
    if(self.weapon == IT_LIGHTNING) {
        if(self.ammo_cells >= 20 ) {
            item.ammo_cells = 20;
            self.ammo_cells = self.ammo_cells - 20;
        } else {
            item.ammo_cells = self.ammo_cells;
            self.ammo_cells = 0;
        }
    }

    // see if we are using the plasma gun
    if(self.weapon == IT_PLASMA_GUN) {
        if(self.ammo_plasma >= 10 ) {
            item.ammo_plasma = 10;
            self.ammo_plasma = self.ammo_plasma - 10;
        } else {
            item.ammo_plasma = self.ammo_plasma;
            self.ammo_plasma = 0;
        }
    }

    // see if we don't have the lightning gun
    if(!(self.items & IT_LIGHTNING)) {
        if(self.ammo_cells >= 20 ) {
            item.ammo_cells = 20;
            self.ammo_cells = self.ammo_cells - 20;
        } else {
            item.ammo_cells = self.ammo_cells;
            self.ammo_cells = 0;
        }
        if(self.ammo_plasma >= 10 ) {
            item.ammo_plasma = 10;
            self.ammo_plasma = self.ammo_plasma - 10;
        } else {
            item.ammo_plasma = self.ammo_plasma;
            self.ammo_plasma = 0;
        }
    }

    if(item.ammo_shells + item.ammo_nails + item.ammo_lava_nails +
        item.ammo_rockets + item.ammo_multi_rockets + item.ammo_cells +
        item.ammo_plasma == 0) {
        sprint(self, "No ammo available!\n");
        remove(item);
        return;
    }

    */

    item.owner = self;
    makevectors(self.v_angle);

    setorigin(item, self.origin + '0 0 16');
    item.velocity = aim(self, 1000);
    item.velocity = item.velocity * 500;
    item.flags = FL_ITEM;
    item.solid = SOLID_TRIGGER;
    item.movetype = MOVETYPE_BOUNCE;

    setmodel (item, "progs/backpack.mdl");
    setsize(item, '-16 -16 0', '16 16 56');
    item.touch = BackpackTouch;
    item.nextthink = time + 120;    // remove after 2 minutes
    item.think = SUB_Remove;

    W_SetCurrentAmmo();
};

void() Team_weapon_touch =
{
    // TODO VR: (P1) rewrite for VR
    /*

    local    float    hadammo, best, new, old;
    local entity stemp;

    if(!(other.flags & FL_CLIENT))
        return;
    // Don't let the owner pick up his own weapon for a second.
    if((other == self.owner) && ((self.nextthink - time) > 119))
        return;

// if the player was using his best weapon, change up to the new one if better
    stemp = self;
    self = other;
    best = W_BestWeapon();
    self = stemp;

    if(self.classname == "weapon_nailgun")
        new = IT_NAILGUN;
    else if(self.classname == "weapon_supernailgun")
        new = IT_SUPER_NAILGUN;
    else if(self.classname == "weapon_supershotgun")
        new = IT_SUPER_SHOTGUN;
    else if(self.classname == "weapon_rocketlauncher")
        new = IT_ROCKET_LAUNCHER;
    else if(self.classname == "weapon_grenadelauncher")
        new = IT_GRENADE_LAUNCHER;
    else if(self.classname == "weapon_lightning")
        new = IT_LIGHTNING;
    else
        objerror ("Team_weapon_touch: unknown classname");

    sprint (other, "You got the ");
    sprint (other, self.netname);
    sprint (other, "\n");
// weapon touch sound
    sound (other, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
    stuffcmd (other, "bf\n");

    bound_other_ammo ();

// change to the weapon
    old = other.items;
    other.items = other.items | new;

    remove(self);
    self = other;

    if(!deathmatch)
        self.weapon = new;
    else
        Deathmatch_Weapon (old, new);

    EnableComboWeapons(self);

    W_SetCurrentAmmo();

    // dropped weapon doesn't target, so no need to call UseTargets

    */
};

void() TossWeapon =
{
    // TODO VR: (P1) rewrite for VR

    /*

    local entity item;

    if(deathmatch != 1)
        return;  // only in deathmatch 1

    // only valid in teamplay modes
    if(teamplay < 1)
        return;

    if((self.weapon == IT_AXE) || (self.weapon == IT_SHOTGUN) ||
        (self.weapon == IT_GRAPPLE))
        return;

    item = spawn();
    item.owner = self;
    makevectors(self.v_angle);
    item.weapon = 0;

    setorigin(item, self.origin + '0 0 16');
    item.velocity = aim(self, 1000);
    item.velocity = item.velocity * 500;
    item.flags = FL_ITEM;
    item.solid = SOLID_TRIGGER;
    item.movetype = MOVETYPE_BOUNCE;

    if(self.weapon == IT_SUPER_SHOTGUN)
    {
        setmodel (item, "progs/g_shot.mdl");
        item.weapon = IT_SUPER_SHOTGUN;
        item.netname = "Double-barrelled Shotgun";
        item.classname = "weapon_supershotgun";
        self.items = self.items - IT_SUPER_SHOTGUN;
    }

    if(self.weapon == IT_NAILGUN || self.weapon == IT_LAVA_NAILGUN )
    {
        setmodel (item, "progs/g_nail.mdl");
        item.weapon = IT_NAILGUN;
        item.netname = "nailgun";
        item.classname = "weapon_nailgun";
        self.items = self.items - (self.items & (IT_NAILGUN | IT_LAVA_NAILGUN));
    }

    if(self.weapon == IT_SUPER_NAILGUN || self.weapon == IT_LAVA_SUPER_NAILGUN)
    {
        setmodel (item, "progs/g_nail2.mdl");
        item.weapon = IT_SUPER_NAILGUN;
        item.netname = "Super Nailgun";
        item.classname = "weapon_supernailgun";
        self.items = self.items - (self.items & (IT_SUPER_NAILGUN | IT_LAVA_SUPER_NAILGUN));
    }

    if(self.weapon == IT_GRENADE_LAUNCHER || self.weapon == IT_MULTI_GRENADE)
    {
        setmodel (item, "progs/g_rock.mdl");
        item.weapon = IT_GRENADE_LAUNCHER;
        item.netname = "Grenade Launcher";
        item.classname = "weapon_grenadelauncher";
        self.items = self.items - (self.items & (IT_GRENADE_LAUNCHER | IT_MULTI_GRENADE));
    }

    if(self.weapon == IT_ROCKET_LAUNCHER || self.weapon == IT_MULTI_ROCKET)
    {
        setmodel (item, "progs/g_rock2.mdl");
        item.weapon = IT_ROCKET_LAUNCHER;
        item.netname = "Rocket Launcher";
        item.classname = "weapon_rocketlauncher";
        self.items = self.items - (self.items & (IT_ROCKET_LAUNCHER | IT_MULTI_ROCKET));
    }

    if(self.weapon == IT_LIGHTNING || self.weapon == IT_PLASMA_GUN)
    {
        setmodel (item, "progs/g_light.mdl");
        item.weapon = IT_LIGHTNING;
        item.netname = "Thunderbolt";
        item.classname = "weapon_lightning";
        self.items = self.items - (self.items & (IT_LIGHTNING | IT_PLASMA_GUN));
    }
    setsize(item, '-16 -16 0', '16 16 56');
    item.touch = Team_weapon_touch;
    item.think = SUB_Remove;
    item.nextthink = time + 120;

    self.weapon = W_BestWeapon();
    W_SetCurrentAmmo();

    */
};

void(entity flg) RegenFlag =
{
    flg.movetype = MOVETYPE_TOSS;
    flg.solid = SOLID_TRIGGER;
    sound (flg, CHAN_VOICE, "items/itembk2.wav", 1, ATTN_NORM);    // play respawn sound
    setorigin(flg, flg.oldorigin);
    flg.angles = flg.mangle;
    flg.cnt = FLAG_AT_BASE; // it's at home base
    flg.owner = world;
};

void(entity flg) TeamReturnFlag =
{
    local entity p;
    local string n;

    RegenFlag(flg);

    p = find(world, classname, "player");
    while (p != world) {
        if(teamplay == TEAM_CTF_ONEFLAG) // one flag mode?
            centerprint(p, "The flag has been returned!\n");
        else {
            if(teamplay == TEAM_CTF_ALT) {
                if(flg.team == TEAM1)
                    centerprint(p, "��� flag has been returned to base!\n");
                else if(flg.team == TEAM2)
                    centerprint(p, "���� flag has been returned to base!\n");
                else
                    centerprint(p, "Some flag has been returned to base!\n");
            } else {
                if(p.steam != flg.team)
                    centerprint(p, "Enemy flag has been returned to base!\n");
                else if(p.steam == flg.team)
                    centerprint(p, "Your flag has been returned to base!\n");
            }
        }
        p = find(p, classname, "player");
    }
};

void () TeamRegenFlags =
{
    local entity f;

    if(teamplay == TEAM_CTF_ONEFLAG) {
        f = find(world, classname, "item_flag");
        if(f != world)
            RegenFlag(f);
        return;
    }

    f = find(world, classname, "item_flag_team1");
    if(f != world)
        RegenFlag(f);
    f = find(world, classname, "item_flag_team2");
    if(f != world)
        RegenFlag(f);
};

void(entity flg) TeamDropFlag =
{
    local entity item, f, oself;
    local entity p;

    p = flg.owner;

    bprint(p.netname);
    if(teamplay == TEAM_CTF_ONEFLAG)
        bprint(" ���� the flag!\n");
    else {
        if(flg.team == TEAM1)
            bprint(" ���� the ��� flag!\n"); // red
        else
            bprint(" ���� the ���� flag!\n"); // blue
    }

    setorigin(flg, p.origin - '0 0 24');
    flg.cnt = FLAG_DROPPED;
    flg.velocity_z = 300;
    flg.velocity_x = 0;
    flg.velocity_y = 0;
    flg.flags = FL_ITEM;
    flg.solid = SOLID_TRIGGER;
    flg.movetype = MOVETYPE_TOSS;
    setsize(flg, '-16 -16 0', '16 16 74');
    // return it after so long
    flg.super_time = time + CTF_FLAG_RETURN_TIME;
};

void(entity player) TeamDropFlagOfPlayer =
{
    local string kn;
    local entity e;

    if(teamplay == TEAM_CTF_ONEFLAG && (player.ctf_flags & CTF_FLAG_FLAG))
        kn = "item_flag";
    else if(player.ctf_flags & CTF_FLAG_TEAM1)
        kn = "item_flag_team1";
    else if(player.ctf_flags & CTF_FLAG_TEAM2)
        kn = "item_flag_team2";
    else
        return; // doesn't have a flg
    player.ctf_flags = player.ctf_flags -
        (player.ctf_flags & (CTF_FLAG_FLAG | CTF_FLAG_TEAM1 | CTF_FLAG_TEAM2));
    e = find(world, classname, kn);
    if(e != world)
        TeamDropFlag(e);
};

// A flag was touched.  In one flag mode, the player always just picks it up
// (the flagbase models at each base will cause the score).  In two flag mode,
// it could be the guy returning his flag, or the guy getting the enemy flag
// Three team mode gets more complicated as the third team touches the
// flagbase to score
void() TeamFlagTouch =
{
    local entity p, oself;

    if(other.classname != "player")
        return;
    if(other.health <= 0)
        return;

    if(other.team != other.steam)
        return; // something is fishy, somebody is playing with colors

    if(self.cnt == FLAG_CARRIED)
        return; // huh?

    // Ok, first up, let's do it for one flag mode
    if(teamplay == TEAM_CTF_ONEFLAG) {
        // in one flag mode, we always pick up the flag.  The touch of the
        // flagbase entities does the scoring
        bprint(other.netname);
        bprint(" ��� the flag!\n");
        if(CTF_FLAG_BONUS)
            other.frags = other.frags + CTF_FLAG_BONUS;
        //centerprint(other, "YOU GOT THE FLAG\n\nTAKE IT TO ENEMY BASE\n");
          centerprint(other, "��� ��� ��� ����\n\n���� �� �� ����� ����\n");

        sound (other, CHAN_ITEM, self.noise, 1, ATTN_NORM);

        other.ctf_flags = other.ctf_flags | CTF_FLAG_FLAG;
        other.items = other.items | self.items;
        other.ctf_flagsince = time;

        // pick up the flag
        self.cnt = FLAG_CARRIED;
        self.movetype = MOVETYPE_NOCLIP;
        self.solid = SOLID_NOT;
        self.owner = other;

        p = find(world, classname, "player");
        while (p != world) {
            if(p != other)
                centerprint(p, "The flag has been taken!\n");
            p = find(p, classname, "player");
        }
        return;
    }

    // Regular and Alt CTF mode
    if(teamplay != TEAM_CTF && teamplay != TEAM_CTF_ALT)
        return; // odd, but ignore it

    if(self.team == other.steam) {
        // same team, if the flag is *not* at the base, return
        // it to base.  we overload the 'cnt' field for this
        if(self.cnt == FLAG_AT_BASE) {
            // the flag is at home base.  if the player has the enemy
            // flag, he's just won!
            if((self.team == TEAM1 && (other.ctf_flags & CTF_FLAG_TEAM2)) ||
                (self.team == TEAM2 && (other.ctf_flags & CTF_FLAG_TEAM1))) {
                bprint(other.netname);
                if(other.team == TEAM1)
                    bprint(" �������� the ���� flag!\n"); // blue
                else
                    bprint(" �������� the ��� flag!\n"); // red
                other.items = other.items - (other.items & (IT_KEY1 | IT_KEY2));

                sound (other, CHAN_VOICE, "misc/flagcap.wav", 1, ATTN_NONE);

                // other gets another 10 frag bonus
                other.frags = other.frags + CTF_CAPTURE_BONUS;

                // Ok, let's do the player loop, hand out the bonuses
                p = find(world, classname, "player");
                while (p != world) {
                    self = p;
                    if(self.team == other.team && self != other)
                        self.frags = self.frags + CTF_TEAM_BONUS;
                    if(self.team != other.team) {
                        if(self.team == TEAM3)
                            if(other.team == TEAM1)
                                centerprint(self, "���� flag was captured!\n");
                            else
                                centerprint(self, "��� flag was captured!\n");
                        else
                            centerprint(self, "Your flag was captured!\n");
                        // reset the last_hurt_carrier variable in all enemy players, so that you don't get
                        // bonuses for defending the flag carrier if the flag carrier has already
                        // completed a capture
                        self.ctf_lasthurtcarrier = -5;
                    } else if(self.team == other.team) {
                        // done to all players on the capturing team
                        centerprint(self, "Your team captured the flag!\n");
                        // award extra points for capture assists
                        if(self.ctf_lastreturnedflag + CTF_RETURN_FLAG_ASSIST_TIMEOUT > time) {
                            bprint(self.netname);
                            if(self.team == TEAM1)
                                bprint(" gets an assist for returning the ��� flag!\n");
                            else
                                bprint(" gets an assist for returning the ���� flag!\n");
                            self.frags = self.frags + CTF_RETURN_FLAG_ASSIST_BONUS;
                        }
                        if(self.ctf_lastfraggedcarrier + CTF_FRAG_CARRIER_ASSIST_TIMEOUT > time) {
                            bprint(self.netname);
                            bprint(" gets an assist for fragging the flag carrier!\n");
                            self.frags = self.frags + CTF_FRAG_CARRIER_ASSIST_BONUS;
                        }
                    }
                    self.ctf_flags = self.ctf_flags - (self.ctf_flags &
                        (CTF_FLAG_TEAM1 | CTF_FLAG_TEAM2));
                    p = find(p, classname, "player");
                }
                // respawn flags
                TeamRegenFlags();
                return;
            }
            return; // its at home base already
        }
        // hey, its not home.  return it by teleporting it back
        bprint(other.netname);
        if(other.team == TEAM1)
            bprint(" �������� the ��� flag!\n"); // red
        else
            bprint(" �������� the ���� flag!\n"); // blue
        other.frags = other.frags + CTF_RECOVERY_BONUS;
        other.ctf_lastreturnedflag = time;
        sound (other, CHAN_ITEM, self.noise1, 1, ATTN_NORM);
        TeamReturnFlag(self);
        return;
    }

    // if we have any flags, leave now
    if(other.ctf_flags & (CTF_FLAG_TEAM1 | CTF_FLAG_TEAM2))
        return;

    // hey, its not our flag, pick it up
    bprint(other.netname);
    if(self.team == TEAM1)
        bprint(" ��� the ��� flag!\n"); // red
    else
        bprint(" ��� the ���� flag!\n"); // blue
    other.frags = other.frags + CTF_FLAG_BONUS;

    // if in three team, messages are a little different
    if(other.team == TEAM3) {
//        centerprint(other, "YOU GOT THE ENEMY FLAG\n\nRETURN IT TO THE OTHER BASE!\n");
        centerprint(other, "��� ��� ��� ����� ����\n\n������ �� �� ��� ����� ����\n");
    } else {
//        centerprint(other, "YOU GOT THE ENEMY FLAG!\n\nRETURN TO BASE!\n");
        centerprint(other, "��� ��� ��� ����� ����\n\n������ �� ����\n");
    }
    sound (other, CHAN_ITEM, self.noise, 1, ATTN_NORM);

    if(self.team == TEAM1)
        other.ctf_flags = other.ctf_flags | CTF_FLAG_TEAM1;
    else
        other.ctf_flags = other.ctf_flags | CTF_FLAG_TEAM2;
    other.items = other.items | self.items;

    other.ctf_flagsince = time;

    // pick up the flag
    self.cnt = FLAG_CARRIED;
    self.movetype = MOVETYPE_NOCLIP;
    self.solid = SOLID_NOT;
    self.owner = other;

    // PGM Fix - 03/06/97 Made it work right in three team.
    p = find(world, classname, "player");
    while (p != world)
    {
        if(p != other)
        {
            if(p.steam == self.team)
            {
                centerprint(p, "Your flag has been taken!\n");
            }
            else if(p.steam == other.team)
            {
                if(self.team == TEAM1)
                    centerprint(p, "Your team has the ��� flag!\n");    // Red Flag
                else
                    centerprint(p, "Your team has the ���� flag!\n");    // Blue Flag
            }
            else
            {
                if(self.team == TEAM1)
                    if(other.steam == TEAM2)
                        centerprint(p, "���� team has the ��� flag!\n"); // Blue has red
                    else // must be team3
                        centerprint(p, "���� team has the ��� flag!\n"); // grey has red
                else
                    if(other.steam == TEAM1)
                        centerprint(p, "��� team has the ���� flag!\n"); // red has blue
                    else // must be team3
                        centerprint(p, "���� team has the ���� flag!\n");// grey has blue
            }
        }
        p = find(p, classname, "player");
    }
};

// A flagbase was touched.  In one flag mode, this is how a capture is made.
// in three team mode, only team3 touches this.  In regular CTF, this is
// ignored
void() TeamFlagBaseTouch =
{
    local entity p, oself, f;

    if(other.classname != "player")
        return;
    if(other.health <= 0)
        return;
    if(other.team != other.steam)
        return; // something is fishy, somebody is playing with colors

    // Ok, first up, let's do it for one flag mode
    if(teamplay == TEAM_CTF_ONEFLAG) {
        // ok, if they guy touching it has the flag and this is his
        // base, capture!
        if(((self.team == TEAM1 && other.steam == TEAM2) ||
            (self.team == TEAM2 && other.steam == TEAM1)) &&
            (other.ctf_flags & CTF_FLAG_FLAG)) {
            // he just touched enemy base, capture it
            bprint(other.netname);
            bprint(" �������� the flag!\n"); // blue
            other.items = other.items - (other.items & (IT_KEY1 | IT_KEY2));
            sound (other, CHAN_VOICE, "misc/flagcap.wav", 1, ATTN_NONE);

            // other gets another 10 frag bonus
            other.frags = other.frags + CTF_CAPTURE_BONUS;

            // Ok, let's do the player loop, hand out the bonuses
            p = find(world, classname, "player");
            while (p != world) {
                self = p;
                if(self.team == other.team && self != other)
                    self.frags = self.frags + CTF_TEAM_BONUS;
                if(self.team != other.team) {
                    self.ctf_lasthurtcarrier = -5;
                } else if(self.team == other.team) {
                    // award extra points for capture assists
                    if(self.ctf_lastfraggedcarrier + CTF_FRAG_CARRIER_ASSIST_TIMEOUT > time) {
                        bprint(self.netname);
                        bprint(" gets an assist for fragging the flag carrier!\n");
                        self.frags = self.frags + CTF_FRAG_CARRIER_ASSIST_BONUS;
                    }
                }
                self.ctf_flags = self.ctf_flags - (self.ctf_flags & CTF_FLAG_FLAG);
                p = find(p, classname, "player");
            }
            // respawn flags
            TeamRegenFlags();
            return;
        }
    }

    // in three team mode (TEAM_CTF_ALT) and we're on team3, see if we capture
    if(teamplay != TEAM_CTF_ALT || other.steam != TEAM3)
        return;

    if(((other.ctf_flags & CTF_FLAG_TEAM1) && (self.team == TEAM2)) ||
        ((other.ctf_flags & CTF_FLAG_TEAM2) && (self.team == TEAM1))) {
        // third team captured

        bprint(other.netname);
        if(self.team == TEAM1)
            bprint(" �������� the ���� flag!\n"); // blue
        else
            bprint(" �������� the ��� flag!\n"); // red
        other.items = other.items - (other.items & (IT_KEY1 | IT_KEY2));

        sound (other, CHAN_VOICE, "misc/flagcap.wav", 1, ATTN_NONE);

        // other gets another 10 frag bonus
        other.frags = other.frags + CTF_ALT_CAPTURE_BONUS;

        // Ok, let's do the player loop, hand out the bonuses
        p = find(world, classname, "player");
        while (p != world) {
            self = p;
            if(self.team == other.team && self != other)
                self.frags = self.frags + CTF_ALT_TEAM_BONUS;
            if(self.team != other.team) {
                if((other.ctf_flags & CTF_FLAG_TEAM1) && self.team == TEAM1)
                    centerprint(self, "Your flag was captured!\n");
                else if((other.ctf_flags & CTF_FLAG_TEAM2) && self.team == TEAM2)
                    centerprint(self, "The enemy flag was captured!\n");
                // reset the last_hurt_carrier variable in all enemy players, so that you don't get
                // bonuses for defending the flag carrier if the flag carrier has already
                // completed a capture
                self.ctf_lasthurtcarrier = -5;
            } else if(self.team == other.team) {
                // done to all players on the capturing team
                centerprint(self, "Your team captured the flag!\n");
            }
            p = find(p, classname, "player");
        }
        // respawn flags
        if(other.ctf_flags & CTF_FLAG_TEAM1) {
            f = find(world, classname, "item_flag_team1");
            if(f != world)
                RegenFlag(f);
        } else { // must be flag2
            f = find(world, classname, "item_flag_team2");
            if(f != world)
                RegenFlag(f);
        }
        other.ctf_flags = other.ctf_flags - (other.ctf_flags &
            (CTF_FLAG_TEAM1 | CTF_FLAG_TEAM2));
        return;
    }
};


void() TeamFlagThink =
{
    local entity e;
    local vector v;
    local float f;
    local string s;

    self.nextthink = time + 0.1;

    if(self.cnt == FLAG_AT_BASE)
        return; // just sitting around waiting to be picked up

    if(self.cnt == FLAG_DROPPED) {
        if(time - self.super_time > CTF_FLAG_RETURN_TIME)
            TeamReturnFlag(self);
        return;
    }

    if(self.cnt != FLAG_CARRIED)
        objerror("Flag in invalid state\n");

    e = self.owner;
    if(e.classname != "player" || e.deadflag) {
        TeamDropFlag(self);
        return;
    }

    if(teamplay == TEAM_CTF_ONEFLAG && !(e.ctf_flags & CTF_FLAG_FLAG)) {
        TeamDropFlag(self);
        return;
    }
    // must be TEAM_CTF or TEAM_CTF_ALT
    if((!(e.ctf_flags & CTF_FLAG_TEAM1) && self.team == TEAM1) ||
        (!(e.ctf_flags & CTF_FLAG_TEAM2) && self.team == TEAM2)) {
        TeamDropFlag(self);
        return;
    }

    makevectors(e.angles);
    v = v_forward;
    v_z = (-1) * v_z; // reverse z component

    f = 14;
    if(self.owner.frame >= 29 && self.owner.frame <= 40) {
        if(self.owner.frame >= 29 && self.owner.frame <= 34) { //axpain
            if      (self.owner.frame == 29) f = f + 2;
            else if(self.owner.frame == 30) f = f + 8;
            else if(self.owner.frame == 31) f = f + 12;
            else if(self.owner.frame == 32) f = f + 11;
            else if(self.owner.frame == 33) f = f + 10;
            else if(self.owner.frame == 34) f = f + 4;
        } else if(self.owner.frame >= 35 && self.owner.frame <= 40) { // pain
            if      (self.owner.frame == 35) f = f + 2;
            else if(self.owner.frame == 36) f = f + 10;
            else if(self.owner.frame == 37) f = f + 10;
            else if(self.owner.frame == 38) f = f + 8;
            else if(self.owner.frame == 39) f = f + 4;
            else if(self.owner.frame == 40) f = f + 2;
        }
    } else if(self.owner.frame >= 103 && self.owner.frame <= 118) {
        if      (self.owner.frame >= 103 && self.owner.frame <= 104) f = f + 6;  //nailattack
        else if(self.owner.frame >= 105 && self.owner.frame <= 106) f = f + 6;  //light
        else if(self.owner.frame >= 107 && self.owner.frame <= 112) f = f + 7;  //rocketattack
        else if(self.owner.frame >= 112 && self.owner.frame <= 118) f = f + 7;  //shotattack
    }
    self.origin = e.origin + '0 0 -16' - f*v + v_right * 22;
    self.angles = e.angles + '0 0 -45';
    setorigin (self, self.origin);
    self.nextthink = time + 0.01;
};

void() TeamFlagStatusReport =
{
    local entity flag1, flag2, p;
    local string n;

    if(!deathmatch)
        return;

    if(teamplay != TEAM_CTF && teamplay != TEAM_CTF_ONEFLAG &&
        teamplay != TEAM_CTF_ALT) {
        sprint(self, "Capture the Flag is not enabled.\n");
        return;
    }

    if(teamplay == TEAM_CTF_ONEFLAG) {
        flag1 = find (world, classname, "item_flag");
        if(flag1 == world)
            sprint(self, "The flag is missing!\n");
        else if(flag1.cnt == FLAG_AT_BASE)
            sprint(self, "The flag is at base!\n");
        else if(flag1.cnt == FLAG_DROPPED)
            sprint(self, "The flag is lying about!\n");
        else if(flag1.cnt == FLAG_CARRIED) {
            if(flag1.owner == self)
                sprint(self, "You have the flag!\n");
            else {
                sprint(self, flag1.owner.netname);
                n = GetCTFTeam(flag1.owner.steam);
                sprint(self, " of the ");
                sprint(self, n);
                sprint(self, " team has the flag!\n");
            }
        } else
            sprint(self, "The flag is screwed up!\n");
        return;
    }

    // normal CTF
    if(teamplay == TEAM_CTF) {
        // Find the flags at home base
        flag1 = find (world,classname, "item_flag_team1");
        flag2 = find (world,classname, "item_flag_team2");

        // If on team 2 switch meanings of flags
        if(self.team != TEAM1) {
            p = flag1;
            flag1 = flag2;
            flag2 = p;
        }

        if(flag1 != world && flag1.cnt == FLAG_CARRIED) {
            sprint(self, flag1.owner.netname);
            sprint(self, " has your flag. ");
        } else {
            sprint(self, "Your flag is ");
            if(flag1 == world)
                sprint(self, "missing!\n");
            if(flag1.cnt == FLAG_AT_BASE)
                sprint(self, "in your base.\n");
            else if(flag1.cnt == FLAG_DROPPED)
                sprint(self, "lying about.\n");
            else
                sprint(self, " corrupt.\n");
        }

        if(flag2 != world && flag2.cnt == FLAG_CARRIED) {
            if(self == flag2.owner)
                sprint(self, "You have the enemy flag.\n");
            else {
                sprint(self, flag2.owner.netname);
                sprint(self, " has the enemy flag.\n");
            }
        } else {
            sprint(self, "The enemy flag is ");
            if(flag2 == world)
                sprint(self, "missing!\n");
            if(flag2.cnt == FLAG_AT_BASE)
                sprint(self, "in their base.\n");
            else if(flag2.cnt == FLAG_DROPPED)
                sprint(self, "lying about.\n");
            else
                sprint(self, " corrupt.\n");
        }
    }

    // three team CTF
    if(teamplay == TEAM_CTF_ALT) {
        // Find the flags at home base
        flag1 = find (world,classname, "item_flag_team1");
        flag2 = find (world,classname, "item_flag_team2");

        if(flag1 != world && flag1.cnt == FLAG_CARRIED) {
            if(flag1.owner == self)
                sprint(self, "You have the ��� flag!\n");
            else {
                sprint(self, flag1.owner.netname);
                n = GetCTFTeam(flag1.owner.steam);
                sprint(self, " of the ");
                sprint(self, n);
                sprint(self, " team has the ��� flag.\n");
            }
        } else {
            if(self.steam == flag1.team)
                sprint(self, "Your flag is ");
            else
                sprint(self, "��� flag is ");
            if(flag1 == world)
                sprint(self, "missing!\n");
            if(flag1.cnt == FLAG_AT_BASE)
                sprint(self, "at base.\n");
            else if(flag1.cnt == FLAG_DROPPED)
                sprint(self, "lying about.\n");
            else
                sprint(self, " corrupt.\n");
        }

        if(flag2 != world && flag2.cnt == FLAG_CARRIED) {
            if(flag2.owner == self)
                sprint(self, "You have the ���� flag.\n");
            else {
                sprint(self, flag2.owner.netname);
                n = GetCTFTeam(flag2.owner.steam);
                sprint(self, " of the ");
                sprint(self, n);
                sprint(self, " team has the ���� flag.\n");
            }
        } else {
            if(self.steam == flag1.team)
                sprint(self, "Your flag is ");
            else
                sprint(self, "���� flag is ");
            if(flag2 == world)
                sprint(self, "missing!\n");
            if(flag2.cnt == FLAG_AT_BASE)
                sprint(self, "at base.\n");
            else if(flag2.cnt == FLAG_DROPPED)
                sprint(self, "lying about.\n");
            else
                sprint(self, " corrupt.\n");
        }
    }
};

/////////////////////////////////////////////////////////////////////////

$cd id1/models/flag
$base base
$skin skin

void() place_flag = {
    self.mdl = self.model;        // so it can be restored on respawn
    self.flags = FL_ITEM;        // make extra wide
    self.solid = SOLID_TRIGGER;
    self.movetype = MOVETYPE_TOSS;
    self.velocity = '0 0 0';
    self.origin_z = self.origin_z + 6;
    self.think = TeamFlagThink;
    self.touch = TeamFlagTouch;
    self.nextthink = time + 0.1;
    self.cnt = FLAG_AT_BASE;
    self.mangle = self.angles;
    self.effects = self.effects | EF_DIMLIGHT;
    if(!droptofloor()) {
        dprint ("Flag fell out of level at ");
        dprint (vtos(self.origin));
        dprint ("\n");
        remove(self);
        return;
    }
    self.oldorigin = self.origin; // save for flag return
};

void(entity flg, string cname) place_flagbase =
{
    local entity oself;

    teamplay = cvar("teamplay");

    oself = self;
    self = spawn();
    self.classname = cname;
    setorigin(self, flg.origin);
    self.angles = flg.angles;
    precache_model ("progs/ctfbase.mdl");
    setmodel (self, "progs/ctfbase.mdl");
    self.skin = flg.skin;
    self.team = flg.team;
//    setsize(self, '-8 -8 -4', '8 8 4');
    setsize(self, '-8 -8 0', '8 8 8');
    self.flags = FL_ITEM;        // make extra wide
    self.movetype = MOVETYPE_TOSS;
    self.velocity = '0 0 0';
    self.origin_z = self.origin_z + 6;
    if(teamplay == TEAM_CTF_ONEFLAG || teamplay == TEAM_CTF_ALT) {
        self.solid = SOLID_TRIGGER;
        self.touch = TeamFlagBaseTouch;
    } else
        self.solid = SOLID_NOT;
    if(!droptofloor()) {
        dprint ("Flagbase fell out of level at ");
        dprint (vtos(self.origin));
        dprint ("\n");
        remove(self);
        self = oself;
        return;
    }
    self = oself;
};

// ZOID Capture the flag
/*QUAKED item_flag_team1 (0 .5 .8) (-8 -8 -32) (8 8 24)
red team flag
Only appears in CTF teamplay

The flag waves 90 degrees off from the entity angle. If
you want the flag to point at 180, set then entity angle
to 90.
*/
void() item_flag_team1 =
{
    if(!deathmatch) {
        remove(self);
        return;
    }

    teamplay = cvar("teamplay");

    if(teamplay == TEAM_CTF_ONEFLAG) {
        precache_sound ("misc/flagtk.wav");        // flag taken
        precache_sound ("misc/flagcap.wav");    // flag capture
        self.skin = 0;
        self.team = TEAM1;
        place_flagbase(self, "item_flagbase_team1");
        remove(self);
        return;
    }

    if(teamplay == TEAM_CTF || teamplay == TEAM_CTF_ALT) {
        self.team = TEAM1;
        self.items = IT_KEY2;
        precache_model ("progs/ctfmodel.mdl");
        setmodel (self, "progs/ctfmodel.mdl");
        self.skin = 0;
        precache_sound ("misc/flagtk.wav");        // flag taken
        precache_sound ("misc/flagcap.wav");    // flag capture
        precache_sound ("misc/flagret.wav");    // flag return
        self.noise = "misc/flagtk.wav";
        self.noise1 = "misc/flagret.wav";
        setsize(self, '-16 -16 0', '16 16 74');
        self.nextthink = time + 0.2;    // items start after other solids
        self.think = place_flag;
        place_flagbase(self, "item_flagbase_team1");
    } else {
        remove(self); // not teamplay or deathmatch
    }
};

/*QUAKED item_flag_team2 (0 .5 .8) (-8 -8 -32) (8 8 24)
blue team flag
Only appears in CTF teamplay

The flag waves 90 degrees off from the entity angle. If
you want the flag to point at 180, set then entity angle
to 90.
*/
void() item_flag_team2 =
{
    if(!deathmatch) {
        remove(self);
        return;
    }

    teamplay = cvar("teamplay");

    if(teamplay == TEAM_CTF_ONEFLAG) {
        precache_sound ("misc/flagtk.wav");        // flag taken
        precache_sound ("misc/flagcap.wav");    // flag capture
        self.team = TEAM2;
        self.skin = 1;
        place_flagbase(self, "item_flagbase_team2");
        remove(self);
        return;
    }

    if(teamplay == TEAM_CTF || teamplay == TEAM_CTF_ALT) {
        self.team = TEAM2;
        self.items = IT_KEY1;
        precache_model ("progs/ctfmodel.mdl");
        setmodel (self, "progs/ctfmodel.mdl");
        self.skin = 1;
        precache_sound ("misc/flagtk.wav");        // flag taken
        precache_sound ("misc/flagcap.wav");    // flag capture
        precache_sound ("misc/flagret.wav");    // flag return
        self.noise = "misc/flagtk.wav";
        self.noise1 = "misc/flagret.wav";
        setsize(self, '-16 -16 0', '16 16 74');
        self.nextthink = time + 0.2;    // items start after other solids
        self.think = place_flag;
        place_flagbase(self, "item_flagbase_team2");
    } else {
        remove(self); // not teamplay or deathmatch
    }
};

/*QUAKED item_flag (0 .5 .8) (-8 -8 -32) (8 8 24)
flag for OneTeam play.
Only appears in CTF teamplay

The flag waves 90 degrees off from the entity angle. If
you want the flag to point at 180, set then entity angle
to 90.
*/
void() item_flag =
{
    if(cvar("teamplay") != TEAM_CTF_ONEFLAG) {
        remove(self);
        return;
    }
    self.team = 0; // no team
    self.items = IT_KEY1 | IT_KEY2;
//    precache_model ("progs/flag.mdl");
//    setmodel (self, "progs/flag.mdl");
    precache_model ("progs/ctfmodel.mdl");        // PGM 01/18/97
    setmodel (self, "progs/ctfmodel.mdl");    // PGM 01/18/97
    self.skin = 2;
    precache_sound ("misc/flagtk.wav");        // flag taken
    precache_sound ("misc/flagcap.wav");    // flag capture
    precache_sound ("misc/flagret.wav");    // flag return
    self.noise = "misc/flagtk.wav";
    self.noise1 = "misc/flagret.wav";
    setsize(self, '-16 -16 0', '16 16 74');
    self.nextthink = time + 0.2;    // items start after other solids
    self.think = place_flag;
    place_flagbase(self, "item_flagbase");
};

/*QUAKED info_player_team1 (1 0 1) (-16 -16 -24) (16 16 24)
potential spawning position for CTF games team 1.
*/
void() info_player_team1 =
{
};

/*QUAKED info_player_team2 (1 0 1) (-16 -16 -24) (16 16 24)
potential spawning position for CTF games team 2.
*/
void() info_player_team2 =
{
};

void() TeamSetUpdate =
{
    nextteamupdtime = time + CTF_UPDATE_TIME;
};

void() TeamCheckUpdate =
{
    local float total1, total2, total3;
    local entity p;
    local string s;
    local string ts1, ts2, ts3;

    if(nextteamupdtime > time || teamplay < 1 || !deathmatch)
        return;

    TeamSetUpdate();

    // count up total
    total1 = total2 = total3 = 0;

    ts1 = GetCTFTeam(TEAM1);
    ts2 = GetCTFTeam(TEAM2);
    ts3 = GetCTFTeam(TEAM3);

    if(teamplay == TEAM_CTF || teamplay == TEAM_CTF_ALT || teamplay == TEAM_CTF_ONEFLAG) {
        p = find(world, classname, "player");
        while (p != world) {
            if(p.steam == TEAM1)
                total1 = total1 + p.frags;
            else if(p.steam == TEAM2)
                total2 = total2 + p.frags;
            else if(p.steam == TEAM3)
                total3 = total3 + p.frags;
            p = find(p, classname, "player");
        }

        if(teamplay == TEAM_CTF || teamplay == TEAM_CTF_ONEFLAG) {
            if(total1 > total2) {
                bprint(ts1);
                bprint(" team is leading by ");
                s = ftos(total1 - total2);
                bprint(s);
                bprint(" points!\n");
            } else if(total1 < total2) {
                bprint(ts2);
                bprint(" team is leading by ");
                s = ftos(total2 - total1);
                bprint(s);
                bprint(" points!\n");
            } else {
                bprint(ts1);
                bprint(" and ");
                bprint(ts2);
                bprint(" teams are tied with ");
                s = ftos(total1);
                bprint(s);
                bprint(" points!\n");
            }
        } else if(teamplay == TEAM_CTF_ALT) {
            if(total1 > total2 && total1 > total3) {
                bprint(ts1);
                bprint(" team is leading by ");
                if(total2 > total3)
                    s = ftos(total1 - total2);
                else
                    s = ftos(total1 - total3);
                bprint(s);
                bprint(" points!\n");
            } else if(total2 > total1 && total2 > total3) {
                bprint(ts2);
                bprint(" team is leading by ");
                if(total1 > total3)
                    s = ftos(total2 - total1);
                else
                    s = ftos(total2 - total3);
                bprint(s);
                bprint(" points!\n");
            } else if(total3 > total1 && total3 > total2) {
                bprint(ts3);
                bprint(" team is leading by ");
                if(total1 > total2)
                    s = ftos(total3 - total1);
                else
                    s = ftos(total3 - total2);
                bprint(s);
                bprint(" points!\n");
            } else if(total1 == total2) {
                bprint(ts1);
                bprint(" and ");
                bprint(ts2);
                bprint(" teams are tied with ");
                s = ftos(total1);
                bprint(s);
                bprint(" points!\n");
            } else if(total3 == total2) {
                bprint(ts2);
                bprint(" and ");
                bprint(ts3);
                bprint(" teams are tied with ");
                s = ftos(total3);
                bprint(s);
                bprint(" points!\n");
            } else if(total3 == total1) {
                bprint(ts1);
                bprint(" and ");
                bprint(ts3);
                bprint(" teams are tied with ");
                s = ftos(total1);
                bprint(s);
                bprint(" points!\n");
            } else {
                bprint(ts1);
                bprint(", ");
                bprint(ts2);
                bprint(" and ");
                bprint(ts3);
                bprint(" teams are tied with ");
                s = ftos(total1);
                bprint(s);
                bprint(" points!\n");
            }
        }
    }
};

/*QUAKED func_ctf_wall (0 .5 .8) ?
This is just a solid wall if not inhibitted
Only appears in CTF teamplay
*/
void() func_ctf_wall =
{
    teamplay = cvar("teamplay");
    if(teamplay == TEAM_CTF || teamplay == TEAM_CTF_ONEFLAG ||
        teamplay == TEAM_CTF_ALT) {
        self.angles = '0 0 0';
        self.movetype = MOVETYPE_PUSH;    // so it doesn't get pushed by anything
        self.solid = SOLID_BSP;
        setmodel (self, self.model);
    } else
        remove(self);
};


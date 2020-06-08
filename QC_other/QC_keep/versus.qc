/* VERSUS.QC
===================================================================================================================
by Qmaster

Multiplayer game mode where there are two teams

Survival team must battle monsters and collect coins to pay for unlocking the next area, 
weapons, powerups, etc. until they can pay for and access the exit.

Gamemaster team must spawn monsters of their choice and collect bones to pay for more monsters
until the survival team are out of lives.

FOR AD MOD OR AD DERIVATIVES ONLY
===================================================================================================================
*/
/*  ADD TO setttings.qc -> ImpulseCommands() FUNCTION
===================================================================================================================
void(entity targ) print_coins;
void(entity targ) print_bones;

    else if (self.impulse == 141) print_coins();        // Display versus currency for survival player(s)
    else if (self.impulse == 142) print_bones();        // Display versus currency for gamemaster player(s)

===================================================================================================================
*/



void () versus_setup = {
   coins = 0;
   bones = 0;
   numsurvivor = 0;
   numgamemaster = 0;
};

void (entity targ) set_teamsurvivor = {
   if (targ.versustype < 1) {
      targ.versustype = VT_SURVIVOR;
   } else {
      if (developer) {
         targ.versustype = VT_SURVIVOR;
      } else {
         sprint(targ,"Already on a team, can't change mid-game");
      }
   }   
};

void (entity targ) set_teamgamemaster = {
   if (targ.versustype < 1) {
      targ.versustype = VT_GAMEMASTER;
   } else {
      if (developer) {
         targ.versustype = VT_GAMEMASTER;
      } else {
         sprint(targ,"Already on a team, can't change mid-game");
      }
   }   
};

void (entity targ) print_coins = {
   local string retval;
   retval = ftos(coins);
   //retval = ("Coins:\n" + retval);
   centerprint (targ,retval);
};


void (entity targ) print_bones = {
   local string retval;
   retval = ftos(bones);
   centerprint (targ,retval);
};

void () item_coin_touch = {
   if (other.versustype != VT_SURVIVOR)  return;

   sprint (other, "You got ");
   sprint (other, ftos(self.count));
   sprint (other, "coins\n");
   stuffcmd (other, "bf\n");
   activator = other;
   coins = coins + self.count;
   SUB_UseTargets();
};

void () item_coin_setup = {
   if (self.noise == "")
      self.noise = "items/money.wav";

   precache_sound (self.noise);
   self.touch2 = item_coin_touch;
   self.bbmins = VEC_KEYS_MIN;
   self.bbmaxs = VEC_KEYS_MAX;
   item_start();
};

void () item_coin = {
   if (self.mdl == "")
      self.mdl = "progs/ne_ruins/coin.mdl";

   precache_model (self.mdl);
   if (self.count < 1) self.count = 1;
   item_coin_setup();
};

void () item_bone_touch = {
   if (other.versustype != VT_GAMEMASTER)  return;

   sprint (other, "You got ");
   sprint (other, ftos(self.count));
   sprint (other, "bones\n");
   stuffcmd (other, "bf\n");
   activator = other;
   bones = bones + self.count;
   SUB_UseTargets();
};

void () item_bone_setup = {
   if (self.noise == "")
      self.noise = "items/money.wav";

   precache_sound (self.noise);
   self.touch2 = item_bone_touch;
   self.bbmins = VEC_KEYS_MIN;
   self.bbmaxs = VEC_KEYS_MAX;
   item_start();
};

void () item_bone = {
   if (self.mdl == "")
      self.mdl = "progs/ad171/proj_wraithpoison.mdl";

   precache_model (self.mdl);
   if (self.count < 1) self.count = 1;
   item_bone_setup();
};
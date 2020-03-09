//=====================================================================
//
// Spawning Functions (Hipnotic)
//
//=====================================================================

//================
//
// spawn_think
//
//================

void() spawn_think =
   {
   self.think = spawn_think;
   self.nextthink = time + 1;
   };

//================
//
// spawn_use
//
//================

void() spawn_use =
   {
   local entity spawnentity;
   local entity tempself;

   if ((self.spawnmulti == 1) || (horn_active))
      {
      // spawn the new entity
      spawnentity = spawn();
      // copy the master mold
      SUB_CopyEntity(self.spawnmaster,spawnentity);
      }
   else
      {
      spawnentity = self.spawnmaster;
      }
   // restore the model
   spawnentity.model = spawnentity.spawnmodel;
   //restore solid flag
   spawnentity.solid = spawnentity.spawnsolidtype;
   //restore thinking function
   spawnentity.think = spawnentity.spawnthink;
   setmodel (spawnentity, spawnentity.model);
   setorigin (spawnentity, spawnentity.origin);
   spawnentity.mins = spawnentity.spawnmins;
   spawnentity.maxs = spawnentity.spawnmaxs;
   setsize (spawnentity, spawnentity.mins, spawnentity.maxs);
   // spawn the teleport effect
   if (self.spawnsilent == 0)
      spawn_tfog (spawnentity.origin);
//   horn_active = 0;
//   horn_charmer = find( world, classname, "player" );
   // call spawnentity think function
   if (horn_active)
      {
      spawnentity.charmer = horn_charmer;
      spawnentity.charmed = 1;
      }
//   if (spawnentity.think)
//      {
//      spawnentity.nextthink = time+0.1;
//      tempself = self;
//      self = spawnentity;
//      self.think();
//      self = tempself;
//      spawnentity.nextthink = time+0.1;
//      self.nextthink = 1;
//      if (spawnentity.nextthink < time)
//         spawnentity.nextthink = 1;
//      }
   // check to see if it is a monster
   if (spawnentity.flags & FL_MONSTER)
      {
      if ((self.spawnmulti != 0) && (horn_active == 0))
         {
         total_monsters = total_monsters + 1;
         WriteByte (MSG_BROADCAST, SVC_UPDATESTAT);
         WriteByte (MSG_BROADCAST, STAT_TOTALMONSTERS);
         WriteLong (MSG_BROADCAST, total_monsters);
         }
      // spawn the telefrag effect
//      if (self.spawnsilent == 0)
//         spawn_tdeath(spawnentity.origin, spawnentity);
      if (horn_active)
         {
         spawnentity.effects = spawnentity.effects | EF_DIMLIGHT;
//         spawnentity.effects = spawnentity.effects | EF_BRIGHTFIELD;
         }
      }
   if ((self.spawnmulti == 0) && (horn_active == 0))
      {
      remove(self);
      }
   };

//================
//
// func_spawn
//
//================
/*QUAKED func_spawn (0 .5 .8) (-32 -32 -24) (32 32 64) big/ambush megahealth
This will spawn a thing upon being used. The thing that
is spawned depends upon the value of "spawnfunction".
"spawnclassname" should contain the same value as "spawnfunction".
If "spawnfunction" is unspecified a random monster is chosen.
The angles, target and all flags are passed on
Think of it like setting up a normal entity.
"spawnsilent" set this to 1 if you want a silent spawn.
"spawnmulti" set this to 1 if you want this spawn to be reoccuring.
*/

void() func_spawn =
   {
   local entity tempself;
   local entity monster;
   local float tempdeathmatch;
   local float temptotal_monsters;
   local vector mn,mx;

//   if (deathmatch)
//   {
//      remove(self);
//      return;
//   }

   // save off deathmatch and zero it out
   tempself = self;
   tempdeathmatch = deathmatch;
   deathmatch = 0;
   if (!self.spawnfunction)
      {
      local float spawnchance;

      spawnchance = random();
      monster = tempself;

      // save off monster count so it doesn't get f'ed up
      temptotal_monsters = total_monsters;

      // spawn dog
      // spawn the new entity
      self = spawn();
      // copy over everything
      SUB_CopyEntity(tempself,self);
      self.spawnfunction = monster_dog;
      self.spawnclassname = "monster_dog";
      self.classname = self.spawnclassname;

      // call the named spawn function
      self.spawnfunction();
      self.spawnmodel = self.model;
      self.spawnmins = self.mins;
      self.spawnmaxs = self.maxs;
      setmodel (self, "");
      setsize (self, self.spawnmins, self.spawnmaxs);
      //save off solid flag
      self.spawnsolidtype = self.solid;
      self.solid = SOLID_NOT;
      //save off think func and
      //get rid of his thinking
      self.spawnthink = self.think;
      self.think = spawn_think;
      self.nextthink = time + 1;

      if (spawnchance<0.5 && monster==tempself)
         {
         monster = self;
         }

      // spawn ogre
      // spawn the new entity
      self = spawn();
      // copy over everything
      SUB_CopyEntity(tempself,self);
      self.spawnfunction = monster_ogre;
      self.spawnclassname = "monster_ogre";
      self.classname = self.spawnclassname;

      // call the named spawn function
      self.spawnfunction();
      self.spawnmodel = self.model;
      self.spawnmins = self.mins;
      self.spawnmaxs = self.maxs;
      setmodel (self, "");
      setsize (self, self.spawnmins, self.spawnmaxs);
      //save off solid flag
      self.spawnsolidtype = self.solid;
      self.solid = SOLID_NOT;
      //save off think func and
      //get rid of his thinking
      self.spawnthink = self.think;
      self.think = spawn_think;
      self.nextthink = time + 1;

      if (spawnchance<0.8 && monster==tempself)
         {
         monster = self;
         }

      // spawn fiend
      // spawn the new entity
      self = spawn();
      // copy over everything
      SUB_CopyEntity(tempself,self);
      self.spawnfunction = monster_demon1;
      self.spawnclassname = "monster_demon1";
      self.classname = self.spawnclassname;

      // call the named spawn function
      self.spawnfunction();
      self.spawnmodel = self.model;
      self.spawnmins = self.mins;
      self.spawnmaxs = self.maxs;
      setmodel (self, "");
      setsize (self, self.spawnmins, self.spawnmaxs);
      //save off solid flag
      self.spawnsolidtype = self.solid;
      self.solid = SOLID_NOT;
      //save off think func and
      //get rid of his thinking
      self.spawnthink = self.think;
      self.think = spawn_think;
      self.nextthink = time + 1;

      if (spawnchance<0.92 && monster==tempself)
         {
         monster = self;
         }

      // spawn zombie
      // spawn the new entity
      self = spawn();
      // copy over everything
      SUB_CopyEntity(tempself,self);
      self.spawnfunction = monster_zombie;
      self.spawnclassname = "monster_zombie";
      self.classname = self.spawnclassname;

      // call the named spawn function
      self.spawnfunction();
      self.spawnmodel = self.model;
      self.spawnmins = self.mins;
      self.spawnmaxs = self.maxs;
      setmodel (self, "");
      setsize (self, self.spawnmins, self.spawnmaxs);
      //save off solid flag
      self.spawnsolidtype = self.solid;
      self.solid = SOLID_NOT;
      //save off think func and
      //get rid of his thinking
      self.spawnthink = self.think;
      self.think = spawn_think;
      self.nextthink = time + 1;

      if (spawnchance<0.97 && monster==tempself)
         {
         monster = self;
         }

      // spawn shambler
      // spawn the new entity
      self = spawn();
      // copy over everything
      SUB_CopyEntity(tempself,self);
      self.spawnfunction = monster_shambler;
      self.spawnclassname = "monster_shambler";
      self.classname = self.spawnclassname;

      // call the named spawn function
      self.spawnfunction();
      self.spawnmodel = self.model;
      self.spawnmins = self.mins;
      self.spawnmaxs = self.maxs;
      setmodel (self, "");
      setsize (self, self.spawnmins, self.spawnmaxs);
      //save off solid flag
      self.spawnsolidtype = self.solid;
      self.solid = SOLID_NOT;
      //save off think func and
      //get rid of his thinking
      self.spawnthink = self.think;
      self.think = spawn_think;
      self.nextthink = time + 1;

      if (monster==tempself)
         {
         monster = self;
         }
      // make sure monster count is correct
      total_monsters = temptotal_monsters + 1;
      }
   else
      {
      // spawn the new entity
      self = spawn();
      // copy over everything
      SUB_CopyEntity(tempself,self);

      // save off monster count so it doesn't get f'ed up
      temptotal_monsters = total_monsters;
      if (self.spawnclassname == string_null)
         {
         objerror("No spawnclassname defined");
         }
      self.classname = self.spawnclassname;
      // call the named spawn function
      self.spawnfunction();
      if (self.spawnmulti != 0)
         {
         // make sure monster count is correct
         total_monsters = temptotal_monsters;
         }
      self.spawnmodel = self.model;
      self.spawnmins = self.mins;
      self.spawnmaxs = self.maxs;
      setmodel (self, "");
      self.model = "";
      setsize (self, self.spawnmins, self.spawnmaxs);
      //save off solid flag
      self.spawnsolidtype = self.solid;
      self.solid = SOLID_NOT;
      //save off think func and
      //get rid of his thinking
      self.spawnthink = self.think;
      self.think = spawn_think;
      self.nextthink = time + 1;
      monster = self;
      }
   self = tempself;
   deathmatch = tempdeathmatch;
   self.solid = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;
	self.modelindex = 0;
	self.model = "";
   setmodel (self, self.model);  // set size and link into world
   self.use = spawn_use;
   self.spawnmaster = monster;
   };

//================
//
// func_spawn_small
//
//================
/*QUAKED func_spawn_small (0 .5 .8) (-16 -16 -24) (16 16 40) big/ambush megahealth
This will spawn a thing upon being used. The thing that
is spawned depends upon the value of "spawnfunction".
"spawnclassname" should contain the same value as "spawnfunction".
If "spawnfunction" is unspecified a random monster is chosen.
The angles, target and all flags are passed on
Think of it like setting up a normal entity.
"spawnsilent" set this to 1 if you want a silent spawn.
"spawnmulti" set this to 1 if you want this spawn to be reoccuring.
*/

void() func_spawn_small =
   {
   func_spawn();
   };

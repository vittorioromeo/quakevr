//=========================
//
// Hipnotic Sub-routines
//
//=========================


//=========================
//
// SUB_CopyEntity
//
//=========================

void (entity srcent, entity destent) SUB_CopyEntity =
   {
   destent.modelindex=srcent.modelindex;
   destent.absmin=srcent.absmin;
   destent.absmax=srcent.absmax;
   destent.ltime=srcent.ltime;
   destent.movetype=srcent.movetype;
   destent.solid=srcent.solid;
   destent.origin=srcent.origin;
   destent.oldorigin=srcent.oldorigin;
   destent.velocity=srcent.velocity;
   destent.angles=srcent.angles;
   destent.avelocity=srcent.avelocity;
   destent.punchangle=srcent.punchangle;
   destent.classname=srcent.classname;
   destent.model=srcent.model;
   destent.frame=srcent.frame;
   destent.skin=srcent.skin;
   destent.effects=srcent.effects;
   destent.mins=srcent.mins;
   destent.maxs=srcent.maxs;
   destent.size=srcent.size;
   destent.touch=srcent.touch;
   destent.use=srcent.use;
   destent.think=srcent.think;
   destent.blocked=srcent.blocked;
   destent.nextthink=srcent.nextthink;
   destent.groundentity=srcent.groundentity;
   destent.health=srcent.health;
   destent.frags=srcent.frags;
   destent.weapon=srcent.weapon;
   destent.weaponmodel=srcent.weaponmodel;
   destent.weaponframe=srcent.weaponframe;
   destent.currentammo=srcent.currentammo;
   destent.ammo_shells=srcent.ammo_shells;
   destent.ammo_nails=srcent.ammo_nails;
   destent.ammo_rockets=srcent.ammo_rockets;
   destent.ammo_cells=srcent.ammo_cells;
   destent.items=srcent.items;
   destent.takedamage=srcent.takedamage;
   destent.chain=srcent.chain;
   destent.deadflag=srcent.deadflag;
   destent.view_ofs=srcent.view_ofs;
   destent.button0=srcent.button0;
   destent.button1=srcent.button1;
   destent.button2=srcent.button2;
   destent.impulse=srcent.impulse;
   destent.fixangle=srcent.fixangle;
   destent.v_angle=srcent.v_angle;
   destent.idealpitch=srcent.idealpitch;
   destent.netname=srcent.netname;
   destent.enemy=srcent.enemy;
   destent.flags=srcent.flags;
   destent.colormap=srcent.colormap;
   destent.team=srcent.team;
   destent.max_health=srcent.max_health;
   destent.teleport_time=srcent.teleport_time;
   destent.armortype=srcent.armortype;
   destent.armorvalue=srcent.armorvalue;
   destent.waterlevel=srcent.waterlevel;
   destent.watertype=srcent.watertype;
   destent.ideal_yaw=srcent.ideal_yaw;
   destent.yaw_speed=srcent.yaw_speed;
   destent.aiment=srcent.aiment;
   destent.goalentity=srcent.goalentity;
   destent.spawnflags=srcent.spawnflags;
   destent.target=srcent.target;
   destent.targetname=srcent.targetname;
   destent.dmg_take=srcent.dmg_take;
   destent.dmg_save=srcent.dmg_save;
   destent.dmg_inflictor=srcent.dmg_inflictor;
   destent.owner=srcent.owner;
   destent.movedir=srcent.movedir;
   destent.message=srcent.message;
   destent.sounds=srcent.sounds;
   destent.noise=srcent.noise;
   destent.noise1=srcent.noise1;
   destent.noise2=srcent.noise2;
   destent.noise3=srcent.noise3;
   destent.wad=srcent.wad;
   destent.map=srcent.map;
   destent.worldtype=srcent.worldtype;
   destent.killtarget=srcent.killtarget;
   destent.light_lev=srcent.light_lev;
   destent.style=srcent.style;
   destent.th_stand=srcent.th_stand;
   destent.th_walk=srcent.th_walk;
   destent.th_run=srcent.th_run;
   destent.th_missile=srcent.th_missile;
   destent.th_melee=srcent.th_melee;
   destent.th_pain=srcent.th_pain;
   destent.th_die=srcent.th_die;
   destent.th_turn=srcent.th_turn;
   destent.oldenemy=srcent.oldenemy;
   destent.speed=srcent.speed;
   destent.lefty=srcent.lefty;
   destent.search_time=srcent.search_time;
   destent.attack_state=srcent.attack_state;
   destent.walkframe=srcent.walkframe;
   destent.attack_finished=srcent.attack_finished;
   destent.pain_finished=srcent.pain_finished;
   destent.invincible_finished=srcent.invincible_finished;
   destent.invisible_finished=srcent.invisible_finished;
   destent.super_damage_finished=srcent.super_damage_finished;
   destent.radsuit_finished=srcent.radsuit_finished;
   destent.invincible_time=srcent.invincible_time;
   destent.invincible_sound=srcent.invincible_sound;
   destent.invisible_time=srcent.invisible_time;
   destent.invisible_sound=srcent.invisible_sound;
   destent.super_time=srcent.super_time;
   destent.super_sound=srcent.super_sound;
   destent.rad_time=srcent.rad_time;
   destent.fly_sound=srcent.fly_sound;
   destent.axhitme=srcent.axhitme;
   destent.show_hostile=srcent.show_hostile;
   destent.jump_flag=srcent.jump_flag;
   destent.swim_flag=srcent.swim_flag;
   destent.air_finished=srcent.air_finished;
   destent.bubble_count=srcent.bubble_count;
   destent.deathtype=srcent.deathtype;
   destent.mdl=srcent.mdl;
   destent.mangle=srcent.mangle;
   destent.oldorigin=srcent.oldorigin;
   destent.t_length=srcent.t_length;
   destent.t_width=srcent.t_width;
   destent.dest=srcent.dest;
   destent.dest1=srcent.dest1;
   destent.dest2=srcent.dest2;
   destent.wait=srcent.wait;
   destent.delay=srcent.delay;
   destent.trigger_field=srcent.trigger_field;
   destent.noise4=srcent.noise4;
   destent.pausetime=srcent.pausetime;
   destent.movetarget=srcent.movetarget;
   destent.aflag=srcent.aflag;
   destent.dmg=srcent.dmg;
   destent.cnt=srcent.cnt;
   destent.think1=srcent.think1;
   destent.finaldest=srcent.finaldest;
   destent.finalangle=srcent.finalangle;
   destent.count=srcent.count;
   destent.lip=srcent.lip;
   destent.state=srcent.state;
   destent.pos1=srcent.pos1;
   destent.pos2=srcent.pos2;
   destent.height=srcent.height;
   destent.waitmin=srcent.waitmin;
   destent.waitmax=srcent.waitmax;
   destent.distance=srcent.distance;
   destent.volume=srcent.volume;
   destent.gorging=srcent.gorging;
   destent.stoleweapon=srcent.stoleweapon;
   destent.spawnfunction=srcent.spawnfunction;
   destent.spawnthink=srcent.spawnthink;
   destent.spawnmodel=srcent.spawnmodel;
   destent.spawnsolidtype=srcent.spawnsolidtype;
   destent.spawnmins=srcent.spawnmins;
   destent.spawnmaxs=srcent.spawnmaxs;
   destent.spawnclassname=srcent.spawnclassname;
   };

//=========================
//
// SUB_NormalizeAngles
//
//=========================

vector ( vector ang ) SUB_NormalizeAngles =
   {
   while( ang_x > 360 )
      {
      ang_x = ang_x - 360;
      }
   while( ang_x < 0 )
      {
      ang_x = ang_x + 360;
      }

   while( ang_y > 360 )
      {
      ang_y = ang_y - 360;
      }
   while( ang_y < 0 )
      {
      ang_y = ang_y + 360;
      }

   while( ang_z > 360 )
      {
      ang_z = ang_z - 360;
      }
   while( ang_z < 0 )
      {
      ang_z = ang_z + 360;
      }

   return ang;
   };

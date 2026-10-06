// Native Horde acceptance uses the production spawns, pickup handlers, wave and revival paths.
float MGH_test_pass, MGH_test_fail;
void(float ok, string label) MGH_Check = {
 if(ok) MGH_test_pass++; else MGH_test_fail++;
 dprint(sprintf("mghordetest: %s %s\n",ok ? "PASS" : "FAIL",label));
};
void() MGH_Report = {
 entity p=find(world,classname,"player");
 dprint(sprintf("mghordetest: map %s active %g wave %g waiting %g live %g\n",mapname,MGH_Active(),MGH_horde_ent.wave,MGH_horde_ent.wait,MGH_HordeGetMonstersAlive()));
 dprint(sprintf("mghordetest: squads %g/%g/%g keys %g/%g spawned %g\n",MGH_horde_ent.fodder,MGH_horde_ent.elites,MGH_horde_ent.bosses,MGH_keys_silver,MGH_keys_gold,MGH_key_spawned));
 dprint(sprintf("mghordetest: player health %g max %g hunger %g time %g shells %g armor %g\n",p.health,p.max_health,p.MGH_hunger_time,time,p.ammo_shells,p.armorvalue));
 dprint(sprintf("mghordetest: hands weapons %g/%g clips %g/%g ids %g/%g\n",p.weapon,p.weapon2,p.weaponinst.wi_clip,p.weaponinst2.wi_clip,p.weaponinst.wi_uid,p.weaponinst2.wi_uid));
 dprint(sprintf("mghordetest: flashlight %g holster clips %g/%g/%g/%g/%g/%g\n",p.vr_flashlight_state,p.holsterweaponinst0.wi_clip,p.holsterweaponinst1.wi_clip,p.holsterweaponinst2.wi_clip,p.holsterweaponinst3.wi_clip,p.holsterweaponinst4.wi_clip,p.holsterweaponinst5.wi_clip));
 dprint(sprintf("mghordetest: holster ids %g/%g/%g/%g/%g/%g\n",p.holsterweaponinst0.wi_uid,p.holsterweaponinst1.wi_uid,p.holsterweaponinst2.wi_uid,p.holsterweaponinst3.wi_uid,p.holsterweaponinst4.wi_uid,p.holsterweaponinst5.wi_uid));

};
void() MGH_Unit = {
 entity caller=self, p=find(world,classname,"player"), manager=MGH_horde_ent;
 MGH_test_pass=MGH_test_fail=0;
 manager.nextthink=0; manager.think=SUB_Null;
 vector pos=p.origin; float savedflags=serverflags;
 entity point=spawn(); self=point; point.classname="info_monster_start"; setorigin(point,p.origin); info_monster_start();
 MGH_Check(MGH_CheckBlockedSpawn(point),"living player blocks overlapping normal spawn");
 setorigin(point,p.origin+'0 0 1024'); MGH_Check(!MGH_CheckBlockedSpawn(point),"vertical separation permits spawn");
 point.spawnflags=2; setorigin(point,p.origin); MGH_Check(!MGH_CheckBlockedSpawn(point),"authored bypass permits occupied spawn");
 point.spawnflags=0; MGH_monster_start_use(); MGH_Check(point.spawnflags & 1,"target toggles spawn disabled"); MGH_monster_start_use(); MGH_Check(!(point.spawnflags & 1),"target toggles spawn enabled"); remove(point);
 float before=total_monsters;
 self=manager; entity m=MGH_SpawnMonster("wizard",p.origin+'256 0 160','0 0 0'); self=m; MGH_MonsterStart();
 MGH_Check(m.classname=="monster_wizard" && (m.flags & FL_FLY) && m.max_health==80,"flying squad uses native VR scrag constructor");
 MGH_Check(m.enemy==p && m.think==FoundTarget,"spawned scrag targets living player"); remove(m);
 self=manager; m=MGH_SpawnMonster("shambler",p.origin+'256 0 0','0 0 0'); self=m; MGH_MonsterStart();
 MGH_Check(m.classname=="monster_shambler" && m.max_health==600 && m.th_die==sham_die,"boss uses native VR shambler constructor"); remove(m);
 self=manager; m=MGH_SpawnMonster("enforcer",p.origin+'256 0 0','0 0 0'); self=m; MGH_MonsterStart();
 MGH_Check(m.classname=="monster_enforcer" && m.max_health==80 && m.th_missile==enf_atk1,"ranged squad uses native VR enforcer constructor"); remove(m);
 MGH_Check(total_monsters==before+3,"native spawns count once each");
 // Shared currency preserves item bit representation and cannot underflow.
 MGH_keys_silver=0; MGH_keys_gold=0;
 MGH_horde_key_give(IT_KEY1); MGH_horde_key_give(IT_KEY1);
 MGH_Check(MGH_keys_silver==2 && VR_ItemUtil_EntHasItem(p,IID_KEY1),"two shared silver currency pickups");
 MGH_horde_key_spend(IT_KEY1); MGH_Check(MGH_keys_silver==1 && VR_ItemUtil_EntHasItem(p,IID_KEY1),"spending retains last available key bit");
 MGH_horde_key_spend(IT_KEY1); MGH_horde_key_spend(IT_KEY1);
 MGH_Check(MGH_keys_silver==0 && !VR_ItemUtil_EntHasItem(p,IID_KEY1),"last key clears shared bit without underflow");
 // Physical ammo object calls source shared-ammo pickup, does not replace either weapon.
 entity sp=spawn(); self=sp; setorigin(sp,p.origin+'64 0 0'); MGH_SpawnAmmo();
 entity item=nextent(world); while(item && item.MGH_pickup_spawn!=sp) item=nextent(item);
 float w=p.weapon,w2=p.weapon2,id=p.weaponinst.wi_uid,id2=p.weaponinst2.wi_uid;
 p.ammo_shells=p.ammo_nails=p.ammo_rockets=p.ammo_cells=0;
 MGH_Check(item && item.carry_use==MGH_horde_ammo_touch && (item.flags & FL_FORCEGRABBABLE),"spawned ammunition has physical VR box interface");
 self=item; other=p; item.carry_use();
 MGH_Check(p.ammo_shells+p.ammo_nails+p.ammo_rockets+p.ammo_cells>0 && sp.think==MGH_SpawnAmmo && sp.nextthink==time+20,"shared ammo awards reserve and schedules source 20-second respawn");
 MGH_Check(p.weapon==w && p.weapon2==w2 && p.weaponinst.wi_uid==id && p.weaponinst2.wi_uid==id2,"shared ammo preserves both hand weapons and IDs"); remove(sp);

 // Authored health/armor rewards use their native physical interfaces.
 entity rewardspawn=spawn(); setorigin(rewardspawn,p.origin+'64 64 0');
 self=rewardspawn; MGH_SpawnItem(); entity reward=nextent(world);
 while(reward && reward.MGH_pickup_spawn!=rewardspawn) reward=nextent(reward);
 MGH_Check(reward && (reward.carry_use==MGH_horde_health_touch || reward.carry_use==MGH_horde_armor_touch || reward.handtouch==MGH_horde_armor_touch),"wave reward exposes physical health or wearable armor pickup");
 p.health=10; p.armorvalue=0; self=reward; other=p;
 if(reward.carry_use) reward.carry_use(); else reward.handtouch();
 MGH_Check(!rewardspawn.wait && (p.health>10 || p.armorvalue==100),"wave reward heals or grants source armor and frees spawn"); remove(rewardspawn);
 self=manager; MGH_powerup_chance=1; MGH_horde_spawn_powerup();
 entity power=nextent(world); while(power && !power.MGH_powerup_expire) power=nextent(power);
 MGH_Check(power && power.vr_pickup_obj==QVR_PICKUPOBJ_HOLSTER && power.carry_use==MGH_powerup_touch,"chance drop exposes native physical powerup and authored lifetime");
 self=power; other=p; MGH_powerup_touch();
 MGH_Check(p.super_damage_finished==time+30 || p.invincible_finished==time+30,"actual powerup grants source thirty-second effect");

 entity runetest=spawn(); self=runetest; runetest.spawnflags=2;
 serverflags=savedflags-(savedflags & 2); p.MGH_hunger_time=0;
 MGH_RuneHunger(); MGH_Check(p.MGH_hunger_time==time+10,"first Hunger rune sets timer before server rune bit is recorded"); remove(runetest);
 // Source Hunger initialization on rune collection and reset only on successful healing.
 serverflags=serverflags|2; p.health=40; other=p;
 MGH_ResetHunger(p); MGH_Check(p.MGH_hunger_time==time+10,"Hunger rune schedules source ten-second timer");
 p.MGH_hunger_time=0; T_Heal(p,5,0);
 MGH_Check(p.MGH_hunger_time==time+10 && p.health==45,"successful healing resets Hunger timer");
 p.health=p.max_health; p.MGH_hunger_time=123; T_Heal(p,5,0);
 MGH_Check(p.MGH_hunger_time==123,"rejected healing does not reset Hunger timer");
 serverflags=savedflags; setorigin(p,pos); self=caller;
 dprint(sprintf("mghordetest: %g passed, %g failed\n",MGH_test_pass,MGH_test_fail));
};
void() MGH_KillWave = {
 entity p=find(world,classname,"player"),e=find(world,MGH_category,MGH_CATEGORY_MONSTER);
 float count=0; while(e) { entity next=find(e,MGH_category,MGH_CATEGORY_MONSTER); if(e.health>0 && e.classname!="monster_zombie") { e.health=0; Killed(e,p); count++; } e=next; }
 dprint(sprintf("mghordetest: actual killed %g wave %g live %g\n",count,MGH_horde_ent.wave,MGH_HordeGetMonstersAlive()));
};
// Wave monitor (request 13 on, 14 off): logs each wave's source squad budget as SpawnWavePrep set it, then every
// squad SpawnWave2 spends from it with the monster classes it spawned (Misc/quakevr/check_horde_waves.py checks the log).
.float MGH_test_seen;
float MGH_test_monitor, MGH_test_wave, MGH_test_fodder, MGH_test_elites, MGH_test_bosses, MGH_test_squad;
void() MGH_TestMonitor = {
 entity manager=MGH_horde_ent; if(!manager) return;
 if(manager.wave!=MGH_test_wave) {
  MGH_test_wave=manager.wave;
  dprint(sprintf("mghordewave: wave %g skill %g players %g army %g squads %g/%g/%g\n",manager.wave,skill,MGH_HordeGetPlayersAlive(),manager.army,manager.fodder,manager.elites,manager.bosses));
 }
 string cat="other";
 if(manager.fodder<MGH_test_fodder) cat="fodder"; else if(manager.elites<MGH_test_elites) cat="elite"; else if(manager.bosses<MGH_test_bosses) cat="boss";
 float fresh=FALSE; entity e=find(world,MGH_category,MGH_CATEGORY_MONSTER);
 while(e) {
  if(!e.MGH_test_seen) {
   if(!fresh) { fresh=TRUE; MGH_test_squad++; }
   e.MGH_test_seen=TRUE;
   dprint(sprintf("mghordesquad: wave %g squad %g cat %s army %g class %s\n",manager.wave,MGH_test_squad,cat,manager.army,e.classname));
  }
  e=find(e,MGH_category,MGH_CATEGORY_MONSTER);
 }
 MGH_test_fodder=manager.fodder; MGH_test_elites=manager.elites; MGH_test_bosses=manager.bosses;
};
void() MGH_TestFrame = {
 if(MGH_test_monitor) MGH_TestMonitor();
 float request=cvar("vr_mg_horde_test"); if(!request) return; cvar_set("vr_mg_horde_test","0");
 entity caller=self,prevother=other,prevact=activator;
 if(request==2) MGH_Report();
 else if(!MGH_Active()) dprint("mghordetest: owned Machine Horde arena required\n");
 else if(request==1) MGH_Unit();
 else if(request==3) MGH_KillWave();
 else if(request==4) { self=MGH_horde_ent; self.wave=2; self.wait=1; self.fodder=self.elites=self.bosses=0; MGH_Countdown(); }
 else if(request==5) {
  entity key=nextent(world),p=find(world,classname,"player"); while(key) { if(key.touch==MGH_horde_key_touch || key.carry_use==MGH_horde_key_touch) { self=key; other=p; MGH_horde_key_touch(); break; } key=nextent(key); }
  dprint(sprintf("mghordetest: key collected %g silver %g gold %g\n",!!key,MGH_keys_silver,MGH_keys_gold));
 } else if(request==6) {
  entity p=find(world,classname,"player"); self=p; float mainid=p.weaponinst.wi_uid,offid=p.weaponinst2.wi_uid,clip=p.weaponinst.wi_clip,offclip=p.weaponinst2.wi_clip;
  float h0=p.holsterweaponinst0.wi_uid,h5=p.holsterweaponinst5.wi_uid;
  p.health=0; PlayerDie();
  self=MGH_horde_ent;
  if(MGH_HordeGetMonstersAlive()==0) { self.wave=1; self.wait=0; MGH_Wavecheck(); }
  else MGH_RespawnAllPlayers();
  self=p;
  MGH_Check(p.health==p.max_health && p.deadflag==DEAD_NO,"native Horde revival restores live player");
  MGH_Check(p.weaponinst.wi_uid==mainid && p.weaponinst2.wi_uid==offid && p.weaponinst.wi_clip==clip && p.weaponinst2.wi_clip==offclip,"revival preserves hand instance IDs and magazines");
  MGH_Check(p.holsterweaponinst0.wi_uid==h0 && p.holsterweaponinst5.wi_uid==h5,"revival preserves independent holster instance IDs"); MGH_Report();
 } else if(request==8) {
  entity p=find(world,classname,"player"), button=find(world,classname,"func_button");
  while(button && !button.items) button=find(button,classname,"func_button");
  if(button) {
   float keybit=button.items, expected=keybit==IT_KEY1 ? MGH_keys_silver : MGH_keys_gold;
   MGH_horde_key_give(keybit); self=button; activator=other=p;
   button_touch();
   MGH_Check((keybit==IT_KEY1 ? MGH_keys_silver : MGH_keys_gold)==expected,"actual keyed physical button spends shared currency once");
   button_touch();
   MGH_Check((keybit==IT_KEY1 ? MGH_keys_silver : MGH_keys_gold)==expected,"held button contact does not spend again");
  } else dprint("mghordetest: no authored keyed button in this arena\n");
 } else if(request==11) {
  entity p=find(world,classname,"player"),exit=find(world,classname,"trigger_changelevel");
  if(exit) { self=exit; other=activator=p; changelevel_touch(); dprint(sprintf("mghordetest: actual authored arena exit next %s\n",nextmap)); }
 } else if(request==12) {
  if(intermission_running) { ExitIntermission(); localcmd("exec mghorde_after.cfg\n"); }
 } else if(request==10) {
  entity p=find(world,classname,"player"),m=nextent(world); while(m && !m.MG_deferred) m=nextent(m);
  if(m) {
   self=m; activator=p; MG_MonsterActivate(); MG_MonsterRunStart();
   MGH_Check(m.MGH_category==MGH_CATEGORY_MONSTER && m.health>0,"authored deferred arena monster joins live wave accounting");
  } else dprint("mghordetest: no authored deferred monster in this arena\n");
 } else if(request==9) {
  entity p=find(world,classname,"player"); p.MGH_hunger_time=time+10; serverflags=serverflags|2; dprint("mghordetest: save Hunger timer set\n");
 } else if(request==13) {
  MGH_test_monitor=TRUE; MGH_test_wave=-1; MGH_test_fodder=MGH_test_elites=MGH_test_bosses=0;
  entity seen=find(world,MGH_category,MGH_CATEGORY_MONSTER); while(seen) { seen.MGH_test_seen=TRUE; seen=find(seen,MGH_category,MGH_CATEGORY_MONSTER); }
  dprint("mghordetest: wave monitor on\n");
 } else if(request==15) {
  dprint(sprintf("mghordeplayers: wave %g waiting %g live %g alive %g coop %g keys %g/%g\n",MGH_horde_ent.wave,MGH_horde_ent.wait,MGH_HordeGetMonstersAlive(),MGH_HordeGetPlayersAlive(),coop,MGH_keys_silver,MGH_keys_gold));
  entity pl=find(world,classname,"player");
  while(pl) {
   dprint(sprintf("mghordeplayer: %s client %g health %g dead %g ",pl.netname,!!(pl.flags & FL_CLIENT),pl.health,pl.deadflag));
   dprint(sprintf("silver %g gold %g frags %g ids %g/%g\n",VR_ItemUtil_EntHasItem(pl,IID_KEY1),VR_ItemUtil_EntHasItem(pl,IID_KEY2),pl.frags,pl.weaponinst.wi_uid,pl.weaponinst2.wi_uid));
   pl=find(pl,classname,"player");
  }
 } else if(request==17) {
  // Coop team wipe: every living player dies as from the kill command (official horde ClientKill).
  entity victim=find(world,classname,"player");
  while(victim) { if(victim.health>0 && (victim.flags & FL_CLIENT)) { self=victim; self.health=0; PlayerDie(); } victim=find(victim,classname,"player"); }
  dprint(sprintf("mghordetest: team wiped, alive %g\n",MGH_HordeGetPlayersAlive()));
 } else if(request==18) {
  // Keyed door: shared currency is spent once (official doors.qc horde_key_spend), the door opens, no key remains.
  entity p=find(world,classname,"player"), door=find(world,classname,"door");
  while(door && !(door.touch==door_touch && (door.vr_itemId==IID_KEY1 || door.vr_itemId==IID_KEY2))) door=find(door,classname,"door");
  if(door) {
   MGH_test_pass=MGH_test_fail=0; float silver=door.vr_itemId==IID_KEY1, keybit=silver ? IT_KEY1 : IT_KEY2;
   MGH_keys_silver=MGH_keys_gold=0; MGH_horde_key_remove_all(IT_KEY1); MGH_horde_key_remove_all(IT_KEY2);
   self=door; other=activator=p; door.owner.attack_finished=0; door_touch();
   MGH_Check(door.touch==door_touch && door.owner.state==STATE_BOTTOM,"keyed door stays shut without shared currency");
   MGH_horde_key_give(keybit); MGH_horde_key_give(keybit); door.owner.attack_finished=0; door_touch();
   MGH_Check((silver ? MGH_keys_silver : MGH_keys_gold)==1 && door.touch==SUB_Null && door.owner.state==STATE_UP,"keyed door spends one shared key and opens");
   MGH_Check(VR_ItemUtil_EntHasItem(p,door.vr_itemId),"a second shared key stays available after the spend");
   dprint(sprintf("mghordetest: %g passed, %g failed\n",MGH_test_pass,MGH_test_fail));
  } else dprint("mghordetest: no authored keyed door in this arena\n");
 } else if(request==16) {
  // Scripts: a team wipe's "restart" is appended after the rest of the test script, so the script sets this and
  // continues from mghorde_after.cfg once the restarted arena runs.
  localcmd("exec mghorde_after.cfg\n");
 } else if(request==14) {
  MGH_test_monitor=FALSE; dprint("mghordetest: wave monitor off\n");
 } else if(request==7) {
  entity p=find(world,classname,"player"); self=p; MGH_powerup_chance=1; MGH_horde_spawn_powerup(); dprint("mghordetest: forced production powerup drop\n");
 }
 self=caller; other=prevother; activator=prevact;
};

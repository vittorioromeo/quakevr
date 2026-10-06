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
void() MGH_TestFrame = {
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
 } else if(request==7) {
  entity p=find(world,classname,"player"); self=p; MGH_powerup_chance=1; MGH_horde_spawn_powerup(); dprint("mghordetest: forced production powerup drop\n");
 }
 self=caller; other=prevother; activator=prevact;
};

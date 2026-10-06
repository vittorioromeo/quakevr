// Campaign-scoped behavioral acceptance; requests use the existing unarchived world test cvar.
float MG1_G_pass, MG1_G_fail, MG1_G_baseline;
void(float ok, string text) MG1_G_Check = {
    if(ok) MG1_G_pass++; else MG1_G_fail++;
    dprint(sprintf("mggameplaytest: %s %s\n", ok ? "PASS" : "FAIL", text));
};
void(float request) MG1_GameplayTest = {
    entity caller=self, oldother=other, oldact=activator, p=find(world,classname,"player");
    if(request == 1008 && p) {
        entity e=spawn(), victim=spawn(); self=e; e.spawnflags=8192;
        setorigin(e,p.origin); setorigin(victim,e.origin+'0 350 0');
        MG1_G_Check(range(victim)==RANGE_NEAR,"stock/Dopa/Honey nearsighted flag does not change vision");
        e.spawnflags=16384; MG1_G_Check(!VR_Liquid_Immune(e),"stock/Dopa/Honey content immunity flag stays isolated");
        entity b=find(world,classname,"monster_boss");
        if(b) { self=b; activator=p; b.spawnflags=2; boss_awake(); MG1_G_Check(b.health==3 && !b.takedamage,"stock killable flag does not change Chthon"); }
        remove(e); remove(victim); self=caller; other=oldother; activator=oldact; return;
    }
    if(!MG1_Campaign() || !p) { dprint("mggameplaytest: Machine campaign required\n"); return; }
    if(request == 1001) {
        MG1_G_pass=MG1_G_fail=0;
        entity e=spawn(), victim=spawn(); self=e;
        setorigin(e,p.origin+'0 0 128'); e.angles='0 0 0'; e.spawnflags=8192;
        setorigin(victim,e.origin+'0 350 0');
        MG1_G_Check(range(victim)==RANGE_FAR,"nearsighted idle max range340");
        e.enemy=victim; MG1_G_Check(range(victim)==RANGE_NEAR,"nearsighted engaged near range400");
        setorigin(victim,e.origin+'200 150 0'); MG1_G_Check(!infront(victim),"nearsighted thirty-degree field of view");
        e.spawnflags=0; MG1_G_Check(infront(victim),"ordinary view angle preserved");
        e.target="mg1_test_friend"; victim.targetname=e.target; victim.takedamage=DAMAGE_YES;
        MG1_G_Check(MG1_FriendTarget(e)==victim,"authored friendly victim selected");
        victim.takedamage=DAMAGE_NO; MG1_G_Check(!MG1_FriendTarget(e),"invulnerable friendly victim excluded");
        e.spawnflags=16384; MG1_G_Check(VR_Liquid_Immune(e),"authored no-contents-damage flag immune");
        e.spawnflags=0; MG1_G_Check(!VR_Liquid_Immune(e),"ordinary contents damage remains enabled");
        // Find real lava along the finale's central pool and test the production hazard path.
        entity boss=find(world,classname,"monster_boss");
        vector pos=boss.origin; float z=-512, found=0;
        while(z < 512) { pos_z=z; if(pointcontents(pos)==CONTENT_LAVA) { found=1; break; } z+=16; }
        if(found) {
            setorigin(e,pos); setsize(e,'-8 -8 0','8 8 16'); e.view_ofs='0 0 16';
            e.classname="mg1_hazard_test"; e.health=1000; e.takedamage=DAMAGE_YES;
            float level=VR_Liquid_Level(e); VR_Liquid_Monster(e,TRUE);
            MG1_G_Check(e.health == 1000-30*level,"actual lava thirty damage per submerged level");
            float savedHP=e.health; e.spawnflags=16384; VR_Liquid_Monster(e,TRUE);
            MG1_G_Check(e.health==savedHP,"actual lava honors authored immunity");
        } else MG1_G_Check(FALSE,"real finale lava fixture found");
        e.spawnflags=0; e.movetype=MOVETYPE_BOUNCE; e.velocity='0 0 0'; e.vr_gren_lastvel='0 0 -200';
        // Real map floor trace; this does not replace the VR fuse.
        setorigin(e,p.origin+'0 0 -24'); e.vr_gren_fuse=time+2.5; float fuse=e.vr_gren_fuse;
        MG1_GrenadeSlopeBounce(); MG1_G_Check(e.velocity_z > 0 && !(e.flags & FL_ONGROUND),"actual floor trace reflects stalled descending grenade"); MG1_G_Check(e.vr_gren_fuse==fuse,"slope handling preserves grenade fuse");
        // Exercise the real launcher constructor with catches disabled and enabled.
        float catchMode=cvar("vr_grenade_catch"), oldWeapon=p.weapon, oldFlags=p.weaponflags, oldRockets=p.ammo_rockets;
        entity oldRecord=p.weaponinst, temporaryRecord=WeaponInst_Carried(WID_GRENADE_LAUNCHER,2,0);
        p.weapon=WID_GRENADE_LAUNCHER; p.weaponflags=0; p.weaponinst=temporaryRecord; p.ammo_rockets=20; self=p;
        cvar_set("vr_grenade_catch","0"); W_FireGrenade(cVR_MainHand);
        entity g=find(world,classname,"grenade");
        while(g && g.owner != p) g=find(g,classname,"grenade");
        MG1_G_Check(g && !g.vr_grenade && g.think==MG1_GrenadeThink && fabs(g.MG1_grenade_fuse-time-2.5)<0.001,"actual noncatchable launcher uses source bounce tick and original fuse");
        if(g) remove(g);
        cvar_set("vr_grenade_catch","2"); W_FireGrenade(cVR_MainHand);
        g=find(world,classname,"grenade"); while(g && g.owner != p) g=find(g,classname,"grenade");
        MG1_G_Check(g && g.vr_grenade && g.think==VR_Grenade_Think && fabs(g.vr_gren_fuse-time-2.5)<0.001,"actual catchable launcher retains native VR tick and original fuse");
        if(g) remove(g);
        cvar_set("vr_grenade_catch",ftos(catchMode)); p.weapon=oldWeapon; p.weaponflags=oldFlags; p.weaponinst=oldRecord; p.ammo_rockets=oldRockets;
        remove(temporaryRecord); self=e;
        float wanted = qvr_weaponinst_next+8;
        entity maprecord = WeaponInst_Make(5,wanted);
        MG1_ReserveCarryUid(wanted);
        entity carried = WeaponInst_Carried(WID_SHOTGUN,3,wanted);
        MG1_G_Check(maprecord.wi_uid != wanted && maprecord.wi_clip == 5 && carried.wi_uid == wanted && carried.wi_clip == 3,"carried UID wins fresh-map collision without aliasing magazines");
        remove(maprecord); remove(carried);
        remove(e); remove(victim);
        dprint(sprintf("mggameplaytest: %g passed %g failed\n",MG1_G_pass,MG1_G_fail));
    } else if(request == 1007) {
        MG1_G_Check(!find(world,classname,"monster_boss"),"finale boss removed after death animation");
        MG1_G_Check(killed_monsters==MG1_G_baseline+1,"finale death increments kill count once");
        entity door=find(world,targetname,"end_door");
        MG1_G_Check(door.state==STATE_TOP || door.state==STATE_UP,"actual authored end door opens after boss death");
        dprint(sprintf("mggameplaytest: %g passed %g failed\n",MG1_G_pass,MG1_G_fail));
    } else {
        entity b=find(world,classname,"monster_boss");
        if(!b) { dprint("mggameplaytest: finale boss unavailable\n"); return; }
        self=b; activator=p;
        if(request==1002) {
            MG1_G_baseline=killed_monsters; b.use();
            MG1_G_Check(b.health==4000 && b.max_health==4000 && b.takedamage==DAMAGE_AIM && b.th_die==boss_death1,"actual killable Chthon activates with4000 health");
            T_Damage(b,world,world,1); MG1_G_Check(b.health==3999 && b.think==boss_shockz2,"first actual hit starts nonfreezing source shock");
        } else if(request==1003) {
            T_Damage(b,world,world,b.health-2500);
            MG1_G_Check(b.think==boss_shocka2,"actual two-thirds crossing starts authored shock");
        } else if(request==1004) {
            MG1_G_Check(b.MG_is_frozen && b.use==MG_SUB_Unfreeze,"two-thirds phase freezes until authored relay");
            b.use(); MG1_G_Check(!b.MG_is_frozen,"authored relay resumes frozen boss");
            T_Damage(b,world,world,b.health-1000);
            MG1_G_Check(b.think==boss_shockb2,"actual one-third crossing starts authored shock");
        } else if(request==1005) {
            MG1_G_Check(b.MG_is_frozen,"one-third phase freezes until authored relay"); b.use();
            T_Damage(b,world,world,b.health+1);
            MG1_G_Check(!b.takedamage && b.think==boss_death2,"actual fatal hit starts native death animation");
        } else if(request==1006) {
            dprint(sprintf("mggameplaytest: boss health %g frozen %g total %g kills %g targets %s\n",b.health,b.MG_is_frozen,total_monsters,killed_monsters,b.target));
            dprint(sprintf("mggameplaytest: %g passed %g failed\n",MG1_G_pass,MG1_G_fail));
        }
    }
    self=caller; other=oldother; activator=oldact;
};

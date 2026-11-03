// vr_ogre_friends.qc -- ogres mind their friends (his note 2026-11-02: "avoid shooting grenades/rockets directly into
// friendlies if they are in their line of sight. Basic sidestepping like for the armed humanoids"): before each shot
// (id's ogre, Honey's, Dawn of the Machine's rocket ogre: their ogre_nail4), the shot's way is checked: a grenade's lob as
// it will fly (VR_AG_ArcBurst: what it would burst on first), a rocket's line (VR_AG_FriendInLine). (Its attack only begins
// with a clear sight line, CheckAttack, as ever: this is for the friend who walked in during the wind-up, or off that one line
// but on the shot's way.) A friend first on it:
// the armed humanoids' rule (VR_AI_FriendCare: vr_ogre_friendly_care's chance, away from the friend's side, firing anyway
// after vr_ogre_friendly_giveup): it gives the attack up and strafes (ai_run_slide, AS_SLIDING) for vr_ogre_friendly_step
// to twice that, facing you, then attacks again. Arcane Dimensions' ogres have code of their own (ad_ogre_shoot*): not these.

float vr_ogre_ff_held;   // shots held for friends (the tests)
float vr_ogre_ff_shots;  // ... shots that went

// The friend a grenade thrown now would burst on first (its lob, and one `deg` degrees either side): world, none. Its
// velocity OgreFireGrenade's own (OgreGrenadeAim).
entity(float deg) VR_Ogre_FriendOnLob =
{
    entity probe = spawn();
    OgreGrenadeAim(probe);
    vector vel = probe.velocity;
    remove(probe);
    float i;
    for(i = 0; i < (deg > 0 ? 3 : 1); i++)
    {
        VR_AG_ArcBurst(self.origin, i == 0 ? vel : VR_AG_Rotate(vel, '0 0 1', i == 1 ? deg : -deg), FALSE);
        if(VR_AI_IsFriend(vr_ag_arc_hit))
        {
            return vr_ag_arc_hit;
        }
    }
    return world;
};

// The friend first on the way of the shot it is about to fire; world: none (or it shoots through a portal: that way's its own).
entity() VR_Ogre_FriendInWay =
{
    if(!self.enemy || self.vr_ai_gate > 0)
    {
        return world;
    }
    float deg = cvar_hget(cvarh_vr_ogre_friendly_spread);
    if(!self.vr_mg3_mon)
    {
        return VR_Ogre_FriendOnLob(deg);
    }
    // (a volley's second rocket goes 64 units either side of you: MG3_OgreFireRocket)
    float d = vlen(self.enemy.origin - self.origin);
    return VR_AG_FriendInLine(self.origin + '0 0 16', max(deg, d > 1 ? (64 * self.vr_mg3_volley / d) * (180 / 3.14159) : 0));
};

// ogre_nail4, about to fire (its frame's think set): TRUE when it holds for a friend, its attack given up, stepping aside.
float() VR_Ogre_FriendHold =
{
    float care = cvar_hget(cvarh_vr_ogre_friendly_care);
    if(care <= 0)
    {
        vr_ogre_ff_shots = vr_ogre_ff_shots + 1;
        return FALSE;
    }
    float giveup = cvar_hget(cvarh_vr_ogre_friendly_giveup);
    if(!VR_AI_FriendCare(VR_Ogre_FriendInWay(), care, giveup, cvar_hget(cvarh_vr_ogre_friendly_step), max(2, giveup)))
    {
        vr_ogre_ff_shots = vr_ogre_ff_shots + 1;
        return FALSE;
    }
    vr_ogre_ff_held = vr_ogre_ff_held + 1;
    self.vr_mg3_volley = 0; // (a rocket ogre's: a new one next time)
    self.attack_finished = self.vr_ag_ff_until; // (no shot went: its next try when the step is done, not its next attack's time)
    self.attack_state = AS_SLIDING;
    self.think = self.th_run;
    return TRUE;
};

// ai_run, no attack begun: its sidestep over, straight at you again (AS_SLIDING stays till an attack on those that always strafe).
void() VR_Ogre_FriendStepOver =
{
    if(self.vr_ag_ff_until > 0 && time >= self.vr_ag_ff_until && self.attack_state == AS_SLIDING)
    {
        self.attack_state = AS_STRAIGHT;
        self.vr_ag_ff_until = 0;
    }
};

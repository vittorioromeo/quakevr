// One-hop ranged perception. The real enemy entity never moves or becomes a pathfinding image.
.entity vr_ai_target;
.float vr_ai_gate;

float() VR_AI_PortalEligible =
{
    if((!self.th_missile && self.classname != "monster_boss") ||
       (self.classname == "monster_gremlin" && (!self.stoleweapon || self.weapon == WID_AXE))) { return FALSE; }
    // Jump/charge attacks are physical melee, not ranged weapons.
    return self.classname != "monster_dog" && self.classname != "monster_demon1" &&
           self.classname != "monster_tarbaby" && self.classname != "monster_eel" &&
           self.classname != "monster_fish" && self.classname != "monster_knight";
};

vector(entity targ) VR_AI_Origin =
{
    if(targ == self.vr_ai_target && self.vr_ai_gate > 0)
        return portal_ai_map(self.vr_ai_gate, targ.origin, FALSE);
    return targ.origin;
};
vector(vector point) VR_AI_Point = { return portal_ai_map(self.vr_ai_gate, point, FALSE); };
vector(vector velocity) VR_AI_Vector = { return portal_ai_map(self.vr_ai_gate, velocity, TRUE); };

// Revalidate the exact muzzle at the firing frame, retaining the selected gate.
// A closed/blocked route cancels the shot; it never falls back to aiming through a wall.
float(vector from) VR_AI_ShotReady =
{
    if(self.vr_ai_gate <= 0) { return TRUE; }
    if(self.enemy != self.vr_ai_target || self.enemy.health <= 0) { return FALSE; }
    return portal_ai_sight(self, self.enemy, from, self.enemy.origin + self.enemy.view_ofs, self.vr_ai_gate) > 0;
};

void(float type, vector from, vector to, float part) VR_AI_Beam =
{
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, type);
    WriteEntity(MSG_BROADCAST, self);
    WriteByte(MSG_BROADCAST, part);
    WriteVec3(MSG_BROADCAST, from);
    WriteVec3(MSG_BROADCAST, to);
};

// Lightning's visual and damage segments share the same folded trace.
void(vector from, vector dir, float reach, float type, float damage) VR_AI_Lightning =
{
    if(!VR_AI_ShotReady(from)) { return; }
    vector end = self.vr_ai_gate > 0 ? from + normalize(dir) * reach : self.origin + normalize(dir) * reach;
    traceline(from, end, TRUE | (self.vr_ai_gate > 0 ? MOVE_PORTALS : 0), self);
    vector finish = trace_endpos;
    if(portal_crossings() > 0)
    {
        vector entry = portal_entry(0);
        vector exit = portal_exit(portal_crossings() - 1);
        vector turned = portal_turn(normalize(dir));
        VR_AI_Beam(type, from, entry, 0);
        VR_AI_Beam(type, exit, finish, 1);
        LightningDamage(from, entry, self, damage);
        LightningDamage(exit, finish + turned * 4, self, damage);
    }
    else
    {
        VR_AI_Beam(type, from, finish, 0);
        LightningDamage(from, finish + (type == TE_LIGHTNING2 ? normalize(dir) * 4 : VEC_ORIGIN), self, damage);
    }
};

vector(vector point) VR_AI_HomePoint =
{
    self.vr_ai_gate = 0;
    if(!point_visible(self.origin, point))
        self.vr_ai_gate = portal_ai_sight(self, self.enemy, self.origin, point, 0);
    return portal_ai_map(self.vr_ai_gate, point, FALSE);
};

void(entity projectile) VR_AI_Tag =
{
    projectile.vr_ai_target = self.enemy;
    projectile.vr_ai_gate = self.vr_ai_gate;
};

// QVR: ranged aim cone (vr_enemy_aim_cone; ROUND22.md, "Ranged aim cone"). Quake's monsters start a ranged attack
// facing the player within about 45 degrees (FacingIdeal) and turn at their yaw_speed during it, but each shot aims
// straight at him whatever their facing: circling or sidestepping one still turning, it hit him from 60 degrees or more
// off its facing. Against a player each shot now:
// - leaves along the shooter's facing, bent toward him at most its aim cone (vr_enemy_aim_angle each side, times its
//   kind's multiplier); up and down it is free (VR_AimCone_Bend: the shot's way turned about the vertical only);
// - with him outside its wider fire cone (vr_enemy_aim_fire, times the multiplier) waits first, the monster turning
//   after him meanwhile, up to vr_enemy_aim_delay seconds, then fires (bent as above): never wild (VR_AimCone_Wait).
// One wait an attack: a shot within a second of a wait's end fires unheld (a death knight's six-shot fan waits once).
// Kinds (VR_AimCone_Kind, by classname; a site's own kind for the rest): narrow (VR_AIM_NARROW: hitscan and fast shots,
// vr_enemy_aim_narrow), wide (VR_AIM_WIDE: lobbed and slow ones, vr_enemy_aim_wide), none (homing shots, bosses).
// The ranged counterpart of the melee arc (fight.qc); Show Aim Cones (vr_enemy_aim_show) beside Show Melee Arcs.
float VR_AIMCONE_DEG = 57.29578; // degrees a radian
const float VR_AIM_NONE = 0;   // left alone (homing shots, bosses)
const float VR_AIM_NARROW = 1; // hitscan and fast shots (a grunt's, an enforcer's laser, a gremlin's, a scourge's spikes)
const float VR_AIM_WIDE = 2;   // lobbed and slow shots (an ogre's grenade, a scrag's spit, a death knight's magic)

float vr_aimcone_last_time; // the last shot weighed (VR_AimCone_Bend): when (time), for the test (vr_aimcone_test.qc)
float vr_aimcone_last_off;  // ... its way's angle off its shooter's facing (degrees, level)
float vr_aimcone_last_bent; // ... how far it was bent (degrees; 0: inside the aim cone, as Quake's)
float vr_aimcone_last_wait; // the last wait's length (seconds) and when it ended (time)
float vr_aimcone_last_waited;
float vr_aimcone_waits;     // waits begun (the test)

// The monster a shot is self's: self, or a monster's shot-maker (a scrag's spit maker: its owner).
entity() VR_AimCone_Shooter =
{
    if(self.flags & FL_MONSTER)
    {
        return self;
    }
    if(self.owner && (self.owner.flags & FL_MONSTER))
    {
        return self.owner;
    }
    return world;
};

// A kind by classname (VR_AIM_*), else `site`'s (the shot's own: AD's launchers' by their kind of shot).
float(entity m, float site) VR_AimCone_Kind =
{
    string c = m.classname;
    // Bosses and homing shots: none (AD's bosses share its launchers with everyone).
    if(c == "monster_boss" || c == "monster_oldone" || c == "monster_armagon" || c == "monster_dragon" ||
        c == "monster_super_wrath" || c == "monster_lava_man" || c == "monster_shalrath" || c == "monster_wrath" ||
        c == "monster_super_shambler" || c == "monster_oldone_new" || c == "monster_shub_eye" ||
        c == "monster_shub_seeker" || AD_IsBoss(m))
    {
        return VR_AIM_NONE;
    }
    // An ogre's grenades; MG3's rocket ogre's rockets and AD's ogre's nails by their shot's kind.
    if(c == "monster_ogre" || c == "monster_ogre_marksman")
    {
        return (m.vr_mg3_mon || AD_Run(m)) ? site : VR_AIM_WIDE;
    }
    // Lobbed and slow (AD's too: its launchers' kind is by the shot, a spike or a missile, so slow spikes go here).
    if(c == "monster_zombie" || c == "monster_mummy" ||
        c == "monster_wizard" || c == "monster_hell_knight" || c == "monster_hell_pointknight" ||
        c == "monster_ranged_knight" || c == "monster_dknight" || c == "monster_death_knight" ||
        c == "monster_dknightghost" || c == "monster_golem" || c == "monster_skullwiz" || c == "monster_wraith")
    {
        return VR_AIM_WIDE;
    }
    // Hitscan and fast.
    if(c == "monster_army" || c == "monster_enforcer" || c == "monster_shambler" || c == "monster_gremlin" ||
        c == "monster_scourge" || c == "monster_morph" || c == "monster_ogre_rocket" || c == "monster_orb")
    {
        return VR_AIM_NARROW;
    }
    return site;
};

// Against a player, and on: TRUE.
float(entity m) VR_AimCone_On =
{
    return cvar("vr_enemy_aim_cone") && m && m.health > 0 && m.enemy && (m.enemy.flags & FL_CLIENT);
};

// A kind's aim cone and fire cone (degrees each side of the facing; the fire cone at least the aim cone).
float(float kind) VR_AimCone_Angle =
{
    float a = cvar("vr_enemy_aim_angle") * cvar(kind == VR_AIM_WIDE ? "vr_enemy_aim_wide" : "vr_enemy_aim_narrow");
    return a < 0 ? 0 : (a > 180 ? 180 : a);
};
float(float kind) VR_AimCone_Fire =
{
    float a = cvar("vr_enemy_aim_fire") * cvar(kind == VR_AIM_WIDE ? "vr_enemy_aim_wide" : "vr_enemy_aim_narrow");
    a = a > 180 ? 180 : a;
    float cone = VR_AimCone_Angle(kind);
    return a < cone ? cone : a;
};

// Degrees (signed, -180..180, level) from m's facing to the way `dir`.
float(entity m, vector dir) VR_AimCone_Yaw =
{
    float d = anglemod(atan2(dir_y, dir_x) * VR_AIMCONE_DEG - m.angles_y);
    return d > 180 ? d - 360 : d;
};

// Degrees (0..180, level) between m's facing and the way to its enemy.
float(entity m) VR_AimCone_Off =
{
    vector d = VR_AI_Origin(m.enemy) - m.origin;
    d_z = 0;
    return d == '0 0 0' ? 0 : fabs(VR_AimCone_Yaw(m, d));
};

// A shot's way (a direction, or a velocity: a lobbed grenade's with its upward part) from self's shooter, `kind` its
// kind (VR_AIM_*, unless its class has one): against a player with the aim cone on, turned about the vertical to at
// most the aim cone off the shooter's facing (its length and its up and down kept); else as given.
vector(vector dir, float kind) VR_AimCone_Bend =
{
    entity m = VR_AimCone_Shooter();
    if(!VR_AimCone_On(m))
    {
        return dir;
    }
    kind = VR_AimCone_Kind(m, kind);
    float flat = sqrt(dir_x * dir_x + dir_y * dir_y);
    if(kind == VR_AIM_NONE || flat < 0.0001)
    {
        return dir;
    }
    float cone = VR_AimCone_Angle(kind);
    float d = VR_AimCone_Yaw(m, dir);
    vr_aimcone_last_time = time;
    vr_aimcone_last_off = fabs(d);
    vr_aimcone_last_bent = 0;
    if(fabs(d) > cone)
    {
        vr_aimcone_last_bent = fabs(d) - cone;
        float yaw = (m.angles_y + (d > 0 ? cone : -cone)) / VR_AIMCONE_DEG;
        dir_x = cos(yaw) * flat;
        dir_y = sin(yaw) * flat;
    }
    if(cvar("vr_enemy_aim_debug"))
    {
        bprint(sprintf("aimcone: %s at %v shot %.0f deg off (aim %.0f, fire %.0f; him %.0f off): bent %.0f\n", m.classname,
            m.origin, vr_aimcone_last_off, cone, VR_AimCone_Fire(kind), VR_AimCone_Off(m), vr_aimcone_last_bent));
    }
    return dir;
};

// A shot's direction from `org` toward `targ`, bent (VR_AimCone_Bend): normalize(targ - org)'s.
vector(vector org, vector targ, float kind) VR_AimCone_Dir =
{
    return VR_AimCone_Bend(normalize(targ - org), kind);
};

void() VR_AimCone_HoldThink;

// At a shot, before it is made (`fn` the function that makes it: the shot's frame function, or a fire function with no
// arguments; `kind` as VR_AimCone_Bend's): TRUE if it waits (the caller returns at once, the shot not made). Against a
// player outside the fire cone, with the aim cone on and a delay: the frame is held (self's think VR_AimCone_HoldThink)
// while its shooter turns after him, until he is inside the fire cone or the delay is up; then `fn` runs again and the
// shot is made (this returning FALSE that frame), and the frame goes on to its next as before.
float(void() fn, float kind) VR_AimCone_Wait =
{
    if(self.vr_aim_go == time)
    {
        return FALSE; // (the wait just ended: the shot now)
    }
    if(self.think == VR_AimCone_HoldThink)
    {
        return TRUE; // (a second shot in a held frame: dropped, its wait's own shot made when it ends)
    }
    entity m = VR_AimCone_Shooter();
    float delay = cvar("vr_enemy_aim_delay");
    if(!VR_AimCone_On(m) || delay <= 0 || (self.vr_aim_ended && time < self.vr_aim_ended + 1))
    {
        return FALSE;
    }
    kind = VR_AimCone_Kind(m, kind);
    if(kind == VR_AIM_NONE || VR_AimCone_Off(m) <= VR_AimCone_Fire(kind))
    {
        return FALSE;
    }
    self.vr_aim_fn = fn;
    self.vr_aim_next = self.think;
    self.vr_aim_step = self.nextthink > time ? self.nextthink - time : 0.1;
    self.vr_aim_kind = kind;
    self.vr_aim_holding = time;
    self.think = VR_AimCone_HoldThink;
    self.nextthink = time + 0.1;
    vr_aimcone_waits++;
    if(cvar("vr_enemy_aim_debug"))
    {
        bprint(sprintf("aimcone: %s waits: him %.0f deg off (fire %.0f)\n", m.classname, VR_AimCone_Off(m),
            VR_AimCone_Fire(kind)));
    }
    return TRUE;
};

// A held shot's frame, each tenth of a second (its turns at its own speed, as its frames'): its shooter turns after
// him (unless it is a shot-maker's: its owner's own frames turn it); with him inside the fire cone now or the delay up,
// the shot is made.
void() VR_AimCone_HoldThink =
{
    entity m = VR_AimCone_Shooter();
    float go = !VR_AimCone_On(m) || time >= self.vr_aim_holding + cvar("vr_enemy_aim_delay") - 0.01;
    if(!go)
    {
        if(m == self)
        {
            self.ideal_yaw = vectoyaw(VR_AI_Origin(self.enemy) - self.origin);
            ChangeYaw();
        }
        go = VR_AimCone_Off(m) <= VR_AimCone_Fire(self.vr_aim_kind);
    }
    if(!go)
    {
        self.nextthink = time + 0.1;
        return;
    }
    vr_aimcone_last_wait = time - self.vr_aim_holding;
    vr_aimcone_last_waited = time;
    if(cvar("vr_enemy_aim_debug") && m)
    {
        bprint(sprintf("aimcone: %s waited %.2f s: him %.0f deg off\n", m.classname, vr_aimcone_last_wait,
            VR_AimCone_Off(m)));
    }
    self.vr_aim_holding = 0;
    self.vr_aim_ended = time;
    self.vr_aim_go = time;
    self.think = self.vr_aim_next;
    self.nextthink = time + self.vr_aim_step;
    self.vr_aim_fn();
};

// Show Aim Cones (vr_enemy_aim_show), from StartFrame: each live ranged monster within 1000 units of a player and after
// him gets its cones now (.vr_aim_cone and kin, drawn by the engine: vr_bodycontact.cpp). With 2 also printed once a
// second (aimcone: show), for the logs.
float vr_aimcone_show_print; // (when it last printed)
void() VR_AimCone_ShowFrame =
{
    if(!cvar("vr_enemy_aim_show"))
    {
        return;
    }
    float print = cvar("vr_enemy_aim_show") >= 2 && time >= vr_aimcone_show_print + 1;
    if(print)
    {
        vr_aimcone_show_print = time;
    }
    entity p = find(world, classname, "player");
    while(p)
    {
        entity m = findradius(p.origin, 1000);
        while(m)
        {
            if((m.flags & FL_MONSTER) && m.health > 0 && m.enemy == p && (m.th_missile || AD_HasMissile(m)))
            {
                float kind = VR_AimCone_Kind(m, VR_AIM_NARROW);
                if(kind != VR_AIM_NONE)
                {
                    float on = cvar("vr_enemy_aim_cone");
                    m.vr_aim_cone = on ? VR_AimCone_Angle(kind) : 180;
                    m.vr_aim_fire = on ? VR_AimCone_Fire(kind) : 180;
                    m.vr_aim_shown = time;
                    if(m.think != VR_AimCone_HoldThink)
                    {
                        m.vr_aim_holding = 0; // (a wait cut short: its pain, its death)
                    }
                    if(print)
                    {
                        bprint(sprintf("aimcone: show %s kind %g aim %.0f fire %.0f him %.0f deg off%s\n", m.classname,
                            kind, m.vr_aim_cone, m.vr_aim_fire, VR_AimCone_Off(m), m.vr_aim_holding ? " (waiting)" : ""));
                    }
                }
            }
            m = m.chain;
        }
        p = find(p, classname, "player");
    }
};

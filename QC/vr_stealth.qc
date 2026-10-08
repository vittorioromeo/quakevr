// vr_stealth.qc -- the monsters' stealth AI: Idle, Alert (investigating) or Hostile (Quake's: an enemy); a suspicion
// meter for sight (light, distance, facing, movement, crouching), touch, noises heard through the level, shots grazing,
// flashlight beams, torches, bodies; Alert monsters turn, walk to what disturbed them, search, go back to their post.
// docs/vr-port/STEALTH_PLAN.md has the rules and their cvars (Combat > Stealth AI). vr_ai_enhanced 0 is Quake's AI.
// The engine's side (vr_stealth.cpp): stealthlight, flashlightbeam, pvsvisible; the props' knocks call
// VR_Stealth_PropNoise (vr_physsound.cpp).

float stl_was_on; // vr_ai_enhanced as last seen (StartFrame: switched off, every monster goes back to Quake's AI)
entity stl_players; // the players, listed each frame by .stl_pnext (VR_Stealth_Frame)
// Set while an Alert monster's stand or walk animation is started from its own stand or walk think: a kind whose walk is
// its stand (Rogue's invisible swordsman) would come straight back in (VR_Stealth_Stand, VR_Stealth_Walk: nothing more
// that frame).
float stl_anim_guard;

float() VR_Stealth_On =
{
    return cvar_hget(cvarh_vr_ai_enhanced) && !deathmatch;
};

void(entity m, string s) VR_Stealth_Log =
{
    if(cvar_hget(cvarh_vr_stealth_debug))
    {
        bprint(sprintf("stealth: %.2f %s at %v: %s\n", time, m.classname, m.origin, s));
    }
};

// Its kind's senses, and whether the rules apply to it at all (bosses, flyers and swimmers keep Quake's AI).
void(entity m) VR_Stealth_ClassOf =
{
    string c = m.classname;
    m.stl_class = 1;
    m.stl_hear = 1;
    m.stl_sight = 1;
    if((m.flags & (FL_FLY | FL_SWIM)) || !m.th_stand || !m.th_walk || c == "monster_boss" || c == "monster_boss_final" ||
       c == "monster_oldone" || c == "monster_oldone_new" || substring(c, 0, 13) == "monster_shub_" ||
       c == "monster_dragon" || c == "monster_lava_man" || c == "monster_armagon" || c == "monster_super_shambler")
    {
        m.stl_class = -1;
        return;
    }
    if(c == "monster_dog" || c == "monster_demodog")
    {
        m.stl_hear = 1.6;
    }
    else if(c == "monster_zombie" || c == "monster_mummy")
    {
        m.stl_hear = 0.4;
        m.stl_sight = 0.6;
    }
    else if(c == "monster_demon1" || c == "monster_shambler")
    {
        m.stl_hear = 1.4;
    }
    else if(c == "monster_ogre")
    {
        m.stl_hear = 0.8;
        m.stl_sight = 0.9;
    }
};

float(entity m) VR_Stealth_Applies =
{
    // (a dormant one, not yet in the game: a statue knight before its map wakes it, a monster waiting to be spawned in:
    // no damage taken, or not solid; Quake's: only its map wakes it)
    if(!VR_Stealth_On() || m.health <= 0 || m.charmed || m.takedamage == DAMAGE_NO || m.solid == SOLID_NOT)
    {
        return FALSE;
    }
    if(!m.stl_class)
    {
        VR_Stealth_ClassOf(m);
    }
    return m.stl_class > 0;
};

// FindTarget (self): its sight of a player is the meter's.
float() VR_Stealth_MeterOn =
{
    return cvar_hget(cvarh_vr_stealth_meter) && VR_Stealth_Applies(self);
};

float(entity m) VR_Stealth_Hearing =
{
    return cvar_hget(cvarh_vr_stealth_senses) ? m.stl_hear : 1;
};

float(entity m) VR_Stealth_Ambusher =
{
    return (m.spawnflags & 1) && cvar_hget(cvarh_vr_stealth_ambush_deaf);
};

// Its stand (still: turning, searching) or walk animation, as its AI's own thinks start them.
void(entity m, float xWalking) VR_Stealth_Anim =
{
    entity os = self;
    float og = stl_anim_guard;
    self = m;
    stl_anim_guard = TRUE;
    if(xWalking)
    {
        self.th_walk();
    }
    else
    {
        self.pausetime = 99999999;
        self.th_stand();
    }
    stl_anim_guard = og;
    self = os;
};

entity(entity m, vector at) VR_Stealth_MarkerAt;

// ----------------------------------------------------------------------------
// Slipgates (vr_stealth_gates; seamless slipgates' paired gates, vr_portals.cpp; docs/vr-port/STEALTH_PLAN.md,
// "Slipgates"): what a monster sees or hears through a gate is its image there (beyond the gate's face, in its own room's
// terms). It walks into the gate and through, and its points go through with it (VR_Stealth_Crossed): the one beyond
// is then in its room, the ones it left behind beyond the face back, so it walks on instead of back.

// A sighting's gate (visible()'s vr_ai_gate) as a point's way (stl_via): the face it walks through (an exit: its pair's
// face), or minus the gate when it can't (not paired, vr_portals_monsters 0, a player-only teleport).
float(float xGate) VR_Stealth_Via =
{
    if(xGate <= 0)
    {
        return 0;
    }
    if(portal_ai_gateinfo(xGate, 1) & 4)
    {
        float face = portal_ai_gateinfo(xGate, 2);
        if(face > 0)
        {
            return face;
        }
    }
    return -xGate;
};

// Where a monster walks for `point` seen through `xVia`: into the gate's face (its middle, beyond its plane: it goes
// through), or up to the face of one it can't walk through; no gate (or one shut since): the point.
vector(vector point, float xVia) VR_Stealth_WayTo =
{
    if(!xVia || !(portal_ai_gateinfo(fabs(xVia), 1) & 1))
    {
        return point;
    }
    vector mid = portal_ai_gate(fabs(xVia), 0, VEC_ORIGIN);
    vector n = portal_ai_gate(fabs(xVia), 1, VEC_ORIGIN);
    return xVia > 0 ? mid - n * 48 : mid + n * 48;
};

// visible() (self): under the stealth rules every monster sees a player through paired gates (ranged ones through
// every gate, vr_portal_ai.qc).
float() VR_Stealth_GateSight =
{
    return cvar_hget(cvarh_vr_stealth_gates) && VR_Stealth_MeterOn();
};

// p carried as a crossing carried its monster from `xFrom` to `xTo`, turning `xYaw` degrees.
vector(vector p, vector xFrom, vector xTo, float xYaw) VR_Stealth_Carried =
{
    float c = cos(xYaw * (3.14159265 / 180));
    float s = sin(xYaw * (3.14159265 / 180));
    vector d = p - xFrom;
    vector r;
    r_x = d_x * c - d_y * s;
    r_y = d_x * s + d_y * c;
    r_z = d_z;
    return xTo + r;
};

// A point's way after its monster went through face `xFace`: there (no gate) when that was the way to it, else back
// through `xBack` (the face in the new room that leads back).
float(float xVia, float xFace, float xBack) VR_Stealth_ViaAfter =
{
    if(xVia > 0 && xVia == xFace)
    {
        return 0;
    }
    return xBack;
};

// VR_Portal_Crossed (triggers.qc): the engine walked m through a gate, from `xFrom` (now at its origin), turned `xYaw`.
void(entity m, vector xFrom, float xYaw, float xFace, float xBack) VR_Stealth_Crossed =
{
    if(!(m.flags & FL_MONSTER) || !VR_Stealth_On())
    {
        return;
    }
    vector to = m.origin;
    m.stl_point = VR_Stealth_Carried(m.stl_point, xFrom, to, xYaw);
    m.stl_via = VR_Stealth_ViaAfter(m.stl_via, xFace, xBack);
    m.stl_post = VR_Stealth_Carried(m.stl_post, xFrom, to, xYaw);
    m.stl_post_via = VR_Stealth_ViaAfter(m.stl_post_via, xFace, xBack);
    m.stl_post_yaw = anglemod(m.stl_post_yaw + xYaw);
    m.stl_lost_at = VR_Stealth_Carried(m.stl_lost_at, xFrom, to, xYaw);
    m.stl_lost_via = VR_Stealth_ViaAfter(m.stl_lost_via, xFace, xBack);
    m.stl_walk_from = VR_Stealth_Carried(m.stl_walk_from, xFrom, to, xYaw);
    if(m.stl_marker.owner == m && m.stl_marker.classname == "stl_marker")
    {
        if(m.stl_state == STL_ALERT && m.stl_phase == STL_WALK)
        {
            setorigin(m.stl_marker, VR_Stealth_WayTo(m.stl_point, m.stl_via));
        }
        else if(m.stl_state == STL_ALERT && m.stl_phase == STL_RETURN)
        {
            setorigin(m.stl_marker, VR_Stealth_WayTo(m.stl_post, m.stl_post_via));
        }
        else
        {
            setorigin(m.stl_marker, VR_Stealth_Carried(m.stl_marker.origin, xFrom, to, xYaw));
        }
    }
    if(cvar_hget(cvarh_vr_stealth_debug))
    {
        VR_Stealth_Log(m, sprintf("through gate %g (back %g): point %v via %g, post via %g", xFace, xBack, m.stl_point,
            m.stl_via, m.stl_post_via));
    }
};

// In ai_run (self: Hostile, a melee monster under the stealth rules): its enemy seen only through a paired gate: it
// runs into the gate and through (ranged ones shoot through and stay); one it can't walk through: it faces him there.
void(float dist) VR_Stealth_ChaseGate =
{
    float via = VR_Stealth_Via(self.vr_ai_gate);
    if(via <= 0)
    {
        self.ideal_yaw = vectoyaw(VR_AI_Origin(self.enemy) - self.origin);
        ChangeYaw();
        return;
    }
    entity goal = self.goalentity;
    self.goalentity = VR_Stealth_MarkerAt(self, VR_Stealth_WayTo(VEC_ORIGIN, via));
    movetogoal(dist);
    self.goalentity = goal;
};

// A marker whose monster is gone (removed without dying: a map's killtarget, a fade) goes too.
void() VR_Stealth_MarkerThink =
{
    if(self.owner.stl_marker != self || self.owner.health <= 0)
    {
        remove(self);
        return;
    }
    self.nextthink = time + 5;
};

entity(entity m, vector at) VR_Stealth_MarkerAt =
{
    if(m.stl_marker == world || m.stl_marker.classname != "stl_marker")
    {
        m.stl_marker = spawn();
        m.stl_marker.classname = "stl_marker";
        m.stl_marker.owner = m;
        m.stl_marker.think = VR_Stealth_MarkerThink;
        m.stl_marker.nextthink = time + 5;
    }
    setorigin(m.stl_marker, at);
    return m.stl_marker;
};

void(entity m, float xPhase) VR_Stealth_Phase =
{
    m.stl_phase = xPhase;
    m.stl_phase_time = time;
    m.stl_stuck = 0;
    m.stl_walk_from = m.origin;
    m.stl_walk_check = time + 1;
    if(xPhase == STL_SEARCH)
    {
        m.stl_look_base = m.angles_y;
        m.stl_look_time = time + 0.8;
    }
    else if(xPhase == STL_WALK || xPhase == STL_RETURN)
    {
        // (walking: the time it allows itself, by the way's length: a grunt walks about 18 units a second)
        m.stl_look_time = time + 10 + vlen((xPhase == STL_WALK ? m.stl_point : m.stl_post) - m.origin) / 10;
    }
};

// Back to Quake's AI (the umbrella switched off, or the rules no longer apply to it): the meter cleared, an Alert
// monster's investigation dropped, its path (or none) and its stand or walk animation back.
void(entity m) VR_Stealth_Reset =
{
    m.stl_meter = 0;
    m.stl_lit = 0;
    m.stl_settle = 0;
    if(m.stl_state != STL_ALERT)
    {
        return;
    }
    m.stl_state = STL_IDLE;
    m.stl_phase = 0;
    if(m.enemy != world || m.health <= 0)
    {
        return;
    }
    m.goalentity = m.movetarget = m.stl_post_goal;
    VR_Stealth_Anim(m, m.stl_post_walk);
    VR_Stealth_Log(m, "reset (Quake's AI)");
};

// Investigated and found nothing: Idle again at its post (or on its path), wary for a while.
void(entity m) VR_Stealth_Finish =
{
    m.stl_state = STL_IDLE;
    m.stl_phase = 0;
    m.stl_wary = time + cvar_hget(cvarh_vr_stealth_sensitive_time);
    m.goalentity = m.movetarget = m.stl_post_goal;
    if(m.stl_post_walk)
    {
        VR_Stealth_Anim(m, TRUE);
    }
    else
    {
        m.ideal_yaw = m.stl_post_yaw;
        m.stl_settle = TRUE;
        VR_Stealth_Anim(m, FALSE);
    }
    VR_Stealth_Log(m, "idle again");
};

void(entity m) VR_Stealth_GoBack =
{
    vector to = m.stl_post - m.origin;
    to_z = 0;
    // (its post beyond a slipgate it went through: back through it first, VR_Stealth_WayTo; even to its path)
    if((m.stl_post_via <= 0 && (m.stl_post_walk || vlen(to) < 32)) || (m.spawnflags & 1) || m.th_walk == m.th_stand)
    {
        VR_Stealth_Finish(m);
        return;
    }
    VR_Stealth_Phase(m, STL_RETURN);
    vector way = VR_Stealth_WayTo(m.stl_post, m.stl_post_via);
    m.goalentity = VR_Stealth_MarkerAt(m, way);
    m.ideal_yaw = vectoyaw(way - m.origin); // (movetogoal keeps stepping its way while it can)
    VR_Stealth_Anim(m, TRUE);
};

// Its "huh?": its idle sound, a little quieter (the ones its spawn function precached).
void(entity m) VR_Stealth_Voice =
{
    string c = m.classname;
    string s = "";
    if(c == "monster_army") { s = "soldier/idle.wav"; }
    else if(c == "monster_knight") { s = "knight/idle.wav"; }
    else if(c == "monster_ogre") { s = "ogre/ogidle.wav"; }
    else if(c == "monster_demon1") { s = "demon/idle1.wav"; }
    else if(c == "monster_dog") { s = "dog/idle.wav"; }
    else if(c == "monster_shambler") { s = "shambler/sidle.wav"; }
    else if(c == "monster_zombie") { s = "zombie/z_idle.wav"; }
    else if(c == "monster_gremlin") { s = "grem/idle.wav"; }
    else if(c == "monster_scourge") { s = "scourge/idle.wav"; }
    if(s != "" && !(!MG_WorldCampaign() && (m.spawnflags & SPAWNFLAG_SF_SILENT_WAKEUP)))
    {
        sound(m, CHAN_VOICE, s, 0.7, ATTN_NORM);
    }
};

// The deferred share of an alarm (VR_Stealth_Share): next frame, never inside the loop that raised it (a findradius
// loop's chain would be overwritten: T_RadiusDamage's, a noise's).
void() VR_Stealth_ShareThink;

void(entity m, float xHostile) VR_Stealth_ShareLater =
{
    if(cvar_hget(cvarh_vr_stealth_share_near) <= 0 && cvar_hget(cvarh_vr_stealth_share_view) <= 0)
    {
        return;
    }
    entity ev = spawn();
    ev.classname = "stl_share";
    ev.owner = m;
    ev.cnt = xHostile;
    ev.think = VR_Stealth_ShareThink;
    ev.nextthink = time;
};

// m went Alert or Hostile: the idle (and, for Hostile, Alert) monsters near it (in line, not through walls) or seeing it
// within the view range (half for Alert) do too: Hostile at its enemy (as monster_use: their FoundTarget next think),
// Alert at its point (one hop: theirs isn't shared on).
void(entity m, float xHostile) VR_Stealth_Share =
{
    float near = cvar_hget(cvarh_vr_stealth_share_near);
    float view = cvar_hget(cvarh_vr_stealth_share_view);
    if(!xHostile)
    {
        view = view * 0.5;
    }
    float reach = max(near, view);
    if(reach <= 0 || m.health <= 0 || !VR_Stealth_On())
    {
        return;
    }
    if(xHostile && m.enemy.classname != "player")
    {
        return;
    }
    if(!xHostile && m.stl_state != STL_ALERT)
    {
        return;
    }
    entity os = self;
    vector eye = m.origin + m.view_ofs;
    for(entity e = findradius(m.origin, reach); e; e = e.chain)
    {
        if(e == m || !(e.flags & FL_MONSTER) || e.health <= 0 || e.enemy != world || !VR_Stealth_Applies(e) ||
           VR_Stealth_Ambusher(e) || (!xHostile && e.stl_state == STL_ALERT))
        {
            continue;
        }
        traceline(eye, e.origin + e.view_ofs, TRUE, m);
        if(trace_fraction < 1)
        {
            continue;
        }
        float d = vlen(e.origin - m.origin);
        if(d > near)
        {
            self = e;
            float sees = d <= view && infront(m);
            self = os;
            if(!sees)
            {
                continue;
            }
        }
        if(xHostile)
        {
            e.enemy = m.enemy;
            e.think = FoundTarget;
            e.nextthink = time + 0.1 + random() * 0.2;
            VR_Stealth_Log(e, "hostile (shared)");
        }
        else
        {
            VR_Stealth_AlertVia(e, m.stl_point, m.stl_via, STL_A_SHARED, 0.5, FALSE); // (in line: the same room)
        }
    }
};

void() VR_Stealth_ShareThinkNow =
{
    entity ev = self;
    VR_Stealth_Share(ev.owner, ev.cnt);
    remove(ev);
};

void() VR_Stealth_ShareThink =
{
    stealthprofile(1);
    VR_Stealth_ShareThinkNow();
    stealthprofile(0);
};

// Something drew m's attention to `at` (STL_A_* or a noise's kind; `xPower` how strongly: a new disturbance as weak
// or weaker within a second is ignored). Idle: Alert, it turns to look (its post recorded); Alert already: the new
// point (walking: it walks there instead; searching or going back: it turns to it again). `xVia`: the slipgate `at` is
// seen through (stl_via; 0 none).
void(entity m, vector at, float xVia, float xKind, float xPower, float xShare) VR_Stealth_AlertVia =
{
    if(m.enemy != world || !VR_Stealth_Applies(m))
    {
        return;
    }
    if(m.stl_state == STL_ALERT && time - m.stl_alert_time < 1 && xPower <= m.stl_alert_power)
    {
        return;
    }
    float first = (m.stl_state != STL_ALERT);
    if(first)
    {
        m.stl_post = m.origin;
        m.stl_post_via = 0;
        m.stl_post_yaw = m.angles_y;
        m.stl_post_goal = m.movetarget;
        m.stl_post_walk = ((m.movetarget.classname == "path_corner") && (m.pausetime < 99999999)); // (walking its path, or waiting at a corner)
        m.stl_settle = 0;
        VR_Stealth_Voice(m);
        if(cvar_hget(cvarh_vr_stealth_marks))
        {
            floattext(m.origin + '0 0 40', "?", '1 0.85 0.2', 1);
        }
    }
    m.stl_state = STL_ALERT;
    m.stl_point = at;
    m.stl_via = xVia;
    m.stl_alert_time = time;
    m.stl_alert_power = xPower;
    if(first || m.stl_phase == STL_SEARCH || m.stl_phase == STL_RETURN)
    {
        VR_Stealth_Phase(m, STL_TURN);
        VR_Stealth_Anim(m, FALSE);
    }
    else if(m.stl_phase == STL_WALK)
    {
        m.goalentity = VR_Stealth_MarkerAt(m, VR_Stealth_WayTo(at, xVia));
    }
    if(cvar_hget(cvarh_vr_stealth_debug))
    {
        VR_Stealth_Log(m, sprintf("alert (kind %g, power %.2f) at %v%s", xKind, xPower, at,
            xVia ? sprintf(" through gate %g", xVia) : "")); // (built only when logged)
    }
    if(first && xShare)
    {
        VR_Stealth_ShareLater(m, FALSE);
    }
};

void(entity m, vector at, float xKind, float xPower, float xShare) VR_Stealth_Alert =
{
    VR_Stealth_AlertVia(m, at, 0, xKind, xPower, xShare);
};

// m spots p (touch, a noise he made in its sight): Hostile, as Quake's FindTarget's sighting.
void(entity m, entity p) VR_Stealth_Spot =
{
    if(m.enemy != world || (p.flags & FL_NOTARGET) || (p.items & IT_INVISIBILITY))
    {
        return;
    }
    entity os = self;
    self = m;
    self.enemy = p;
    FoundTarget();
    self = os;
};

// In FoundTarget (self): Hostile. Any Alert state goes; an enemy that is a player is shared with the monsters near.
void() VR_Stealth_Hostile =
{
    if(self.stl_state == STL_ALERT)
    {
        self.movetarget = self.stl_post_goal;
    }
    self.stl_state = STL_IDLE;
    self.stl_phase = 0;
    self.stl_meter = 0;
    self.stl_settle = 0;
    self.stl_lost_time = time;
    if(!VR_Stealth_Applies(self) || self.enemy.classname != "player")
    {
        return;
    }
    if(cvar_hget(cvarh_vr_stealth_marks))
    {
        floattext(self.origin + '0 0 40', "!", '1 0.2 0.1', 1);
    }
    VR_Stealth_Log(self, "hostile");
    VR_Stealth_ShareLater(self, TRUE);
};

// ----------------------------------------------------------------------------
// Sight: the suspicion meter

// The light on a player (128: Quake's full light), measured at most ten times a second.
float(entity p) VR_Stealth_LightOf =
{
    float force = cvar_hget(cvarh_vr_stealth_light_force);
    if(force >= 0)
    {
        return force;
    }
    if(time >= p.stl_light_time || time < p.stl_light_time - 1)
    {
        p.stl_light_time = time + 0.1;
        // (as his own client measured it: his dynamic lights, a coop client's too; else the host's view of the place)
        float l = clientlight(p);
        if(l < 0)
        {
            l = stealthlight(p.origin);
        }
        p.stl_light = l < 0 ? 128 : l;
    }
    return p.stl_light;
};

float(float xLight) VR_Stealth_LightShare =
{
    float lo = cvar_hget(cvarh_vr_stealth_light_dark);
    float hi = cvar_hget(cvarh_vr_stealth_light_bright);
    if(xLight <= lo)
    {
        return 0;
    }
    if(xLight >= hi || hi <= lo)
    {
        return 1;
    }
    float t = (xLight - lo) / (hi - lo);
    return t * t * (3 - 2 * t);
};

float(entity p) VR_Stealth_MoveShare =
{
    vector v = p.velocity;
    v_z = 0;
    float s = vlen(v);
    if(s < 30)
    {
        return cvar_hget(cvarh_vr_stealth_still);
    }
    return s > cvar_hget(cvarh_vr_stealth_run_speed) ? 2 : 1;
};

// A full crouch (eyes 40% under his standing eye height): vr_stealth_crouch; standing 1; between, between.
float(entity p) VR_Stealth_CrouchShare =
{
    if(p.headpos == '0 0 0' || p.stl_stand_eye <= 8)
    {
        return 1;
    }
    float f = (p.stl_stand_eye - (p.headpos_z - p.absmin_z)) / (0.4 * p.stl_stand_eye);
    f = max(0, min(1, f));
    return 1 + (cvar_hget(cvarh_vr_stealth_crouch) - 1) * f;
};

// FindTarget's sighting of a player (self: the monster, Idle or Alert): the meter fills by the light on him, how near,
// in front, moving, standing up; full: Hostile (FoundTarget, TRUE). Past the glimpse share: Alert, turning to look.
float(entity client) VR_Stealth_SeeNow =
{
    if(!visible(client))
    {
        return FALSE;
    }
    vector eye = self.origin + self.view_ofs;
    float d = vlen(VR_AI_Origin(client) + client.view_ofs - eye);
    float far = cvar_hget(cvarh_vr_stealth_sight_range);
    if(d >= far)
    {
        return FALSE;
    }
    float nearShare = d <= 128 ? 1 : 1 - (d - 128) / max(1, far - 128);
    makevectors(self.angles);
    float dot = normalize(VR_AI_Origin(client) - self.origin) * v_forward;
    float facing = dot > 0.3 ? 1 : (dot > -0.3 ? cvar_hget(cvarh_vr_stealth_peripheral) : 0);
    float light = (self.stl_lit > time && self.stl_lit_by == client) ? 1 : VR_Stealth_LightShare(VR_Stealth_LightOf(client));
    float rate = light * nearShare * facing * VR_Stealth_MoveShare(client) * VR_Stealth_CrouchShare(client) *
        (cvar_hget(cvarh_vr_stealth_senses) ? self.stl_sight : 1) *
        (time < self.stl_wary ? cvar_hget(cvarh_vr_stealth_sensitive) : 1) / max(0.05, cvar_hget(cvarh_vr_stealth_meter_time));
    if(rate <= 0)
    {
        if(client == self.stl_suspect)
        {
            self.stl_rate = 0;
        }
        return FALSE;
    }
    // Coop: the meter is on the most suspicious player in sight. FindTarget's sightings take the players in turn (a tenth
    // of a second each): another one's while its suspect, seen lately, fills it faster is passed over (its suspect's
    // next sighting counts the time between); a faster one takes the meter over.
    if(client != self.stl_suspect)
    {
        if(self.stl_suspect.classname == "player" && self.stl_suspect.health > 0 && time - self.stl_seen <= 0.25 &&
           time >= self.stl_seen && rate <= self.stl_rate)
        {
            return FALSE;
        }
        self.stl_suspect = client;
    }
    self.stl_rate = rate;
    float dt = time - self.stl_seen;
    if(dt > 0.25 || dt < 0)
    {
        dt = 0.1;
    }
    self.stl_seen = time;
    self.stl_meter = self.stl_meter + rate * dt;
    if(cvar_hget(cvarh_vr_stealth_debug) >= 2)
    {
        bprint(sprintf("stealth: %.2f %s meter %.2f (rate %.2f: light %.2f near %.2f facing %.2f)\n", time, self.classname,
            self.stl_meter, rate, light, nearShare, facing));
    }
    if(self.stl_meter >= 1)
    {
        self.stl_meter = 1;
        self.enemy = client;
        FoundTarget();
        return TRUE;
    }
    if(self.stl_meter >= cvar_hget(cvarh_vr_stealth_glimpse))
    {
        // (seen through a slipgate: his image there, vr_stealth_gates; visible() above chose the gate)
        if(self.vr_ai_gate > 0 && cvar_hget(cvarh_vr_stealth_gates))
        {
            VR_Stealth_AlertVia(self, VR_AI_Origin(client), VR_Stealth_Via(self.vr_ai_gate), STL_A_GLIMPSE, 1, TRUE);
        }
        else
        {
            VR_Stealth_Alert(self, client.origin, STL_A_GLIMPSE, 1, TRUE);
        }
    }
    return FALSE;
};

float(entity client) VR_Stealth_See =
{
    stealthprofile(1);
    float r = VR_Stealth_SeeNow(client);
    stealthprofile(0);
    return r;
};

// ----------------------------------------------------------------------------
// What else it notices: flashlight beams, torches, bodies (VR_Stealth_Tick, a few times a second)

// The player's flashlight: lighting m (Alert at the lens: it knows where the light comes from; and its meter sees him
// lit) or landing where m sees it (Alert at a guess round the lens: it looks about there).
void(entity m, entity p, vector eye) VR_Stealth_Beam =
{
    vector b = flashlightbeam(p, 2);
    if(b_x < 1)
    {
        return;
    }
    vector lens = flashlightbeam(p, 0);
    vector dir = flashlightbeam(p, 1);
    vector mid = (m.absmin + m.absmax) * 0.5;
    vector to = mid - lens;
    float d = vlen(to);
    if(d < b_y && d > 1 && normalize(to) * dir > b_z)
    {
        traceline(lens, mid, TRUE, p);
        if(trace_fraction >= 1 || trace_ent == m)
        {
            m.stl_lit = time + 0.5;
            m.stl_lit_by = p;
            VR_Stealth_Alert(m, lens, STL_A_BEAM, 1.5, TRUE);
            return;
        }
    }
    traceline(lens, lens + dir * b_y, FALSE, p);
    if(trace_fraction >= 1 || trace_ent == m)
    {
        return;
    }
    vector spot = trace_endpos - dir * 4;
    if(vlen(spot - eye) > 1000 || normalize(spot - eye) * v_forward < 0.3)
    {
        return;
    }
    traceline(eye, spot, TRUE, m);
    if(trace_fraction < 1)
    {
        return;
    }
    vector guess = lens + '256 0 0' * crandom() + '0 256 0' * crandom();
    VR_Stealth_Alert(m, guess, STL_A_BEAMSEEN, 0.6, TRUE);
};

// A lit torch taken from its wall (held or thrown) or a body or gib (STL_NOTICE_BODY) m sees in front of it.
// The next entity after `start` with `flags` in `fld` whose box's centre is within `range` of `eye` and ahead of it
// (v_forward, cos 0.3): findflagsinview. vr_edictindex_verify: QuakeC's own test too, any difference printed.
entity(entity start, .float fld, float flags, vector eye, float range) VR_Stealth_InView =
{
    entity got = findflagsinview(start, fld, flags, eye, range, 0.3);
    if(cvar_hget(cvarh_vr_edictindex_verify))
    {
        entity e;
        vector at;
        for(e = findflags(start, fld, flags); e; e = findflags(e, fld, flags))
        {
            at = (e.absmin + e.absmax) * 0.5;
            if(vlen(at - eye) > range || normalize(at - eye) * v_forward < 0.3)
            {
                continue;
            }
            break;
        }
        if(e != got)
        {
            bprint(sprintf("vr_edictindex ERROR: findflagsinview after %s at %v: %s at %v, QuakeC's %s at %v\n",
                start.classname, start.origin, got.classname, got.origin, e.classname, e.origin));
        }
    }
    return got;
};

void(entity m, vector eye) VR_Stealth_LookAbout =
{
    float bodies = cvar_hget(cvarh_vr_stealth_corpses);
    float torches = cvar_hget(cvarh_vr_stealth_torch);
    entity e;
    vector at;
    if(torches > 0)
    {
        // (the distance and the cone in the engine, in these same float steps: findflagsinview)
        for(e = VR_Stealth_InView(world, wt_state, 1, eye, torches); e; e = VR_Stealth_InView(e, wt_state, 1, eye, torches))
        {
            if(e.wt_fire < 0.1)
            {
                continue;
            }
            at = (e.absmin + e.absmax) * 0.5;
            traceline(eye, at, TRUE, m);
            if(trace_fraction >= 1)
            {
                VR_Stealth_Alert(m, at, STL_A_TORCH, 0.8, TRUE);
                return;
            }
        }
    }
    if(bodies > 0)
    {
        for(e = VR_Stealth_InView(world, stl_notice, STL_NOTICE_BODY, eye, bodies); e;
            e = VR_Stealth_InView(e, stl_notice, STL_NOTICE_BODY, eye, bodies))
        {
            if(e == m || e == m.stl_body || (e.stl_found && time - e.stl_found > 30))
            {
                continue;
            }
            at = (e.absmin + e.absmax) * 0.5;
            traceline(eye, at, TRUE, m);
            if(trace_fraction >= 1)
            {
                if(!e.stl_found)
                {
                    e.stl_found = time;
                }
                m.stl_body = e;
                VR_Stealth_Alert(m, at, STL_A_BODY, 0.8, TRUE);
                return;
            }
        }
    }
};

// At the start of FindTarget (self: an Idle or Alert monster): the meter drains while he isn't seen; a few times a
// second it looks for beams, torches and bodies. With the rules off for it: back to Quake's AI.
void() VR_Stealth_TickNow =
{
    if(!VR_Stealth_Applies(self))
    {
        if(self.stl_state == STL_ALERT || self.stl_meter > 0)
        {
            VR_Stealth_Reset(self);
        }
        return;
    }
    float dt = time - self.stl_decayed;
    self.stl_decayed = time;
    if(dt > 0 && dt < 1 && self.stl_meter > 0 && time - self.stl_seen > 0.25)
    {
        self.stl_meter = max(0, self.stl_meter - dt * cvar_hget(cvarh_vr_stealth_meter_decay));
    }
    if(time < self.stl_scan_time && time > self.stl_scan_time - 2)
    {
        return;
    }
    self.stl_scan_time = time + 0.2 + random() * 0.1;
    vector eye = self.origin + self.view_ofs;
    if(cvar_hget(cvarh_vr_stealth_flashlight))
    {
        for(entity p = stl_players; p; p = p.stl_pnext)
        {
            if(p.health > 0 && !(p.flags & FL_NOTARGET))
            {
                makevectors(self.angles);
                VR_Stealth_Beam(self, p, eye);
                if(self.enemy != world)
                {
                    return;
                }
            }
        }
    }
    makevectors(self.angles);
    VR_Stealth_LookAbout(self, eye);
};

void() VR_Stealth_Tick =
{
    stealthprofile(1);
    VR_Stealth_TickNow();
    stealthprofile(0);
};

// ----------------------------------------------------------------------------
// The Alert monster's thinks (ai_stand, ai_walk; self)

void() VR_Stealth_Turn =
{
    float ys = self.yaw_speed;
    self.yaw_speed = ys * max(0.1, cvar_hget(cvarh_vr_stealth_turn));
    ChangeYaw();
    self.yaw_speed = ys;
};

// In ai_stand after FindTarget: TRUE when the Alert state took the think (turning to look, searching).
float() VR_Stealth_StandNow =
{
    if(stl_anim_guard && self.stl_state == STL_ALERT)
    {
        return TRUE; // (its animation being switched: VR_Stealth_Anim)
    }
    if(self.stl_state != STL_ALERT)
    {
        if(self.stl_settle)
        {
            if(!VR_Stealth_Applies(self))
            {
                self.stl_settle = 0;
                return FALSE;
            }
            self.ideal_yaw = self.stl_post_yaw;
            VR_Stealth_Turn();
            if(FacingIdeal())
            {
                self.stl_settle = 0;
            }
        }
        return FALSE;
    }
    if(!VR_Stealth_Applies(self))
    {
        VR_Stealth_Reset(self);
        return TRUE;
    }
    if(self.stl_phase == STL_TURN)
    {
        self.ideal_yaw = vectoyaw(self.stl_point - self.origin);
        VR_Stealth_Turn();
        if(FacingIdeal() || time - self.stl_phase_time > 2.5)
        {
            vector to = self.stl_point - self.origin;
            to_z = 0;
            if((vlen(to) > 64 || self.stl_via) && cvar_hget(cvarh_vr_stealth_investigate) && !(self.spawnflags & 1) &&
               self.th_walk != self.th_stand) // (a kind that doesn't walk searches where it stands)
            {
                VR_Stealth_Phase(self, STL_WALK);
                self.goalentity = VR_Stealth_MarkerAt(self, VR_Stealth_WayTo(self.stl_point, self.stl_via));
                VR_Stealth_Anim(self, TRUE);
            }
            else
            {
                VR_Stealth_Phase(self, STL_SEARCH);
            }
        }
        return TRUE;
    }
    if(self.stl_phase == STL_SEARCH)
    {
        if(time >= self.stl_look_time)
        {
            self.stl_look_time = time + 1 + random() * 0.6;
            self.ideal_yaw = anglemod(self.stl_look_base + (random() - 0.5) * 120);
        }
        VR_Stealth_Turn();
        if(time - self.stl_phase_time > cvar_hget(cvarh_vr_stealth_search_time))
        {
            VR_Stealth_GoBack(self);
        }
        return TRUE;
    }
    VR_Stealth_Anim(self, TRUE); // (walking or going back, in a stand frame: walk on)
    return TRUE;
};

float() VR_Stealth_Stand =
{
    stealthprofile(1);
    float r = VR_Stealth_StandNow();
    stealthprofile(0);
    return r;
};

// In ai_walk after FindTarget: TRUE when the Alert state took the think (walking to the point, or back to its post).
float(float dist) VR_Stealth_WalkNow =
{
    if(stl_anim_guard && self.stl_state == STL_ALERT)
    {
        return TRUE; // (its animation being switched: VR_Stealth_Anim)
    }
    if(self.stl_state != STL_ALERT)
    {
        return FALSE;
    }
    if(!VR_Stealth_Applies(self))
    {
        VR_Stealth_Reset(self);
        return TRUE;
    }
    if(self.stl_phase != STL_WALK && self.stl_phase != STL_RETURN)
    {
        VR_Stealth_Anim(self, FALSE);
        return TRUE;
    }
    // (a point beyond a slipgate: into the gate first, VR_Stealth_WayTo)
    float via = self.stl_phase == STL_WALK ? self.stl_via : self.stl_post_via;
    vector goal = VR_Stealth_WayTo(self.stl_phase == STL_WALK ? self.stl_point : self.stl_post, via);
    vector to = goal - self.origin;
    to_z = 0;
    if(self.stl_phase == STL_RETURN && !via && self.stl_post_walk)
    {
        VR_Stealth_Finish(self); // (back through the gate to its path's room: on along its path)
        return TRUE;
    }
    if(time >= self.stl_walk_check)
    {
        self.stl_stuck = vlen(self.origin - self.stl_walk_from) < 16 ? self.stl_stuck + 1 : 0;
        self.stl_walk_from = self.origin;
        self.stl_walk_check = time + 1;
        // Each second, headed for it again (movetogoal keeps stepping the way it goes while it can: round an
        // obstacle it chose another).
        if(!self.stl_stuck)
        {
            self.ideal_yaw = vectoyaw(goal - self.origin);
        }
    }
    // (not into lava or slime: a pool level with the floor that Quake's step check lets it walk into; it ends the walk
    // there, as stuck)
    float molten = FALSE;
    if(vlen(to) >= 1)
    {
        vector ahead = self.origin + normalize(to) * (self.maxs_x + 8 + dist);
        ahead_z = self.absmin_z + 16;
        traceline(ahead, ahead - '0 0 80', TRUE, self);
        float c = pointcontents(trace_endpos + '0 0 1');
        molten = (c == CONTENT_LAVA || c == CONTENT_SLIME);
    }
    if((via <= 0 && vlen(to) < (self.stl_phase == STL_WALK ? 64 : 24)) || self.stl_stuck >= 3 || time > self.stl_look_time ||
       molten)
    {
        if(self.stl_phase == STL_WALK)
        {
            VR_Stealth_Phase(self, STL_SEARCH);
            VR_Stealth_Anim(self, FALSE);
        }
        else
        {
            VR_Stealth_Finish(self);
        }
        return TRUE;
    }
    if(self.goalentity != self.stl_marker || self.stl_marker.origin != goal)
    {
        self.goalentity = VR_Stealth_MarkerAt(self, goal);
    }
    movetogoal(dist);
    return TRUE;
};

float(float dist) VR_Stealth_Walk =
{
    stealthprofile(1);
    float r = VR_Stealth_WalkNow(dist);
    stealthprofile(0);
    return r;
};

// In ai_run (self: Hostile), its enemy in sight or not: a player out of its sight for STL_LOSE_TIME: it gives up the
// hunt and goes to where it saw him last, Alert (searches there, then goes back to Idle, wary). TRUE: it gave up.
float STL_LOSE_TIME = 6;

float(float xVis) VR_Stealth_LoseNow =
{
    if(self.enemy.classname != "player" || !cvar_hget(cvarh_vr_stealth_lose) || !VR_Stealth_Applies(self))
    {
        return FALSE;
    }
    if(xVis || !self.stl_lost_time)
    {
        self.stl_lost_time = time;
        self.stl_lost_at = self.enemy.origin;
        self.stl_lost_via = 0;
        if(xVis && self.vr_ai_gate > 0 && cvar_hget(cvarh_vr_stealth_gates)) // (seen through a slipgate: his image there)
        {
            self.stl_lost_at = VR_AI_Origin(self.enemy);
            self.stl_lost_via = VR_Stealth_Via(self.vr_ai_gate);
        }
        return FALSE;
    }
    if(time - self.stl_lost_time < STL_LOSE_TIME)
    {
        return FALSE;
    }
    VR_Stealth_Log(self, "lost him");
    self.enemy = self.oldenemy = self.goalentity = self.movetarget = world;
    VR_Stealth_AlertVia(self, self.stl_lost_at, self.stl_lost_via, STL_A_LOST, 3, FALSE);
    return TRUE;
};

float(float xVis) VR_Stealth_Lose =
{
    stealthprofile(1);
    float r = VR_Stealth_LoseNow(xVis);
    stealthprofile(0);
    return r;
};

// ----------------------------------------------------------------------------
// Noise

void() VR_Stealth_NoiseThink;

// Every noise source calls this: `org` where it is, `range` the units at which a monster hears it in the open, `src`
// what made it (a player: his own noise; a prop; world), `kind` STL_N_*. Heard next frame (VR_Stealth_NoiseThink).
void(vector org, float range, entity src, float kind) VR_Stealth_Noise =
{
    if(range < 16 || !VR_Stealth_On() || !cvar_hget(cvarh_vr_stealth_noise))
    {
        return;
    }
    if(src.classname == "player")
    {
        if(src.flags & FL_NOTARGET)
        {
            return; // (notarget: the monsters pay him no heed, Quake's cheat; his blasts are still heard)
        }
        if(time - src.stl_loud_time > 0.5 || range > src.stl_loud)
        {
            src.stl_loud = range;
            src.stl_loud_time = time;
        }
    }
    if(cvar_hget(cvarh_vr_stealth_debug) >= 2)
    {
        bprint(sprintf("stealth: %.2f noise %g, range %.0f, at %v (%s)\n", time, kind, range, org, src.classname));
    }
    entity ev = spawn();
    ev.classname = "stl_noise";
    setorigin(ev, org);
    ev.owner = src;
    ev.dmg = range;
    ev.cnt = kind;
    ev.think = VR_Stealth_NoiseThink;
    ev.nextthink = time;
};

// The living monsters within `rad` of `org`, listed by stl_next (findradius's chain is anyone's to overwrite).
entity(vector org, float rad) VR_Stealth_Gather =
{
    entity first = world;
    entity last = world;
    for(entity e = findradius(org, rad); e; e = e.chain)
    {
        if(!(e.flags & FL_MONSTER) || e.health <= 0)
        {
            continue;
        }
        e.stl_next = world;
        if(last)
        {
            last.stl_next = e;
        }
        else
        {
            first = e;
        }
        last = e;
    }
    return first;
};

float(vector v) VR_Stealth_InLiquid =
{
    float c = pointcontents(v);
    return c == CONTENT_WATER || c == CONTENT_SLIME || c == CONTENT_LAVA;
};

// The way from the noise to a monster's ears: a clear line keeps its reach; walls (and doors) round a corner (the place
// in the monster's potentially visible set) keep vr_stealth_noise_wall of it, solid walls vr_stealth_noise_solid; a prop
// in the line vr_stealth_noise_absorb; a water surface between them (one under, the other not) as a wall.
float(vector org, vector ear, entity m, entity src) VR_Stealth_NoiseLine =
{
    float f = 1;
    if(VR_Stealth_InLiquid(org) != VR_Stealth_InLiquid(ear))
    {
        f = cvar_hget(cvarh_vr_stealth_noise_wall);
    }
    traceline(org, ear, TRUE, src);
    if(trace_fraction < 1)
    {
        return f * (pvsvisible(ear, org) ? cvar_hget(cvarh_vr_stealth_noise_wall) : cvar_hget(cvarh_vr_stealth_noise_solid));
    }
    traceline(org, ear, FALSE, src);
    if(trace_fraction < 1 && trace_ent != m && trace_ent != world && !(trace_ent.flags & (FL_MONSTER | FL_CLIENT)))
    {
        return f * cvar_hget(cvarh_vr_stealth_noise_absorb);
    }
    return f;
};

float(vector org, entity m, entity src) VR_Stealth_NoisePath =
{
    return VR_Stealth_NoiseLine(org, m.origin + m.view_ofs, m, src);
};

// A noise heard through paired slipgates (vr_stealth_gates): at each gate whose exit the noise is beyond (in front of
// the exit's face, within its reach), the monsters on the gate's side hear its image (the noise seen through the gate):
// the way through the gate's opening, the straight line to the image through its aperture (or held to the aperture's
// edge: round a corner, vr_stealth_noise_wall), each room's part of it taken by its walls as VR_Stealth_NoisePath's.
// An Alert one walks to the gate and through, then on to the noise (VR_Stealth_Via).
void(vector org, float range, entity src, float kind, float own) VR_Stealth_NoiseGates =
{
    float count = cvar_hget(cvarh_vr_stealth_gates) ? portal_ai_gateinfo(0, 0) : 0;
    for(float g = 1; g <= count; g++)
    {
        if((portal_ai_gateinfo(g, 1) & 3) != 3)
        {
            continue; // (shut, or no real exit: a one-way Quake teleport's destination isn't an opening)
        }
        vector n = portal_ai_gate(g, 1, VEC_ORIGIN);
        vector mid = portal_ai_gate(g, 0, VEC_ORIGIN);
        vector img = portal_ai_map(g, org, FALSE);
        float b = (img - mid) * n;
        if(b >= 0 || vlen(portal_ai_gate(g, 2, img) - img) >= range * 1.6)
        {
            continue; // (not beyond this gate, or out of reach of it)
        }
        for(entity e = VR_Stealth_Gather(img, range * 1.6); e; e = e.stl_next)
        {
            if(e.enemy != world || !VR_Stealth_Applies(e) || VR_Stealth_Ambusher(e))
            {
                continue;
            }
            vector ear = e.origin + e.view_ofs;
            float a = (ear - mid) * n;
            if(a <= 0)
            {
                continue; // (not on the gate's side)
            }
            vector cross = ear + (img - ear) * (a / (a - b));
            vector p = portal_ai_gate(g, 2, cross);
            float d = vlen(ear - p) + vlen(p - img);
            float reach = range * VR_Stealth_Hearing(e);
            if(reach < d)
            {
                continue;
            }
            float f = vlen(p - cross) > 1 ? cvar_hget(cvarh_vr_stealth_noise_wall) : 1;
            f = f * VR_Stealth_NoiseLine(p + n * 2, ear, e, src);
            f = f * VR_Stealth_NoiseLine(org, portal_ai_gate(g, 3, p - n * 2), world, src);
            reach = reach * f;
            if(reach < d || (e.stl_heard == time && e.stl_heard_src == src && e.stl_state == STL_ALERT &&
               reach / max(1, d) <= e.stl_alert_power))
            {
                continue; // (out of reach this way, or heard as loud the other way)
            }
            e.stl_heard = time;
            e.stl_heard_kind = kind;
            e.stl_heard_src = src;
            if(cvar_hget(cvarh_vr_stealth_debug) >= 2)
            {
                bprint(sprintf("stealth: %.2f %s hears noise %g (range %g) through gate %g at %g units (reach %.0f)\n", time,
                    e.classname, kind, range, g, d, reach));
            }
            float spotted = FALSE;
            if(own)
            {
                entity os = self;
                self = e;
                spotted = (!(src.flags & FL_NOTARGET) && !(src.items & IT_INVISIBILITY) && visible(src));
                self = os;
            }
            if(spotted)
            {
                VR_Stealth_Spot(e, src);
            }
            else
            {
                VR_Stealth_AlertVia(e, own ? portal_ai_map(g, src.origin, FALSE) : img, VR_Stealth_Via(g), kind,
                    reach / max(1, d), TRUE);
            }
        }
    }
};

void() VR_Stealth_NoiseThinkNow =
{
    entity ev = self;
    vector org = ev.origin;
    float range = ev.dmg;
    entity src = ev.owner;
    float kind = ev.cnt;
    float own = (src.classname == "player") && (kind == STL_N_STEP || kind == STL_N_MELEE || kind == STL_N_GUN);
    for(entity e = VR_Stealth_Gather(org, range * 1.6); e; e = e.stl_next)
    {
        if(e.enemy != world || !VR_Stealth_Applies(e) || VR_Stealth_Ambusher(e))
        {
            continue;
        }
        float d = vlen(e.origin + e.view_ofs - org);
        float reach = range * VR_Stealth_Hearing(e);
        if(reach < d)
        {
            continue;
        }
        reach = reach * VR_Stealth_NoisePath(org, e, src);
        if(reach < d)
        {
            continue;
        }
        e.stl_heard = time;
        e.stl_heard_kind = kind;
        e.stl_heard_src = src;
        if(cvar_hget(cvarh_vr_stealth_debug) >= 2)
        {
            bprint(sprintf("stealth: %.2f %s hears noise %g (range %g) at %g units (reach %.0f)\n", time, e.classname, kind, range, d, reach));
        }
        float spotted = FALSE;
        if(own)
        {
            self = e;
            spotted = (!(src.flags & FL_NOTARGET) && !(src.items & IT_INVISIBILITY) && visible(src));
            self = ev;
        }
        if(spotted)
        {
            VR_Stealth_Spot(e, src);
        }
        else
        {
            VR_Stealth_Alert(e, own ? src.origin : org, kind, reach / max(1, d), TRUE);
        }
    }
    VR_Stealth_NoiseGates(org, range, src, kind, own);
    remove(ev);
};

void() VR_Stealth_NoiseThink =
{
    stealthprofile(1);
    VR_Stealth_NoiseThinkNow();
    stealthprofile(0);
};

// A player's shot (W_AttackImpl, the enemy guns' rounds): heard by its weapon's loudness (a held trigger's stream:
// once a fifth of a second).
void(entity shooter, float xWeapon) VR_Stealth_Shot =
{
    float r = 1000;
    if(xWeapon == WID_SHOTGUN) { r = 1100; }
    else if(xWeapon == WID_SUPER_SHOTGUN) { r = 1300; }
    else if(xWeapon == WID_NAILGUN) { r = 800; }
    else if(xWeapon == WID_SUPER_NAILGUN) { r = 900; }
    else if(xWeapon == WID_GRENADE_LAUNCHER || xWeapon == WID_PROXIMITY_GUN) { r = 600; }
    else if(xWeapon == WID_ROCKET_LAUNCHER || xWeapon == WID_LIGHTNING || xWeapon == WID_LASER_CANNON) { r = 900; }
    else if(xWeapon == WID_CHAINSAW) { r = 700; }
    else if(xWeapon == WID_GRAPPLE) { r = 250; }
    if(time - shooter.stl_shot_time < 0.2 && time >= shooter.stl_shot_time)
    {
        return;
    }
    shooter.stl_shot_time = time;
    VR_Stealth_Noise(shooter.origin, r * cvar_hget(cvarh_vr_stealth_noise_guns), shooter, STL_N_GUN);
};

// A prop's knock as the physics sounds play it (vr_physsound.cpp; `volume` 0..1+, by its speed and weight).
void(entity prop, float volume) VR_Stealth_PropNoise =
{
    if(time < 4)
    {
        return; // (a new map's props settling)
    }
    VR_Stealth_Noise(prop.origin, cvar_hget(cvarh_vr_stealth_noise_props) * min(volume, 1.5), prop, STL_N_PROP);
};

// An explosion (T_RadiusDamage): heard by its size, 8 units a point of its damage (at least 800), times
// vr_stealth_noise_blasts.
void(vector org, float damage) VR_Stealth_Blast =
{
    VR_Stealth_Noise(org, max(800, damage * 8) * cvar_hget(cvarh_vr_stealth_noise_blasts), world, STL_N_BLAST);
};

// ----------------------------------------------------------------------------
// Shots grazing: a player's shot from `from` to `to` (it hit `hit`) passing within vr_stealth_graze of a monster:
// Alert, turning to where it came from, at any distance.
void(vector from, vector to, entity shooter, entity hit) VR_Stealth_GrazeNow =
{
    float g = cvar_hget(cvarh_vr_stealth_graze);
    if(g <= 0 || shooter.classname != "player" || !VR_Stealth_On())
    {
        return;
    }
    float len = vlen(to - from);
    if(len < 1)
    {
        return;
    }
    vector dir = (to - from) * (1 / len);
    for(entity e = VR_Stealth_Gather((from + to) * 0.5, len * 0.5 + g + 64); e; e = e.stl_next)
    {
        if(e == hit || e.enemy != world)
        {
            continue;
        }
        vector c = (e.absmin + e.absmax) * 0.5;
        float t = (c - from) * dir;
        if(t < 0 || t > len || vlen(c - (from + dir * t)) - e.size_x * 0.5 > g)
        {
            continue;
        }
        VR_Stealth_Alert(e, from, STL_A_GRAZE, 2, TRUE);
    }
};

void(vector from, vector to, entity shooter, entity hit) VR_Stealth_Graze =
{
    stealthprofile(1);
    VR_Stealth_GrazeNow(from, to, shooter, hit);
    stealthprofile(0);
};

// A launched projectile's straight line to the first wall (nails, rockets; at launch).
void(entity mis) VR_Stealth_GrazeMissile =
{
    if(mis.owner.classname != "player" || mis.velocity == '0 0 0' || cvar_hget(cvarh_vr_stealth_graze) <= 0 || !VR_Stealth_On())
    {
        return;
    }
    traceline(mis.origin, mis.origin + normalize(mis.velocity) * 4096, FALSE, mis);
    VR_Stealth_Graze(mis.origin, trace_endpos, mis.owner, trace_ent);
};

// ----------------------------------------------------------------------------
// Damage and death

// T_Damage's sneak attack: a player's damage to a monster that has noticed nothing (Idle).
float(entity targ, entity attacker) VR_Stealth_SneakMult =
{
    if(!(attacker.flags & FL_CLIENT) || !(targ.flags & FL_MONSTER) || targ.health <= 0 || targ.enemy != world ||
       targ.stl_state == STL_ALERT || !VR_Stealth_Applies(targ))
    {
        return 1;
    }
    return max(0, cvar_hget(cvarh_vr_stealth_sneak));
};

// Killed: its body may be noticed; its marker goes.
void(entity m) VR_Stealth_Died =
{
    if(!(m.flags & FL_MONSTER))
    {
        return;
    }
    m.stl_notice = m.stl_notice | STL_NOTICE_BODY;
    m.stl_state = STL_IDLE;
    if(m.stl_marker != world && m.stl_marker.classname == "stl_marker")
    {
        remove(m.stl_marker);
    }
    m.stl_marker = world;
};

// ----------------------------------------------------------------------------
// The player (PlayerPostThink, self)

float(vector xMin, vector xMax, entity e, float xGrow) VR_Stealth_BoxNear =
{
    return xMin_x <= e.absmax_x + xGrow && xMax_x >= e.absmin_x - xGrow && xMin_y <= e.absmax_y + xGrow &&
        xMax_y >= e.absmin_y - xGrow && xMin_z <= e.absmax_z + xGrow && xMax_z >= e.absmin_z - xGrow;
};

// Touching a monster (his body's box, a hand, a prop he holds): it spots him.
void() VR_Stealth_Contact =
{
    entity h1 = self.mainhand_held;
    entity h2 = self.offhand_held;
    for(entity e = VR_Stealth_Gather(self.origin, 160); e; e = e.stl_next)
    {
        if(e.enemy != world || !VR_Stealth_Applies(e))
        {
            continue;
        }
        float touch = VR_Stealth_BoxNear(self.absmin, self.absmax, e, 4) ||
            (self.handpos != '0 0 0' && VR_Stealth_BoxNear(self.handpos, self.handpos, e, 4)) ||
            (self.offhandpos != '0 0 0' && VR_Stealth_BoxNear(self.offhandpos, self.offhandpos, e, 4)) ||
            (h1 != world && VR_Stealth_BoxNear(h1.absmin, h1.absmax, e, 2)) ||
            (h2 != world && VR_Stealth_BoxNear(h2.absmin, h2.absmax, e, 2));
        if(touch)
        {
            VR_Stealth_Log(e, "touched");
            VR_Stealth_Spot(e, self);
        }
    }
};

void() VR_Stealth_PlayerFrameNow =
{
    if(!VR_Stealth_On() || self.health <= 0)
    {
        self.stl_hud = -1;
        return;
    }
    if(self.headpos != '0 0 0')
    {
        float eye = self.headpos_z - self.absmin_z;
        self.stl_stand_eye = eye > self.stl_stand_eye ? eye : max(eye, self.stl_stand_eye - frametime * 0.5);
    }
    if(VRHasBit(self.vrbits0, QVR_VRBITS0_OFFHAND_CLIMBING) || VRHasBit(self.vrbits0, QVR_VRBITS0_MAINHAND_CLIMBING))
    {
        self.stl_climb_time = time;
    }
    vector v = self.velocity;
    v_z = 0;
    if((self.flags & FL_ONGROUND) && vlen(v) > cvar_hget(cvarh_vr_stealth_run_speed) && time - self.stl_climb_time > 0.5 &&
       time >= self.stl_step_time && !(self.flags & FL_NOTARGET))
    {
        self.stl_step_time = time + 0.25;
        VR_Stealth_Noise(self.origin, cvar_hget(cvarh_vr_stealth_noise_run), self, STL_N_STEP);
    }
    if(cvar_hget(cvarh_vr_stealth_contact))
    {
        VR_Stealth_Contact();
    }
    self.stl_vis = VR_Stealth_LightShare(VR_Stealth_LightOf(self)) * VR_Stealth_CrouchShare(self);
    float ago = time - self.stl_loud_time;
    float loud = (ago >= 0 && ago < 1.5) ? min(1, self.stl_loud / 1200) * (1 - ago / 1.5) : 0;
    self.stl_hud = rint(self.stl_vis * 100) + 1000 * rint(loud * 100);
};

void() VR_Stealth_PlayerFrame =
{
    stealthprofile(1);
    VR_Stealth_PlayerFrameNow();
    stealthprofile(0);
};

// ----------------------------------------------------------------------------
// StartFrame: the umbrella switched off: every monster back to Quake's AI at once.
void() VR_Stealth_Frame =
{
    stealthprofile(-1);
    float on = VR_Stealth_On();
    if(!on && stl_was_on)
    {
        for(entity e = findflags(world, flags, FL_MONSTER); e; e = findflags(e, flags, FL_MONSTER))
        {
            VR_Stealth_Reset(e);
        }
    }
    stl_was_on = on;
    entity last = world;
    stl_players = world;
    for(entity p = find(world, classname, "player"); p; p = find(p, classname, "player"))
    {
        p.stl_pnext = world;
        if(last)
        {
            last.stl_pnext = p;
        }
        else
        {
            stl_players = p;
        }
        last = p;
    }
    VR_Stealth_TestFrame();
};

/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

// TODO VR: this is manually updated as fteqcc does not generate a progdefs
struct globalvars_t
{
    int pad[28];
    int self;
    int other;
    int world;
    float time;
    float frametime;
    float force_retouch;
    string_t mapname;
    float deathmatch;
    float coop;
    float teamplay;
    float serverflags;
    float total_secrets;
    float total_monsters;
    float found_secrets;
    float killed_monsters;
    float parm1;
    float parm2;
    float parm3;
    float parm4;
    float parm5;
    float parm6;
    float parm7;
    float parm8;
    float parm9;
    float parm10;
    float parm11;
    float parm12;
    float parm13;
    float parm14;
    float parm15;
    float parm16;
    glm::vec3 v_forward;
    glm::vec3 v_up;
    glm::vec3 v_right;
    float trace_allsolid;
    float trace_startsolid;
    float trace_fraction;
    glm::vec3 trace_endpos;
    glm::vec3 trace_plane_normal;
    float trace_plane_dist;
    int trace_ent;
    float trace_inopen;
    float trace_inwater;
    int msg_entity;
    func_t main;
    func_t StartFrame;
    func_t PlayerPreThink;
    func_t PlayerPostThink;
    func_t ClientKill;
    func_t ClientConnect;
    func_t PutClientInServer;
    func_t ClientDisconnect;
    func_t SetNewParms;
    func_t SetChangeParms;
};

struct entvars_t
{
    float modelindex;
    glm::vec3 absmin;
    glm::vec3 absmax;
    float ltime;
    float movetype;
    float solid;
    glm::vec3 origin;
    glm::vec3 oldorigin;
    glm::vec3 velocity;
    glm::vec3 angles;
    glm::vec3 avelocity;
    glm::vec3 punchangle;
    string_t classname;
    string_t model;
    float frame;
    float skin;
    float effects;
    glm::vec3 mins;
    glm::vec3 maxs;
    glm::vec3 size;
    func_t touch;
    func_t handtouch;
    func_t use;
    func_t think;
    func_t think2;
    func_t blocked;
    float nextthink;
    float nextthink2;
    int groundentity;
    float health;
    float frags;
    float weapon;
    string_t weaponmodel;
    float weaponframe;
    float weapon2;
    string_t weaponmodel2;
    float weaponframe2;
    float prevweapon;
    float prevweapon2;
    float holsterweapon0;
    float holsterweapon1;
    float holsterweapon2;
    float holsterweapon3;
    float holsterweapon4;
    float holsterweapon5;
    string_t holsterweaponmodel0;
    string_t holsterweaponmodel1;
    string_t holsterweaponmodel2;
    string_t holsterweaponmodel3;
    string_t holsterweaponmodel4;
    string_t holsterweaponmodel5;
    float currentammo;
    float currentammo2;
    float ammocounter;
    float ammocounter2;
    float ammo_shells;
    float ammo_nails;
    float ammo_rockets;
    float ammo_cells;
    float items;
    float takedamage;
    int chain;
    float deadflag;
    glm::vec3 view_ofs;
    float button0;
    float button1;
    float button2;
    float button3;
    float impulse;
    float fixangle;
    glm::vec3 v_angle;
    float idealpitch;
    string_t netname;
    int enemy;
    float flags;
    float colormap;
    float team;
    float max_health;
    float teleport_time;
    float armortype;
    float armorvalue;
    float waterlevel;
    float watertype;
    float ideal_yaw;
    float yaw_speed;
    int aiment;
    int goalentity;
    float spawnflags;
    string_t target;
    string_t targetname;
    float dmg_take;
    float dmg_save;
    int dmg_inflictor;
    int owner;
    glm::vec3 movedir;
    string_t message;
    float sounds;
    string_t noise;
    string_t noise1;
    string_t noise2;
    string_t noise3;
    glm::vec3 handpos;
    glm::vec3 handrot;
    glm::vec3 handvel;
    float handvelmag;
    glm::vec3 handavel;
    glm::vec3 v_viewangle;
    glm::vec3 offhandpos;
    glm::vec3 offhandrot;
    glm::vec3 offhandvel;
    float offhandvelmag;
    glm::vec3 offhandavel;
    float touchinghand;
    glm::vec3 muzzlepos;
    glm::vec3 offmuzzlepos;
    float teleporting;
    glm::vec3 teleport_target;
    float offhand_grabbing;
    float mainhand_grabbing;
    float offhand_hotspot;
    float mainhand_hotspot;
    float throwhit;
    float throwstabilize;
    float throwstabilizedim;
    float handtouch_hand;
    int handtouch_ent;
    glm::vec3 roomscalemove;
};

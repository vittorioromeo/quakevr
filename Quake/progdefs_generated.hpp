
/* File generated by FTEQCC, relevent for engine modding only, the generated crc must be the same as your engine expects. */

typedef struct globalvars_s
{	int pad;
	int ofs_return[3];
	int ofs_parm0[3];
	int ofs_parm1[3];
	int ofs_parm2[3];
	int ofs_parm3[3];
	int ofs_parm4[3];
	int ofs_parm5[3];
	int ofs_parm6[3];
	int ofs_parm7[3];
	int	self;
	int	other;
	int	world;
	float	time;
	float	frametime;
	float	force_retouch;
	string_t	mapname;
	float	deathmatch;
	float	coop;
	float	teamplay;
	float	serverflags;
	float	total_secrets;
	float	total_monsters;
	float	found_secrets;
	float	killed_monsters;
	float	parm1;
	float	parm2;
	float	parm3;
	float	parm4;
	float	parm5;
	float	parm6;
	float	parm7;
	float	parm8;
	float	parm9;
	float	parm10;
	float	parm11;
	float	parm12;
	float	parm13;
	float	parm14;
	float	parm15;
	float	parm16;
	float	parm17;
	float	parm18;
	float	parm19;
	float	parm20;
	float	parm21;
	float	parm22;
	float	parm23;
	float	parm24;
	float	parm25;
	float	parm26;
	float	parm27;
	float	parm28;
	float	parm29;
	float	parm30;
	float	parm31;
	float	parm32;
	vec3_t	v_forward;
	vec3_t	v_up;
	vec3_t	v_right;
	float	trace_allsolid;
	float	trace_startsolid;
	float	trace_fraction;
	vec3_t	trace_endpos;
	vec3_t	trace_plane_normal;
	float	trace_plane_dist;
	int	trace_ent;
	float	trace_inopen;
	float	trace_inwater;
	int	msg_entity;
	func_t	main;
	func_t	StartFrame;
	func_t	PlayerPreThink;
	func_t	PlayerPostThink;
	func_t	ClientKill;
	func_t	ClientConnect;
	func_t	PutClientInServer;
	func_t	ClientDisconnect;
	func_t	SetNewParms;
	func_t	SetChangeParms;
} globalvars_t;

typedef struct entvars_s
{
	float	modelindex;
	vec3_t	absmin;
	vec3_t	absmax;
	float	ltime;
	float	movetype;
	float	solid;
	vec3_t	origin;
	vec3_t	oldorigin;
	vec3_t	velocity;
	vec3_t	angles;
	vec3_t	avelocity;
	vec3_t	punchangle;
	string_t	classname;
	string_t	model;
	float	frame;
	float	skin;
	float	effects;
	vec3_t	mins;
	vec3_t	maxs;
	vec3_t	size;
	func_t	touch;
	func_t	use;
	func_t	think;
	func_t	blocked;
	float	nextthink;
	int	groundentity;
	float	health;
	float	frags;
	float	weapon;
	string_t	weaponmodel;
	float	weaponframe;
	float	currentammo;
	float	ammocounter;
	float	ammo_shells;
	float	ammo_nails;
	float	ammo_rockets;
	float	ammo_cells;
	float	items;
	float	takedamage;
	int	chain;
	float	deadflag;
	vec3_t	view_ofs;
	float	button0;
	float	button1;
	float	button2;
	float	impulse;
	float	fixangle;
	vec3_t	v_angle;
	float	idealpitch;
	string_t	netname;
	int	enemy;
	float	flags;
	float	colormap;
	float	team;
	float	max_health;
	float	teleport_time;
	float	armortype;
	float	armorvalue;
	float	waterlevel;
	float	watertype;
	float	lastwatertime;
	float	ideal_yaw;
	float	yaw_speed;
	int	aiment;
	int	goalentity;
	float	spawnflags;
	string_t	target;
	string_t	targetname;
	float	dmg_take;
	float	dmg_save;
	int	dmg_inflictor;
	int	owner;
	vec3_t	movedir;
	string_t	message;
	float	sounds;
	string_t	noise;
	string_t	noise1;
	string_t	noise2;
	string_t	noise3;
	vec3_t	v_viewangle;
	vec3_t	scale;
	vec3_t	scale_origin;
	float	vr_itemId;
	func_t	handtouch;
	func_t	vr_wpntouch;
	func_t	think2;
	float	nextthink2;
	float	weaponflags;
	float	weapon2;
	string_t	weaponmodel2;
	float	weaponframe2;
	float	weaponflags2;
	float	holsterweapon0;
	float	holsterweapon1;
	float	holsterweapon2;
	float	holsterweapon3;
	float	holsterweapon4;
	float	holsterweapon5;
	string_t	holsterweaponmodel0;
	string_t	holsterweaponmodel1;
	string_t	holsterweaponmodel2;
	string_t	holsterweaponmodel3;
	string_t	holsterweaponmodel4;
	string_t	holsterweaponmodel5;
	float	holsterweaponflags0;
	float	holsterweaponflags1;
	float	holsterweaponflags2;
	float	holsterweaponflags3;
	float	holsterweaponflags4;
	float	holsterweaponflags5;
	float	currentammo2;
	float	ammocounter2;
	float	button3;
	vec3_t	handpos;
	vec3_t	handrot;
	vec3_t	handvel;
	vec3_t	handthrowvel;
	float	handvelmag;
	vec3_t	handavel;
	vec3_t	offhandpos;
	vec3_t	offhandrot;
	vec3_t	offhandvel;
	vec3_t	offhandthrowvel;
	float	offhandvelmag;
	vec3_t	offhandavel;
	vec3_t	muzzlepos;
	vec3_t	offmuzzlepos;
	float	teleporting;
	vec3_t	teleport_target;
	vec3_t	roomscalemove;
	float	touchinghand;
	float	handtouch_hand;
	int	handtouch_ent;
	float	mainhand_grabbing;
	float	mainhand_prevgrabbing;
	float	offhand_grabbing;
	float	offhand_prevgrabbing;
	float	offhand_forcegrabbing;
	float	mainhand_forcegrabbing;
	float	offhand_hotspot;
	float	mainhand_hotspot;
	float	throwhit;
	float	throwstabilize;
	float	throwstabilizedim;
	float	offhand_attack_finished;
	float	mainhand_melee_attack_finished;
	float	in_melee;
	float	melee_hit_sound_played;
	float	offhand_melee_attack_finished;
	float	offhand_in_melee;
	float	offhand_melee_hit_sound_played;
	func_t	thinkArgFn;
	func_t	think2ArgFn;
	float	thinkArg;
	float	think2Arg;
} entvars_t;

#define PROGHEADER_CRC 34720

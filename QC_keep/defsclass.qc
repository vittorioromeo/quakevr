.float        classtype;            // Class type - used for quick reference
.float        classgroup;            // Class group - used for quick reference
.float        classmove;            // 1=Walk, 2=Fly, 4=Swim
.float        classproj;            // Projectile used by entity class

//======================================================================
// Unique Class Groups for quicker searches (mostly items)
// Previous system used classname (slow string search)
//======================================================================
float CG_PLAYERS = 50;            // All player/clients
float CG_HEALTH = 100;            // All health pack types (15/25/+100)
float CG_AMMOITEM = 200;        // All ammo items/packs
float CG_PROJALL = 205;            // All monster projectiles
float CG_PROJSHELLS = 210;        // To help with combining shell damage
float CG_PROJNAILS = 220;        // Testing ammo resistance in t_damage
float CG_PROJROCKETS = 230;
float CG_PROJCELLS = 240;
float CG_MINIONEGG = 245;        // Special minion eggs
float CG_WEAPON = 250;            // All weapon items
float CG_ARMOR = 275;            // All armor types
float CG_KEY = 300;                // All keys (gold/silver/custom)
float CG_RUNE = 310;            // All runes 1-4
float CG_ARTIFACT = 350;        // All powerups
float CG_TRAPS = 400;            // Endless supply of traps!
float CG_MISCENT = 410;            // All misc entity types
float CG_TEMPENT = 420;            // All temporary types (gibs, smoke, sparks)
float CG_BREAKABLE = 430;        // All breakable types
float CG_FUNCMOVER = 450;        // All FUNC movers
float CG_MONSTERS = 500;        // Default

//----------------------------------------------------------------------
// Useful for infighting grouping checks
//----------------------------------------------------------------------
float CG_ARMY = 600;            // (4) Shotgun, Rocket Launcher and Plasma
float CG_ENFORCER = 610;        // (5) Standard, Grenade and Flame, Pyro, Centurion
float CG_DEMON = 615;            // (3) Demon, Drole, Minotaur, Boglord, Lichfiend, Hephaestus
float CG_DOG = 620;                // (1) Dog
float CG_FISH = 625;            // (2) Fish
float CG_ROBOT = 630;            // (2) Jim, Laser/Rocket, Seeker, Floyd
float CG_KNIGHT = 635;            // (3) Knight, Death Guard and Crossbow
float CG_DKNIGHT = 637;            // (2) Hell and Death Knight
float CG_DCAPTAIN = 640;        // (2) Fury, Sergeant Captains
float CG_STONE = 645;            // (4) Boss Eidolon, Statues and Golem
float CG_OGRE = 650;            // (4) Vanilla and Hunter (Chainsaw, Mace, Hammer), Freddie
float CG_SPIDER = 660;            // (4) Shalrath, Vorling, Wraith, Spider, Snakeman
float CG_SHAM = 666;            // (1) Shambler
float CG_SPAWN = 670;            // (2) Blue/Green, Boil
float CG_WIZARD = 680;            // (6) Wizard, Gargoyle, Gaunt, Skull Wizard, Skull, Nour
float CG_ZOMBIE = 685;            // (3) Zombie, Knight, Mummy
float CG_XMAS = 686;            // (4) Elf, Raindeer, Snowman, Santa
float CG_BOSS = 690;            // (3) Cthon, Old one, Anaconda
float CG_DRAGON = 695;            // (9) Rogue Dragon, Drake Dragons, Drake Dragon King

//======================================================================
// Unique Class Types for quicker entity searches
// Previous system used classname (slow string search)
//======================================================================
float CT_WORLD = 1;                // World entity
float CT_PLAYER = 7;            // Player (007)

float CT_SPAWNSP = 10;            // Spawn location - Single Player
float CT_SPAWNCOOP = 11;        // Spawn location - Coop
float CT_SPAWNDM = 12;            // Spawn location - Deathmatch
float CT_CONTROL = 15;            // Controller entity
float CT_CAMERA = 16;            // Camera entity (info_intermission)

float CT_LIGHT = 20;            // Light entity
float CT_SOUNDEMITTER = 22;        // Sound emitter
float CT_DEVMARKER = 24;        // Developer model marker
float CT_PATHCORNER = 26;        // Standard path nodes

float CT_ENEMYTARGET = 30;        // Target enemy for flying enemies
float CT_ATTACHMENT = 32;        // Attachment for entities
float CT_ESTATE = 34;            // Entity state modifier
float CT_MAPVAR = 35;            // Map Variable logic

float CT_PARTICLE = 40;            // A particle
float CT_PARTICLEEMIT = 41;        // A particle emitter
float CT_PARTICLECONT = 42;        // Particle controller (setup particle banks)
float CT_PARTICLETEMP = 45;        // Particle emitter template

float CT_TEMPGIB = 50;            // Temporary entity (gib)
float CT_TEMPSPARK = 52;        // Temporary entity (spark)
float CT_TEMPSTREAM = 54;        // Temporary entity (stream)
float CT_REFLECTLIGHT = 55;        // Reflection entity (Lightning)
float CT_REFLECTPLASMA = 56;    // Reflection entity (Plasma)

float CT_STORYCONTROL = 60;        // Controller for story system
float CT_STORYBOOK = 62;        // Story book for the player
float CT_STORYCHAPTER = 64;        // Story chapter for story system

float CT_TARGETDUMMY = 90;        // Misc target dummy
float CT_TARGETNUMBER = 95;        // Misc number drawn ingame

//----------------------------------------------------------------------
// Class Types for all monsters 
//----------------------------------------------------------------------
float CT_MONARMY = 100;            // Army Soldier
float CT_MONARMYAXE = 101;        // Axe Grunt
float CT_MONNEMESANT = 102;        // Zerstorer Nemesant plus Drake additions
float CT_MONARCHER = 103;        // Hexen2 Archer from Drake
float CT_MONVOMITUS = 104;        // Vomitus from Qtest (Drake version)
float CT_MONRYU = 105;            // Red Dragon from Drake
float CT_MONWYRM = 106;            // Green Dragon from Drake
float CT_MONSHADE = 107;        // Nightshade Dragon from Drake
float CT_MONBARON = 108;        // Baron from Drake/SOE
float CT_MONSUPERGRUNT = 109;        // Super Grunt from Kinn
float CT_MONENFORCER = 110;        // Enforcer
float CT_MONSENTINEL = 111;        // Sentinel from Quoth 2
float CT_MONWILLYSPIDER = 112;        // Willy the Spider
float CT_MONSNAKEMAN = 113;        // Snakeman
float CT_MONFLOYD = 114;        // Floyd robot
float CT_MONDEMON = 115;        // 2 clawed fiend
float CT_MONDOG = 120;            // 4 legged friend
float CT_MONFISH = 125;            // Fish fish fishy
float CT_MONHELLK = 130;        // Strong spikey Knights
float CT_MONKNIGHT = 140;        // Small metal Knights
float CT_MONOGRE = 150;            // Chainsaw wielding manics
float CT_MONOGREFISH = 155;        // Madfox fishing ogre
float CT_MONSHAL = 160;            // Shalrath / Vore
float CT_MONSHAM = 165;            // Shambler
float CT_MONBOGLORD = 166;        // Lord of the Bog! (Big shammy)
float CT_MONSPAWN = 170;        // Bouncing annoying shit!
float CT_MONWIZARD = 180;        // Wizard / Scrags
float CT_CACHEWIZARD = 181;        // Special entity for precache of assets
float CT_MINIONWIZARD = 182;    // Minion Spawning version for Nour Boss
float CT_MONZOMBIE = 185;        // Rotten flesh
float CT_MONZOMBIEK = 186;        // Undead Knights
float CT_MONCYBERDEMON = 187;        // Doom Cyber Demon
float CT_MONHELLHOUND = 188;        // Hell Hound
float CT_MONANACONDA = 189;        // Anaconda
float CT_MONIDCHTHON = 190;        // ID Software Chthon
float CT_MONXXCHTHON = 191;        // Mapper Custom Chthon
float CT_MONFIRETOP = 192;        // Firetop Mountain Chthon
float CT_MONIDSHUB = 195;        // ID Software Shub-Niggurath
float CT_MONXXSHUB = 196;        // Mapper Custom Shub-Niggurath
float CT_MONNOUR = 197;            // Nour Boss
float CT_MONVIRTUS = 198;        // Virtus worm boss
// New (death) Knight series (sock/necros)
float CT_MONDGUARD = 200;        // HD Knight (Guard)
float CT_MONDCROSS = 205;        // HD Knight (Crossbow)
float CT_MONDFURY = 210;        // HD Knight (Fury)
float CT_MONDBLOOD = 215;        // HD Hell Knight (Blood)
float CT_MONDKNIGHT = 220;        // HD Hell Knight (Death)
float CT_MONDSERG = 225;        // HD Hell Knight (Sergeant)
float CT_MONCOMM = 230;            // HD Hell Knight (Commander)

// New (hunter) Ogre series (sock/necros)
float CT_MONHOGRE = 250;        // HD Ogre (Chainsaw)
float CT_MONHOGREMAC = 255;        // HD Ogre (Mace)
float CT_MONHOGREHAM = 260;        // HD Ogre (Hammer)

// New base monster series (Inspired by lunaran/Quoth)
float CT_MONARMYROCKET = 270;        // Army Soldier (Rocket)
float CT_MONARMYGRENADE = 272;        // Army Soldier (Grenade)
float CT_MONARMYPLASMA = 275;        // Army Soldier (Plasma)
float CT_MONDEFENDER = 280;        // Enforcer SSG/GL (Defender)
float CT_MONELIMATOR = 285;        // Enforcer Plasma (Eliminator)

// Hexen 2 series (Raven Software)
float CT_MONSPIDER = 300;        // Spiders (brown/green versions)
float CT_CACHESPIDER = 302;        // Special entity for precache of assets
float CT_MINIONSPIDER = 304;        // Minion Spawning version from wraith
float CT_MONSCORPION = 310;        // Deadly Scorpion (HL Black Crab!)
float CT_CACHESCORPION = 312;        // Special entity for precache of assets
float CT_MINIONSCORPION = 314;        // Minion Spawning version from wraith
float CT_MONLOSTSOUL = 315;        // The Original Doom flying lost soul
float CT_CACHELOSTSOUL = 317;        // Special entity for precache of assets
float CT_MINIONLOSTSOUL = 319;        // Minion Spawning version from skullwizard
float CT_MONSKULLW = 320;        // Creepy Skull Wizard (Teleport)
float CT_MONGARGOYLE = 325;        // Flying Gargoyle (Flying fireball)
float CT_MONGARGOYLE_MINI = 326;    // Flying Impling
float CT_CACHEGARGOYLE = 327;        // Special entity for precache of assets
float CT_MONGARGOYLE_MEGA = 328;    // Flying Imp Lord
float CT_MINIONGARGOYLE = 329;        // Minion Spawning version from minotaur
float CT_MONGOLEM = 330;        // Giant stone Golem (ground slam)
float CT_MONICEGOLEM = 335;        // Giant Ice Golem (shards+ground slam)
float CT_MONICEGOLEMWAVE = 337;        // Wave targets for boss Ice Golem
float CT_MONMINOTAUR = 340;        // Large tank like bull (magic/charge)
float CT_MONEIDOLON = 345;        // Large demon boss Eidolon

float CT_MONEIDOLONWAVE = 347;        // Wave targets for boss Eidolon


float CT_MONWRAITH = 350;        // Undead wraith (Burning debuff)
// Twisted Xmas MOD (Twisted Matrix)
float CT_MONELF = 351;            // Little melee (knight)
float CT_MONRAINDEER = 352;        // Rocket wielding stag (ogre)
float CT_MONSNOWMAN = 353;        // Quick firing turret
float CT_MONSANTA = 354;        // Good/bad santa (boss)
float CT_MONMUMMY = 355;        // Undead mummy (Poison debuff)
float CT_MONMORPH = 356;        // Guardian
float CT_MONBOIL = 360;            // Slow moving zombie bomb
float CT_MONLAVAMAN = 361;        // Hephaestus from Rogue
float CT_MONLICHFIEND = 362;        // Headless fiend spawning boss
float CT_MONSEEKER = 364;        // Robot replacement for Shambler
float CT_MONEEL = 365;            // Sparky eel (electric projectile) from Rogue plus AD plasma shot
float CT_MONINVISSWORD = 366;        // Invisible swordsman from Rogue
float CT_MONGREMLIN = 367;        // Gremlin from Hipnotic

// Quoth MOD (Kell/Necros/Preach)
float CT_MONVORELING = 370;        // Little baby vorelings! ahh so cute!
float CT_CACHEVORELING = 372;        // Special entity for precache of assets
float CT_MINIONVORELING = 374;        // Minion Spawning version from shalrath
float CT_MONSWAMPLING = 375;        // Tiny swamplings! ahh so cute!
float CT_MONDROLE = 380;        // Fast moving tentacle tank with rockets
float CT_MONFREDDIE = 381;        // Slow moving mechanical ogre mutant
float CT_MONGAUNT = 382;        // Flying harpy thing with plasma bolts!
float CT_MONJIM = 385;            // Jim the flying robot of doom!
float CT_MONJIMROCKET = 386;        // Jim has a rocket launcher!
float CT_MONGUG = 387;                // Gug!
float CT_MONPOLYP = 388;            // Flying Polyp
float CT_MONVERMIS = 389;            // Vermis

float CT_MONPYRO = 390;            // Enforcer Flame (Pyro)
float CT_MONENF_ULTRA = 391;            // Ultra Enforcer
float CT_MONFUMIGATOR = 392;        // Poison version of the Pyro
float CT_MONENF_MEGA = 393;            // Mega Enforcer
float CT_MONCENTURION = 395;        // Enforcer hoverboard (Plasma)

//----------------------------------------------------------------------
// Class Types for all projectiles in game (there is a lot!)
//----------------------------------------------------------------------
float CT_PROJ_TRACE = 400;        // Tracer shot (true aim)
float CT_PROJ_SMARTBALL = 401;        // Nemesant bouncing fire ball
float CT_PROJ_TRACEPART = 402;    // Tracer shot (extra particles)
float CT_PROJ_SHELLC = 405;        // Shellcase projectile
float CT_PROJ_WILLY = 406;        // Willy the Spider projectile
float CT_PROJ_CYBER = 407;        // Cyber Demon rocket


float CT_PROJ_SG = 410;            // SG projectile
float CT_PROJ_SSG = 412;        // SSG projectile


float CT_PROJ_NG = 415;            // NG projectile
float CT_PROJ_SNG = 416;        // SNG projectile
float CT_PROJ_MONNG = 417;        // NG monster projectile (glowing red)
float CT_PROJ_REFNG = 418;        // Reflected (glowing red)
float CT_PROJ_HKN = 419;        // Hell Knight Spike
float CT_PROJ_WBONE = 420;        // Wraith Bone Bolt (nail)
float CT_PROJ_GROCK = 421;        // Golem rock debris (pebble)
float CT_PROJ_SCORP = 422;        // Scorpion Tail (nail)

float CT_PROJ_SANTA = 424;        // Santa Snowball shotgun!
float CT_PROJ_WIZ = 425;        // Wizard Spit
float CT_PROJ_NOUR1 = 426;        // Nour Spit 
float CT_PROJ_MPOISON = 427;    // Minotaur Poison Bolt (nail)
float CT_PROJ_WPOISON = 428;    // Wraith Poison Bolt (nail)
float CT_PROJ_FURY2 = 429;        // Fury Spit with grenade trail





float CT_PROJ_BOLT1 = 435;        // Crossbow Knight Bolt +hknight trail
float CT_PROJ_BOLT2 = 436;        // Poison
float CT_PROJ_NOUR3 = 437;        // Nour Lightning Bolt
float CT_PROJ_ENFHKN = 438;        // Ultra Enforcer knight spike
float CT_PROJ_LAVANG = 439;        // Lava nailgun
float CT_PROJ_LASER = 440;        // Enforcer/Traps/Bob Laser
float CT_PROJ_PLASMA = 441;        // Generic Plasma (blue laser version)
float CT_PROJ_LAVASNG = 442;        // Lava super nailgun
float CT_PROJ_GAUNT = 443;        // Gaunt Plasma bolt
float CT_PROJ_MINO = 444;        // Minotaur Plasma bolt
float CT_PROJ_WBOLT = 445;        // Wraith Hitscan Bolt 
float CT_PROJ_BLBOLT = 446;        // Boglord fast bolt
float CT_PROJ_LAVASPIK = 447;        // Lava nail

float CT_PROJ_BLBOLT2 = 449;    // Boglord fast bolt (red)


float CT_PROJ_GL = 450;            // Grenade projectile

float CT_PROJ_GLMON = 452;        // Monster grenades
float CT_PROJ_GLMONMULTI = 453;        // Monster multi-grenades
float CT_PROJ_GLMONPROX = 454;        // Monster grenades
float CT_PROJ_MEGG = 455;        // Minion Egg (grenade arc+bounce)




float CT_PROJ_FLESH = 460;        // Zombie Flesh (grenade arc)

float CT_PROJ_FLESHP = 462;        // Zombie Poison Flesh (grenade arc)
float CT_PROJ_SWAMP = 463;        // Swampling Spit (grenade arc)
float CT_PROJ_ELF = 464;        // Elf snowball (grenade arc)
float CT_PROJ_SPID = 465;        // Spider Spit (grenade arc)
float CT_PROJ_VORE = 466;        // Voreling Spit (grenade arc)
float CT_PROJ_NOUR2 = 467;        // Nour Spit bomb! (grenade arc+bounce)
float CT_PROJ_CHTHON2 = 468;    // Chthon Frenzy (grenade arc)
float CT_PROJ_SHUB2 = 469;        // Shub Frenzy (grenade arc)
float CT_PROJ_ROCKET = 470;        // Rocket projectile
float CT_PROJ_LAVA = 471;        // Chthon Lavaball
float CT_PROJ_FIRETOP = 472;    // Chthon Lavaball (less splash dmg)
float CT_PROJ_CHTHON = 473;        // Custom Chthon Lava/Slime ball
float CT_PROJ_GARG = 475;        // Gargoyle fireball
float CT_PROJ_SKULLW = 477;        // Skull Wizard Skull (rocket)
float CT_PROJ_SKULLWP = 478;    // Skull Wizard Skull (poison rocket)
float CT_PROJ_FURY1 = 480;        // Fury Knight spike ball +rocket trail
float CT_PROJ_ARMY = 482;        // Army rocket
float CT_PROJ_JIM2 = 483;        // Jim robot rocket
float CT_PROJ_SEEKER = 484;        // Seeker robot rocket
float CT_PROJ_DROLE = 485;        // Drole fireball
float CT_PROJ_BLORD = 487;        // Bog Lord Slime bomb
float CT_PROJ_BLORD2 = 488;        // Bog Lord Red Slime bomb
float CT_PROJ_EIDO1 = 490;        // Eidolon boulder
float CT_PROJ_ICEG1 = 492;        // Ice Golem ice shard
float CT_WANDSPARK = 493;        // Magic wand normal spark
float CT_WANDBIGSPARK = 494;        // Magic wand large burst
float CT_PROJ_SHAL = 495;        // Shalrath spike ball +purple trail
float CT_PROJ_SERG = 496;        // Death Sergeant spike ball +hknight trail
float CT_PROJ_SHUB1 = 497;        // Custom Shub homing spike attack
float CT_PROJ_RAINDEER = 498;    // Raindeer rocket


//----------------------------------------------------------------------
float CT_UPGRADE_AXE = 500;        // Shadow Axe
float CT_AXE = 501;            // Axe
float CT_SHOTGUN = 505;            // Shotgun
float CT_SUPER_SHOTGUN = 510;        // Super Shotgun
float CT_UPGRADE_SSG = 515;        // The Widowmaker Shotgun
float CT_NAILGUN = 520;            // Nailgun
float CT_LAVANG = 521;            // Lava nailgun
float CT_SUPER_NAILGUN = 525;        // Super Nailgun
float CT_LAVASNG = 526;            // Lava nailgun
float CT_WAND = 527;            // Magic Wand (Drake)
float CT_GRAPPLE = 528;            // Grapple (Rogue with Drake upgrades)
float CT_CHAINLIGHTGUN = 529;    // Chain Lightning Gun
float CT_GRENADE_LAUNCHER = 530;    // Grenade Launcher
float CT_PROXIMITY_GUN = 531;        // Grenade Launcher
float CT_MULTI_GRENADE = 532;        // Multi Grenade Launcher
float CT_MULTI_ROCKET = 533;        // Multi Rocket Launcher
float CT_BLASTERWEP = 534;            // Drake Laser Blaster
float CT_ROCKET_LAUNCHER = 535;        // Rocket Launcher
float CT_LIGHTNING = 540;        // Lightning Gun
float CT_UPGRADE_LG = 545;        // Plasma Gun
float CT_PLASMAGUN = 546;        // Rogue Plasma Gun
float CT_MJOLNIR = 547;            // Mjolnir's Hammer
float CT_EXTRA_WEAPON = 548;        // Extra Weapon (Crossbow)
float CT_LASER_CANNON = 549;        // Laser Cannon
float CT_ARTSUIT = 550;            // Environment suit
float CT_CIRCUIT = 551;            // Circuit Board collectible (17mm Fromitz board from Rubicon2)
float CT_CHAINSAW = 552;        // Chainsaw (Zerstorer with Drake upgrades)


float CT_ARTPENT = 555;            // Pentagram - Invulnerability
float CT_ARTINVS = 560;            // Ring of Shadows
float CT_ARTQUAD = 565;            // Quad Damage
float CT_ARTSHARP = 570;        // Sharp Shooter
float CT_ARTPIERCE = 575;        // Nail Piercer







float CT_ARTTOME = 590;            // Tome of Power
float CT_ARTPSHIELD = 591;        // Power Shield
float CT_ARTGRAVBELT = 592;        // Anti-Gravity Belt
float CT_ARTWETSUIT = 593;        // Wetsuit
float CT_ARTEMPATHY = 594;        // Empathy Shield
float CT_ARTHORNCONJ = 595;        // Horn of Conjuring
float CT_ARTTHERMSUIT = 596;    // Thermal Protection Suit
float CT_ARTTRINITY = 597;        // Trinity
float CT_ARTCROSS = 598;        // Cross of Deflection
float CT_ARTHORN = 599;            // Horn of Conjuring
float CT_SILVERKEY = 600;        // Silver key
float CT_GOLDKEY = 605;            // Gold key
float CT_CUSTOMKEY = 610;        // Custom key
float CT_CUSTOMITEM = 615;        // Custom pickup item
float CT_RUNEKEY = 620;            // End of Level rune keys 1-4
float CT_ARTBERSERK = 621;        // Berserker Helm, Drake
float CT_ARTEQUALIZER = 622;    // Equalizer Twin Shot, Drake
float CT_ARTLEVITATION = 623;    // Boots of Levitation, Drake
float CT_ARTOPTIMIZER = 624;    // Shotgun Optimizer, Drake
float CT_SIGILBANE = 625;        // Seal of Bane, Drake

float CT_SPIKESHOOTER = 650;        // Trap SPIKE shooter
float CT_GRENADESHOOTER = 652;        // Trap GRENADE shooter
float CT_ROCKETSHOOTER = 654;        // Trap ROCKET shooter
float CT_LIGHTSHOOTER = 656;        // Trap LIGHTNING shooter
float CT_GASSHOOTER = 658;        // Trap GAS shooter
float CT_EXPLO_BOX = 660;        // Exploding box
float CT_PENDULUM = 662;        // Swing Pendulum
float CT_SAWBLADE = 666;        // Saw Blade

float CT_FIREBALL = 670;        // Misc Fireball
float CT_MISCSPARK = 672;        // Misc Spark
float CT_MISCSHAKE = 676;        // Misc Shake of screen/player
float CT_MISCDRIP = 680;        // Misc Drip (water)
float CT_BUBBLE = 682;            // Bubbles
float CT_MISCMODEL = 690;        // Misc Model
float CT_MISCSMOKE = 692;        // Misc Smoke

//----------------------------------------------------------------------
float CT_AMMOPACK = 700;        // Ammo Backpacks
float CT_AMMORANDOM = 702;        // Random Ammo Backpack
float CT_AMMOSHELLS = 710;        // Different ammo types
float CT_AMMONAILS = 720;
float CT_AMMOROCKETS = 730;
float CT_AMMOCELLS = 740;
float CT_AMMOPOISON = 741;
float CT_AMMOBOLTS = 742;
float CT_AMMOLAVANAILS = 743;
float CT_AMMOMULTIROCKETS = 744;
float CT_AMMOPLASMA = 745;




float CT_HEALROT = 750;            // Different Health packs




float CT_HEALNORM = 755;




float CT_HEALMEGA = 760;

float CT_ARMOR1 = 770;            // Green armour
float CT_ARMOR2 = 775;            // Yellow armour
float CT_ARMORINV = 790;        // Red armour


//----------------------------------------------------------------------
float CT_FUNCDOOR = 800;        // Func door
float CT_FUNCDOORTRIG = 801;    // Generated touch trigger around door
float CT_FUNCSECDOOR = 805;        // Func SECRET door
float CT_FUNCBUTTON = 810;        // Func button

float CT_FUNCPSWITCH = 812;        // Func Pressure Switch
float CT_FUNCINSVOLUME = 813;    // Func to detect inside volume

float CT_FUNCBOB = 815;            // Func bobbing bmodels
float CT_FUNCTRAIN = 820;        // Func train
float CT_FUNCWALL = 830;        // Func wall
float CT_FUNCILLUSIONARY = 832;    // Func illusionary
float CT_FUNCLASER = 834;        // Func laser
float CT_FUNCSKILL = 836;        // Func_skill pillars

float CT_FUNCBREAK = 840;        // Func breakable (parent)
float CT_FUNCBREAKWALL = 841;    // Bmodel to activate after breakage
float CT_FUNCBREAKMDL = 845;    // Func breakable (model)
float CT_FUNCBREAKSPN = 846;    // Func breakable (spawner)
float CT_FUNCBREAKOBJ = 847;    // Func breakable (rubble))
float CT_FUNCPUSHABLE = 850;    // Func pushable

float CT_FUNCEPISODEGATE = 875;    // Episode gates (not used anymore)
float CT_FUNCBOSSGATE = 880;    // Final boss gate (not used anymore)
float CT_FUNCPLAT = 890;        // Old platform system
float CT_FUNCLIFT = 895;        // New platform system

//----------------------------------------------------------------------
float CT_TRIGONCE = 900;        // Once, volume

float CT_TRIGMULTI = 905;        // Multiple, volume

float CT_TRIGRELAY = 910;        // Relay, point entity no touch

float CT_TRIGDOORSTATE = 914;    // Set func_door to specific state
float CT_TRIGRELAYDELAY = 915;    // Relay, point entity no touch

float CT_TRIGCOUNT = 920;        // Count, point entity no touch

float CR_TRIGENG = 922;            // Engine, point entity no touch

float CT_TRIGRUNES = 925;        // Rune, volume
float CT_TRIGSKILLS = 927;        // Skill, volume
float CT_TRIGTELEPORT = 930;    // Trigger teleport
float CT_MISCTELEPORT = 932;    // info_teleport_destination
float CT_SKULLTELEPORT = 934;    // info_skullwiz_destination
float CT_TRIGEXPLODE = 945;        // Trigger sprite explosion
float CT_TRIGSECRET = 950;        // Secret, volume
float CT_TRIGLADDER = 955;        // Ladder, volume

float CT_TRIGFOG = 957;            // Fog, volume

float CT_TRIGHURT = 960;        // Hurt, volume

float CT_TRIGGIVEITEM = 964;    // Give items, volume
float CT_TRIGHEAL = 965;        // Heal, volume

float CT_TRIGTSOUND = 968;        // Touch sounds, volume

float CT_TRIGPUSH = 970;        // Push, volume

float CT_TRIGLEVEL = 975;        // Change level
float CT_TRIGSKYBOX = 976;        // Change/Load Skybox console command
float CT_TRIGITEMFLAG = 977;    // Change item spawnflags
float CT_TRIGCLMSG = 978;        // Message to all clients
float CT_TRIGCDTRACK = 979;        // Change CD track
float CT_TRIGMONJUMP = 980;        // Triger monster to jump forward
float CT_TRIGNOJUMP = 981;        // Trigger monster to NOT jump!
float CT_TRIGMONDROP = 982;        // Trigger monster to fall naturally

float CT_TRIGMONTURRET = 985;    // Trigger monster to turn into turret

float CT_TRIGMONMOVE = 987;        // Trigger monster movement state
float CT_TRIGMONBREAK = 990;    // Trigger monster/breakable together

float CT_TRIGMONKILL = 992;        // Trigger monster to be killed

float CT_TRIGMONATT = 996;        // Trigger monster to attack something

float CT_TRIGVOID = 999;        // Skybox garbage collector!
//----------------------------------------------------------------------
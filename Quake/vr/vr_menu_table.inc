// The VR menu's pages: their builders, the table of them (pages[], by number) and how a page is known (PageRef).
// Included at the end of vr_menu_internal.hpp, in namespace menu_detail: every menu file links pages by builder
// (pageIndex(pageConsole)), turned into the page's number while compiling.
//
// A page is its number, never its builder's address: the release link folds functions with the same code into one
// (/OPT:ICF), so builders with the same body share an address. Search's, Console's and the Map Library's builders are
// the same line of text (their pages are drawn by vr_menu_search.inc, vr_menu_console.inc and vr_menu_maps.inc), and
// when pages were found by comparing builders, the corner's Console and Map Library buttons opened Search (ROUND22.md,
// "Menu pages known by number, not by builder"). So pages[] holds builders only to call them (PageBuild: no ==), and a
// builder named in the code becomes its page's number at compile time (PageRef's consteval constructor), where two
// functions are always two.

// A page's builder in pages[]: called (pages[p].build()), never compared (no ==; compare pages by number: PageRef).
struct PageBuild
{
    PageFn fn;
    constexpr PageBuild(PageFn f) : fn(f) {}
    za::Vector<Item> operator()() const { return fn(); }
    bool operator==(const PageBuild&) const = delete;
    bool operator==(PageFn) const = delete;
};

struct Page
{
    const char* title; // over the page; also its name in vr_menu_positions (renamed: its record is ignored)
    PageBuild build;
    PageBuild home; // the page listing it in the tree (Back after menu_vr <n>; links elsewhere go back where they came from)
    int level{LevelAdvanced};
};

// Pages linked before they are defined.
[[nodiscard]] za::Vector<Item> pageBodyArms();
[[nodiscard]] za::Vector<Item> pageBodyCalibration();
[[nodiscard]] za::Vector<Item> pageBodyCrouch();
[[nodiscard]] za::Vector<Item> pageSeated();
[[nodiscard]] za::Vector<Item> pageSeatedHolsters();
[[nodiscard]] za::Vector<Item> pageWeightDamage();
[[nodiscard]] za::Vector<Item> pageAimingSettings();
[[nodiscard]] za::Vector<Item> pageWeaponDamage();
[[nodiscard]] za::Vector<Item> pageHotBlades();
[[nodiscard]] za::Vector<Item> pageFlail(); // AD support (vr_menu_ad_guns.inc)
[[nodiscard]] za::Vector<Item> pageDamage();
[[nodiscard]] za::Vector<Item> pageBloodFeedback();
[[nodiscard]] za::Vector<Item> pageFingersCollisions();
[[nodiscard]] za::Vector<Item> pageFlashlightLowGrip();
[[nodiscard]] za::Vector<Item> pageFlashlightOverheadGrip();
[[nodiscard]] za::Vector<Item> pageFlashlightMounts();
[[nodiscard]] za::Vector<Item> pageBodyDisplay();
[[nodiscard]] za::Vector<Item> pageHeadset();
[[nodiscard]] za::Vector<Item> pageDebugViews();
[[nodiscard]] za::Vector<Item> pageDebugLogging();
[[nodiscard]] za::Vector<Item> pageDebugProfiling();
[[nodiscard]] za::Vector<Item> pageDebugReports();
[[nodiscard]] za::Vector<Item> pageDebugTools();
[[nodiscard]] za::Vector<Item> pageDebugTests();
[[nodiscard]] za::Vector<Item> pageDebugAutomation();
[[nodiscard]] za::Vector<Item> pageDebugCheats();
[[nodiscard]] za::Vector<Item> pageStealth();
[[nodiscard]] za::Vector<Item> pageStealthTests();
[[nodiscard]] za::Vector<Item> pageRagdollTests();
[[nodiscard]] za::Vector<Item> pageRagtool(); // (vr_menu_ragtool.inc)
[[nodiscard]] za::Vector<Item> pageRagdolls();
[[nodiscard]] za::Vector<Item> pageSpawnWeapons();
[[nodiscard]] za::Vector<Item> pageHitbox();
[[nodiscard]] za::Vector<Item> pageMonsterHitbox();
[[nodiscard]] za::Vector<Item> pageChanged();
[[nodiscard]] za::Vector<Item> pageSearch();
[[nodiscard]] za::Vector<Item> pageConsole();
[[nodiscard]] za::Vector<Item> pageMaps();
// (Settings with their home on another page link to it: one home per setting.)
[[nodiscard]] za::Vector<Item> pageMain();
[[nodiscard]] za::Vector<Item> pageVrsHud();
[[nodiscard]] za::Vector<Item> pageVrsAppearance();
[[nodiscard]] za::Vector<Item> pageVrsRangerHead();
[[nodiscard]] za::Vector<Item> pageColours();
[[nodiscard]] za::Vector<Item> pageBulletTime();
[[nodiscard]] za::Vector<Item> pageStamina();
[[nodiscard]] za::Vector<Item> pageGibs();
[[nodiscard]] za::Vector<Item> pageGore();
[[nodiscard]] za::Vector<Item> pageLimbGore();
[[nodiscard]] za::Vector<Item> pageScreens();
[[nodiscard]] za::Vector<Item> pageDoom();
[[nodiscard]] za::Vector<Item> pageTips();
[[nodiscard]] za::Vector<Item> pageHipHolsters();
[[nodiscard]] za::Vector<Item> pageHolsterZones();
[[nodiscard]] za::Vector<Item> pageClimbing();
[[nodiscard]] za::Vector<Item> pageRecording();
[[nodiscard]] za::Vector<Item> pageReviewTake();

// The world: monsters, knights' swords, weapon drops and what you feel (the Gameplay page before the menus were
// reorganized; its damage and knockback are on Damage and Knockback, its voice notes on Debug).
[[nodiscard]] za::Vector<Item> pageEnemyWeapons();
[[nodiscard]] za::Vector<Item> pageEnemyShoves();
[[nodiscard]] za::Vector<Item> pageArmedGrunts();
[[nodiscard]] za::Vector<Item> pageArmedRepl();
[[nodiscard]] za::Vector<Item> pageArmedGrips();
[[nodiscard]] za::Vector<Item> pageArmedHolds();
[[nodiscard]] za::Vector<Item> pageKnockdowns();
[[nodiscard]] za::Vector<Item> pageKdTriggers();
[[nodiscard]] za::Vector<Item> pageKdTrigLedge();
[[nodiscard]] za::Vector<Item> pageKdTrigStairs();
[[nodiscard]] za::Vector<Item> pageKdTrigSlope();
[[nodiscard]] za::Vector<Item> pageKdTrigShotgun();
[[nodiscard]] za::Vector<Item> pageKdTrigWounded();
[[nodiscard]] za::Vector<Item> pageKdTrigBlow();
[[nodiscard]] za::Vector<Item> pageKdTrigBlast();
[[nodiscard]] za::Vector<Item> pageHoldingEnemies();
[[nodiscard]] za::Vector<Item> pageHoldingStruggle();
[[nodiscard]] za::Vector<Item> pageEnemyMelee();
[[nodiscard]] za::Vector<Item> pageSound();

[[nodiscard]] za::Vector<Item> pageMain();
[[nodiscard]] za::Vector<Item> pageSaves();
[[nodiscard]] za::Vector<Item> pageVrsHeight();
[[nodiscard]] za::Vector<Item> pageVrsHands();
[[nodiscard]] za::Vector<Item> pageVrsHolsters();
[[nodiscard]] za::Vector<Item> pageVrsAmmoPouch();
[[nodiscard]] za::Vector<Item> pageVrsBodyEquipment();
[[nodiscard]] za::Vector<Item> pageVrsItemCollection();
[[nodiscard]] za::Vector<Item> pageVrsControls();
[[nodiscard]] za::Vector<Item> pageVrsLocomotion();
[[nodiscard]] za::Vector<Item> pageVrsComfort();
[[nodiscard]] za::Vector<Item> pageVrsTeleport();
[[nodiscard]] za::Vector<Item> pageVrsTurning();
[[nodiscard]] za::Vector<Item> pageVrsFlashlight();
[[nodiscard]] za::Vector<Item> pageVrsLighting();
[[nodiscard]] za::Vector<Item> pageVrsWeapons();
[[nodiscard]] za::Vector<Item> pageVrsBulletTime();
[[nodiscard]] za::Vector<Item> pageVrsBody();
[[nodiscard]] za::Vector<Item> pageVrsHaptics();
[[nodiscard]] za::Vector<Item> pageVrsHud();
[[nodiscard]] za::Vector<Item> pageVrsSound();
[[nodiscard]] za::Vector<Item> pageVrsDisplay();
[[nodiscard]] za::Vector<Item> pageVrsScaling();
[[nodiscard]] za::Vector<Item> pageVrsGraphics();
[[nodiscard]] za::Vector<Item> pageVrsHeightHands();
[[nodiscard]] za::Vector<Item> pageVrsLocoControls();
[[nodiscard]] za::Vector<Item> pageVrsComfortTurning();
[[nodiscard]] za::Vector<Item> pageVrsWeaponsBulletTime();
[[nodiscard]] za::Vector<Item> pageVrsBodyFlashlight();
[[nodiscard]] za::Vector<Item> pageVrsHudHapticsSound();
[[nodiscard]] za::Vector<Item> pageVrsDisplayGraphics();
[[nodiscard]] za::Vector<Item> pageAdvanced();
[[nodiscard]] za::Vector<Item> pageWeaponOffsets();
[[nodiscard]] za::Vector<Item> pageWofsHand();
[[nodiscard]] za::Vector<Item> pageWofsFingers();
[[nodiscard]] za::Vector<Item> pageWofsSights();
[[nodiscard]] za::Vector<Item> pageWofsTwoHanded();
[[nodiscard]] za::Vector<Item> pageWofsStock();
[[nodiscard]] za::Vector<Item> pageWofsScreen();
[[nodiscard]] za::Vector<Item> pageWofsHolstered();
[[nodiscard]] za::Vector<Item> pageWofsEffects();
[[nodiscard]] za::Vector<Item> pageWofsFlashlight();
[[nodiscard]] za::Vector<Item> pageCombat();
[[nodiscard]] za::Vector<Item> pageMovement();
[[nodiscard]] za::Vector<Item> pageCarryingHub();
[[nodiscard]] za::Vector<Item> pageWeaponsHub();
[[nodiscard]] za::Vector<Item> pageHudHub();
[[nodiscard]] za::Vector<Item> pageLightningWater();
[[nodiscard]] za::Vector<Item> pageWeaponEffects();


// The pages, by their number (menu_vr <n>): the numbers stay as they were, new pages are added last (the menus'
// tree is in the pages' links: VR Settings > Advanced VR Options > its groups > their pages; ROUND21.md, "Menus
// reorganized"). Links find their page by its builder (pageIndex).
inline constexpr Page pages[] = {
    {"VR Settings", pageMain, nullptr, LevelStandard},                                     // 0
    {"Advanced VR Options", pageAdvanced, pageMain},                        // 1
    {"Play", pagePlay, pageAdvanced},                                       // 2
    {"World", pageGameplay, pageAdvanced},                                  // 3 (Gameplay before)
    {"Parry and Bash", pageParryBash, pageCombat},                          // 4 (Parry, Bash and Headbutt before)
    {"Melee", pageMeleeSettings, pageCombat},                               // 5
    {"Motion Recorder", pageMotionRecorder, pageAdvanced, LevelDeveloper},                  // 6
    {"Review Takes", pageReviewTakes, pageAdvanced, LevelDeveloper},                        // 7
    {"Take", pageReviewTake, pageReviewTakes, LevelDeveloper},                              // 8
    {"Gore", pageGore, pageAdvanced},                                       // 9
    {"Throwing and Physics", pageThrowing, pageCarryingHub, LevelDeveloper},                // 10
    {"Carrying", pageCarrying, pageCarryingHub},                            // 11 (Carrying and Gibs before)
    {"Force Grab", pageForceGrab, pageCarryingHub},                         // 12
    {"Grappling Hook", pageGrapple, pageMovement},                          // 13
    {"Body", pageBody, pageAdvanced},                                       // 14
    {"Body - Arms and Pauldrons", pageBodyArms, pageBody, LevelDeveloper},                  // 15
    {"Body Calibration", pageBodyCalibration, pageBody, LevelStandard},                    // 16
    {"Flashlight", pageFlashlight, pageAdvanced},                           // 17
    {"Player Calibration", pagePlayerCalibration, pageBody},                // 18
    {"Locomotion", pageLocomotionSettings, pageMovement},                   // 19
    {"Swimming", pageSwimSettings, pageMovement},                           // 20
    {"Immersion", pageImmersionSettings, pageWeaponsHub},                   // 21
    {"Hand/Gun Calibration", pageHandGunCalibration, pageWeaponsHub},       // 22
    {"Weapon Offsets", pageWeaponOffsets, pageWeaponsHub, LevelDeveloper},                  // 23
    {"Aiming", pageAimingSettings, pageWeaponsHub},                         // 24
    {"Hotspots", pageHotspotSettings, pageWeaponsHub},                      // 25
    {"Wrist Gadget", pageGadget, pageHudHub},                               // 26
    {"Screens", pageScreens, pageHudHub},                                   // 27
    {"Colours", pageColours, pageHudHub},                                   // 28
    {"Status Bar", pageHudConfiguration, pageHudHub},                       // 29
    {"Crosshair", pageCrosshairSettings, pageHudHub},                       // 30
    {"Menu", pageMenuSettings, pageHudHub},                                 // 31
    {"Graphics", pageGraphics, pageAdvanced},                               // 32
    {"Graphics - Lights", pageGraphicsLights, pageGraphics},                // 33
    {"Graphics - Shadows", pageGraphicsShadows, pageGraphics},              // 34
    {"Graphics - Surfaces", pageGraphicsSurfaces, pageGraphics},            // 35
    {"Graphics - Liquids", pageGraphicsLiquids, pageGraphics},              // 36
    {"Graphics - Post-processing", pageGraphicsPost, pageGraphics},         // 37
    {"Graphics - Models and Effects", pageGraphicsModels, pageGraphics},    // 38
    {"Particles", pageParticleSettings, pageGraphics},                      // 39
    {"Transparency", pageTransparencyOptions, pageGraphics},                // 40
    {"Held Object Offsets", pageHeldObjectOffsets, pageCarryingHub, LevelDeveloper},        // 41
    {"Weapon Weights", pageWeaponWeights, pageWeaponsHub, LevelDeveloper},                  // 42
    {"Held Object Weights", pageHeldObjectWeights, pageCarryingHub, LevelDeveloper},        // 43
    // Added with the menus reorganized (ROUND21.md).
    {"Combat", pageCombat, pageAdvanced},                                   // 44
    {"Movement", pageMovement, pageAdvanced},                               // 45
    {"Carrying and Throwing", pageCarryingHub, pageAdvanced},               // 46
    {"Weapons", pageWeaponsHub, pageAdvanced},                              // 47
    {"HUD and Menus", pageHudHub, pageAdvanced},                            // 48
    {"Body and Display", pageBodyDisplay, pageAdvanced, LevelStandard},                        // 49
    {"Headset", pageHeadset, pageAdvanced, LevelStandard},                                     // 50
    {"Damage and Knockback", pageDamage, pageCombat},                       // 51
    {"Stamina", pageStamina, pageCombat},                                   // 52
    {"Batting and Catching", pageBatting, pageCombat},                      // 53
    {"Climbing", pageClimbing, pageMovement},                               // 54
    {"Wall Torches", pageWallTorches, pageCarryingHub},                     // 55
    {"Rocks and Bricks", pageRocksBricks, pageCarryingHub},                 // 56
    {"Gibs and Corpses", pageGibs, pageCarryingHub},                        // 57
    {"Flashlight - Low Grip", pageFlashlightLowGrip, pageFlashlight, LevelDeveloper},       // 58
    {"Flashlight - Overhead Grip", pageFlashlightOverheadGrip, pageFlashlight, LevelDeveloper}, // 59
    {"Flashlight - On a Gun or Head", pageFlashlightMounts, pageFlashlight, LevelDeveloper}, // 60
    {"Fingers and Collisions", pageFingersCollisions, pageWeaponsHub, LevelDeveloper},      // 61
    {"Weight and Damage", pageWeightDamage, pageWeaponsHub},                // 62
    {"Hip Holsters", pageHipHolsters, pageWeaponsHub},                      // 63
    {"Debug", pageDebug, pageAdvanced, LevelDeveloper},                                     // 64
    {"Recording", pageRecording, pageGraphics},                             // 65 (the desktop window's view)
    // The Debug page's parts (ROUND21.md, "Debug menu; quad sound; grenade catch default; no empty-hand deflection").
    {"Debug - Views", pageDebugViews, pageDebug, LevelDeveloper},                           // 66
    {"Debug - Logging", pageDebugLogging, pageDebug, LevelDeveloper},                       // 67
    {"Debug - Profiling and Memory", pageDebugProfiling, pageDebug, LevelDeveloper},        // 68
    {"Debug - Reports", pageDebugReports, pageDebug, LevelDeveloper},                       // 69
    {"Debug - Tools", pageDebugTools, pageDebug, LevelDeveloper},                           // 70
    {"Debug - Tests", pageDebugTests, pageDebug, LevelDeveloper},                           // 71
    {"Checklist", pageChecklist, pageDebug, LevelDeveloper},                                // 72 (also the corner's button)
    {"Player Hitbox", pageHitbox, pageMovement, LevelDeveloper},                            // 73
    {"Monster Hitbox", pageMonsterHitbox, pageMovement, LevelDeveloper},                    // 74
    {"Lightning Gun in Water", pageLightningWater, pageWeaponsHub},         // 75
    {"Enemy Weapons", pageEnemyWeapons, pageCombat},                        // 76
    {"Sound", pageSound, pageAdvanced},                                         // 77
    {"Crates", pageCrates, pageCarryingHub},                                // 78
    {"Weapon Damage", pageWeaponDamage, pageCombat},                        // 79
    {"Weapon Effects", pageWeaponEffects, pageWeaponsHub},                  // 80
    {"Enemy Shoves", pageEnemyShoves, pageCombat},                          // 81
    {"Chainsaw Engine", pageChainsawEngine, pageEnemyWeapons, LevelDeveloper},              // 82
    {"Bullet Time", pageBulletTime, pageCombat},                            // 83 (vr_menu_recording.inc)
    // Weapon Offsets' parts (ROUND21.md, "Weapon Offsets split; the virtual stock's turn"): its main page links them.
    {"Weapon Offsets - Hand and Grip", pageWofsHand, pageWeaponOffsets, LevelDeveloper},           // 84
    {"Weapon Offsets - Fingers", pageWofsFingers, pageWeaponOffsets, LevelDeveloper},              // 85
    {"Weapon Offsets - Muzzle and Sights", pageWofsSights, pageWeaponOffsets, LevelDeveloper},     // 86
    {"Weapon Offsets - Two-Handed", pageWofsTwoHanded, pageWeaponOffsets, LevelDeveloper},        // 87
    {"Weapon Offsets - Virtual Stock", pageWofsStock, pageWeaponOffsets, LevelDeveloper},          // 88
    {"Weapon Offsets - Ammo Screen", pageWofsScreen, pageWeaponOffsets, LevelDeveloper},           // 89
    {"Weapon Offsets - Holstered", pageWofsHolstered, pageWeaponOffsets, LevelDeveloper},          // 90
    {"Weapon Offsets - Effects", pageWofsEffects, pageWeaponOffsets, LevelDeveloper},              // 91
    {"Weapon Offsets - Flashlight", pageWofsFlashlight, pageWeaponOffsets, LevelDeveloper},        // 92
    {"Small Gibs", pageSmallGibs, pageGore, LevelDeveloper},                                       // 93
    {"Burning", pageBurning, pageCombat},                                          // 94
    {"Graphics - Retro Textures", pageGraphicsRetro, pageGraphics},                // 95
    {"Retro Textures - World", pageRetroCategory<retro::Category::World>, pageGraphicsRetro, LevelDeveloper}, // 96
    {"Retro Textures - Brush Entities", pageRetroCategory<retro::Category::Brush>, pageGraphicsRetro, LevelDeveloper}, // 97
    {"Retro Textures - Item Pickups", pageRetroCategory<retro::Category::Items>, pageGraphicsRetro, LevelDeveloper}, // 98
    {"Retro Textures - Props and Debris", pageRetroCategory<retro::Category::Props>, pageGraphicsRetro, LevelDeveloper}, // 99
    {"Retro Textures - Gibs", pageRetroCategory<retro::Category::Gibs>, pageGraphicsRetro, LevelDeveloper}, // 100
    {"Retro Textures - Small Gibs", pageRetroCategory<retro::Category::SmallGibs>, pageGraphicsRetro, LevelDeveloper}, // 101
    {"Retro Textures - Weapons in the World", pageRetroCategory<retro::Category::Weapons>, pageGraphicsRetro, LevelDeveloper}, // 102
    {"Retro Textures - Held Weapons", pageRetroCategory<retro::Category::Held>, pageGraphicsRetro, LevelDeveloper}, // 103
    {"Retro Textures - Monsters", pageRetroCategory<retro::Category::Monsters>, pageGraphicsRetro, LevelDeveloper}, // 104
    {"Retro Textures - Your Hands", pageRetroCategory<retro::Category::Hands>, pageGraphicsRetro, LevelDeveloper}, // 105
    {"Retro Textures - Your Arms", pageRetroCategory<retro::Category::Arms>, pageGraphicsRetro, LevelDeveloper}, // 106
    {"Retro Textures - Your Torso", pageRetroCategory<retro::Category::Torso>, pageGraphicsRetro, LevelDeveloper}, // 107
    {"Retro Textures - Your Legs", pageRetroCategory<retro::Category::Legs>, pageGraphicsRetro, LevelDeveloper}, // 108
    {"Retro Textures - Your Gear", pageRetroCategory<retro::Category::Gear>, pageGraphicsRetro, LevelDeveloper}, // 109
    {"Retro Textures - Decals", pageRetroCategory<retro::Category::Decals>, pageGraphicsRetro, LevelDeveloper}, // 110
    {"Retro Textures - Particles", pageRetroCategory<retro::Category::Particles>, pageGraphicsRetro, LevelDeveloper}, // 111
    {"Retro Textures - Sprites", pageRetroCategory<retro::Category::Sprites>, pageGraphicsRetro, LevelDeveloper}, // 112
    {"Retro Textures - Other Models", pageRetroCategory<retro::Category::Other>, pageGraphicsRetro, LevelDeveloper}, // 113
    {"Retro Textures - Override", pageRetroOverride, pageGraphicsRetro, LevelDeveloper}, // 114
    {"Retro Textures - All Categories", pageRetroAll, pageGraphicsRetro, LevelDeveloper}, // 115
    {"Graphics - Retro Lighting", pageGraphicsRetroLight, pageGraphics},          // 116
    {"Gibs and Corpses - Ragdolls", pageRagdolls, pageGibs, LevelDeveloper},                       // 117
    {"Ragdolls - Grunt", pageRagdollGrunt, pageRagdolls, LevelDeveloper},                          // 118
    {"Ragdolls - Knight", pageRagdollKnight, pageRagdolls, LevelDeveloper},                        // 119
    {"Gibs and Corpses - Corpse Damage and Health", pageCorpseDamage, pageGibs, LevelDeveloper},   // 120
    {"Ragdolls - Ogre", pageRagdollOgre, pageRagdolls, LevelDeveloper},                            // 121
    {"Ragdolls - Enforcer", pageRagdollEnforcer, pageRagdolls, LevelDeveloper},                    // 122
    {"Ragdolls - Death Knight", pageRagdollDeathKnight, pageRagdolls, LevelDeveloper},             // 123
    {"Ragdolls - Rottweiler", pageRagdollRottweiler, pageRagdolls, LevelDeveloper},                // 124
    {"Ragdolls - Scrag", pageRagdollScrag, pageRagdolls, LevelDeveloper},                          // 125
    {"Gore - Decapitation", pageDecapitation, pageGore},                           // 126
    {"Ragdolls - Zombie", pageRagdollZombie, pageRagdolls, LevelDeveloper},                        // 127
    {"Ragdolls - Fiend", pageRagdollFiend, pageRagdolls, LevelDeveloper},                         // 128
    {"Ragdolls - Shambler", pageRagdollShambler, pageRagdolls, LevelDeveloper},                   // 129
    {"Ragdolls - Gremlin", pageRagdollGremlin, pageRagdolls, LevelDeveloper},                     // 130
    {"Ragdolls - Mummy", pageRagdollMummy, pageRagdolls, LevelDeveloper},                         // 131
    {"Changed Settings", pageChanged, pageAdvanced, LevelStandard},                                    // 132 (archive/MENU_REVIEW.md)
    {"Search", pageSearch, pageMain, LevelStandard},                                               // 133 (the corner's Search; vr_menu_search.inc)
    {"Console", pageConsole, pageMain, LevelStandard},                                             // 134 (the corner's Console; vr_menu_console.inc)
    {"Tips", pageTips, pageAdvanced, LevelStandard},                                                   // 135 (vr_tips.cpp)
    {"Graphics - Teleporters", pageGraphicsTeleporters, pageGraphics},                                 // 135 (vr_portals.cpp)
    {"Small Gibs - Per Enemy", pageSmallGibsEnemies, pageSmallGibs, LevelDeveloper},              // 136
    {"Knockdowns", pageKnockdowns, pageCombat},                                                    // 137
    {"Explosion Debris", pageExplosionDebris, pageParticleSettings},
    {"Fire Particles", pageFireParticles, pageParticleSettings},
    {"Spawn Pickup Weapons", pageSpawnWeapons, pageDebugTests, LevelDeveloper},
    {"Map Library", pageMaps, pageMain, LevelStandard}, // (the corner's Maps, and Single Player > Map Library; vr_menu_maps.inc)
    {"Official Campaigns", pageCampaigns, pagePlay, LevelStandard},
    {"Machine Horde Tests", pageMachineHordeTests, pageDebugAutomation, LevelDeveloper},
    {"Graphics - Relighting", pageGraphicsRelighting, pageGraphics},
    {"Gore - Limb Gore", pageLimbGore, pageGore},
    {"Ragdolls - Vore", pageRagdollVore, pageRagdolls, LevelDeveloper},
    {"Ragdolls - Centroid", pageRagdollCentroid, pageRagdolls, LevelDeveloper},
    {"Dawn of the Machine Tests", pageMg3Tests, pageDebugAutomation, LevelDeveloper},
    {"Dawn of the Machine Weapons", pageMg3WeaponTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine Monsters", pageMg3MonsterTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine Bestiary", pageMg3BestiaryTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine: Shub", pageMg3ShubTests, pageDebugAutomation, LevelDeveloper},
    {"Dawn of the Machine: Chthon", pageMg3ChthonTests, pageDebugAutomation, LevelDeveloper},
    {"Debug - Cheats and Recording", pageDebugCheats, pageDebug, LevelDeveloper}, // (vr_menu_cheats.inc)
    {"Reloading", pageReloading, pageWeaponsHub},
    {"Reloading - Shotgun", pageReloadShotgun, pageReloading},
    {"Reloading - Super Shotgun", pageReloadSuperShotgun, pageReloading},
    {"Reloading - Nailgun", pageReloadNailgun, pageReloading},
    {"Reloading - Super Nailgun", pageReloadSuperNailgun, pageReloading},
    {"Reloading - Thunderbolt", pageReloadThunderbolt, pageReloading},
    {"Reloading - Nail Pistol", pageReloadNailPistol, pageReloading},
    {"Reloading - Heavy Revolver", pageReloadRevolver, pageReloading},
    {"Reloading - Launchers", pageReloadLaunchers, pageReloading},
    {"Reloading - Widowmaker", pageReloadWidowmaker, pageReloading},  // AD support
    {"Reloading - Plasma Gun", pageReloadPlasmaGun, pageReloading},   // AD support
    {"Reloading - Cell Cords", pageReloadCellCords, pageReloading},
    {"Per-Weapon Reload Overrides", pageReloadOverrides, pageReloading},
    {"Stealth AI", pageStealth, pageCombat}, // (vr_menu_stealth.inc)
    {"Stealth AI Tests", pageStealthTests, pageDebugAutomation, LevelDeveloper},
    {"Ragdoll Behaviour Tests", pageRagdollTests, pageDebugTests, LevelDeveloper}, // (vr_menu_ragtests.inc)
    {"Holding Enemies", pageHoldingEnemies, pageCombat},
    {"Holding Enemies - Struggle", pageHoldingStruggle, pageHoldingEnemies},
    {"Enemy Melee", pageEnemyMelee, pageCombat},
    {"Body - Crouched Pose", pageBodyCrouch, pageBody},
    {"Seated Play", pageSeated, pageVrsHeightHands, LevelStandard}, // (vr_seated.hpp; SEATED.md)
    {"Seated Holsters", pageSeatedHolsters, pageSeated, LevelStandard},
    {"Doom", pageDoom, pageGadget, LevelStandard},
    {"Arcane Dimensions", pageArcaneDimensions, pageAdvanced}, // AD support: dedicated tweaks page
    {"Arcane Dimensions Monsters", pageAdMonsterTests, pageDebugTests, LevelDeveloper}, // AD support (vr_menu_ad_monsters.inc)
    {"Arcane Dimensions' Guns", pageAdGuns, pageEnemyWeapons}, // AD support: the AD soldiers' guns (vr_ad_enemyguns.qc)
    {"Incandescent Blades", pageHotBlades, pageWeaponsHub}, // (QC vr_hotblade.qc)
    {"Blood Feedback", pageBloodFeedback, pageVrsComfortTurning, LevelStandard},
    // VR Settings' sections as pages (ROUND22.md, "VR Settings' sections as submenus"): vrSettingsParts.
    {"VR Settings - Height and Hand Calibration", pageVrsHeightHands, pageMain, LevelStandard},
    {"VR Settings - Body and Equipment", pageVrsBodyEquipment, pageMain, LevelStandard}, // (ROUND22.md, "Body and Equipment": the holsters and the ammo pouch in it)
    {"VR Settings - Ammo Pouch", pageVrsAmmoPouch, pageVrsBodyEquipment, LevelStandard}, // (its rows also on Body and Equipment)
    {"VR Settings - Locomotion and Controls", pageVrsLocoControls, pageMain, LevelStandard},
    {"VR Settings - Comfort, Teleport and Turning", pageVrsComfortTurning, pageMain, LevelStandard},
    {"VR Settings - Weapons and Bullet Time", pageVrsWeaponsBulletTime, pageMain, LevelStandard},
    {"VR Settings - Body and Flashlight", pageVrsBodyFlashlight, pageMain, LevelStandard},
    {"VR Settings - HUD, Haptics and Sound", pageVrsHudHapticsSound, pageMain, LevelStandard},
    {"VR Settings - Display and Graphics", pageVrsDisplayGraphics, pageMain, LevelStandard},
    {"VR Settings - Player Appearance", pageVrsAppearance, pageMain, LevelStandard}, // (ROUND22.md, "Gloves; Player Appearance")
    {"Ranger Head", pageVrsRangerHead, pageVrsAppearance, LevelStandard}, // (ROUND22.md, "The ranger's head: sliders")
    {"Debug - Automation & Self-Tests", pageDebugAutomation, pageDebug, LevelDeveloper}, // (vr_menu_automation.inc)
    {"Holster Zones", pageHolsterZones, pageImmersionSettings}, // (vr_holsterzones.hpp)
    {"Graphics - Loot Beams", pageGraphicsLootBeams, pageGraphics}, // (vr_lootbeams.cpp)
    {"Ragdoll Triggers", pageKdTriggers, pageCombat}, // (QC vr_kdtrig.qc)
    {"Ragdoll Triggers - Ledges", pageKdTrigLedge, pageKdTriggers},
    {"Ragdoll Triggers - Stairs", pageKdTrigStairs, pageKdTriggers},
    {"Ragdoll Triggers - Slopes", pageKdTrigSlope, pageKdTriggers},
    {"Ragdoll Triggers - Shotgun Range", pageKdTrigShotgun, pageKdTriggers},
    {"Ragdoll Triggers - Wounded", pageKdTrigWounded, pageKdTriggers},
    {"Ragdoll Triggers - Strong Blows", pageKdTrigBlow, pageKdTriggers},
    {"Ragdoll Triggers - Explosions", pageKdTrigBlast, pageKdTriggers},
    {"Ragdoll Triggers - Thrown Bodies", pageKdTrigThrown, pageKdTriggers},
    {"Ragdoll Triggers - Shoulder Charges", pageKdTrigTackle, pageKdTriggers},
    // Combat > Monster Groups (vr_menu_mgroups.inc): a page for each group, then one for each trigger and action.
    {"Monster Groups", pageMonsterGroups, pageCombat},
#define QVR_MGROUP(g, label, ...) {"Monster Groups - " label, pageMonsterGroup<mgroups::g##Group>, pageMonsterGroups},
#include "vr_mgroups.inc"
#undef QVR_MGROUP
    {"Monster Groups - Ragdoll Triggers", pageMonsterParam<0>, pageMonsterGroups},
    {"Monster Groups - Trigger Resistance", pageMonsterParam<1>, pageMonsterGroups},
    {"Monster Groups - Ledges", pageMonsterParam<2>, pageMonsterGroups},
    {"Monster Groups - Stairs", pageMonsterParam<3>, pageMonsterGroups},
    {"Monster Groups - Slopes", pageMonsterParam<4>, pageMonsterGroups},
    {"Monster Groups - Shotgun Range", pageMonsterParam<5>, pageMonsterGroups},
    {"Monster Groups - Wounded", pageMonsterParam<6>, pageMonsterGroups},
    {"Monster Groups - Strong Blows", pageMonsterParam<7>, pageMonsterGroups},
    {"Monster Groups - Explosions", pageMonsterParam<8>, pageMonsterGroups},
    {"Monster Groups - Thrown Bodies", pageMonsterParam<9>, pageMonsterGroups},
    {"Monster Groups - Tackles", pageMonsterParam<10>, pageMonsterGroups},
    {"Monster Groups - Shove Knockdown", pageMonsterParam<11>, pageMonsterGroups},
    {"Monster Groups - Shove Push", pageMonsterParam<12>, pageMonsterGroups},
    {"Monster Groups - Hits Unbalance", pageMonsterParam<13>, pageMonsterGroups},
    {"Monster Groups - Leg Sweep", pageMonsterParam<14>, pageMonsterGroups},
    {"Monster Groups - Leg Pull and Lift", pageMonsterParam<15>, pageMonsterGroups},
    {"Monster Groups - Lift", pageMonsterParam<16>, pageMonsterGroups},
    {"Monster Groups - Neck Snap", pageMonsterParam<17>, pageMonsterGroups},
    {"Monster Groups - Manhandle", pageMonsterParam<18>, pageMonsterGroups},
    {"Monster Groups - Two-Hand Throw", pageMonsterParam<19>, pageMonsterGroups},
    {"Monster Groups - Hold Strength", pageMonsterParam<20>, pageMonsterGroups},
    {"Monster Groups - Parry Poise", pageMonsterParam<21>, pageMonsterGroups},
    {"Monster Groups - Health", pageMonsterParam<22>, pageMonsterGroups},
    {"Monster Groups - Knockdown Chance", pageMonsterParam<23>, pageMonsterGroups},
    {"Monster Groups - Time Down", pageMonsterParam<24>, pageMonsterGroups},
    {"Monster Groups - Get-Up Speed", pageMonsterParam<25>, pageMonsterGroups},
    {"Monster Groups - Neck Snap Kills", pageMonsterParam<26>, pageMonsterGroups},
    {"Monster Groups - Struggle", pageMonsterParam<27>, pageMonsterGroups},
    {"Monster Groups - Struggle Strength", pageMonsterParam<28>, pageMonsterGroups},
    {"Monster Groups - Struggle Speed", pageMonsterParam<29>, pageMonsterGroups},
    {"Monster Groups - Struggle Arm Drive", pageMonsterParam<30>, pageMonsterGroups},
    {"Monster Groups - Struggle Weaker When Hurt", pageMonsterParam<31>, pageMonsterGroups},
    {"Monster Health", pageMonsterHealth, pageCombat}, // (vr_menu_mgroups.inc)
    {"Ragdoll Tool", pageRagtool, pageDebug}, // (vr_menu_ragtool.inc; vr_ragtool.cpp)
    {"Ragdoll Tool - Motors", pageRagtoolParams<0>, pageRagtool},
    {"Ragdoll Tool - Joint Groups", pageRagtoolParams<1>, pageRagtool},
    {"Ragdoll Tool - Stagger and Balance", pageRagtoolParams<2>, pageRagtool},
    {"Ragdoll Tool - Shove", pageRagtoolParams<3>, pageRagtool},
    {"Ragdoll Tool - Knockdown and Get-Up", pageRagtoolParams<4>, pageRagtool},
    {"Ragdoll Tool - Death and Crawl", pageRagtoolParams<5>, pageRagtool},
    {"Ragdoll Tool - Struggle", pageRagtoolParams<6>, pageRagtool},
    {"Ragdoll Tool - Parry and Hit Stagger", pageRagtoolParams<7>, pageRagtool},
    {"Ragdoll Tool - Flail, Liquids and Hand", pageRagtoolParams<8>, pageRagtool},
    {"Saves and Autosaves", pageSaves, pageMain, LevelStandard}, // (vr_menu_settings.inc; vr_continue.cpp)
    // (The author, 2026-10-22: the holsters' rows on their own page; Carry and Auto-Collect under Playing, then one Item Pickup Mode switch.)
    {"Holster Calibration", pageVrsHolsters, pageMain, LevelStandard}, // (2026-10-23: on VR Settings itself)
    {"VR Settings - Item Collection", pageVrsItemCollection, pageMain, LevelStandard},
    {"Armed Grunts (Experimental)", pageArmedGrunts, pageCombat}, // (vr_menu_gameplay.inc; QC vr_armedgrunt.qc)
    {"Armed Grunts - Grip Editor", pageArmedGrips, pageArmedGrunts}, // (vr_menu_gameplay.inc; vr_aggrip.cpp)
    {"Armed Humanoid Replacements", pageArmedRepl, pageArmedGrunts}, // (vr_menu_gameplay.inc; QC vr_armedrepl.qc)
    {"Armed Grunts - Weapon Holds", pageArmedHolds, pageArmedGrunts}, // (vr_menu_gameplay.inc; vr_agholds.inc)
    {"Graphics Presets", pageGraphicsPresets, pageMain, LevelStandard}, // (vr_menu_pages.inc; vr_gfxpresets.cpp)
    {"Graphics - Shoreline Foam", pageGraphicsFoam, pageGraphicsLiquids}, // (vr_menu_pages.inc; vr_water.cpp foamLook)
    {"Death Sergeant's Flail", pageFlail, pageMeleeSettings}, // AD support (vr_menu_ad_guns.inc; vr_box3d_flail.cpp)
};
inline constexpr int pageCount = static_cast<int>(sizeof(pages) / sizeof(pages[0]));

// The number of the page `build` builds, while compiling only (at run time two builders may share an address: above).
consteval int pageNumberOf(PageFn build)
{
    for(int p = 0; p < pageCount; p++)
    {
        if(pages[p].build.fn == build)
        {
            return p;
        }
    }
    return -1;
}

// Each page's builder in pages[] once (a link finds the first), and every page's home a page.
consteval bool pageBuildersUnique()
{
    for(int p = 0; p < pageCount; p++)
    {
        for(int q = 0; q < p; q++)
        {
            if(pages[p].build.fn == pages[q].build.fn)
            {
                return false;
            }
        }
    }
    return true;
}
static_assert(pageBuildersUnique(), "a page's builder is in pages[] twice: each page needs its own builder");

struct PageHomes
{
    int of[pageCount]; // by page: its home's number (PageMain for VR Settings itself)
};

consteval PageHomes pageHomesOf()
{
    PageHomes h{};
    for(int p = 0; p < pageCount; p++)
    {
        h.of[p] = pages[p].home.fn ? pageNumberOf(pages[p].home.fn) : PageMain;
    }
    return h;
}

consteval bool pageHomesFound()
{
    const PageHomes h = pageHomesOf();
    for(int p = 0; p < pageCount; p++)
    {
        if(h.of[p] < 0)
        {
            return false;
        }
    }
    return true;
}
static_assert(pageHomesFound(), "a page's home is not in pages[]");

inline constexpr PageHomes pageHomes = pageHomesOf();

// A link to a builder not in pages[] does not compile: this is not constexpr, and never defined.
void linkedPageIsNotInPagesTable();

consteval PageRef::PageRef(PageFn build) : index(pageNumberOf(build))
{
    if(index < 0)
    {
        linkedPageIsNotInPagesTable();
    }
}

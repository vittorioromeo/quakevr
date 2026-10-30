// vr_haptics.qc -- the hand-held things' named buzzes (the review's haptic presets): each kind of grab, take, refusal
// and knock felt in the hand, its values written once here. Every one sends to `self` (the player), as haptic() does.
// The values are the ones each caller used before (no feel changed): a preset is a name, not a new pulse.
// (Combat, melee, burning and the other effects keep their own calls.)

// The same preset in both hands, the off hand's first.
void(void(float xHand) xPreset) VR_Haptic_Both =
{
    xPreset(cVR_OffHand);
    xPreset(cVR_MainHand);
};

// --- Pickups and holsters

// An item picked up by touch (VRItemPickupHaptic).
void(float xHand) VR_Haptic_Pickup = { haptic(xHand, 0.0, 0.3, 75, 1.0); };

// No: a firm double knock (armour not worn, yours as good; an item not taken at a holster, you have it).
void(float xHand) VR_Haptic_Refuse =
{
    haptic(xHand, 0.0, 0.06, 40, 0.8);
    haptic(xHand, 0.12, 0.06, 40, 0.8);
};

// A faint double tap (armour over the torso, yours better).
void(float xHand) VR_Haptic_FaintTap =
{
    haptic(xHand, 0.0, 0.03, 40, 0.3);
    haptic(xHand, 0.08, 0.03, 40, 0.3);
};

// The holster's buzz (a hand over a holster, armour over the torso that can go on): vr_holster_haptic_time seconds,
// three pulses each taking over from the one before, strong as the hand arrives and fading.
void(float xHand) VR_Haptic_HolsterHover =
{
    float t = max(0.05, cvar_hget(cvarh_vr_holster_haptic_time));
    haptic(xHand, 0.0, t * 0.45, 80, 0.65);
    haptic(xHand, t * 0.4, t * 0.35, 80, 0.4);
    haptic(xHand, t * 0.7, t * 0.3, 80, 0.2);
};

// --- Carrying

// A prop taken (by one hand, or the second hand joining).
void(float xHand) VR_Haptic_GrabProp = { haptic(xHand, 0.0, 0.1, 70, 0.6); };

// A soft knock: a carried thing nudged, let go of from both hands.
void(float xHand) VR_Haptic_SoftKnock = { haptic(xHand, 0.0, 0.05, 60, 0.3); };

// The pack is full.
void(float xHand) VR_Haptic_PackFull = { haptic(xHand, 0.0, 0.06, 40, 0.9); };

// A hand touching a ragdoll.
void(float xHand) VR_Haptic_RagdollTouch = { haptic(xHand, 0.0, 0.08, 60, 0.5); };

// A wall torch taken.
void(float xHand) VR_Haptic_GrabTorch = { haptic(xHand, 0.0, 0.08, 80, 0.8); };

// The flashlight off its mount, and back on (a click and a second, higher one).
void(float xHand) VR_Haptic_LampDetach = { haptic(xHand, 0, 0.06, 90, 0.6); };
void(float xHand) VR_Haptic_LampAttach =
{
    haptic(xHand, 0, 0.05, 120, 0.8);
    haptic(xHand, 0.09, 0.05, 160, 0.5);
};

// A grenade slapped into the palm (trapped); the held grenade's cue (VR_Grenade_Cue).
void(float xHand) VR_Haptic_GrenadeTrap = { haptic(xHand, 0.0, 0.06, 60, 0.5); };
void(float xHand) VR_Haptic_GrenadeTick = { haptic(xHand, 0.0, 0.03, 140, 0.35); };

// --- The force grab

// A weapon caught: a firm knock and a short fade.
void(float xHand) VR_Haptic_ForceCatch =
{
    haptic(xHand, 0.0, 0.07, 90, 0.85);
    haptic(xHand, 0.06, 0.08, 70, 0.4);
};
void(float xHand) VR_Haptic_ForceLaunch = { haptic(xHand, 0.0, 0.12, 50, 0.6); };   // it flies to the hand
void(float xHand) VR_Haptic_ForceEligible = { haptic(xHand, 0.0, 0.08, 75, 0.2); }; // another one in reach
void(float xHand) VR_Haptic_ForceTarget = { haptic(xHand, 0.0, 0.1, 100, 0.5); };   // the one targeted

// --- Foes

void(float xHand) VR_Haptic_GrabFoe = { haptic(xHand, 0, 0.08, 120, 0.6); };   // a foe taken
void(float xHand) VR_Haptic_FoeTorn = { haptic(xHand, 0, 0.12, 60, 0.7); };    // torn from the hand
void(float xHand) VR_Haptic_FoeDown = { haptic(xHand, 0, 0.18, 90, 1); };      // thrown down
void(float xHand) VR_Haptic_FoeHolds = { haptic(xHand, 0, 0.1, 40, 0.5); };    // too strong: it holds its ground
void(float xHand) VR_Haptic_FoeBreakFree = { haptic(xHand, 0, 0.2, 60, 0.9); }; // it broke free
void(float xHand) VR_Haptic_NeckSnap = { haptic(xHand, 0, 0.15, 50, 0.8); };    // its neck snapped (no damage)
void(float xHand) VR_Haptic_NeckCrack = { haptic(xHand, 0, 0.2, 45, 0.9); };    // a tough one's neck hurt
void(float xHand) VR_Haptic_NeckBreak = { haptic(xHand, 0, 0.25, 45, 1); };     // its neck broken: dead

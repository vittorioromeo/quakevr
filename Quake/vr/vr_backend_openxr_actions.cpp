// vr_backend_openxr_actions.cpp -- the controllers (vr_backend_openxr.cpp): the actions and their suggested bindings,
// the interaction profiles, the input read, haptics and the hand poses.

#include "vr_backend_openxr_internal.hpp"

#ifdef QVR_HAVE_OPENXR

namespace qvr::oxr
{

int OpenXrBackend::handSide(int hand)
{
    return hand == HAND_MAIN ? 1 : 0;
}

const char* OpenXrBackend::shortProfile(const char* name)
{
    const char* prefix = "/interaction_profiles/";
    return strncmp(name, prefix, strlen(prefix)) ? name : name + strlen(prefix);
}

void OpenXrBackend::updateControllers()
{
    bool changed = false;
    for(int side = 0; side < 2; side++)
    {
        const char* hand = side == 0 ? "left" : "right";
        XrInteractionProfileState state{XR_TYPE_INTERACTION_PROFILE_STATE};
        controller[side] = Controller::Other;
        char name[XR_MAX_PATH_LENGTH]{};
        const XrResult r = xrGetCurrentInteractionProfile(session, handPaths[side], &state);
        if(XR_SUCCEEDED(r) && state.interactionProfile != XR_NULL_PATH)
        {
            uint32_t len = 0;
            xrPathToString(instance, state.interactionProfile, sizeof(name), &len, name);
        }
        if(strcmp(name, profileName[side]) != 0)
        {
            q_strlcpy(profileName[side], name, sizeof(profileName[side]));
            changed = true;
            char line[XR_MAX_PATH_LENGTH + 96];
            q_snprintf(line, sizeof(line), "OpenXR: the %s hand's controller profile: %s%s\n", hand, name[0] ? name : "none",
                XR_SUCCEEDED(r) ? "" : " (xrGetCurrentInteractionProfile failed)");
            Con_Printf("%s", line);
            xrruntime::logLine(va("%s %s", wallClock(), line));
        }
        if(vr_debug_buttons.value)
        {
            Con_Printf("VR buttons: the %s hand: %s%s (%s)\n", hand, name[0] ? "interaction profile " : "no interaction profile",
                name, runtime);
        }
        if(!name[0])
        {
            continue;
        }

        if(!strcmp(name, "/interaction_profiles/oculus/touch_controller") ||
            !strcmp(name, "/interaction_profiles/meta/touch_plus_controller"))
        {
            controller[side] = Controller::Touch;
        }
        else if(!strcmp(name, "/interaction_profiles/bytedance/pico_neo3_controller") ||
                !strcmp(name, "/interaction_profiles/bytedance/pico4_controller") ||
                !strcmp(name, "/interaction_profiles/htc/vive_cosmos_controller") ||
                !strcmp(name, "/interaction_profiles/htc/vive_focus3_controller"))
        {
            // Shaped as Touch controllers (a handle, a stick and two face buttons): Touch's legacy grip is the nearest
            // the released offsets have (ROUND22.md, "Menu on any controller").
            controller[side] = Controller::Touch;
        }
        else if(!strcmp(name, "/interaction_profiles/valve/index_controller"))
        {
            // Virtual Desktop's "Emulate Index controllers" reports Index controllers for what
            // are Quest controllers: VDXR's grip is Meta's Touch one whatever the profile.
            controller[side] = vdxr ? Controller::Touch : Controller::Index;
        }

        // A profile without the game's buttons (the simple controller: a trigger and a menu button; or one the game
        // has no bindings for): said once a start, in the console, qvr_openxr.txt and in the game.
        bool full = false;
        for(const char* known : fullProfiles)
        {
            full = full || !strcmp(name, known);
        }
        if(!full && !limitedWarned)
        {
            limitedWarned = true;
            const char* line = va("OpenXR: the runtime plays the %s controller as %s, which has few of the game's "
                                  "buttons (no grips, no A/B/X/Y): grabbing, climbing and jumping may not work. Its "
                                  "controller bindings for the game were turned down or it does not know the "
                                  "controllers; qvr_openxr.txt has the details\n",
                hand, name);
            Con_Warning("%s", line);
            xrruntime::logLine(va("WARNING: %s", line)); // (xrruntime::warn's log is the start's only)
            q_snprintf(limitedNotice, sizeof(limitedNotice),
                "Your controllers are played as \"%s\":\ncontrols may be limited (no grips or A/B/X/Y).\nSee qvr_openxr.txt "
                "in the quakevr folder.",
                shortProfile(name));
        }
    }
    if(changed)
    {
        setCrashVr(nullptr);
    }
}

Pose OpenXrBackend::toLegacyPose(const Pose& grip, int side) const
{
    if(!grip.valid || controller[side] == Controller::Other)
    {
        return grip;
    }

    const GripInRaw g = legacyGripInRaw(controller[side] == Controller::Touch, side);
    const glm::vec3 offset = g.offset;
    const glm::quat r = g.turn;

    // raw = grip * (T(offset) R)^-1 = grip * R^-1 T(-offset)
    const glm::quat rInv = glm::inverse(r);
    Pose raw = grip;
    raw.orientation = grip.orientation * rInv;
    raw.position = grip.position + grip.orientation * (rInv * -offset);
    if(grip.velocityValid)
    {
        // With the runtime's angular velocity as it came: vr_angvel.cpp redoes this lever term once it has put that in
        // the tracking space (VirtualDesktopXR gives it in the controller's frame). Takes record it as it is here.
        raw.linearVelocity = grip.linearVelocity + glm::cross(grip.angularVelocity, raw.position - grip.position);
        raw.gripVelocity = grip.linearVelocity;
        raw.gripVelocityValid = true;
    }
    return raw;
}

XrPath OpenXrBackend::path(const char* s) const
{
    XrPath p = XR_NULL_PATH;
    xrStringToPath(instance, s, &p);
    return p;
}

XrAction OpenXrBackend::makeAction(XrActionType type, const char* name, const char* localized, bool perHand)
{
    XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
    info.actionType = type;
    q_strlcpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE);
    q_strlcpy(info.localizedActionName, localized, XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    if(perHand)
    {
        info.countSubactionPaths = 2;
        info.subactionPaths = handPaths;
    }

    XrAction action = XR_NULL_HANDLE;
    check(xrCreateAction(actionSet, &info, &action), name);
    return action;
}

XrResult OpenXrBackend::suggestList(XrPath profile, const za::Vector<XrActionSuggestedBinding>& list) const
{
    XrInteractionProfileSuggestedBinding info{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    info.interactionProfile = profile;
    info.suggestedBindings = list.data();
    info.countSuggestedBindings = static_cast<uint32_t>(list.size());
    return xrSuggestInteractionProfileBindings(instance, &info);
}

bool OpenXrBackend::suggest(const char* profile, za::InitializerList<Binding> bindings)
{
    const XrPath profilePath = path(profile);
    za::Vector<XrActionSuggestedBinding> all;
    for(const Binding& b : bindings)
    {
        all.pushBack({*b.action, path(b.path)});
    }
    const int count = static_cast<int>(all.size());
    const XrResult whole = suggestList(profilePath, all);
    if(XR_SUCCEEDED(whole))
    {
        xrruntime::logLine(va("OpenXR: bindings suggested for %s (%d)\n", profile, count));
        return true;
    }

    char result[XR_MAX_RESULT_STRING_SIZE] = "";
    xrResultToString(instance, whole, result);
    za::Vector<XrActionSuggestedBinding> kept;
    za::Vector<int> dropped; // indexes into bindings
    bool lastTurnedDown = false;
    for(int i = 0; i < count; i++)
    {
        kept.pushBack(all[i]);
        lastTurnedDown = !XR_SUCCEEDED(suggestList(profilePath, kept));
        if(lastTurnedDown)
        {
            kept.popBack();
            dropped.pushBack(i);
        }
    }
    if(kept.empty())
    {
        xrruntime::logLine(va("OpenXR: no bindings for %s: the runtime does not have it (%s)\n", profile, result));
        return false;
    }
    // (A turned-down call changes nothing, but a runtime might not hold to that: the kept ones once more.)
    if(lastTurnedDown && !XR_SUCCEEDED(suggestList(profilePath, kept)))
    {
        xrruntime::warn("OpenXR: %s: its bindings turned down (%s), and its %d accepted one by one too\n", profile,
            result, static_cast<int>(kept.size()));
        return false;
    }
    for(const int i : dropped)
    {
        xrruntime::warn("OpenXR: %s: %s turned down by the runtime (%s): not bound\n", profile, bindings.begin()[i].path,
            result);
    }
    xrruntime::logLine(va("OpenXR: bindings suggested for %s (%d of %d: %d turned down)\n", profile,
        static_cast<int>(kept.size()), count, static_cast<int>(dropped.size())));
    return true;
}

void OpenXrBackend::suggestBindings()
{
#define QVR_L(p) "/user/hand/left/" p
#define QVR_R(p) "/user/hand/right/" p
#define QVR_BOTH(action, p) {&action, QVR_L(p)}, {&action, QVR_R(p)}

    suggest("/interaction_profiles/khr/simple_controller",
        {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/select/click"),
            QVR_BOTH(triggerValueAction, "input/select/click"),
            QVR_BOTH(menuAction, "input/menu/click"), QVR_BOTH(hapticAction, "output/haptic")});

    // Touch: the core profile's paths only (OpenXR 1.0, no extension's; VDXR 1.0.x and 1.1 take every one). Quest 3's
    // own profile (XR_META_touch_controller_plus: /interaction_profiles/meta/touch_plus_controller) is not suggested:
    // 1.0.0 asked for it under a wrong name (meta/touch_controller_plus, turned down), so every runtime has played
    // Quest 3 controllers as Touch ones, with these bindings; that stays.
    suggest("/interaction_profiles/oculus/touch_controller",
        {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
            QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
            QVR_BOTH(stickClickAction, "input/thumbstick/click"),
            {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
            {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
            {&menuAction, QVR_L("input/menu/click")}, QVR_BOTH(hapticAction, "output/haptic"),
            QVR_BOTH(triggerValueAction, "input/trigger/value"),
            QVR_BOTH(gripValueAction, "input/squeeze/value"),
            QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"),
            {&thumbTouchAction, QVR_L("input/x/touch")}, {&thumbTouchAction, QVR_L("input/y/touch")},
            {&thumbTouchAction, QVR_R("input/a/touch")}, {&thumbTouchAction, QVR_R("input/b/touch")},
            QVR_BOTH(triggerTouchAction, "input/trigger/touch")});

    // No menu button: the left B opens the menu.
    suggest("/interaction_profiles/valve/index_controller",
        {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
            QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
            QVR_BOTH(stickClickAction, "input/thumbstick/click"), QVR_BOTH(primaryAction, "input/a/click"),
            {&secondaryAction, QVR_R("input/b/click")}, {&menuAction, QVR_L("input/b/click")},
            QVR_BOTH(hapticAction, "output/haptic"), QVR_BOTH(triggerValueAction, "input/trigger/value"),
            QVR_BOTH(gripValueAction, "input/squeeze/value"),
            QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"), QVR_BOTH(thumbTouchAction, "input/a/touch"),
            QVR_BOTH(thumbTouchAction, "input/b/touch"), QVR_BOTH(thumbTouchAction, "input/trackpad/touch"),
            QVR_BOTH(triggerTouchAction, "input/trigger/touch")});

    // Trackpads as sticks, their clicks as the primary buttons; the right menu button is the
    // secondary button.
    suggest("/interaction_profiles/htc/vive_controller",
        {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/click"),
            QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/trackpad"),
            QVR_BOTH(primaryAction, "input/trackpad/click"),
            {&secondaryAction, QVR_R("input/menu/click")}, {&menuAction, QVR_L("input/menu/click")},
            QVR_BOTH(hapticAction, "output/haptic"), QVR_BOTH(triggerValueAction, "input/trigger/value"),
            QVR_BOTH(gripValueAction, "input/squeeze/click"),
            QVR_BOTH(thumbTouchAction, "input/trackpad/touch")});

    // Trackpad clicks are the primary buttons, the right menu button the secondary one.
    suggest("/interaction_profiles/microsoft/motion_controller",
        {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
            QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/thumbstick"),
            QVR_BOTH(stickClickAction, "input/thumbstick/click"),
            QVR_BOTH(primaryAction, "input/trackpad/click"),
            {&secondaryAction, QVR_R("input/menu/click")}, {&menuAction, QVR_L("input/menu/click")},
            QVR_BOTH(hapticAction, "output/haptic"), QVR_BOTH(triggerValueAction, "input/trigger/value"),
            QVR_BOTH(gripValueAction, "input/squeeze/click"),
            QVR_BOTH(thumbTouchAction, "input/trackpad/touch")});

    // The vendors' profiles, when the runtime offers them (vr_xr_vendor_profiles). Each has a menu button for the
    // menu: the left one (the right is the runtime's own system button on most).
    if(vendorProfile[VendorPico])
    {
        for(const char* pico : {"/interaction_profiles/bytedance/pico_neo3_controller",
                "/interaction_profiles/bytedance/pico4_controller"})
        {
            suggest(pico,
                {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                    QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
                    QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                    {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
                    {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
                    {&menuAction, QVR_L("input/menu/click")}, QVR_BOTH(hapticAction, "output/haptic"),
                    QVR_BOTH(triggerValueAction, "input/trigger/value"),
                    QVR_BOTH(gripValueAction, "input/squeeze/value"),
                    QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"),
                    {&thumbTouchAction, QVR_L("input/x/touch")}, {&thumbTouchAction, QVR_L("input/y/touch")},
                    {&thumbTouchAction, QVR_R("input/a/touch")}, {&thumbTouchAction, QVR_R("input/b/touch")},
                    QVR_BOTH(triggerTouchAction, "input/trigger/touch")});
        }
    }
    // The HTC ones' grips as clicks (the Focus 3's value came in a later revision: not asked).
    if(vendorProfile[VendorCosmos])
    {
        suggest("/interaction_profiles/htc/vive_cosmos_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/thumbstick"),
                QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
                {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
                {&menuAction, QVR_L("input/menu/click")}, QVR_BOTH(hapticAction, "output/haptic"),
                QVR_BOTH(triggerValueAction, "input/trigger/value"), QVR_BOTH(gripValueAction, "input/squeeze/click"),
                QVR_BOTH(thumbTouchAction, "input/thumbstick/touch")});
    }
    if(vendorProfile[VendorFocus3])
    {
        suggest("/interaction_profiles/htc/vive_focus3_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/thumbstick"),
                QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
                {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
                {&menuAction, QVR_L("input/menu/click")}, QVR_BOTH(hapticAction, "output/haptic"),
                QVR_BOTH(triggerValueAction, "input/trigger/value"), QVR_BOTH(gripValueAction, "input/squeeze/click"),
                QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"), QVR_BOTH(thumbTouchAction, "input/thumbrest/touch"),
                QVR_BOTH(triggerTouchAction, "input/trigger/touch")});
    }
    // The Reverb G2: A/B and X/Y as on Touch (the WMR profile has none), a menu button on each hand.
    if(vendorProfile[VendorHp])
    {
        suggest("/interaction_profiles/hp/mixed_reality_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
                QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
                {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
                QVR_BOTH(menuAction, "input/menu/click"), QVR_BOTH(hapticAction, "output/haptic"),
                QVR_BOTH(triggerValueAction, "input/trigger/value"), QVR_BOTH(gripValueAction, "input/squeeze/value")});
    }
    // As the WMR controllers (the same controls).
    if(vendorProfile[VendorOdyssey])
    {
        suggest("/interaction_profiles/samsung/odyssey_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/thumbstick"),
                QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                QVR_BOTH(primaryAction, "input/trackpad/click"),
                {&secondaryAction, QVR_R("input/menu/click")}, {&menuAction, QVR_L("input/menu/click")},
                QVR_BOTH(hapticAction, "output/haptic"), QVR_BOTH(triggerValueAction, "input/trigger/value"),
                QVR_BOTH(gripValueAction, "input/squeeze/click"),
                QVR_BOTH(thumbTouchAction, "input/trackpad/touch")});
    }

#undef QVR_BOTH
#undef QVR_L
#undef QVR_R
}

bool OpenXrBackend::createActions()
{
    XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    q_strlcpy(setInfo.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE);
    q_strlcpy(setInfo.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
    if(!check(xrCreateActionSet(instance, &setInfo, &actionSet), "xrCreateActionSet"))
    {
        return false;
    }

    handPaths[0] = path("/user/hand/left");
    handPaths[1] = path("/user/hand/right");

    gripPose = makeAction(XR_ACTION_TYPE_POSE_INPUT, "grip_pose", "Hand pose", true);
    triggerAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "trigger", "Trigger", true);
    gripAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "grip", "Grip", true);
    primaryAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "primary", "Primary button (A/X)", true);
    secondaryAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "secondary", "Secondary button (B/Y)", true);
    stickClickAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "stick_click", "Thumbstick click", true);
    menuAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu", true);
    stickAction = makeAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "stick", "Thumbstick", true);
    triggerValueAction = makeAction(XR_ACTION_TYPE_FLOAT_INPUT, "trigger_curl", "Index finger (trigger)", true);
    gripValueAction = makeAction(XR_ACTION_TYPE_FLOAT_INPUT, "grip_curl", "Other fingers (grip)", true);
    thumbTouchAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "thumb_touch", "Thumb resting", true);
    triggerTouchAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "trigger_touch", "Index finger on the trigger", true);
    hapticAction = makeAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Haptics", true);

    suggestBindings();

    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &actionSet;
    if(!check(xrAttachSessionActionSets(session, &attachInfo), "xrAttachSessionActionSets"))
    {
        return false;
    }

    for(int side = 0; side < 2; side++)
    {
        XrActionSpaceCreateInfo spaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        spaceInfo.action = gripPose;
        spaceInfo.subactionPath = handPaths[side];
        spaceInfo.poseInActionSpace.orientation.w = 1.f;
        if(!check(xrCreateActionSpace(session, &spaceInfo, &handSpaces[side]), "xrCreateActionSpace"))
        {
            return false;
        }
    }

    return true;
}

bool OpenXrBackend::boolState(XrAction action, XrPath subaction) const
{
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = subaction;
    XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
    return XR_SUCCEEDED(xrGetActionStateBoolean(session, &info, &state)) && state.isActive &&
           state.currentState;
}

float OpenXrBackend::floatState(XrAction action, XrPath subaction) const
{
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = subaction;
    XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
    if(!XR_SUCCEEDED(xrGetActionStateFloat(session, &info, &state)) || !state.isActive)
    {
        return 0.f;
    }
    return state.currentState;
}

glm::vec2 OpenXrBackend::vec2State(XrAction action, XrPath subaction) const
{
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = subaction;
    XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
    if(!XR_SUCCEEDED(xrGetActionStateVector2f(session, &info, &state)) || !state.isActive)
    {
        return glm::vec2{0.f};
    }
    return {state.currentState.x, state.currentState.y};
}

void OpenXrBackend::readInput(InputState& in) const
{
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const XrPath side = handPaths[handSide(h)];
        HandInput& hand = in.hands[h];
        hand.trigger = boolState(triggerAction, side);
        hand.grip = boolState(gripAction, side);
        hand.primary = boolState(primaryAction, side);
        hand.secondary = boolState(secondaryAction, side);
        hand.stickClick = boolState(stickClickAction, side);
        hand.menu = boolState(menuAction, side);
        hand.stick = vec2State(stickAction, side);
        hand.triggerValue = floatState(triggerValueAction, side);
        hand.gripValue = floatState(gripValueAction, side);
        hand.thumbTouch = boolState(thumbTouchAction, side);
        hand.triggerTouch = boolState(triggerTouchAction, side);
    }
}

void OpenXrBackend::haptic(int hand, float seconds, float frequency, float amplitude)
{
    // Every vibration times Vibration Strength (vr_haptics_strength; 0: none).
    const float strength = CLAMP(0.f, vr_haptics_strength.value, 2.f);
    if(!sessionRunning || hapticAction == XR_NULL_HANDLE || strength <= 0.f)
    {
        return;
    }
    amplitude *= strength;

    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
    vibration.duration = seconds > 0.f ? static_cast<XrDuration>(seconds * 1e9) : XR_MIN_HAPTIC_DURATION;
    vibration.frequency = frequency > 0.f ? frequency : XR_FREQUENCY_UNSPECIFIED;
    vibration.amplitude = CLAMP(0.f, amplitude, 1.f);

    XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
    info.action = hapticAction;
    info.subactionPath = handPaths[handSide(hand)];
    xrApplyHapticFeedback(session, &info, reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
}

void OpenXrBackend::checkControllers()
{
    const double now = Sys_DoubleTime();
    if(sessionRunning && (!profileName[0][0] || !profileName[1][0]) && now >= nextProfileCheck)
    {
        nextProfileCheck = now + 2.0;
        updateControllers();
    }
    if(limitedNotice[0] && cls.signon == SIGNONS && key_dest == key_game)
    {
        SCR_CenterPrint(limitedNotice);
        limitedNotice[0] = '\0';
    }
}

} // namespace qvr::oxr

#endif // QVR_HAVE_OPENXR

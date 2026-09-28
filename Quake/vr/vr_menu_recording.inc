// vr_menu_recording.inc -- the Recording page (Graphics > Recording): what the desktop window shows while in VR, for
// recording it with OBS (vr_window.cpp; ROUND21.md, "Recording: smoothed mirror and spectator camera"). Included by
// vr_menu.cpp; self-contained, so it can move with a reorganized menu.

[[nodiscard]] std::vector<Item> pageRecording()
{
    return {
        cycle("Window View", "vr_window_view", {{0.f, "Left Eye (raw)"}, {1.f, "Smoothed Mirror"}, {2.f, "Spectator Camera"}})
            .help("What the desktop window shows, to record it (OBS: a Window Capture of it). Left Eye: the eye's image as it "
                  "is, shaking with your head. Smoothed Mirror: the same image, turned to follow a steadied head and cropped "
                  "to the window: free. Spectator Camera (Recording Mode): the game drawn a third time for the window, from "
                  "your head, steadied, with its own field of view: sharpest and widest, and it costs about one more eye's "
                  "rendering. The headset's view is the same in all three."),
        cycle("Desktop Mirror", "vr_mirror", {{0.f, "Off"}, {1.f, "On"}, {2.f, "Both Eyes (Left Eye view)"}})
            .help("Off: the window shows nothing of the game in VR (in any Window View), which saves its small cost."),
        header("Steadying"),
        slider("Smoothing", "vr_window_smooth", 0.f, 0.5f, 0.01f, "%.2f s")
            .help("How much the head's shake is smoothed out: the view's lag behind your head. 0.1-0.2 s takes out the "
                  "shake and keeps turns lively; more glides like a camera operator's."),
        slider("Level Horizon", "vr_window_level", 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of your head's tilt to the side is taken out of the window's view: 1 keeps the horizon level."),
        header("Smoothed Mirror"),
        slider("Zoom", "vr_window_zoom", 1.f, 2.f, 0.05f, "%.2fx")
            .help("How much of the eye's image the window shows: 1 the widest crop of the window's shape; higher crops "
                  "closer and leaves a margin for the steadied view to turn in. Past the margin, the view follows your "
                  "head: raise it if quick turns show the view catching up."),
        header("Spectator Camera"),
        slider("Field of View", "vr_spectator_fov", 60.f, 120.f, 1.f, "%.0f deg").extend(40.f, 130.f)
            .help("The window's horizontal field of view (90 as a flat game; the vertical one follows the window's shape)."),
        slider("Resolution Scale", "vr_spectator_scale", 0.5f, 2.f, 0.25f, "%.2fx").extend(0.25f, 2.f)
            .help("Its resolution, times the window's: 1 sharp at the window's size; lower costs less (the pixels go "
                  "with its square); 2 supersamples (four times the pixels)."),
        slider("Position Smoothing", "vr_spectator_pos_smooth", 0.f, 0.3f, 0.01f, "%.2f s")
            .help("How much your head's small movements are smoothed out (its lag). Walking, turning and teleporting are "
                  "never smoothed."),
    };
}

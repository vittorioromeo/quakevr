// vr_bodycal.hpp -- Body Calibration (VR menu > Body): the player is asked to hold a few poses and make a few moves,
// and the body's arms are measured from them, so that the drawn arm bends and straightens with the real one.
//
// The poses (vr_bodycal.cpp's steps): standing tall with the arms straight down (the eye height, and the shoulders'
// height), a T-pose, both arms forward, both arms up (each captured once held still), big circles with straight arms,
// the forearms waved about still elbows, and the wrists bent about still forearms (each recorded for a few seconds).
// Seated, the first is left out (the height stays).
//
// What is measured, jointly (a least-squares fit with outliers dropped, vr_bodycal.cpp's fit()), with the body's own
// arm model (avatar::shoulderInChest: the shoulders on the chest, their rise and swing): the shoulders' place on the chest
// (vr_bodycal_shoulders_back, _up, _out), the reach from the shoulder joint to the drawn wrist and its split into the
// upper arm and the forearm (vr_bodycal_upper_arm, vr_bodycal_forearm, real centimetres: the circles and poses give the
// reach, the forearm's sweep about the elbow its length and the elbow's place), and how far the shoulders rise and swing
// (vr_bodycal_shoulder_rise, _swing). The wrists' bends check the hand calibration: how far the drawn hand's wrist is
// from the real wrist they turn about. The drawn wrist is the empty hand's on the calibrated controller (Hand
// Calibration's), whatever the hand holds.
//
// The player's tweaks (vr_body_tweak_*, Arms and Pauldrons) go on top of the measurements, in the same units (0: as
// measured). Apply keeps them, and the preview shows them; only the first Apply (from the default body, which they were
// made for) sets them to 0. Undo puts back the measurements and the tweaks from before.
//
// Each session's samples are saved (bodycal/<date>.txt in the game folder); vr_bodycal_refit fits one again.

#pragma once

#include "vr_hands.hpp"

namespace qvr::bodycal
{

void init(); // the cvars' commands

// Once a frame (vr_main.cpp, after the texts are cleared): the steps, their text, the ghost showing the pose, the
// preview of a result.
void frame();
// From the view, once the hands are set up this frame: the samples.
void viewFrame(const hands::State& s);

enum class Phase
{
    Idle,
    Capturing,
    Result, // measurements to Apply or Cancel
};
[[nodiscard]] Phase phase();

// The menu page (reopened on `returnPage` with the result). start: the missing poses (all, or those not yet taken
// after a stop); restart: all of them again; redo: pose `step` only, the others kept.
bool start(int returnPage);
bool restart(int returnPage);
bool redo(int step, int returnPage);
void apply();
void cancel();
// Whether the result can be applied (its poses agree).
[[nodiscard]] bool trusted();
[[nodiscard]] bool canUndo();
void undo();
// Whether some poses are taken but not all (a stopped session: Continue).
[[nodiscard]] bool partial();
// Result: whether the body shown in front shows the new measurements (else the current settings); switch() flips it.
[[nodiscard]] bool showingNew();
void switchShown();

// Changes whenever the page must be built again.
[[nodiscard]] int version();
// The page's text lines (0..): the result, the progress, or what was applied; null past the last.
[[nodiscard]] const char* statusLine(int i);
// The poses: how many, and each one's row on the page (its name and how well it fitted).
[[nodiscard]] int stepCount();
[[nodiscard]] const char* stepRow(int i);
[[nodiscard]] const char* stepHelp(int i);
[[nodiscard]] bool stepUsed(int i); // (seated: the first isn't)

// The arms' settings (vr_cvars.inc): the measurements with the tweaks on top. Uncalibrated, the tweaks go on the default
// body: the model's arms times Arm Length, its shoulders 4 cm in front of the chest, rising 25 and swinging 20 degrees.
// Whether both arms' lengths are measured (the calibrated arm: the shoulders rise from lower down and reach first).
[[nodiscard]] bool calibrated();
// A bone's measured length, real cm (0: not measured, the model's times Arm Length instead): 0 the upper arm (the
// shoulder joint to the elbow), 1 the forearm (the elbow to the drawn hand's wrist); and its tweak, cm.
[[nodiscard]] float measuredArmCm(int bone);
[[nodiscard]] float armTweakCm(int bone);
// The shoulder joints from the model's, metres: back, up, out.
[[nodiscard]] glm::vec3 shoulderOffset();
// Degrees the shoulders rise reaching up and swing forward reaching far forward.
[[nodiscard]] float shoulderRise();
[[nodiscard]] float shoulderSwing();
// Whether a tweak isn't 0; resetTweaks sets them all to 0 (as measured, or the default body).
[[nodiscard]] bool tweaked();
void resetTweaks();
// Arms and Pauldrons: what is measured, a line at a time (null past the last).
[[nodiscard]] const char* measuredLine(int i);
// Round 21's settings (vr_body_upper_arm, _forearm, _shoulders_*, _shoulder_up, _forward: absolute, Apply wrote over
// them), set by a config or in the console, moved to the measurements and tweaks with the same look; an Undo saved with
// them too. After the saved config (vr_migrate_config) and once a frame.
void migrate();

} // namespace qvr::bodycal

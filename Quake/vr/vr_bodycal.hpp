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
// (vr_body_shoulders_back, _up, _out), the reach from the shoulder joint to the drawn wrist and its split into the upper
// arm and the forearm (vr_body_upper_arm, vr_body_forearm, real centimetres: the circles and poses give the reach, the
// forearm's sweep about the elbow its length and the elbow's place), and how far the shoulders rise and swing
// (vr_body_shoulder_up, _forward). The wrists' bends check the hand calibration: how far the drawn hand's wrist is from
// the real wrist they turn about. The drawn wrist is the empty hand's on the calibrated controller (Hand Calibration's),
// whatever the hand holds.
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

} // namespace qvr::bodycal

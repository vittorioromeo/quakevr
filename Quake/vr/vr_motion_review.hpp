// vr_motion_review.hpp -- reviewing the motion takes (docs/vr-port/MOTIONS.md, "Reviewing failing takes"): which
// takes fail vr_motion_eval or are suspect, looked at in the game (VR Settings > Advanced VR Options > Review Takes),
// and kept, discarded or relabelled there.
//
// - The verdicts: vr_motion_eval writes each take's (verdict, reason, the expected and the registered events) into
//   eval_status.csv next to the takes (quakevr/motions/eval_status.csv), merged: an evaluation of some takes keeps the
//   others'. The takes themselves are never written by it.
// - The suspects: quakevr/motions/suspects.cfg (in git, like expect.cfg): takes whose label is doubtful, and why.
// - The decisions, in quakevr/motions/review/: reviewed.csv (the takes kept or relabelled), undo.csv (every
//   change, newest last: Undo Last takes back the last one, also after a restart), relabelled/ (each relabelled
//   take's original file, byte for byte). A discarded take goes into motions/discarded/, as the recorder's Delete
//   Last Take does; Restore brings it back.
// - Re-evaluate: the evaluation drives the mock headset (the take's tracking replaces the head's and the hands'),
//   so it never runs in the game being played: it runs in a second copy of the game, in the background (the mock
//   headset, its own small window, below normal priority, no sound, the config not written), which writes
//   eval_status.csv; the list reads it when that copy is done.
// - The ghost: a take replayed in front of the training dummy, drawn: its weapons (or empty hands) translucent where
//   the take had them relative to the dummy, the weapons' lines and striking points, the far end's trail, the head,
//   and the events (the take's own, live, where they happened; the evaluation's replay, over the dummy). Nothing
//   is driven: the player's own tracking, body and weapons are untouched.

#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::motion::review
{

void init();        // commands (vr_motion_review ...)
void frame();       // VR_BeginFrame (motion::frame): the ghost's drawing, the re-evaluation's progress
void serverFrame(); // VR_ServerFrameEnd (motion::serverFrame): where the ghost's target is
void invalidate();  // the takes changed (a take saved or deleted by the recorder): the list is made again

// One take's verdict (vr_motion_eval's table row), for eval_status.csv.
struct Verdict
{
    za::String path; // the take's file (its folder gets the eval_status.csv)
    za::String label, weapons, expected, verdict, reason, events, recorded, same;
    int frames{0};
    double handError{-1.0};
};
// vr_motion_eval's results, merged into eval_status.csv in each take's folder.
void recordEval(const za::Vector<Verdict>& verdicts);

// ---- The menu (vr_menu.cpp) ----

// Changes when the list or the selected take changes: the pages are built again.
[[nodiscard]] int generation();
[[nodiscard]] int rowCount();
[[nodiscard]] const char* rowText(int row); // 40 columns: verdict, suspect mark, label, time, reviewed mark
[[nodiscard]] const char* rowHelp(int row); // expected, got, why
void pick(int row);                         // the take of that row is the one the Take page shows

[[nodiscard]] const char* summary();      // "474 takes: 54 failing, 39 suspect"
[[nodiscard]] const char* reviewedLine(); // "3 reviewed (2 kept, 1 relabelled), 1 discarded"
[[nodiscard]] const char* evalLine();  // when they were evaluated, or the re-evaluation's progress
[[nodiscard]] const char* listTitle(); // "Failing or Suspect, Slash: 12"
[[nodiscard]] const char* lastAction();
[[nodiscard]] const char* detailLine(int line); // the picked take's details, a line of 40 columns ("" past the end)
constexpr int detailLines = 16;

void playGhost();
void stopGhost();
void replayMock(); // vr_motion_play watch (the mock headset only: it drives the tracking)
void keep();       // marks it reviewed (again: unmarks it)
void discard();    // into motions/discarded/
void restore();    // back from motions/discarded/
void relabel();    // as vr_motion_relabel_category / _detail
void undo();
void nextTake();
void previousTake();
void reevaluateTake();
void reevaluateShown();
void stopReevaluation();

} // namespace qvr::motion::review

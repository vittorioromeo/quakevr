# Round 26: from 2026-11-24, towards 1.2

A per-feature reference: current behaviour only. Round 25 (2026-11-12 to 11-24, 12,400 lines) is history: `ROUND25.md`;
earlier: `ROUND24.md` to `ROUND22.md`. Grep them; never read one whole.

Open work and bugs: BACKLOG.md (section 1, headed "Open on 2026-11-24", names the ROUND25.md sections that hold the notes).

## Adding a section

One short section per feature, appended at the end (never rewrite others' sections; the file merges as `union`):
a heading "## <what it does> (branch agent/<name>, <date>)", then what it does, settings and defaults, files, tests with their
numbers, open issues. Fold a follow-up into its feature's section instead of adding a new one. Start ROUND27.md when
this file passes about 2,000 lines.

## embedtests: VR embed test 24 cases 2/3 aimed by the axe's edge as held (agent/embedtests, 2026-11-25)
- Test 24 (`vr_embed_test 24`) `VR_EH_Blow` swing: for an axe the way "across" was the level side of the blow (laid out for axe pitch 0). Now it is the
  hand's forward turned square to the blade's line, reversed (`VRGetHandRot`: the edge faces so with the shipped Weapon Offset pitch 15.5, found by
  `vr_debug_axestick` leads). Test-side only (QC/vr_embed_test.qc); swords unchanged.
- Numbers: 24 before 16/3 (cases 2 and 3 fail, plus the known flaky "eased"); after 21/1 over 2 runs: cases 2 and 3 pass; the one left is the gentle
  tug in case 3 (hand reaches its goal 30 cm out, the blade is still held at t 4 s, `vr_hold_tugv` 0): case 3 never got that far before. Open.
- Test 33 (embedgrip) read 15/0 in 2 runs on this tree (case 46 passed): not reproduced here; no change made. Test 52 (embeddual) 2/2 fail "case 1
  swing stays in" with no change of mine in its path (it does not use VR_EH_Blow): not investigated.

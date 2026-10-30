// vr_tunables.inc -- Quake VR's tuning constants: QVR_TUNABLE(name, default) (vr_cvars.hpp, "Tuning constants").
// Numbers tuned once that no player sets: no menu row, not in the config. Constexpr in a release build (read as
// name.value, name.string, or by name from the QC); an archived cvar in a tuning build (MSBuild -p:QvrTuning=1). A player
// setting is a QVR_CVAR in vr_cvars.inc; to make one of these a setting again, move its line back there and give it a
// menu row. The list per page and why: ROUND23.md, "Settings tiers".

#ifdef QVR_TESTS
// vr_hosterror_test.qc -- the QuakeC side of vr_hosterror_test (Quake/vr/vr_hosterror.cpp): a function the engine's
// C++ calls inside the player's turn in its own time, which fails with a QuakeC error (PR_RunError, "assignment to
// world entity"): Host_Error's longjmp out of the C++ frames, whose open state VR_OnHostError puts back.
void() VR_HostErrorTest_Raise =
{
    entity e = world;
    dprint("vr_hosterror_test: QuakeC writes a field of the world: a QuakeC error\n");
    e.frags = e.frags + 1;
};
#endif

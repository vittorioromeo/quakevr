#ifdef QVR_TESTS
// vr_testutil.qc -- the QC tests' shared tally (CODE_STYLE.md, "Canonical helpers"; TESTING.md). A test starts with
// VR_Test_Begin("<tag>"), counts each check with VR_Test_Check(ok, what) (a `<tag>: PASS|FAIL <what>` line) and ends
// with its verdict line (VR_Test_Verdict: `<tag>: <p> passed, <f> failed`, the form run.sh -QuitOn and
// run_baseline.py read). One test runs at a time: the tally is the running test's.

string vr_test_tag;  // the running test's line prefix ("nailtest")
float vr_test_pass;  // its checks passed so far
float vr_test_fail;  // and failed

// A test (or one of its parts) starts: its tag, the tally cleared.
void(string tag) VR_Test_Begin =
{
    vr_test_tag = tag;
    vr_test_pass = 0;
    vr_test_fail = 0;
};

// One check: counted, and its line printed (developer 1).
void(float ok, string what) VR_Test_Check =
{
    if(ok) vr_test_pass++; else vr_test_fail++;
    dprint(sprintf("%s: %s %s\n", vr_test_tag, ok ? "PASS" : "FAIL", what));
};

// The test's verdict line.
void() VR_Test_Verdict =
{
    dprint(sprintf("%s: %g passed, %g failed\n", vr_test_tag, vr_test_pass, vr_test_fail));
};
#endif

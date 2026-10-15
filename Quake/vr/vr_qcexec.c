// vr_qcexec.c -- Quake VR's QuakeC interpreter loop (docs/vr-port/QC_VM.md). Included by pr_exec.c (it uses that
// file's statics: PR_EnterFunction, PR_LeaveFunction, PR_PrintStatement, the vr_qcprofile state), not compiled alone.
// PR_ExecuteProgramRun calls QCExec_Run first; vr_qcexec 0 keeps the original loop (pr_exec.c), 1 (the default) runs
// vr_qcexec_loop.inc with computed-goto dispatch, 2 the same loop with a switch (the comparison, and the fallback of a
// compiler without computed goto). "vr_qcexec_bench" times a QuakeC function in each and checks they agree.

cvar_t vr_qcexec = {"vr_qcexec", "1", CVAR_ARCHIVE};

#define QX_LIMIT 0x1000000 // the runaway limit: statements per outermost call (the original's "++profile > 0x1000000")

#if defined(__clang__) || defined(__GNUC__)
#define QX_HAVE_GOTO 1
#define QX_NOINLINE __attribute__((noinline))
#define QX_COLD __attribute__((noinline, cold))
#else
#define QX_HAVE_GOTO 0
#define QX_NOINLINE
#define QX_COLD
#endif

static int qcexec_force = -1; // vr_qcexec_bench: the loop to use (0 the original, 1 goto, 2 switch), -1 the cvar's

// Every opcode, in opcode_t's order (the dispatch tables).
#define QX_OPS(X) \
	X(OP_DONE) X(OP_MUL_F) X(OP_MUL_V) X(OP_MUL_FV) X(OP_MUL_VF) X(OP_DIV_F) X(OP_ADD_F) X(OP_ADD_V) X(OP_SUB_F) \
	X(OP_SUB_V) X(OP_EQ_F) X(OP_EQ_V) X(OP_EQ_S) X(OP_EQ_E) X(OP_EQ_FNC) X(OP_NE_F) X(OP_NE_V) X(OP_NE_S) X(OP_NE_E) \
	X(OP_NE_FNC) X(OP_LE) X(OP_GE) X(OP_LT) X(OP_GT) X(OP_LOAD_F) X(OP_LOAD_V) X(OP_LOAD_S) X(OP_LOAD_ENT) \
	X(OP_LOAD_FLD) X(OP_LOAD_FNC) X(OP_ADDRESS) X(OP_STORE_F) X(OP_STORE_V) X(OP_STORE_S) X(OP_STORE_ENT) \
	X(OP_STORE_FLD) X(OP_STORE_FNC) X(OP_STOREP_F) X(OP_STOREP_V) X(OP_STOREP_S) X(OP_STOREP_ENT) X(OP_STOREP_FLD) \
	X(OP_STOREP_FNC) X(OP_RETURN) X(OP_NOT_F) X(OP_NOT_V) X(OP_NOT_S) X(OP_NOT_ENT) X(OP_NOT_FNC) X(OP_IF) \
	X(OP_IFNOT) X(OP_CALL0) X(OP_CALL1) X(OP_CALL2) X(OP_CALL3) X(OP_CALL4) X(OP_CALL5) X(OP_CALL6) X(OP_CALL7) \
	X(OP_CALL8) X(OP_STATE) X(OP_GOTO) X(OP_AND) X(OP_OR) X(OP_BITAND) X(OP_BITOR)

// The runaway error, raised where the loop finds the count past the limit. Since the count only changes meaning where
// control jumps (and is checked there and before anything leaving the VM), the statement that crossed the limit is
// in the current straight run: the one whose count is QX_LIMIT + 1, as the original's per-statement check names.
static QX_COLD FUNC_NORETURN void QCExec_Runaway (int off)
{
	qcvm->xstatement = QX_LIMIT + 1 - off;
	PR_RunError("runaway loop error");
}

// A builtin's call, as the original loop makes it (its checks, Tracy zone, vr_qcprofile and vr_profile_fine timing).
static QX_NOINLINE void QCExec_Builtin (dfunction_t *newf)
{
	int i = -newf->first_statement;
	if (i >= qcvm->numbuiltins)
		PR_RunError("Bad builtin call number %d", i);
	PR_CheckBuiltinExtension (newf);
	VR_TracyQC(VR_TracyQCEnabled() ? PR_GetString(newf->s_name) : "", VR_TracyQCEnabled() ? PR_GetString(newf->s_file) : "", 1);
	if (qcprof.active) // QVR: vr_qcprofile: the builtin's own time (QuakeC it calls out), in its caller's self too
	{
		const unsigned long long child0 = qcprof.stack[qcvm->depth].child, t0 = __rdtsc ();
		qcvm->builtins[i]();
		{
			const unsigned long long dt = __rdtsc () - t0;
			const int fn = (int)(newf - qcvm->functions);
			const unsigned long long self = dt - (qcprof.stack[qcvm->depth].child - child0);
			qcprof.self[fn] += self;
			qcprof.incl[fn] += dt;
			qcprof.calls[fn]++;
			QCProf_Pair ((int)(qcvm->xfunction - qcvm->functions), fn, self);
		}
	}
	else if (vr_profile_fine) // QVR: vr_profile_detail 2 times each builtin call (QuakeC it calls is timed apart)
	{
		int inqc = vr_profile_inqc;
		vr_profile_inqc = 0;
		VR_ProfileBegin ("qc builtin");
		qcvm->builtins[i]();
		VR_ProfileEnd ();
		vr_profile_inqc = inqc;
	}
	else
		qcvm->builtins[i]();
	VR_TracyQCEnd();
}

#if QX_HAVE_GOTO
#define QX_GOTO 1
#define QX_FUNC QCExec_RunGoto
#include "vr_qcexec_loop.inc"
#undef QX_GOTO
#undef QX_FUNC
#endif

#define QX_GOTO 0
#define QX_FUNC QCExec_RunSwitch
#include "vr_qcexec_loop.inc"
#undef QX_GOTO
#undef QX_FUNC

// PR_ExecuteProgramRun's hook: true when this file's loop ran the call.
static qboolean QCExec_Run (func_t fnum)
{
	const int mode = qcexec_force >= 0 ? qcexec_force : (int)vr_qcexec.value;
	if (mode <= 0)
		return false;
#if QX_HAVE_GOTO
	if (mode == 1)
	{
		QCExec_RunGoto (fnum);
		return true;
	}
#endif
	QCExec_RunSwitch (fnum);
	return true;
}

dfunction_t *VR_ED_FindFunction (const char *fn_name); // (pr_edict.c)

static double QCExec_TotalProfile (void)
{
	double total = 0;
	int i;
	for (i = 0; i < qcvm->progs->numfunctions; i++)
		total += qcvm->functions[i].profile;
	return total;
}

// vr_qcexec_bench [function] [calls] [args...]: times a server QuakeC function (float arguments; it should have no
// lasting effects: the default, AD's math library's pow, loops over arithmetic) in each loop, rounds alternated, the
// best round each; checks every loop returns the same bits and counts the same statements.
static void QCExec_Bench_f (void)
{
	const char *name = Cmd_Argc () > 1 ? Cmd_Argv (1) : "ad_mathlib_pow";
	const int calls = Cmd_Argc () > 2 ? q_max (1, Q_atoi (Cmd_Argv (2))) : 20000;
	float args[8] = {1.0001f, 2500.5f, 0, 0, 0, 0, 0, 0};
	int nargs = Cmd_Argc () > 3 ? q_min (8, Cmd_Argc () - 3) : 2;
	double best[3] = {1e30, 1e30, 1e30}, stmts[3] = {0, 0, 0};
	int ret[3][3], i, k, round, mode, equal = 1;
	dfunction_t *f;

	if (!sv.active)
	{
		Con_Printf ("vr_qcexec_bench: no server running (load a map)\n");
		return;
	}
	for (i = 0; i < nargs && Cmd_Argc () > 3; i++)
		args[i] = Q_atof (Cmd_Argv (3 + i));
	PR_SwitchQCVM (&sv.qcvm);
	f = VR_ED_FindFunction (name);
	if (!f || f->first_statement < 0)
	{
		Con_Printf ("vr_qcexec_bench: no QuakeC function %s\n", name);
		PR_SwitchQCVM (NULL);
		return;
	}
	for (i = 0; i < nargs; i++) // a first call apart (a function setting up a global the first time counts more)
		G_FLOAT(OFS_PARM0 + 3 * i) = args[i];
	PR_ExecuteProgramRun ((func_t)(f - qcvm->functions));
	for (round = 0; round < 7; round++)
	{
		for (mode = 0; mode < 3; mode++)
		{
			const double p0 = QCExec_TotalProfile ();
			double t0, t;
			qcexec_force = mode;
			t0 = Sys_DoubleTime ();
			for (k = 0; k < calls; k++)
			{
				for (i = 0; i < nargs; i++)
					G_FLOAT(OFS_PARM0 + 3 * i) = args[i];
				PR_ExecuteProgramRun ((func_t)(f - qcvm->functions));
			}
			t = Sys_DoubleTime () - t0;
			qcexec_force = -1;
			best[mode] = q_min (best[mode], t);
			stmts[mode] = (QCExec_TotalProfile () - p0) / calls;
			for (i = 0; i < 3; i++)
				ret[mode][i] = ((int *)qcvm->globals)[OFS_RETURN + i];
		}
		for (mode = 1; mode < 3; mode++)
			equal &= !memcmp (ret[mode], ret[0], sizeof (ret[0])) && stmts[mode] == stmts[0];
	}
	Con_Printf ("vr_qcexec_bench: %s x%d, %.0f statements a call: original %.1f ns/call, goto %.1f (%.2fx), switch %.1f "
		"(%.2fx); results and counts %s\n", name, calls, stmts[0], best[0] * 1e9 / calls, best[1] * 1e9 / calls,
		best[0] / best[1], best[2] * 1e9 / calls, best[0] / best[2], equal ? "equal" : "DIFFER");
	if (!equal)
		for (mode = 0; mode < 3; mode++)
			Con_Printf ("vr_qcexec_bench: loop %d: %.3f statements a call, returned %08x %08x %08x\n", mode, stmts[mode],
				ret[mode][0], ret[mode][1], ret[mode][2]);
	PR_SwitchQCVM (NULL);
}

// vr_qcexec_run's trace: fabs's builtin swapped, once, for one that turns tracing on after it (as traceon would).
static builtin_t qcexec_fabs;
static int qcexec_fabs_num;

static void QCExec_FabsThenTrace (void)
{
	qcvm->builtins[qcexec_fabs_num] = qcexec_fabs;
	qcexec_fabs ();
	qcvm->trace = true;
}

// vr_qcexec_run [trace] <function> [args...]: one call of a server QuakeC function (float arguments) in vr_qcexec's
// loop: what it returned and the statements it ran. "trace": tracing (each statement printed) from its first fabs()
// on. For comparing the loops' results, errors (runaway: ad_mathlib_fast_pow 2 1e39) and traces.
static void QCExec_Run_f (void)
{
	int a = 1, i, nargs;
	qboolean trace = false;
	dfunction_t *f;
	double p0;

	if (Cmd_Argc () > a && !strcmp (Cmd_Argv (a), "trace"))
		trace = true, a++;
	if (Cmd_Argc () <= a || !sv.active)
	{
		Con_Printf ("vr_qcexec_run [trace] <function> [args...] (in a map)\n");
		return;
	}
	PR_SwitchQCVM (&sv.qcvm);
	f = VR_ED_FindFunction (Cmd_Argv (a));
	if (!f || f->first_statement < 0)
	{
		Con_Printf ("vr_qcexec_run: no QuakeC function %s\n", Cmd_Argv (a));
		PR_SwitchQCVM (NULL);
		return;
	}
	nargs = q_min (8, Cmd_Argc () - a - 1);
	for (i = 0; i < nargs; i++)
		G_FLOAT(OFS_PARM0 + 3 * i) = Q_atof (Cmd_Argv (a + 1 + i));
	if (trace)
		for (i = 0; i < pr_numbuiltindefs; i++)
			if (!strcmp (pr_builtindefs[i].name, "fabs") && pr_builtindefs[i].number > 0 &&
				pr_builtindefs[i].number < qcvm->numbuiltins)
			{
				qcexec_fabs_num = pr_builtindefs[i].number;
				qcexec_fabs = qcvm->builtins[qcexec_fabs_num];
				qcvm->builtins[qcexec_fabs_num] = QCExec_FabsThenTrace;
				break;
			}
	p0 = QCExec_TotalProfile ();
	PR_ExecuteProgramRun ((func_t)(f - qcvm->functions));
	if (trace && qcvm->builtins[qcexec_fabs_num] == QCExec_FabsThenTrace)
		qcvm->builtins[qcexec_fabs_num] = qcexec_fabs; // (it never called fabs)
	qcvm->trace = false;
	Con_Printf ("vr_qcexec_run: loop %d: %s returned %g %g %g (%08x %08x %08x), %.0f statements\n", (int)vr_qcexec.value,
		Cmd_Argv (a), qcvm->globals[OFS_RETURN], qcvm->globals[OFS_RETURN + 1], qcvm->globals[OFS_RETURN + 2],
		((int *)qcvm->globals)[OFS_RETURN], ((int *)qcvm->globals)[OFS_RETURN + 1],
		((int *)qcvm->globals)[OFS_RETURN + 2], QCExec_TotalProfile () - p0);
	PR_SwitchQCVM (NULL);
}

void VR_QCExec_Init (void)
{
	Cvar_RegisterVariable (&vr_qcexec);
	Cmd_AddCommand ("vr_qcexec_bench", QCExec_Bench_f);
	Cmd_AddCommand ("vr_qcexec_run", QCExec_Run_f);
}

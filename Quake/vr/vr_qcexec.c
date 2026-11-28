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

// The combined instructions (vr_qcexec_fuse; qcconv 2026-11-28; QC_VM.md "Combined instructions"): X(name, first,
// second) runs a statement whose opcode is "first" (its body: vr_qcexec_loop.inc's QX_DO_<first>) and goes straight
// to the handler of the next statement's "second" without a dispatch. The pairs are the most frequent consecutive
// opcodes of the hip1m1 fight (vr_qcexec_stats), loads and stores by their group (QCExec_Canon). QCExec_Fuse gives
// statement i the pair's opcode in a copy of the statements (vm->qx_fused) when i and i + 1 are such a pair: i + 1
// keeps its own opcode, so a jump to it runs it alone, and control only reaches i + 1 from i by falling through.
#define QX_FUSED(X) \
	X(AND_IFNOT, AND, OP_IFNOT) X(OR_IFNOT, OR, OP_IFNOT) X(NE_IFNOT, NE_F, OP_IFNOT) X(EQ_IFNOT, EQ_F, OP_IFNOT) \
	X(LT_IFNOT, LT, OP_IFNOT) X(GT_IFNOT, GT, OP_IFNOT) X(LE_IFNOT, LE, OP_IFNOT) X(GE_IFNOT, GE, OP_IFNOT) \
	X(EQE_IFNOT, EQ_E, OP_IFNOT) X(NEE_IFNOT, NE_E, OP_IFNOT) X(OR_IF, OR, OP_IF) X(AND_IF, AND, OP_IF) \
	X(AND_RETURN, AND, OP_RETURN) X(EQ_OR, EQ_F, OP_OR) X(OR_EQ, OR, OP_EQ_F) X(NOT_AND, NOT_F, OP_AND) \
	X(STORE_STORE, STORE_F, OP_STORE_F) X(STORE_CALL1, STORE_F, OP_CALL1) X(STORE_CALL2, STORE_F, OP_CALL2) \
	X(STORE_CALL3, STORE_F, OP_CALL3) X(STORE_NE, STORE_F, OP_NE_F) X(STOREV_STOREV, STORE_V, OP_STORE_V) \
	X(IFNOT_LOAD, IFNOT, OP_LOAD_F) X(IFNOT_STORE, IFNOT, OP_STORE_F) X(IFNOT_RETURN, IFNOT, OP_RETURN) \
	X(IF_LOAD, IF, OP_LOAD_F) X(LOAD_NE, LOAD_F, OP_NE_F) X(LOAD_NOT, LOAD_F, OP_NOT_F) X(LOAD_EQ, LOAD_F, OP_EQ_F) \
	X(LOAD_EQS, LOAD_F, OP_EQ_S) X(LOAD_LE, LOAD_F, OP_LE) X(LOAD_GT, LOAD_F, OP_GT) X(LOAD_LT, LOAD_F, OP_LT) \
	X(ADDRESS_STOREP, ADDRESS, OP_STOREP_F) X(ADDRESS_STOREPV, ADDRESS, OP_STOREP_V)

#define QX_FENUM(name, first, second) QXF_##name,
enum { QXF_BEFORE_ = OP_BITOR, QX_FUSED (QX_FENUM) QXF_COUNT };
#undef QX_FENUM
#define QX_FUSE 0 // (the loops without them; QCExec_RunGotoFused is the one with QX_FUSE 1)

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
	VR_TracyQC(newf);
	if (qcprof.active) // QVR: vr_qcprofile: the builtin's own time (QuakeC it calls out), in its caller's self too
	{
		const unsigned long long child0 = qcprof.stack[qcvm->depth].child, t0 = QCPROF_TICKS ();
		qcvm->builtins[i]();
		{
			const unsigned long long dt = QCPROF_TICKS () - t0;
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

// The loop's own entry into a QuakeC function and return from it (QX_FASTCALL; qcvm 2026-11-28): PR_EnterFunction's
// and PR_LeaveFunction's work with the VM in a register (their every qcvm-> is a reload: their stores through the
// globals may alias it), the locals saved and put back by memcpy, each parameter copied in one go. The same state
// after, in the same order. Anything off the common path goes to those functions as it is: vr_qcprofile (latched per
// outermost call: the same for a call and its return), a Tracy build, a stack at either end (their errors).
#if defined(QVR_TRACY)
#define QX_FAST_OK() 0
#else
#define QX_FAST_OK() (!qcprof.active)
#endif

// n ints from s to d: a function's locals, saved on entry and put back on return (qcconv: most functions have a few,
// and memcpy with a variable size is a library call, whose register spills cost more than the copy).
static inline void QCExec_CopyInts (int *d, const int *s, int n)
{
	switch (n)
	{
	case 8: d[7] = s[7]; // fall through
	case 7: d[6] = s[6]; // fall through
	case 6: d[5] = s[5]; // fall through
	case 5: d[4] = s[4]; // fall through
	case 4: d[3] = s[3]; // fall through
	case 3: d[2] = s[2]; // fall through
	case 2: d[1] = s[1]; // fall through
	case 1: d[0] = s[0]; // fall through
	case 0: break;
	default: memcpy (d, s, (size_t)n * sizeof (int));
	}
}

static inline int QCExec_Enter (qcvm_t *vm, dfunction_t *f)
{
	int *const g = (int *)vm->globals;
	const int c = f->locals, depth = vm->depth, used = vm->localstack_used, np = f->numparms;
	int i, o = f->parm_start;
	if (!QX_FAST_OK () || depth + 1 >= MAX_STACK_DEPTH || used + c > LOCALSTACK_SIZE)
		return PR_EnterFunction (f);
	vm->stack[depth].s = vm->xstatement;
	vm->stack[depth].f = vm->xfunction;
	vm->depth = depth + 1;
	QCExec_CopyInts (vm->localstack + used, g + o, c); // save off the locals the function steps on
	vm->localstack_used = used + c;
	for (i = 0; i < np; i++) // copy the parameters (OFS_PARM0's slots are 3 apart; the locals far above them)
	{
		const int n = f->parm_size[i];
		const int *const src = g + OFS_PARM0 + i * 3;
		if (n == 1)
			g[o] = src[0];
		else if (n == 3)
			g[o] = src[0], g[o + 1] = src[1], g[o + 2] = src[2];
		else
			memcpy (g + o, src, (size_t)n * sizeof (int));
		o += n;
	}
	vm->xfunction = f;
	return f->first_statement - 1; // offset the s++
}

static inline int QCExec_Leave (qcvm_t *vm)
{
	dfunction_t *const f = vm->xfunction;
	const int depth = vm->depth, used = vm->localstack_used - f->locals;
	if (!QX_FAST_OK () || depth <= 0 || used < 0)
		return PR_LeaveFunction ();
	vm->localstack_used = used;
	QCExec_CopyInts ((int *)vm->globals + f->parm_start, vm->localstack + used, f->locals);
	vm->depth = depth - 1;
	vm->xfunction = vm->stack[depth - 1].f;
	return vm->stack[depth - 1].s;
}
#undef QX_FAST_OK

// vr_qcexec_stats (qcconv 2026-11-28): the server VM's dynamic opcode counts, the counts of each pair and triple of
// opcodes run one after the other at consecutive statements (the candidates for a combined instruction: a fused pair
// runs only when control falls through), and the calls (QuakeC, builtins by number, by argument count). While on, the
// server's QuakeC runs in QCExec_RunStats (the switch loop with a count at each dispatch: same behaviour, slower).
#define QX_NOPS (OP_BITOR + 1)
#define QX_STATS 0 // the loops below count nothing; QCExec_RunStats is the one with QX_STATS 1
static struct
{
	int on, previdx, prevadj;
	unsigned prev, prev2;
	unsigned long long ops[QX_NOPS], pairs[QX_NOPS][QX_NOPS], builtins[MAX_BUILTINS], qccalls, bcalls, argc[9], entries, locals[4], saved;
	int prevfused; // the last statement ran as a combined instruction's first (vr_qcexec_fuse 1 would have)
	unsigned long long *triples; // QX_NOPS^3 (malloc'd by "start")
} qcstats;

static void QCExec_Stat (int idx, unsigned op)
{
	const int adj = idx == qcstats.previdx + 1;
	if (op >= QX_NOPS)
		return;
	qcstats.ops[op]++;
	if (adj)
	{
		qcstats.pairs[qcstats.prev][op]++;
		if (qcstats.prevadj)
			qcstats.triples[((size_t)qcstats.prev2 * QX_NOPS + qcstats.prev) * QX_NOPS + op]++;
	}
	qcstats.prevadj = adj;
	if (adj && qcstats.prevfused) // the dispatches the combined instructions save (each pair's second)
		qcstats.saved++, qcstats.prevfused = 0;
	else
		qcstats.prevfused = sv.qcvm.qx_fused && qcvm == &sv.qcvm && sv.qcvm.qx_fused[idx].op > OP_BITOR;
	qcstats.prev2 = qcstats.prev;
	qcstats.prev = op;
	qcstats.previdx = idx;
}

static void QCExec_StatCall (dfunction_t *newf, int argc)
{
	if (newf->first_statement < 0)
	{
		const int b = -newf->first_statement;
		qcstats.bcalls++;
		if (b < MAX_BUILTINS)
			qcstats.builtins[b]++;
	}
	else
		qcstats.qccalls++, qcstats.locals[newf->locals <= 8 ? 0 : newf->locals <= 16 ? 1 : newf->locals <= 32 ? 2 : 3]++;
	if ((unsigned)argc < 9)
		qcstats.argc[argc]++;
}

// vr_qcexec_fuse 1 (the default): vr_qcexec 1 runs the combined instructions (QX_FUSED); 0 without them (the A/B).
cvar_t vr_qcexec_fuse = {"vr_qcexec_fuse", "1", CVAR_ARCHIVE};

static unsigned short qcexec_fusemap[QX_NOPS][QX_NOPS]; // [first][second] (by group): the pair's opcode, 0 none

// An opcode's group: the loads, stores and pointer stores of one int each run the same code.
static unsigned QCExec_Canon (unsigned op)
{
	switch (op)
	{
	case OP_STORE_ENT: case OP_STORE_FLD: case OP_STORE_S: case OP_STORE_FNC: return OP_STORE_F;
	case OP_LOAD_FLD: case OP_LOAD_ENT: case OP_LOAD_S: case OP_LOAD_FNC: return OP_LOAD_F;
	case OP_STOREP_ENT: case OP_STOREP_FLD: case OP_STOREP_S: case OP_STOREP_FNC: return OP_STOREP_F;
	default: return op;
	}
}

static void QCExec_FuseInit (void)
{
#define QX_FMAP(name, first, second) qcexec_fusemap[OP_##first][second] = QXF_##name;
	QX_FUSED (QX_FMAP)
#undef QX_FMAP
}

// vm->qx_fused: a copy of the statements, each statement that starts a pair (QX_FUSED) given the pair's opcode.
static void QCExec_Fuse (qcvm_t *vm)
{
	const int n = vm->progs->numstatements;
	int i, fused = 0;
	dstatement_t *const fs = (dstatement_t *)malloc ((size_t)q_max (n, 1) * sizeof (dstatement_t));
	if (!fs)
		Sys_Error ("vr_qcexec_fuse: out of memory (%d statements)", n);
	memcpy (fs, vm->statements, (size_t)n * sizeof (dstatement_t));
	for (i = 0; i + 1 < n; i++)
	{
		const unsigned a = vm->statements[i].op, b = vm->statements[i + 1].op;
		if (a < QX_NOPS && b < QX_NOPS && qcexec_fusemap[QCExec_Canon (a)][QCExec_Canon (b)])
		{
			fs[i].op = qcexec_fusemap[QCExec_Canon (a)][QCExec_Canon (b)];
			fused++;
		}
	}
	vm->qx_fused = fs;
	Con_DPrintf ("vr_qcexec_fuse: %d of %d statements start a combined instruction (%s)\n", fused, n,
		vm == &sv.qcvm ? "server" : "client");
}

#if QX_HAVE_GOTO
#define QX_GOTO 1
#define QX_FASTCALL 1
#define QX_FUNC QCExec_RunGoto
#include "vr_qcexec_loop.inc"
#undef QX_FUNC
#undef QX_FUSE
#define QX_FUSE 1
#define QX_FUNC QCExec_RunGotoFused
#include "vr_qcexec_loop.inc"
#undef QX_FUSE
#define QX_FUSE 0
#undef QX_FASTCALL
#undef QX_FUNC
#define QX_FASTCALL 0
#define QX_FUNC QCExec_RunGotoOldCalls
#include "vr_qcexec_loop.inc"
#include "vr_api.h"
#undef QX_GOTO
#undef QX_FASTCALL
#undef QX_FUNC
#endif

#define QX_GOTO 0
#define QX_FASTCALL 1
#define QX_FUNC QCExec_RunSwitch
#include "vr_qcexec_loop.inc"
#undef QX_FUNC
#undef QX_STATS
#define QX_STATS 1
#define QX_FUNC QCExec_RunStats
#include "vr_qcexec_loop.inc"
#undef QX_STATS
#undef QX_GOTO
#undef QX_FASTCALL
#undef QX_FUNC

// PR_ExecuteProgramRun's hook: true when this file's loop ran the call. vr_qcexec 1 goto, 2 switch, 3 goto with the
// calls and returns through PR_EnterFunction/PR_LeaveFunction (the loop before QX_FASTCALL: the A/B).
static qboolean QCExec_Run (func_t fnum)
{
	const int mode = qcexec_force >= 0 ? qcexec_force : (int)vr_qcexec.value;
	if (mode <= 0)
		return false;
	if (qcstats.on && qcvm == &sv.qcvm)
	{
		qcstats.entries++; // (C calling QuakeC: each run of the loop, nested ones too)
		QCExec_RunStats (fnum);
		return true;
	}
#if QX_HAVE_GOTO
	if (mode == 4 || (mode == 1 && qcexec_force < 0 && vr_qcexec_fuse.value))
	{
		if (!qcvm->qx_fused)
			QCExec_Fuse (qcvm);
		QCExec_RunGotoFused (fnum);
		return true;
	}
	if (mode == 1)
	{
		QCExec_RunGoto (fnum);
		return true;
	}
	if (mode == 3)
	{
		QCExec_RunGotoOldCalls (fnum);
		return true;
	}
#endif
	QCExec_RunSwitch (fnum);
	return true;
}

// vr_qcexec_time [label|stop]: QuakeC's time a frame (every outermost call into a VM, builtins included: vr_profile's
// "quakec") and its statements a frame since the last call, printed and started again; the first call starts it,
// "stop" ends it. One clock read either side of each outermost call while on (the A/B of the loops, QC_VM.md).
static struct
{
	int on;
	unsigned long long ticks, tsc0;
	double time0, instr0;
	int frame0;
} qcexec_time;

static void QCExec_TimeReset (void)
{
	qcexec_time.ticks = 0;
	qcexec_time.tsc0 = QCPROF_TICKS ();
	qcexec_time.time0 = Sys_DoubleTime ();
	qcexec_time.instr0 = sv.qcvm.instr_total;
	qcexec_time.frame0 = host_framecount;
}

static void QCExec_Time_f (void)
{
	const char *label = Cmd_Argc () > 1 ? Cmd_Argv (1) : "";
	if (qcexec_time.on && strcmp (label, "stop") != 0)
	{
		const double secs = Sys_DoubleTime () - qcexec_time.time0;
		const unsigned long long ticks = QCPROF_TICKS () - qcexec_time.tsc0;
		const double tickms = ticks ? secs * 1000.0 / (double)ticks : 0.0;
		const int frames = q_max (1, host_framecount - qcexec_time.frame0);
		const double ms = qcexec_time.ticks * tickms / frames, stmts = (sv.qcvm.instr_total - qcexec_time.instr0) / frames;
		Con_Printf ("vr_qcexec_time %s: loop %d, %d frames, QuakeC %.4f ms a frame, %.0f statements a frame (%.2f ns "
			"each with builtins), %.3f ms a frame in all\n", label, (int)vr_qcexec.value, frames, ms, stmts,
			stmts > 0 ? ms * 1e6 / stmts : 0.0, secs * 1000.0 / frames);
	}
	qcexec_time.on = strcmp (label, "stop") != 0;
	QCExec_TimeReset ();
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
	double best[5] = {1e30, 1e30, 1e30, 1e30, 1e30}, stmts[5] = {0, 0, 0, 0, 0};
	int ret[5][3], i, k, round, mode, equal = 1;
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
		for (mode = 0; mode < 5; mode++)
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
		for (mode = 1; mode < 5; mode++)
			equal &= !memcmp (ret[mode], ret[0], sizeof (ret[0])) && stmts[mode] == stmts[0];
	}
	Con_Printf ("vr_qcexec_bench: %s x%d, %.0f statements a call: original %.1f ns/call, goto %.1f (%.2fx), switch %.1f "
		"(%.2fx), goto with the old calls %.1f (%.2fx), goto combined %.1f (%.2fx); results and counts %s\n", name, calls,
		stmts[0], best[0] * 1e9 / calls, best[1] * 1e9 / calls, best[0] / best[1], best[2] * 1e9 / calls,
		best[0] / best[2], best[3] * 1e9 / calls, best[0] / best[3], best[4] * 1e9 / calls, best[0] / best[4],
		equal ? "equal" : "DIFFER");
	if (!equal)
		for (mode = 0; mode < 5; mode++)
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
	int a = 1, i, nargs, swapped = 0;
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
				swapped = 1;
				break;
			}
	p0 = QCExec_TotalProfile ();
	PR_ExecuteProgramRun ((func_t)(f - qcvm->functions));
	if (swapped && qcvm->builtins[qcexec_fabs_num] == QCExec_FabsThenTrace)
		qcvm->builtins[qcexec_fabs_num] = qcexec_fabs; // (it never called fabs)
	qcvm->trace = false;
	Con_Printf ("vr_qcexec_run: loop %d: %s returned %g %g %g (%08x %08x %08x), %.0f statements\n", (int)vr_qcexec.value,
		Cmd_Argv (a), qcvm->globals[OFS_RETURN], qcvm->globals[OFS_RETURN + 1], qcvm->globals[OFS_RETURN + 2],
		((int *)qcvm->globals)[OFS_RETURN], ((int *)qcvm->globals)[OFS_RETURN + 1],
		((int *)qcvm->globals)[OFS_RETURN + 2], QCExec_TotalProfile () - p0);
	PR_SwitchQCVM (NULL);
}

#define QX_NAME(op) #op,
static const char *const qcexec_opnames[QX_NOPS] = { QX_OPS (QX_NAME) }; // (pr_opnames calls every load INDIRECT)
#undef QX_NAME

static const char *QCExec_OpName (unsigned op)
{
	return op < QX_NOPS ? qcexec_opnames[op] + 3 : "?"; // (less the "OP_")
}

// The index of the largest count above "below" (or equal to it at an index past "after"): the next in a descending
// walk without sorting (the top N of n counts).
static size_t QCExec_NextMax (const unsigned long long *v, size_t n, unsigned long long *last, size_t *lasti)
{
	size_t i, best = n;
	for (i = 0; i < n; i++)
		if ((v[i] < *last || (v[i] == *last && i > *lasti)) && v[i] && (best == n || v[i] > v[best]))
			best = i;
	if (best < n)
		*last = v[best], *lasti = best;
	return best;
}

static void QCExec_StatsDump (int top)
{
	unsigned long long total = 0, last, pairtotal = 0;
	size_t i, k, lasti;
	int f;
	for (i = 0; i < QX_NOPS; i++)
		total += qcstats.ops[i];
	for (i = 0; i < QX_NOPS * QX_NOPS; i++)
		pairtotal += (&qcstats.pairs[0][0])[i];
	Con_Printf ("qcstats: %llu statements, %llu fall-through pairs, %llu QuakeC calls, %llu builtin calls; calls by "
		"argument count:", total, pairtotal, qcstats.qccalls, qcstats.bcalls);
	Con_Printf (" (C calling QuakeC: %llu runs; QuakeC calls by locals <=8 %llu, 9-16 %llu, 17-32 %llu, more %llu; dispatches the combined instructions save %llu)", qcstats.entries, qcstats.locals[0], qcstats.locals[1], qcstats.locals[2], qcstats.locals[3], qcstats.saved);
	for (i = 0; i < 9; i++)
		Con_Printf (" %llu", qcstats.argc[i]);
	Con_Printf ("\n");
	if (!total)
		return;
	last = ~0ull, lasti = 0;
	for (k = 0; k < (size_t)top && (i = QCExec_NextMax (qcstats.ops, QX_NOPS, &last, &lasti)) < QX_NOPS; k++)
		Con_Printf ("qcstats op %2d %-12s %12llu %5.2f%%\n", (int)k + 1, QCExec_OpName ((unsigned)i), qcstats.ops[i],
			100.0 * qcstats.ops[i] / total);
	last = ~0ull, lasti = 0;
	for (k = 0; k < (size_t)top && (i = QCExec_NextMax (&qcstats.pairs[0][0], QX_NOPS * QX_NOPS, &last, &lasti)) <
		QX_NOPS * QX_NOPS; k++)
		Con_Printf ("qcstats pair %2d %-12s %-12s %12llu %5.2f%%\n", (int)k + 1, QCExec_OpName ((unsigned)(i / QX_NOPS)),
			QCExec_OpName ((unsigned)(i % QX_NOPS)), (&qcstats.pairs[0][0])[i], 100.0 * (&qcstats.pairs[0][0])[i] / total);
	last = ~0ull, lasti = 0;
	for (k = 0; k < (size_t)top && qcstats.triples &&
		(i = QCExec_NextMax (qcstats.triples, (size_t)QX_NOPS * QX_NOPS * QX_NOPS, &last, &lasti)) <
		(size_t)QX_NOPS * QX_NOPS * QX_NOPS; k++)
		Con_Printf ("qcstats triple %2d %-12s %-12s %-12s %12llu %5.2f%%\n", (int)k + 1,
			QCExec_OpName ((unsigned)(i / (QX_NOPS * QX_NOPS))), QCExec_OpName ((unsigned)(i / QX_NOPS % QX_NOPS)),
			QCExec_OpName ((unsigned)(i % QX_NOPS)), qcstats.triples[i], 100.0 * qcstats.triples[i] / total);
	last = ~0ull, lasti = 0;
	for (k = 0; k < (size_t)top && (i = QCExec_NextMax (qcstats.builtins, MAX_BUILTINS, &last, &lasti)) < MAX_BUILTINS;
		k++)
	{
		const char *name = "?";
		for (f = 0; f < sv.qcvm.progs->numfunctions; f++)
			if (sv.qcvm.functions[f].first_statement == -(int)i)
			{
				name = PR_GetString (sv.qcvm.functions[f].s_name);
				break;
			}
		Con_Printf ("qcstats builtin %2d #%-4d %-24s %10llu %5.2f%%\n", (int)k + 1, (int)i, name, qcstats.builtins[i],
			100.0 * qcstats.builtins[i] / q_max (1ull, qcstats.bcalls));
	}
}

// vr_qcexec_stats start|stop|dump [N]|hash: the server VM's opcode, pair and triple counts and calls (QCExec_Stat);
// "dump" prints the top N (20) of each; "hash" prints a hash of every function's statement count and the statements
// run in all (equal between loops and modes over a fixed-frame run: the identity check of QC_VM.md).
static void QCExec_Stats_f (void)
{
	const char *what = Cmd_Argc () > 1 ? Cmd_Argv (1) : "dump";
	if (!strcmp (what, "start"))
	{
		unsigned long long *const tri = qcstats.triples;
		memset (&qcstats, 0, sizeof (qcstats));
		qcstats.triples = tri ? tri : (unsigned long long *)malloc ((size_t)QX_NOPS * QX_NOPS * QX_NOPS * sizeof (*tri));
		if (!qcstats.triples)
			Sys_Error ("vr_qcexec_stats: out of memory");
		memset (qcstats.triples, 0, (size_t)QX_NOPS * QX_NOPS * QX_NOPS * sizeof (*tri));
		qcstats.previdx = -2;
		if (sv.active && !sv.qcvm.qx_fused)
			QCExec_Fuse (&sv.qcvm);
		qcstats.on = 1;
	}
	else if (!strcmp (what, "stop"))
		qcstats.on = 0;
	else if (!strcmp (what, "hash"))
	{
		unsigned long long h = 1469598103934665603ull;
		int i;
		if (!sv.active)
			return;
		for (i = 0; i < sv.qcvm.progs->numfunctions; i++)
			h = (h ^ (unsigned)sv.qcvm.functions[i].profile) * 1099511628211ull;
		h = (h ^ (unsigned long long)sv.qcvm.instr_total) * 1099511628211ull;
		Con_Printf ("vr_qcexec_stats hash: %016llx, %.0f statements in all, loop %d\n", h, sv.qcvm.instr_total,
			(int)vr_qcexec.value);
	}
	else if (sv.active)
	{
		PR_SwitchQCVM (&sv.qcvm); // (the builtins' names)
		QCExec_StatsDump (Cmd_Argc () > 2 ? q_max (1, Q_atoi (Cmd_Argv (2))) : 20);
		PR_SwitchQCVM (NULL);
	}
}

void VR_QCExec_Init (void)
{
	Cmd_AddCommand ("vr_qcexec_stats", QCExec_Stats_f);
	Cvar_RegisterVariable (&vr_qcexec_fuse);
	QCExec_FuseInit ();
	Cvar_RegisterVariable (&vr_qcexec);
	Cmd_AddCommand ("vr_qcexec_bench", QCExec_Bench_f);
	Cmd_AddCommand ("vr_qcexec_run", QCExec_Run_f);
	Cmd_AddCommand ("vr_qcexec_time", QCExec_Time_f);
}

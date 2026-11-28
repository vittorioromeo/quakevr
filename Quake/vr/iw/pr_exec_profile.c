// QVR: the per-function QuakeC profiler (vr_qcprofile, profile_qc).
// Moved as it was out of pr_exec.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
============
QVR: per-function QuakeC time (vr_qcprofile 1; "profile_qc [n]" prints the n costliest and resets, "profile_qc 0"
only resets). Server VM only. Each function's inclusive time (its callees in), its self time (its own statements and
the builtins it calls, its QuakeC callees out) and its calls; each builtin's self time and calls apart. TSC ticks,
converted with the wall clock between the reset and the print. Off: one test a call.
============
*/
#if defined(_WIN32)
#include <intrin.h>
#define QCPROF_TICKS() __rdtsc ()
#elif defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define QCPROF_TICKS() __rdtsc ()
#else // no TSC (ARM): the monotonic clock in nanoseconds (the print converts ticks by the wall clock all the same)
#include <time.h>
static inline unsigned long long QCProf_Ticks (void)
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}
#define QCPROF_TICKS() QCProf_Ticks ()
#endif

cvar_t vr_qcprofile = {"vr_qcprofile", "0", CVAR_NONE};

#define QCPROF_PAIRS 4096 // open addressing: a pair past a full table isn't counted

typedef struct
{
	unsigned long long t0;    // the call's start
	unsigned long long child; // its QuakeC callees' inclusive time (also through builtins)
} qcprof_frame_t;

typedef struct
{
	qcprof_frame_t stack[MAX_STACK_DEPTH + 1];
	unsigned long long *incl, *self, *calls; // [numfunctions]
	int num;
	int active;                    // latched by each outermost call into the server VM
	unsigned long long reset_tsc;
	double reset_time;
	int reset_frame;
	struct { unsigned key; unsigned long long self, calls; } pairs[QCPROF_PAIRS]; // (caller << 16 | builtin) -> its time
} qcprof_t;

static qcprof_t qcprof;

static void QCProf_Reset (void)
{
	if (qcprof.incl && qcprof.num)
	{
		memset (qcprof.incl, 0, sizeof (*qcprof.incl) * qcprof.num);
		memset (qcprof.self, 0, sizeof (*qcprof.self) * qcprof.num);
		memset (qcprof.calls, 0, sizeof (*qcprof.calls) * qcprof.num);
	}
	memset (qcprof.pairs, 0, sizeof (qcprof.pairs));
	qcprof.stack[0].child = 0;
	qcprof.reset_tsc = QCPROF_TICKS ();
	qcprof.reset_time = Sys_DoubleTime ();
	qcprof.reset_frame = host_framecount;
}

// At each outermost call: on for the server VM when vr_qcprofile is set (the arrays fit its progs).
static void QCProf_Latch (void)
{
	qcprof.active = 0;
	if (!vr_qcprofile.value || qcvm != &sv.qcvm || !qcvm->progs)
		return;
	if (qcprof.num != qcvm->progs->numfunctions)
	{
		free (qcprof.incl);
		free (qcprof.self);
		free (qcprof.calls);
		qcprof.num = qcvm->progs->numfunctions;
		qcprof.incl = (unsigned long long *) calloc (qcprof.num, sizeof (unsigned long long));
		qcprof.self = (unsigned long long *) calloc (qcprof.num, sizeof (unsigned long long));
		qcprof.calls = (unsigned long long *) calloc (qcprof.num, sizeof (unsigned long long));
		QCProf_Reset ();
	}
	qcprof.active = qcprof.incl != NULL;
}

// A builtin's name: the progs' (fteqcc's -O3 strips most of them), else the engine's table's.
static const char *QCProf_BuiltinName (const dfunction_t *f)
{
	const char *name = PR_GetString (f->s_name);
	int j;
	if (name && *name)
		return name;
	for (j = 0; j < pr_numbuiltindefs; j++)
		if (pr_builtindefs[j].number == -f->first_statement)
			return pr_builtindefs[j].name;
	return "?";
}

static void QCProf_Pair (int caller, int builtin, unsigned long long self)
{
	const unsigned key = ((unsigned) caller << 16 | (unsigned) builtin) + 1;
	unsigned h = (key * 2654435761u) & (QCPROF_PAIRS - 1), n;
	for (n = 0; n < QCPROF_PAIRS; n++, h = (h + 1) & (QCPROF_PAIRS - 1))
	{
		if (qcprof.pairs[h].key == key || !qcprof.pairs[h].key)
		{
			qcprof.pairs[h].key = key;
			qcprof.pairs[h].self += self;
			qcprof.pairs[h].calls++;
			return;
		}
	}
}

static int QCProf_PairCmp (const void *a, const void *b)
{
	unsigned long long x = qcprof.pairs[*(const int *) a].self, y = qcprof.pairs[*(const int *) b].self;
	return x < y ? 1 : x > y ? -1 : 0;
}

static unsigned long long *qcprof_sortkey;
static int QCProf_Cmp (const void *a, const void *b)
{
	unsigned long long x = qcprof_sortkey[*(const int *) a], y = qcprof_sortkey[*(const int *) b];
	return x < y ? 1 : x > y ? -1 : 0;
}

void PR_ProfileQC_f (void)
{
	int shown = Cmd_Argc () > 1 ? Q_atoi (Cmd_Argv (1)) : 15;
	int i, n, k, frames;
	int *order;
	double secs, tickms;
	unsigned long long ticks;

	if (!sv.active || !qcprof.incl || qcprof.num != sv.qcvm.progs->numfunctions)
	{
		Con_Printf ("profile_qc: nothing recorded (vr_qcprofile 1, then play)\n");
		QCProf_Reset ();
		return;
	}
	if (shown <= 0)
	{
		QCProf_Reset ();
		return;
	}
	ticks = QCPROF_TICKS () - qcprof.reset_tsc;
	secs = Sys_DoubleTime () - qcprof.reset_time;
	frames = host_framecount - qcprof.reset_frame;
	if (frames < 1)
		frames = 1;
	tickms = ticks ? secs * 1000.0 / (double) ticks : 0.0;
	PR_SwitchQCVM (&sv.qcvm);
	order = (int *) malloc (sizeof (int) * qcprof.num);
	Con_Printf ("profile_qc: %d frames, QuakeC %.3f ms/frame (whole calls from the engine)\n", frames,
		qcprof.stack[0].child * tickms / frames);
	Con_Printf ("profile_qc:  self_ms  incl_ms  calls/fr  function (self: own statements + builtins it calls)\n");
	for (n = 0, i = 1; i < qcprof.num; i++)
		if (qcprof.calls[i] && qcvm->functions[i].first_statement >= 0)
			order[n++] = i;
	qcprof_sortkey = qcprof.self;
	qsort (order, n, sizeof (int), QCProf_Cmp);
	for (k = 0; k < n && k < shown; k++)
	{
		i = order[k];
		Con_Printf ("profile_qc: %8.4f %8.4f %9.2f  %s\n", qcprof.self[i] * tickms / frames, qcprof.incl[i] * tickms / frames,
			(double) qcprof.calls[i] / frames, PR_GetString (qcvm->functions[i].s_name));
	}
	Con_Printf ("profile_qc: builtins: self_ms  calls/fr  name\n");
	for (n = 0, i = 1; i < qcprof.num; i++)
		if (qcprof.calls[i] && qcvm->functions[i].first_statement < 0)
			order[n++] = i;
	qsort (order, n, sizeof (int), QCProf_Cmp);
	for (k = 0; k < n && k < shown; k++)
	{
		i = order[k];
		Con_Printf ("profile_qc: b %8.4f %9.2f  %s\n", qcprof.self[i] * tickms / frames, (double) qcprof.calls[i] / frames,
			QCProf_BuiltinName (&qcvm->functions[i]));
	}
	Con_Printf ("profile_qc: callers' builtins: self_ms  calls/fr  caller > builtin\n");
	free (order);
	order = (int *) malloc (sizeof (int) * QCPROF_PAIRS);
	for (n = 0, i = 0; i < QCPROF_PAIRS; i++)
		if (qcprof.pairs[i].key)
			order[n++] = i;
	qsort (order, n, sizeof (int), QCProf_PairCmp);
	for (k = 0; k < n && k < shown; k++)
	{
		const unsigned key = qcprof.pairs[order[k]].key - 1;
		Con_Printf ("profile_qc: p %8.4f %9.2f  %s > %s\n", qcprof.pairs[order[k]].self * tickms / frames,
			(double) qcprof.pairs[order[k]].calls / frames, PR_GetString (qcvm->functions[key >> 16].s_name),
			QCProf_BuiltinName (&qcvm->functions[key & 0xffff]));
	}
	free (order);
	PR_SwitchQCVM (NULL);
	QCProf_Reset ();
}


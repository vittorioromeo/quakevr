/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"

static const char *const pr_opnames[] =
{
	"DONE",

	"MUL_F",
	"MUL_V",
	"MUL_FV",
	"MUL_VF",

	"DIV",

	"ADD_F",
	"ADD_V",

	"SUB_F",
	"SUB_V",

	"EQ_F",
	"EQ_V",
	"EQ_S",
	"EQ_E",
	"EQ_FNC",

	"NE_F",
	"NE_V",
	"NE_S",
	"NE_E",
	"NE_FNC",

	"LE",
	"GE",
	"LT",
	"GT",

	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",

	"ADDRESS",

	"STORE_F",
	"STORE_V",
	"STORE_S",
	"STORE_ENT",
	"STORE_FLD",
	"STORE_FNC",

	"STOREP_F",
	"STOREP_V",
	"STOREP_S",
	"STOREP_ENT",
	"STOREP_FLD",
	"STOREP_FNC",

	"RETURN",

	"NOT_F",
	"NOT_V",
	"NOT_S",
	"NOT_ENT",
	"NOT_FNC",

	"IF",
	"IFNOT",

	"CALL0",
	"CALL1",
	"CALL2",
	"CALL3",
	"CALL4",
	"CALL5",
	"CALL6",
	"CALL7",
	"CALL8",

	"STATE",

	"GOTO",

	"AND",
	"OR",

	"BITAND",
	"BITOR"
};

static const char *const pr_extnames[QCEXT_COUNT] =
{
	"STD_QC",

	#define QCEXTENSION(name) #name,
	QCEXTENSIONS_ALL
	#undef QCEXTENSION
};

/*
=================
PR_FindExtensionByName
=================
*/
int PR_FindExtensionByName (const char *name)
{
	int i;
	for (i = 1; i < QCEXT_COUNT; i++)
		if (!strcmp (name, pr_extnames[i]))
			return i;
	return 0;
}

const char *PR_GlobalString (int ofs);
const char *PR_GlobalStringNoContents (int ofs);


//=============================================================================

/*
=================
PR_PrintStatement
=================
*/
static void PR_PrintStatement (dstatement_t *s)
{
	int	i;

	if ((unsigned int)s->op < Q_COUNTOF(pr_opnames))
	{
		Con_Printf("%s ", pr_opnames[s->op]);
		i = strlen(pr_opnames[s->op]);
		for ( ; i < 10; i++)
			Con_Printf(" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT)
		Con_Printf("%sbranch %i", PR_GlobalString(s->a), s->b);
	else if (s->op == OP_GOTO)
	{
		Con_Printf("branch %i", s->a);
	}
	else if ((unsigned int)(s->op-OP_STORE_F) < 6)
	{
		Con_Printf("%s", PR_GlobalString(s->a));
		Con_Printf("%s", PR_GlobalStringNoContents(s->b));
	}
	else
	{
		if (s->a)
			Con_Printf("%s", PR_GlobalString(s->a));
		if (s->b)
			Con_Printf("%s", PR_GlobalString(s->b));
		if (s->c)
			Con_Printf("%s", PR_GlobalStringNoContents(s->c));
	}
	Con_Printf("\n");
}

/*
============
PR_StackTrace
============
*/
static void PR_StackTrace (void)
{
	int		i;
	dfunction_t	*f;

	if (qcvm->depth == 0)
	{
		Con_Printf("<NO STACK>\n");
		return;
	}

	qcvm->stack[qcvm->depth].f = qcvm->xfunction;
	for (i = qcvm->depth; i >= 0; i--)
	{
		f = qcvm->stack[i].f;
		if (!f)
		{
			Con_Printf("<NO FUNCTION>\n");
		}
		else
		{
			Con_Printf("%12s : %s\n", PR_GetString(f->s_file), PR_GetString(f->s_name));
		}
	}
}


/*
============
PR_Profile_f

============
*/
void PR_Profile_f (void)
{
	int		i, num, shown;
	double	total = 0;
	int		pmax;
	dfunction_t	*f, *best;

	if (!sv.active)
		return;

	shown = Cmd_Argc () > 1 ? Q_atoi (Cmd_Argv (1)) : 10; // QVR: "profile [n]" prints the n costliest (all are zeroed)

	PR_SwitchQCVM(&sv.qcvm);

	num = 0;
	do
	{
		pmax = 0;
		best = NULL;
		for (i = 0; i < qcvm->progs->numfunctions; i++)
		{
			f = &qcvm->functions[i];
			if (f->profile > pmax)
			{
				pmax = f->profile;
				best = f;
			}
		}
		if (best)
		{
			if (num < shown)
				Con_Printf("%7i %s\n", best->profile, PR_GetString(best->s_name));
			total += best->profile;
			num++;
			best->profile = 0;
		}
	} while (best);
	Con_Printf("%7.0f profile total (QC instructions since the last profile)\n", total); // QVR

	PR_SwitchQCVM(NULL);
}

/*
============
QVR: per-function QuakeC time (vr_qcprofile 1; "profile_qc [n]" prints the n costliest and resets, "profile_qc 0"
only resets). Server VM only. Each function's inclusive time (its callees in), its self time (its own statements and
the builtins it calls, its QuakeC callees out) and its calls; each builtin's self time and calls apart. TSC ticks,
converted with the wall clock between the reset and the print. Off: one test a call.
============
*/
#include <intrin.h>

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
	qcprof.reset_tsc = __rdtsc ();
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
	ticks = __rdtsc () - qcprof.reset_tsc;
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


/*
============
PR_RunError

Aborts the currently executing function
============
*/
void PR_RunError (const char *error, ...)
{
	va_list	argptr;
	char	string[1024];

	va_start (argptr, error);
	q_vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	PR_PrintStatement(qcvm->statements + qcvm->xstatement);
	PR_StackTrace();

	Con_Printf("%s\n", string);

	qcvm->depth = 0;	// dump the stack so host_error can shutdown functions

	Host_Error("Program error");
}

/*
====================
PR_EnterFunction

Returns the new program statement counter
====================
*/
static int PR_EnterFunction (dfunction_t *f)
{
	int	i, j, c, o;

	qcvm->stack[qcvm->depth].s = qcvm->xstatement;
	qcvm->stack[qcvm->depth].f = qcvm->xfunction;
	qcvm->depth++;
	if (qcvm->depth >= MAX_STACK_DEPTH)
		PR_RunError("stack overflow");

	// save off any locals that the new function steps on
	c = f->locals;
	if (qcvm->localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError("PR_ExecuteProgram: locals stack overflow");

	for (i = 0; i < c ; i++)
		qcvm->localstack[qcvm->localstack_used + i] = ((int *)qcvm->globals)[f->parm_start + i];
	qcvm->localstack_used += c;

	// copy parameters
	o = f->parm_start;
	for (i = 0; i < f->numparms; i++)
	{
		for (j = 0; j < f->parm_size[i]; j++)
		{
			((int *)qcvm->globals)[o] = ((int *)qcvm->globals)[OFS_PARM0 + i*3 + j];
			o++;
		}
	}

	qcvm->xfunction = f;
	if (qcprof.active) // QVR: vr_qcprofile
	{
		qcprof.stack[qcvm->depth].t0 = __rdtsc ();
		qcprof.stack[qcvm->depth].child = 0;
	}
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PR_LeaveFunction
====================
*/
static int PR_LeaveFunction (void)
{
	int	i, c;

	if (qcvm->depth <= 0)
		Host_Error("prog stack underflow");

	// Restore locals from the stack
	c = qcvm->xfunction->locals;
	qcvm->localstack_used -= c;
	if (qcvm->localstack_used < 0)
		PR_RunError("PR_ExecuteProgram: locals stack underflow");

	for (i = 0; i < c; i++)
		((int *)qcvm->globals)[qcvm->xfunction->parm_start + i] = qcvm->localstack[qcvm->localstack_used + i];

	if (qcprof.active) // QVR: vr_qcprofile
	{
		const int fn = (int)(qcvm->xfunction - qcvm->functions);
		const unsigned long long dt = __rdtsc () - qcprof.stack[qcvm->depth].t0;
		qcprof.incl[fn] += dt;
		qcprof.self[fn] += dt - qcprof.stack[qcvm->depth].child;
		qcprof.calls[fn]++;
		qcprof.stack[qcvm->depth - 1].child += dt;
	}

	// up stack
	qcvm->depth--;
	qcvm->xfunction = qcvm->stack[qcvm->depth].f;
	return qcvm->stack[qcvm->depth].s;
}

/*
====================
PR_CheckBuiltinExtension
====================
*/
static void PR_CheckBuiltinExtension (dfunction_t *func)
{
	uint32_t builtin = -func->first_statement;
	uint32_t extnum = qcvm->builtin_ext[builtin];
	uint32_t checked, advertised;

	if (!extnum)
		return;

	checked = GetBit (qcvm->checked_ext, extnum);
	advertised = GetBit (qcvm->advertised_ext, extnum);
	if (checked && advertised)
		return;

	if (GetBit (qcvm->warned_builtin[checked], builtin))
		return;
	SetBit (qcvm->warned_builtin[checked], builtin);

	Con_DWarning (checked ?
		"[%s] \"%s\" ignored when calling %s (%s: %s)\n" :
		"[%s] check \"%s\" before calling %s (%s: %s)\n",
		(qcvm == &cl.qcvm) ? "CL" : "SV",
		pr_extnames[extnum], PR_GetString (func->s_name),
		PR_GetString (qcvm->xfunction->s_file), PR_GetString (qcvm->xfunction->s_name)
	);
}

/*
====================
PR_ExecuteProgram

The interpretation main loop
====================
*/
#define OPA ((eval_t *)&qcvm->globals[(unsigned short)st->a])
#define OPB ((eval_t *)&qcvm->globals[(unsigned short)st->b])
#define OPC ((eval_t *)&qcvm->globals[(unsigned short)st->c])

static void PR_ExecuteProgramRun (func_t fnum);

// QVR: the profiler's "quakec" scope round the outermost call (vr_profile_report); with profiling off, one test.
void PR_ExecuteProgram (func_t fnum)
{
	if (qcvm->depth == 0 && qcvm == &sv.qcvm) // QVR: QuakeC is between broadcast messages (a full sv.datagram drops whole ones)
		VR_BroadcastQCRun ();
	if (qcvm->depth == 0) // QVR: vr_qcprofile
		QCProf_Latch ();
	if (vr_profile_on && !vr_profile_inqc) // QVR: profile (the rest in PR_ExecuteProgramRun)
	{
		vr_profile_inqc = 1;
		VR_ProfileBegin ("quakec");
		PR_ExecuteProgramRun (fnum);
		VR_ProfileEnd ();
		vr_profile_inqc = 0; // (a Host_Error jumping out leaves it set: VR_ProfileFrame clears it)
	}
	else
		PR_ExecuteProgramRun (fnum);
}

static void PR_ExecuteProgramRun (func_t fnum)
{
	eval_t		*ptr;
	dstatement_t	*st;
	dfunction_t	*f, *newf;
	int profile, startprofile;
	edict_t		*ed;
	int		exitdepth;

	if (!fnum || fnum >= qcvm->progs->numfunctions)
	{
		if (pr_global_struct->self)
			ED_Print (PROG_TO_EDICT(pr_global_struct->self));
		Host_Error ("PR_ExecuteProgram: NULL function");
	}

	f = &qcvm->functions[fnum];

	qcvm->trace = false;

// make a stack frame
	exitdepth = qcvm->depth;

	st = &qcvm->statements[PR_EnterFunction(f)];
	startprofile = profile = 0;

    while (1)
    {
	st++;	/* next statement */

	if (++profile > 0x1000000) /* was 100000 */
	{
		qcvm->xstatement = st - qcvm->statements;
		PR_RunError("runaway loop error");
	}

	if (qcvm->trace)
		PR_PrintStatement(st);

	switch (st->op)
	{
	case OP_ADD_F:
		OPC->_float = OPA->_float + OPB->_float;
		break;
	case OP_ADD_V:
		OPC->vector[0] = OPA->vector[0] + OPB->vector[0];
		OPC->vector[1] = OPA->vector[1] + OPB->vector[1];
		OPC->vector[2] = OPA->vector[2] + OPB->vector[2];
		break;

	case OP_SUB_F:
		OPC->_float = OPA->_float - OPB->_float;
		break;
	case OP_SUB_V:
		OPC->vector[0] = OPA->vector[0] - OPB->vector[0];
		OPC->vector[1] = OPA->vector[1] - OPB->vector[1];
		OPC->vector[2] = OPA->vector[2] - OPB->vector[2];
		break;

	case OP_MUL_F:
		OPC->_float = OPA->_float * OPB->_float;
		break;
	case OP_MUL_V:
		OPC->_float = OPA->vector[0] * OPB->vector[0] +
			      OPA->vector[1] * OPB->vector[1] +
			      OPA->vector[2] * OPB->vector[2];
		break;
	case OP_MUL_FV:
		OPC->vector[0] = OPA->_float * OPB->vector[0];
		OPC->vector[1] = OPA->_float * OPB->vector[1];
		OPC->vector[2] = OPA->_float * OPB->vector[2];
		break;
	case OP_MUL_VF:
		OPC->vector[0] = OPB->_float * OPA->vector[0];
		OPC->vector[1] = OPB->_float * OPA->vector[1];
		OPC->vector[2] = OPB->_float * OPA->vector[2];
		break;

	case OP_DIV_F:
		OPC->_float = OPA->_float / OPB->_float;
		break;

	case OP_BITAND:
		OPC->_float = (int)OPA->_float & (int)OPB->_float;
		break;

	case OP_BITOR:
		OPC->_float = (int)OPA->_float | (int)OPB->_float;
		break;

	case OP_GE:
		OPC->_float = OPA->_float >= OPB->_float;
		break;
	case OP_LE:
		OPC->_float = OPA->_float <= OPB->_float;
		break;
	case OP_GT:
		OPC->_float = OPA->_float > OPB->_float;
		break;
	case OP_LT:
		OPC->_float = OPA->_float < OPB->_float;
		break;
	case OP_AND:
		OPC->_float = OPA->_float && OPB->_float;
		break;
	case OP_OR:
		OPC->_float = OPA->_float || OPB->_float;
		break;

	case OP_NOT_F:
		OPC->_float = !OPA->_float;
		break;
	case OP_NOT_V:
		OPC->_float = !OPA->vector[0] && !OPA->vector[1] && !OPA->vector[2];
		break;
	case OP_NOT_S:
		OPC->_float = !OPA->string || !*PR_GetString(OPA->string);
		break;
	case OP_NOT_FNC:
		OPC->_float = !OPA->function;
		break;
	case OP_NOT_ENT:
		OPC->_float = (PROG_TO_EDICT(OPA->edict) == qcvm->edicts);
		break;

	case OP_EQ_F:
		OPC->_float = OPA->_float == OPB->_float;
		break;
	case OP_EQ_V:
		OPC->_float = (OPA->vector[0] == OPB->vector[0]) &&
			      (OPA->vector[1] == OPB->vector[1]) &&
			      (OPA->vector[2] == OPB->vector[2]);
		break;
	case OP_EQ_S:
		OPC->_float = !strcmp(PR_GetString(OPA->string), PR_GetString(OPB->string));
		break;
	case OP_EQ_E:
		OPC->_float = OPA->_int == OPB->_int;
		break;
	case OP_EQ_FNC:
		OPC->_float = OPA->function == OPB->function;
		break;

	case OP_NE_F:
		OPC->_float = OPA->_float != OPB->_float;
		break;
	case OP_NE_V:
		OPC->_float = (OPA->vector[0] != OPB->vector[0]) ||
			      (OPA->vector[1] != OPB->vector[1]) ||
			      (OPA->vector[2] != OPB->vector[2]);
		break;
	case OP_NE_S:
		OPC->_float = strcmp(PR_GetString(OPA->string), PR_GetString(OPB->string));
		break;
	case OP_NE_E:
		OPC->_float = OPA->_int != OPB->_int;
		break;
	case OP_NE_FNC:
		OPC->_float = OPA->function != OPB->function;
		break;

	case OP_STORE_F:
	case OP_STORE_ENT:
	case OP_STORE_FLD:	// integers
	case OP_STORE_S:
	case OP_STORE_FNC:	// pointers
		OPB->_int = OPA->_int;
		break;
	case OP_STORE_V:
		OPB->vector[0] = OPA->vector[0];
		OPB->vector[1] = OPA->vector[1];
		OPB->vector[2] = OPA->vector[2];
		break;

	case OP_STOREP_F:
	case OP_STOREP_ENT:
	case OP_STOREP_FLD:	// integers
	case OP_STOREP_S:
	case OP_STOREP_FNC:	// pointers
		ptr = (eval_t *)((byte *)qcvm->edicts + OPB->_int);
		ptr->_int = OPA->_int;
		break;
	case OP_STOREP_V:
		ptr = (eval_t *)((byte *)qcvm->edicts + OPB->_int);
		ptr->vector[0] = OPA->vector[0];
		ptr->vector[1] = OPA->vector[1];
		ptr->vector[2] = OPA->vector[2];
		break;

	case OP_ADDRESS:
		ed = PROG_TO_EDICT(OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);	// Make sure it's in range
#endif
		if (ed == (edict_t *)qcvm->edicts && sv.state == ss_active)
		{
			qcvm->xstatement = st - qcvm->statements;
			PR_RunError("assignment to world entity");
		}
		OPC->_int = (byte *)((int *)&ed->v + OPB->_int) - (byte *)qcvm->edicts;
		break;

	case OP_LOAD_F:
	case OP_LOAD_FLD:
	case OP_LOAD_ENT:
	case OP_LOAD_S:
	case OP_LOAD_FNC:
		ed = PROG_TO_EDICT(OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);	// Make sure it's in range
#endif
		OPC->_int = ((eval_t *)((int *)&ed->v + OPB->_int))->_int;
		break;

	case OP_LOAD_V:
		ed = PROG_TO_EDICT(OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);	// Make sure it's in range
#endif
		ptr = (eval_t *)((int *)&ed->v + OPB->_int);
		OPC->vector[0] = ptr->vector[0];
		OPC->vector[1] = ptr->vector[1];
		OPC->vector[2] = ptr->vector[2];
		break;

	case OP_IFNOT:
		if (!OPA->_int)
			st += st->b - 1;	/* -1 to offset the st++ */
		break;

	case OP_IF:
		if (OPA->_int)
			st += st->b - 1;	/* -1 to offset the st++ */
		break;

	case OP_GOTO:
		st += st->a - 1;		/* -1 to offset the st++ */
		break;

	case OP_CALL0:
	case OP_CALL1:
	case OP_CALL2:
	case OP_CALL3:
	case OP_CALL4:
	case OP_CALL5:
	case OP_CALL6:
	case OP_CALL7:
	case OP_CALL8:
		qcvm->xfunction->profile += profile - startprofile;
		startprofile = profile;
		qcvm->xstatement = st - qcvm->statements;
		qcvm->argc = st->op - OP_CALL0;
		if (!OPA->function)
			PR_RunError("NULL function");
		newf = &qcvm->functions[OPA->function];
		if (newf->first_statement < 0)
		{ // Built-in function
			int i = -newf->first_statement;
			if (i >= qcvm->numbuiltins)
				PR_RunError("Bad builtin call number %d", i);
			PR_CheckBuiltinExtension (newf);
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
			break;
		}
		// Normal function
		st = &qcvm->statements[PR_EnterFunction(newf)];
		break;

	case OP_DONE:
	case OP_RETURN:
		qcvm->xfunction->profile += profile - startprofile;
		startprofile = profile;
		qcvm->xstatement = st - qcvm->statements;
		qcvm->globals[OFS_RETURN] = qcvm->globals[(unsigned short)st->a];
		qcvm->globals[OFS_RETURN + 1] = qcvm->globals[(unsigned short)st->a + 1];
		qcvm->globals[OFS_RETURN + 2] = qcvm->globals[(unsigned short)st->a + 2];
		st = &qcvm->statements[PR_LeaveFunction()];
		if (qcvm->depth == exitdepth)
		{ // Done
			return;
		}
		break;

	case OP_STATE:
		ed = PROG_TO_EDICT(pr_global_struct->self);
		ed->v.nextthink = pr_global_struct->time + 0.1;
		ed->v.frame = OPA->_float;
		ed->v.think = OPB->function;
		break;

	default:
		qcvm->xstatement = st - qcvm->statements;
		PR_RunError("Bad opcode %i", st->op);
	}
    }	/* end of while(1) loop */
}

#undef OPA
#undef OPB
#undef OPC

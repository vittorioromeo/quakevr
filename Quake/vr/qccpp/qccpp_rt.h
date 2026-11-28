// qccpp_rt.h -- the runtime of the C++ that FTEQCC's -cppout writes from our QuakeC (Tools/fteqcc/qcc_cpp.c;
// docs/vr-port/QC_TO_CPP.md). Each macro is one interpreter statement exactly as pr_exec.c / vr_qcexec_loop.inc run
// it: the same float steps in the same order (no FMA contraction: the generated files build with -ffp-contract=off),
// the same memory (the VM's globals and edict fields), the same checks, errors, profile counts and edict-index hooks.
// Calls and returns keep the VM's stack and locals stack (PR_RunError's trace, recursion, nested VM calls).
#pragma once

extern "C" {
#include "quakedef.h"
void VR_EdictIndex_Address (int ofs, int watched); // (vr_api.h, which C++ includes with glm)
void VR_EdictIndex_Stored (int ofs);

// vr_qcexec.c (in pr_exec.c): the interpreter's own entry, exit and builtin call
void QCCpp_EnterFn (qcvm_t *vm, int fnum);
void QCCpp_LeaveFn (qcvm_t *vm);
void QCCpp_BuiltinFn (dfunction_t *f);
}

// vr_qccpp.cpp
void QCCpp_CallDynamic (qcvm_t *vm, int fnum);
[[noreturn]] void QCCpp_Runaway (int s);
[[noreturn]] void QCCpp_WorldError (int s);

using QCppFn = void (*) ();
struct QCppFunction
{
	const char *name; // the progs function's name (saves, spawn functions, think/touch pointers resolve by it)
	QCppFn fn;		  // nullptr: a builtin
};
struct QCppField
{
	const char *name;
	unsigned ofs, type;
};
struct QCppProgs
{
	unsigned hash; // FNV-1a of the statements and every function's first statement (vr_qccpp.cpp checks it)
	int numstatements, numfunctions;
	unsigned numglobals;
	int numfields;
	const QCppFunction *functions;
	const QCppField *fields;
};

#define QCPP_LIMIT 0x1000000 // the runaway limit: statements per outermost call (pr_exec.c)

// operands: a global's float, its bits, a vector at it
#define F(o) (qg[o])
#define I(o) (((int *)qg)[o])
#define V(o) (qg + (o))

// a function's entry (PR_EnterFunction: the stack frame, the locals saved, the parameters copied)
#define QCPP_ENTER(fn)                \
	qcvm_t *const vm = qcvm;          \
	float *const qg = vm->globals;    \
	(void)qg;                         \
	QCCpp_EnterFn (vm, fn)

// a basic block of n statements from statement s: the profile count and the runaway check
#define QCPP_COUNT(n, s)                               \
	do                                                 \
	{                                                  \
		if ((vm->qcpp_prof += (n)) > QCPP_LIMIT)       \
			QCCpp_Runaway (s);                         \
	} while (0)

// the statements since the last call or return, credited to the running function (the `profile` command)
#define QCPP_CREDIT()                                               \
	(vm->xfunction->profile += vm->qcpp_prof - vm->qcpp_mark, vm->qcpp_mark = vm->qcpp_prof)

#define QCPP_CALL(s, n, f)        \
	do                            \
	{                             \
		QCPP_CREDIT ();           \
		vm->xstatement = (s);     \
		vm->argc = (n);           \
		f ();                     \
	} while (0)
#define QCPP_BUILTIN(s, n, fn)                        \
	do                                                \
	{                                                 \
		QCPP_CREDIT ();                               \
		vm->xstatement = (s);                         \
		vm->argc = (n);                               \
		QCCpp_BuiltinFn (vm->functions + (fn));       \
	} while (0)
#define QCPP_CALLD(s, n, fv)          \
	do                                \
	{                                 \
		QCPP_CREDIT ();               \
		vm->xstatement = (s);         \
		vm->argc = (n);               \
		QCCpp_CallDynamic (vm, (fv)); \
	} while (0)
// OP_RETURN / OP_DONE: the value to OFS_RETURN (in this order: OP_DONE's a=0 overlaps it), then the exit
#define QCPP_RETURN(a, s)                          \
	do                                             \
	{                                              \
		QCPP_CREDIT ();                            \
		vm->xstatement = (s);                      \
		qg[OFS_RETURN] = qg[(a)];                  \
		qg[OFS_RETURN + 1] = qg[(a) + 1];          \
		qg[OFS_RETURN + 2] = qg[(a) + 2];          \
		QCCpp_LeaveFn (vm);                        \
	} while (0)

// vectors (element by element, as the VM: an operand may be the result)
#define QCPP_DOT(a, b) (qg[(a)] * qg[(b)] + qg[(a) + 1] * qg[(b) + 1] + qg[(a) + 2] * qg[(b) + 2])
#define QCPP_VADD(c, a, b) \
	(qg[(c)] = qg[(a)] + qg[(b)], qg[(c) + 1] = qg[(a) + 1] + qg[(b) + 1], qg[(c) + 2] = qg[(a) + 2] + qg[(b) + 2])
#define QCPP_VSUB(c, a, b) \
	(qg[(c)] = qg[(a)] - qg[(b)], qg[(c) + 1] = qg[(a) + 1] - qg[(b) + 1], qg[(c) + 2] = qg[(a) + 2] - qg[(b) + 2])
#define QCPP_MULFV(c, a, b) \
	(qg[(c)] = qg[(a)] * qg[(b)], qg[(c) + 1] = qg[(a)] * qg[(b) + 1], qg[(c) + 2] = qg[(a)] * qg[(b) + 2])
#define QCPP_MULVF(c, a, b) \
	(qg[(c)] = qg[(b)] * qg[(a)], qg[(c) + 1] = qg[(b)] * qg[(a) + 1], qg[(c) + 2] = qg[(b)] * qg[(a) + 2])
#define QCPP_VSTORE(b, a) (qg[(b)] = qg[(a)], qg[(b) + 1] = qg[(a) + 1], qg[(b) + 2] = qg[(a) + 2])
#define QCPP_VEQ(a, b) ((qg[(a)] == qg[(b)]) && (qg[(a) + 1] == qg[(b) + 1]) && (qg[(a) + 2] == qg[(b) + 2]))
#define QCPP_VNE(a, b) ((qg[(a)] != qg[(b)]) || (qg[(a) + 1] != qg[(b) + 1]) || (qg[(a) + 2] != qg[(b) + 2]))
#define QCPP_VNOT(a) (!qg[(a)] && !qg[(a) + 1] && !qg[(a) + 2])

// strings (OP_NE_S keeps strcmp's value, as the VM does)
#define QCPP_SEQ(a, b) (!strcmp (PR_GetString (a), PR_GetString (b)))
#define QCPP_SNE(a, b) ((float)strcmp (PR_GetString (a), PR_GetString (b)))
#define QCPP_SNOT(a) (!(a) || !*PR_GetString (a))
#define QCPP_ENOT(a) (PROG_TO_EDICT (a) == vm->edicts)

// edict fields: e an entity (byte offset), f a field (int offset)
#define QCPP_FLD(e, f) ((int *)&PROG_TO_EDICT (e)->v + (f))
#define QCPP_LOAD(e, f) (*QCPP_FLD (e, f))
#define QCPP_LOADV(c, e, f)                        \
	do                                             \
	{                                              \
		const float *const p_ = (const float *)QCPP_FLD (e, f); \
		qg[(c)] = p_[0];                           \
		qg[(c) + 1] = p_[1];                       \
		qg[(c) + 2] = p_[2];                       \
	} while (0)

static inline int QCPP_ADDRESS_impl (qcvm_t *vm, int e, int f, int s)
{
	edict_t *const ed = PROG_TO_EDICT (e);
	if (ed == vm->edicts && sv.state == ss_active)
		QCCpp_WorldError (s);
	const int r = (int)((byte *)((int *)&ed->v + f) - (byte *)vm->edicts);
	if (vm->fieldwatch && (unsigned)f < (unsigned)vm->fieldwatch_n && vm->fieldwatch[f])
		VR_EdictIndex_Address (r, vm->fieldwatch[f]);
	return r;
}
#define QCPP_ADDRESS(e, f, s) QCPP_ADDRESS_impl (vm, (e), (f), (s))

static inline void QCPP_STOREP_impl (qcvm_t *vm, int p, int v)
{
	*(int *)((byte *)vm->edicts + p) = v;
	if (vm->watchpending)
		VR_EdictIndex_Stored (p);
}
#define QCPP_STOREP(p, v) QCPP_STOREP_impl (vm, (p), (v))
static inline void QCPP_STOREPV_impl (qcvm_t *vm, const float *qg, int p, int a)
{
	float *const d = (float *)((byte *)vm->edicts + p);
	d[0] = qg[a];
	d[1] = qg[a + 1];
	d[2] = qg[a + 2];
	if (vm->watchpending)
		VR_EdictIndex_Stored (p);
}
#define QCPP_STOREPV(p, a) QCPP_STOREPV_impl (vm, qg, (p), (a))

// OP_STATE: self's next think in 0.1 s, its frame and think
#define QCPP_STATE(fr_, th_)                                         \
	do                                                                   \
	{                                                                    \
		edict_t *const ed_ = PROG_TO_EDICT (pr_global_struct->self);     \
		ed_->v.nextthink = pr_global_struct->time + 0.1;                 \
		ed_->v.frame = (fr_);                                          \
		ed_->v.think = (th_);                                          \
	} while (0)

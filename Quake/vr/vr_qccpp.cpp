// vr_qccpp.cpp -- runs our QuakeC as C++ (docs/vr-port/QC_TO_CPP.md): the C++ that FTEQCC's -cppout writes from the
// same compile as progs.dat (Tools/fteqcc/qcc_cpp.c), linked in when the engine is built with it (QVR_QC_CPP: the
// Visual Studio build's /p:QvrQcCpp=1, or QVR_QC_CPP=1 in the environment). vr_qc_cpp 1 runs every server QuakeC
// call through it instead of the interpreter; 0 (the default) keeps the interpreter. Both work on the same memory,
// so the switch can happen at any time, and saves, MP and the engine's own reads of fields and globals don't change.
//
// The C++ is used only for the progs it was generated from: at each progs load the statements and every function's
// first statement are hashed and compared with the generated table's hash (and the function and field tables), so
// a QuakeC edit without regenerating, or another progs (a -game mod), runs in the interpreter.

#include "qccpp/qccpp_rt.h"

#include "Zancle/Base/SizeT.hpp"

#ifdef QVR_QC_CPP
extern const QCppProgs qcpp_progs;
#endif

extern "C" {
cvar_t vr_qc_cpp = {"vr_qc_cpp", "0", CVAR_NONE};
}

namespace
{

struct
{
	const QCppProgs *progs; // the generated progs when it matches the loaded one, else nullptr
	bool checked;			// the match was checked since the last progs load
} qccpp;

unsigned fnv (unsigned h, const void *p, za::SizeT n)
{
	const unsigned char *c = static_cast<const unsigned char *> (p);
	for (za::SizeT i = 0; i < n; i++)
		h = (h ^ c[i]) * 16777619u;
	return h;
}

// The generated progs when the server's loaded progs is the one it came from (checked once per load), else nullptr.
const QCppProgs *boundProgs ()
{
	if (qccpp.checked)
		return qccpp.progs;
	qccpp.checked = true;
	qccpp.progs = nullptr;
#ifdef QVR_QC_CPP
	const QCppProgs &p = qcpp_progs;
	qcvm_t *const vm = &sv.qcvm;
	if (!vm->progs)
		return nullptr;
	const char *why = nullptr;
	if (vm->progs->numstatements != p.numstatements || vm->progs->numfunctions != p.numfunctions ||
		(unsigned)vm->progs->numglobals != p.numglobals || vm->progs->numfielddefs != p.numfields)
		why = "the sizes differ";
	else
	{
		unsigned h = 2166136261u;
		for (int i = 0; i < p.numstatements; i++)
			h = fnv (h, &vm->statements[i], sizeof (dstatement_t));
		for (int i = 0; i < p.numfunctions; i++)
			h = fnv (h, &vm->functions[i].first_statement, sizeof (int));
		if (h != p.hash)
			why = "the code differs";
		for (int i = 0; !why && i < p.numfields; i++)
			if (vm->fielddefs[i].ofs != p.fields[i].ofs || strcmp (PR_GetString (vm->fielddefs[i].s_name), p.fields[i].name))
				why = "the fields differ";
		for (int i = 1; !why && i < p.numfunctions; i++)
			if (strcmp (PR_GetString (vm->functions[i].s_name), p.functions[i].name))
				why = "the function names differ";
	}
	if (why)
	{
		Con_Printf ("vr_qc_cpp: the built-in C++ QuakeC is not this progs.dat's (%s): the interpreter runs it\n", why);
		return nullptr;
	}
	qccpp.progs = &p;
#endif
	return qccpp.progs;
}

} // namespace

// A call through a function value (self.think (), a function variable): the VM's checks, then the builtin, the
// generated function, or (a function the table lacks) the interpreter.
void QCCpp_CallDynamic (qcvm_t *vm, int fnum)
{
	if (!fnum)
		PR_RunError ("NULL function");
	dfunction_t *const f = vm->functions + fnum;
	if (f->first_statement < 0)
	{
		QCCpp_BuiltinFn (f);
		return;
	}
	const QCppProgs *const p = qccpp.progs;
	if (p && fnum < p->numfunctions && p->functions[fnum].fn)
	{
		p->functions[fnum].fn ();
		return;
	}
	PR_ExecuteProgram (fnum); // (never with a matching progs: every QuakeC function has its C++)
}

void QCCpp_Runaway (int s)
{
	qcvm->xstatement = s;
	PR_RunError ("runaway loop error");
}

void QCCpp_WorldError (int s)
{
	qcvm->xstatement = s;
	PR_RunError ("assignment to world entity");
}

extern "C" {

// QCExec_Run's hook (vr_qcexec.c): true when the C++ ran the outermost call fnum (the server's VM, vr_qc_cpp 1,
// the generated progs matching the loaded one). The statement count starts again for it, as the interpreter's does
// for each outermost or nested run, and is added to instr_total at the end.
qboolean VR_QCCpp_Run (func_t fnum)
{
	if (vr_qc_cpp.value < 1.f || qcvm != &sv.qcvm)
		return false;
	const QCppProgs *const p = boundProgs ();
	if (!p || fnum <= 0 || fnum >= p->numfunctions || !p->functions[fnum].fn)
		return false;
	qcvm_t *const vm = qcvm;
	const int prof = vm->depth ? vm->qcpp_prof : 0, mark = vm->depth ? vm->qcpp_mark : 0;
	vm->qcpp_prof = vm->qcpp_mark = 0;
	vm->trace = false;
	p->functions[fnum].fn ();
	vm->instr_total += vm->qcpp_prof;
	vm->qcpp_prof = prof;
	vm->qcpp_mark = mark;
	return true;
}

// PR_LoadProgs: check the match again before the next call.
void VR_QCCpp_ProgsLoaded (void)
{
	qccpp.checked = false;
	qccpp.progs = nullptr;
}

static void QCCpp_Info_f (void)
{
#ifdef QVR_QC_CPP
	const QCppProgs *const p = sv.active ? boundProgs () : nullptr;
	Con_Printf ("vr_qc_cpp %d: built-in C++ QuakeC %08x (%d functions, %d statements): %s\n", (int)vr_qc_cpp.value,
				qcpp_progs.hash, qcpp_progs.numfunctions, qcpp_progs.numstatements,
				!sv.active ? "no server" : p ? "matches the loaded progs" : "does not match the loaded progs");
#else
	Con_Printf ("vr_qc_cpp: this engine was built without the C++ QuakeC (QVR_QC_CPP)\n");
#endif
}

void VR_QCCpp_Init (void)
{
	Cvar_RegisterVariable (&vr_qc_cpp);
	Cmd_AddCommand ("vr_qc_cpp_info", QCCpp_Info_f);
}

} // extern "C"

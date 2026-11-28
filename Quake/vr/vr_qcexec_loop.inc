// vr_qcexec_loop.inc -- the QuakeC interpreter's loop, included twice by vr_qcexec.c: QX_GOTO 1 (computed-goto
// dispatch, clang/gcc) and 0 (a switch); QX_FUNC names the function. Behaviour identical to pr_exec.c's original loop
// (docs/vr-port/QC_VM.md): same statements in the same order, the same errors at the same statement, the same profile
// counts. The differences are all bookkeeping:
// - the VM's pointers (globals, edicts, statements) live in locals (reloaded after each builtin, which may change them);
// - the statement count is implicit: count = (st - stbase) + off, with off changed only where control jumps (a taken
//   branch, a call, a return), so straight-line code counts nothing; profile totals stay exact;
// - the runaway limit is checked wherever off changes and before anything that leaves the VM (builtins, errors), so
//   the error names the same statement as the original's per-statement check (QCExec_Runaway);
// - tracing (traceon) is a second dispatch table whose every entry prints the statement first (the switch: a flag).

static void QX_FUNC (func_t fnum)
{
	qcvm_t			*vm;
	float			*glob;
	byte			*edb;
	dstatement_t	*stbase, *st;
	dfunction_t		*f, *newf;
	edict_t			*ed;
	eval_t			*ptr;
	int				off, startprofile, exitdepth;
#if QX_GOTO
#define QX_LABEL(op) [op] = &&qx_##op,
#define QX_TRACED(op) [op] = &&qx_trace,
#if QX_FUSE // the combined instructions too (vr_qcexec.c, QX_FUSED; the statements are vm->qx_fused's)
#define QX_FLABEL(name, first, second) [QXF_##name] = &&qxf_##name,
#define QX_FTRACED(name, first, second) [QXF_##name] = &&qx_trace,
#define QX_NDISP QXF_COUNT
	static const void *const fast[QX_NDISP] = { QX_OPS (QX_LABEL) QX_FUSED (QX_FLABEL) }; // statics-ok: label addresses exist only inside the function (computed goto)
	static const void *const traced[QX_NDISP] = { QX_OPS (QX_TRACED) QX_FUSED (QX_FTRACED) }; // statics-ok: as above
#undef QX_FLABEL
#undef QX_FTRACED
#else
#define QX_NDISP (OP_BITOR + 1)
	static const void *const fast[QX_NDISP] = { QX_OPS (QX_LABEL) }; // statics-ok: label addresses exist only inside the function (computed goto)
	static const void *const traced[QX_NDISP] = { QX_OPS (QX_TRACED) }; // statics-ok: as above
#endif
#undef QX_LABEL
#undef QX_TRACED
	const void *const *disp;
#define QX_CASE(op) qx_##op:
#define QX_DISPATCH() do { const unsigned op_ = st->op; if (op_ >= QX_NDISP) goto qx_badop; goto *disp[op_]; } while (0)
#define QX_SETTRACE() (disp = vm->trace ? traced : fast)
#define QX_BADOP_TRACE() do { if (disp == traced) PR_PrintStatement (st); } while (0)
#else
	int				tracing;
#define QX_CASE(op) case op:
#define QX_DISPATCH() goto qx_dispatch
#define QX_SETTRACE() (tracing = vm->trace)
#define QX_BADOP_TRACE() ((void)0)
#endif
#define QX_IDX(s) ((int)((s) - stbase))
#define QX_COUNT() (QX_IDX (st) + off)
#define QX_RUNAWAY() do { if (QX_COUNT () > QX_LIMIT) QCExec_Runaway (off); } while (0)
#define QX_NEXT() do { ++st; QX_DISPATCH (); } while (0)
#define QX_JUMP(d) do { const int d_ = (d); QX_RUNAWAY (); off -= d_ - 1; st += d_; QX_DISPATCH (); } while (0)
#define QX_A ((eval_t *)&glob[st->a])
#define QX_B ((eval_t *)&glob[st->b])
#define QX_C ((eval_t *)&glob[st->c])
#define QX_ABC eval_t *const a = QX_A, *const b = QX_B, *const c = QX_C; (void)a; (void)b; (void)c
// The bodies of the opcodes a combined instruction starts with (QX_FUSE; the single handlers below use them too).
#define QX_DO_BIN(field, expr) { QX_ABC; c->field = (expr); }
#define QX_DO_ADD_F QX_DO_BIN (_float, a->_float + b->_float)
#define QX_DO_SUB_F QX_DO_BIN (_float, a->_float - b->_float)
#define QX_DO_MUL_F QX_DO_BIN (_float, a->_float * b->_float)
#define QX_DO_GE QX_DO_BIN (_float, a->_float >= b->_float)
#define QX_DO_LE QX_DO_BIN (_float, a->_float <= b->_float)
#define QX_DO_GT QX_DO_BIN (_float, a->_float > b->_float)
#define QX_DO_LT QX_DO_BIN (_float, a->_float < b->_float)
#define QX_DO_AND QX_DO_BIN (_float, a->_float && b->_float)
#define QX_DO_OR QX_DO_BIN (_float, a->_float || b->_float)
#define QX_DO_NOT_F QX_DO_BIN (_float, !a->_float)
#define QX_DO_EQ_F QX_DO_BIN (_float, a->_float == b->_float)
#define QX_DO_EQ_E QX_DO_BIN (_float, a->_int == b->_int)
#define QX_DO_NE_F QX_DO_BIN (_float, a->_float != b->_float)
#define QX_DO_NE_E QX_DO_BIN (_float, a->_int != b->_int)
#define QX_DO_STORE_F { QX_ABC; b->_int = a->_int; }
#define QX_DO_STORE_V { QX_ABC; b->vector[0] = a->vector[0]; b->vector[1] = a->vector[1]; b->vector[2] = a->vector[2]; }
#define QX_DO_LOAD_F { QX_ABC; \
		ed = (edict_t *)(edb + a->edict); \
		c->_int = ((eval_t *)((int *)&ed->v + b->_int))->_int; }
#define QX_DO_LOAD_V { QX_ABC; \
		ed = (edict_t *)(edb + a->edict); \
		ptr = (eval_t *)((int *)&ed->v + b->_int); \
		c->vector[0] = ptr->vector[0]; \
		c->vector[1] = ptr->vector[1]; \
		c->vector[2] = ptr->vector[2]; }
#define QX_DO_ADDRESS { QX_ABC; \
		ed = (edict_t *)(edb + a->edict); \
		if (ed == (edict_t *)edb && sv.state == ss_active) \
		{ \
			QX_RUNAWAY (); \
			vm->xstatement = QX_IDX (st); \
			PR_RunError("assignment to world entity"); \
		} \
		c->_int = (byte *)((int *)&ed->v + b->_int) - edb; \
		if (vm->fieldwatch && (unsigned)b->_int < (unsigned)vm->fieldwatch_n && vm->fieldwatch[b->_int]) \
		{ \
			QX_RUNAWAY (); /* (it leaves the VM) */ \
			VR_EdictIndex_Address (c->_int, vm->fieldwatch[b->_int]); /* QVR: the edict index follows the store */ \
		} }
#define QX_DO_IFNOT { if (!QX_A->_int) QX_JUMP (st->b); }
#define QX_DO_IF { if (QX_A->_int) QX_JUMP (st->b); }
#if QX_FASTCALL // calls and returns by the loop's own entry and exit (vr_qcexec.c, QCExec_Enter)
#define QX_ENTERF(f) QCExec_Enter (vm, f)
#define QX_LEAVEF() QCExec_Leave (vm)
#else
#define QX_ENTERF(f) PR_EnterFunction (f)
#define QX_LEAVEF() PR_LeaveFunction ()
#endif

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

	{
		const int first = PR_EnterFunction (f) + 1;
		vm = qcvm;
		glob = vm->globals;
		edb = (byte *)vm->edicts;
#if QX_FUSE
		stbase = vm->qx_fused; // (QCExec_Run made it)
#else
		stbase = vm->statements;
#endif
		st = stbase + first;
		off = 1 - first; // the first statement is the 1st
	}
	startprofile = 0;
	QX_SETTRACE ();

#if QX_GOTO
	QX_DISPATCH ();

qx_trace:
	QX_RUNAWAY ();
#if QX_FUSE // traced: the original statement, run alone
	PR_PrintStatement (vm->statements + QX_IDX (st));
	goto *fast[vm->statements[QX_IDX (st)].op];
#else
	PR_PrintStatement (st);
	goto *fast[st->op];
#endif
#else
qx_dispatch:
#if QX_STATS
	QCExec_Stat (QX_IDX (st), st->op);
#endif
	if (tracing)
	{
		QX_RUNAWAY ();
		PR_PrintStatement (st);
	}
	switch (st->op)
	{
#endif

	QX_CASE (OP_ADD_F) QX_DO_ADD_F QX_NEXT ();
	QX_CASE (OP_ADD_V) { QX_ABC;
		c->vector[0] = a->vector[0] + b->vector[0];
		c->vector[1] = a->vector[1] + b->vector[1];
		c->vector[2] = a->vector[2] + b->vector[2];
	} QX_NEXT ();

	QX_CASE (OP_SUB_F) QX_DO_SUB_F QX_NEXT ();
	QX_CASE (OP_SUB_V) { QX_ABC;
		c->vector[0] = a->vector[0] - b->vector[0];
		c->vector[1] = a->vector[1] - b->vector[1];
		c->vector[2] = a->vector[2] - b->vector[2];
	} QX_NEXT ();

	QX_CASE (OP_MUL_F) QX_DO_MUL_F QX_NEXT ();
	QX_CASE (OP_MUL_V) { QX_ABC;
		c->_float = a->vector[0] * b->vector[0] +
			    a->vector[1] * b->vector[1] +
			    a->vector[2] * b->vector[2];
	} QX_NEXT ();
	QX_CASE (OP_MUL_FV) { QX_ABC;
		c->vector[0] = a->_float * b->vector[0];
		c->vector[1] = a->_float * b->vector[1];
		c->vector[2] = a->_float * b->vector[2];
	} QX_NEXT ();
	QX_CASE (OP_MUL_VF) { QX_ABC;
		c->vector[0] = b->_float * a->vector[0];
		c->vector[1] = b->_float * a->vector[1];
		c->vector[2] = b->_float * a->vector[2];
	} QX_NEXT ();

	QX_CASE (OP_DIV_F) { QX_ABC;
		c->_float = a->_float / b->_float;
	} QX_NEXT ();

	QX_CASE (OP_BITAND) { QX_ABC;
		c->_float = (int)a->_float & (int)b->_float;
	} QX_NEXT ();

	QX_CASE (OP_BITOR) { QX_ABC;
		c->_float = (int)a->_float | (int)b->_float;
	} QX_NEXT ();

	QX_CASE (OP_GE) QX_DO_GE QX_NEXT ();
	QX_CASE (OP_LE) QX_DO_LE QX_NEXT ();
	QX_CASE (OP_GT) QX_DO_GT QX_NEXT ();
	QX_CASE (OP_LT) QX_DO_LT QX_NEXT ();
	QX_CASE (OP_AND) QX_DO_AND QX_NEXT ();
	QX_CASE (OP_OR) QX_DO_OR QX_NEXT ();

	QX_CASE (OP_NOT_F) QX_DO_NOT_F QX_NEXT ();
	QX_CASE (OP_NOT_V) { QX_ABC;
		c->_float = !a->vector[0] && !a->vector[1] && !a->vector[2];
	} QX_NEXT ();
	QX_CASE (OP_NOT_S) { QX_ABC;
		c->_float = !a->string || !*PR_GetString(a->string);
	} QX_NEXT ();
	QX_CASE (OP_NOT_FNC) { QX_ABC;
		c->_float = !a->function;
	} QX_NEXT ();
	QX_CASE (OP_NOT_ENT) { QX_ABC;
		c->_float = ((edict_t *)(edb + a->edict) == vm->edicts);
	} QX_NEXT ();

	QX_CASE (OP_EQ_F) QX_DO_EQ_F QX_NEXT ();
	QX_CASE (OP_EQ_V) { QX_ABC;
		c->_float = (a->vector[0] == b->vector[0]) &&
			    (a->vector[1] == b->vector[1]) &&
			    (a->vector[2] == b->vector[2]);
	} QX_NEXT ();
	QX_CASE (OP_EQ_S) { QX_ABC;
		c->_float = !strcmp(PR_GetString(a->string), PR_GetString(b->string));
	} QX_NEXT ();
	QX_CASE (OP_EQ_E) QX_DO_EQ_E QX_NEXT ();
	QX_CASE (OP_EQ_FNC) { QX_ABC;
		c->_float = a->function == b->function;
	} QX_NEXT ();

	QX_CASE (OP_NE_F) QX_DO_NE_F QX_NEXT ();
	QX_CASE (OP_NE_V) { QX_ABC;
		c->_float = (a->vector[0] != b->vector[0]) ||
			    (a->vector[1] != b->vector[1]) ||
			    (a->vector[2] != b->vector[2]);
	} QX_NEXT ();
	QX_CASE (OP_NE_S) { QX_ABC;
		c->_float = strcmp(PR_GetString(a->string), PR_GetString(b->string));
	} QX_NEXT ();
	QX_CASE (OP_NE_E) QX_DO_NE_E QX_NEXT ();
	QX_CASE (OP_NE_FNC) { QX_ABC;
		c->_float = a->function != b->function;
	} QX_NEXT ();

	QX_CASE (OP_STORE_F)
	QX_CASE (OP_STORE_ENT)
	QX_CASE (OP_STORE_FLD)	// integers
	QX_CASE (OP_STORE_S)
	QX_CASE (OP_STORE_FNC)	// pointers
		QX_DO_STORE_F QX_NEXT ();
	QX_CASE (OP_STORE_V) QX_DO_STORE_V QX_NEXT ();

	QX_CASE (OP_STOREP_F)
	QX_CASE (OP_STOREP_ENT)
	QX_CASE (OP_STOREP_FLD)	// integers
	QX_CASE (OP_STOREP_S)
	QX_CASE (OP_STOREP_FNC)	// pointers
	{ QX_ABC;
		ptr = (eval_t *)(edb + b->_int);
		ptr->_int = a->_int;
		if (vm->watchpending) // QVR: a watched field's store (the edict index reads the edict again)
		{
			QX_RUNAWAY (); // (it leaves the VM)
			VR_EdictIndex_Stored (b->_int);
		}
	} QX_NEXT ();
	QX_CASE (OP_STOREP_V) { QX_ABC;
		ptr = (eval_t *)(edb + b->_int);
		ptr->vector[0] = a->vector[0];
		ptr->vector[1] = a->vector[1];
		ptr->vector[2] = a->vector[2];
		if (vm->watchpending) // QVR: a watched vector's store (findradius: .origin, .mins, .maxs)
		{
			QX_RUNAWAY (); // (it leaves the VM)
			VR_EdictIndex_Stored (b->_int);
		}
	} QX_NEXT ();

	QX_CASE (OP_ADDRESS) QX_DO_ADDRESS QX_NEXT ();

	QX_CASE (OP_LOAD_F)
	QX_CASE (OP_LOAD_FLD)
	QX_CASE (OP_LOAD_ENT)
	QX_CASE (OP_LOAD_S)
	QX_CASE (OP_LOAD_FNC)
		QX_DO_LOAD_F QX_NEXT ();

	QX_CASE (OP_LOAD_V) QX_DO_LOAD_V QX_NEXT ();

	QX_CASE (OP_IFNOT) QX_DO_IFNOT QX_NEXT ();

	QX_CASE (OP_IF) QX_DO_IF QX_NEXT ();

	QX_CASE (OP_GOTO)
		QX_JUMP (st->a);

	QX_CASE (OP_CALL0)
	QX_CASE (OP_CALL1)
	QX_CASE (OP_CALL2)
	QX_CASE (OP_CALL3)
	QX_CASE (OP_CALL4)
	QX_CASE (OP_CALL5)
	QX_CASE (OP_CALL6)
	QX_CASE (OP_CALL7)
	QX_CASE (OP_CALL8)
	{
		const int cur = QX_COUNT ();
		if (cur > QX_LIMIT)
			QCExec_Runaway (off);
		vm->xfunction->profile += cur - startprofile;
		startprofile = cur;
		vm->xstatement = QX_IDX (st);
		vm->argc = st->op - OP_CALL0;
		if (!QX_A->function)
			PR_RunError("NULL function");
		newf = &vm->functions[QX_A->function];
#if QX_STATS
		QCExec_StatCall (newf, vm->argc);
#endif
		if (newf->first_statement < 0)
		{ // Built-in function
#if QX_FASTCALL && !defined(QVR_TRACY)
			// the common case straight from here (qcconv): no extension to check, no profiler timing it; else as before
			const int bi = -newf->first_statement;
			if (bi < vm->numbuiltins && !vm->builtin_ext[bi] && !qcprof.active && !vr_profile_fine)
				vm->builtins[bi] ();
			else
#endif
			QCExec_Builtin (newf);
			vm = qcvm; // (a builtin may run anything)
			glob = vm->globals;
			edb = (byte *)vm->edicts;
			QX_SETTRACE (); // traceon/traceoff, or QuakeC it called (which starts untraced)
			QX_NEXT ();
		}
		// Normal function
		{
			const int first = QX_ENTERF (newf) + 1;
			off += QX_IDX (st) + 1 - first;
			st = stbase + first;
		}
	}
	QX_DISPATCH ();

	QX_CASE (OP_DONE)
	QX_CASE (OP_RETURN)
	{
		const int cur = QX_COUNT ();
		if (cur > QX_LIMIT)
			QCExec_Runaway (off);
		vm->xfunction->profile += cur - startprofile;
		startprofile = cur;
		vm->xstatement = QX_IDX (st);
		glob[OFS_RETURN] = glob[st->a];
		glob[OFS_RETURN + 1] = glob[st->a + 1];
		glob[OFS_RETURN + 2] = glob[st->a + 2];
		{
			const int ret = QX_LEAVEF ();
			if (vm->depth == exitdepth)
			{ // Done
				vm->instr_total += cur; // QVR: the per-edict think costs (SV_RunThink, QC thinkcost)
				return;
			}
			off += QX_IDX (st) - ret; // the caller resumes after its call (ret + 1), counted one after this
			st = stbase + ret + 1;
		}
	}
	QX_DISPATCH ();

	QX_CASE (OP_STATE) { QX_ABC;
		ed = PROG_TO_EDICT(pr_global_struct->self);
		ed->v.nextthink = pr_global_struct->time + 0.1;
		ed->v.frame = a->_float;
		ed->v.think = b->function;
	} QX_NEXT ();

#if !QX_GOTO
	default:
		goto qx_badop;
	}
#endif

#if QX_FUSE // a combined instruction: its first statement, then straight into the second's handler (no dispatch)
#define QX_FHANDLER(name, first, second) qxf_##name: QX_DO_##first ++st; goto qx_##second;
	QX_FUSED (QX_FHANDLER)
#undef QX_FHANDLER
#endif

qx_badop:
	QX_RUNAWAY ();
	QX_BADOP_TRACE ();
	vm->xstatement = QX_IDX (st);
	PR_RunError("Bad opcode %i", st->op);

#undef QX_DO_BIN
#undef QX_DO_ADD_F
#undef QX_DO_SUB_F
#undef QX_DO_MUL_F
#undef QX_DO_GE
#undef QX_DO_LE
#undef QX_DO_GT
#undef QX_DO_LT
#undef QX_DO_AND
#undef QX_DO_OR
#undef QX_DO_NOT_F
#undef QX_DO_EQ_F
#undef QX_DO_EQ_E
#undef QX_DO_NE_F
#undef QX_DO_NE_E
#undef QX_DO_STORE_F
#undef QX_DO_STORE_V
#undef QX_DO_LOAD_F
#undef QX_DO_LOAD_V
#undef QX_DO_ADDRESS
#undef QX_DO_IFNOT
#undef QX_DO_IF
#undef QX_NDISP
#undef QX_CASE
#undef QX_DISPATCH
#undef QX_SETTRACE
#undef QX_BADOP_TRACE
#undef QX_IDX
#undef QX_COUNT
#undef QX_RUNAWAY
#undef QX_NEXT
#undef QX_JUMP
#undef QX_A
#undef QX_B
#undef QX_C
#undef QX_ABC
#undef QX_ENTERF
#undef QX_LEAVEF
}

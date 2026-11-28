// QVR: pr_knownstrings_check: the known-string table's growth checked.
// Moved as it was out of pr_edict.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
QVR: pr_knownstrings_check: a scratch VM's known-string table and zoned bitmap grown many times (realloc), every slot
checked after each growth (the earlier strings kept, the new slots NULL), the bitmap's bits and zeroed tail, then the
free list's reuse (no growth, the other slots kept). Torn down as PR_ClearProgs does (free). Prints
"pr_knownstrings_check: N passed, M failed" (ROUND25.md, "norealloc").
*/
static int PR_KSC_Slots (qcvm_t *vm, const char *text, int filled, int reused)
{	// slots below filled hold text + 2*i (+1 when reused and i%3 == 0), the rest up to maxknownstrings are NULL
	int i, bad = 0;
	for (i = 0; i < vm->maxknownstrings; i++)
	{
		const char *want = i < filled ? text + 2*i + (reused && i % 3 == 0) : NULL;
		if (vm->knownstrings[i] != want || (want && PR_GetString (-1 - i) != want))
			bad++;
	}
	return bad;
}

static void PR_KnownStringsCheck_f (void)
{
	const int count = 40 * PR_STRING_ALLOCSLOTS + 7; // 41 growths of the table, ~1300 bytes of bitmap
	qcvm_t *vm = (qcvm_t *) calloc (1, sizeof (qcvm_t));
	char *text = (char *) calloc (count * 2 + 1, 1); // slot i's string: text + 2*i, reused: text + 2*i + 1
	qcvm_t *oldvm;
	int i, bad, prevmax = 0, passed = 0, failed = 0;
	size_t id;

	if (!vm || !text)
		Sys_Error ("pr_knownstrings_check: out of memory");
	vm->strings = text + count * 2; // (no progs strings: none of the test's pointers fall in them)
	vm->stringssize = 1;
	PR_PushQCVM (vm, &oldvm);

	bad = 0;
	for (i = 0; i < count; i++)
	{
		bad += PR_SetEngineString (text + 2*i) != -1 - i;
		if (i % 5 == 0)
			PR_MarkZonedString (i);
		if (vm->maxknownstrings != prevmax)
		{	// grown: every slot checked
			int b = PR_KSC_Slots (vm, text, i + 1, 0);
			if (b)
				Con_Printf ("pr_knownstrings_check: FAIL: growth to %i slots: %i slots wrong\n", vm->maxknownstrings, b);
			passed += !b;
			failed += !!b;
			prevmax = vm->maxknownstrings;
		}
	}
	bad += PR_KSC_Slots (vm, text, count, 0);
	if (bad)
		Con_Printf ("pr_knownstrings_check: FAIL: filled table: %i wrong\n", bad);
	passed += !bad;
	failed += !!bad;

	bad = 0;
	for (id = 0; id < vm->knownzonesize; id++)
		bad += !!(vm->knownzone[id>>3] & (1u<<(id&7))) != (id < (size_t)count && id % 5 == 0);
	if (bad)
		Con_Printf ("pr_knownstrings_check: FAIL: zoned bitmap (%i ids): %i bits wrong\n", (int)vm->knownzonesize, bad);
	passed += !bad;
	failed += !!bad;

	for (i = 0; i < count; i += 3)
		PR_ClearEngineString (-1 - i);
	bad = 0;
	for (i = (count - 1) / 3 * 3; i >= 0; i -= 3) // (the free list is last in, first out)
		PR_SetEngineString (text + 2*i + 1);
	bad += vm->maxknownstrings != prevmax || vm->numknownstrings != count || vm->firstfreeknownstring;
	bad += PR_KSC_Slots (vm, text, count, 1);
	if (bad)
		Con_Printf ("pr_knownstrings_check: FAIL: free list reuse: %i wrong\n", bad);
	passed += !bad;
	failed += !!bad;

	PR_PopQCVM (oldvm);
	free ((void *)vm->knownstrings);
	free (vm->knownzone);
	free (vm);
	free (text);
	Con_Printf ("pr_knownstrings_check: %i passed, %i failed\n", passed, failed);
}


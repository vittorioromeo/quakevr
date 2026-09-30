// vr_crash.cpp -- automated test runs (QVR_NO_ERROR_DIALOG, set by the kit's run.ps1 and the review's child copies):
// a crash writes a report instead of hanging, and an error quits instead of waiting on a dialog (main_sdl.c calls
// VR_InstallCrashHandler; pl_win.c's PL_ErrorDialog, VR_ErrorDialogSuppressed). Windows only; its own translation
// unit: <windows.h> stays out of the engine's headers.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

/* A crash in an automated test run (QVR_NO_ERROR_DIALOG) writes qvr_crash.txt (the exception and the crashing
 * thread's stack, with symbols from the build's .pdb) and qvr_crash.dmp (a minidump for a debugger) in the working
 * directory; run.ps1 prints the stack. DbgHelp is loaded only then (no link dependency). */

typedef BOOL (WINAPI *qvr_SymInitialize_t) (HANDLE, PCSTR, BOOL);
typedef DWORD (WINAPI *qvr_SymSetOptions_t) (DWORD);
typedef BOOL (WINAPI *qvr_SymFromAddr_t) (HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL (WINAPI *qvr_SymGetLineFromAddr64_t) (HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
typedef BOOL (WINAPI *qvr_StackWalk64_t) (DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64,
	PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
typedef BOOL (WINAPI *qvr_MiniDumpWriteDump_t) (HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
	PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

static void PL_CrashReport (EXCEPTION_POINTERS *ep, const char *what)
{
	static volatile LONG entered = 0;
	HMODULE dbg;
	HANDLE proc = GetCurrentProcess (), thread = GetCurrentThread ();
	FILE *f;
	if (InterlockedExchange (&entered, 1)) // a second thread crashing meanwhile: leave the first one's report
		return;
	dbg = LoadLibraryA ("dbghelp.dll");
	f = fopen ("qvr_crash.txt", "w");
	if (f)
	{
		EXCEPTION_RECORD *er = ep->ExceptionRecord;
		if (what)
			fprintf (f, "%s: ", what);
		fprintf (f, "exception 0x%08lx at %p", (unsigned long)er->ExceptionCode, er->ExceptionAddress);
		if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
			fprintf (f, " (%s 0x%llx)", er->ExceptionInformation[0] == 0 ? "reading" : er->ExceptionInformation[0] == 1 ? "writing" : "executing",
				(unsigned long long)er->ExceptionInformation[1]);
		fprintf (f, ", thread %lu\n", (unsigned long)GetCurrentThreadId ());
		fflush (f);
	}
	if (dbg)
	{
		qvr_SymInitialize_t pSymInitialize = (qvr_SymInitialize_t)GetProcAddress (dbg, "SymInitialize");
		qvr_SymSetOptions_t pSymSetOptions = (qvr_SymSetOptions_t)GetProcAddress (dbg, "SymSetOptions");
		qvr_SymFromAddr_t pSymFromAddr = (qvr_SymFromAddr_t)GetProcAddress (dbg, "SymFromAddr");
		qvr_SymGetLineFromAddr64_t pSymGetLine = (qvr_SymGetLineFromAddr64_t)GetProcAddress (dbg, "SymGetLineFromAddr64");
		qvr_StackWalk64_t pStackWalk64 = (qvr_StackWalk64_t)GetProcAddress (dbg, "StackWalk64");
		PFUNCTION_TABLE_ACCESS_ROUTINE64 pFta = (PFUNCTION_TABLE_ACCESS_ROUTINE64)GetProcAddress (dbg, "SymFunctionTableAccess64");
		PGET_MODULE_BASE_ROUTINE64 pGmb = (PGET_MODULE_BASE_ROUTINE64)GetProcAddress (dbg, "SymGetModuleBase64");
		qvr_MiniDumpWriteDump_t pDump = (qvr_MiniDumpWriteDump_t)GetProcAddress (dbg, "MiniDumpWriteDump");
		if (f && pSymInitialize && pSymFromAddr && pStackWalk64 && pFta && pGmb)
		{
			CONTEXT ctx = *ep->ContextRecord;
			STACKFRAME64 sf;
			int i;
			if (pSymSetOptions)
				pSymSetOptions (SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
			pSymInitialize (proc, NULL, TRUE);
			memset (&sf, 0, sizeof (sf));
#ifdef _M_X64
			sf.AddrPC.Offset = ctx.Rip; sf.AddrFrame.Offset = ctx.Rbp; sf.AddrStack.Offset = ctx.Rsp;
#define QVR_MACHINE IMAGE_FILE_MACHINE_AMD64
#else
			sf.AddrPC.Offset = ctx.Eip; sf.AddrFrame.Offset = ctx.Ebp; sf.AddrStack.Offset = ctx.Esp;
#define QVR_MACHINE IMAGE_FILE_MACHINE_I386
#endif
			sf.AddrPC.Mode = sf.AddrFrame.Mode = sf.AddrStack.Mode = AddrModeFlat;
			for (i = 0; i < 64 && pStackWalk64 (QVR_MACHINE, proc, thread, &sf, &ctx, NULL, pFta, pGmb, NULL); i++)
			{
				char buf[sizeof (SYMBOL_INFO) + 256];
				SYMBOL_INFO *sym = (SYMBOL_INFO *)buf;
				DWORD64 disp = 0;
				DWORD ldisp = 0;
				IMAGEHLP_LINE64 line;
				char mod[MAX_PATH] = "?";
				DWORD64 base = pGmb (proc, sf.AddrPC.Offset);
				if (!sf.AddrPC.Offset)
					break;
				if (base)
				{
					char *slash;
					GetModuleFileNameA ((HMODULE)(uintptr_t)base, mod, sizeof (mod));
					slash = strrchr (mod, '\\');
					if (slash)
						memmove (mod, slash + 1, strlen (slash + 1) + 1);
				}
				memset (buf, 0, sizeof (buf));
				sym->SizeOfStruct = sizeof (SYMBOL_INFO);
				sym->MaxNameLen = 255;
				fprintf (f, "  #%02d %s+0x%llx", i, mod, (unsigned long long)(sf.AddrPC.Offset - base));
				if (pSymFromAddr (proc, sf.AddrPC.Offset, &disp, sym))
					fprintf (f, " %s+0x%llx", sym->Name, (unsigned long long)disp);
				memset (&line, 0, sizeof (line));
				line.SizeOfStruct = sizeof (line);
				if (pSymGetLine && pSymGetLine (proc, sf.AddrPC.Offset, &ldisp, &line))
					fprintf (f, " (%s:%lu)", line.FileName, (unsigned long)line.LineNumber);
				fprintf (f, "\n");
				fflush (f);
			}
#undef QVR_MACHINE
		}
		if (pDump)
		{
			HANDLE h = CreateFileA ("qvr_crash.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (h != INVALID_HANDLE_VALUE)
			{
				MINIDUMP_EXCEPTION_INFORMATION mei;
				mei.ThreadId = GetCurrentThreadId ();
				mei.ExceptionPointers = ep;
				mei.ClientPointers = FALSE;
				pDump (proc, GetCurrentProcessId (), h, (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo),
					&mei, NULL, NULL);
				CloseHandle (h);
			}
		}
	}
	if (f)
		fclose (f);
}

static LONG WINAPI PL_CrashFilter (EXCEPTION_POINTERS *ep)
{
	PL_CrashReport (ep, NULL);
	return EXCEPTION_CONTINUE_SEARCH; // the process still ends as a crash (its exit code: the exception's)
}

/* The CRT's fatal errors end the process with a fast fail (0xc0000409), which no exception filter sees: abort()
 * (std::terminate, a failed assert), an invalid parameter to a CRT function, a pure virtual call. Their report too,
 * from the caller's stack, and the same exit code. */
static void PL_FatalReport (const char *what)
{
	CONTEXT ctx;
	EXCEPTION_RECORD er;
	EXCEPTION_POINTERS ep;
	memset (&er, 0, sizeof (er));
	RtlCaptureContext (&ctx);
	er.ExceptionCode = 0xc0000409; // STATUS_STACK_BUFFER_OVERRUN: what the CRT's fast fail ends with
#ifdef _M_X64
	er.ExceptionAddress = (PVOID)(uintptr_t)ctx.Rip;
#else
	er.ExceptionAddress = (PVOID)(uintptr_t)ctx.Eip;
#endif
	ep.ExceptionRecord = &er;
	ep.ContextRecord = &ctx;
	PL_CrashReport (&ep, what);
	TerminateProcess (GetCurrentProcess (), 0xc0000409);
}

static void PL_AbortSignal (int sig)
{
	(void)sig;
	PL_FatalReport ("abort");
}

#ifdef _MSC_VER
static void PL_InvalidParameter (const wchar_t *expr, const wchar_t *func, const wchar_t *file, unsigned int line, uintptr_t reserved)
{
	(void)expr; (void)func; (void)file; (void)line; (void)reserved;
	PL_FatalReport ("invalid parameter to a CRT function");
}

static void PL_PureCall (void)
{
	PL_FatalReport ("pure virtual call");
}
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
/* Debug builds: the debug CRT's own reports (a failed _ASSERT, a heap check, abort's "Debug Error!") would wait on a
 * dialog. In a test run a warning goes to stderr; an error or an assert writes the report (its message first) and
 * ends the run, as a crash. */
static int __cdecl PL_CrtReportHook (int type, char *message, int *returnValue)
{
	if (type == _CRT_WARN)
		return FALSE; // (written to stderr: _CrtSetReportMode below)
	PL_FatalReport (message ? message : "a debug CRT report");
	*returnValue = 0;
	return TRUE;
}
#endif

extern "C" void VR_InstallCrashHandler (void)
{
	static bool installed = false;
	if (!installed && getenv ("QVR_NO_ERROR_DIALOG"))
	{
		installed = true;
		remove ("qvr_crash.txt");
		remove ("qvr_crash.dmp");
		SetUnhandledExceptionFilter (PL_CrashFilter);
		signal (SIGABRT, PL_AbortSignal);
#ifdef _MSC_VER
		_set_abort_behavior (0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
		_set_invalid_parameter_handler (PL_InvalidParameter);
		_set_purecall_handler (PL_PureCall);
		_set_error_mode (_OUT_TO_STDERR);
#ifdef _DEBUG
		// No debug CRT dialog in a test run: its reports to stderr, and an error's or an assert's to the crash report.
		_CrtSetReportMode (_CRT_WARN, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (_CRT_WARN, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode (_CRT_ERROR, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (_CRT_ERROR, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode (_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (_CRT_ASSERT, _CRTDBG_FILE_STDERR);
		_CrtSetReportHook2 (_CRT_RPTHOOK_INSTALL, PL_CrtReportHook);
#endif
#endif
	}
}

// Installed before the other files' static initialisers as well (the library's segment runs before the user's): a
// failure while the statics are made (a scratch set's, a Zancle container's) reports too instead of waiting on a
// dialog. (main_sdl.c's call finds it done.)
#pragma init_seg(lib)
namespace
{
struct EarlyCrashHandler
{
	EarlyCrashHandler () { VR_InstallCrashHandler (); }
} earlyCrashHandler;
} // namespace

/*
==================
VR_ErrorDialogSuppressed -- in an automated test run, the error written to qvr_error.txt in the working directory
(for the test scripts to print) instead of shown in a dialog: nonzero then
==================
*/
extern "C" int VR_ErrorDialogSuppressed (const char *errorMsg)
{
	FILE *f;
	if (!getenv ("QVR_NO_ERROR_DIALOG"))
		return 0;
	f = fopen ("qvr_error.txt", "w");
	if (f)
	{
		fprintf (f, "%s\n", errorMsg);
		fclose (f);
	}
	return 1;
}

#else

extern "C" void VR_InstallCrashHandler (void)
{
}

extern "C" int VR_ErrorDialogSuppressed (const char *errorMsg)
{
	(void) errorMsg;
	return 0;
}

#endif // _WIN32

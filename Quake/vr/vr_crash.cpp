// vr_crash.cpp -- automated test runs (QVR_NO_ERROR_DIALOG, set by the kit's run.ps1 and the review's child copies):
// a crash writes a report instead of hanging, and an error quits instead of waiting on a dialog (main_sdl.c calls
// VR_InstallCrashHandler; pl_win.c's PL_ErrorDialog, VR_ErrorDialogSuppressed). Windows only; its own translation
// unit: <windows.h> stays out of the engine's headers.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// vr_zancle.cpp: Zancle's failed asserts (ZA_ASSERT) reported as a Quake error (za::setAssertHandler).
extern "C" void VR_InstallZancleAssertHandler (void);

// The build's version: qvr_buildver.h, written by every Visual Studio build in its intermediate folder
// (Windows/VisualStudio/quakevr.props, QvrBuildVersion): QVR_VERSION, the repository's VERSION file ("0.9.0");
// QVR_VERSION_DEV, 1 unless the release script built it; QVR_BUILD_VERSION, the version ("-dev" on a dev build), the last
// commit's date and short hash, "-dirty" with uncommitted changes to tracked files. The CMake and Makefile builds pass
// QVR_VERSION from VERSION (Quake/vr/vr.cmake, vr.mk) and are dev builds.
#if defined(__has_include)
#if __has_include("qvr_buildver.h")
#include "qvr_buildver.h"
#endif
#endif
#ifndef QVR_VERSION
#define QVR_VERSION "0.0.0"
#endif
#ifndef QVR_VERSION_DEV
#define QVR_VERSION_DEV 1
#endif
#ifndef QVR_BUILD_VERSION
#define QVR_BUILD_VERSION QVR_VERSION "-dev (unknown build)"
#endif

extern "C" const char *VR_BuildVersion (void)
{
	return QVR_BUILD_VERSION;
}

extern "C" const char *VR_Version (void)
{
	return QVR_VERSION;
}

extern "C" int VR_VersionIsDev (void)
{
	return QVR_VERSION_DEV;
}

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

/* A crash writes qvr_crash.txt (the exception, the build and the map being played (VR_SetCrashContext), and the
 * crashing thread's stack, with symbols from the build's .pdb) and qvr_crash.dmp (a minidump for a debugger) in the
 * working directory: in an automated test run (QVR_NO_ERROR_DIALOG; run.ps1 prints the stack) and in a player's run
 * alike (only the exception filter there: the crash then ends as before). DbgHelp is loaded only then (no link
 * dependency). */

typedef BOOL (WINAPI *qvr_SymInitialize_t) (HANDLE, PCSTR, BOOL);
typedef DWORD (WINAPI *qvr_SymSetOptions_t) (DWORD);
typedef BOOL (WINAPI *qvr_SymFromAddr_t) (HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL (WINAPI *qvr_SymGetLineFromAddr64_t) (HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
typedef BOOL (WINAPI *qvr_StackWalk64_t) (DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64,
	PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
typedef BOOL (WINAPI *qvr_MiniDumpWriteDump_t) (HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
	PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

static volatile LONG crashEntered = 0; // PL_CrashReport: a report under way (any thread)
static bool crashHandlerInstalled = false; // VR_InstallCrashHandler: once
// VR_SetCrashContext: what the game was doing (the map asked for, the map package mounted): the report's second line.
static char crashContext[512];

static void PL_CrashReport (EXCEPTION_POINTERS *ep, const char *what)
{
	HMODULE dbg;
	HANDLE proc = GetCurrentProcess (), thread = GetCurrentThread ();
	FILE *f;
	if (InterlockedExchange (&crashEntered, 1)) // a second thread crashing meanwhile: leave the first one's report
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
		{
			// The exe's link time (its PE header): which build crashed, whichever file was compiled last.
			const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)GetModuleHandleA (NULL);
			const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const char *)dos + dos->e_lfanew);
			time_t linked = (time_t)nt->FileHeader.TimeDateStamp;
			char when[32] = "?";
			struct tm *tm = localtime (&linked);
			if (tm)
				strftime (when, sizeof (when), "%Y-%m-%d %H:%M", tm);
			fprintf (f, "Quake VR %s, exe linked %s; %s\n", QVR_BUILD_VERSION, when, crashContext[0] ? crashContext : "no map spawned yet");
		}
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
			{
				// The .pdb beside the exe (a player's install; the working directory is the caller's), else the working
				// directory. The path the linker wrote in the exe is tried first (a developer's build).
				char symPath[MAX_PATH + 4] = ".";
				char exePath[MAX_PATH];
				DWORD n = GetModuleFileNameA (NULL, exePath, MAX_PATH);
				char *slash = n > 0 && n < MAX_PATH ? strrchr (exePath, '\\') : NULL;
				if (slash)
				{
					*slash = 0;
					snprintf (symPath, sizeof (symPath), "%s;.", exePath);
				}
				pSymInitialize (proc, symPath, TRUE);
			}
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

// A fatal error found by the game's own checks (a failed Zancle assert, vr_zancle.cpp): reported as the CRT's are.
extern "C" void VR_FatalReport (const char *what)
{
	PL_FatalReport (what);
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

extern "C" void VR_SetCrashContext (const char *what)
{
	snprintf (crashContext, sizeof (crashContext), "%s", what ? what : "");
}

extern "C" void VR_InstallCrashHandler (void)
{
	VR_InstallZancleAssertHandler ();
	bool &installed = crashHandlerInstalled;
	if (!installed && !getenv ("QVR_NO_ERROR_DIALOG"))
	{
		// A player's run: a crash writes the same report (the stack, the map and package, the dump) in the working
		// directory, then ends as it always did (Windows' own report; a debugger attached sees it first). Nothing else
		// changes: no dialog suppressed, and the last report kept until the next crash (it is the evidence).
		installed = true;
		SetUnhandledExceptionFilter (PL_CrashFilter);
	}
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

/*
==================
VR_DescribeCallers -- the calling thread's stack as one line ("fn (file.c:12) < caller (file.c:34) < ..."), skip
frames above the caller left out, at most depth frames, symbols from the build's .pdb: gl_vidsdl.c's GL debug callback
names where a GL error came from. Returns a hash of the frames' addresses (0: no stack).
==================
*/
static int callersSymInit = 0; // VR_DescribeCallers: SymInitialize done (1) or failed (-1)
static HMODULE callersDbg = NULL;

extern "C" unsigned VR_DescribeCallers (char *out, int outSize, int skip, int depth)
{
	void *pcs[32];
	USHORT n;
	unsigned hash = 2166136261u;
	int len = 0;
	HANDLE proc = GetCurrentProcess ();
	qvr_SymFromAddr_t pSymFromAddr = NULL;
	qvr_SymGetLineFromAddr64_t pSymGetLine = NULL;
	if (outSize > 0)
		out[0] = 0;
	if (depth > 32)
		depth = 32;
	n = CaptureStackBackTrace ((DWORD)(skip + 1), (DWORD)depth, pcs, NULL);
	if (!n)
		return 0;
	if (!callersSymInit)
	{
		qvr_SymInitialize_t pSymInitialize;
		qvr_SymSetOptions_t pSymSetOptions;
		callersSymInit = -1;
		callersDbg = LoadLibraryA ("dbghelp.dll");
		pSymInitialize = callersDbg ? (qvr_SymInitialize_t)GetProcAddress (callersDbg, "SymInitialize") : NULL;
		pSymSetOptions = callersDbg ? (qvr_SymSetOptions_t)GetProcAddress (callersDbg, "SymSetOptions") : NULL;
		if (pSymSetOptions)
			pSymSetOptions (SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
		if (pSymInitialize)
		{
			// The crash report's search path (a later crash report's SymInitialize then finds this one's).
			char symPath[MAX_PATH + 4] = ".";
			char exePath[MAX_PATH];
			DWORD len = GetModuleFileNameA (NULL, exePath, MAX_PATH);
			char *slash = len > 0 && len < MAX_PATH ? strrchr (exePath, '\\') : NULL;
			if (slash)
			{
				*slash = 0;
				snprintf (symPath, sizeof (symPath), "%s;.", exePath);
			}
			if (pSymInitialize (proc, symPath, TRUE))
				callersSymInit = 1;
		}
	}
	if (callersSymInit == 1)
	{
		pSymFromAddr = (qvr_SymFromAddr_t)GetProcAddress (callersDbg, "SymFromAddr");
		pSymGetLine = (qvr_SymGetLineFromAddr64_t)GetProcAddress (callersDbg, "SymGetLineFromAddr64");
	}
	for (USHORT i = 0; i < n; i++)
	{
		DWORD64 addr = (DWORD64)(uintptr_t)pcs[i];
		char buf[sizeof (SYMBOL_INFO) + 128];
		SYMBOL_INFO *sym = (SYMBOL_INFO *)buf;
		DWORD64 disp = 0;
		DWORD ldisp = 0;
		IMAGEHLP_LINE64 line;
		char frame[256];
		hash = (hash ^ (unsigned)(addr & 0xffffffffu)) * 16777619u;
		memset (buf, 0, sizeof (buf));
		sym->SizeOfStruct = sizeof (SYMBOL_INFO);
		sym->MaxNameLen = 127;
		memset (&line, 0, sizeof (line));
		line.SizeOfStruct = sizeof (line);
		if (pSymFromAddr && pSymFromAddr (proc, addr, &disp, sym))
		{
			if (pSymGetLine && pSymGetLine (proc, addr, &ldisp, &line))
			{
				const char *file = strrchr (line.FileName, '\\');
				snprintf (frame, sizeof (frame), "%s (%s:%lu)", sym->Name, file ? file + 1 : line.FileName, (unsigned long)line.LineNumber);
			}
			else
				snprintf (frame, sizeof (frame), "%s", sym->Name);
		}
		else
			snprintf (frame, sizeof (frame), "%p", pcs[i]);
		if (outSize > 0 && len < outSize - 1)
			len += snprintf (out + len, (size_t)(outSize - len), "%s%s", i ? " < " : "", frame);
	}
	return hash;
}

#else

extern "C" unsigned VR_DescribeCallers (char *out, int outSize, int skip, int depth)
{
	(void) skip;
	(void) depth;
	if (outSize > 0)
		out[0] = 0;
	return 0;
}

extern "C" void VR_InstallCrashHandler (void)
{
	VR_InstallZancleAssertHandler ();
}

extern "C" void VR_FatalReport (const char *what)
{
	(void) what;
}

extern "C" void VR_SetCrashContext (const char *what)
{
	(void) what;
}

extern "C" int VR_ErrorDialogSuppressed (const char *errorMsg)
{
	(void) errorMsg;
	return 0;
}

#endif // _WIN32

// vr_crash.cpp -- crash reports: every crash and fatal error writes quakevr/crash/<date>_<time>.txt (the stack with
// symbols, the build, the map, the modules) and a minidump, and its dialog names them (main_sdl.c calls
// VR_InstallCrashHandler; sys_sdl_win.c's Sys_ReportError, VR_ErrorReport; pl_win.c's PL_ErrorDialog). Automated test
// runs (QVR_NO_ERROR_DIALOG, set by the kit's run.ps1 and the review's child copies): no dialog, and an error quits
// instead of waiting on one (VR_ErrorDialogSuppressed). Windows only; its own translation unit: <windows.h> stays out
// of the engine's headers.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// vr_zancle.cpp: Zancle's failed asserts (ZA_ASSERT) reported as a Quake error (za::setAssertHandler).
extern "C" void VR_InstallZancleAssertHandler (void);

// The build's version: qvr_buildver.h, written by every Visual Studio build in its intermediate folder
// (Windows/VisualStudio/quakevr.props, QvrBuildVersion): QVR_VERSION, the repository's VERSION file ("1.0.0");
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
#include <tlhelp32.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <intrin.h>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

extern "C" void Con_DebugLog (const char *msg); // console.c: -condebug's qconsole.log
extern "C" char com_gamedir[];                  // common.c: the game folder (the report names it)
extern "C" int VR_GLRecentSteps (char *out, int outSize, int count); // vr_glsafe.cpp: the last GL breadcrumbs
extern "C" void VR_DiagnosticsEnd (int crashed); // vr_diagnostics.cpp: diagnostics mode's logs copied
extern "C" const char *VR_GLSafeDescribe (void);                       // vr_glsafe.cpp: the run's GL safe mode

/* Crash reports (docs/vr-port/TESTING.md, "Crash reports"). Every fatal end of the game writes one:
 *   - a crash: an exception no one handles (an access violation, a stack overflow, ...) on any thread: the unhandled
 *     exception filter (a stack overflow: a vectored handler, which still has the stack's reserve to run on);
 *   - the CRT's fatal errors, which end the process with a fast fail no filter sees: abort() (std::terminate, a failed
 *     assert), an invalid parameter to a CRT function, a pure virtual call;
 *   - an engine error (Sys_Error: its dialog, "Quake VR: Unleashed - Error"; a Host_Error that ends the game is one),
 *     and a failed Zancle assert (vr_zancle.cpp).
 * The report is quakevr/crash/<date>_<time>.txt (what happened, the build, the map, the thread, its stack with function
 * names, files and lines from ironwail.pdb beside the exe, and every module loaded with its address: the offsets let
 * the stack be read again offline with the release's .pdb) and a minidump beside it (.dmp, for a debugger). The report
 * goes to stderr and qconsole.log (-condebug) as well, and the dialog names the files.
 *
 * A dedicated thread writes it (made at start, waiting): the crashed thread may have no stack left, may hold a lock, or
 * may be a worker; it only hands over its context and waits (60 s at most, then the process ends anyway). Nothing on the
 * reporter's way takes the CRT's heap: Win32 files, fixed buffers, DbgHelp loaded at start (MiniDumpWriteDump is meant to
 * be called from another thread than the crashed one). One report per process: a second thread crashing meanwhile
 * waits for the first one's to end the process.
 *
 * Automated test runs (QVR_NO_ERROR_DIALOG): no dialog, and the report also as qvr_crash.txt in the working directory
 * (the kit's run.ps1 prints it as ENGINE CRASH). */

typedef BOOL (WINAPI *qvr_SymInitialize_t) (HANDLE, PCSTR, BOOL);
typedef BOOL (WINAPI *qvr_SymCleanup_t) (HANDLE);
typedef DWORD (WINAPI *qvr_SymSetOptions_t) (DWORD);
typedef BOOL (WINAPI *qvr_SymFromAddr_t) (HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL (WINAPI *qvr_SymGetLineFromAddr64_t) (HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
typedef BOOL (WINAPI *qvr_StackWalk64_t) (DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64,
	PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
typedef BOOL (WINAPI *qvr_MiniDumpWriteDump_t) (HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
	PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
typedef HRESULT (WINAPI *qvr_GetThreadDescription_t) (HANDLE, PWSTR *);
typedef DWORD (WINAPI *qvr_SymAddrIncludeInlineTrace_t) (HANDLE, DWORD64);
typedef BOOL (WINAPI *qvr_SymQueryInlineTrace_t) (HANDLE, DWORD64, DWORD, DWORD64, DWORD64, LPDWORD, LPDWORD);
typedef BOOL (WINAPI *qvr_SymFromInlineContext_t) (HANDLE, DWORD64, ULONG, PDWORD64, PSYMBOL_INFO);
typedef BOOL (WINAPI *qvr_SymGetLineFromInlineContext_t) (HANDLE, DWORD64, ULONG, DWORD64, PDWORD, PIMAGEHLP_LINE64);

#ifdef _M_X64
#define QVR_MACHINE IMAGE_FILE_MACHINE_AMD64
#else
#define QVR_MACHINE IMAGE_FILE_MACHINE_I386
#endif

// The stack of a report without an exception starts at the function that called the one asking (its return address).
#define QVR_CALLER ((DWORD64)(uintptr_t)_ReturnAddress ())

// A report without an exception (Sys_Error, abort, an assert): its record's code, printed as no exception.
#define QVR_NO_EXCEPTION 0u

namespace
{

// DbgHelp's functions, looked up at start (no link dependency; nothing loaded at the crash). DbgHelp is not
// thread-safe: VR_DescribeCallers and the reporter take turns (dbgHelpLock).
struct DbgHelpApi
{
	HMODULE dll;
	qvr_SymInitialize_t symInitialize;
	qvr_SymCleanup_t symCleanup;
	qvr_SymSetOptions_t symSetOptions;
	qvr_SymFromAddr_t symFromAddr;
	qvr_SymGetLineFromAddr64_t symGetLine;
	qvr_StackWalk64_t stackWalk64;
	PFUNCTION_TABLE_ACCESS_ROUTINE64 functionTableAccess;
	PGET_MODULE_BASE_ROUTINE64 getModuleBase;
	qvr_MiniDumpWriteDump_t miniDumpWriteDump;
	qvr_SymAddrIncludeInlineTrace_t symAddrIncludeInlineTrace; // (the inline ones: Windows 8's DbgHelp and later)
	qvr_SymQueryInlineTrace_t symQueryInlineTrace;
	qvr_SymFromInlineContext_t symFromInlineContext;
	qvr_SymGetLineFromInlineContext_t symGetLineFromInlineContext;
	int symState; // SymInitialize: 0 not yet, 1 done, -1 failed
};
DbgHelpApi dbgHelp;
SRWLOCK dbgHelpLock = SRWLOCK_INIT;

// What the crashed (or failing) thread hands to the reporter.
struct CrashRequest
{
	EXCEPTION_POINTERS *ep; // the exception, or a record made for a report without one (QVR_NO_EXCEPTION)
	const char *what;       // what failed (an error's message; NULL: the exception says it)
	DWORD threadId;         // the thread whose stack it is
	DWORD64 startPc;        // the stack shown from the frame with this address (the report's own frames above it left
	                        // out: the caller's return address in the function that asked); 0: from the top
	bool dialog;            // a player's run: the crash's dialog naming the report (Sys_Error shows its own)
	const wchar_t *dialogTitle; // (NULL: the crash's)
};

struct Reporter
{
	HANDLE thread;
	DWORD threadId;
	HANDLE request;   // a report asked for
	HANDLE written;   // the files written
	HANDLE dismissed; // the dialog closed (or none)
	HANDLE finished;  // the copies in stderr and the log written too
	CrashRequest req;
};
Reporter reporter;

volatile LONG reportingThread = 0; // the thread whose report is under way (0: none): one report per process
DWORD mainThreadId = 0;            // the thread that installed the handler (the CRT's start-up: the main thread)
ULONGLONG startTick = 0;           // GetTickCount64 at start (the report's uptime)
bool testRun = false;              // QVR_NO_ERROR_DIALOG: an automated test run
bool crashHandlerInstalled = false; // VR_InstallCrashHandler: once

char crashDir[MAX_PATH * 2] = "quakevr\\crash"; // VR_SetCrashDir: where the reports go (UTF-8; relative: the working directory)
char crashContext[512];                         // VR_SetCrashContext: the map and map package (the report's "Map:")
char reportPath[MAX_PATH * 2];                  // the last report's .txt (UTF-8, full path), "" before one
char dumpPath[MAX_PATH * 2];                    // its .dmp
char report[128 * 1024];                        // the report's text
int reportLen = 0;
int reportHeadLen = 0; // the report's head (what, where, the stack): what stderr, the log and the dialog get
char dialogText[4096];
char frameText[1024];
char crashGpu[512]; // VR_SetCrashGpu: the report's "GPU:" line
char crashVr[1024]; // VR_SetCrashVr: the report's "VR:" line (the OpenXR runtime, its version, the headset, the layers)
char glSteps[6144]; // the last GL breadcrumbs (VR_GLRecentSteps, vr_glsafe.cpp)
// The main thread's context and stack, copied the moment another thread crashed (requestReport; a GL driver's own
// thread: what the game had asked of it). The walk reads the copy: the main thread runs on while the report is written.
constexpr SIZE_T mainStackMax = 512 * 1024;
unsigned char mainStack[mainStackMax];
DWORD64 mainStackLo = 0;
DWORD64 mainStackHi = 0;
CONTEXT mainContext;
bool mainCaptured = false;
wchar_t wideText[8192];
wchar_t widePath[MAX_PATH * 2];

void appendf (const char *fmt, ...)
{
	va_list ap;
	int n;
	if (reportLen >= (int)sizeof (report) - 1)
		return;
	va_start (ap, fmt);
	n = vsnprintf (report + reportLen, sizeof (report) - (size_t)reportLen, fmt, ap);
	va_end (ap);
	if (n > 0)
		reportLen = reportLen + n < (int)sizeof (report) - 1 ? reportLen + n : (int)sizeof (report) - 1;
}

const char *exceptionName (DWORD code)
{
	switch (code)
	{
	case EXCEPTION_ACCESS_VIOLATION: return "access violation";
	case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
	case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
	case EXCEPTION_INT_OVERFLOW: return "integer overflow";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "float divide by zero";
	case EXCEPTION_FLT_INVALID_OPERATION: return "float invalid operation";
	case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned data";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
	case EXCEPTION_IN_PAGE_ERROR: return "in-page error (a file mapped in memory unreadable)";
	case EXCEPTION_BREAKPOINT: return "breakpoint";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "noncontinuable exception";
	case 0xc0000409: return "fast fail (the CRT's or a security check's)";
	case 0xe06d7363: return "C++ exception";
	default: return "exception";
	}
}

// The module holding addr (its file name without the folders) and its base: from the loader, not DbgHelp (that one
// works without symbols).
DWORD64 moduleOf (DWORD64 addr, char *name, int nameSize)
{
	HMODULE mod = NULL;
	snprintf (name, (size_t)nameSize, "?");
	if (!GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)(uintptr_t)addr, &mod) || !mod)
		return 0;
	{
		char path[MAX_PATH];
		const char *slash;
		DWORD n = GetModuleFileNameA (mod, path, MAX_PATH);
		if (n > 0 && n < MAX_PATH)
		{
			slash = strrchr (path, '\\');
			snprintf (name, (size_t)nameSize, "%s", slash ? slash + 1 : path);
		}
	}
	return (DWORD64)(uintptr_t)mod;
}

// A source file's path shortened to the repository's part ("Quake\vr\vr_crash.cpp"), else its name.
const char *shortSourcePath (const char *path)
{
	const char *q = strstr (path, "\\Quake\\");
	const char *slash;
	if (q)
		return q + 1;
	slash = strrchr (path, '\\');
	return slash ? slash + 1 : path;
}

// One frame into frameText: "fn+0x12 (Quake\file.c:34) [ironwail.exe+0x1234]" (symbols: dbgHelpLock held). pc: the
// frame's address; at: the address looked up (a return address less one: the call's own line, not the next one's);
// inlined: a function the compiler put inside that frame's (ThinLTO inlines across files), its inline context.
void describeFrame (DWORD64 pc, DWORD64 at, bool inlined, DWORD inlineContext)
{
	char buf[sizeof (SYMBOL_INFO) + 256];
	SYMBOL_INFO *sym = (SYMBOL_INFO *)buf;
	DWORD64 disp = 0;
	DWORD ldisp = 0;
	IMAGEHLP_LINE64 line;
	char mod[MAX_PATH];
	int len = 0;
	HANDLE proc = GetCurrentProcess ();
	BOOL named = FALSE, lined = FALSE;
	const DWORD64 base = moduleOf (pc, mod, sizeof (mod));
	memset (buf, 0, sizeof (buf));
	sym->SizeOfStruct = sizeof (SYMBOL_INFO);
	sym->MaxNameLen = 255;
	memset (&line, 0, sizeof (line));
	line.SizeOfStruct = sizeof (line);
	frameText[0] = 0;
	if (dbgHelp.symState == 1)
	{
		if (inlined)
		{
			named = dbgHelp.symFromInlineContext && dbgHelp.symFromInlineContext (proc, at, inlineContext, &disp, sym);
			lined = named && dbgHelp.symGetLineFromInlineContext && dbgHelp.symGetLineFromInlineContext (proc, at, inlineContext, 0, &ldisp, &line);
		}
		else
		{
			named = dbgHelp.symFromAddr (proc, at, &disp, sym);
			lined = named && dbgHelp.symGetLine && dbgHelp.symGetLine (proc, at, &ldisp, &line);
		}
	}
	if (named)
	{
		if (inlined)
			len += snprintf (frameText + len, sizeof (frameText) - (size_t)len, "%s", sym->Name);
		else
			len += snprintf (frameText + len, sizeof (frameText) - (size_t)len, "%s+0x%llx", sym->Name, (unsigned long long)(disp + (pc - at)));
		if (lined && len < (int)sizeof (frameText))
			len += snprintf (frameText + len, sizeof (frameText) - (size_t)len, " (%s:%lu)", shortSourcePath (line.FileName), (unsigned long)line.LineNumber);
	}
	else
		len += snprintf (frameText, sizeof (frameText), "?");
	if (len < (int)sizeof (frameText))
	{
		if (inlined)
			snprintf (frameText + len, sizeof (frameText) - (size_t)len, " [inlined]");
		else
			snprintf (frameText + len, sizeof (frameText) - (size_t)len, " [%s+0x%llx]", mod, (unsigned long long)(base ? pc - base : pc));
	}
}

// A frame's lines: the functions inlined at its address first (innermost first), then its own. returnAddress: a
// caller's frame (its address is where the call returns to).
void appendFrame (DWORD64 pc, bool returnAddress, int *shown)
{
	const DWORD64 at = returnAddress && pc ? pc - 1 : pc;
	HANDLE proc = GetCurrentProcess ();
	DWORD inlines = 0;
	if (dbgHelp.symState == 1 && dbgHelp.symAddrIncludeInlineTrace && dbgHelp.symQueryInlineTrace && dbgHelp.symFromInlineContext)
		inlines = dbgHelp.symAddrIncludeInlineTrace (proc, at);
	if (inlines)
	{
		DWORD context = 0, index = 0;
		if (dbgHelp.symQueryInlineTrace (proc, at, 0, at, at, &context, &index))
		{
			for (DWORD k = 0; k < inlines && k < 16; k++, context++)
			{
				describeFrame (pc, at, true, context);
				appendf ("  #%02d %s\n", (*shown)++, frameText);
			}
		}
	}
	describeFrame (pc, at, false, 0);
	appendf ("  #%02d %s\n", (*shown)++, frameText);
}

// DbgHelp's symbols for this process (dbgHelpLock held). Looked for beside the exe (a player's install: ironwail.pdb
// ships there), then in the working directory; the path the linker wrote in the exe first (a developer's build).
void ensureSymbols ()
{
	char symPath[MAX_PATH + 4] = ".";
	char exePath[MAX_PATH];
	DWORD n;
	char *slash;
	if (dbgHelp.symState || !dbgHelp.symInitialize)
		return;
	dbgHelp.symState = -1;
	if (dbgHelp.symSetOptions)
		dbgHelp.symSetOptions (SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
	n = GetModuleFileNameA (NULL, exePath, MAX_PATH);
	slash = n > 0 && n < MAX_PATH ? strrchr (exePath, '\\') : NULL;
	if (slash)
	{
		*slash = 0;
		snprintf (symPath, sizeof (symPath), "%s;.", exePath);
	}
	if (dbgHelp.symInitialize (GetCurrentProcess (), symPath, TRUE))
		dbgHelp.symState = 1;
	else if (dbgHelp.symCleanup)
	{
		// Initialised already by someone else (vr_alloccount's report, which cleans up after itself): again.
		dbgHelp.symCleanup (GetCurrentProcess ());
		if (dbgHelp.symInitialize (GetCurrentProcess (), symPath, TRUE))
			dbgHelp.symState = 1;
	}
}

void loadDbgHelp ()
{
	HMODULE dll = LoadLibraryA ("dbghelp.dll");
	if (!dll)
		return;
	dbgHelp.dll = dll;
	dbgHelp.symInitialize = (qvr_SymInitialize_t)GetProcAddress (dll, "SymInitialize");
	dbgHelp.symCleanup = (qvr_SymCleanup_t)GetProcAddress (dll, "SymCleanup");
	dbgHelp.symSetOptions = (qvr_SymSetOptions_t)GetProcAddress (dll, "SymSetOptions");
	dbgHelp.symFromAddr = (qvr_SymFromAddr_t)GetProcAddress (dll, "SymFromAddr");
	dbgHelp.symGetLine = (qvr_SymGetLineFromAddr64_t)GetProcAddress (dll, "SymGetLineFromAddr64");
	dbgHelp.stackWalk64 = (qvr_StackWalk64_t)GetProcAddress (dll, "StackWalk64");
	dbgHelp.functionTableAccess = (PFUNCTION_TABLE_ACCESS_ROUTINE64)GetProcAddress (dll, "SymFunctionTableAccess64");
	dbgHelp.getModuleBase = (PGET_MODULE_BASE_ROUTINE64)GetProcAddress (dll, "SymGetModuleBase64");
	dbgHelp.miniDumpWriteDump = (qvr_MiniDumpWriteDump_t)GetProcAddress (dll, "MiniDumpWriteDump");
	dbgHelp.symAddrIncludeInlineTrace = (qvr_SymAddrIncludeInlineTrace_t)GetProcAddress (dll, "SymAddrIncludeInlineTrace");
	dbgHelp.symQueryInlineTrace = (qvr_SymQueryInlineTrace_t)GetProcAddress (dll, "SymQueryInlineTrace");
	dbgHelp.symFromInlineContext = (qvr_SymFromInlineContext_t)GetProcAddress (dll, "SymFromInlineContext");
	dbgHelp.symGetLineFromInlineContext = (qvr_SymGetLineFromInlineContext_t)GetProcAddress (dll, "SymGetLineFromInlineContext");
	if (!dbgHelp.symFromAddr)
		dbgHelp.symInitialize = NULL; // (no symbols then: addresses alone)
}

// The reporter's turn with DbgHelp: the lock, unless another thread keeps it 2 s (a crash inside VR_DescribeCallers
// holds it for ever): then without.
bool lockDbgHelpForReport ()
{
	for (int i = 0; i < 200; i++)
	{
		if (TryAcquireSRWLockExclusive (&dbgHelpLock))
			return true;
		Sleep (10);
	}
	return false;
}

// The stack from the thread's context (StackWalk64: the unwind tables), at most 64 frames.
// The frames above the one holding startPc (0: none; not among the top 16: none).
int framesAbove (const CONTEXT *start, HANDLE thread, DWORD64 startPc, PREAD_PROCESS_MEMORY_ROUTINE64 readMemory)
{
	CONTEXT ctx = *start;
	STACKFRAME64 sf;
	int i;
	if (!startPc || !dbgHelp.stackWalk64 || !dbgHelp.functionTableAccess || !dbgHelp.getModuleBase)
		return 0;
	memset (&sf, 0, sizeof (sf));
#ifdef _M_X64
	sf.AddrPC.Offset = ctx.Rip; sf.AddrFrame.Offset = ctx.Rbp; sf.AddrStack.Offset = ctx.Rsp;
#else
	sf.AddrPC.Offset = ctx.Eip; sf.AddrFrame.Offset = ctx.Ebp; sf.AddrStack.Offset = ctx.Esp;
#endif
	sf.AddrPC.Mode = sf.AddrFrame.Mode = sf.AddrStack.Mode = AddrModeFlat;
	for (i = 0; i < 16 && dbgHelp.stackWalk64 (QVR_MACHINE, GetCurrentProcess (), thread, &sf, &ctx, readMemory,
			dbgHelp.functionTableAccess, dbgHelp.getModuleBase, NULL); i++)
		if (sf.AddrPC.Offset == startPc)
			return i;
	return 0;
}

// readMemory: NULL reads the process (the thread's own stack as it is now).
void appendStack (const CONTEXT *start, HANDLE thread, DWORD64 startPc, PREAD_PROCESS_MEMORY_ROUTINE64 readMemory)
{
	const int skip = framesAbove (start, thread, startPc, readMemory);
	CONTEXT ctx = *start;
	STACKFRAME64 sf;
	int i, shown = 0;
	memset (&sf, 0, sizeof (sf));
#ifdef _M_X64
	sf.AddrPC.Offset = ctx.Rip; sf.AddrFrame.Offset = ctx.Rbp; sf.AddrStack.Offset = ctx.Rsp;
#else
	sf.AddrPC.Offset = ctx.Eip; sf.AddrFrame.Offset = ctx.Ebp; sf.AddrStack.Offset = ctx.Esp;
#endif
	sf.AddrPC.Mode = sf.AddrFrame.Mode = sf.AddrStack.Mode = AddrModeFlat;
	if (!dbgHelp.stackWalk64 || !dbgHelp.functionTableAccess || !dbgHelp.getModuleBase)
	{
		describeFrame (sf.AddrPC.Offset, sf.AddrPC.Offset, false, 0);
		appendf ("  #00 %s\n  (no DbgHelp: the rest of the stack is in the .dmp)\n", frameText);
		return;
	}
	DWORD64 lastPc = 0;
	int repeats = 0; // the last frame's repeats not printed (a recursion: one line for them)
	for (i = 0; i < 1000 + skip && shown < 64 && dbgHelp.stackWalk64 (QVR_MACHINE, GetCurrentProcess (), thread, &sf, &ctx, readMemory,
			dbgHelp.functionTableAccess, dbgHelp.getModuleBase, NULL); i++)
	{
		if (!sf.AddrPC.Offset)
			break;
		if (i < skip)
			continue;
		if (sf.AddrPC.Offset == lastPc)
		{
			repeats++;
			continue;
		}
		if (repeats)
			appendf ("      (the same frame %d more times)\n", repeats);
		repeats = 0;
		lastPc = sf.AddrPC.Offset;
		appendFrame (sf.AddrPC.Offset, i > 0, &shown); // (the top frame's address: the instruction itself)
	}
	if (repeats)
		appendf ("      (the same frame %d more times%s)\n", repeats, i >= 1000 + skip ? ", and more not walked" : "");
	if (!shown)
	{
		describeFrame (sf.AddrPC.Offset, sf.AddrPC.Offset, false, 0);
		appendf ("  #00 %s (the stack could not be walked further)\n", frameText);
	}
}

// Every module loaded: base, size, file (the stack's offsets are against these).
void appendModules ()
{
	HANDLE snap = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE, GetCurrentProcessId ());
	MODULEENTRY32W me;
	if (snap == INVALID_HANDLE_VALUE)
	{
		appendf ("  (could not be listed)\n");
		return;
	}
	me.dwSize = sizeof (me);
	if (Module32FirstW (snap, &me))
	{
		do
		{
			char path[MAX_PATH * 3];
			if (!WideCharToMultiByte (CP_UTF8, 0, me.szExePath, -1, path, sizeof (path), NULL, NULL))
				snprintf (path, sizeof (path), "?");
			appendf ("  %016llx %9lu %s\n", (unsigned long long)(uintptr_t)me.modBaseAddr, (unsigned long)me.modBaseSize, path);
		} while (Module32NextW (snap, &me));
	}
	CloseHandle (snap);
}

bool toWide (const char *utf8, wchar_t *out, int outCount)
{
	return MultiByteToWideChar (CP_UTF8, 0, utf8, -1, out, outCount) > 0;
}

void writeFileUtf8Path (const char *path, const char *data, int len)
{
	HANDLE h;
	DWORD done;
	if (!toWide (path, widePath, MAX_PATH * 2))
		return;
	h = CreateFileW (widePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return;
	WriteFile (h, data, (DWORD)len, &done, NULL);
	CloseHandle (h);
}

// The report's file names: <crashDir>\<date>_<time>.txt and .dmp (the folder made; a full path).
void chooseReportPaths ()
{
	SYSTEMTIME t;
	char stem[MAX_PATH * 2];
	wchar_t full[MAX_PATH * 2];
	GetLocalTime (&t);
	if (toWide (crashDir, widePath, MAX_PATH * 2))
	{
		// The folder and its parent (quakevr\crash: quakevr is there already, unless the game folder has moved).
		wchar_t *slash = wcsrchr (widePath, L'\\');
		if (!CreateDirectoryW (widePath, NULL) && GetLastError () == ERROR_PATH_NOT_FOUND && slash)
		{
			*slash = 0;
			CreateDirectoryW (widePath, NULL);
			*slash = L'\\';
			CreateDirectoryW (widePath, NULL);
		}
		if (GetFullPathNameW (widePath, MAX_PATH * 2, full, NULL))
			WideCharToMultiByte (CP_UTF8, 0, full, -1, stem, sizeof (stem), NULL, NULL);
		else
			snprintf (stem, sizeof (stem), "%s", crashDir);
	}
	else
		snprintf (stem, sizeof (stem), ".");
	snprintf (reportPath, sizeof (reportPath), "%s\\%04u-%02u-%02u_%02u-%02u-%02u.txt", stem, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	snprintf (dumpPath, sizeof (dumpPath), "%s\\%04u-%02u-%02u_%02u-%02u-%02u.dmp", stem, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
}

void appendHeader (const CrashRequest &req)
{
	const EXCEPTION_RECORD *er = req.ep->ExceptionRecord;
	SYSTEMTIME t;
	GetLocalTime (&t);
	appendf ("Quake VR: Unleashed crash report\n");
	if (req.what)
		appendf ("What: %s\n", req.what);
	if (er->ExceptionCode != QVR_NO_EXCEPTION)
	{
		describeFrame ((DWORD64)(uintptr_t)er->ExceptionAddress, (DWORD64)(uintptr_t)er->ExceptionAddress, false, 0);
		appendf ("Exception: 0x%08lx, %s", (unsigned long)er->ExceptionCode, exceptionName (er->ExceptionCode));
		if ((er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || er->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) && er->NumberParameters >= 2)
			appendf (" %s 0x%llx", er->ExceptionInformation[0] == 0 ? "reading" : er->ExceptionInformation[0] == 1 ? "writing" : "executing",
				(unsigned long long)er->ExceptionInformation[1]);
		appendf (", at %s\n", frameText);
	}
	{
		// The thread: its id, and its name when it has one (the job system's workers, SDL's threads).
		char name[128] = "";
		HANDLE th = OpenThread (THREAD_QUERY_LIMITED_INFORMATION, FALSE, req.threadId);
		qvr_GetThreadDescription_t getDescription = (qvr_GetThreadDescription_t)GetProcAddress (GetModuleHandleA ("kernel32.dll"), "GetThreadDescription");
		if (th && getDescription)
		{
			PWSTR desc = NULL;
			if (SUCCEEDED (getDescription (th, &desc)) && desc)
			{
				WideCharToMultiByte (CP_UTF8, 0, desc, -1, name, sizeof (name), NULL, NULL);
				LocalFree (desc);
			}
		}
		if (th)
			CloseHandle (th);
		appendf ("Thread: %lu (%s%s%s)\n", (unsigned long)req.threadId, req.threadId == mainThreadId ? "the main thread" : "not the main thread",
			name[0] ? ", " : "", name);
	}
	{
		// The exe's link time (its PE header): which build crashed, whichever file was compiled last.
		const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)GetModuleHandleA (NULL);
		const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const char *)dos + dos->e_lfanew);
		FILETIME ft;
		SYSTEMTIME utc, local;
		ULONGLONG ticks = ((ULONGLONG)nt->FileHeader.TimeDateStamp + 11644473600ull) * 10000000ull;
		ft.dwLowDateTime = (DWORD)ticks;
		ft.dwHighDateTime = (DWORD)(ticks >> 32);
		if (FileTimeToSystemTime (&ft, &utc) && SystemTimeToTzSpecificLocalTime (NULL, &utc, &local))
			appendf ("Build: Quake VR %s, exe linked %04u-%02u-%02u %02u:%02u\n", QVR_BUILD_VERSION, local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute);
		else
			appendf ("Build: Quake VR %s\n", QVR_BUILD_VERSION);
	}
	appendf ("When: %04u-%02u-%02u %02u:%02u:%02u, %.1f s after start\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
		(double)(GetTickCount64 () - startTick) / 1000.0);
	appendf ("GPU: %s; GL safe mode %s\n", crashGpu[0] ? crashGpu : "no GL context yet", VR_GLSafeDescribe ());
	if (crashVr[0])
		appendf ("VR: %s\n", crashVr);
	appendf ("Map: %s; game folder %s\n", crashContext[0] ? crashContext : "no map spawned yet", com_gamedir[0] ? com_gamedir : "not set yet");
	appendf ("Files: %s and %s\n", reportPath, dumpPath);
	appendf ("\nStack (thread %lu):\n", (unsigned long)req.threadId);
}

// The parts that can fail on a damaged process each in their own __try: what is written so far stays.
void tryStack (const CrashRequest *req)
{
	__try
	{
		HANDLE th = OpenThread (THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, req->threadId);
		appendStack (req->ep->ContextRecord, th ? th : GetCurrentThread (), req->startPc, NULL);
		if (th)
			CloseHandle (th);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		appendf ("  (the stack walk failed: the .dmp has it)\n");
	}
}

// The main thread's stack copied at the crash (snapshotMainThread), when another thread crashed.
BOOL CALLBACK readMainStack (HANDLE process, DWORD64 base, PVOID buffer, DWORD size, LPDWORD read)
{
	SIZE_T got = 0;
	BOOL ok;
	if (base >= mainStackLo && base + size <= mainStackHi)
	{
		memcpy (buffer, mainStack + (base - mainStackLo), size);
		*read = size;
		return TRUE;
	}
	ok = ReadProcessMemory (process, (LPCVOID)(uintptr_t)base, buffer, size, &got);
	*read = (DWORD)got;
	return ok;
}

void tryMainStack (const CrashRequest *req)
{
	if (!mainCaptured || req->threadId == mainThreadId)
		return;
	__try
	{
		appendf ("\nThe main thread's stack when it crashed (thread %lu):\n", (unsigned long)mainThreadId);
		HANDLE th = OpenThread (THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, mainThreadId);
		appendStack (&mainContext, th ? th : GetCurrentThread (), 0, readMainStack);
		if (th)
			CloseHandle (th);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		appendf ("  (the walk failed: the .dmp has it)\n");
	}
}

// The last GL steps (vr_glsafe.cpp): the start-up's, and the shaders compiled since.
void tryGLSteps ()
{
	__try
	{
		if (VR_GLRecentSteps (glSteps, sizeof (glSteps), 12) > 0)
			appendf ("\nLast GL steps (oldest first; each named before its GL calls ran):\n%s", glSteps);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void tryModules ()
{
	__try
	{
		appendModules ();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		appendf ("  (the list failed)\n");
	}
}

void tryHeader (const CrashRequest *req)
{
	__try
	{
		appendHeader (*req);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		appendf ("(the report's head failed)\n");
	}
}

void tryDump (const CrashRequest *req)
{
	__try
	{
		HANDLE h;
		MINIDUMP_EXCEPTION_INFORMATION mei;
		if (!dbgHelp.miniDumpWriteDump || !toWide (dumpPath, widePath, MAX_PATH * 2))
			return;
		h = CreateFileW (widePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE)
			return;
		mei.ThreadId = req->threadId;
		mei.ExceptionPointers = req->ep;
		mei.ClientPointers = FALSE;
		// Small: every thread's stack and registers, the memory they point at, the modules (a few MB).
		dbgHelp.miniDumpWriteDump (GetCurrentProcess (), GetCurrentProcessId (), h,
			(MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules), &mei, NULL, NULL);
		CloseHandle (h);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void tryLog ()
{
	__try
	{
		// qconsole.log (-condebug): the head (the modules are in the file).
		char saved = report[reportHeadLen];
		report[reportHeadLen] = 0;
		Con_DebugLog ("\n");
		Con_DebugLog (report);
		report[reportHeadLen] = saved;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

// Diagnostics mode: the logs copied beside the report (vr_diagnostics.cpp).
void tryDiagnostics ()
{
	__try
	{
		VR_DiagnosticsEnd (1);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void showDialog (const CrashRequest &req)
{
	const EXCEPTION_RECORD *er = req.ep->ExceptionRecord;
	char what[1200];
	if (req.what)
		snprintf (what, sizeof (what), "%s", req.what);
	else
		snprintf (what, sizeof (what), "%s (0x%08lx)", exceptionName (er->ExceptionCode), (unsigned long)er->ExceptionCode);
	snprintf (dialogText, sizeof (dialogText),
		"Quake VR has crashed: %s\n\nA crash report was saved:\n%s\n%s\n\nPlease attach both files to your bug report, with what you were doing.",
		what, reportPath, dumpPath);
	if (!toWide (dialogText, wideText, 8192))
		return;
	MessageBoxW (NULL, wideText, req.dialogTitle ? req.dialogTitle : L"Quake VR: Unleashed - Crash",
		MB_OK | MB_ICONSTOP | MB_SETFOREGROUND | MB_TOPMOST);
}

// The report itself (the reporter's thread, or the crashed one's when the reporter could not be made).
void writeReport (const CrashRequest &req)
{
	const bool locked = lockDbgHelpForReport ();
	reportLen = 0;
	chooseReportPaths ();
	if (locked)
	{
		__try
		{
			ensureSymbols ();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			dbgHelp.symState = -1;
		}
	}
	tryHeader (&req);
	tryStack (&req);
	tryMainStack (&req);
	tryGLSteps ();
	reportHeadLen = reportLen;
	appendf ("\nCommand line: %s\n", GetCommandLineA ());
	appendf ("\nModules (base, size, file):\n");
	tryModules ();
	writeFileUtf8Path (reportPath, report, reportLen);
	if (testRun)
		writeFileUtf8Path ("qvr_crash.txt", report, reportLen); // (the kit's run.ps1 prints it)
	tryDump (&req);
	tryDiagnostics ();
	if (locked)
		ReleaseSRWLockExclusive (&dbgHelpLock);
}

void echoReport ()
{
	HANDLE err = GetStdHandle (STD_ERROR_HANDLE);
	DWORD done;
	if (err && err != INVALID_HANDLE_VALUE)
		WriteFile (err, report, (DWORD)reportHeadLen, &done, NULL);
	{
		char saved = report[reportHeadLen];
		report[reportHeadLen] = 0;
		OutputDebugStringA (report);
		report[reportHeadLen] = saved;
	}
	tryLog ();
}

DWORD WINAPI reporterMain (void *)
{
	WaitForSingleObject (reporter.request, INFINITE);
	writeReport (reporter.req);
	SetEvent (reporter.written);
	if (reporter.req.dialog)
		showDialog (reporter.req);
	SetEvent (reporter.dismissed);
	echoReport ();
	SetEvent (reporter.finished);
	return 0;
}

void startReporter ()
{
	reporter.request = CreateEventW (NULL, TRUE, FALSE, NULL);
	reporter.written = CreateEventW (NULL, TRUE, FALSE, NULL);
	reporter.dismissed = CreateEventW (NULL, TRUE, FALSE, NULL);
	reporter.finished = CreateEventW (NULL, TRUE, FALSE, NULL);
	if (!reporter.request || !reporter.written || !reporter.dismissed || !reporter.finished)
		return;
	// Its own stack (DbgHelp's PDB reader is deep), made now: a crashed process may not manage it.
	reporter.thread = CreateThread (NULL, 1024 * 1024, reporterMain, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, &reporter.threadId);
	if (reporter.thread)
	{
		typedef HRESULT (WINAPI *SetThreadDescription_t) (HANDLE, PCWSTR);
		SetThreadDescription_t setDescription = (SetThreadDescription_t)GetProcAddress (GetModuleHandleA ("kernel32.dll"), "SetThreadDescription");
		if (setDescription)
			setDescription (reporter.thread, L"Crash reporter");
	}
}

/* Asks for the report of the calling thread's failure and waits for it. false: no report from this call (this
 * thread's report is under way already: a crash in the shutdown after an error's report, or in the report itself).
 * A second thread failing while another's report is under way never returns: that one ends the process. */
/* Another thread crashed: the main thread's context and stack copied now (it is suspended only for that: no lock it
 * may hold is waited for), before it runs on. A stack overflow's thread has no room for this. */
void snapshotMainThread (EXCEPTION_POINTERS *ep)
{
	HANDLE th;
	if (!mainThreadId || GetCurrentThreadId () == mainThreadId || !ep || ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
		return;
	th = OpenThread (THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, mainThreadId);
	if (!th)
		return;
	if (SuspendThread (th) != (DWORD)-1)
	{
		__try
		{
			MEMORY_BASIC_INFORMATION mbi;
			memset (&mainContext, 0, sizeof (mainContext));
			mainContext.ContextFlags = CONTEXT_FULL;
			if (GetThreadContext (th, &mainContext))
			{
#ifdef _M_X64
				const DWORD64 lo = mainContext.Rsp;
#else
				const DWORD64 lo = mainContext.Esp;
#endif
				if (VirtualQuery ((LPCVOID)(uintptr_t)lo, &mbi, sizeof (mbi)))
				{
					DWORD64 hi = (DWORD64)(uintptr_t)mbi.BaseAddress + mbi.RegionSize;
					if (hi - lo > mainStackMax)
						hi = lo + mainStackMax;
					memcpy (mainStack, (const void *)(uintptr_t)lo, (size_t)(hi - lo));
					mainStackLo = lo;
					mainStackHi = hi;
					mainCaptured = true;
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			mainCaptured = false;
		}
		ResumeThread (th);
	}
	CloseHandle (th);
}

bool requestReport (EXCEPTION_POINTERS *ep, const char *what, DWORD64 startPc, bool dialog, const wchar_t *dialogTitle)
{
	const DWORD me = GetCurrentThreadId ();
	LONG prev;
	if (me == reporter.threadId)
		return false;
	prev = InterlockedCompareExchange (&reportingThread, (LONG)me, 0);
	if (prev != 0)
	{
		if ((DWORD)prev == me)
			return false;
		for (;;)
			Sleep (INFINITE);
	}
	snapshotMainThread (what ? NULL : ep); // (an exception's: an error or a failed check names its own place)
	reporter.req.ep = ep;
	reporter.req.what = what;
	reporter.req.threadId = me;
	reporter.req.startPc = startPc;
	reporter.req.dialog = dialog && !testRun;
	reporter.req.dialogTitle = dialogTitle;
	if (reporter.thread)
	{
		SetEvent (reporter.request);
		// The files: a minute at most (a slow disk reading the .pdb; a lock the crashed thread holds, for ever). Then the
		// dialog, as long as it is up; then the copies (the log's lock may be this thread's: a few seconds).
		if (WaitForSingleObject (reporter.written, 60000) == WAIT_OBJECT_0)
		{
			WaitForSingleObject (reporter.dismissed, INFINITE);
			WaitForSingleObject (reporter.finished, 5000);
		}
	}
	else
	{
		writeReport (reporter.req);
		if (reporter.req.dialog)
			showDialog (reporter.req);
		echoReport ();
	}
	return true;
}

/* A report without an exception (an error, abort, an assert): the calling thread's context, its stack from the
 * frame holding startPc on (QVR_CALLER in the function asking: its caller's frame; the report's own left out). */
__declspec(noinline) bool reportHere (const char *what, DWORD64 startPc, bool dialog, const wchar_t *dialogTitle)
{
	CONTEXT ctx;
	EXCEPTION_RECORD er;
	EXCEPTION_POINTERS ep;
	memset (&er, 0, sizeof (er));
	RtlCaptureContext (&ctx);
	er.ExceptionCode = QVR_NO_EXCEPTION;
#ifdef _M_X64
	er.ExceptionAddress = (PVOID)(uintptr_t)ctx.Rip;
#else
	er.ExceptionAddress = (PVOID)(uintptr_t)ctx.Eip;
#endif
	ep.ExceptionRecord = &er;
	ep.ContextRecord = &ctx;
	return requestReport (&ep, what, startPc, dialog, dialogTitle);
}

LONG WINAPI crashFilter (EXCEPTION_POINTERS *ep)
{
	if (!requestReport (ep, NULL, 0, true, NULL))
		return EXCEPTION_CONTINUE_SEARCH; // (this thread's report is under way: the process ends as a crash)
	TerminateProcess (GetCurrentProcess (), ep->ExceptionRecord->ExceptionCode);
	return EXCEPTION_EXECUTE_HANDLER;
}

/* A stack overflow: first thing, from the vectored handler (the unhandled filter runs after the dispatcher's search,
 * deeper in what is left of the stack). Only that exception: an access violation is often caught by whoever caused
 * it (the GL driver, the OpenXR runtime probe memory and handle their own), so those wait for the unhandled filter. */
LONG CALLBACK stackOverflowHandler (EXCEPTION_POINTERS *ep)
{
	if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_STACK_OVERFLOW || IsDebuggerPresent ())
		return EXCEPTION_CONTINUE_SEARCH;
	return crashFilter (ep);
}

/* The CRT's fatal errors end the process with a fast fail (0xc0000409), which no exception filter sees: abort()
 * (std::terminate, a failed assert), an invalid parameter to a CRT function, a pure virtual call. Their report too,
 * from the caller's stack, and the same exit code. */
void fatalReport (const char *what, DWORD64 startPc, UINT exitCode, const wchar_t *dialogTitle)
{
	reportHere (what, startPc, true, dialogTitle);
	TerminateProcess (GetCurrentProcess (), exitCode);
}

void abortSignal (int sig)
{
	(void)sig;
	fatalReport ("abort() (std::terminate, a failed assert, or the game's own fatal check)", QVR_CALLER, 0xc0000409, NULL);
}

#ifdef _MSC_VER
void invalidParameter (const wchar_t *expr, const wchar_t *func, const wchar_t *file, unsigned int line, uintptr_t reserved)
{
	(void)expr; (void)func; (void)file; (void)line; (void)reserved;
	fatalReport ("an invalid parameter to a CRT function", QVR_CALLER, 0xc0000409, NULL);
}

void pureCall (void)
{
	fatalReport ("a pure virtual call", QVR_CALLER, 0xc0000409, NULL);
}
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
/* Debug builds: the debug CRT's own reports (a failed _ASSERT, a heap check, abort's "Debug Error!") would wait on a
 * dialog. In a test run a warning goes to stderr; an error or an assert writes the report (its message first) and
 * ends the run, as a crash. */
int __cdecl crtReportHook (int type, char *message, int *returnValue)
{
	if (type == _CRT_WARN)
		return FALSE; // (written to stderr: _CrtSetReportMode below)
	fatalReport (message ? message : "a debug CRT report", QVR_CALLER, 0xc0000409, NULL);
	*returnValue = 0;
	return TRUE;
}
#endif

} // namespace

// A fatal error found by the game's own checks (a failed Zancle assert off the main thread or before the engine is
// up, vr_zancle.cpp): the report, its dialog, the process ended.
extern "C" __declspec(noinline) void VR_FatalReport (const char *what)
{
	fatalReport (what, QVR_CALLER, 0xc0000409, NULL);
}

/* Sys_Error: the report, with the stack from Sys_ReportError's caller; the error's own dialog names it
 * (PL_ErrorDialog, VR_LastCrashReport). Off the main thread (VR_FatalError): the report, a dialog, and the process
 * ended there (the engine's shutdown is the main thread's). */
extern "C" __declspec(noinline) void VR_ErrorReport (const char *message)
{
	char what[1100];
	snprintf (what, sizeof (what), "Sys_Error: %s", message);
	reportHere (what, QVR_CALLER, false, NULL); // (the stack from Sys_ReportError)
}

extern "C" __declspec(noinline) void VR_FatalError (const char *message)
{
	char what[1100];
	snprintf (what, sizeof (what), "Sys_Error (not on the main thread): %s", message);
	reportHere (what, QVR_CALLER, true, L"Quake VR: Unleashed - Error");
	TerminateProcess (GetCurrentProcess (), 1);
}

extern "C" const char *VR_LastCrashReport (void)
{
	return reportPath;
}

extern "C" void VR_SetCrashGpu (const char *line)
{
	snprintf (crashGpu, sizeof (crashGpu), "%s", line ? line : "");
}

extern "C" void VR_SetCrashVr (const char *line)
{
	snprintf (crashVr, sizeof (crashVr), "%s", line ? line : "");
}

extern "C" void VR_SetCrashContext (const char *what)
{
	snprintf (crashContext, sizeof (crashContext), "%s", what ? what : "");
	// Ours still the unhandled exception filter (a DLL loaded since may have put its own: the VR runtime's, a driver's).
	if (crashHandlerInstalled)
		SetUnhandledExceptionFilter (crashFilter);
}

extern "C" void VR_SetCrashDir (const char *dir)
{
	// UTF-8, made full now (the working directory can't change the report's place later).
	wchar_t full[MAX_PATH * 2];
	char path[MAX_PATH * 2];
	if (!dir || !dir[0])
		return;
	snprintf (path, sizeof (path), "%s\\crash", dir);
	for (char *c = path; *c; c++)
		if (*c == '/')
			*c = '\\';
	if (toWide (path, widePath, MAX_PATH * 2) && GetFullPathNameW (widePath, MAX_PATH * 2, full, NULL))
		WideCharToMultiByte (CP_UTF8, 0, full, -1, path, sizeof (path), NULL, NULL);
	snprintf (crashDir, sizeof (crashDir), "%s", path);
}

extern "C" void VR_InstallCrashHandler (void)
{
	VR_InstallZancleAssertHandler ();
	if (crashHandlerInstalled)
		return;
	crashHandlerInstalled = true;
	testRun = getenv ("QVR_NO_ERROR_DIALOG") != NULL;
	mainThreadId = GetCurrentThreadId ();
	startTick = GetTickCount64 ();
	if (testRun)
		DeleteFileA ("qvr_crash.txt");
	loadDbgHelp ();
	startReporter ();
	// The main thread's reserve for a stack overflow's handler (the vectored one: hands over and waits).
	{
		ULONG reserve = 64 * 1024;
		SetThreadStackGuarantee (&reserve);
	}
	AddVectoredExceptionHandler (1, stackOverflowHandler);
	SetUnhandledExceptionFilter (crashFilter);
	signal (SIGABRT, abortSignal);
#ifdef _MSC_VER
	_set_abort_behavior (0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_set_invalid_parameter_handler (invalidParameter);
	_set_purecall_handler (pureCall);
	if (testRun)
		_set_error_mode (_OUT_TO_STDERR);
#ifdef _DEBUG
	if (testRun)
	{
		// No debug CRT dialog in a test run: its reports to stderr, and an error's or an assert's to the crash report.
		_CrtSetReportMode (_CRT_WARN, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (_CRT_WARN, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode (_CRT_ERROR, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (_CRT_ERROR, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode (_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (_CRT_ASSERT, _CRTDBG_FILE_STDERR);
		_CrtSetReportHook2 (_CRT_RPTHOOK_INSTALL, crtReportHook);
	}
#endif
#endif
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
names where a GL error came from, Host_Error its caller. Returns a hash of the frames' addresses (0: no stack).
==================
*/
extern "C" unsigned VR_DescribeCallers (char *out, int outSize, int skip, int depth)
{
	void *pcs[32];
	USHORT n;
	unsigned hash = 2166136261u;
	int len = 0;
	HANDLE proc = GetCurrentProcess ();
	if (outSize > 0)
		out[0] = 0;
	if (depth > 32)
		depth = 32;
	n = CaptureStackBackTrace ((DWORD)(skip + 1), (DWORD)depth, pcs, NULL);
	if (!n)
		return 0;
	AcquireSRWLockExclusive (&dbgHelpLock);
	ensureSymbols ();
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
		if (dbgHelp.symState == 1 && dbgHelp.symFromAddr (proc, addr, &disp, sym))
		{
			if (dbgHelp.symGetLine && dbgHelp.symGetLine (proc, addr, &ldisp, &line))
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
	ReleaseSRWLockExclusive (&dbgHelpLock);
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

extern "C" void VR_ErrorReport (const char *message)
{
	(void) message;
}

extern "C" void VR_FatalError (const char *message)
{
	(void) message;
}

extern "C" const char *VR_LastCrashReport (void)
{
	return "";
}

extern "C" void VR_SetCrashContext (const char *what)
{
	(void) what;
}

extern "C" void VR_SetCrashGpu (const char *line)
{
	(void) line;
}

extern "C" void VR_SetCrashVr (const char *line)
{
	(void) line;
}

extern "C" void VR_SetCrashDir (const char *dir)
{
	(void) dir;
}

extern "C" int VR_ErrorDialogSuppressed (const char *errorMsg)
{
	(void) errorMsg;
	return 0;
}

#endif // _WIN32

/* vi:ts=4:sw=4
 *
 * w32crash.c
 *
 * Crash reporting for the Win32 (mingw) build of JVim.
 *
 * JVim used to wrap main() in
 *      __try { main(); } __except (EXCEPTION_CONTINUE_EXECUTION) { ; }
 * which resumed execution at the faulting instruction. Faults were therefore
 * invisible and the editor kept running on corrupted state. Instead we install
 * a last chance exception filter that records where we died and then lets the
 * process go down.
 *
 * On a fault this writes
 *      <dir>/jvim-crash-<pid>-<time>.log   human readable report
 *      <dir>/jvim-crash-<pid>-<time>.dmp   minidump (if dbghelp.dll is there)
 * where <dir> is $JVIM_CRASHDIR, else %LOCALAPPDATA%\jvim3, else %TEMP%.
 *
 * Addresses are logged twice: as loaded, and relocated back to the address the
 * image was linked at ("static"). Feed the static one to addr2line to get
 * file:line, see ../scripts/resolve-crash.sh.
 *
 * Set $JVIM_CRASH_QUIET to suppress the message box.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"

#ifdef NT

#define CRASH_MAXFRAMES		62

static char			crash_log[MAXPATHL * 2];
static char			crash_dmp[MAXPATHL * 2];
static int			crash_gui;
static volatile LONG	crash_busy;

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE, DWORD, HANDLE, DWORD,
										 PVOID, PVOID, PVOID);
typedef USHORT (WINAPI *CAPTURESTACKBACKTRACE)(ULONG, ULONG, PVOID *, PULONG);

/*
 * Directory the reports go to. Never fails: falls back to the current dir.
 */
	static void
crash_getdir(buf, len)
	char	*buf;
	int		len;
{
	char	*p;

	if ((p = getenv("JVIM_CRASHDIR")) != NULL && *p != '\0')
	{
		lstrcpynA(buf, p, len);
		return;
	}
	if ((p = getenv("LOCALAPPDATA")) != NULL && *p != '\0')
	{
		wsprintfA(buf, "%.*s\\jvim3", len - 8, p);
		CreateDirectoryA(buf, NULL);
		return;
	}
	if ((p = getenv("TEMP")) != NULL && *p != '\0')
	{
		lstrcpynA(buf, p, len);
		return;
	}
	lstrcpynA(buf, ".", len);
}

	static const char *
crash_excname(code)
	DWORD	code;
{
	switch (code)
	{
	case EXCEPTION_ACCESS_VIOLATION:		return "ACCESS_VIOLATION";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:	return "ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_DATATYPE_MISALIGNMENT:	return "DATATYPE_MISALIGNMENT";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:		return "FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_ILLEGAL_INSTRUCTION:		return "ILLEGAL_INSTRUCTION";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:		return "INT_DIVIDE_BY_ZERO";
	case EXCEPTION_IN_PAGE_ERROR:			return "IN_PAGE_ERROR";
	case EXCEPTION_PRIV_INSTRUCTION:		return "PRIV_INSTRUCTION";
	case EXCEPTION_STACK_OVERFLOW:			return "STACK_OVERFLOW";
	case EXCEPTION_BREAKPOINT:				return "BREAKPOINT";
	default:								return "UNKNOWN";
	}
}

/*
 * Describe an address: owning module, offset in it, and the address the module
 * was linked at (what addr2line wants, ASLR undone).
 */
	static void
crash_where(fp, what, addr)
	FILE			*fp;
	const char		*what;
	void			*addr;
{
	MEMORY_BASIC_INFORMATION	mbi;
	char						name[MAXPATHL];
	HMODULE						base;
	PIMAGE_DOS_HEADER			dos;
	PIMAGE_NT_HEADERS			nth;
	DWORD_PTR					linked;

	if (VirtualQuery(addr, &mbi, sizeof(mbi)) != sizeof(mbi)
			|| mbi.AllocationBase == NULL)
	{
		fprintf(fp, "%s %p  <unmapped>\n", what, addr);
		return;
	}
	base = (HMODULE)mbi.AllocationBase;
	if (GetModuleFileNameA(base, name, sizeof(name)) == 0)
		lstrcpynA(name, "<unknown module>", sizeof(name));

	linked = (DWORD_PTR)base;
	dos = (PIMAGE_DOS_HEADER)base;
	if (dos->e_magic == IMAGE_DOS_SIGNATURE)
	{
		nth = (PIMAGE_NT_HEADERS)((char *)base + dos->e_lfanew);
		if (nth->Signature == IMAGE_NT_SIGNATURE)
			linked = (DWORD_PTR)nth->OptionalHeader.ImageBase;
	}

	fprintf(fp, "%s %p  %s+0x%lx  static=0x%lx\n",
			what, addr, gettail((char_u *)name),
			(unsigned long)((DWORD_PTR)addr - (DWORD_PTR)base),
			(unsigned long)((DWORD_PTR)addr - (DWORD_PTR)base + linked));
}

/*
 * Walk the frame pointer chain starting at the faulting frame. Needs
 * -fno-omit-frame-pointer, which makefile.mingw passes.
 */
	static void
crash_backtrace(fp, ep)
	FILE				*fp;
	EXCEPTION_POINTERS	*ep;
{
#if defined(_X86_) || defined(__i386__)
	DWORD_PTR	*frame;
	int			depth;

	fprintf(fp, "\nstack (frame pointer chain):\n");
	crash_where(fp, "  #00", (void *)ep->ContextRecord->Eip);
	frame = (DWORD_PTR *)ep->ContextRecord->Ebp;
	for (depth = 1; depth < CRASH_MAXFRAMES; depth++)
	{
		char	label[8];

		if (frame == NULL
				|| IsBadReadPtr(frame, sizeof(DWORD_PTR) * 2)
				|| frame[1] == 0)
			break;
		wsprintfA(label, "  #%02d", depth);
		crash_where(fp, label, (void *)frame[1]);
		if ((DWORD_PTR *)frame[0] <= frame)		/* not walking up: give up */
			break;
		frame = (DWORD_PTR *)frame[0];
	}
#else
	CAPTURESTACKBACKTRACE	capture;
	HMODULE					k32;
	void					*frames[CRASH_MAXFRAMES];
	USHORT					n, i;

	fprintf(fp, "\nstack (captured):\n");
	crash_where(fp, "  #00", (void *)ep->ContextRecord->Rip);
	if ((k32 = GetModuleHandleA("kernel32.dll")) == NULL)
		return;
	capture = (CAPTURESTACKBACKTRACE)GetProcAddress(k32,
											"RtlCaptureStackBackTrace");
	if (capture == NULL)
		return;
	n = capture(0, CRASH_MAXFRAMES, frames, NULL);
	for (i = 0; i < n; i++)
	{
		char	label[8];

		wsprintfA(label, "  #%02d", i + 1);
		crash_where(fp, label, frames[i]);
	}
#endif
}

	static void
crash_registers(fp, ctx)
	FILE		*fp;
	CONTEXT		*ctx;
{
#if defined(_X86_) || defined(__i386__)
	fprintf(fp, "\nregisters:\n");
	fprintf(fp, "  eip=%08lx esp=%08lx ebp=%08lx eflags=%08lx\n",
			(unsigned long)ctx->Eip, (unsigned long)ctx->Esp,
			(unsigned long)ctx->Ebp, (unsigned long)ctx->EFlags);
	fprintf(fp, "  eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx\n",
			(unsigned long)ctx->Eax, (unsigned long)ctx->Ebx,
			(unsigned long)ctx->Ecx, (unsigned long)ctx->Edx);
	fprintf(fp, "  esi=%08lx edi=%08lx\n",
			(unsigned long)ctx->Esi, (unsigned long)ctx->Edi);
#else
	fprintf(fp, "\nregisters:\n");
	fprintf(fp, "  rip=%016llx rsp=%016llx rbp=%016llx\n",
			(unsigned long long)ctx->Rip, (unsigned long long)ctx->Rsp,
			(unsigned long long)ctx->Rbp);
#endif
}

/*
 * What was the editor doing? Written last, because reading the editor state
 * after memory corruption may fault again; everything above is already
 * flushed by then.
 */
	static void
crash_editorstate(fp)
	FILE	*fp;
{
	fprintf(fp, "\neditor state:\n");
	fflush(fp);
	if (curbuf != NULL)
	{
		fprintf(fp, "  file    %s\n", curbuf->b_filename != NULL
								? (char *)curbuf->b_filename : "<none>");
		fflush(fp);
		fprintf(fp, "  changed %d  readonly %d  jcode %s\n",
				(int)curbuf->b_changed, (int)curbuf->b_p_ro,
				curbuf->b_p_jc != NULL ? (char *)curbuf->b_p_jc : "?");
		fflush(fp);
	}
	if (curwin != NULL)
		fprintf(fp, "  cursor  line %ld col %d\n",
				(long)curwin->w_cursor.lnum, (int)curwin->w_cursor.col);
	fprintf(fp, "  State   %d  Rows %ld Columns %ld\n",
			State, (long)Rows, (long)Columns);
	fflush(fp);
}

	static LONG WINAPI
crash_filter(ep)
	EXCEPTION_POINTERS	*ep;
{
	FILE			*fp;
	SYSTEMTIME		st;
	char			dir[MAXPATHL];
	char			exe[MAXPATHL];
	DWORD			code;

	/* A fault while reporting must not loop. */
	if (InterlockedExchange(&crash_busy, 1) != 0)
		return EXCEPTION_EXECUTE_HANDLER;

	code = ep->ExceptionRecord->ExceptionCode;
	GetLocalTime(&st);
	crash_getdir(dir, sizeof(dir));
	wsprintfA(crash_log, "%s\\jvim-crash-%lu-%04d%02d%02d-%02d%02d%02d.log",
			dir, (unsigned long)GetCurrentProcessId(),
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	wsprintfA(crash_dmp, "%s\\jvim-crash-%lu-%04d%02d%02d-%02d%02d%02d.dmp",
			dir, (unsigned long)GetCurrentProcessId(),
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	if (GetModuleFileNameA(NULL, exe, sizeof(exe)) == 0)
		lstrcpynA(exe, "?", sizeof(exe));

	if ((fp = fopen(crash_log, "w")) != NULL)
	{
		fprintf(fp, "JVim crash report\n");
		fprintf(fp, "  when      %04d-%02d-%02d %02d:%02d:%02d\n",
				st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond);
		fprintf(fp, "  version   %s\n", (char *)longVersion);
#ifdef KANJI
		fprintf(fp, "            %s\n", (char *)longJpVersion);
#endif
		fprintf(fp, "  exe       %s\n", exe);
		fprintf(fp, "  cmdline   %s\n", GetCommandLineA());
		fprintf(fp, "  pid       %lu\n",
				(unsigned long)GetCurrentProcessId());
		fprintf(fp, "\nexception %08lx (%s)\n",
				(unsigned long)code, crash_excname(code));
		if (code == EXCEPTION_ACCESS_VIOLATION
				&& ep->ExceptionRecord->NumberParameters >= 2)
			fprintf(fp, "  %s address %p\n",
					ep->ExceptionRecord->ExceptionInformation[0] ? "writing"
																 : "reading",
					(void *)ep->ExceptionRecord->ExceptionInformation[1]);
		crash_where(fp, "  faulting pc", ep->ExceptionRecord->ExceptionAddress);
		crash_registers(fp, ep->ContextRecord);
		fflush(fp);
		crash_backtrace(fp, ep);
		fflush(fp);
		crash_editorstate(fp);
		fprintf(fp, "\nResolve the static addresses with:\n"
					"  addr2line -e <exe> -f -C -i <static addr> ...\n"
					"or run scripts/resolve-crash.sh on this file.\n");
		fclose(fp);
	}

	/* Minidump is a bonus: only if dbghelp.dll happens to be available. */
	{
		HMODULE				dbg;
		MINIDUMPWRITEDUMP	mdwd;
		HANDLE				fh;

		if ((dbg = LoadLibraryA("dbghelp.dll")) != NULL
			&& (mdwd = (MINIDUMPWRITEDUMP)GetProcAddress(dbg,
											"MiniDumpWriteDump")) != NULL)
		{
			fh = CreateFileA(crash_dmp, GENERIC_WRITE, 0, NULL,
							CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (fh != INVALID_HANDLE_VALUE)
			{
				struct {
					DWORD				ThreadId;
					EXCEPTION_POINTERS	*ExceptionPointers;
					BOOL				ClientPointers;
				} mei;

				mei.ThreadId = GetCurrentThreadId();
				mei.ExceptionPointers = ep;
				mei.ClientPointers = FALSE;
				mdwd(GetCurrentProcess(), GetCurrentProcessId(), fh,
						0x0002 /* MiniDumpWithFullMemory */, &mei, NULL, NULL);
				CloseHandle(fh);
			}
		}
	}

	if (getenv("JVIM_CRASH_QUIET") == NULL)
	{
		char	msg[MAXPATHL * 3];

		wsprintfA(msg,
			"JVim terminated abnormally.\r\n"
			"exception %08lx (%s) at %p\r\n\r\n"
			"report:\r\n%s",
			(unsigned long)code, crash_excname(code),
			ep->ExceptionRecord->ExceptionAddress, crash_log);
		if (crash_gui)
			MessageBoxA(NULL, msg, "JVim", MB_OK | MB_ICONERROR
											| MB_SETFOREGROUND);
		else
			fprintf(stderr, "\nJVim terminated abnormally.\n"
					"exception %08lx (%s) at %p\nreport: %s\n",
					(unsigned long)code, crash_excname(code),
					ep->ExceptionRecord->ExceptionAddress, crash_log);
	}

	return EXCEPTION_EXECUTE_HANDLER;		/* die, do not resume */
}

/*
 * Call once, as early as possible. gui is TRUE when there is no console to
 * print to.
 */
	void
w32crash_init(gui)
	int		gui;
{
	crash_gui = gui;
	SetUnhandledExceptionFilter(crash_filter);
	SetErrorMode(GetErrorMode() | SEM_NOGPFAULTERRORBOX);
}

#endif /* NT */

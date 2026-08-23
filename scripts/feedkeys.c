/*
 * feedkeys.exe -- queue real console key events, then run a console program
 * that reads them.  Used to exercise JVim's Win32 console key handling without
 * a keyboard: the records go into the shared console input buffer before the
 * child starts, so the child reads them as if they had been typed.
 *
 *   feedkeys.exe keys.txt jvim32.exe file
 *   feedkeys.exe -dump screen.txt keys.txt jvim32.exe file
 *
 * keys.txt holds the key spec; it is a file rather than an argument because
 * cmd.exe eats the "<" and ">" of the special-key names as redirections.
 *
 * Specials: <L> <R> <U> <D> arrows, <SL> <SR> shift-arrows, <PGUP> <PGDN>,
 * <HOME> <END> <DEL>, <CR>, <ESC>, and <xNN> for one raw byte -- which is how
 * a CP932 console hands over the bytes of a kanji.
 *
 * The console build needs "-nw": GuiWin defaults on, so jvim32.exe opens a
 * window like jvim32w.exe unless it is told not to.  See scripts/guikeys.c for
 * the window.
 *
 * With "-dump <file>" the console screen is read into <file> a few seconds after
 * the keys go in and the child is then killed, which is how to see what a
 * command that only draws -- ":e" plus CTRL-D, say -- actually put on screen.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "windesk.h"

static HANDLE hin;

static void key(WORD vk, char ch, DWORD ctrl)
{
	INPUT_RECORD	ir[2];
	DWORD			n;
	WORD			scan = (WORD)MapVirtualKey(vk, 0 /* VK_TO_VSC */);
	int				i;

	for (i = 0; i < 2; i++)
	{
		ir[i].EventType = KEY_EVENT;
		ir[i].Event.KeyEvent.bKeyDown = (i == 0);
		ir[i].Event.KeyEvent.wRepeatCount = 1;
		ir[i].Event.KeyEvent.wVirtualKeyCode = vk;
		ir[i].Event.KeyEvent.wVirtualScanCode = scan;
		ir[i].Event.KeyEvent.uChar.AsciiChar = ch;
		ir[i].Event.KeyEvent.dwControlKeyState = ctrl;
	}
	if (!WriteConsoleInput(hin, ir, 2, &n))
		fprintf(stderr, "WriteConsoleInput failed: %lu\n", GetLastError());
}

static void ch_key(char c)
{
	SHORT	v = VkKeyScan((TCHAR)c);
	WORD	vk = (WORD)(v & 0xff);
	DWORD	ctrl = 0;

	if (v == -1)
		vk = 0;
	if (v & 0x100)
		ctrl |= SHIFT_PRESSED;
	key(vk, c, ctrl);
}

/*
 * Read the console screen into a file, so that what a command drew can be
 * looked at from the WSL side.
 */
static void dump_screen(HANDLE hout, const char *path)
{
	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	FILE						*fp;
	COORD						at;
	WCHAR						*line;
	char						*utf8;
	int							y;

	if (!GetConsoleScreenBufferInfo(hout, &csbi))
	{
		fprintf(stderr, "cannot read the screen: %lu\n", GetLastError());
		return;
	}
	if ((fp = fopen(path, "wb")) == NULL)
	{
		fprintf(stderr, "cannot write %s\n", path);
		return;
	}
	/*
	 * The wide call, and UTF-8 out. ReadConsoleOutputCharacterA converts each
	 * cell through the code page into a buffer counted in cells, so a screen
	 * with Japanese on it came out as garbage of the wrong length -- which is no
	 * use for checking where a column landed.
	 */
	if ((line = malloc((csbi.dwSize.X + 1) * sizeof(WCHAR))) == NULL)
	{
		fclose(fp);
		return;
	}
	if ((utf8 = malloc(csbi.dwSize.X * 4 + 1)) == NULL)
	{
		free(line);
		fclose(fp);
		return;
	}
	for (y = 0; y < csbi.dwSize.Y; y++)
	{
		DWORD	got = 0;
		int		end;
		int		n;

		at.X = 0;
		at.Y = (SHORT)y;
		if (!ReadConsoleOutputCharacterW(hout, line, csbi.dwSize.X, at, &got))
			break;
		for (end = (int)got; end > 0 && line[end - 1] == L' '; end--)
			;
		line[end] = L'\0';
		n = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8,
									csbi.dwSize.X * 4 + 1, NULL, NULL);
		fprintf(fp, "%s\n", n > 0 ? utf8 : "");
	}
	free(utf8);
	free(line);
	fclose(fp);
}

int main(int argc, char **argv)
{
	windesk_reexec();		/* out of sight; see windesk.h */
	const char			*spec;
	char				cmd[2048] = "";
	static char			specbuf[4096];
	SECURITY_ATTRIBUTES	sa;
	HANDLE				hout;
	const char			*dump = NULL;
	int					i;
	STARTUPINFO			si;
	PROCESS_INFORMATION	pi;
	DWORD				code = 0;

	if (argc > 2 && strcmp(argv[1], "-dump") == 0)
	{
		dump = argv[2];
		argv += 2;
		argc -= 2;
	}
	if (argc < 3)
	{
		fprintf(stderr, "usage: feedkeys [-dump screen.txt] keyfile program [args...]\n");
		return 2;
	}
	/* Not GetStdHandle(): when this is launched from WSL or with input
	 * redirected, stdin is a pipe.  CONIN$ is the console itself, which is
	 * what the child reads. */
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
	hin = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					&sa, OPEN_EXISTING, 0, NULL);
	hout = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					&sa, OPEN_EXISTING, 0, NULL);
	if (hin == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "cannot open CONIN$: %lu\n", GetLastError());
		return 2;
	}
	{
		FILE	*fp = fopen(argv[1], "rb");
		size_t	n;

		if (fp == NULL)
		{
			fprintf(stderr, "cannot read %s\n", argv[1]);
			return 2;
		}
		n = fread(specbuf, 1, sizeof(specbuf) - 1, fp);
		specbuf[n] = '\0';
		fclose(fp);
		while (n > 0 && (specbuf[n - 1] == '\n' || specbuf[n - 1] == '\r'))
			specbuf[--n] = '\0';
	}
	spec = specbuf;
	while (*spec)
	{
		if (*spec == '<')
		{
			const char *e = strchr(spec, '>');
			if (e != NULL)
			{
				size_t	n = e - spec - 1;
				char	nm[16];

				if (n < sizeof(nm))
				{
					memcpy(nm, spec + 1, n);
					nm[n] = '\0';
					if (!strcmp(nm, "L")) key(VK_LEFT, 0, ENHANCED_KEY);
					else if (!strcmp(nm, "R")) key(VK_RIGHT, 0, ENHANCED_KEY);
					else if (!strcmp(nm, "U")) key(VK_UP, 0, ENHANCED_KEY);
					else if (!strcmp(nm, "D")) key(VK_DOWN, 0, ENHANCED_KEY);
					else if (!strcmp(nm, "SL")) key(VK_LEFT, 0, ENHANCED_KEY|SHIFT_PRESSED);
					else if (!strcmp(nm, "SR")) key(VK_RIGHT, 0, ENHANCED_KEY|SHIFT_PRESSED);
					else if (!strcmp(nm, "CR")) key(VK_RETURN, '\r', 0);
					else if (!strcmp(nm, "ESC")) key(VK_ESCAPE, 27, 0);
					else if (!strcmp(nm, "HOME")) key(VK_HOME, 0, ENHANCED_KEY);
					else if (!strcmp(nm, "END")) key(VK_END, 0, ENHANCED_KEY);
					else if (!strcmp(nm, "DEL")) key(VK_DELETE, 0, ENHANCED_KEY);
					else if (!strcmp(nm, "PGUP")) key(VK_PRIOR, 0, ENHANCED_KEY);
					else if (!strcmp(nm, "PGDN")) key(VK_NEXT, 0, ENHANCED_KEY);
					else if (nm[0] == 'x')
						/* one raw byte, as a CP932 console hands over the
						 * bytes of a kanji */
						key(0, (char)strtol(nm + 1, NULL, 16), 0);
					else { fprintf(stderr, "unknown key <%s>\n", nm); return 2; }
					spec = e + 1;
					continue;
				}
			}
		}
		ch_key(*spec);
		spec++;
	}

	for (i = 2; i < argc; i++)
	{
		if (i > 2)
			strcat(cmd, " ");
		strcat(cmd, argv[i]);
	}
	/* Hand the child the console, not our own stdin/stdout: launched from WSL
	 * or with output captured, those are pipes, and jvim then decides it is
	 * being telnetted to and reads a byte stream instead of key events. */
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = hin;
	si.hStdOutput = hout;
	si.hStdError = hout;
	memset(&pi, 0, sizeof(pi));
	if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
	{
		fprintf(stderr, "cannot start %s: %lu\n", cmd, GetLastError());
		return 2;
	}
	if (dump != NULL)
	{
		/* let it draw, look at the screen, then stop it */
		WaitForSingleObject(pi.hProcess, 3000);
		dump_screen(hout, dump);
		TerminateProcess(pi.hProcess, 0);
	}
	WaitForSingleObject(pi.hProcess, 20000);
	GetExitCodeProcess(pi.hProcess, &code);
	if (code == STILL_ACTIVE)
	{
		fprintf(stderr, "child still running after 20s, killing\n");
		TerminateProcess(pi.hProcess, 1);
		code = 99;
	}
	return (int)code;
}

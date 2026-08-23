/*
 * guikeys.exe -- drive the JVim Win32 GUI build without a keyboard.
 *
 *   guikeys.exe keys.txt jvim32w.exe file
 *
 * Starts the editor, waits for its window, then posts WM_KEYDOWN for the
 * special keys and WM_CHAR for text, which is how the real keyboard reaches
 * the same handlers (see WM_KEYDOWN / WM_CHAR in winjnt.c).  The key spec is
 * the one feedkeys.exe reads, except that <uNNNN> sends one UTF-16 unit, which
 * is what the Unicode window gets from the IME.
 *
 * With "-shot <file.bmp>" the window's client area is saved as a bitmap once the
 * keys have gone in and the editor is then left alone. That is the only way to
 * see what the GUI actually drew: it paints from its own screen array with
 * ExtTextOutW, so nothing about the layout can be told from the buffer contents.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "windesk.h"

static HWND	hwnd;

static void spec_key(WORD vk)
{
	PostMessageW(hwnd, WM_KEYDOWN, vk, (MapVirtualKey(vk, 0) << 16) | 1);
	Sleep(15);
	PostMessageW(hwnd, WM_KEYUP, vk, (MapVirtualKey(vk, 0) << 16) | 0xC0000001);
	Sleep(15);
}

static void text_key(unsigned char c)
{
	PostMessageW(hwnd, WM_CHAR, c, 1);
	Sleep(15);
}

/*
 * Save the window's client area to a 24 bit BMP.
 */
static void shoot(HWND hwnd, const char *path)
{
	RECT				rc;
	HDC					hdcWin, hdcMem;
	HBITMAP				bm, old;
	BITMAPINFOHEADER	bi;
	BITMAPFILEHEADER	bf;
	int					w, h, stride;
	unsigned char		*bits;
	FILE				*fp;

	if (!GetClientRect(hwnd, &rc))
		return;
	w = rc.right - rc.left;
	h = rc.bottom - rc.top;
	if (w <= 0 || h <= 0)
		return;
	stride = ((w * 3) + 3) & ~3;
	hdcWin = GetDC(hwnd);
	hdcMem = CreateCompatibleDC(hdcWin);
	bm = CreateCompatibleBitmap(hdcWin, w, h);
	old = (HBITMAP)SelectObject(hdcMem, bm);
	if (!PrintWindow(hwnd, hdcMem, 1 /* PW_CLIENTONLY */))
		BitBlt(hdcMem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY);
	SelectObject(hdcMem, old);

	memset(&bi, 0, sizeof(bi));
	bi.biSize = sizeof(bi);
	bi.biWidth = w;
	bi.biHeight = -h;			/* top down */
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = BI_RGB;
	if ((bits = malloc((size_t)stride * h)) != NULL
			&& GetDIBits(hdcMem, bm, 0, h, bits, (BITMAPINFO *)&bi, DIB_RGB_COLORS)
			&& (fp = fopen(path, "wb")) != NULL)
	{
		memset(&bf, 0, sizeof(bf));
		bf.bfType = 0x4d42;
		bf.bfOffBits = sizeof(bf) + sizeof(bi);
		bf.bfSize = bf.bfOffBits + stride * h;
		bi.biHeight = -h;
		fwrite(&bf, sizeof(bf), 1, fp);
		fwrite(&bi, sizeof(bi), 1, fp);
		fwrite(bits, (size_t)stride * h, 1, fp);
		fclose(fp);
	}
	else
		fprintf(stderr, "cannot save %s\n", path);
	free(bits);
	DeleteObject(bm);
	DeleteDC(hdcMem);
	ReleaseDC(hwnd, hdcWin);
}

int main(int argc, char **argv)
{
	windesk_reexec();		/* out of sight; see windesk.h */
	static char			specbuf[4096];
	const char			*spec;
	char				cmd[2048] = "";
	int					i;
	STARTUPINFO			si;
	PROCESS_INFORMATION	pi;
	DWORD				code = 0;
	FILE				*fp;
	size_t				n;
	const char			*shot = NULL;

	if (argc > 2 && strcmp(argv[1], "-shot") == 0)
	{
		shot = argv[2];
		argv += 2;
		argc -= 2;
	}
	if (argc < 3)
	{
		fprintf(stderr, "usage: guikeys keyfile program [args...]\n");
		return 2;
	}
	if ((fp = fopen(argv[1], "rb")) == NULL)
	{
		fprintf(stderr, "cannot read %s\n", argv[1]);
		return 2;
	}
	n = fread(specbuf, 1, sizeof(specbuf) - 1, fp);
	specbuf[n] = '\0';
	fclose(fp);
	while (n > 0 && (specbuf[n - 1] == '\n' || specbuf[n - 1] == '\r'))
		specbuf[--n] = '\0';

	for (i = 2; i < argc; i++)
	{
		if (i > 2)
			strcat(cmd, " ");
		strcat(cmd, argv[i]);
	}
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	memset(&pi, 0, sizeof(pi));
	if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
	{
		fprintf(stderr, "cannot start %s: %lu\n", cmd, GetLastError());
		return 2;
	}
	for (i = 0; i < 200; i++)		/* up to 10s for the window */
	{
		if ((hwnd = FindWindowW(L"JVim", NULL)) != NULL)
			break;
		Sleep(50);
	}
	if (hwnd == NULL)
	{
		fprintf(stderr, "no JVim window appeared\n");
		TerminateProcess(pi.hProcess, 1);
		return 2;
	}
	/*
	 * The window appears before the editor is ready for a key: it is still
	 * reading the rc, and a key posted while it is doing that is lost. Not
	 * "delayed" -- lost, and only the first one, so a case reads as though its
	 * second keystroke were its first. With no rc in the work directory that
	 * is a fraction of a second and a fixed sleep covered it nearly always,
	 * which is the worst way for it to be wrong: one case in a hundred runs
	 * fails and nothing explains it. An rc that sources the rule files takes
	 * long enough to lose the key every time, which is how it was finally seen.
	 *
	 * WaitForInputIdle is the usual answer and is not one here: it reports the
	 * process idle as soon as it has been idle once, which happens before the
	 * rc is read. So the first key is a throwaway. Escape in normal mode does
	 * nothing -- every case starts there -- so whether it is swallowed or
	 * delivered, what follows is unaffected, and the editor is provably past
	 * the point of losing keys because it has just been given one.
	 */
	WaitForInputIdle(pi.hProcess, 10000);
	Sleep(300);						/* and a little for the first paint */
	spec_key(VK_ESCAPE);
	Sleep(200);

	spec = specbuf;
	while (*spec)
	{
		if (*spec == '<')
		{
			const char *e = strchr(spec, '>');
			if (e != NULL)
			{
				size_t	l = e - spec - 1;
				char	nm[16];

				if (l < sizeof(nm))
				{
					memcpy(nm, spec + 1, l);
					nm[l] = '\0';
					if (!strcmp(nm, "L")) spec_key(VK_LEFT);
					else if (!strcmp(nm, "R")) spec_key(VK_RIGHT);
					else if (!strcmp(nm, "U")) spec_key(VK_UP);
					else if (!strcmp(nm, "D")) spec_key(VK_DOWN);
					else if (!strcmp(nm, "PGUP")) spec_key(VK_PRIOR);
					else if (!strcmp(nm, "PGDN")) spec_key(VK_NEXT);
					else if (nm[0] == 'u')
					{	/* one UTF-16 unit, as the Unicode window gets it */
						PostMessageW(hwnd, WM_CHAR,
									(WPARAM)strtol(nm + 1, NULL, 16), 1);
						Sleep(15);
					}
					else if (!strcmp(nm, "CR")) text_key('\r');
					else if (!strcmp(nm, "ESC")) spec_key(VK_ESCAPE);
					else { fprintf(stderr, "unknown key <%s>\n", nm); return 2; }
					spec = e + 1;
					continue;
				}
			}
		}
		text_key((unsigned char)*spec);
		spec++;
	}

	if (shot != NULL)
	{
		Sleep(1200);				/* let it finish drawing */
		shoot(hwnd, shot);
		TerminateProcess(pi.hProcess, 0);
	}
	WaitForSingleObject(pi.hProcess, 15000);
	GetExitCodeProcess(pi.hProcess, &code);
	if (code == STILL_ACTIVE)
	{
		fprintf(stderr, "editor still running, killing\n");
		TerminateProcess(pi.hProcess, 1);
		code = 99;
	}
	return (int)code;
}

/*
 * windesk.h -- run a test driver where nobody has to watch it.
 *
 * feedkeys.exe and guikeys.exe drive a real editor: the console build reads
 * real console key events, the GUI build reads real window messages, and that
 * is the point of them -- it is the path "-s" cannot reach. But a real editor
 * opens a real window, on the screen of whoever is running the suite, fourteen
 * times, each one taking the keyboard for a couple of seconds. Running the
 * tests and using the machine were mutually exclusive.
 *
 * The window is not what the tests need; the message queue is. PostMessage and
 * WriteConsoleInput do not care whether a window is visible, focused, or on the
 * desktop being looked at -- only a person does. So the driver moves itself to
 * a desktop of its own before it starts anything. Windows there are ordinary
 * windows, with ordinary message queues, that nothing on screen ever shows and
 * that can never take focus.
 *
 * Hiding the window instead (ShowWindow, or starting it minimised) does not
 * work: winjnt.c calls ShowWindow(SW_NORMAL) itself and ignores what it was
 * started with, so there is always a flash and always a stolen keystroke.
 *
 * A desktop is chosen when a process starts and cannot be changed afterwards
 * for a process that already has windows, so this re-runs the driver once,
 * with the same command line, on the new desktop. The child keeps this
 * process's stdout and stderr, so a message still reaches whoever is watching,
 * and its exit code becomes ours.
 *
 * The console build needs a second thing. A desktop hides windows; it does not
 * stop one being made, and CREATE_NEW_CONSOLE is handed to whatever is set as
 * the default terminal, which on Windows 11 is Windows Terminal -- and that
 * opens the console on the desktop the person is looking at, whichever desktop
 * asked for it. So the console is made with CREATE_NO_WINDOW, which has an
 * input buffer WriteConsoleInput works on and no window for anything to adopt.
 *
 * WINDESK_OFF=1 in the environment turns it off, for when something has to be
 * watched happening.
 */
#ifndef JVIM_WINDESK_H
#define JVIM_WINDESK_H

#include <windows.h>
#include <stdlib.h>

static void
windesk_reexec(void)
{
	static char				desktop[] = "jvim-tests";
	HDESK					desk;
	STARTUPINFO				si;
	PROCESS_INFORMATION		pi;
	DWORD					code = 1;

	if (getenv("WINDESK_DONE") != NULL)		/* this is the second run */
		return;
	if (getenv("WINDESK_OFF") != NULL)		/* asked to stay in sight */
		return;
	/*
	 * A station has room for a handful of desktops and the name is reused, so
	 * an earlier run's desktop is opened again rather than piling up.
	 */
	desk = OpenDesktop(desktop, 0, FALSE, GENERIC_ALL);
	if (desk == NULL)
		desk = CreateDesktop(desktop, NULL, NULL, 0, GENERIC_ALL, NULL);
	if (desk == NULL)
		return;					/* no rights for one: carry on in the open */

	SetEnvironmentVariable("WINDESK_DONE", "1");
	memset(&si, 0, sizeof(si));
	si.cb			= sizeof(si);
	si.lpDesktop	= desktop;
	/* Keep the pipes we were given: the shell is still reading them. */
	si.dwFlags		= STARTF_USESTDHANDLES;
	si.hStdInput	= GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput	= GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError	= GetStdHandle(STD_ERROR_HANDLE);
	memset(&pi, 0, sizeof(pi));
	/*
	 * A console of its own, because the console build is handed it through
	 * CONIN$ and a console cannot be shared across desktops -- but one with no
	 * window at all. CREATE_NEW_CONSOLE would be handed to whatever is set as
	 * the default terminal, and Windows Terminal opens it on the desktop the
	 * person is looking at, whatever desktop asked for it. CREATE_NO_WINDOW
	 * still gives a console with a working input buffer; there is simply
	 * nothing to hand over. (Checked: CONIN$ opens, WriteConsoleInput
	 * succeeds, GetConsoleWindow() is NULL.)
	 */
	if (CreateProcess(NULL, GetCommandLine(), NULL, NULL, TRUE,
						CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
	{
		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, &code);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	else
	{
		CloseDesktop(desk);
		return;					/* could not: carry on in the open */
	}
	CloseDesktop(desk);
	ExitProcess(code);
}

#endif /* JVIM_WINDESK_H */

/*
 * ptyrun -- run a command with a pty for its terminal.
 *
 *	ptyrun command [args ...]
 *
 * JVim will not draw a screen without a terminal, so the test harness has to
 * give it one. script(1) can do that, but it is a different program on every
 * system: util-linux and NetBSD take the command with -c, the other BSDs take
 * it after the typescript file, and NetBSD's exits as soon as its own standard
 * input reaches end of file, which with a redirected stdin can happen before
 * the command has run at all. This does the one thing that is wanted instead.
 *
 * Anything on our standard input is passed on to the command as keystrokes,
 * and whatever the command draws is copied to our standard output. The window
 * is a fixed 80x24 so that the screen column tests do not depend on the
 * terminal the suite happens to be started from.
 *
 * POSIX only: posix_openpt(), no <pty.h>, <util.h> or <libutil.h>, and so no
 * -lutil either.
 */

/* glibc hides posix_openpt() and friends unless X/Open is asked for; the BSDs
 * show them by default and lose other things if X/Open is asked for, so this
 * is only for Linux. */
#ifdef __linux__
# define _XOPEN_SOURCE 600
# define _DEFAULT_SOURCE 1
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*
 * A test that leaves the editor waiting for a key would otherwise hang forever:
 * when its script file runs out, it reads the terminal, and nothing is coming.
 * PTYRUN_TIMEOUT seconds (20 by default, 0 to wait for ever) and the child is
 * killed and 124 returned, the way timeout(1) does it -- which macOS does not
 * have.
 *
 * PTYRUN_SIGNAL turns that into something a test can use on purpose: the named
 * signal (HUP, TERM, INT, QUIT, or a number) is sent instead of SIGKILL, and
 * then the copying carries on so that what the command prints on its way out
 * and the status it exits with are both collected. That is the only way to test
 * what the editor does when the session goes away, since a command started in
 * the background cannot have the pty as its controlling terminal.
 */
static pid_t	child = 0;
static int		alarm_sig = SIGKILL;

	static void
on_alarm(int sig)
{
	if (child > 0)
		kill(child, alarm_sig);
	if (alarm_sig == SIGKILL)
		_exit(124);
	/*
	 * A signal the command is expected to act on rather than die of. Let the
	 * loop below run on, and arm a hard kill behind it in case it does not go
	 * after all.
	 */
	alarm_sig = SIGKILL;
	signal(SIGALRM, on_alarm);
	alarm(10);
}

/* PTYRUN_SIGNAL, as a name or a number. 0 for anything not recognised, so a
 * typo behaves like the default rather than sending something unintended. */
	static int
signal_named(const char *s)
{
	static const struct { const char *name; int sig; } names[] =
	{
		{"HUP", SIGHUP}, {"INT", SIGINT}, {"QUIT", SIGQUIT},
		{"TERM", SIGTERM}, {"USR1", SIGUSR1}, {NULL, 0}
	};
	int		i;

	if (s == NULL || *s == '\0')
		return 0;
	if (s[0] >= '0' && s[0] <= '9')
		return (int)strtol(s, NULL, 10);
	if (strncmp(s, "SIG", 3) == 0)
		s += 3;
	for (i = 0; names[i].name != NULL; i++)
		if (strcmp(s, names[i].name) == 0)
			return names[i].sig;
	return 0;
}

int
main(int argc, char **argv)
{
	int				master, slave, stdin_open = 1;
	char			*name, buf[4096], out[4096];
	size_t			pending = 0, sent = 0;
	pid_t			pid;
	int				status;
	struct winsize	ws;
	struct termios	tio;

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s command [args ...]\n", argv[0]);
		return 2;
	}

	ws.ws_row = 24;
	ws.ws_col = 80;
	ws.ws_xpixel = 0;
	ws.ws_ypixel = 0;

	master = posix_openpt(O_RDWR | O_NOCTTY);
	if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0
			|| (name = ptsname(master)) == NULL)
	{
		perror("ptyrun: cannot get a pty");
		return 2;
	}

	/* The slave is opened and set up here, before the fork, rather than in the
	 * child. Doing it in the child is a race the caller loses: keystrokes fed
	 * in straight away arrive while the pty is still in canonical mode with
	 * echo on, or before the slave is open at all, and are then mangled or
	 * dropped. FreeBSD is fast enough to lose that race every time.
	 *
	 * Raw matters for a second reason: in canonical mode a line longer than
	 * MAX_CANON (255 on NetBSD) is thrown away, so pasting 600 bytes of kana
	 * loses most of it and the editor waits for the rest of the keys forever. */
	slave = open(name, O_RDWR | O_NOCTTY);
	if (slave < 0)
	{
		perror("ptyrun: cannot open the pty");
		return 2;
	}
	if (tcgetattr(slave, &tio) == 0)
	{
		tio.c_iflag &= ~(ICRNL | INLCR | IGNCR | IXON | ISTRIP | BRKINT);
		tio.c_oflag &= ~OPOST;
		tio.c_lflag &= ~(ICANON | ECHO | ISIG
#ifdef IEXTEN
				| IEXTEN
#endif
				);
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;
		tcsetattr(slave, TCSANOW, &tio);
	}
	ioctl(slave, TIOCSWINSZ, &ws);

	pid = fork();
	if (pid < 0)
	{
		perror("ptyrun: fork");
		return 2;
	}
	if (pid == 0)
	{
		close(master);
		if (setsid() < 0)
			_exit(127);
		ioctl(slave, TIOCSCTTY, (char *)0);	/* our controlling terminal */
		dup2(slave, 0);
		dup2(slave, 1);
		dup2(slave, 2);
		if (slave > 2)
			close(slave);
		execvp(argv[1], argv + 1);
		_exit(127);
	}
	/* Only the command may hold the slave open, or the pty never reports the
	 * end of its output and the loop below would not know when to stop. */
	close(slave);

	child = pid;
	{
		const char	*t = getenv("PTYRUN_TIMEOUT");
		unsigned	secs = 20;
		int			named = signal_named(getenv("PTYRUN_SIGNAL"));

		if (named > 0)
			alarm_sig = named;
		if (t != NULL)
			secs = (unsigned)strtoul(t, NULL, 10);
		if (secs > 0)
		{
			signal(SIGALRM, on_alarm);
			alarm(secs);
		}
	}

	/* Keystrokes go out in whatever size the pty will take right now. Writing
	 * the lot in one blocking call deadlocks on a small tty buffer: we would be
	 * waiting for the command to read while the command waits for us to drain
	 * what it has drawn. NetBSD's buffer is small enough for that to happen on
	 * a 600 byte paste. */
	fcntl(master, F_SETFL, fcntl(master, F_GETFL, 0) | O_NONBLOCK);

	for (;;)
	{
		fd_set	r, w;
		int		n;
		ssize_t	got;

		FD_ZERO(&r);
		FD_ZERO(&w);
		FD_SET(master, &r);
		if (pending > sent)
			FD_SET(master, &w);
		else if (stdin_open)
			FD_SET(0, &r);
		n = select(master + 1, &r, &w, NULL, NULL);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}
		if (pending == sent && stdin_open && FD_ISSET(0, &r))
		{
			got = read(0, buf, sizeof(buf));
			if (got <= 0)
				stdin_open = 0;
			else
			{
				pending = (size_t)got;
				sent = 0;
			}
		}
		if (pending > sent && FD_ISSET(master, &w))
		{
			got = write(master, buf + sent, pending - sent);
			if (got > 0)
				sent += (size_t)got;
			else if (got < 0 && errno != EAGAIN && errno != EINTR)
				break;
			if (sent == pending)
				pending = sent = 0;
		}
		if (FD_ISSET(master, &r))
		{
			/* The command closing the pty shows up as end of file, or as EIO
			 * on Linux; either way there is nothing more to copy. */
			got = read(master, out, sizeof(out));
			if (got < 0 && (errno == EAGAIN || errno == EINTR))
				continue;
			if (got <= 0)
				break;
			if (write(1, out, (size_t)got) < 0)
				break;
		}
	}

	close(master);
	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
			return 2;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return 1;
}

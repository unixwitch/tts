/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<sys/types.h>
#include	<sys/select.h>

#include	<wchar.h>
#include	<ctype.h>
#include	<wctype.h>
#include	<fcntl.h>
#include	<time.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<strings.h>
#include	<string.h>
#include	<pwd.h>
#include	<limits.h>
#include	<errno.h>
#include	<unistd.h>
#include	<locale.h>
#include	<stdarg.h>
#include	<inttypes.h>
#include	<math.h>

#include	"config.h"

#ifdef	HAVE_IOREGISTERFORSYSTEMPOWER
# define USE_DARWIN_POWER

# include	<mach/mach_port.h>
# include	<mach/mach_interface.h>
# include	<mach/mach_init.h>
# include	<mach/mach_error.h>

# include	<sys/event.h>

# include	<IOKit/pwr_mgt/IOPMLib.h>
# include	<IOKit/IOMessage.h>

# include	<alloca.h>
#endif

#include	"tailq.h"
#include	"tts.h"
#include	"entry.h"
#include	"wide.h"
#include	"tts_curses.h"
#include	"ui.h"
#include	"functions.h"
#include	"commands.h"
#include	"bindings.h"
#include	"str.h"
#include	"style.h"
#include	"variable.h"

volatile sig_atomic_t doexit;

time_t laststatus;

history_t *searchhist;
history_t *prompthist;

#define STATFILE ".rttts"
#define RCFILE ".ttsrc"

int load(void);
int save(void);
static time_t lastsave;
static char statfile[PATH_MAX + 1];

static int load_file(const char *);

int pagestart;
entry_t *curent;

int showinv = 0;

static WCHAR *macro_text, *macro_pos;

time_t itime = 0;

int show_billable = 0;
int delete_advance = 1;
int mark_advance = 1;
int bill_advance = 0;
int bill_increment = 0;
WCHAR *auto_nonbillable;

variable_t variables[] = {
	{ WIDE("delete_advance"),	VTYPE_BOOL,	&delete_advance },
	{ WIDE("mark_advance"),		VTYPE_BOOL,	&mark_advance },
	{ WIDE("billable_advance"),	VTYPE_BOOL,	&bill_advance },
	{ WIDE("show_billable"),	VTYPE_BOOL,	&show_billable },
	{ WIDE("auto_non_billable"),	VTYPE_STRING,	&auto_nonbillable },
	{ WIDE("bill_increment"),	VTYPE_INT,	&bill_increment },
	{ }
};

#ifdef USE_DARWIN_POWER
static IONotificationPortRef	port_ref;
static io_object_t		notifier;
static mach_port_t		ioport;
static io_connect_t		root_port;

static volatile sig_atomic_t	donesleep;
static time_t			sleeptime;

static void  power_setup(struct kevent *);
static void  power_handle(struct kevent *);
static void  power_event(void *, io_service_t, natural_t, void *);
static void  prompt_sleep(void);

static void
power_setup(ev)
	struct kevent	*ev;
{
mach_port_t	pset;
int		ret;

	if ((ret = mach_port_allocate(mach_task_self(),
				       MACH_PORT_RIGHT_PORT_SET,
				       &pset)) != KERN_SUCCESS) {
		fprintf(stderr, "mach_port_allocate: %s [%x]\n",
			mach_error_string(ret), ret);
		exit(1);
	}

/* Register a handler for sleep and wake events */
	root_port = IORegisterForSystemPower(NULL, &port_ref, power_event,
			&notifier);
	ioport = IONotificationPortGetMachPort(port_ref);
	EV_SET(ev, pset, EVFILT_MACHPORT, EV_ADD, 0, 0, 0);

	if ((ret = mach_port_insert_member(mach_task_self(), ioport,
					   pset)) != KERN_SUCCESS) {
		fprintf(stderr, "mach_port_insert_member: %s [%x]\n",
			mach_error_string(ret), ret);
		exit(1);
	}
}

static void
power_handle(ev)
	struct kevent	*ev;
{
mach_msg_return_t	 ret;
void			*msg = alloca(ev->data);

/* Receive the message */
	memset(msg, 0, ev->data);
	ret = mach_msg(msg, MACH_RCV_MSG, 0, ev->data, ioport,
		       MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

	if (ret != MACH_MSG_SUCCESS) {
		fprintf(stderr, "mach_msg: %s [%x]\n",
			mach_error_string(ret), ret);
		exit(1);
	}

/* Give the message to IOKit to handle */
	IODispatchCalloutFromMessage(NULL, msg, port_ref);
}

static void
power_event(ref, service, msgtype, arg)
	void		*ref, *arg;
	io_service_t	 service;
	natural_t	 msgtype;
{
static time_t	sleep_started;
time_t		diff;

	switch (msgtype) {
	case kIOMessageSystemWillSleep:
		/* System is about to sleep; save the current time */
		time(&sleep_started);
		IOAllowPowerChange(root_port, (long) arg);
		break;

	case kIOMessageSystemHasPoweredOn:
		/* System has finished wake-up; calculate the sleep time and
		 * prompt the user.
		 */
		time(&diff);
		diff -= sleep_started;
		sleeptime += diff;
		prompt_sleep();
		break;
	}
}
#endif

static void
sigexit(sig)
{
	doexit = 1;
}

int
main(argc, argv)
	char	**argv;
{
struct passwd	*pw;
char		 rcfile[PATH_MAX + 1];
#ifdef	USE_DARWIN_POWER
int		 kq;
struct kevent	 evs[2], rev;
# define	STDIN_EV	0
# define	IOKIT_EV	1
#endif

	setlocale(LC_ALL, "");

#ifdef USE_DARWIN_POWER
	if ((kq = kqueue()) == -1) {
		perror("kqueue");
		return 1;
	}

	memset(evs, 0, sizeof(evs));

	EV_SET(&evs[STDIN_EV], STDIN_FILENO, EVFILT_READ, EV_ADD, 0, 0, 0);
	power_setup(&evs[IOKIT_EV]);

	if (kevent(kq, evs, 2, NULL, 0, NULL) == -1) {
		perror("kevent");
		return 1;
	}
#endif

	signal(SIGTERM, sigexit);
	signal(SIGINT, sigexit);

	searchhist = hist_new();
	prompthist = hist_new();

	if ((pw = getpwuid(getuid())) == NULL || !pw->pw_dir || !*pw->pw_dir) {
		fprintf(stderr, "Sorry, I can't find your home directory.");
		return 1;
	}

	snprintf(statfile, sizeof(statfile), "%s/%s", pw->pw_dir, STATFILE);
	snprintf(rcfile, sizeof(rcfile), "%s/%s", pw->pw_dir, RCFILE);

	initscr();
	in_curses = 1;
	start_color();
#ifdef HAVE_USE_DEFAULT_COLORS
	use_default_colors();
#endif
	cbreak();
	noecho();
	nonl();
	nodelay(stdscr, TRUE);

	pair_content(0, &default_fg, &default_bg);

	refresh();

	intrflush(stdscr, TRUE);
	keypad(stdscr, TRUE);
	leaveok(stdscr, TRUE);

	titwin = newwin(1, 0, 0, 0);
	intrflush(titwin, FALSE);
	keypad(titwin, TRUE);
	leaveok(titwin, TRUE);

	statwin = newwin(1, 0, LINES - 1, 0);
	intrflush(statwin, FALSE);
	keypad(statwin, TRUE);
	leaveok(statwin, TRUE);

	listwin = newwin(LINES - 2, 0, 1, 0);
	intrflush(listwin, FALSE);
	keypad(listwin, TRUE);
	leaveok(listwin, TRUE);

	init_pair(1, default_fg, default_bg);
	init_pair(2, default_fg, default_bg);
	init_pair(3, default_fg, default_bg);
	init_pair(4, default_fg, default_bg);
	init_pair(5, default_fg, default_bg);
	init_pair(6, default_fg, default_bg);

	style_set(&sy_header, WIDE("reverse"), NULL);
	style_set(&sy_status, WIDE("normal"), NULL);
	style_set(&sy_entry, WIDE("normal"), NULL);
	style_set(&sy_selected, WIDE("normal"), NULL);
	style_set(&sy_running, WIDE("bold"), NULL);
	style_set(&sy_date, WIDE("underline"), NULL);
	apply_styles();

	if (load_file(rcfile) == -1) {
		endwin();
		return 1;
	}

	curs_set(0);

	bind_key(WIDE("?"),		WIDE("help"), 0);
	bind_key(WIDE("a"),		WIDE("add"), 0);
	bind_key(WIDE("A"),		WIDE("add-old"), 0);
	bind_key(WIDE("d"),		WIDE("delete"), 0);
	bind_key(WIDE("u"),		WIDE("undelete"), 0);
	bind_key(WIDE("q"),		WIDE("quit"), 0);
	bind_key(WIDE("<CTRL-C>"),	WIDE("quit"), 0);
	bind_key(WIDE("i"),		WIDE("invoice"), 0);
	bind_key(WIDE("b"),		WIDE("billable"), 0);
	bind_key(WIDE("m"),		WIDE("mark"), 0);
	bind_key(WIDE("U"),		WIDE("unmarkall"), 0);
	bind_key(WIDE("<SPACE>"),	WIDE("startstop"), 0);
	bind_key(WIDE("e"),		WIDE("edit-desc"), 0);
	bind_key(WIDE("\\"),		WIDE("edit-time"), 0);
	bind_key(WIDE("<TAB>"),		WIDE("showhide-inv"), 0);
	bind_key(WIDE("c"),		WIDE("copy"), 0);
	bind_key(WIDE("+"),		WIDE("add-time"), 0);
	bind_key(WIDE("-"),		WIDE("sub-time"), 0);
	bind_key(WIDE("/"),		WIDE("search"), 0);
	bind_key(WIDE("$"),		WIDE("sync"), 0);
	bind_key(WIDE("<UP>"),		WIDE("prev"), 0);
	bind_key(WIDE("<DOWN>"),	WIDE("next"), 0);
	bind_key(WIDE(":"),		WIDE("execute"), 0);
	bind_key(WIDE("M"),		WIDE("merge"), 0);
	bind_key(WIDE("r"),		WIDE("interrupt"), 0);
	bind_key(WIDE("R"),		WIDE("interrupt"), 0);

	/*
	 * Make sure we can save (even if it's an empty file or nothing has
	 * changed) before the user starts entering entries.
	 */
	load();
	save();

	drawheader();
	drawstatus(WIDE(""));

	if (!TTS_TAILQ_EMPTY(&entries)) {
		curent = TTS_TAILQ_FIRST(&entries);
		while (!showinv && curent->en_flags.efl_invoiced)
			if ((curent = TTS_TAILQ_NEXT(curent, en_entries)) == NULL)
				break;
	}

	for (;;) {
	INT		 c;
	binding_t	*bi;
#ifdef	USE_DARWIN_POWER
	struct timespec	 timeout;
	int		 nev;
#else
	fd_set		 in_set;
	struct timeval	 timeout;
#endif

		if (doexit)
			break;

		drawheader();
		drawentries();
		wrefresh(listwin);

#ifdef	USE_DARWIN_POWER
		timeout.tv_sec = 0;
		timeout.tv_nsec = 500000000;

		if ((nev = kevent(kq, NULL, 0, &rev, 1, &timeout)) == -1) {
			if (doexit)
				break;
			if (errno == EINTR)
				continue;
			perror("kevent");
			return 1;
		}

		if (nev == 1 && (rev.filter == EVFILT_MACHPORT)) {
			power_handle(&rev);
			continue;
		}
#else
	/* Wait for input to be ready. */
		FD_ZERO(&in_set);
		FD_SET(STDIN_FILENO, &in_set);

		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;

		if (select(STDIN_FILENO + 1, &in_set, NULL, NULL, &timeout) == -1) {
			if (doexit)
				break;
			if (errno == EINTR)
				continue;
		}
#endif

		while (input_char(&c) != ERR) {
#ifdef KEY_RESIZE
			if (c == KEY_RESIZE)
				continue;
#endif

			drawstatus(WIDE(""));

			TTS_TAILQ_FOREACH(bi, &bindings, bi_entries) {
				if (bi->bi_code != c)
					continue;

				if (!macro_text && bi->bi_macro)
					input_macro(bi->bi_macro);
				else if (bi->bi_func)
					bi->bi_func->fn_hdl();

				goto next;
			}

			drawstatus(WIDE("Unknown command."));
		next:	;
		}

		if (doexit)
			break;

		if (time(NULL) - laststatus >= 2)
			drawstatus(WIDE(""));

		if (time(NULL) - lastsave > 60)
			save();
	}

	save();
	endwin();
	return 0;
}

int
load()
{
FILE	*f;
char	 input[4096];
WCHAR	 line[4096];
entry_t	*en;

	TTS_TAILQ_FOREACH(en, &entries, en_entries)
		entry_free(en);

	if ((f = fopen(statfile, "r")) == NULL) {
		if (errno == ENOENT)
			return 0;

		errbox(WIDE("Can't read %s: %s"), statfile, strerror(errno));
		exit(1);
	}

	if (fgets(input, sizeof(input), f) == NULL) {
		errbox(WIDE("Can't read %s: %s"), statfile, strerror(errno));
		fclose(f);
		exit(1);
	}

	MBSTOWCS(line, input, WSIZEOF(line));

	if (STRCMP(line, WIDE("#%RT/TTS V1\n"))) {
		errbox(WIDE("Can't read %s: invalid magic signature"), statfile);
		fclose(f);
		exit(1);
	}

	while (fgets(input, sizeof(input), f)) {
	unsigned long	 cre, secs;
	WCHAR	 	 flags[10], desc[4096], *p;
	entry_t		*en;
	int		 i;

		MBSTOWCS(line, input, WSIZEOF(line));

		if (SSCANF(line, WIDE("#%%showinv %d\n"), &i) == 1) {
			showinv = i ? 1 : 0;
			continue;
		}

		if (SSCANF(line, WIDE("%lu %lu %9"FMT_L"[in-] %4095"FMT_L"[^\n]\n"),
				&cre, &secs, flags, desc) != 4) {
			errbox(WIDE("Can't read %s: invalid entry format"), statfile);
			fclose(f);
			exit(1);
		}

		en = entry_new(desc);
		en->en_created = cre;
		en->en_secs = secs;
		for (p = flags; *p; p++) {
			switch (*p) {
			case '-':
				break;

			case 'i':
				en->en_flags.efl_invoiced = 1;
				break;

			case 'n':
				en->en_flags.efl_nonbillable = 1;
				break;

			default:
				errbox(WIDE("Can't read %s: invalid flag"), statfile);
				fclose(f);
				exit(1);
			}
		}
	}

	if (ferror(f)) {
		errbox(WIDE("Can't read %s: %s"), statfile, strerror(errno));
		fclose(f);
		exit(1);
	}

	fclose(f);
	return 0;
}

int
save()
{
FILE	*f;
int	 fd;
char	 p[PATH_MAX + 1];
entry_t	*en;

	snprintf(p, sizeof(p), "%s_", statfile);

	if ((fd = open(p, O_WRONLY | O_CREAT, 0600)) == -1) {
		errbox(WIDE("%s_: %s"), statfile, strerror(errno));
		endwin();
		exit(1);
	}

	if ((f = fdopen(fd, "w")) == NULL) {
		errbox(WIDE("%s: %s"), p, strerror(errno));
		endwin();
		exit(1);
	}

	if (FPRINTF(f, WIDE("#%%RT/TTS V1\n")) == -1) {
		fclose(f);
		unlink(p);

		errbox(WIDE("%s: write error (header): %s"), p, strerror(errno));
		endwin();
		exit(1);
	}

	if (FPRINTF(f, WIDE("#%%showinv %d\n"), showinv) == -1) {
		fclose(f);
		unlink(p);

		errbox(WIDE("%s: write error (showinv): %s"), p, strerror(errno));
		endwin();
		exit(1);
	}

	TTS_TAILQ_FOREACH_REVERSE(en, &entries, entrylist, en_entries) {
	char	 flags[10], *fp = flags, wdesc[4096] = {};
	time_t	 n;

		WCSTOMBS(wdesc, en->en_desc, sizeof(wdesc));

		memset(flags, 0, sizeof(flags));
		if (en->en_flags.efl_invoiced)
			*fp++ = 'i';
		if (en->en_flags.efl_nonbillable)
			*fp++ = 'n';

		n = en->en_secs;
		if (en->en_started)
			n += time(NULL) - en->en_started;

		if (FPRINTF(f, WIDE("%lu %lu %s %s\n"),
				(unsigned long) en->en_created,
				(unsigned long) n,
				*flags ? flags : "-", wdesc) == -1) {
			errbox(WIDE("%s: write error (entry): %s"), p, strerror(errno));
			fclose(f);
			unlink(p);
			endwin();
			exit(1);
		}
	}

	if (fclose(f) == EOF) {
		unlink(p);
		errbox(WIDE("%s: write error (closing): %s"), p, strerror(errno));
		endwin();
		exit(1);
	}

	if (rename(p, statfile) == -1) {
		unlink(p);
		errbox(WIDE("%s: rename: %s"), statfile, strerror(errno));
		endwin();
		exit(1);
	}

	time(&lastsave);
	return 0;
}

history_t *
hist_new()
{
history_t	*hi;
	if ((hi = calloc(1, sizeof(*hi))) == NULL)
		return NULL;
	TTS_TAILQ_INIT(&hi->hi_ents);
	return hi;
}

void
hist_add(hi, text)
	history_t	*hi;
	const WCHAR	*text;
{
histent_t	*hent;

	if (!*text)
		return;

	if ((hent = calloc(1, sizeof(*hent))) == NULL)
		return;

	if ((hent->he_text = STRDUP(text)) == NULL) {
		free(hent);
		return;
	}

	TTS_TAILQ_INSERT_TAIL(&hi->hi_ents, hent, he_entries);

	if (hi->hi_nents == 50)
		TTS_TAILQ_REMOVE(&hi->hi_ents, TTS_TAILQ_FIRST(&hi->hi_ents), he_entries);
	else
		++hi->hi_nents;
}

variable_t *
find_variable(name)
	WCHAR const	*name;
{
variable_t	*v;
	for (v = variables; v->va_name; v++)
		if (STRCMP(name, v->va_name) == 0)
			return v;
	return NULL;
}

static char *curfile;
static int lineno, nerr;

void
cmderr(const WCHAR *msg, ...)
{
va_list	ap;

	va_start(ap, msg);
	vcmderr(msg, ap);
	va_end(ap);
}

void
vcmderr(msg, ap)
	const WCHAR	*msg;
	va_list		 ap;
{
	nerr++;

	if (curfile) {
	WCHAR	s[1024];
	char	t[1024];
		VSNPRINTF(s, WSIZEOF(t), msg, ap);
		WCSTOMBS(t, s, sizeof(t));

		if (in_curses) {
			endwin();
			in_curses = 0;
		}

		fprintf(stderr, "\"%s\", line %d: %s\n",
			curfile, lineno, t);
	} else {
		input_macro(NULL);
		vdrawstatus(msg, ap);
	}
}

/*
 * Load configuration commands from the file .name.  Expects to be called
 * inside curses mode; it will endwin() if an error occurs.
 *
 * Returns 0 on success, or -1 if any errors occur.
 */
int
load_file(name)
	const char	*name;
{
FILE	*s;
char	 input[1024];

	nerr = 0;

	if ((s = fopen(name, "r")) == NULL) {
		if (errno == ENOENT)
			return 0;

		fprintf(stderr, "%s: %s", name, strerror(errno));
		return -1;
	}

	curfile = strdup(name);

	while (fgets(input, sizeof(input), s)) {
	size_t		nargs;
	WCHAR		**args;
	command_t	*cmds;
	WCHAR		 line[1024];

		++lineno;

		MBSTOWCS(line, input, WSIZEOF(line));

		if (line[0] == '#')
			continue;

		nargs = tokenise(line, &args);
		if (nargs == 0) {
			tokfree(&args);
			continue;
		}

		if ((cmds = find_command(args[0])) == NULL) {
			cmderr(WIDE("Unknown command \"%"FMT_L"s\"."), args[0]);
			nerr++;
			tokfree(&args);
			continue;
		}

		cmds->cm_hdl(nargs, args);
		tokfree(&args);
	}

	fclose(s);
	free(curfile);
	curfile = NULL;

	if (nerr)
		return -1;

	return 0;
}

#ifdef	USE_DARWIN_POWER
static void
prompt_sleep()
{
/*
 * We woke from sleep.  If there's a running entry, prompt the user to
 * subtract the time spent sleeping, in case they forgot to turn off
 * the timer.
 */
WCHAR	 pr[128];
int	 h, m, s = 0;

	donesleep = 0;

/* Only prompt if an entry is running */
	if (!running)
		return;

/* Draw the prompt */
	s = sleeptime;

	h = s / (60 * 60);
	s %= (60 * 60);
	m = s / 60;
	s %= 60;

	SNPRINTF(pr, WSIZEOF(pr),
		 WIDE("Remove %02d:%02d:%02d time asleep from running entry?"),
		 h, m, s);

	if (!yesno(pr)) {
		sleeptime = 0;
		return;
	}

/* 
 * This is a bit of a fudge, but it has the desired effect.  Alternatively
 * we could merge en_started into en_secs, then subtract that.
 */
	running->en_started += sleeptime;
	sleeptime = 0;
}
#endif	/* USE_DARWIN_POWER */

void
input_macro(s)
	WCHAR	*s;
{
	free(macro_text);
	macro_text = macro_pos = NULL;

	if (!s)
		return;

	macro_text = macro_pos = STRDUP(s);
}

int
input_char(c)
	WCHAR	*c;
{
	if (macro_pos) {
		if (*macro_pos) {
			*c = *macro_pos++;
			return 0;
		}
		free(macro_text);
		macro_text = macro_pos = NULL;
	}

	return GETCH(c);
}

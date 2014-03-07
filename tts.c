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

# include	<IOKit/pwr_mgt/IOPMLib.h>
# include	<IOKit/IOMessage.h>

# include	<pthread.h>
#endif

#include	"queue.h"
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

extern char const *tts_version;

volatile sig_atomic_t doexit;

static time_t laststatus;

history_t *searchhist;
history_t *prompthist;

#define STATFILE ".rttts"
#define RCFILE ".ttsrc"

int load(void);
int save(void);
static time_t lastsave;
static char statfile[PATH_MAX + 1];

static int load_file(const char *);

static tkey_t keys[] = {
	{ KEY_BREAK,		WIDE("<BREAK>")		},
	{ KEY_DOWN,		WIDE("<DOWN>")		},
	{ KEY_UP,		WIDE("<UP>")		},
	{ KEY_LEFT,		WIDE("<LEFT>")		},
	{ KEY_RIGHT,		WIDE("<RIGHT>")		},
	{ KEY_HOME,		WIDE("<HOME>")		},
	{ KEY_BACKSPACE,	WIDE("<BACKSPACE>")	},
	{ 0x7F,			WIDE("<BACKSPACE>")	}, /* DEL */
	{ KEY_F(0),		WIDE("<F0>")		},
	{ KEY_F(1),		WIDE("<F1>")		},
	{ KEY_F(2),		WIDE("<F2>")		},
	{ KEY_F(3),		WIDE("<F3>")		},
	{ KEY_F(4),		WIDE("<F4>")		},
	{ KEY_F(5),		WIDE("<F5>")		},
	{ KEY_F(6),		WIDE("<F6>")		},
	{ KEY_F(7),		WIDE("<F7>")		},
	{ KEY_F(8),		WIDE("<F8>")		},
	{ KEY_F(9),		WIDE("<F9>")		},
	{ KEY_F(10),		WIDE("<F10>")		},
	{ KEY_F(11),		WIDE("<F1l>")		},
	{ KEY_F(12),		WIDE("<F12>")		},
	{ KEY_F(13),		WIDE("<F13>")		},
	{ KEY_F(14),		WIDE("<F14>")		},
	{ KEY_F(15),		WIDE("<F15>")		},
	{ KEY_F(16),		WIDE("<F16>")		},
	{ KEY_F(17),		WIDE("<F17>")		},
	{ KEY_F(18),		WIDE("<F18>")		},
	{ KEY_F(19),		WIDE("<F19>")		},
	{ KEY_F(20),		WIDE("<F20>")		},
	{ KEY_F(21),		WIDE("<F21>")		},
	{ KEY_F(22),		WIDE("<F22>")		},
	{ KEY_F(23),		WIDE("<F23>")		},
	{ KEY_F(24),		WIDE("<F24>")		},
	{ KEY_NPAGE,		WIDE("<NEXT>")		},
	{ KEY_PPAGE,		WIDE("<PREV>")		},
	{ '\001',		WIDE("<CTRL-A>")	},
	{ '\002',		WIDE("<CTRL-B>")	},
	{ '\003',		WIDE("<CTRL-C>")	},
	{ '\004',		WIDE("<CTRL-D>")	},
	{ '\005',		WIDE("<CTRL-E>")	},
	{ '\006',		WIDE("<CTRL-F>")	},
	{ '\007',		WIDE("<CTRL-G>")	},
	{ '\010',		WIDE("<CTRL-H>")	},
	{ '\011',		WIDE("<CTRL-I>")	},
	{ '\011',		WIDE("<TAB>")		},
	{ '\012',		WIDE("<CTRL-J>")	},
	{ '\013',		WIDE("<CTRL-K>")	},
	{ '\014',		WIDE("<CTRL-L>")	},
	{ '\015',		WIDE("<CTRL-N>")	},
	{ '\016',		WIDE("<CTRL-O>")	},
	{ '\017',		WIDE("<CTRL-P>")	},
	{ '\020',		WIDE("<CTRL-Q>")	},
	{ '\021',		WIDE("<CTRL-R>")	},
	{ '\022',		WIDE("<CTRL-S>")	},
	{ '\023',		WIDE("<CTRL-T>")	},
	{ '\024',		WIDE("<CTRL-U>")	},
	{ '\025',		WIDE("<CTRL-V>")	},
	{ '\026',		WIDE("<CTRL-W>")	},
	{ '\027',		WIDE("<CTRL-X>")	},
	{ '\030',		WIDE("<CTRL-Y>")	},
	{ '\031',		WIDE("<CTRL-Z>")	},
	{ ' ',			WIDE("<SPACE>")		},
	{ KEY_ENTER,		WIDE("<ENTER>")		},
	{ KEY_BACKSPACE,	WIDE("<BACKSPACE>")	},
	{ KEY_DC,		WIDE("<DELETE>")	}
};

int pagestart;
entry_t *curent;

int showinv = 0;

static attrname_t attrnames[] = {
	{ WIDE("normal"),	WA_NORMAL	},
	{ WIDE("bold"),		WA_BOLD		},
	{ WIDE("reverse"),	WA_REVERSE	},
	{ WIDE("blink"),	WA_BLINK	},
	{ WIDE("dim"),		WA_DIM		},
	{ WIDE("underline"),	WA_UNDERLINE	},
	{ WIDE("standout"),	WA_STANDOUT	}
};

static colour_t colours[] = {
	{ WIDE("black"),	COLOR_BLACK	},
	{ WIDE("red"),		COLOR_RED	},
	{ WIDE("green"),	COLOR_GREEN	},
	{ WIDE("yellow"),	COLOR_YELLOW	},
	{ WIDE("blue"),		COLOR_BLUE	},
	{ WIDE("magenta"),	COLOR_MAGENTA	},
	{ WIDE("cyan"),		COLOR_CYAN	},
	{ WIDE("white"),	COLOR_WHITE	}
};

static short default_fg, default_bg;

style_t sy_header	= { 1, 0 },
	sy_status	= { 2, 0 },
	sy_entry	= { 3, 0 },
	sy_running	= { 4, 0 },
	sy_selected	= { 5, 0 },
	sy_date		= { 6, 0 };

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
static pthread_t power_thread;
static io_connect_t root_port;
static volatile sig_atomic_t donesleep;
static time_t sleeptime;

static void *power_thread_run(void *);
static void  power_event(void *, io_service_t, natural_t, void *);
static void  prompt_sleep(void);

static void
sigsleep(sig)
{
/* Delivered from the power thread as SIGUSR1 */
	donesleep = 1;
}

/*
 * Darwin power notifications are delivered from IOKit via Mach ports, which
 * is incompatible with TTS's curses-based main loop.  We therefore spawn a
 * separate thread to listen for these events, and when we receive one, we
 * translate it into a signal (SIGUSR1) which is delivered to the main thread
 * to handle.  The signal will interrupt getch().
 */
static void *
power_thread_run(arg)
	void	*arg;
{
sigset_t		ss;
IONotificationPortRef	port_ref;
io_object_t		notifier;

/* Block SIGUSR1 so it's always delivered to the main thread, not us */
	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &ss, NULL);

/* Register a handler for sleep and wake events */
	root_port = IORegisterForSystemPower(NULL, &port_ref, power_event,
			&notifier);
	CFRunLoopAddSource(CFRunLoopGetCurrent(),
		IONotificationPortGetRunLoopSource(port_ref),
		kCFRunLoopCommonModes);
	CFRunLoopRun();

	/*NOTREACHED*/
	return NULL;
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
		 * notify the main thread.
		 */
		time(&diff);
		diff -= sleep_started;
		sleeptime += diff;
		raise(SIGUSR1);
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

	setlocale(LC_ALL, "");

	signal(SIGTERM, sigexit);
	signal(SIGINT, sigexit);

#ifdef USE_DARWIN_POWER
	signal(SIGUSR1, sigsleep);
	pthread_create(&power_thread, NULL, power_thread_run, NULL);
#endif

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
	halfdelay(5);

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

	bind_key(WIDE("?"),		WIDE("help"));
	bind_key(WIDE("a"),		WIDE("add"));
	bind_key(WIDE("A"),		WIDE("add-old"));
	bind_key(WIDE("d"),		WIDE("delete"));
	bind_key(WIDE("u"),		WIDE("undelete"));
	bind_key(WIDE("q"),		WIDE("quit"));
	bind_key(WIDE("<CTRL-C>"),	WIDE("quit"));
	bind_key(WIDE("i"),		WIDE("invoice"));
	bind_key(WIDE("b"),		WIDE("billable"));
	bind_key(WIDE("m"),		WIDE("mark"));
	bind_key(WIDE("U"),		WIDE("unmarkall"));
	bind_key(WIDE("<SPACE>"),	WIDE("startstop"));
	bind_key(WIDE("e"),		WIDE("edit-desc"));
	bind_key(WIDE("\\"),		WIDE("edit-time"));
	bind_key(WIDE("<TAB>"),		WIDE("showhide-inv"));
	bind_key(WIDE("c"),		WIDE("copy"));
	bind_key(WIDE("+"),		WIDE("add-time"));
	bind_key(WIDE("-"),		WIDE("sub-time"));
	bind_key(WIDE("/"),		WIDE("search"));
	bind_key(WIDE("$"),		WIDE("sync"));
	bind_key(WIDE("<UP>"),		WIDE("prev"));
	bind_key(WIDE("<DOWN>"),	WIDE("next"));
	bind_key(WIDE(":"),		WIDE("execute"));
	bind_key(WIDE("M"),		WIDE("merge"));
	bind_key(WIDE("r"),		WIDE("interrupt"));
	bind_key(WIDE("R"),		WIDE("interrupt"));

	/*
	 * Make sure we can save (even if it's an empty file or nothing has
	 * changed) before the user starts entering entries.
	 */
	load();
	save();

	drawheader();
	drawstatus(WIDE(""));

	if (!TAILQ_EMPTY(&entries)) {
		curent = TAILQ_FIRST(&entries);
		while (!showinv && curent->en_flags.efl_invoiced)
			if ((curent = TAILQ_NEXT(curent, en_entries)) == NULL)
				break;
	}

	for (;;) {
	INT		 c;
	binding_t	*bi;

		if (doexit)
			break;

#ifdef USE_DARWIN_POWER
		if (donesleep)
			prompt_sleep();
#endif

		drawheader();
		drawentries();
		wrefresh(listwin);

		if (GETCH(&c) == ERR) {
			if (doexit)
				break;
#ifdef USE_DARWIN_POWER
			if (donesleep)
				prompt_sleep();
#endif
			if (time(NULL) - laststatus >= 2)
				drawstatus(WIDE(""));
			if (time(NULL) - lastsave > 60)
				save();
			continue;
		}

#ifdef KEY_RESIZE
		if (c == KEY_RESIZE)
			continue;
#endif

		drawstatus(WIDE(""));

		TAILQ_FOREACH(bi, &bindings, bi_entries) {
			if (bi->bi_code != c)
				continue;
			bi->bi_func->fn_hdl();
			goto next;
		}

		drawstatus(WIDE("Unknown command."));
	next:	;
	}

	save();
	endwin();
	return 0;
}

void
drawheader()
{
WCHAR	title[64];

	SNPRINTF(title, WSIZEOF(title), WIDE("TTS %s - Type '?' for help"),
		 tts_version);
	wmove(titwin, 0, 0);
	WADDSTR(titwin, title);

	if (itime > 0) {
	WCHAR	str[128];
	int	h, m, s;
	time_t	passed = time(NULL) - itime;

		time_to_hms(passed, h, m, s);
		SNPRINTF(str, WSIZEOF(str), WIDE("  *** MARK INTERRUPT: %02d:%02d:%02d ***"),
			h, m, s);

		wattron(titwin, A_BOLD);
		WADDSTR(titwin, str);
		wattroff(titwin, A_BOLD);
	}
	wclrtoeol(titwin);
	wrefresh(titwin);
}

void
vdrawstatus(msg, ap)
	const WCHAR	*msg;
	va_list		 ap;
{
WCHAR	s[1024];
	VSNPRINTF(s, WSIZEOF(s), msg, ap);

	wmove(statwin, 0, 0);
	WADDSTR(statwin, s);
	wclrtoeol(statwin);
	wrefresh(statwin);
	time(&laststatus);
}

void
drawstatus(const WCHAR *msg, ...)
{
va_list	ap;
	va_start(ap, msg);
	vdrawstatus(msg, ap);
	va_end(ap);
}

int
yesno(msg)
	const WCHAR	*msg;
{
WINDOW	*pwin;
INT	 c;

	pwin = newwin(1, COLS, LINES - 2, 0);
	keypad(pwin, TRUE);
	wattron(pwin, A_BOLD);
	wmove(pwin, 0, 0);
	WADDSTR(pwin, msg);
	WADDSTR(pwin, WIDE(" [y/N]? "));
	wattroff(pwin, A_BOLD);

	while (WGETCH(pwin, &c) == ERR
#ifdef KEY_RESIZE
			|| (c == KEY_RESIZE)
#endif
			)
		;

	return (c == 'Y' || c == 'y') ? 1 : 0;
}

WCHAR *
prompt(msg, def, hist)
	const WCHAR	*msg, *def;
	history_t	*hist;
{
WINDOW		*pwin;
WCHAR		 input[256];
size_t		 pos = 0;
histent_t	*histpos = NULL;

	if (hist == NULL)
		hist = prompthist;

	memset(input, 0, sizeof(input));
	if (def) {
		STRNCPY(input, def, WSIZEOF(input) - 1);
		pos = STRLEN(input);
	}

	pwin = newwin(1, COLS, LINES - 2, 0);
	keypad(pwin, TRUE);

	wattr_on(pwin, style_fg(sy_status), NULL);
	wbkgd(pwin, style_bg(sy_status));

	wattron(pwin, A_BOLD);
	wmove(pwin, 0, 0);
	WADDSTR(pwin, msg);
	wattroff(pwin, A_BOLD);

	curs_set(1);

	for (;;) {
	INT	c;
		wmove(pwin, 0, STRLEN(msg) + 1);
		WADDSTR(pwin, input);
		wclrtoeol(pwin);
		wmove(pwin, 0, STRLEN(msg) + 1 + pos);
		wrefresh(pwin);

		if (WGETCH(pwin, &c) == ERR)
			continue;

		switch (c) {
		case '\n':
		case '\r':
			goto end;

		case KEY_BACKSPACE:
		case 0x7F:
		case 0x08:
			if (pos) {
				if (pos == STRLEN(input))
					input[--pos] = 0;
				else {
				int	i = STRLEN(input);
					pos--;
					MEMCPY(input + pos, input + pos + 1, STRLEN(input) - pos);
					input[i] = 0;
				}
			}
			break;

		case KEY_DC:
			if (pos < STRLEN(input)) {
			int	i = STRLEN(input);
				MEMCPY(input + pos, input + pos + 1, STRLEN(input) - pos);
				input[i] = 0;
			}
			break;

		case KEY_LEFT:
			if (pos)
				pos--;
			break;

		case KEY_RIGHT:
			if (pos < STRLEN(input))
				pos++;
			break;

		case KEY_HOME:
		case 0x01:	/* ^A */
			pos = 0;
			break;

		case KEY_END:
		case 0x05:	/* ^E */
			pos = STRLEN(input);
			break;

		case 0x07:	/* ^G */
		case 0x1B:	/* ESC */
			curs_set(0);
			delwin(pwin);
			return NULL;

		case 0x15:	/* ^U */
			input[0] = 0;
			pos = 0;
			break;

#ifdef KEY_RESIZE
		case KEY_RESIZE:
			break;
#endif

		case KEY_UP:
			if (histpos == NULL) {
				if ((histpos = TAILQ_LAST(&hist->hi_ents, hentlist)) == NULL) {
					beep();
					break;
				}
			} else {
				if (TAILQ_PREV(histpos, hentlist, he_entries) == NULL) {
					beep();
					break;
				} else
					histpos = TAILQ_PREV(histpos, hentlist, he_entries);
			}


			STRNCPY(input, histpos->he_text, WSIZEOF(input) - 1);
			pos = STRLEN(input);
			break;

		case KEY_DOWN:
			if (histpos == NULL) {
				beep();
				break;
			}

			if (TAILQ_NEXT(histpos, he_entries) == NULL) {
				beep();
				break;
			} else
				histpos = TAILQ_NEXT(histpos, he_entries);


			STRNCPY(input, histpos->he_text, WSIZEOF(input) - 1);
			pos = STRLEN(input);
			break;

		default:
			if (pos != STRLEN(input)) {
				MEMMOVE(input + pos + 1, input + pos, STRLEN(input) - pos);
				input[pos++] = c;
			} else {
				input[pos++] = c;
				input[pos] = 0;
			}

			break;
		}
	}
end:	;

	curs_set(0);
	delwin(pwin);
	wtouchln(statwin, 1, 1, 1);
	hist_add(hist, input);
	return STRDUP(input);
}

void
drawentries()
{
int	 i, nlines;
int	 cline = 0;
time_t	 lastday = 0;
entry_t	*en;
chtype	 oldbg;

	getmaxyx(listwin, nlines, i);

	TAILQ_FOREACH(en, &entries, en_entries)
		en->en_flags.efl_visible = 0;

	en = TAILQ_FIRST(&entries);
	for (i = 0; i < pagestart; i++)
		if ((en = TAILQ_NEXT(en, en_entries)) == NULL)
			return;

	for (; en; en = TAILQ_NEXT(en, en_entries)) {
	time_t	 n;
	int	 h, s, m;
	WCHAR	 flags[10], stime[16], *p;
	attr_t	 attrs = WA_NORMAL;

		if (!showinv && en->en_flags.efl_invoiced)
			continue;

		oldbg = getbkgd(listwin);

		if (lastday != entry_day(en)) {
		struct tm	*lt;
		WCHAR		 lbl[128];
		time_t		 itime = entry_time_for_day(entry_day(en), 1, 0),
				 ntime = entry_time_for_day(entry_day(en), 0, 0),
				 btime = entry_time_for_day(entry_day(en), 2, bill_increment);
		int		 hi, mi, si,
				 hn, mn, sn,
				 hb, mb, sb,
				 ht, mt, st;
		WCHAR		 hdrtext[256];

			time_to_hms(itime, hi, mi, si);
			time_to_hms(ntime, hn, mn, sn);
			time_to_hms(btime, hb, mb, sb);
			time_to_hms(itime + ntime, ht, mt, st);

			oldbg = getbkgd(listwin);
			wbkgdset(listwin, style_bg(sy_entry));
			wattr_on(listwin, style_fg(sy_entry), NULL);

			wmove(listwin, cline, 0);
			wclrtoeol(listwin);

			wbkgdset(listwin, oldbg);
			wattr_off(listwin, style_fg(sy_entry), NULL);

			if (++cline >= nlines)
				break;

			lastday = entry_day(en);
			lt = localtime(&lastday);

			STRFTIME(lbl, WSIZEOF(lbl), WIDE("%A, %d %B %Y"), lt);
			if (show_billable)
				SNPRINTF(hdrtext, WSIZEOF(hdrtext),
					 WIDE("%-30"FMT_L"s [I:%02d:%02d:%02d / "
					"N:%02d:%02d:%02d / T:%02d:%02d:%02d / "
					"B:%02d:%02d:%02d]"),
					lbl, hi, mi, si, hn, mn, sn, ht, mt, st,
					hb, mb, sb);
			else
				SNPRINTF(hdrtext, WSIZEOF(hdrtext),
					WIDE("%-30"FMT_L"s [I:%02d:%02d:%02d / "
					"N:%02d:%02d:%02d / T:%02d:%02d:%02d]"),
					lbl, hi, mi, si, hn, mn, sn, ht, mt, st);

			wattr_on(listwin, style_fg(sy_date), NULL);
			wbkgdset(listwin, style_bg(sy_date));
			wmove(listwin, cline, 0);
			WADDSTR(listwin, hdrtext);
			wclrtoeol(listwin);
			wattr_off(listwin, style_fg(sy_date), NULL);
			wbkgdset(listwin, oldbg);

			if (++cline >= nlines) {
				wbkgdset(listwin, oldbg);
				wattr_off(listwin, style_fg(sy_date), NULL);
				break;
			}

			oldbg = getbkgd(listwin);
			wbkgdset(listwin, style_bg(sy_entry));
			wattr_on(listwin, style_fg(sy_entry), NULL);

			wmove(listwin, cline, 0);
			wclrtoeol(listwin);

			wbkgdset(listwin, oldbg);
			wattr_off(listwin, style_fg(sy_entry), NULL);

			if (++cline >= nlines)
				break;
		}

		en->en_flags.efl_visible = 1;
		wmove(listwin, cline, 0);

		attrs = style_fg(sy_entry);

		if (en->en_started && en == curent)
			attrs = style_fg(sy_selected) |
				(style_fg(sy_running) & (
					WA_STANDOUT | WA_UNDERLINE |
					WA_REVERSE | WA_BLINK | WA_DIM |
					WA_BOLD));
		else if (en->en_started)
			attrs = style_fg(sy_running);
		else if (en == curent)
			attrs = style_fg(sy_selected);

		wbkgdset(listwin, ' ' | (attrs & ~WA_UNDERLINE));
		wattr_on(listwin, attrs, NULL);

		if (en == curent) {
			WADDSTR(listwin, WIDE("  -> "));
		} else
			WADDSTR(listwin, WIDE("     "));

		n = en->en_secs;
		if (en->en_started)
			n += time(NULL) - en->en_started;
		h = n / (60 * 60);
		n %= (60 * 60);
		m = n / 60;
		n %= 60;
		s = n;

		SNPRINTF(stime, WSIZEOF(stime), WIDE("%02d:%02d:%02d%c "),
			h, m, s, (itime && (en == running)) ? '*' : ' ');
		WADDSTR(listwin, stime);

		memset(flags, 0, sizeof(flags));
		p = flags;

		if (en->en_flags.efl_marked)
			*p++ = 'M';
		else
			*p++ = ' ';

		if (en->en_flags.efl_invoiced)
			*p++ = 'I';
		else
			*p++ = ' ';

		if (!en->en_flags.efl_nonbillable)
			*p++ = 'B';
		else
			*p++ = ' ';

		if (en->en_flags.efl_deleted)
			*p++ = 'D';
		else
			*p++ = ' ';

		if (*flags) {
		WCHAR	s[10];
			SNPRINTF(s, WSIZEOF(s), WIDE("%-5"FMT_L"s  "), flags);
			WADDSTR(listwin, s);
		} else
			WADDSTR(listwin, WIDE("       "));

		WADDSTR(listwin, en->en_desc);
		wclrtoeol(listwin);
		wbkgdset(listwin, oldbg);
		wattr_off(listwin, attrs, NULL);

		if (++cline >= nlines)
			return;
	}

	oldbg = getbkgd(listwin);
	wattr_on(listwin, style_fg(sy_entry), NULL);
	wbkgdset(listwin, style_bg(sy_entry));
	for (; cline < nlines; cline++) {
		wmove(listwin, cline, 0);
		wclrtoeol(listwin);
	}
	wattr_off(listwin, style_fg(sy_entry), NULL);
	wbkgdset(listwin, oldbg);
}

int
load()
{
FILE	*f;
char	 input[4096];
WCHAR	 line[4096];
entry_t	*en;

	TAILQ_FOREACH(en, &entries, en_entries)
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

	TAILQ_FOREACH_REVERSE(en, &entries, entrylist, en_entries) {
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

void
errbox(const WCHAR *msg, ...)
{
va_list	ap;
	va_start(ap, msg);
	verrbox(msg, ap);
	va_end(ap);
}

void
verrbox(msg, ap)
	const WCHAR	*msg;
	va_list		 ap;
{
WCHAR	 text[4096];
WINDOW	*ewin;

#define ETITLE	WIDE(" Error ")
#define ECONT	WIDE(" <OK> ")
int	width;
INT	c;

	VSNPRINTF(text, WSIZEOF(text), msg, ap);
	width = STRLEN(text);

	ewin = newwin(6, width + 4,
			(LINES / 2) - ((1 + 2)/ 2),
			(COLS / 2) - ((width + 2) / 2));
	leaveok(ewin, TRUE);
	wborder(ewin, 0, 0, 0, 0, 0, 0, 0, 0);

	wattron(ewin, A_REVERSE | A_BOLD);
	wmove(ewin, 0, (width / 2) - (WSIZEOF(ETITLE) - 1)/2);
	WADDSTR(ewin, ETITLE);
	wattroff(ewin, A_REVERSE | A_BOLD);

	wmove(ewin, 2, 2);
	WADDSTR(ewin, text);
	wattron(ewin, A_REVERSE | A_BOLD);
	wmove(ewin, 4, (width / 2) - ((WSIZEOF(ECONT) - 1) / 2));
	WADDSTR(ewin, ECONT);
	wattroff(ewin, A_REVERSE | A_BOLD);

	for (;;) {
		if (WGETCH(ewin, &c) == ERR)
			continue;
		if (c == '\r')
			break;
	}

	delwin(ewin);
}

history_t *
hist_new()
{
history_t	*hi;
	if ((hi = calloc(1, sizeof(*hi))) == NULL)
		return NULL;
	TAILQ_INIT(&hi->hi_ents);
	return hi;
}

void hist_add(hi, text)
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

	TAILQ_INSERT_TAIL(&hi->hi_ents, hent, he_entries);

	if (hi->hi_nents == 50)
		TAILQ_REMOVE(&hi->hi_ents, TAILQ_FIRST(&hi->hi_ents), he_entries);
	else
		++hi->hi_nents;
}

/*
 * Return the tkey_t for the key called .name, or NULL if such a key doesn't
 * exist.
 */
tkey_t *
find_key(name)
	const WCHAR	*name;
{
size_t	i;

	for (i = 0; i < sizeof(keys) / sizeof(*keys); i++)
		if (STRCMP(name, keys[i].ky_name) == 0)
			return &keys[i];
	return NULL;
}

/*
 * Return the function_t for the function called .name, or NULL if such a
 * function doesn't exist.
 */
function_t *
find_func(name)
	const WCHAR	*name;
{
function_t	*f;
	for (f = funcs; f->fn_name; f++)
		if (STRCMP(name, f->fn_name) == 0)
			return f;
	return NULL;
}

/*
 * Bind .keyname to run the function .funcname.  If a binding for .keyname
 * already exists, overwrite it.
 *
 * If .keyname is a single character, e.g. 'a', it is used as a key name
 * directly, rather than being looked up in the key table.
 */
void
bind_key(keyname, funcname)
	const WCHAR	*keyname, *funcname;
{
tkey_t		*key = NULL;
function_t	*func;
binding_t	*binding;
INT		 code;

	/* Find the key and the function */
	if (STRLEN(keyname) > 1) {
		if ((key = find_key(keyname)) == NULL) {
			errbox(WIDE("Unknown key \"%"FMT_L"s\""), keyname);
			return;
		}
		code = key->ky_code;
	} else
		code = *keyname;

	if ((func = find_func(funcname)) == NULL) {
		errbox(WIDE("Unknown function \"%"FMT_L"s\""), funcname);
		return;
	}

	/* Do we already have a binding for this key? */
	TAILQ_FOREACH(binding, &bindings, bi_entries) {
		if (binding->bi_code == code) {
			binding->bi_func = func;
			return;
		}
	}

	/* No, add a new one */
	if ((binding = calloc(1, sizeof(*binding))) == NULL)
		return;

	binding->bi_key = key;
	binding->bi_func = func;
	binding->bi_code = code;
	TAILQ_INSERT_TAIL(&bindings, binding, bi_entries);
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

int
attr_find(name, result)
	const WCHAR	*name;
	attr_t		*result;
{
size_t	i;
	for (i = 0; i < sizeof(attrnames) / sizeof(*attrnames); i++) {
		if (STRCMP(attrnames[i].an_name, name) == 0) {
			*result = attrnames[i].an_value;
			return 0;
		}
	}

	return -1;
}

int
colour_find(name, result)
	const WCHAR	*name;
	short		*result;
{
size_t	i;
	for (i = 0; i < sizeof(colours) / sizeof(*colours); i++) {
		if (STRCMP(colours[i].co_name, name) == 0) {
			*result = colours[i].co_value;
			return 0;
		}
	}

	return -1;
}

void
style_clear(sy)
	style_t	*sy;
{
	init_pair(sy->sy_pair, default_fg, default_bg);
	sy->sy_attrs = WA_NORMAL;
}

int
style_set(sy, fg, bg)
	style_t		*sy;
	const WCHAR	*fg, *bg;
{
	sy->sy_attrs = WA_NORMAL;
	init_pair(sy->sy_pair, default_fg, default_bg);
	return style_add(sy, fg, bg);
}

int
style_add(sy, fg, bg)
	style_t		*sy;
	const WCHAR	*fg, *bg;
{
attr_t	at;
short	colfg, colbg = default_bg;

	if (colour_find(fg, &colfg) == 0) {
		if (bg && (colour_find(bg, &colbg) == -1))
			return -1;

		init_pair(sy->sy_pair, colfg, colbg);
		return 0;
	}

	if (attr_find(fg, &at) == -1)
		return -1;
	sy->sy_attrs |= at;
	return 0;
}

void
apply_styles()
{
	wbkgd(statwin, style_bg(sy_status));
	wattr_on(statwin, style_fg(sy_status), NULL);
	drawstatus(WIDE(""));

	wbkgd(titwin, style_bg(sy_header));
	wattr_on(titwin, style_fg(sy_header), NULL);
	drawheader();
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
	} else
		vdrawstatus(msg, ap);
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

int
prduration(pr, hh, mm, ss)
	WCHAR	*pr;
	int	*hh, *mm, *ss;
{
WCHAR	*tstr;
int	 h, m, s;
	if ((tstr = prompt(pr, WIDE("00:00:00"), NULL)) == NULL)
		return -1;

	if (!*tstr) {
		drawstatus(WIDE("No duration entered"));
		free(tstr);
		return -1;
	}

	if (SSCANF(tstr, WIDE("%d:%d:%d"), &h, &m, &s) != 3) {
		h = 0;
		if (SSCANF(tstr, WIDE("%d:%d"), &m, &s) != 2) {
			m = 0;
			if (SSCANF(tstr, WIDE("%d"), &s) != 1) {
				free(tstr);
				drawstatus(WIDE("Invalid time format."));
				return -1;
			}
		}
	}

	free(tstr);

	if (m >= 60) {
		drawstatus(WIDE("Minutes cannot be more than 59."));
		return -1;
	}

	if (s >= 60) {
		drawstatus(WIDE("Seconds cannot be more than 59."));
		return -1;
	}

	*hh = h;
	*mm = m;
	*ss = s;
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

/*
 * TTS - track your time.
 * Copyright (c) 2012, 2013 River Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */
/* $Header: /cvsroot/rttts/tts.c,v 1.77 2013/04/24 11:22:13 river Exp $ */

#define		__EXTENSIONS__
/*
 * Older versions of glibc don't supporte _XOPEN_SOURCE==700 and require
 * this for wcsdup() prototype.
 */
#define		_GNU_SOURCE
#define		_XOPEN_SOURCE 700
#define		_XOPEN_SOURCE_EXTENDED

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

#include	"config.h"
#include	"version.h"

#if defined HAVE_NCURSESW_CURSES_H
#	include <ncursesw/curses.h>
#elif defined HAVE_NCURSESW_H
#	include <ncursesw.h>
#elif defined HAVE_NCURSES_CURSES_H
#	include <ncurses/curses.h>
#elif defined HAVE_NCURSES_H
#	include <ncurses.h>
#elif defined HAVE_CURSES_H
#	include <curses.h>
#else
#	error "SVR4 or XSI compatible curses header file required"
#endif

#include	"queue.h"

#ifdef	HAVE_CURSES_ENHANCED
#	define	WPFX(x)		wcs##x
#	define	WIDE(x)		L##x
#	define	ISX(x)		isw##x
#	define	WCHAR		wchar_t
#	define	FMT_L		"l"
#	define	SNPRINTF	swprintf
#	define	VSNPRINTF	vswprintf
#	define	SSCANF		swscanf
#	define	MEMCPY		wmemcpy
#	define	MEMMOVE		wmemmove
#	define	MBSTOWCS	mbstowcs
#	define	WCSTOMBS	wcstombs
#	define	FPRINTF		fwprintf
#	define	STRTOK		wcstok

#	define	GETCH		get_wch
#	define	WGETCH		wget_wch
#	define	ADDSTR		addwstr
#	define	WADDSTR		waddwstr
#	define	INT		wint_t
#else
#	define	WPFX(x)		str##x
#	define	WIDE(x)		x
#	define	ISX(x)		is##x
#	define	WCHAR		char
#	define	FMT_L
#	define	SNPRINTF	snprintf
#	define	VSNPRINTF	vsnprintf
#	define	SSCANF		sscanf
#	define	MEMCPY		memcpy
#	define	MEMMOVE		memmove
#	define	MBSTOWCS	strncpy
#	define	WCSTOMBS	strncpy
#	define	FPRINTF		fprintf
#	define	STRTOK		strtok_r

#	define	ADDSTR		addstr
#	define	WADDSTR		waddstr
#	define	INT		int

static int
tss_wgetch(win, d)
	WINDOW	*win;
	int	*d;
{
int	c;
	if ((c = wgetch(win)) == ERR)
		return ERR;
	*d = c;
	return OK;
}
#	define	WGETCH		tss_wgetch
#	define	GETCH(c)	tss_wgetch(stdscr,c)
#endif

#define STRLEN		WPFX(len)
#define STRCMP		WPFX(cmp)
#define STRNCMP		WPFX(cmp)
#define STRCPY		WPFX(cpy)
#define STRNCPY		WPFX(ncpy)
#define STRSTR		WPFX(str)
#define STRFTIME	WPFX(ftime)
#define STRDUP		WPFX(dup)

#define	ISSPACE		ISX(space)

#define	WSIZEOF(s)	(sizeof(s) / sizeof(WCHAR))

static volatile sig_atomic_t doexit;

static WINDOW *titwin, *statwin, *listwin;

static int in_curses;

static void drawstatus(const WCHAR *msg, ...);
static void vdrawstatus(const WCHAR *msg, va_list);
static void drawheader(void);
static void drawentries(void);

static time_t laststatus;

typedef struct entry {
	WCHAR			*en_desc;
	int			 en_secs;
	time_t			 en_started;
	time_t			 en_created;

	struct {
		int	efl_visible:1;
		int	efl_invoiced:1;
		int	efl_marked:1;
		int	efl_deleted:1;
	}			 en_flags;
	TAILQ_ENTRY(entry)	 en_entries;
} entry_t;

static TAILQ_HEAD(entrylist, entry) entries = TAILQ_HEAD_INITIALIZER(entries);

static entry_t *running;

static entry_t *entry_new(const WCHAR *);
static void entry_start(entry_t *);
static void entry_stop(entry_t *);
static void entry_free(entry_t *);
static void entry_account(entry_t *);
static time_t entry_time_for_day(time_t, int);

#define time_day(t) (((t) / (60 * 60 * 24)) * (60 * 60 * 24))
#define entry_day(e) (time_day((e)->en_created))

#define time_to_hms(t, h, m, s)	do {			\
				time_t	n = t;		\
				h = n / (60 * 60);	\
				n %= (60 * 60);		\
				m = n / 60;		\
				n %= 60;		\
				s = n;			\
			} while (0)

#define NHIST 50
typedef struct histent {
	WCHAR			*he_text;
	TAILQ_ENTRY(histent)	 he_entries;
} histent_t;
typedef TAILQ_HEAD(hentlist, histent) hentlist_t;

typedef struct history {
	int		hi_nents;
	hentlist_t	hi_ents;
} history_t;

static history_t *hist_new(void);
static void hist_add(history_t *, WCHAR const *);

static history_t *searchhist, *prompthist;

static WCHAR *prompt(WCHAR const *, WCHAR const *, history_t *);
static int prduration(WCHAR *prompt, int *h, int *m, int *s);
static int yesno(WCHAR const *);
static void errbox(WCHAR const *, ...);
static void verrbox(WCHAR const *, va_list);

#define STATFILE ".rttts"
#define RCFILE ".ttsrc"

static int load(void);
static int save(void);
static time_t lastsave;
static char statfile[PATH_MAX + 1];

static int load_file(const char *);

static void cursadvance(void);

static void kadd(void);
static void kaddold(void);
static void kquit(void);
static void kup(void);
static void kdown(void);
static void ktoggle(void);
static void kinvoiced(void);
static void keddesc(void);
static void kedtime(void);
static void ktoggleinv(void);
static void kcopy(void);
static void kaddtime(void);
static void kdeltime(void);
static void khelp(void);
static void kmark(void);
static void kunmarkall(void);
static void ksearch(void);
static void kmarkdel(void);
static void kundel(void);
static void ksync(void);
static void kexec(void);
static void kmerge(void);
static void kint(void);
static void kmarkint(void);

typedef struct function {
	const WCHAR	*fn_name;
	void		(*fn_hdl) (void);
	const WCHAR	*fn_desc;
} function_t;

static function_t funcs[] = {
	{ WIDE("help"),		khelp,		WIDE("display help screen") },
	{ WIDE("add"),		kadd,		WIDE("add a new entry and start the timer") },
	{ WIDE("add-old"),	kaddold,	WIDE("add a new entry and specify its duration") },
	{ WIDE("delete"),	kmarkdel,	WIDE("delete the current entry") },
	{ WIDE("undelete"),	kundel,		WIDE("undelete the current entry") },
	{ WIDE("quit"),		kquit,		WIDE("exit TTS") },
	{ WIDE("invoice"),	kinvoiced,	WIDE("set the current entry as invoiced") },
	{ WIDE("mark"),		kmark,		WIDE("mark the current entry") },
	{ WIDE("unmarkall"),	kunmarkall,	WIDE("unmark all entries") },
	{ WIDE("startstop"),	ktoggle,	WIDE("start or stop the timer") },
	{ WIDE("edit-desc"),	keddesc,	WIDE("edit the current entry's description") },
	{ WIDE("edit-time"),	kedtime,	WIDE("edit the current entry's duration") },
	{ WIDE("showhide-inv"),	ktoggleinv,	WIDE("show or hide invoiced entries") },
	{ WIDE("copy"),		kcopy,		WIDE("copy the current entry's description to a new entry") },
	{ WIDE("add-time"),	kaddtime,	WIDE("add time to the current entry") },
	{ WIDE("sub-time"),	kdeltime,	WIDE("subtract time from the current entry") },
	{ WIDE("search"),	ksearch,	WIDE("search for an entry by name") },
	{ WIDE("sync"),		ksync,		WIDE("purge all deleted entries") },
	{ WIDE("prev"),		kup,		WIDE("move to the previous entry") },
	{ WIDE("next"),		kdown,		WIDE("move to the next entry") },
	{ WIDE("execute"),	kexec,		WIDE("execute a configuration command") },
	{ WIDE("merge"),	kmerge,		WIDE("merge marked entries into current entry") },
	{ WIDE("interrupt"),	kint,		WIDE("split current entry into new entry")},
	{ WIDE("mark-interrupt"), kmarkint,	WIDE("start interrupt timer for current entry")}
};

typedef struct tkey {
	INT		 ky_code;
	const WCHAR	*ky_name;
} tkey_t;

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

typedef struct binding {
	INT			 bi_code;
	tkey_t			*bi_key;
	function_t		*bi_func;
	TAILQ_ENTRY(binding)	 bi_entries;
} binding_t;

typedef struct command {
	const WCHAR	*cm_name;
	void		(*cm_hdl) (size_t, WCHAR **);
} command_t;

static command_t *find_command(const WCHAR *);
static void c_bind(size_t, WCHAR **);
static void c_style(size_t, WCHAR **);
static void c_set(size_t, WCHAR **);

static command_t commands[] = {
	{ WIDE("bind"),		c_bind	},
	{ WIDE("style"),	c_style	},
	{ WIDE("set"),		c_set },
};

static void cmderr(const WCHAR *, ...);
static void vcmderr(const WCHAR *, va_list);

static size_t tokenise(const WCHAR *, WCHAR ***result);
static void tokfree(WCHAR ***);

static TAILQ_HEAD(bindlist, binding) bindings = TAILQ_HEAD_INITIALIZER(bindings);

static tkey_t *find_key(const WCHAR *name);
static function_t *find_func(const WCHAR *name);
static void bind_key(const WCHAR *key, const WCHAR *func);

static int pagestart;
static entry_t *curent;

static int showinv = 0;

typedef struct style {
	short	sy_pair;
	attr_t	sy_attrs;
} style_t;

#define style_fg(s)	(COLOR_PAIR((s).sy_pair) | (s).sy_attrs)
#define style_bg(s)	((INT) ' ' | COLOR_PAIR((s).sy_pair) | ((s).sy_attrs & ~WA_UNDERLINE))

typedef struct attrname {
	const WCHAR	*an_name;
	attr_t		 an_value;
} attrname_t;

static attrname_t attrnames[] = {
	{ WIDE("normal"),	WA_NORMAL	},
	{ WIDE("bold"),		WA_BOLD		},
	{ WIDE("reverse"),	WA_REVERSE	},
	{ WIDE("blink"),	WA_BLINK	},
	{ WIDE("dim"),		WA_DIM		},
	{ WIDE("underline"),	WA_UNDERLINE	},
	{ WIDE("standout"),	WA_STANDOUT	}
};

typedef struct colour {
	const WCHAR	*co_name;
	short		 co_value;
} colour_t;

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

static int attr_find(const WCHAR *name, attr_t *result);
static int colour_find(const WCHAR *name, short *result);

static void style_clear(style_t *);
static int style_set(style_t *, const WCHAR *fg, const WCHAR *bg);
static int style_add(style_t *, const WCHAR *fg, const WCHAR *bg);

static short default_fg, default_bg;

static void apply_styles(void);

static style_t
	sy_header = { 1, 0 },
	sy_status = { 2, 0 },
	sy_entry = { 3, 0 },
	sy_running = { 4, 0 },
	sy_selected = { 5, 0 },
	sy_date = { 6, 0 };

static time_t itime = 0;

static int delete_advance = 1;
static int mark_advance = 1;

#define VTYPE_INT	1
#define VTYPE_BOOL	2
#define VTYPE_STRING	3

typedef struct variable {
	WCHAR const	*va_name;
	int		 va_type;
	void		*va_addr;
} variable_t;

static variable_t variables[] = {
	{ WIDE("delete_advance"),	VTYPE_BOOL,	&delete_advance },
	{ WIDE("mark_advance"),		VTYPE_BOOL,	&mark_advance }
};

static variable_t	*find_variable(const WCHAR *name);

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
int		 i;
char		 rcfile[PATH_MAX + 1];

	setlocale(LC_ALL, "");

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
	bind_key(WIDE("r"),		WIDE("mark-interrupt"));
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
	size_t		 s;
	binding_t	*bi;

		if (doexit)
			break;

		drawheader();
		drawentries();
		wrefresh(listwin);

		if (GETCH(&c) == ERR) {
			if (doexit)
				break;
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
kquit()
{
entry_t	*en;
int	 ndel = 0;

	TAILQ_FOREACH(en, &entries, en_entries) {
		if (en->en_flags.efl_deleted)
			ndel++;
	}

	if (ndel) {
	WCHAR	s[128];
		SNPRINTF(s, WSIZEOF(s), WIDE("Purge %d deleted entries?"), ndel);
		if (yesno(s)) {
			ksync();
		}
	}

	doexit = 1;
}

void
kadd()
{
WCHAR	*name;
entry_t	*en;
	name = prompt(WIDE("Description:"), NULL, NULL);
	if (!name || !*name) {
		free(name);
		return;
	}
	en = entry_new(name);
	entry_start(en);
	curent = en;
	save();
}

void
kaddold()
{
WCHAR	*name;
entry_t	*en;
	name = prompt(WIDE("Description:"), NULL, NULL);

	if (!name || !*name) {
		free(name);
		return;
	}

	en = entry_new(name);
	curent = en;
	kedtime();
	save();
}

void
ktoggle()
{
	itime = 0;

	if (!curent)
		return;

	if (curent == running) {
		entry_stop(curent);
		save();
		return;
	}

	if (running)
		entry_stop(running);
	entry_start(curent);
	save();
}

void
kundel()
{
	if (!curent)
		return;

	curent->en_flags.efl_deleted = 0;
	if (delete_advance)
		cursadvance();
}

void
kmarkdel()
{
entry_t	*newcur;
entry_t	*en, *ten;
int	 nmarked = 0;

	TAILQ_FOREACH(en, &entries, en_entries) {
		if (en->en_flags.efl_marked) {
			nmarked++;
			en->en_flags.efl_deleted = 1;
		}
	}

	if (nmarked)
		return;

	if (!curent) {
		drawstatus(WIDE("No entries to delete."));
		return;
	}

	curent->en_flags.efl_deleted = 1;

	if (delete_advance)
		cursadvance();
}

/*
 * Move the cursor to the next entry after an operation like mark or deleted.
 * If there are no suitable entries after this one, move it backwards instead.
 */
void
cursadvance()
{
entry_t	*en;

	if (!curent) {
		curent = TAILQ_FIRST(&entries);
		return;
	}

	/*
	 * Try to find the next suitable entry to move the cursor to.
	 */
	for (en = TAILQ_NEXT(curent, en_entries); en; en = TAILQ_NEXT(en, en_entries)) {
		if (!showinv && en->en_flags.efl_invoiced)
			continue;
		curent = en;
		if (!curent->en_flags.efl_visible)
			pagestart++;
		return;
	}

	/*
	 * No entries; if the current entry is visible, stay here, otherwise
	 * try moving backwards instead.
	 */
	if (showinv || !curent->en_flags.efl_invoiced)
		return;

	for (en = TAILQ_PREV(curent, entrylist, en_entries); en;
	     en = TAILQ_PREV(en, entrylist, en_entries)) {
		if (!showinv && en->en_flags.efl_invoiced)
			continue;
		curent = en;
		if (!curent->en_flags.efl_visible)
			pagestart--;
		return;
	}

	/*
	 * Couldn't find any entries at all?
	 */
	curent = NULL;
}

void
ksync()
{
entry_t	*en, *ten;

	TAILQ_FOREACH_SAFE(en, &entries, en_entries, ten) {
		if (!en->en_flags.efl_deleted)
			continue;
		if (en == curent)
			curent = NULL;
		TAILQ_REMOVE(&entries, en, en_entries);
		entry_free(en);
	}

	if (curent == NULL)
		curent = TAILQ_FIRST(&entries);
	save();
}

void
kup()
{
entry_t	*prev = curent;
	if (!curent)
		return;

	do {
		if ((prev = TAILQ_PREV(prev, entrylist, en_entries)) == NULL)
			break;
	} while (!showinv && prev->en_flags.efl_invoiced);

	if (prev == NULL) {
		drawstatus(WIDE("Already at first entry."));
		return;
	}

	curent = prev;
	if (!curent->en_flags.efl_visible)
		pagestart--;
}

void
kdown()
{
entry_t	*next = curent;
	if (!curent)
		return;

	do {
		if ((next = TAILQ_NEXT(next, en_entries)) == NULL)
			break;
	} while (!showinv && next->en_flags.efl_invoiced);

	if (next == NULL) {
		drawstatus(WIDE("Already at last entry."));
		return;
	}

	curent = next;
	if (!curent->en_flags.efl_visible)
		pagestart++;
}

void
kinvoiced()
{
entry_t	*en;
int	 anymarked = 0;

	TAILQ_FOREACH(en, &entries, en_entries) {
		if (!en->en_flags.efl_marked)
			continue;
		anymarked = 1;
		en->en_flags.efl_invoiced = !en->en_flags.efl_invoiced;
		en->en_flags.efl_marked = 0;
	}

	if (anymarked) {
		save();
		return;
	}

	if (!curent) {
		drawstatus(WIDE("No entry selected."));
		return;
	}

	curent->en_flags.efl_invoiced = !curent->en_flags.efl_invoiced;
	save();

	en = curent;

	if (showinv) {
		if (TAILQ_NEXT(curent, en_entries) != NULL)
			curent = TAILQ_NEXT(curent, en_entries);
		return;
	}

	/*
	 * Try to find the next uninvoiced request to move the cursor to.
	 */
	for (;;) {
		if ((curent = TAILQ_NEXT(curent, en_entries)) == NULL)
			break;	/* end of list */
		if (!curent->en_flags.efl_invoiced)
			return;
	}

	/*
	 * We didn't find any, so try searching backwards instead.
	 */
	for (curent = en;;) {
		if ((curent = TAILQ_PREV(curent, entrylist, en_entries)) == NULL)
			break;	/* end of list */
		if (!curent->en_flags.efl_invoiced)
			return;
	}
}

void
keddesc()
{
WCHAR	*new;

	if (!curent) {
		drawstatus(WIDE("No entry selected."));
		return;
	}

	if ((new = prompt(WIDE("Description:"), curent->en_desc, NULL)) == NULL)
		return;

	free(curent->en_desc);
	curent->en_desc = new;
	save();
}

void
kedtime()
{
WCHAR	*new, old[64];
time_t	 n;
int	 h, m, s;

	if (!curent) {
		drawstatus(WIDE("No entry selected."));
		return;
	}

	n = curent->en_secs;
	if (curent->en_started)
		n += time(NULL) - curent->en_started;
	h = n / (60 * 60);
	n %= (60 * 60);
	m = n / 60;
	n %= 60;
	s = n;

	SNPRINTF(old, WSIZEOF(old), WIDE("%02d:%02d:%02d"), h, m, s);
	if ((new = prompt(WIDE("Duration [HH:MM:SS]:"), old, NULL)) == NULL)
		return;

	if (!SSCANF(new, WIDE("%d:%d:%d"), &h, &m, &s)) {
		free(new);
		drawstatus(WIDE("Invalid duration."));
	}

	curent->en_secs = (h * 60 * 60) + (m * 60) + s;
	if (curent->en_started)
		time(&curent->en_started);

	save();
}

void
ktoggleinv()
{
entry_t	*en = curent;
	showinv = !showinv;
	drawstatus(WIDE("%"FMT_L"s invoiced entries."),
		showinv ? L"Showing" : L"Hiding");

	if (curent && !curent->en_flags.efl_invoiced)
		return;

	if (!curent) {
		curent = TAILQ_FIRST(&entries);
		return;
	}

	/*
	 * Try to find the next uninvoiced request to move the cursor to.
	 */
	for (;;) {
		if ((curent = TAILQ_NEXT(curent, en_entries)) == NULL)
			break;	/* end of list */
		if (!curent->en_flags.efl_invoiced)
			return;
	}

	/*
	 * We didn't find any, so try searching backwards instead.
	 */
	for (curent = en;;) {
		if ((curent = TAILQ_PREV(curent, entrylist, en_entries)) == NULL)
			break;	/* end of list */
		if (!curent->en_flags.efl_invoiced)
			return;
	}
}

void
kcopy()
{
entry_t	*en;

	if (!curent) {
		drawstatus(WIDE("No entry selected."));
		return;
	}

	en = entry_new(curent->en_desc);
	curent = en;
	entry_start(en);
	save();
}

void
kaddtime()
{
WCHAR	*tstr;
int	 h = 0, m = 0, s = 0, secs;
	if (!curent) {
		drawstatus(WIDE("No entry selected."));
		return;
	}

	if ((tstr = prompt(WIDE("Time to add ([[HH:]MM:]SS):"), NULL, NULL)) == NULL)
		return;

	if (!*tstr) {
		drawstatus(WIDE(""));
		free(tstr);
		return;
	}

	if (SSCANF(tstr, WIDE("%d:%d:%d"), &h, &m, &s) != 3) {
		h = 0;
		if (SSCANF(tstr, WIDE("%d:%d"), &m, &s) != 2) {
			m = 0;
			if (SSCANF(tstr, WIDE("%d"), &s) != 1) {
				free(tstr);
				drawstatus(WIDE("Invalid time format."));
				return;
			}
		}
	}

	free(tstr);

	if (m >= 60) {
		drawstatus(WIDE("Minutes cannot be more than 59."));
		return;
	}

	if (s >= 60) {
		drawstatus(WIDE("Seconds cannot be more than 59."));
		return;
	}

	secs = s + m*60 + h*60*60;
	curent->en_secs += secs;
	save();
}

void
kdeltime()
{
WCHAR	*tstr;
int	 h = 0, m = 0, s = 0, secs;
	if (!curent) {
		drawstatus(WIDE("No entry selected."));
		return;
	}

	if ((tstr = prompt(WIDE("Time to subtract, ([[HH:]MM:]SS):"), NULL, NULL)) == NULL)
		return;

	if (!*tstr) {
		drawstatus(WIDE(""));
		free(tstr);
		return;
	}

	if (SSCANF(tstr, WIDE("%d:%d:%d"), &h, &m, &s) != 3) {
		h = 0;
		if (SSCANF(tstr, WIDE("%d:%d"), &m, &s) != 2) {
			m = 0;
			if (SSCANF(tstr, WIDE("%d"), &s) != 1) {
				free(tstr);
				drawstatus(WIDE("Invalid time format."));
				return;
			}
		}
	}

	free(tstr);
	if (m >= 60) {
		drawstatus(WIDE("Minutes cannot be more than 59."));
		return;
	}

	if (s >= 60) {
		drawstatus(WIDE("Seconds cannot be more than 59."));
		return;
	}

	entry_account(curent);

	secs = s + m*60 + h*60*60;
	if (curent->en_secs - secs < 0) {
		drawstatus(WIDE("Remaining time cannot be less than zero."));
		return;
	}

	curent->en_secs -= secs;
	save();
}

void
kmerge()
{
entry_t	*en, *ten;
int	 nmarked = 0;
WCHAR	 pr[128];
int	 h, m, s = 0;

	if (!curent) {
		drawstatus(WIDE("No entry selected."));
		return;
	}

	/*
	 * Count number of marked entries and the summed time.
	 */
	TAILQ_FOREACH(en, &entries, en_entries) {
		if (!en->en_flags.efl_marked || en == curent)
			continue;
		nmarked++;
		s += en->en_secs;
		if (en->en_started)
			s += time(NULL) - en->en_started;
	}

	if (nmarked == 0) {
		drawstatus(WIDE("No marked entries."));
		return;
	}

	h = s / (60 * 60);
	s %= (60 * 60);
	m = s / 60;
	s %= 60;

	SNPRINTF(pr, WSIZEOF(pr), WIDE("Merge %d marked entries [%02d:%02d:%02d] into current entry?"),
			nmarked, h, m, s);
	if (!yesno(pr))
		return;

	TAILQ_FOREACH_SAFE(en, &entries, en_entries, ten) {
		if (!en->en_flags.efl_marked || en == curent)
			continue;
		curent->en_secs += en->en_secs;
		if (en->en_started) {
			entry_stop(en);
			curent->en_secs += time(NULL) - en->en_started;
		}
		TAILQ_REMOVE(&entries, en, en_entries);
		entry_free(en);
	}
	save();
}

void
khelp()
{
WINDOW		*hwin;
size_t		 nhelp = 0;
WCHAR		**help;
#define HTITLE	WIDE(" TTS keys ")
size_t		 width = 0;
size_t		 i;
INT		 c;
binding_t	*bi;
	/* Count the number of bindings */
	TAILQ_FOREACH(bi, &bindings, bi_entries)
		nhelp++;
	help = calloc(nhelp, sizeof(const WCHAR *));

	i = 0;
	TAILQ_FOREACH(bi, &bindings, bi_entries) {
	WCHAR	s[128];
		if (bi->bi_key)
			SNPRINTF(s, WSIZEOF(s), WIDE("%-10"FMT_L"s %"FMT_L"s (%"FMT_L"s)"),
				bi->bi_key->ky_name, bi->bi_func->fn_desc,
				bi->bi_func->fn_name);
		else
			SNPRINTF(s, WSIZEOF(s), WIDE("%"FMT_L"c          %"FMT_L"s (%"FMT_L"s)"),
				bi->bi_code, bi->bi_func->fn_desc, bi->bi_func->fn_name);
		help[i] = STRDUP(s);
		i++;
	}

	if (nhelp > (LINES - 6))
		nhelp = LINES - 6;

	for (i = 0; i < nhelp; i++)
		if (STRLEN(help[i]) > width)
			width = STRLEN(help[i]);

	hwin = newwin(nhelp + 4, width + 4,
			(LINES / 2) - ((nhelp + 2) / 2),
			(COLS / 2) - ((width + 2) / 2));
	wborder(hwin, 0, 0, 0, 0, 0, 0, 0, 0);

	wattron(hwin, A_REVERSE | A_BOLD);
	wmove(hwin, 0, (width / 2) - (WSIZEOF(HTITLE) - 1)/2);
	WADDSTR(hwin, HTITLE);
	wattroff(hwin, A_REVERSE | A_BOLD);

	for (i = 0; i < nhelp; i++) {
		wmove(hwin, i + 2, 2);
		WADDSTR(hwin, help[i]);
	}

	wrefresh(hwin);

	while (WGETCH(hwin, &c) == ERR
#ifdef KEY_RESIZE
				|| (c == KEY_RESIZE)
#endif
				)
		;

	delwin(hwin);

	for (i = 0; i < nhelp; i++)
		free(help[i]);
	free(help);
}

void
kmark()
{
entry_t	*next;
	if (!curent) {
		drawstatus(WIDE("No entry selected."));
		return;
	}

	curent->en_flags.efl_marked = !curent->en_flags.efl_marked;

	if (mark_advance)
		cursadvance();
}

void
kunmarkall()
{
entry_t	*en;
	TAILQ_FOREACH(en, &entries, en_entries)
		en->en_flags.efl_marked = 0;
}

void
kint()
{
time_t	 duration;
entry_t	*en;
WCHAR	*name;

	if (!running) {
		drawstatus(WIDE("No running entry."));
		return;
	}

	name = prompt(WIDE("Description:"), NULL, NULL);

	if (!name || !*name) {
		itime = 0;
		free(name);
		return;
	}

	if (itime) {
		duration = time(NULL) - itime;
	} else {
	int	 h, m, s;
		if (prduration(WIDE("Duration [HH:MM:SS]:"), &h, &m, &s) == -1)
			return;

		duration = (h * 60 * 60) + (m * 60) + s;
	}

	itime = 0;
	running->en_secs += (time(NULL) - running->en_started);
	running->en_started = time(NULL);
	running->en_secs -= duration;

	en = entry_new(name);
	en->en_created = time(NULL) - duration;
	en->en_secs = duration;
	save();

	free(name);
}

void
kmarkint()
{
	if (itime) {
		kint();
	} else {
		if (!running) {
			drawstatus(WIDE("No running entry."));
			return;
		}
		itime = time(NULL);
	}
}

void
ksearch()
{
static WCHAR	*lastsearch;
WCHAR		*term;
entry_t		*start, *cur;

	if (!curent) {
		drawstatus(WIDE("No entries."));
		return;
	}

	if ((term = prompt(WIDE("Search:"), NULL, NULL)) == NULL)
		return;

	if (!*term) {
		free(term);
		if (!lastsearch)
			return;

		term = lastsearch;
	} else {
		free(lastsearch);
		lastsearch = term;
	}

	cur = start = curent;

	for (;;) {
		cur = TAILQ_NEXT(cur, en_entries);
		if (cur == NULL) {
			drawstatus(WIDE("Search reached last entry, continuing from top."));
			cur = TAILQ_FIRST(&entries);
		}

		if (cur == start) {
			drawstatus(WIDE("No matches."));
			break;
		}

		if (STRSTR(cur->en_desc, term)) {
			curent = cur;
			if (!showinv && cur->en_flags.efl_invoiced)
				showinv = 1;
			return;
		}

	}
}

void
kexec()
{
WCHAR		*cmd;
WCHAR		**args;
command_t	*cmds;
size_t		nargs;

	if ((cmd = prompt(WIDE(":"), NULL, NULL)) == NULL || !*cmd) {
		free(cmd);
		return;
	}

	nargs = tokenise(cmd, &args);
	free(cmd);

	if (nargs == 0) {
		tokfree(&args);
		return;
	}

	if ((cmds = find_command(args[0])) == NULL) {
		drawstatus(WIDE("Unknown command."));
		tokfree(&args);
		return;
	}

	cmds->cm_hdl(nargs, args);
	tokfree(&args);
}

void
drawheader()
{
	wmove(titwin, 0, 0);
	waddstr(titwin, "TTS " TTS_VERSION " - Type '?' for help");
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
		time_t		 itime = entry_time_for_day(entry_day(en), 1),
				 ntime = entry_time_for_day(entry_day(en), 0);
		int		 hi, mi, si,
				 hn, mn, sn,
				 ht, mt, st;
		WCHAR		 hdrtime;
		WCHAR		 hdrtext[256];

			time_to_hms(itime, hi, mi, si);
			time_to_hms(ntime, hn, mn, sn);
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
			SNPRINTF(hdrtext, WSIZEOF(hdrtext), WIDE("%-30"FMT_L"s [I:%02d:%02d:%02d / "
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

entry_t *
entry_new(desc)
	const WCHAR	*desc;
{
entry_t	*en;
	if ((en = calloc(1, sizeof(*en))) == NULL)
		return NULL;

	TAILQ_INSERT_HEAD(&entries, en, en_entries);

	en->en_desc = STRDUP(desc);
	time(&en->en_created);

	return en;
}

void
entry_start(en)
	entry_t	*en;
{
	if (running)
		entry_stop(running);
	time(&en->en_started);
	running = en;
}

void
entry_stop(en)
	entry_t	*en;
{
	if (running == en)
		running = NULL;
	en->en_secs += time(NULL) - en->en_started;
	en->en_started = 0;
}

void
entry_free(en)
	entry_t	*en;
{
	if (en == running)
		entry_stop(en);
	free(en->en_desc);
}

void
entry_account(en)
	entry_t	*en;
{
	if (!en->en_started)
		return;
	en->en_secs += time(NULL) - en->en_started;
	time(&en->en_started);
}

/*
 * Return the amount of time for the day on which the timestamp .when falls.
 * If .inv is 0, sum non-invoiced entries; if 1, sum invoiced entries; if
 * -1, sum all entries.
 */
time_t
entry_time_for_day(when, inv)
	time_t	when;
{
time_t	 day = time_day(when);
time_t	 sum = 0;
entry_t	*en;
	TAILQ_FOREACH(en, &entries, en_entries) {
		if (entry_day(en) > day)
			continue;
		if (entry_day(en) < day)
			break;
		if (inv == 0 && en->en_flags.efl_invoiced)
			continue;
		if (inv == 1 && !en->en_flags.efl_invoiced)
			continue;
		sum += en->en_secs;
		if (en->en_started)
			sum += time(NULL) - en->en_started;
	}
	return sum;
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

		if (SSCANF(line, WIDE("%lu %lu %9"FMT_L"[i-] %4095"FMT_L"[^\n]\n"),
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
size_t	i;
	for (i = 0; i < sizeof(funcs) / sizeof(*funcs); i++)
		if (STRCMP(name, funcs[i].fn_name) == 0)
			return &funcs[i];
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

size_t
tokenise(str, res)
	const WCHAR	*str;
	WCHAR		***res;
{
int		 ntoks = 0;
const WCHAR	*p, *q;

	*res = NULL;
	p = str;

	for (;;) {
	ptrdiff_t	sz;

		while (ISSPACE(*p))
			p++;

		if (!*p)
			break;

		q = p;

		/* Find the next seperator */
		while (!ISSPACE(*q) && *q)
			q++;

		sz = (q - p);
		*res = realloc(*res, sizeof(WCHAR *) * (ntoks + 1));
		(*res)[ntoks] = malloc(sizeof(WCHAR) * sz + 1);
		MEMCPY((*res)[ntoks], p, sz);
		(*res)[ntoks][sz] = 0;
		ntoks++;

		while (ISSPACE(*q))
			q++;

		if (!*q)
			break;
		p = q;
	}

	*res = realloc(*res, sizeof(WCHAR *) * (ntoks + 1));
	(*res)[ntoks] = NULL;
	return ntoks;
}

void
tokfree(vec)
	WCHAR	***vec;
{
WCHAR	**p;
	for (p = (*vec); *p; p++)
		free(*p);
	free(*vec);
}

command_t *
find_command(name)
	const WCHAR	*name;
{
size_t	i;
	for (i = 0; i < sizeof(commands) / sizeof(*commands); i++)
		if (STRCMP(name, commands[i].cm_name) == 0)
			return &commands[i];
	return NULL;
}

void
c_style(argc, argv)
	size_t	argc;
	WCHAR	**argv;
{
style_t	*sy;
WCHAR	*last, *tok;

	if (argc < 3 || argc > 4) {
		cmderr(WIDE("Usage: style <item> <foreground> [background]"));
		return;
	}

	if (STRCMP(argv[1], WIDE("header")) == 0)
		sy = &sy_header;
	else if (STRCMP(argv[1], WIDE("status")) == 0)
		sy = &sy_status;
	else if (STRCMP(argv[1], WIDE("entry")) == 0)
		sy = &sy_entry;
	else if (STRCMP(argv[1], WIDE("selected")) == 0)
		sy = &sy_selected;
	else if (STRCMP(argv[1], WIDE("running")) == 0)
		sy = &sy_running;
	else if (STRCMP(argv[1], WIDE("date")) == 0)
		sy = &sy_date;
	else {
		cmderr(WIDE("Unknown style item."));
		return;
	}

	style_clear(sy);
	for (tok = STRTOK(argv[2], WIDE(","), &last); tok != NULL;
	     tok = STRTOK(NULL, WIDE(","), &last)) {
		style_add(sy, tok, argv[3]);
	}

	apply_styles();
}

void
c_bind(argc, argv)
	size_t	argc;
	WCHAR	**argv;
{
	if (argc != 3) {
		cmderr(WIDE("Usage: bind <key> <function>"));
		return;
	}

	bind_key(argv[1], argv[2]);
}

variable_t *
find_variable(name)
	WCHAR const	*name;
{
variable_t	*v;
size_t		 i;
	for (i = 0; i < sizeof(variables) / sizeof(*variables); i++)
		if (STRCMP(name, variables[i].va_name) == 0)
			return &variables[i];
	return NULL;
}

void
c_set(argc, argv)
	size_t	argc;
	WCHAR	**argv;
{
variable_t	*var;

	if (argc != 3) {
		cmderr(WIDE("Usage: set <variable> <value>"));
		return;
	}

	if ((var = find_variable(argv[1])) == NULL) {
		cmderr(WIDE("Unknown variable \"%"FMT_L"s\"."), argv[1]);
		return;
	}

	switch (var->va_type) {
	case VTYPE_BOOL: {
	int	val;
		if (STRCMP(argv[2], WIDE("true")) == 0 ||
		    STRCMP(argv[2], WIDE("yes")) == 0 ||
		    STRCMP(argv[2], WIDE("on")) == 0 ||
		    STRCMP(argv[2], WIDE("1")) == 0) {
			val = 1;
		} else if (STRCMP(argv[2], WIDE("false")) == 0 ||
			   STRCMP(argv[2], WIDE("no")) == 0 ||
			   STRCMP(argv[2], WIDE("off")) == 0 ||
			   STRCMP(argv[2], WIDE("0")) == 0) {
			val = 0;
		} else {
			cmderr(WIDE("Invalid value for boolean: \"%"FMT_L"s\"."), argv[2]);
			return;
		}

		*(int *)var->va_addr = val;
		break;
	}
	}
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

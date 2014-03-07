/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<stdlib.h>

#include	"functions.h"
#include	"tts.h"
#include	"entry.h"
#include	"ui.h"
#include	"commands.h"
#include	"tts_curses.h"
#include	"bindings.h"
#include	"str.h"

function_t funcs[] = {
	{ WIDE("help"),		khelp,		WIDE("display help screen") },
	{ WIDE("add"),		kadd,		WIDE("add a new entry and start the timer") },
	{ WIDE("add-old"),	kaddold,	WIDE("add a new entry and specify its duration") },
	{ WIDE("delete"),	kmarkdel,	WIDE("delete the current entry") },
	{ WIDE("undelete"),	kundel,		WIDE("undelete the current entry") },
	{ WIDE("quit"),		kquit,		WIDE("exit TTS") },
	{ WIDE("invoice"),	kinvoiced,	WIDE("toggle current entry as invoiced") },
	{ WIDE("billable"),	kbillable,	WIDE("toggle current entry as billable") },
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
	{ }
};

void
kquit()
{
entry_t	*en;
int	 ndel = 0;

	TTS_TAILQ_FOREACH(en, &entries, en_entries) {
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
entry_t	*en;
int	 nmarked = 0;

	TTS_TAILQ_FOREACH(en, &entries, en_entries) {
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

void
ksync()
{
entry_t	*en, *ten;

	TTS_TAILQ_FOREACH_SAFE(en, &entries, en_entries, ten) {
		if (!en->en_flags.efl_deleted)
			continue;
		if (en == curent)
			curent = NULL;
		TTS_TAILQ_REMOVE(&entries, en, en_entries);
		entry_free(en);
	}

	if (curent == NULL)
		curent = TTS_TAILQ_FIRST(&entries);
	save();
}

void
kup()
{
entry_t	*prev = curent;
	if (!curent)
		return;

	do {
		if ((prev = TTS_TAILQ_PREV(prev, entrylist, en_entries)) == NULL)
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
		if ((next = TTS_TAILQ_NEXT(next, en_entries)) == NULL)
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

	TTS_TAILQ_FOREACH(en, &entries, en_entries) {
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
		if (TTS_TAILQ_NEXT(curent, en_entries) != NULL)
			curent = TTS_TAILQ_NEXT(curent, en_entries);
		return;
	}

	/*
	 * Try to find the next uninvoiced request to move the cursor to.
	 */
	for (;;) {
		if ((curent = TTS_TAILQ_NEXT(curent, en_entries)) == NULL)
			break;	/* end of list */
		if (!curent->en_flags.efl_invoiced)
			return;
	}

	/*
	 * We didn't find any, so try searching backwards instead.
	 */
	for (curent = en;;) {
		if ((curent = TTS_TAILQ_PREV(curent, entrylist, en_entries)) == NULL)
			break;	/* end of list */
		if (!curent->en_flags.efl_invoiced)
			return;
	}
}

void
kbillable()
{
entry_t	*en;
int	 anymarked = 0;

	TTS_TAILQ_FOREACH(en, &entries, en_entries) {
		if (!en->en_flags.efl_marked)
			continue;
		anymarked = 1;
		en->en_flags.efl_nonbillable = !en->en_flags.efl_nonbillable;
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

	curent->en_flags.efl_nonbillable = !curent->en_flags.efl_nonbillable;
	save();

	if (bill_advance)
		cursadvance();
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
		curent = TTS_TAILQ_FIRST(&entries);
		return;
	}

	/*
	 * Try to find the next uninvoiced request to move the cursor to.
	 */
	for (;;) {
		if ((curent = TTS_TAILQ_NEXT(curent, en_entries)) == NULL)
			break;	/* end of list */
		if (!curent->en_flags.efl_invoiced)
			return;
	}

	/*
	 * We didn't find any, so try searching backwards instead.
	 */
	for (curent = en;;) {
		if ((curent = TTS_TAILQ_PREV(curent, entrylist, en_entries)) == NULL)
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
	TTS_TAILQ_FOREACH(en, &entries, en_entries) {
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

	TTS_TAILQ_FOREACH_SAFE(en, &entries, en_entries, ten) {
		if (!en->en_flags.efl_marked || en == curent)
			continue;
		if (en->en_started)
			entry_stop(en);
		curent->en_secs += en->en_secs;
		TTS_TAILQ_REMOVE(&entries, en, en_entries);
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
	TTS_TAILQ_FOREACH(bi, &bindings, bi_entries)
		nhelp++;
	help = calloc(nhelp, sizeof(const WCHAR *));

	i = 0;
	TTS_TAILQ_FOREACH(bi, &bindings, bi_entries) {
	WCHAR	s[128], t[16];

		if (bi->bi_key)
			SNPRINTF(t, WSIZEOF(t), WIDE("%"FMT_L"s"), bi->bi_key->ky_name);
		else
			SNPRINTF(t, WSIZEOF(t), WIDE("%"FMT_L"c"), bi->bi_code);

		if (bi->bi_macro)
			SNPRINTF(s, WSIZEOF(s), WIDE("%-10"FMT_L"s execute macro: %"FMT_L"s"),
				t, bi->bi_macro);
		else
			SNPRINTF(s, WSIZEOF(s), WIDE("%-10"FMT_L"s %"FMT_L"s (%"FMT_L"s)"),
				t, bi->bi_func->fn_desc, bi->bi_func->fn_name);

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
	TTS_TAILQ_FOREACH(en, &entries, en_entries)
		en->en_flags.efl_marked = 0;
}

void
kint()
{
time_t	 duration;
entry_t	*en;
WCHAR	*name;

	if (!itime) {
		if (!running) {
			drawstatus(WIDE("No running entry."));
			return;
		}

		itime = time(NULL);
		return;
	}

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
		cur = TTS_TAILQ_NEXT(cur, en_entries);
		if (cur == NULL) {
			drawstatus(WIDE("Search reached last entry, continuing from top."));
			cur = TTS_TAILQ_FIRST(&entries);
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


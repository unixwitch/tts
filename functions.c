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
	{ L"help",		khelp,		L"display help screen" },
	{ L"add",		kadd,		L"add a new entry and start the timer" },
	{ L"add-old",		kaddold,	L"add a new entry and specify its duration" },
	{ L"delete",		kmarkdel,	L"delete the current entry" },
	{ L"undelete",		kundel,		L"undelete the current entry" },
	{ L"quit",		kquit,		L"exit TTS" },
	{ L"invoice",		kinvoiced,	L"toggle current entry as invoiced" },
	{ L"billable",		kbillable,	L"toggle current entry as billable" },
	{ L"mark",		kmark,		L"mark the current entry" },
	{ L"unmarkall",		kunmarkall,	L"unmark all entries" },
	{ L"startstop",		ktoggle,	L"start or stop the timer" },
	{ L"edit-desc",		keddesc,	L"edit the current entry's description" },
	{ L"edit-time",		kedtime,	L"edit the current entry's duration" },
	{ L"showhide-inv",	ktoggleinv,	L"show or hide invoiced entries" },
	{ L"copy",		kcopy,		L"copy the current entry's description to a new entry" },
	{ L"add-time",		kaddtime,	L"add time to the current entry" },
	{ L"sub-time",		kdeltime,	L"subtract time from the current entry" },
	{ L"search",		ksearch,	L"search for an entry by name" },
	{ L"sync",		ksync,		L"purge all deleted entries" },
	{ L"prev",		kup,		L"move to the previous entry" },
	{ L"next",		kdown,		L"move to the next entry" },
	{ L"execute",		kexec,		L"execute a configuration command" },
	{ L"merge",		kmerge,		L"merge marked entries into current entry" },
	{ L"interrupt",		kint,		L"split current entry into new entry"},
	{ }
};

static int
valid_description(d)
	const wchar_t *d;
{
	if (!d)
		return 0;

	while (iswspace(*d))
		d++;

	if (!*d)
		return 0;

	return 1;
}

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
	wchar_t	s[128];
		swprintf(s, wsizeof(s), L"Purge %d deleted entries?", ndel);
		if (yesno(s)) {
			ksync();
		}
	}

	doexit = 1;
}

void
kadd()
{
wchar_t	*name;
entry_t	*en;
	name = prompt(L"Description:", NULL, NULL);

	if (!valid_description(name)) {
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
wchar_t	*name;
entry_t	*en;
	name = prompt(L"Description:", NULL, NULL);

	if (!valid_description(name)) {
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
		drawstatus(L"No entries to delete.");
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
		drawstatus(L"Already at first entry.");
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
		drawstatus(L"Already at last entry.");
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
		drawstatus(L"No entry selected.");
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
		drawstatus(L"No entry selected.");
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
wchar_t	*new;

	if (!curent) {
		drawstatus(L"No entry selected.");
		return;
	}

	if ((new = prompt(L"Description:", curent->en_desc, NULL)) == NULL)
		return;

	free(curent->en_desc);
	curent->en_desc = new;
	save();
}

void
kedtime()
{
time_t	 n;

	if (!curent) {
		drawstatus(L"No entry selected.");
		return;
	}

	n = curent->en_secs;
	if (curent->en_started)
		n += time(NULL) - curent->en_started;

	if ((n = prduration(L"Duration:", n)) == (time_t) -1) {
		drawstatus(L"Invalid duration.");
		return;
	}

	curent->en_secs = n;
	if (curent->en_started)
		time(&curent->en_started);

	save();
}

void
ktoggleinv()
{
entry_t	*en = curent;
	showinv = !showinv;
	drawstatus(L"%ls invoiced entries.", showinv ? L"Showing" : L"Hiding");

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
		drawstatus(L"No entry selected.");
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
time_t	 secs;
	if (!curent) {
		drawstatus(L"No entry selected.");
		return;
	}

	if ((secs = prduration(L"Time to add:", 0)) == (time_t) -1) {
		drawstatus(L"Invalid time format.");
		return;
	}

	curent->en_secs += secs;
	save();
}

void
kdeltime()
{
time_t	 secs;
	if (!curent) {
		drawstatus(L"No entry selected.");
		return;
	}

	if ((secs = prduration(L"Time to subtract:", 0)) == (time_t) -1) {
		drawstatus(L"Invalid time format.");
		return;
	}

	entry_account(curent);

	if (curent->en_secs - secs < 0) {
		drawstatus(L"Remaining time cannot be less than zero.");
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
wchar_t	 pr[128];
wchar_t	*ct;
int	 s = 0;

	if (!curent) {
		drawstatus(L"No entry selected.");
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
		drawstatus(L"No marked entries.");
		return;
	}

	ct = maketime(s, time_format);
	swprintf(pr, wsizeof(pr), L"Merge %d marked entries [%ls] into current entry",
			nmarked, ct);
	free(ct);

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
wchar_t		**help;
#define HTITLE	L" TTS keys "
size_t		 width = 0;
size_t		 i;
wint_t		 c;
binding_t	*bi;

	/* Count the number of bindings */
	TTS_TAILQ_FOREACH(bi, &bindings, bi_entries)
		nhelp++;
	help = calloc(nhelp, sizeof(const wchar_t *));

	i = 0;
	TTS_TAILQ_FOREACH(bi, &bindings, bi_entries) {
	wchar_t	s[128], t[16];

		if (bi->bi_key)
			swprintf(t, wsizeof(t), L"%ls", bi->bi_key->ky_name);
		else
			swprintf(t, wsizeof(t), L"%lc", bi->bi_code);

		if (bi->bi_macro) {
		wchar_t	*esc = escstr(bi->bi_macro);
			swprintf(s, wsizeof(s), L"%-10ls execute macro: %ls",
				t, esc);
			free(esc);
		} else
			swprintf(s, wsizeof(s), L"%-10ls %ls (%ls)",
				t, bi->bi_func->fn_desc, bi->bi_func->fn_name);

		help[i] = wcsdup(s);
		i++;
	}

	if (nhelp > (LINES - 6))
		nhelp = LINES - 6;

	for (i = 0; i < nhelp; i++)
		if (wcslen(help[i]) > width)
			width = wcslen(help[i]);

	hwin = newwin(nhelp + 4, width + 4,
			(LINES / 2) - ((nhelp + 2) / 2),
			(COLS / 2) - ((width + 2) / 2));
	wborder(hwin, 0, 0, 0, 0, 0, 0, 0, 0);

	wattron(hwin, A_REVERSE | A_BOLD);
	wmove(hwin, 0, (width / 2) - (wsizeof(HTITLE) - 1)/2);
	waddwstr(hwin, HTITLE);
	wattroff(hwin, A_REVERSE | A_BOLD);

	for (i = 0; i < nhelp; i++) {
		wmove(hwin, i + 2, 2);
		waddwstr(hwin, help[i]);
	}

	wrefresh(hwin);

	while (wget_wch(hwin, &c) == ERR
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
		drawstatus(L"No entry selected.");
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
wchar_t	*name;

	if (!itime) {
		if (!running) {
			drawstatus(L"No running entry.");
			return;
		}

		itime = time(NULL);
		return;
	}

	if (!running) {
		drawstatus(L"No running entry.");
		return;
	}

	name = prompt(L"Description (<ENTER> to abandon interrupt):", NULL, NULL);

	if (!valid_description(name)) {
		itime = 0;
		free(name);
		return;
	}

	duration = time(NULL) - itime;

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
static wchar_t	*lastsearch;
wchar_t		*term;
entry_t		*start, *cur;

	if (!curent) {
		drawstatus(L"No entries.");
		return;
	}

	if ((term = prompt(L"Search:", NULL, NULL)) == NULL)
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
			drawstatus(L"Search reached last entry, continuing from top.");
			cur = TTS_TAILQ_FIRST(&entries);
		}

		if (cur == start) {
			drawstatus(L"No matches.");
			break;
		}

		if (wcsstr(cur->en_desc, term)) {
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
wchar_t		*cmd;
wchar_t		**args;
command_t	*cmds;
size_t		nargs;

	if ((cmd = prompt(L":", NULL, NULL)) == NULL || !*cmd) {
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
		drawstatus(L"Unknown command.");
		tokfree(&args);
		return;
	}

	cmds->cm_hdl(nargs, args);
	tokfree(&args);
}

/*
 * Return the function_t for the function called .name, or NULL if such a
 * function doesn't exist.
 */
function_t *
find_func(name)
	const wchar_t	*name;
{
function_t	*f;
	for (f = funcs; f->fn_name; f++)
		if (wcscmp(name, f->fn_name) == 0)
			return f;
	return NULL;
}


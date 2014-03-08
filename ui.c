/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<string.h>
#include	<stdlib.h>

#include	"ui.h"
#include	"tts.h"
#include	"style.h"

WINDOW *titwin, *statwin, *listwin;
int in_curses;

void
ui_init(void)
{
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

	curs_set(0);
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
		curent = TTS_TAILQ_FIRST(&entries);
		return;
	}

	/*
	 * Try to find the next suitable entry to move the cursor to.
	 */
	for (en = TTS_TAILQ_NEXT(curent, en_entries); en; en = TTS_TAILQ_NEXT(en, en_entries)) {
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

	for (en = TTS_TAILQ_PREV(curent, entrylist, en_entries); en;
	     en = TTS_TAILQ_PREV(en, entrylist, en_entries)) {
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
drawheader()
{
wchar_t	title[64];

	swprintf(title, wsizeof(title), L"TTS %s - Type '?' for help",
		 tts_version);
	wmove(titwin, 0, 0);
	waddwstr(titwin, title);

	if (itime > 0) {
	wchar_t	str[128];
	int	h, m, s;
	time_t	passed = time(NULL) - itime;

		time_to_hms(passed, h, m, s);
		swprintf(str, wsizeof(str), L"  *** MARK INTERRUPT: %02d:%02d:%02d ***",
			h, m, s);

		wattron(titwin, A_BOLD);
		waddwstr(titwin, str);
		wattroff(titwin, A_BOLD);
	}
	wclrtoeol(titwin);
	wrefresh(titwin);
}

void
vdrawstatus(msg, ap)
	const wchar_t	*msg;
	va_list		 ap;
{
wchar_t	s[1024];
	vswprintf(s, wsizeof(s), msg, ap);

	wmove(statwin, 0, 0);
	waddwstr(statwin, s);
	wclrtoeol(statwin);
	wrefresh(statwin);
	time(&laststatus);
}

void
drawstatus(const wchar_t *msg, ...)
{
va_list	ap;
	va_start(ap, msg);
	vdrawstatus(msg, ap);
	va_end(ap);
}

int
yesno(msg)
	const wchar_t	*msg;
{
WINDOW	*pwin;
wint_t	 c;

	pwin = newwin(1, COLS, LINES - 1, 0);
	keypad(pwin, TRUE);

	wattron(pwin, A_BOLD);
	wattr_on(pwin, style_fg(sy_status), NULL);
	wbkgd(pwin, style_bg(sy_status));

	wmove(pwin, 0, 0);
	waddwstr(pwin, msg);
	waddwstr(pwin, L" [y/N]? ");
	wattroff(pwin, A_BOLD);

	while (input_char(&c) == ERR
#ifdef KEY_RESIZE
			|| (c == KEY_RESIZE)
#endif
			)
		;

	delwin(pwin);
	wtouchln(statwin, 0, 1, 1);
	wrefresh(statwin);

	return (c == 'Y' || c == 'y') ? 1 : 0;
}

wchar_t *
prompt(msg, def, hist)
	const wchar_t	*msg, *def;
	history_t	*hist;
{
WINDOW		*pwin;
wchar_t		 input[256];
size_t		 pos = 0;
histent_t	*histpos = NULL;

	if (hist == NULL)
		hist = prompthist;

	memset(input, 0, sizeof(input));
	if (def) {
		wcsncpy(input, def, wsizeof(input) - 1);
		pos = wcslen(input);
	}

	pwin = newwin(1, COLS, LINES - 1, 0);
	keypad(pwin, TRUE);

	wattr_on(pwin, style_fg(sy_status), NULL);
	wbkgd(pwin, style_bg(sy_status));

	wattron(pwin, A_BOLD);
	wmove(pwin, 0, 0);
	waddwstr(pwin, msg);
	wattroff(pwin, A_BOLD);

	curs_set(1);

	for (;;) {
	wint_t	c;
		wmove(pwin, 0, wcslen(msg) + 1);
		waddwstr(pwin, input);
		wclrtoeol(pwin);
		wmove(pwin, 0, wcslen(msg) + 1 + pos);
		wrefresh(pwin);

		if (input_char(&c) == ERR)
			continue;

		switch (c) {
		case '\n':
		case '\r':
			goto end;

		case KEY_BACKSPACE:
		case 0x7F:
		case 0x08:
			if (pos) {
				if (pos == wcslen(input))
					input[--pos] = 0;
				else {
				int	i = wcslen(input);
					pos--;
					wmemcpy(input + pos, input + pos + 1, wcslen(input) - pos);
					input[i] = 0;
				}
			}
			break;

		case KEY_DC:
			if (pos < wcslen(input)) {
			int	i = wcslen(input);
				wmemcpy(input + pos, input + pos + 1, wcslen(input) - pos);
				input[i] = 0;
			}
			break;

		case KEY_LEFT:
			if (pos)
				pos--;
			break;

		case KEY_RIGHT:
			if (pos < wcslen(input))
				pos++;
			break;

		case KEY_HOME:
		case 0x01:	/* ^A */
			pos = 0;
			break;

		case KEY_END:
		case 0x05:	/* ^E */
			pos = wcslen(input);
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
				if ((histpos = TTS_TAILQ_LAST(&hist->hi_ents, hentlist)) == NULL) {
					beep();
					break;
				}
			} else {
				if (TTS_TAILQ_PREV(histpos, hentlist, he_entries) == NULL) {
					beep();
					break;
				} else
					histpos = TTS_TAILQ_PREV(histpos, hentlist, he_entries);
			}


			wcsncpy(input, histpos->he_text, wsizeof(input) - 1);
			pos = wcslen(input);
			break;

		case KEY_DOWN:
			if (histpos == NULL) {
				beep();
				break;
			}

			if (TTS_TAILQ_NEXT(histpos, he_entries) == NULL) {
				beep();
				break;
			} else
				histpos = TTS_TAILQ_NEXT(histpos, he_entries);


			wcsncpy(input, histpos->he_text, wsizeof(input) - 1);
			pos = wcslen(input);
			break;

		default:
			if (pos != wcslen(input)) {
				wmemmove(input + pos + 1, input + pos, wcslen(input) - pos);
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
	wtouchln(statwin, 0, 1, 1);
	wrefresh(statwin);
	hist_add(hist, input);
	return wcsdup(input);
}

void
errbox(const wchar_t *msg, ...)
{
va_list	ap;
	va_start(ap, msg);
	verrbox(msg, ap);
	va_end(ap);
}

void
verrbox(msg, ap)
	const wchar_t	*msg;
	va_list		 ap;
{
wchar_t	 text[4096];
WINDOW	*ewin;

#define ETITLE	L" Error "
#define ECONT	L" <OK> "
int	width;
wint_t	c;

	vswprintf(text, wsizeof(text), msg, ap);
	width = wcslen(text);

	ewin = newwin(6, width + 4,
			(LINES / 2) - ((1 + 2)/ 2),
			(COLS / 2) - ((width + 2) / 2));
	leaveok(ewin, TRUE);
	wborder(ewin, 0, 0, 0, 0, 0, 0, 0, 0);

	wattron(ewin, A_REVERSE | A_BOLD);
	wmove(ewin, 0, (width / 2) - (wsizeof(ETITLE) - 1)/2);
	waddwstr(ewin, ETITLE);
	wattroff(ewin, A_REVERSE | A_BOLD);

	wmove(ewin, 2, 2);
	waddwstr(ewin, text);
	wattron(ewin, A_REVERSE | A_BOLD);
	wmove(ewin, 4, (width / 2) - ((wsizeof(ECONT) - 1) / 2));
	waddwstr(ewin, ECONT);
	wattroff(ewin, A_REVERSE | A_BOLD);

	for (;;) {
		if (wget_wch(ewin, &c) == ERR)
			continue;
		if (c == '\r')
			break;
	}

	delwin(ewin);
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

	TTS_TAILQ_FOREACH(en, &entries, en_entries)
		en->en_flags.efl_visible = 0;

	en = TTS_TAILQ_FIRST(&entries);
	for (i = 0; i < pagestart; i++)
		if ((en = TTS_TAILQ_NEXT(en, en_entries)) == NULL)
			return;

	for (; en; en = TTS_TAILQ_NEXT(en, en_entries)) {
	time_t	 n;
	int	 h, s, m;
	wchar_t	 flags[10], stime[16], *p;
	attr_t	 attrs = WA_NORMAL;

		if (!showinv && en->en_flags.efl_invoiced)
			continue;

		oldbg = getbkgd(listwin);

		if (lastday != entry_day(en)) {
		struct tm	*lt;
		wchar_t		 lbl[128];
		time_t		 itime = entry_time_for_day(entry_day(en), 1, 0),
				 ntime = entry_time_for_day(entry_day(en), 0, 0),
				 btime = entry_time_for_day(entry_day(en), 2, bill_increment);
		int		 hi, mi, si,
				 hn, mn, sn,
				 hb, mb, sb,
				 ht, mt, st;
		wchar_t		 hdrtext[256];

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

			wcsftime(lbl, wsizeof(lbl), L"%A, %d %B %Y", lt);
			if (show_billable)
				swprintf(hdrtext, wsizeof(hdrtext),
					 L"%-30ls [I:%02d:%02d:%02d / "
					 L"N:%02d:%02d:%02d / T:%02d:%02d:%02d / "
					 L"B:%02d:%02d:%02d]",
					 lbl, hi, mi, si, hn, mn, sn, ht, mt, st,
					 hb, mb, sb);
			else
				swprintf(hdrtext, wsizeof(hdrtext),
					 L"%-30ls [I:%02d:%02d:%02d / "
					 L"N:%02d:%02d:%02d / T:%02d:%02d:%02d]",
					 lbl, hi, mi, si, hn, mn, sn, ht, mt, st);

			wattr_on(listwin, style_fg(sy_date), NULL);
			wbkgdset(listwin, style_bg(sy_date));
			wmove(listwin, cline, 0);
			waddwstr(listwin, hdrtext);
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
			waddwstr(listwin, L"  -> ");
		} else
			waddwstr(listwin, L"     ");

		n = en->en_secs;
		if (en->en_started)
			n += time(NULL) - en->en_started;
		h = n / (60 * 60);
		n %= (60 * 60);
		m = n / 60;
		n %= 60;
		s = n;

		swprintf(stime, wsizeof(stime), L"%02d:%02d:%02d%c ",
			h, m, s, (itime && (en == running)) ? '*' : ' ');
		waddwstr(listwin, stime);

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
		wchar_t	s[10];
			swprintf(s, wsizeof(s), L"%-5ls  ", flags);
			waddwstr(listwin, s);
		} else
			waddwstr(listwin, L"       ");

		waddwstr(listwin, en->en_desc);
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
prduration(pr, hh, mm, ss)
	wchar_t	*pr;
	int	*hh, *mm, *ss;
{
wchar_t	*tstr;
int	 h, m, s;
	if ((tstr = prompt(pr, L"00:00:00", NULL)) == NULL)
		return -1;

	if (!*tstr) {
		drawstatus(L"No duration entered");
		free(tstr);
		return -1;
	}

	if (swscanf(tstr, L"%d:%d:%d", &h, &m, &s) != 3) {
		h = 0;
		if (swscanf(tstr, L"%d:%d", &m, &s) != 2) {
			m = 0;
			if (swscanf(tstr, L"%d", &s) != 1) {
				free(tstr);
				drawstatus(L"Invalid time format.");
				return -1;
			}
		}
	}

	free(tstr);

	if (m >= 60) {
		drawstatus(L"Minutes cannot be more than 59.");
		return -1;
	}

	if (s >= 60) {
		drawstatus(L"Seconds cannot be more than 59.");
		return -1;
	}

	*hh = h;
	*mm = m;
	*ss = s;
	return 0;
}


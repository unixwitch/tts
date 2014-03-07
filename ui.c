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

#include	"ui.h"
#include	"tts.h"
#include	"style.h"

WINDOW *titwin, *statwin, *listwin;
int in_curses;

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


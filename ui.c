/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	"ui.h"
#include	"tts.h"

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


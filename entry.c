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
#include	<string.h>
#include	<time.h>
#include	<math.h>

#include	"entry.h"
#include	"wide.h"
#include	"tts.h"

entry_list entries = TAILQ_HEAD_INITIALIZER(entries);

entry_t *running;

entry_t *
entry_new(desc)
	const WCHAR	*desc;
{
entry_t	*en;
	if ((en = calloc(1, sizeof(*en))) == NULL)
		return NULL;

	if (auto_nonbillable && STRSTR(desc, auto_nonbillable))
		en->en_flags.efl_nonbillable = 1;

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
 * 2, sum billable entries; if -1, sum all entries.  If .incr is non-zero,
 * individual entry time will be rounded up to intervals of that many minutes.
 */
time_t
entry_time_for_day(when, inv, incr)
	time_t	when;
{
time_t	 day = time_day(when);
time_t	 sum = 0;
entry_t	*en;
int	 rnd = incr * 60;
	TAILQ_FOREACH(en, &entries, en_entries) {
	time_t	n;

		if (entry_day(en) > day)
			continue;
		if (entry_day(en) < day)
			break;

		if (inv == 0 && en->en_flags.efl_invoiced)
			continue;
		if (inv == 1 && !en->en_flags.efl_invoiced)
			continue;
		if (inv == 2 && en->en_flags.efl_nonbillable)
			continue;

		n = en->en_secs;
		if (en->en_started)
			n += time(NULL) - en->en_started;

		if (!n)
			continue;

		if (rnd)
			n = (1 + round((n - 1) / rnd)) * rnd;
		sum += n;
	}
	return sum;
}


/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_H
#define	TTS_H

#include	<signal.h>

#include	"wide.h"
#include	"tailq.h"
#include	"entry.h"

extern char const *tts_version;
extern time_t laststatus;

/*
 * Configuration options.
 */

extern int show_billable;
extern int delete_advance;
extern int mark_advance;
extern int bill_advance;
extern int bill_increment;
extern wchar_t *auto_nonbillable;

/*
 * Global state.
 */

extern entry_t *curent;
extern volatile sig_atomic_t doexit;
extern int pagestart;
extern time_t itime;

int load(void);
int save(void);

/*
 * Command history.
 */

#define NHIST 50
typedef struct histent {
	wchar_t			*he_text;
	TTS_TAILQ_ENTRY(histent)	 he_entries;
} histent_t;
typedef TTS_TAILQ_HEAD(hentlist, histent) hentlist_t;

typedef struct history {
	int		hi_nents;
	hentlist_t	hi_ents;
} history_t;

history_t	*hist_new(void);
void		 hist_add(history_t *, wchar_t const *);

extern history_t *searchhist;
extern history_t *prompthist;

#ifndef HAVE_WCSLCPY
size_t wcslcat(wchar_t *s1, const wchar_t *s2, size_t n);
#endif

#ifndef HAVE_WCSLCAT
size_t wcslcpy(wchar_t *s1, const wchar_t *s2, size_t n);
#endif

#endif	/* !TTS_H */

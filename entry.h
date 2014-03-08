/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_ENTRY_H
#define	TTS_ENTRY_H

#include	<time.h>

#include	"tailq.h"
#include	"wide.h"

typedef struct entry {
	wchar_t			*en_desc;
	int			 en_secs;
	time_t			 en_started;
	time_t			 en_created;

	struct {
		int	efl_visible:1;
		int	efl_invoiced:1;
		int	efl_marked:1;
		int	efl_deleted:1;
		int	efl_nonbillable:1;
	}			 en_flags;
	TTS_TAILQ_ENTRY(entry)	 en_entries;
} entry_t;

typedef TTS_TAILQ_HEAD(entrylist, entry) entry_list;
extern entry_list entries;

extern entry_t *running;

entry_t	*entry_new		(const wchar_t *);
void	 entry_start		(entry_t *);
void	 entry_stop		(entry_t *);
void	 entry_free		(entry_t *);
void	 entry_account		(entry_t *);
time_t	 entry_time_for_day	(time_t, int, int);

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

#endif	/* !TTS_ENTRY_H */

/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_STYLE_H
#define	TTS_STYLE_H

#include	"tts_curses.h"

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

typedef struct colour {
	const WCHAR	*co_name;
	short		 co_value;
} colour_t;

extern style_t	sy_header,
		sy_status,
		sy_entry,
		sy_running,
		sy_selected,
		sy_date;

int attr_find(const WCHAR *name, attr_t *result);
int colour_find(const WCHAR *name, short *result);

void style_clear(style_t *);
int style_set(style_t *, const WCHAR *fg, const WCHAR *bg);
int style_add(style_t *, const WCHAR *fg, const WCHAR *bg);

void apply_styles(void);

#endif	/* !TTS_STYLE_H */

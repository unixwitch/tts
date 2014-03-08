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
#include	"wide.h"

typedef struct style {
	short	sy_pair;
	attr_t	sy_attrs;
} style_t;

#define style_fg(s)	(COLOR_PAIR((s).sy_pair) | (s).sy_attrs)
#define style_bg(s)	((wint_t) ' ' | COLOR_PAIR((s).sy_pair) | ((s).sy_attrs & ~WA_UNDERLINE))

typedef struct attrname {
	const wchar_t	*an_name;
	attr_t		 an_value;
} attrname_t;

typedef struct colour {
	const wchar_t	*co_name;
	short		 co_value;
} colour_t;

extern style_t	sy_header,
		sy_status,
		sy_entry,
		sy_running,
		sy_selected,
		sy_date;

extern short default_fg, default_bg;

int	attr_find	(const wchar_t *name, attr_t *result);
int	colour_find	(const wchar_t *name, short *result);

void	style_clear	(style_t *);
int	style_set	(style_t *, const wchar_t *fg, const wchar_t *bg);
int	style_add	(style_t *, const wchar_t *fg, const wchar_t *bg);

void	style_defaults	(void);
void	apply_styles	(void);

#endif	/* !TTS_STYLE_H */

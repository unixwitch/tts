/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_WIDE_H
#define	TTS_WIDE_H

#include	<wchar.h>

#include	"config.h"
#include	"tts_curses.h"

#define	wsizeof(s)	(sizeof(s) / sizeof(wchar_t))

int	input_char	(WINDOW *, wint_t *);
void	input_macro	(wchar_t *);

#endif	/* !TTS_WIDE_H */

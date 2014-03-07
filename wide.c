/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	"wide.h"
#include	"tts_curses.h"

#ifdef	NEED_TTS_WGETCH
static int
tts_wgetch(win, d)
	WINDOW	*win;
	int	*d;
{
int	c;
	if ((c = wgetch(win)) == ERR)
		return ERR;
	*d = c;
	return OK;
}
#endif	/* !NEED_TTS_WGETCH */

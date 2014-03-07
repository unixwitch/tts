/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_CURSES_H
#define	TTS_CURSES_H

#include	"config.h"

#if defined HAVE_NCURSESW_CURSES_H
#	include <ncursesw/curses.h>
#elif defined HAVE_NCURSESW_H
#	include <ncursesw.h>
#elif defined HAVE_NCURSES_CURSES_H
#	include <ncurses/curses.h>
#elif defined HAVE_NCURSES_H
#	include <ncurses.h>
#elif defined HAVE_CURSES_H
#	include <curses.h>
#else
#	error "SVR4 or XSI compatible curses header file required"
#endif

#endif	/* !TTS_CURSES_H */

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

#include	"config.h"
#include	"tts_curses.h"

#ifdef	HAVE_CURSES_ENHANCED
#	include <wchar.h>

#	define	WPFX(x)		wcs##x
#	define	WIDE(x)		L##x
#	define	ISX(x)		isw##x
#	define	WCHAR		wchar_t
#	define	FMT_L		"l"
#	define	SNPRINTF	swprintf
#	define	VSNPRINTF	vswprintf
#	define	SSCANF		swscanf
#	define	MEMCPY		wmemcpy
#	define	MEMMOVE		wmemmove
#	define	MBSTOWCS	mbstowcs
#	define	WCSTOMBS	wcstombs
#	define	FPRINTF		fwprintf
#	define	STRTOK		wcstok

#	define	GETCH		get_wch
#	define	WGETCH		wget_wch
#	define	ADDSTR		addwstr
#	define	WADDSTR		waddwstr
#	define	INT		wint_t
#else
#	define	WPFX(x)		str##x
#	define	WIDE(x)		x
#	define	ISX(x)		is##x
#	define	WCHAR		char
#	define	FMT_L
#	define	SNPRINTF	snprintf
#	define	VSNPRINTF	vsnprintf
#	define	SSCANF		sscanf
#	define	MEMCPY		memcpy
#	define	MEMMOVE		memmove
#	define	MBSTOWCS	strncpy
#	define	WCSTOMBS	strncpy
#	define	FPRINTF		fprintf
#	define	STRTOK		strtok_r

#	define	ADDSTR		addstr
#	define	WADDSTR		waddstr
#	define	INT		int

#	define	NEED_TTS_WGETCH
extern int tts_wgetch(WINDOW *, int *);

#	define	WGETCH		tts_wgetch
#	define	GETCH(c)	tts_wgetch(stdscr,c)
#endif

#define STRLEN		WPFX(len)
#define STRCMP		WPFX(cmp)
#define STRNCMP		WPFX(ncmp)
#define STRCPY		WPFX(cpy)
#define STRNCPY		WPFX(ncpy)
#define STRSTR		WPFX(str)
#define STRFTIME	WPFX(ftime)
#define STRDUP		WPFX(dup)
#define	STRTOL		WPFX(tol)

#define	ISSPACE		ISX(space)

#define	WSIZEOF(s)	(sizeof(s) / sizeof(WCHAR))

int	input_char(WCHAR *);
void	input_macro(WCHAR *);

#endif	/* !TTS_WIDE_H */

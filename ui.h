/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_UI_H
#define	TTS_UI_H

#include	<stdarg.h>

#include	"tts.h"
#include	"tts_curses.h"
#include	"wide.h"

extern WINDOW *titwin, *statwin, *listwin;
extern int in_curses;
extern int showinv;

void	ui_init		(void);

void	cursadvance	(void);
void	drawstatus	(const wchar_t *msg, ...);
void	vdrawstatus	(const wchar_t *msg, va_list);
void	drawheader	(void);
void	drawentries	(void);

wchar_t	*prompt		(wchar_t const *, wchar_t const *, history_t *);
time_t	 prduration	(wchar_t *prompt, time_t def);
int	 yesno		(wchar_t const *);
void	 errbox		(wchar_t const *, ...);
void	 verrbox	(wchar_t const *, va_list);

#endif	/* !TTS_UI_H */

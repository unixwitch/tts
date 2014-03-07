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

void cursadvance(void);

void drawstatus		(const WCHAR *msg, ...);
void vdrawstatus	(const WCHAR *msg, va_list);
void drawheader		(void);
void drawentries	(void);

WCHAR *prompt	(WCHAR const *, WCHAR const *, history_t *);
int prduration	(WCHAR *prompt, int *h, int *m, int *s);
int yesno	(WCHAR const *);
void errbox	(WCHAR const *, ...);
void verrbox	(WCHAR const *, va_list);

#endif	/* !TTS_UI_H */

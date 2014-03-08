/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_STR_H
#define	TTS_STR_H

#include	<stdlib.h>

#include	"wide.h"

#define	TIME_HMS	0 /* HH:MM:SS	*/
#define	TIME_HM		1 /* HH:MM	*/
#define	TIME_AHMS	2 /* 1h10m37s	*/
#define	TIME_AHM	3 /* 1h10w	*/

#define TIMEFMT_FOR_EDIT(f)			\
		(  (f) == TIME_HMS  ? TIME_HMS	\
		 : (f) == TIME_HM   ? TIME_HMS	\
		 : (f) == TIME_AHMS ? TIME_AHMS	\
		 : (f) == TIME_AHM  ? TIME_AHMS	\
		 : 0)

size_t	 tokenise	(const wchar_t *, wchar_t ***result);
void	 tokfree	(wchar_t ***);
time_t	 parsetime	(const wchar_t *);
wchar_t	*maketime	(time_t, int format);
wchar_t	*escstr		(const wchar_t *);

#endif	/* !TTS_STR_H */

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

size_t tokenise(const wchar_t *, wchar_t ***result);
void tokfree(wchar_t ***);

#endif	/* !TTS_STR_H */

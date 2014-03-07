/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_COMMANDS_H
#define	TTS_COMMANDS_H

#include	<stdarg.h>

#include	"wide.h"

typedef struct command {
	const WCHAR	*cm_name;
	void		(*cm_hdl) (size_t, WCHAR **);
} command_t;

command_t *find_command(const WCHAR *);

void c_bind	(size_t, WCHAR **);
void c_style	(size_t, WCHAR **);
void c_set	(size_t, WCHAR **);
void c_macro	(size_t, WCHAR **);

void cmderr	(const WCHAR *, ...);
void vcmderr	(const WCHAR *, va_list);

#endif	/* !TTS_COMMANDS_H */

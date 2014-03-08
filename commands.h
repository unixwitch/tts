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
	const wchar_t	*cm_name;
	void		(*cm_hdl) (size_t, wchar_t **);
} command_t;

command_t *find_command(const wchar_t *);

void c_bind	(size_t, wchar_t **);
void c_style	(size_t, wchar_t **);
void c_set	(size_t, wchar_t **);
void c_macro	(size_t, wchar_t **);

void cmderr	(const wchar_t *, ...);
void vcmderr	(const wchar_t *, va_list);

#endif	/* !TTS_COMMANDS_H */

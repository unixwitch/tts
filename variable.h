/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_VARIABLE_H
#define	TTS_VARIABLE_H

#include	"wide.h"

typedef struct variable {
	wchar_t const	*va_name;
	int		 va_type;
	void		*va_addr;
} variable_t;

#define VTYPE_INT	1
#define VTYPE_BOOL	2
#define VTYPE_STRING	3

variable_t	*find_variable(const wchar_t *name);

#endif	/* !TTS_VARIABLE_H */

/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<stdlib.h>

#include	"commands.h"
#include	"style.h"
#include	"bindings.h"
#include	"variable.h"

static command_t commands[] = {
	{ L"bind",	c_bind	},
	{ L"macro",	c_macro	},
	{ L"style",	c_style	},
	{ L"set",	c_set },
	{ }
};

command_t *
find_command(name)
	const wchar_t	*name;
{
command_t	*c;
	for (c = commands; c->cm_name; c++)
		if (wcscmp(name, c->cm_name) == 0)
			return c;
	return NULL;
}

void
c_style(argc, argv)
	size_t	argc;
	wchar_t	**argv;
{
style_t	*sy;
wchar_t	*last, *tok;

	if (argc < 3 || argc > 4) {
		cmderr(L"Usage: style <item> <foreground> [background]");
		return;
	}

	if (wcscmp(argv[1], L"header") == 0)
		sy = &sy_header;
	else if (wcscmp(argv[1], L"status") == 0)
		sy = &sy_status;
	else if (wcscmp(argv[1], L"entry") == 0)
		sy = &sy_entry;
	else if (wcscmp(argv[1], L"selected") == 0)
		sy = &sy_selected;
	else if (wcscmp(argv[1], L"running") == 0)
		sy = &sy_running;
	else if (wcscmp(argv[1], L"date") == 0)
		sy = &sy_date;
	else {
		cmderr(L"Unknown style item.");
		return;
	}

	style_clear(sy);
	for (tok = wcstok(argv[2], L",", &last); tok != NULL;
	     tok = wcstok(NULL, L",", &last)) {
		style_add(sy, tok, argv[3]);
	}

	apply_styles();
}

void
c_bind(argc, argv)
	size_t	argc;
	wchar_t	**argv;
{
	if (argc != 3) {
		cmderr(L"Usage: bind <key> <function>");
		return;
	}

	bind_key(argv[1], argv[2], 0);
}

void
c_macro(argc, argv)
	size_t	argc;
	wchar_t	**argv;
{
	if (argc != 3) {
		cmderr(L"Usage: macro <key> <def>");
		return;
	}

	bind_key(argv[1], argv[2], 1);
}

void
c_set(argc, argv)
	size_t	argc;
	wchar_t	**argv;
{
variable_t	*var;
int		 val;
wchar_t		*p = NULL;

	if (argc != 3) {
		cmderr(L"Usage: set <variable> <value>");
		return;
	}

	if ((var = find_variable(argv[1])) == NULL) {
		cmderr(L"Unknown variable \"%ls\".", argv[1]);
		return;
	}

	switch (var->va_type) {
	case VTYPE_BOOL:
		if (wcscmp(argv[2], L"true") == 0 ||
		    wcscmp(argv[2], L"yes") == 0 ||
		    wcscmp(argv[2], L"on") == 0 ||
		    wcscmp(argv[2], L"1") == 0) {
			val = 1;
		} else if (wcscmp(argv[2], L"false") == 0 ||
			   wcscmp(argv[2], L"no") == 0 ||
			   wcscmp(argv[2], L"off") == 0 ||
			   wcscmp(argv[2], L"0") == 0) {
			val = 0;
		} else {
			cmderr(L"Invalid value for boolean: \"%ls\".", argv[2]);
			return;
		}

		*(int *)var->va_addr = val;
		break;

	case VTYPE_STRING:
		free(*(wchar_t **)var->va_addr);
		*(wchar_t **)var->va_addr = wcsdup(argv[2]);
		break;

	case VTYPE_INT:
		val = wcstol(argv[2], &p, 0);
		if (!*argv[2] || *p) {
			cmderr(L"Invalid number \"%ls\"", argv[2]);
			break;
		}

		*(int *)var->va_addr = val;
		break;
	}
}


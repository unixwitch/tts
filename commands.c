/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	"commands.h"
#include	"style.h"
#include	"bindings.h"
#include	"variable.h"

static command_t commands[] = {
	{ WIDE("bind"),		c_bind	},
	{ WIDE("style"),	c_style	},
	{ WIDE("set"),		c_set },
	{ }
};

command_t *
find_command(name)
	const WCHAR	*name;
{
size_t	i;
	for (i = 0; i < sizeof(commands) / sizeof(*commands); i++)
		if (STRCMP(name, commands[i].cm_name) == 0)
			return &commands[i];
	return NULL;
}

void
c_style(argc, argv)
	size_t	argc;
	WCHAR	**argv;
{
style_t	*sy;
WCHAR	*last, *tok;

	if (argc < 3 || argc > 4) {
		cmderr(WIDE("Usage: style <item> <foreground> [background]"));
		return;
	}

	if (STRCMP(argv[1], WIDE("header")) == 0)
		sy = &sy_header;
	else if (STRCMP(argv[1], WIDE("status")) == 0)
		sy = &sy_status;
	else if (STRCMP(argv[1], WIDE("entry")) == 0)
		sy = &sy_entry;
	else if (STRCMP(argv[1], WIDE("selected")) == 0)
		sy = &sy_selected;
	else if (STRCMP(argv[1], WIDE("running")) == 0)
		sy = &sy_running;
	else if (STRCMP(argv[1], WIDE("date")) == 0)
		sy = &sy_date;
	else {
		cmderr(WIDE("Unknown style item."));
		return;
	}

	style_clear(sy);
	for (tok = STRTOK(argv[2], WIDE(","), &last); tok != NULL;
	     tok = STRTOK(NULL, WIDE(","), &last)) {
		style_add(sy, tok, argv[3]);
	}

	apply_styles();
}

void
c_bind(argc, argv)
	size_t	argc;
	WCHAR	**argv;
{
	if (argc != 3) {
		cmderr(WIDE("Usage: bind <key> <function>"));
		return;
	}

	bind_key(argv[1], argv[2]);
}

void
c_set(argc, argv)
	size_t	argc;
	WCHAR	**argv;
{
variable_t	*var;
int		 val;

	if (argc != 3) {
		cmderr(WIDE("Usage: set <variable> <value>"));
		return;
	}

	if ((var = find_variable(argv[1])) == NULL) {
		cmderr(WIDE("Unknown variable \"%"FMT_L"s\"."), argv[1]);
		return;
	}

	switch (var->va_type) {
	case VTYPE_BOOL:
		if (STRCMP(argv[2], WIDE("true")) == 0 ||
		    STRCMP(argv[2], WIDE("yes")) == 0 ||
		    STRCMP(argv[2], WIDE("on")) == 0 ||
		    STRCMP(argv[2], WIDE("1")) == 0) {
			val = 1;
		} else if (STRCMP(argv[2], WIDE("false")) == 0 ||
			   STRCMP(argv[2], WIDE("no")) == 0 ||
			   STRCMP(argv[2], WIDE("off")) == 0 ||
			   STRCMP(argv[2], WIDE("0")) == 0) {
			val = 0;
		} else {
			cmderr(WIDE("Invalid value for boolean: \"%"FMT_L"s\"."), argv[2]);
			return;
		}

		*(int *)var->va_addr = val;
		break;

	case VTYPE_STRING:
		*(WCHAR **)var->va_addr = STRDUP(argv[2]);
		break;

	case VTYPE_INT:
		*(int *)var->va_addr = STRTOL(argv[2], NULL, 0);
		break;
	}
}


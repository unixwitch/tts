/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	"style.h"
#include	"ui.h"
#include	"wide.h"

short default_fg, default_bg;

style_t sy_header	= { 1, 0 },
	sy_status	= { 2, 0 },
	sy_entry	= { 3, 0 },
	sy_running	= { 4, 0 },
	sy_selected	= { 5, 0 },
	sy_date		= { 6, 0 };

static attrname_t attrnames[] = {
	{ WIDE("normal"),	WA_NORMAL	},
	{ WIDE("bold"),		WA_BOLD		},
	{ WIDE("reverse"),	WA_REVERSE	},
	{ WIDE("blink"),	WA_BLINK	},
	{ WIDE("dim"),		WA_DIM		},
	{ WIDE("underline"),	WA_UNDERLINE	},
	{ WIDE("standout"),	WA_STANDOUT	}
};

static colour_t colours[] = {
	{ WIDE("black"),	COLOR_BLACK	},
	{ WIDE("red"),		COLOR_RED	},
	{ WIDE("green"),	COLOR_GREEN	},
	{ WIDE("yellow"),	COLOR_YELLOW	},
	{ WIDE("blue"),		COLOR_BLUE	},
	{ WIDE("magenta"),	COLOR_MAGENTA	},
	{ WIDE("cyan"),		COLOR_CYAN	},
	{ WIDE("white"),	COLOR_WHITE	}
};

int
attr_find(name, result)
	const WCHAR	*name;
	attr_t		*result;
{
size_t	i;
	for (i = 0; i < sizeof(attrnames) / sizeof(*attrnames); i++) {
		if (STRCMP(attrnames[i].an_name, name) == 0) {
			*result = attrnames[i].an_value;
			return 0;
		}
	}

	return -1;
}

int
colour_find(name, result)
	const WCHAR	*name;
	short		*result;
{
size_t	i;
	for (i = 0; i < sizeof(colours) / sizeof(*colours); i++) {
		if (STRCMP(colours[i].co_name, name) == 0) {
			*result = colours[i].co_value;
			return 0;
		}
	}

	return -1;
}

void
style_clear(sy)
	style_t	*sy;
{
	init_pair(sy->sy_pair, default_fg, default_bg);
	sy->sy_attrs = WA_NORMAL;
}

int
style_set(sy, fg, bg)
	style_t		*sy;
	const WCHAR	*fg, *bg;
{
	sy->sy_attrs = WA_NORMAL;
	init_pair(sy->sy_pair, default_fg, default_bg);
	return style_add(sy, fg, bg);
}

int
style_add(sy, fg, bg)
	style_t		*sy;
	const WCHAR	*fg, *bg;
{
attr_t	at;
short	colfg, colbg = default_bg;

	if (colour_find(fg, &colfg) == 0) {
		if (bg && (colour_find(bg, &colbg) == -1))
			return -1;

		init_pair(sy->sy_pair, colfg, colbg);
		return 0;
	}

	if (attr_find(fg, &at) == -1)
		return -1;
	sy->sy_attrs |= at;
	return 0;
}

void
apply_styles()
{
	wbkgd(statwin, style_bg(sy_status));
	wattr_on(statwin, style_fg(sy_status), NULL);
	drawstatus(WIDE(""));

	wbkgd(titwin, style_bg(sy_header));
	wattr_on(titwin, style_fg(sy_header), NULL);
	drawheader();
}


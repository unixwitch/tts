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

#include	"bindings.h"
#include	"wide.h"
#include	"ui.h"

binding_list_t bindings = TTS_TAILQ_HEAD_INITIALIZER(bindings);

static tkey_t keys[] = {
	{ KEY_BREAK,		WIDE("<BREAK>")		},
	{ KEY_DOWN,		WIDE("<DOWN>")		},
	{ KEY_UP,		WIDE("<UP>")		},
	{ KEY_LEFT,		WIDE("<LEFT>")		},
	{ KEY_RIGHT,		WIDE("<RIGHT>")		},
	{ KEY_HOME,		WIDE("<HOME>")		},
	{ KEY_BACKSPACE,	WIDE("<BACKSPACE>")	},
	{ 0x7F,			WIDE("<BACKSPACE>")	}, /* DEL */
	{ KEY_F(0),		WIDE("<F0>")		},
	{ KEY_F(1),		WIDE("<F1>")		},
	{ KEY_F(2),		WIDE("<F2>")		},
	{ KEY_F(3),		WIDE("<F3>")		},
	{ KEY_F(4),		WIDE("<F4>")		},
	{ KEY_F(5),		WIDE("<F5>")		},
	{ KEY_F(6),		WIDE("<F6>")		},
	{ KEY_F(7),		WIDE("<F7>")		},
	{ KEY_F(8),		WIDE("<F8>")		},
	{ KEY_F(9),		WIDE("<F9>")		},
	{ KEY_F(10),		WIDE("<F10>")		},
	{ KEY_F(11),		WIDE("<F1l>")		},
	{ KEY_F(12),		WIDE("<F12>")		},
	{ KEY_F(13),		WIDE("<F13>")		},
	{ KEY_F(14),		WIDE("<F14>")		},
	{ KEY_F(15),		WIDE("<F15>")		},
	{ KEY_F(16),		WIDE("<F16>")		},
	{ KEY_F(17),		WIDE("<F17>")		},
	{ KEY_F(18),		WIDE("<F18>")		},
	{ KEY_F(19),		WIDE("<F19>")		},
	{ KEY_F(20),		WIDE("<F20>")		},
	{ KEY_F(21),		WIDE("<F21>")		},
	{ KEY_F(22),		WIDE("<F22>")		},
	{ KEY_F(23),		WIDE("<F23>")		},
	{ KEY_F(24),		WIDE("<F24>")		},
	{ KEY_NPAGE,		WIDE("<NEXT>")		},
	{ KEY_PPAGE,		WIDE("<PREV>")		},
	{ '\001',		WIDE("<CTRL-A>")	},
	{ '\002',		WIDE("<CTRL-B>")	},
	{ '\003',		WIDE("<CTRL-C>")	},
	{ '\004',		WIDE("<CTRL-D>")	},
	{ '\005',		WIDE("<CTRL-E>")	},
	{ '\006',		WIDE("<CTRL-F>")	},
	{ '\007',		WIDE("<CTRL-G>")	},
	{ '\010',		WIDE("<CTRL-H>")	},
	{ '\011',		WIDE("<CTRL-I>")	},
	{ '\011',		WIDE("<TAB>")		},
	{ '\012',		WIDE("<CTRL-J>")	},
	{ '\013',		WIDE("<CTRL-K>")	},
	{ '\014',		WIDE("<CTRL-L>")	},
	{ '\015',		WIDE("<CTRL-N>")	},
	{ '\016',		WIDE("<CTRL-O>")	},
	{ '\017',		WIDE("<CTRL-P>")	},
	{ '\020',		WIDE("<CTRL-Q>")	},
	{ '\021',		WIDE("<CTRL-R>")	},
	{ '\022',		WIDE("<CTRL-S>")	},
	{ '\023',		WIDE("<CTRL-T>")	},
	{ '\024',		WIDE("<CTRL-U>")	},
	{ '\025',		WIDE("<CTRL-V>")	},
	{ '\026',		WIDE("<CTRL-W>")	},
	{ '\027',		WIDE("<CTRL-X>")	},
	{ '\030',		WIDE("<CTRL-Y>")	},
	{ '\031',		WIDE("<CTRL-Z>")	},
	{ ' ',			WIDE("<SPACE>")		},
	{ KEY_ENTER,		WIDE("<ENTER>")		},
	{ KEY_BACKSPACE,	WIDE("<BACKSPACE>")	},
	{ KEY_DC,		WIDE("<DELETE>")	}
};

/*
 * Bind .keyname to run the function .funcname.  If a binding for .keyname
 * already exists, overwrite it.
 *
 * If .keyname is a single character, e.g. 'a', it is used as a key name
 * directly, rather than being looked up in the key table.
 */
void
bind_key(keyname, funcname, is_macro)
	const WCHAR	*keyname, *funcname;
{
tkey_t		*key = NULL;
function_t	*func;
binding_t	*binding;
INT		 code;

	/* Find the key and the function */
	if (STRLEN(keyname) > 1) {
		if ((key = find_key(keyname)) == NULL) {
			errbox(WIDE("Unknown key \"%"FMT_L"s\""), keyname);
			return;
		}
		code = key->ky_code;
	} else
		code = *keyname;

	if (!is_macro) {
		if ((func = find_func(funcname)) == NULL) {
			errbox(WIDE("Unknown function \"%"FMT_L"s\""), funcname);
			return;
		}
	}

	/* Do we already have a binding for this key? */
	TTS_TAILQ_FOREACH(binding, &bindings, bi_entries) {
		if (binding->bi_code == code) {
			if (is_macro) {
				binding->bi_func = NULL;
				binding->bi_macro = STRDUP(funcname);
			} else {
				free(binding->bi_macro);
				binding->bi_func = func;
			}
			return;
		}
	}

	/* No, add a new one */
	if ((binding = calloc(1, sizeof(*binding))) == NULL)
		return;

	binding->bi_key = key;
	binding->bi_code = code;

	if (is_macro)
		binding->bi_macro = STRDUP(funcname);
	else
		binding->bi_func = func;

	TTS_TAILQ_INSERT_TAIL(&bindings, binding, bi_entries);
}

/*
 * Return the tkey_t for the key called .name, or NULL if such a key doesn't
 * exist.
 */
tkey_t *
find_key(name)
	const WCHAR	*name;
{
size_t	i;

	for (i = 0; i < sizeof(keys) / sizeof(*keys); i++)
		if (STRCMP(name, keys[i].ky_name) == 0)
			return &keys[i];
	return NULL;
}


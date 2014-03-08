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
	{ KEY_BREAK,		L"<BREAK>"	},
	{ KEY_DOWN,		L"<DOWN>"	},
	{ KEY_UP,		L"<UP>"		},
	{ KEY_LEFT,		L"<LEFT>"	},
	{ KEY_RIGHT,		L"<RIGHT>"	},
	{ KEY_HOME,		L"<HOME>"	},
	{ KEY_BACKSPACE,	L"<BACKSPACE>"	},
	{ 0x7F,			L"<BACKSPACE>"	}, /* DEL */
	{ KEY_F(0),		L"<F0>"		},
	{ KEY_F(1),		L"<F1>"		},
	{ KEY_F(2),		L"<F2>"		},
	{ KEY_F(3),		L"<F3>"		},
	{ KEY_F(4),		L"<F4>"		},
	{ KEY_F(5),		L"<F5>"		},
	{ KEY_F(6),		L"<F6>"		},
	{ KEY_F(7),		L"<F7>"		},
	{ KEY_F(8),		L"<F8>"		},
	{ KEY_F(9),		L"<F9>"		},
	{ KEY_F(10),		L"<F10>"	},
	{ KEY_F(11),		L"<F1l>"	},
	{ KEY_F(12),		L"<F12>"	},
	{ KEY_F(13),		L"<F13>"	},
	{ KEY_F(14),		L"<F14>"	},
	{ KEY_F(15),		L"<F15>"	},
	{ KEY_F(16),		L"<F16>"	},
	{ KEY_F(17),		L"<F17>"	},
	{ KEY_F(18),		L"<F18>"	},
	{ KEY_F(19),		L"<F19>"	},
	{ KEY_F(20),		L"<F20>"	},
	{ KEY_F(21),		L"<F21>"	},
	{ KEY_F(22),		L"<F22>"	},
	{ KEY_F(23),		L"<F23>"	},
	{ KEY_F(24),		L"<F24>"	},
	{ KEY_NPAGE,		L"<NEXT>"	},
	{ KEY_PPAGE,		L"<PREV>"	},
	{ '\001',		L"<CTRL-A>"	},
	{ '\002',		L"<CTRL-B>"	},
	{ '\003',		L"<CTRL-C>"	},
	{ '\004',		L"<CTRL-D>"	},
	{ '\005',		L"<CTRL-E>"	},
	{ '\006',		L"<CTRL-F>"	},
	{ '\007',		L"<CTRL-G>"	},
	{ '\010',		L"<CTRL-H>"	},
	{ '\011',		L"<CTRL-I>"	},
	{ '\011',		L"<TAB>"	},
	{ '\012',		L"<CTRL-J>"	},
	{ '\013',		L"<CTRL-K>"	},
	{ '\014',		L"<CTRL-L>"	},
	{ '\015',		L"<CTRL-N>"	},
	{ '\016',		L"<CTRL-O>"	},
	{ '\017',		L"<CTRL-P>"	},
	{ '\020',		L"<CTRL-Q>"	},
	{ '\021',		L"<CTRL-R>"	},
	{ '\022',		L"<CTRL-S>"	},
	{ '\023',		L"<CTRL-T>"	},
	{ '\024',		L"<CTRL-U>"	},
	{ '\025',		L"<CTRL-V>"	},
	{ '\026',		L"<CTRL-W>"	},
	{ '\027',		L"<CTRL-X>"	},
	{ '\030',		L"<CTRL-Y>"	},
	{ '\031',		L"<CTRL-Z>"	},
	{ ' ',			L"<SPACE>"	},
	{ KEY_ENTER,		L"<ENTER>"	},
	{ KEY_BACKSPACE,	L"<BACKSPACE>"	},
	{ KEY_DC,		L"<DELETE>"	}
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
	const wchar_t	*keyname, *funcname;
{
tkey_t		*key = NULL;
function_t	*func;
binding_t	*binding;
wint_t		 code;

	/* Find the key and the function */
	if (wcslen(keyname) > 1) {
		if ((key = find_key(keyname)) == NULL) {
			errbox(L"Unknown key \"%ls\"", keyname);
			return;
		}
		code = key->ky_code;
	} else
		code = *keyname;

	if (!is_macro) {
		if ((func = find_func(funcname)) == NULL) {
			errbox(L"Unknown function \"%ls\"", funcname);
			return;
		}
	}

	/* Do we already have a binding for this key? */
	TTS_TAILQ_FOREACH(binding, &bindings, bi_entries) {
		if (binding->bi_code == code) {
			if (is_macro) {
				binding->bi_func = NULL;
				binding->bi_macro = wcsdup(funcname);
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
		binding->bi_macro = wcsdup(funcname);
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
	const wchar_t	*name;
{
size_t	i;

	for (i = 0; i < sizeof(keys) / sizeof(*keys); i++)
		if (wcscmp(name, keys[i].ky_name) == 0)
			return &keys[i];
	return NULL;
}

void
bind_defaults(void)
{
	bind_key(L"?",		L"help", 0);
	bind_key(L"a",		L"add", 0);
	bind_key(L"A",		L"add-old", 0);
	bind_key(L"d",		L"delete", 0);
	bind_key(L"u",		L"undelete", 0);
	bind_key(L"q",		L"quit", 0);
	bind_key(L"<CTRL-C>",	L"quit", 0);
	bind_key(L"i",		L"invoice", 0);
	bind_key(L"b",		L"billable", 0);
	bind_key(L"m",		L"mark", 0);
	bind_key(L"U",		L"unmarkall", 0);
	bind_key(L"<SPACE>",	L"startstop", 0);
	bind_key(L"e",		L"edit-desc", 0);
	bind_key(L"\\",		L"edit-time", 0);
	bind_key(L"<TAB>",	L"showhide-inv", 0);
	bind_key(L"c",		L"copy", 0);
	bind_key(L"+",		L"add-time", 0);
	bind_key(L"-",		L"sub-time", 0);
	bind_key(L"/",		L"search", 0);
	bind_key(L"$",		L"sync", 0);
	bind_key(L"<UP>",	L"prev", 0);
	bind_key(L"<DOWN>",	L"next", 0);
	bind_key(L":",		L"execute", 0);
	bind_key(L"M",		L"merge", 0);
	bind_key(L"r",		L"interrupt", 0);
	bind_key(L"R",		L"interrupt", 0);
}

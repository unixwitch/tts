/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	"str.h"

size_t
tokenise(str, res)
	const WCHAR	*str;
	WCHAR		***res;
{
int		 ntoks = 0;
const WCHAR	*p, *q;
WCHAR		*r;

	*res = NULL;
	p = str;

	for (;;) {
	ptrdiff_t	sz;
	int		qskip = 0;
	int		isbsl = 0;

	/* Skip leading whitespace */
		while (ISSPACE(*p))
			p++;

	/* End of string - no more arguments */
		if (!*p)
			break;

		q = p;

		if (*q == '"') {
	/* Quoted string - scan for end of string */
			p++;

			while (*++q) {
	/* Handle escaping with backslash; currently works but the \ isn't
	 * removed from the string.
	 */
				if (!isbsl && (*q == '\\')) {
					isbsl = 1;
					continue;
				}

				if (!isbsl && (*q == '"'))
					break;

				isbsl = 0;
			}
	/* At this point, *q == '"'.  If it's NUL instead, then the
	 * string was not terminated with a closing '"' before the end 
	 * of the line.  We could give an error here, but it seems
	 * more useful to just accept it.
	 */
			if (*q == '"')
				qskip = 1;
		} else {
	/* Not quoted - just find the next whitespace */
			while (!ISSPACE(*q) && *q)
				q++;
		}

	/* Copy the argument (which is sz bytes long) into the result array */
		sz = (q - p);
		*res = realloc(*res, sizeof(WCHAR *) * (ntoks + 1));
		(*res)[ntoks] = malloc(sizeof(WCHAR) * (sz + 1));
		MEMCPY((*res)[ntoks], p, sz);

	/* Handle \ escapes */
		for (r = (*res)[ntoks]; r < ((*res)[ntoks] + sz);) {
			if (!isbsl) {
				if (*r == '\\') {
					MEMMOVE(r, r + 1, sz - (r - (*res)[ntoks]));
					sz--;
					isbsl = 1;
					continue;
				}

				r++;
				continue;
			}

			switch (*r) {
			case 't':	*r = '\t'; break;
			case 'n':	*r = '\n'; break;
			case 'r':	*r = '\r'; break;
			case 'v':	*r = '\v'; break;
			case '\\':	*r = '\\'; break;
			}

			isbsl = 0;
			r++;
		}

		(*res)[ntoks][sz] = 0;
		ntoks++;

		if (qskip)
			q += qskip;

		while (ISSPACE(*q))
			q++;

	/*
	 * q is the start of the next token (with leading whitespace); reset
	 * p to process the next argument.
	 */
		if (!*q)
			break;
		p = q;
	}

	*res = realloc(*res, sizeof(WCHAR *) * (ntoks + 1));
	(*res)[ntoks] = NULL;
	return ntoks;
}

void
tokfree(vec)
	WCHAR	***vec;
{
WCHAR	**p;
	for (p = (*vec); *p; p++)
		free(*p);
	free(*vec);
}


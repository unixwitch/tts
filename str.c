/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<stddef.h>
#include	<wctype.h>

#include	"str.h"

size_t
tokenise(str, res)
	const wchar_t	*str;
	wchar_t		***res;
{
int		 ntoks = 0;
const wchar_t	*p, *q;
wchar_t		*r;

	*res = NULL;
	p = str;

	for (;;) {
	ptrdiff_t	sz;
	int		qskip = 0;
	int		isbsl = 0;

	/* Skip leading whitespace */
		while (iswspace(*p))
			p++;

	/* End of string - no more arguments */
		if (!*p)
			break;

		q = p;

		if (*q == '"') {
	/* Quoted string - scan for end of string */
			p++;

			while (*++q) {
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
			while (!iswspace(*q) && *q)
				q++;
		}

	/* Copy the argument (which is sz bytes long) into the result array */
		sz = (q - p);
		*res = realloc(*res, sizeof(wchar_t *) * (ntoks + 1));
		(*res)[ntoks] = malloc(sizeof(wchar_t) * (sz + 1));
		wmemcpy((*res)[ntoks], p, sz);

	/* Handle \ escapes */
		for (r = (*res)[ntoks]; r < ((*res)[ntoks] + sz);) {
			if (!isbsl) {
				if (*r == '\\') {
					wmemmove(r, r + 1, sz - (r - (*res)[ntoks]));
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

		while (iswspace(*q))
			q++;

	/*
	 * q is the start of the next token (with leading whitespace); reset
	 * p to process the next argument.
	 */
		if (!*q)
			break;
		p = q;
	}

	*res = realloc(*res, sizeof(wchar_t *) * (ntoks + 1));
	(*res)[ntoks] = NULL;
	return ntoks;
}

void
tokfree(vec)
	wchar_t	***vec;
{
wchar_t	**p;
	for (p = (*vec); *p; p++)
		free(*p);
	free(*vec);
}

time_t
parsetime(tm)
	const wchar_t	*tm;
{
int	h = 0, m = 0, s = 0;
time_t	i = 0, r = 0;

/* The empty string is not a valid duration */
	if (!*tm)
		return (time_t) -1;
/* Check for "hh:mm:ss" or "mm:ss" */
	if (swscanf(tm, L"%d:%d:%d", &h, &m, &s) == 3)
		return (h * 60 * 60) + (m * 60) + s;

	if (swscanf(tm, L"%d:%d", &m, &s) == 2)
		return (m * 60) + s;

/*
 * The string could either be a format like 3h10m, or a simle number like 47
 * (meaning seconds), which is also handled here.  This is effectively an
 * implementation of atoi with special meaning for 'h', 'm' and 's' characters.
 *
 * Note that we make no attempt to handle overflow.
 */
	for (; *tm; tm++) {
		switch (*tm) {
		case L'h':
			r += i * (60 * 60);
			i = 0;
			continue;

		case L'm':
			r += i * 60;
			i = 0;
			continue;

		case L's':
			r += i;
			i = 0;
			continue;
		}

		if (wcschr(L"0123456789", *tm) == NULL)
			return (time_t) -1;

		i *= 10;
		i += *tm - L'0';
	}

	return r + i;
}

wchar_t *
maketime(tm)
	time_t	tm;
{
wchar_t	res[64] = {};
wchar_t	t[16];

	if (tm >= (60 * 60)) {
		swprintf(t, wsizeof(t), L"%dh", tm / (60 * 60));
		wcslcat(res, t, wsizeof(res));
		tm %= (60 * 60);
	}

	if (tm >= 60) {
		swprintf(t, wsizeof(t), L"%dm", tm / 60);
		wcslcat(res, t, wsizeof(res));
		tm %= 60;
	}

	if (tm) {
		swprintf(t, wsizeof(t), L"%ds", tm);
		wcslcat(res, t, wsizeof(res));
	}

	return wcsdup(res);
}

wchar_t *
escstr(s)
	const wchar_t	*s;
{
wchar_t	*ret, *p;

	if ((ret = calloc(sizeof(wchar_t), wcslen(s) * 2 + 1)) == NULL)
		return NULL;

	for (p = ret; *s; s++) {
		switch (*s) {
		case '\\':
			*p++ = L'\\';
			*p++ = L'\\';
			continue;
		case '\n':
			*p++ = L'\\';
			*p++ = L'n';
			continue;
		case '\t':
			*p++ = L'\\';
			*p++ = L't';
			continue;
		case '\v':
			*p++ = L'\\';
			*p++ = L'v';
			continue;
		case '\r':
			*p++ = L'\\';
			*p++ = L'r';
			continue;
		case '"':
			*p++ = L'\\';
			*p++ = L'"';
			continue;
		}

		*p++ = *s;
	}
	return ret;
}

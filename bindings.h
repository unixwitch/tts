/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_BINDINGS_H
#define	TTS_BINDINGS_H

#include	"wide.h"
#include	"functions.h"
#include	"tailq.h"

typedef struct tkey {
	wint_t		 ky_code;
	const wchar_t	*ky_name;
} tkey_t;

typedef struct binding {
	wint_t		 bi_code;
	tkey_t		*bi_key;
	function_t	*bi_func;
	wchar_t		*bi_macro;

	TTS_TAILQ_ENTRY(binding) bi_entries;
} binding_t;

typedef TTS_TAILQ_HEAD(bindlist, binding) binding_list_t;
extern binding_list_t bindings;

void	bind_defaults();
tkey_t *find_key(const wchar_t *name);
void	bind_key(const wchar_t *key, const wchar_t *func, int is_macro);


#endif	/* !TTS_BINDINGS_H */

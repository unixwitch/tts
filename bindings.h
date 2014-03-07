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
	INT		 ky_code;
	const WCHAR	*ky_name;
} tkey_t;

typedef struct binding {
	INT		 bi_code;
	tkey_t		*bi_key;
	function_t	*bi_func;

	TTS_TAILQ_ENTRY(binding) bi_entries;
} binding_t;

typedef TTS_TAILQ_HEAD(bindlist, binding) binding_list_t;
extern binding_list_t bindings;

tkey_t *find_key(const WCHAR *name);
void	bind_key(const WCHAR *key, const WCHAR *func);

#endif	/* !TTS_BINDINGS_H */

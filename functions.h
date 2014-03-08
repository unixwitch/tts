/*
 * TTS - track your time.
 * Copyright (c) 2012-2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	TTS_FUNCTIONS_H
#define	TTS_FUNCTIONS_H

#include	"wide.h"

void kadd(void);
void kaddold(void);
void kquit(void);
void kup(void);
void kdown(void);
void ktoggle(void);
void kinvoiced(void);
void kbillable(void);
void keddesc(void);
void kedtime(void);
void ktoggleinv(void);
void kcopy(void);
void kaddtime(void);
void kdeltime(void);
void khelp(void);
void kmark(void);
void kunmarkall(void);
void ksearch(void);
void kmarkdel(void);
void kundel(void);
void ksync(void);
void kexec(void);
void kmerge(void);
void kint(void);

typedef struct function {
	const wchar_t	*fn_name;
	void		(*fn_hdl) (void);
	const wchar_t	*fn_desc;
} function_t;

extern function_t funcs[];

function_t *find_func(const wchar_t *name);

#endif	/* !TTS_FUNCTIONS_H */

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)queue.h	8.5 (Berkeley) 8/20/94
 * FreeBSD: release/9.0.0/sys/sys/queue.h 221843 2011-05-13 15:49:23Z mdf 
 */

#ifndef	TTS_TAILQ_H
#define	TTS_TAILQ_H

/*
 * Tail queue declarations.
 */
#define	TTS_TAILQ_HEAD(name, type)					\
struct name {								\
	struct type *tqh_first;	/* first element */			\
	struct type **tqh_last;	/* addr of last next element */		\
}

#define	TTS_TAILQ_HEAD_INITIALIZER(head)				\
	{ NULL, &(head).tqh_first }

#define	TTS_TAILQ_ENTRY(type)						\
struct {								\
	struct type *tqe_next;	/* next element */			\
	struct type **tqe_prev;	/* address of previous next element */	\
}

#define	TTS_TAILQ_CONCAT(head1, head2, field) do {			\
	if (!TAILQ_EMPTY(head2)) {					\
		*(head1)->tqh_last = (head2)->tqh_first;		\
		(head2)->tqh_first->field.tqe_prev = (head1)->tqh_last;	\
		(head1)->tqh_last = (head2)->tqh_last;			\
		TTS_TAILQ_INIT((head2));					\
	}								\
} while (0)

#define	TTS_TAILQ_EMPTY(head)	((head)->tqh_first == NULL)

#define	TTS_TAILQ_FIRST(head)	((head)->tqh_first)

#define	TTS_TAILQ_FOREACH(var, head, field)				\
	for ((var) = TTS_TAILQ_FIRST((head));				\
	    (var);							\
	    (var) = TTS_TAILQ_NEXT((var), field))

#define	TTS_TAILQ_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = TTS_TAILQ_FIRST((head));				\
	    (var) && ((tvar) = TTS_TAILQ_NEXT((var), field), 1);	\
	    (var) = (tvar))

#define	TTS_TAILQ_FOREACH_REVERSE(var, head, headname, field)		\
	for ((var) = TTS_TAILQ_LAST((head), headname);			\
	    (var);							\
	    (var) = TTS_TAILQ_PREV((var), headname, field))

#define	TTS_TAILQ_FOREACH_REVERSE_SAFE(var, head, headname, field, tvar)\
	for ((var) = TTS_TAILQ_LAST((head), headname);			\
	    (var) && ((tvar) = TTS_TAILQ_PREV((var), headname, field), 1);	\
	    (var) = (tvar))

#define	TTS_TAILQ_INIT(head) do {					\
	TTS_TAILQ_FIRST((head)) = NULL;					\
	(head)->tqh_last = &TTS_TAILQ_FIRST((head));			\
} while (0)

#define	TTS_TAILQ_INSERT_AFTER(head, listelm, elm, field) do {		\
	if ((TTS_TAILQ_NEXT((elm), field) = TAILQ_NEXT((listelm), field)) != NULL)\
		TTS_TAILQ_NEXT((elm), field)->field.tqe_prev = 		\
		    &TTS_TAILQ_NEXT((elm), field);			\
	else {								\
		(head)->tqh_last = &TTS_TAILQ_NEXT((elm), field);	\
	}								\
	TTS_TAILQ_NEXT((listelm), field) = (elm);			\
	(elm)->field.tqe_prev = &TTS_TAILQ_NEXT((listelm), field);	\
} while (0)

#define	TTS_TAILQ_INSERT_BEFORE(listelm, elm, field) do {		\
	(elm)->field.tqe_prev = (listelm)->field.tqe_prev;		\
	TTS_TAILQ_NEXT((elm), field) = (listelm);			\
	*(listelm)->field.tqe_prev = (elm);				\
	(listelm)->field.tqe_prev = &TTS_TAILQ_NEXT((elm), field);	\
} while (0)

#define	TTS_TAILQ_INSERT_HEAD(head, elm, field) do {			\
	if ((TTS_TAILQ_NEXT((elm), field) = TTS_TAILQ_FIRST((head))) != NULL)	\
		TTS_TAILQ_FIRST((head))->field.tqe_prev =		\
		    &TTS_TAILQ_NEXT((elm), field);			\
	else								\
		(head)->tqh_last = &TTS_TAILQ_NEXT((elm), field);	\
	TTS_TAILQ_FIRST((head)) = (elm);				\
	(elm)->field.tqe_prev = &TTS_TAILQ_FIRST((head));		\
} while (0)

#define	TTS_TAILQ_INSERT_TAIL(head, elm, field) do {			\
	TTS_TAILQ_NEXT((elm), field) = NULL;				\
	(elm)->field.tqe_prev = (head)->tqh_last;			\
	*(head)->tqh_last = (elm);					\
	(head)->tqh_last = &TTS_TAILQ_NEXT((elm), field);		\
} while (0)

#define	TTS_TAILQ_LAST(head, headname)					\
	(*(((struct headname *)((head)->tqh_last))->tqh_last))

#define	TTS_TAILQ_NEXT(elm, field) ((elm)->field.tqe_next)

#define	TTS_TAILQ_PREV(elm, headname, field)				\
	(*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))

#define	TTS_TAILQ_REMOVE(head, elm, field) do {				\
	if ((TTS_TAILQ_NEXT((elm), field)) != NULL)			\
		TTS_TAILQ_NEXT((elm), field)->field.tqe_prev = 		\
		    (elm)->field.tqe_prev;				\
	else {								\
		(head)->tqh_last = (elm)->field.tqe_prev;		\
	}								\
	*(elm)->field.tqe_prev = TTS_TAILQ_NEXT((elm), field);		\
} while (0)

#define TTS_TAILQ_SWAP(head1, head2, type, field) do {			\
	struct type *swap_first = (head1)->tqh_first;			\
	struct type **swap_last = (head1)->tqh_last;			\
	(head1)->tqh_first = (head2)->tqh_first;			\
	(head1)->tqh_last = (head2)->tqh_last;				\
	(head2)->tqh_first = swap_first;				\
	(head2)->tqh_last = swap_last;					\
	if ((swap_first = (head1)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head1)->tqh_first;	\
	else								\
		(head1)->tqh_last = &(head1)->tqh_first;		\
	if ((swap_first = (head2)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head2)->tqh_first;	\
	else								\
		(head2)->tqh_last = &(head2)->tqh_first;		\
} while (0)


#endif	/* !TTS_TAILQ_H */

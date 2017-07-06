/*
 * Copyright (c) 2017 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LIB_EVL_INTERNAL_H_
#define _LIB_EVL_INTERNAL_H_

#include <sys/queue.h>
#include <time.h>

#include "evl.h"
#include "heap.h"

#define SET(_v, _m)	((_v) |= (_m))
#define CLR(_v, _m)	((_v) &= ~(_m))
#define ISSET(_v, _m)	((_v) & (_m))

struct evl_ops {
	void		*(*evlo_create)(void);
	void		 (*evlo_destroy)(void *);

	int		 (*evlo_dispatch)(struct evl_base *,
			       const struct timespec *);

	int		 (*evlo_io_create)(struct evl_io *);
	void		 (*evlo_io_add)(struct evl_io *);
	void		 (*evlo_io_del)(struct evl_io *);
	void		 (*evlo_io_destroy)(struct evl_io *);

#ifdef notyet
	int		 (*evlo_sig_create(struct evl_sig *);
	void		 (*evlo_sig_add)(struct evl_sig *);
	void		 (*evlo_sig_del)(struct evl_sig *);
	void		 (*evlo_sig_destroy)(struct evl_sig *);

	int		 (*evlo_wait_create)(struct evl_wait *);
	void		 (*evlo_wait_add)(struct evl_wait *);
	void		 (*evlo_wait_del)(struct evl_wait *);
	void		 (*evlo_wait_destroy)(struct evl_wait *);
#endif
};

#define EVL_PENDING	(1 << 30)	/* event is waiting to fire */
#define EVL_FIRED	(1 << 31)	/* event has fired */

#define EVL_RW		(EVL_READ|EVL_WRITE)

struct evl_work {
	struct evl_base	  *evl_base;
	TAILQ_ENTRY(evl_work)
			  evl_entry;
	void		(*evl_fn)(int, int, void *);
	void		 *evl_arg;
	int		  evl_ident;
	int		  evl_event;
	int		  evl_fires;
};
#define evl_work_base(_evl)	((_evl)->evl_base)

struct evl_io {
	struct evl_work	  evl_io_work;
	unsigned int	  evl_io_idx;
};
#define evl_io_base(_evlio)	evl_work_base(&(_evlio)->evl_io_work)

struct evl_tmo {
	struct evl_work	  evl_tmo_work;
	struct timespec	  evl_tmo_deadline;
	HEAP_ENTRY(evl_tmo)
			  evl_tmo_entry;
};
#define evl_tmo_base(_evlt)	evl_work_base(&(_evlt)->evl_tmo_work)

#ifdef notyet
struct evl_sig {
	struct evl_work	  evl_sig_work;
	void		(*evl_sig_handler)(int);
};
#define evl_sig_base(_evls)	evl_work_base(&(_evls)->evl_sig_work)

struct evl_wait {
	struct evl_work	  evl_wait_work;
};
#define evl_wait_base(_evlw)	evl_work_base(&(_evlw)->evl_wait_work)
#endif

void		*evl_backend(const struct evl_base *);

void		 evl_io_fire(struct evl_io *, int);
#ifdef notyet
void		 evl_sig_fire(struct evl_sig *);
void		 evl_wait_fire(struct evl_wait *, int);
#endif

#endif /* _LIB_EVL_H */

/*	$OpenBSD$ */

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

#include <sys/time.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#include "evl-internal.h"
#include "evl-config.h"

TAILQ_HEAD(evl_work_list, evl_work);
HEAP_HEAD(evl_tmo_heap);

struct evl_base {
	const struct evl_ops	*evlb_ops;
	void			*evlb_backend;

	struct evl_work_list	 evlb_work;
	struct evl_tmo_heap	 evlb_tmos;

	unsigned int		 evlb_nevl;
	unsigned int		 evlb_running;
};

HEAP_PROTOTYPE(evl_tmo_heap, evl_tmo);

static inline void
evlb_tmo_init(struct evl_base *evlb)
{
	HEAP_INIT(evl_tmo_heap, &evlb->evlb_tmos);
}

static inline struct evl_tmo *
evlb_tmo_cextract(struct evl_base *evlb, const struct evl_tmo *now)
{
	return (HEAP_CEXTRACT(evl_tmo_heap, &evlb->evlb_tmos, now));
}

static inline struct evl_tmo *
evlb_tmo_first(struct evl_base *evlb)
{
	return (HEAP_FIRST(evl_tmo_heap, &evlb->evlb_tmos));
}

static inline void
evlb_tmo_insert(struct evl_base *evlb, struct evl_tmo *evlt,
    const struct timespec *now, const struct timespec *offset)
{
	timespecadd(now, offset, &evlt->evl_tmo_deadline);
	HEAP_INSERT(evl_tmo_heap, &evlb->evlb_tmos, evlt);
}

static inline void
evlb_tmo_remove(struct evl_base *evlb, struct evl_tmo *evlt)
{
	HEAP_REMOVE(evl_tmo_heap, &evlb->evlb_tmos, evlt);
}

static inline void
evlb_work_init(struct evl_base *evlb)
{
	TAILQ_INIT(&evlb->evlb_work);
}

static inline void
evlb_work_insert(struct evl_base *evlb, struct evl_work *evl)
{
	SET(evl->evl_event, EVL_FIRED);
	TAILQ_INSERT_TAIL(&evlb->evlb_work, evl, evl_entry);
}

static inline void
evlb_work_remove(struct evl_base *evlb, struct evl_work *evl)
{
	TAILQ_REMOVE(&evlb->evlb_work, evl, evl_entry);
	CLR(evl->evl_event, EVL_FIRED);
}

static inline struct evl_work *
evlb_work_first(struct evl_base *evlb)
{
	return (TAILQ_FIRST(&evlb->evlb_work));
}

#define evl_monotime(_ts)	clock_gettime(CLOCK_MONOTONIC, (_ts))

#define evl_op_dispatch(_evlb, _deadline)				\
	(*(_evlb)->evlb_ops->evlo_dispatch)((_evlb), (_deadline))

#define evl_op_io_create(_evlb, _evlio)					\
	(*(_evlb)->evlb_ops->evlo_io_create)((_evlio))
#define evl_op_io_add(_evlb, _evlio)					\
	(*(_evlb)->evlb_ops->evlo_io_add)((_evlio))
#define evl_op_io_del(_evlb, _evlio)					\
	(*(_evlb)->evlb_ops->evlo_io_del)((_evlio))
#define evl_op_io_destroy(_evlb, _evlio)				\
	(*(_evlb)->evlb_ops->evlo_io_destroy)((_evlio))

static void	evl_work_init(struct evl_work *, struct evl_base *,
		    int, int, void (*)(int, int, void *), void *);

struct evl_base *
evl_init(void)
{
	const struct evl_ops *ops = EVL_DEFAULT_OPS;
	struct evl_base *evlb;
	void *backend;

	evlb = malloc(sizeof(*evlb));
	if (evlb == NULL)
		return (NULL);

	backend = (*ops->evlo_create)();
	if (backend == NULL) {
		free(evlb);
		return (NULL);
	}

	evlb->evlb_ops = ops;
	evlb->evlb_backend = backend;

	evlb->evlb_running = 0;
	evlb->evlb_nevl = 0;
	evlb_work_init(evlb);
	evlb_tmo_init(evlb);

	return (evlb);
}

#include <unistd.h>
#include <stdio.h>

int
evl_dispatch(struct evl_base *evlb)
{
	struct evl_tmo now, *evlt;
	struct evl_work *evl;
	struct timespec *ts;
	int fires;
printf("%d %s %u\n", getpid(), __func__, __LINE__);

	evlb->evlb_running = 1;
	for (;;) {
		if (evl_monotime(&now.evl_tmo_deadline) == -1)
			return (-1);

printf("%d %s %u\n", getpid(), __func__, __LINE__);
		while ((evlt = evlb_tmo_cextract(evlb, &now)) != NULL) {
printf("%d %s %p\n", getpid(), __func__, evlt);
			CLR(evlt->evl_tmo_work.evl_event, EVL_PENDING);
			evl_work_add(&evlt->evl_tmo_work, EVL_TIMEOUT);
printf("%d %s %u\n", getpid(), __func__, __LINE__);
		}

		while ((evl = evlb_work_first(evlb)) != NULL) {
			evlb_work_remove(evlb, evl);
			fires = evl->evl_fires;
			evl->evl_fires = 0;

printf("%d %s %u\n", getpid(), __func__, __LINE__);
			(*evl->evl_fn)(evl->evl_ident, fires, evl->evl_arg);

			if (!evlb->evlb_running)
				return (0);
		}

printf("%d %s %u\n", getpid(), __func__, __LINE__);
		evlt = evlb_tmo_first(evlb);
		if (evlt != NULL) {
			ts = &now.evl_tmo_deadline;
			timespecsub(&evlt->evl_tmo_deadline, ts, ts);
		} else
			ts = NULL;

printf("%d %s %u\n", getpid(), __func__, __LINE__);
		if (evl_op_dispatch(evlb, ts) == -1)
			return (-1);
	}

	return (0);
}

static void
evl_work_init(struct evl_work *evlw, struct evl_base *evl,
    int ident, int event, void (*fn)(int, int, void *), void *arg)
{
	evlw->evl_base = evl;
	evlw->evl_fn = fn;
	evlw->evl_arg = arg;
	evlw->evl_ident = ident;
	evlw->evl_event = event;
	evlw->evl_fires = 0;
}

struct evl_work *
evl_work_create(struct evl_base *evlb, int ident,
    void (*fn)(int, int, void *), void *arg)
{
	struct evl_work *evl;

	evl = malloc(sizeof(*evl));
	if (evl == NULL)
		return (NULL);

	evl_work_init(evl, evlb, ident, 0, fn, arg);

	return (evl);
}

void
evl_work_set(struct evl_work *evlw, void (*fn)(int, int, void *))
{
	evlw->evl_fn = fn;
}

int
evl_work_add(struct evl_work *evl, int fires)
{
	SET(evl->evl_fires, fires);

	if (ISSET(evl->evl_event, EVL_FIRED))
		return (0);

	evlb_work_insert(evl->evl_base, evl);

	return (1);
}

int
evl_work_pending(const struct evl_work *evlw)
{
	return (ISSET(evlw->evl_event, EVL_FIRED) ? 1 : 0);
}

int
evl_work_del(struct evl_work *evl)
{
	if (!ISSET(evl->evl_event, EVL_FIRED))
		return (0);

	evlb_work_remove(evl->evl_base, evl);
	evl->evl_fires = 0;

	return (1);
}

void
evl_work_destroy(struct evl_work *evlw)
{
	if (evlw == NULL)
		return;

	assert(!evl_work_pending(evlw));

	free(evlw);
}

struct evl_io *
evl_io_create(struct evl_base *evlb, int fd, int events,
    void (*fn)(int, int, void *), void *arg)
{
	struct evl_io *evlio;

	assert(!ISSET(events, ~(EVL_READ|EVL_WRITE|EVL_PERSIST)) && events);

	evlio = malloc(sizeof(*evlio));
	if (evlio == NULL)
		return (NULL);

	evl_work_init(&evlio->evl_io_work, evlb, fd, events, fn, arg);

	if (evl_op_io_create(evlb, evlio) == -1) {
		free(evlio);
		return (NULL);
	};

	return (evlio);
}

void
evl_io_set(struct evl_io *evlio, void (*fn)(int, int, void *))
{
	struct evl_work *evl = &evlio->evl_io_work;

	evl_work_set(evl, fn);
}

int
evl_io_fd(const struct evl_io *evlio)
{
	const struct evl_work *evl = &evlio->evl_io_work;

	return (evl->evl_ident);
}

int
evl_io_add(struct evl_io *evlio)
{
	struct evl_work *evl = &evlio->evl_io_work;
	struct evl_base *evlb = evl->evl_base;

	if (ISSET(evl->evl_event, EVL_PERSIST|EVL_FIRED) == EVL_FIRED)
		return (0);

	if (ISSET(evl->evl_event, EVL_PENDING))
		return (0);

	SET(evl->evl_event, EVL_PENDING);
	evl_op_io_add(evlb, evlio);

	return (1);
}

void
evl_io_fire(struct evl_io *evlio, int events)
{
	struct evl_work *evl = &evlio->evl_io_work;
	struct evl_base *evlb = evl->evl_base;

	if (!ISSET(evl->evl_event, EVL_PERSIST) &&
	    ISSET(events, EVL_PERSIST)) {
		evl_op_io_del(evlb, evlio);
		CLR(evl->evl_event, EVL_PENDING);
	}

	evl_work_add(evl, ISSET(events, EVL_READ|EVL_WRITE));
}

int
evl_io_del(struct evl_io *evlio)
{
	struct evl_work *evl = &evlio->evl_io_work;
	struct evl_base *evlb = evl->evl_base;
	int rv = 0;

	if (evl_work_del(evl))
		rv = 1;

	if (ISSET(evl->evl_event, EVL_PENDING)) {
		evl_op_io_del(evlb, evlio);
		CLR(evl->evl_event, EVL_PENDING);
		rv = 1;
	}

	return (rv);
}

void
evl_io_destroy(struct evl_io *evlio)
{
	struct evl_base *evlb;

	if (evlio == NULL)
		return;

	evlb = evl_io_base(evlio);

	assert(!ISSET(evlio->evl_io_work.evl_event, EVL_PENDING|EVL_FIRED));

	evl_op_io_destroy(evlb, evlio);

	free(evlio);
}

struct evl_tmo *
evl_tmo_create(struct evl_base *evlb,
    void (*fn)(int, int, void *), void *arg)
{
	struct evl_tmo *evlt;

	evlt = malloc(sizeof(*evlt));
	if (evlt == NULL)
		return (NULL);

	evl_work_init(&evlt->evl_tmo_work, evlb, 0, 0, fn, arg);

printf("%d %s %p\n", getpid(), __func__, evlt);

	return (evlt);
}

int
evl_tmo_add(struct evl_tmo *evlt, const struct timespec *offset)
{
	struct evl_work *evl = &evlt->evl_tmo_work;
	struct evl_base *evlb = evl->evl_base;
	struct timespec now;
	int rv = 0;

printf("%d %s %p\n", getpid(), __func__, evlt);

	if (evl_monotime(&now) == -1)
		return (-1);

	if (evl_work_del(evl))
		;
	else if (ISSET(evl->evl_event, EVL_PENDING))
		evlb_tmo_remove(evlb, evlt);
	else {
		SET(evl->evl_event, EVL_PENDING);
		rv = 1;
	}

	evlb_tmo_insert(evlb, evlt, &now, offset);

	return (rv);
}

int
evl_tmo_pending(const struct evl_tmo *evlt, struct timespec *ts)
{
	const struct evl_work *evl = &evlt->evl_tmo_work;
	struct timespec now;
	int rv = 0;

	if (evl_work_pending(evl)) {
		if (ts != NULL) {
			ts->tv_sec = 0;
			ts->tv_nsec = 0;
		}

		rv = 1;
	} else if (ISSET(evl->evl_event, EVL_PENDING)) {
		if (ts != NULL) {
			if (evl_monotime(&now) == -1)
				return (-1);

			timespecsub(&now, &evlt->evl_tmo_deadline, ts);
		}

		rv = 1;
	}

	return (rv);
}

int
evl_tmo_del(struct evl_tmo *evlt)
{
	struct evl_work *evl = &evlt->evl_tmo_work;
	struct evl_base *evlb = evl->evl_base;
	int rv = 0;

printf("%d %s %p\n", getpid(), __func__, evlt);

	if (evl_work_del(evl))
		rv = 1;
	else if (ISSET(evl->evl_event, EVL_PENDING)) {
		CLR(evl->evl_event, EVL_PENDING);
		evlb_tmo_remove(evlb, evlt);
		rv = 1;
	}

	return (rv);
}

void
evl_tmo_destroy(struct evl_tmo *evlt)
{
	free(evlt);
}

void *
evl_backend(const struct evl_base *evlb)
{
	return (evlb->evlb_backend);
}

static inline int
evl_tmo_compare(const struct evl_tmo *a, const struct evl_tmo *b)
{
	if (a->evl_tmo_deadline.tv_sec > b->evl_tmo_deadline.tv_sec)
		return (1);
	if (a->evl_tmo_deadline.tv_sec < b->evl_tmo_deadline.tv_sec)
		return (-1);
	if (a->evl_tmo_deadline.tv_nsec > b->evl_tmo_deadline.tv_nsec)
		return (1);
	if (a->evl_tmo_deadline.tv_nsec < b->evl_tmo_deadline.tv_nsec)
		return (-1);

	return (0);
}

HEAP_GENERATE(evl_tmo_heap, evl_tmo, evl_tmo_entry, evl_tmo_compare);

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

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "evl-internal.h"

static void	*evl_kq_init(void);
static void	 evl_kq_destroy(void *);
static int	 evl_kq_dispatch(struct evl_base *,
		     const struct timespec *);

static int	 evl_kq_io_create(struct evl_io *);
static void	 evl_kq_io_add(struct evl_io *);
static void	 evl_kq_io_del(struct evl_io *);
static void	 evl_kq_io_destroy(struct evl_io *);

const struct evl_ops evl_ops_kq = {
	evl_kq_init,
	evl_kq_destroy,
	evl_kq_dispatch,
	evl_kq_io_create,
	evl_kq_io_add,
	evl_kq_io_del,
	evl_kq_io_destroy,
};

struct evl_kq {
	int		 evlkq_fd;

	struct kevent	*evlkq_kevents;
	unsigned int	 evlkq_keventslen;
	unsigned int	 evlkq_nevents;
	unsigned int	 evlkq_nchanges;
};

static void *
evl_kq_init(void)
{
	struct evl_kq *evlkq;
	int fd;

	evlkq = malloc(sizeof(*evlkq));
	if (evlkq == NULL)
		return (NULL);

	fd = kqueue();
	if (fd == -1) {
		free(evlkq);
		return (NULL);
	}

	evlkq->evlkq_fd = fd;
	evlkq->evlkq_kevents = NULL;
	evlkq->evlkq_keventslen = 0;
	evlkq->evlkq_nevents = 0;
	evlkq->evlkq_nchanges = 0;

	return (evlkq);
}

static void
evl_kq_destroy(void *backend)
{
	struct evl_kq *evlkq = backend;

	free(evlkq->evlkq_kevents);
	close(evlkq->evlkq_fd);
	free(evlkq);
}

static void
evl_kq_io_fire(const struct kevent *kev, int events)
{
	struct evl_io *evlio;
	struct evl_work *evl;

	if (ISSET(kev->flags, EV_ERROR)) {
		switch (kev->data) {
		case 0:
			/* handle EV_RECEIPT */
			evlio = kev->udata;
			evlio->evl_io_idx = ~0; /* invalidate changes */
			return;
		case EBADF:
			/* this can happen after a close */
			return;
		default:
			/* oh well */
			return;
		}
	}

	evlio = kev->udata;
	evlio->evl_io_idx = ~0; /* invalidate changes */
	evl = &evlio->evl_io_work;

	if (ISSET(evl->evl_event, EVL_RW) != EVL_RW)
		SET(events, ISSET(evl->evl_event, EVL_PERSIST));
	else
		SET(events, EVL_PERSIST);

	evl_io_fire(evlio, events);
}

static int
evl_kq_dispatch(struct evl_base *evlb, const struct timespec *ts)
{
	struct evl_kq *evlkq = evl_backend(evlb);
	struct kevent *kevs = evlkq->evlkq_kevents, *kev;
	int nevents;
	int i;

	nevents = kevent(evlkq->evlkq_fd, kevs, evlkq->evlkq_nchanges,
	    kevs, evlkq->evlkq_keventslen, ts);
	if (nevents == -1) {
		if (errno == EINTR)
			return (0);

		return (-1);
	}

	evlkq->evlkq_nchanges = 0;

	for (i = 0; i < nevents; i++) {
		kev = &kevs[i];

		switch (kev->filter) {
		case EVFILT_READ:
			evl_kq_io_fire(kev, EVL_READ);
			break;
		case EVFILT_WRITE:
			evl_kq_io_fire(kev, EVL_WRITE);
			break;
		}
	}

	return (0);
}

static struct kevent *
evl_kq_next_change(struct evl_kq *evlkq, unsigned int n)
{
	struct kevent *kev;
	unsigned int len = evlkq->evlkq_nevents + n + 1;

	if (len >= evlkq->evlkq_keventslen) {
		kev = reallocarray(evlkq->evlkq_kevents, len, sizeof(*kev));
		if (kev == NULL)
			return (NULL);

		/* commit */
		evlkq->evlkq_kevents = kev;
		evlkq->evlkq_keventslen = len;
	} else
		kev = evlkq->evlkq_kevents;

	return (kev + evlkq->evlkq_nchanges + n);
}

static int
evl_kq_io_create(struct evl_io *evlio)
{
	struct evl_base *evlb = evl_io_base(evlio);
	struct evl_kq *evlkq = evl_backend(evlb);
	struct evl_work *evl = &evlio->evl_io_work;
	struct kevent *kev;
	unsigned int n;
	int flags = EV_ADD | EV_DISABLE | EV_RECEIPT;
	int fd;

	fd = evl->evl_ident;
	n = 0;

	if (ISSET(evl->evl_event, EVL_READ)) {
		kev = evl_kq_next_change(evlkq, n++);
		if (kev == NULL)
			return (-1);

		EV_SET(kev, fd, EVFILT_READ, flags, NOTE_EOF, 0, evlio);
	}

	if (ISSET(evl->evl_event, EVL_WRITE)) {
		kev = evl_kq_next_change(evlkq, n++);
		if (kev == NULL)
			return (-1);

		EV_SET(kev, fd, EVFILT_WRITE, flags, 0, 0, evlio);
	}

	/* commit */
	evlio->evl_io_idx = evlkq->evlkq_nchanges;
	evlkq->evlkq_nchanges += n;
	evlkq->evlkq_nevents += n;

	return (0);
}

static void
evl_kq_io(struct evl_io *evlio, int flags)
{
	struct evl_base *evlb = evl_io_base(evlio);
	struct evl_kq *evlkq = evl_backend(evlb);
	struct evl_work *evl = &evlio->evl_io_work;
	struct kevent *kev;
	unsigned int idx;
	int fd;

	idx = evlio->evl_io_idx;
	if (idx == ~0U) {
		/* new changes */
		idx = evlkq->evlkq_nchanges;
		fd = evl->evl_ident;

		if (ISSET(evl->evl_event, EVL_READ)) {
			kev = evlkq->evlkq_kevents + idx++;
			EV_SET(kev, fd, EVFILT_READ, flags, NOTE_EOF, 0, evlio);
		}

		if (ISSET(evl->evl_event, EVL_WRITE)) {
			kev = evlkq->evlkq_kevents + idx++;
			EV_SET(kev, fd, EVFILT_WRITE, flags, 0, 0, evlio);
		}

		evlkq->evlkq_nchanges = idx;
	} else {
		/* tweak existing changes */
		if (ISSET(evl->evl_event, EVL_READ)) {
			kev = evlkq->evlkq_kevents + idx++;
			CLR(kev->flags, ~EV_ADD);
			SET(kev->flags, flags);
		}

		if (ISSET(evl->evl_event, EVL_WRITE)) {
			kev = evlkq->evlkq_kevents + idx++;
			CLR(kev->flags, ~EV_ADD);
			SET(kev->flags, flags);
		}
	}
}

static void
evl_kq_io_add(struct evl_io *evlio)
{
	struct evl_work *evl = &evlio->evl_io_work;
	int flags = EV_ENABLE | EV_RECEIPT;

	if (ISSET(evl->evl_event, EVL_RW) != EVL_RW &&
	    !ISSET(evl->evl_event, EVL_PERSIST))
		SET(flags, EV_DISPATCH);

	evl_kq_io(evlio, flags);
}

static void
evl_kq_io_del(struct evl_io *evlio)
{
	int flags = EV_DISABLE | EV_RECEIPT;

	evl_kq_io(evlio, flags);
}

static inline unsigned int
evl_kq_nevents(const struct evl_io *evlio)
{
	return (ISSET(evlio->evl_io_work.evl_event, EVL_RW) == EVL_RW ? 2 : 1);
}

static void
evl_kq_io_destroy(struct evl_io *evlio)
{
	struct evl_base *evlb = evl_io_base(evlio);
	struct evl_kq *evlkq = evl_backend(evlb);
	struct evl_work *evl = &evlio->evl_io_work;
	struct kevent *kev, *nkev;
	unsigned int nevents, n, idx;
	int fd;

	idx = evlio->evl_io_idx;
	if (idx == ~0U) { /* new changes */
		idx = evlkq->evlkq_nchanges;
		fd = evl->evl_ident;
		nevents = 0;

		if (ISSET(evl->evl_event, EVL_READ)) {
			kev = evlkq->evlkq_kevents + idx++;
			EV_SET(kev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
			nevents++;
		}

		if (ISSET(evl->evl_event, EVL_WRITE)) {
			kev = evlkq->evlkq_kevents + idx++;
			EV_SET(kev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
			nevents++;
		}

		evlkq->evlkq_nchanges = idx;
	} else {
		/* event is already queued for a change */
		kev = evlkq->evlkq_kevents + idx;
		nevents = evl_kq_nevents(evlio);

		if (ISSET(kev->flags, EV_ADD)) {
			/* event was just added, so remove it from changes */
			evlkq->evlkq_nchanges -= nevents;

			nkev = kev + nevents;
			while (idx < evlkq->evlkq_nchanges) {
				evlio = nkev->udata;
				evlio->evl_io_idx = idx;

				n = evl_kq_nevents(evlio);
				idx += n;

				while (n--)
					*kev++ = *nkev++;
			}

		} else {
			/* update in place */
			for (n = 0; n < nevents; n++)
				kev[n].flags = EV_DELETE;
		}
	}

	evlkq->evlkq_nevents -= nevents;
}

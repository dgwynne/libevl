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

#include <stdlib.h>
#include <stddef.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

#include "evl-internal.h"

static void	*evl_poll_init(void);
static void	 evl_poll_destroy(void *);
static int	 evl_poll_dispatch(struct evl_base *,
		     const struct timespec *);
static int	 evl_poll_io_create(struct evl_io *);
static void	 evl_poll_io_add(struct evl_io *);
static void	 evl_poll_io_del(struct evl_io *);
static void	 evl_poll_io_destroy(struct evl_io *);

const struct evl_ops evl_ops_poll = {
	evl_poll_init,
	evl_poll_destroy,
	evl_poll_dispatch,
	evl_poll_io_create,
	evl_poll_io_add,
	evl_poll_io_del,
	evl_poll_io_destroy,
};

struct evl_pollfd {
	HEAP_ENTRY()	 evlpfd_entry;
	struct evl_io	*evlpfd_io;
	int		 evlpfd_idx;
};
HEAP_HEAD(evl_pollfd_live);
HEAP_HEAD(evl_pollfd_free);

struct evl_poll {
	struct pollfd	 *evlp_pfds;
	struct evl_pollfd **
			  evlp_evlpfds;
	unsigned int	  evlp_len;	/* length of the arrays */
	unsigned int	  evlp_npfds;	/* creates - destroys */
	unsigned int	  evlp_nfds;	/* adds - dels */

	struct evl_pollfd_live
			  evlp_live;
	struct evl_pollfd_free
			  evlp_free;
};

HEAP_PROTOTYPE(evl_pollfd_live, evl_pollfd);
HEAP_PROTOTYPE(evl_pollfd_free, evl_pollfd);

static inline int
evl_pollfd_min_cmp(const struct evl_pollfd *a, const struct evl_pollfd *b)
{
	if (a->evlpfd_idx > b->evlpfd_idx)
		return (1);
	if (a->evlpfd_idx < b->evlpfd_idx)
		return (-1);

	return (0);
}

static inline int
evl_pollfd_max_cmp(const struct evl_pollfd *a, const struct evl_pollfd *b)
{
	if (a->evlpfd_idx < b->evlpfd_idx)
		return (1);
	if (a->evlpfd_idx > b->evlpfd_idx)
		return (-1);

	return (0);
}

HEAP_GENERATE(evl_pollfd_live, evl_pollfd, evlpfd_entry, evl_pollfd_max_cmp);
HEAP_GENERATE(evl_pollfd_free, evl_pollfd, evlpfd_entry, evl_pollfd_min_cmp);

#define evlp_live_init(_evlp)						\
	HEAP_INIT(evl_pollfd_live, &(_evlp)->evlp_live)			
#define evlp_live_first(_evlp) 						\
	HEAP_FIRST(evl_pollfd_live, &(_evlp)->evlp_live)
#define evlp_live_remove(_evlp, _e)					\
	HEAP_REMOVE(evl_pollfd_live, &(_evlp)->evlp_live, (_e));		
#define evlp_live_insert(_evlp, _e)					\
	HEAP_INSERT(evl_pollfd_live, &(_evlp)->evlp_live, (_e));		

#define evlp_free_init(_evlp)						\
	HEAP_INIT(evl_pollfd_free, &(_evlp)->evlp_free)
#define evlp_free_first(_evlp) 						\
	HEAP_FIRST(evl_pollfd_free, &(_evlp)->evlp_free)
#define evlp_free_cextract(_evlp, _k)					\
	HEAP_CEXTRACT(evl_pollfd_free, &(_evlp)->evlp_free, (_k))
#define evlp_free_extract(_evlp)					\
	HEAP_EXTRACT(evl_pollfd_free, &(_evlp)->evlp_free)
#define evlp_free_insert(_evlp, _e)					\
	HEAP_INSERT(evl_pollfd_free, &(_evlp)->evlp_free, (_e))

static void *
evl_poll_init(void)
{
	struct evl_poll *evlp;

	evlp = malloc(sizeof(*evlp));
	if (evlp == NULL)
		return (NULL);

	evlp->evlp_pfds = NULL;
	evlp->evlp_evlpfds = NULL;
	evlp->evlp_len = 0;
	evlp->evlp_npfds = 0;
	evlp->evlp_nfds = 0;

	evlp_live_init(evlp);
	evlp_free_init(evlp);

	return (evlp);
}

static void
evl_poll_destroy(void *backend)
{
	struct evl_poll *evlp = backend;
	struct evl_pollfd *evlpfd;
	unsigned int i;

	for (i = 0; i < evlp->evlp_len; i++) {
		evlpfd = evlp->evlp_evlpfds[i];
		free(evlpfd);
	}

	free(evlp->evlp_evlpfds);
	free(evlp->evlp_pfds);
	free(evlp);
}

static void
evl_poll_pack(struct evl_poll *evlp)
{
	struct evl_pollfd *fevlpfd, *levlpfd;
	struct pollfd *fpfd, *lpfd;
	struct evl_io *evlio;

	while ((levlpfd = evlp_live_first(evlp)) != NULL &&
	    (fevlpfd = evlp_free_cextract(evlp, levlpfd)) != NULL) {
		evlp_live_remove(evlp, levlpfd);

		fpfd = &evlp->evlp_pfds[fevlpfd->evlpfd_idx];
		lpfd = &evlp->evlp_pfds[levlpfd->evlpfd_idx];

		fpfd->fd = lpfd->fd;
		fpfd->events = lpfd->events;

		evlio = levlpfd->evlpfd_io;
		fevlpfd->evlpfd_io = evlio;
		evlio->evl_io_idx = fevlpfd->evlpfd_idx;

		evlp_live_insert(evlp, fevlpfd);
		evlp_free_insert(evlp, levlpfd);
	}
}

static int
evl_poll_dispatch(struct evl_base *evlb, const struct timespec *ts)
{
	struct evl_poll *evlp = evl_backend(evlb);
	nfds_t nfds;
	int n;
	unsigned int i;

	evl_poll_pack(evlp);

	nfds = evlp->evlp_nfds;
	n = ppoll(evlp->evlp_pfds, nfds, ts, NULL);
	if (n == -1) {
		if (errno == EINTR)
			return (0);
		return (-1);
	}

	for (i = 0; i < nfds; i++) {
		struct evl_pollfd *evlpfd;
		struct pollfd *pfd;
		struct evl_io *evlio;
		int event = 0;

		pfd = &evlp->evlp_pfds[i];

		if (ISSET(pfd->revents, POLLHUP|POLLERR))
			SET(event, EVL_READ|EVL_WRITE);
		else {
			if (ISSET(pfd->revents, POLLIN))
				SET(event, EVL_READ);
			if (ISSET(pfd->revents, POLLOUT))
				SET(event, EVL_WRITE);
		}

		evlpfd = evlp->evlp_evlpfds[i];
		evlio = evlpfd->evlpfd_io;
		if (ISSET(evlio->evl_io_work.evl_event, event))
			evl_io_fire(evlio, event | EVL_PERSIST);
			
		if (pfd->revents != 0 && --n == 0)
			break;
	}

	return (0);
}

static int
evl_poll_io_create(struct evl_io *evlio)
{
	struct evl_base *evlb = evl_io_base(evlio);
	struct evl_poll *evlp = evl_backend(evlb);
	struct evl_pollfd *evlpfd;
	unsigned int npfds = evlp->evlp_npfds;
	unsigned int idx;

	idx = npfds++;
	if (npfds > evlp->evlp_len) {
		struct evl_pollfd **evlpfds;
		struct pollfd *pfds;

		evlpfd = malloc(sizeof(*evlpfd));
		if (evlpfd == NULL)
			return (-1);

		evlpfds = reallocarray(evlp->evlp_evlpfds, npfds,
		    sizeof(*evlpfds));
		if (evlpfds == NULL) {
			free(evlpfd);
			return (-1);
		}

		evlp->evlp_evlpfds = evlpfds;

		pfds = reallocarray(evlp->evlp_pfds, npfds, sizeof(*pfds));
		if (pfds == NULL) {
			free(evlpfd);
			return (-1);
		}

		/* commit */
		evlp->evlp_pfds = pfds;
		evlp->evlp_len = npfds;

		evlpfds[idx] = evlpfd;
		evlpfd->evlpfd_idx = idx;

		evlp_free_insert(evlp, evlpfd);
	}

	evlp->evlp_npfds = npfds;

	return (0);
}

static void
evl_poll_io_add(struct evl_io *evlio)
{
	struct evl_base *evlb = evl_io_base(evlio);
	struct evl_poll *evlp = evl_backend(evlb);
	struct evl_work *evl = &evlio->evl_io_work;
	struct evl_pollfd *evlpfd;
	struct pollfd *pfd;
	unsigned int idx;

	evlpfd = evlp_free_extract(evlp);
	idx = evlpfd->evlpfd_idx;

	evlpfd->evlpfd_io = evlio;
	evlio->evl_io_idx = idx;

	pfd = &evlp->evlp_pfds[idx];
	pfd->fd = evl->evl_ident;
	pfd->events = (ISSET(evl->evl_event, EVL_READ) ? POLLIN : 0) |
	    (ISSET(evl->evl_event, EVL_WRITE) ? POLLOUT : 0);

	evlp_live_insert(evlp, evlpfd);
	evlp->evlp_nfds++;
}

static void
evl_poll_io_del(struct evl_io *evlio)
{
	struct evl_base *evlb = evl_io_base(evlio);
	struct evl_poll *evlp = evl_backend(evlb);
	struct evl_pollfd *evlpfd;

	evlp->evlp_nfds--;
	evlpfd = evlp->evlp_evlpfds[evlio->evl_io_idx];
	evlp_live_remove(evlp, evlpfd);
	evlp_free_insert(evlp, evlpfd);
}

static void
evl_poll_io_destroy(struct evl_io *evlio)
{
	struct evl_base *evlb = evl_io_base(evlio);
	struct evl_poll *evlp = evl_backend(evlb);

	evlp->evlp_npfds--;
}

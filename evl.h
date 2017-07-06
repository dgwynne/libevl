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

#ifndef _LIB_EVL_H_
#define _LIB_EVL_H_

struct timespec;

struct evl_base;
struct evl_io;
struct evl_tmo;
#ifdef notyet
struct evl_sig;
struct evl_wait;
#endif
struct evl_work;

struct evl_base		*evl_init(void);
int			 evl_dispatch(struct evl_base *);
void			 evl_break(struct evl_base *);

struct evl_io		*evl_io_create(struct evl_base *, int, int,
			     void (*)(int, int, void *), void *);
void			 evl_io_set(struct evl_io *,
			     void (*)(int, int, void *));
int			 evl_io_fd(const struct evl_io *);
int			 evl_io_add(struct evl_io *);
int			 evl_io_pending(const struct evl_io *);
int			 evl_io_del(struct evl_io *);
void			 evl_io_destroy(struct evl_io *);

struct evl_tmo		*evl_tmo_create(struct evl_base *,
			     void (*)(int, int, void *), void *);
void			 evl_tmo_set(struct evl_tmo *,
			     void (*)(int, int, void *));
int			 evl_tmo_add(struct evl_tmo *, const struct timespec *);
int			 evl_tmo_pending(const struct evl_tmo *,
			     struct timespec *);
int			 evl_tmo_del(struct evl_tmo *);
void			 evl_tmo_destroy(struct evl_tmo *);

#ifdef notyet
struct evl_sig		*evl_sig_create(struct evl_base *, int,
			     void (*)(int, int, void *), void *);
void			 evl_sig_set(struct evl_sig *,
			     void (*)(int, int, void *));
int			 evl_sig_add(struct evl_sig *);
int			 evl_sig_del(struct evl_sig *);
void			 evl_sig_destroy(struct evl_sig *);

struct evl_wait		*evl_wait_create(struct evl_base *, int);
int			 evl_wait_set(struct evl_wait *,
			     void (*)(int, int, void *), void *);
int			 evl_wait_add(struct evl_wait *);
int			 evl_wait_del(struct evl_wait *);
void			 evl_wait_destroy(struct evl_wait *);
#endif

struct evl_work		*evl_work_create(struct evl_base *, int,
			     void (*)(int, int, void *), void *);
void			 evl_work_set(struct evl_work *,
			     void (*)(int, int, void *));
int			 evl_work_add(struct evl_work *, int);
int			 evl_work_pending(const struct evl_work *);
int			 evl_work_del(struct evl_work *);
void			 evl_work_destroy(struct evl_work *);

#define EVL_READ		(1 << 16)
#define EVL_WRITE		(1 << 17)
#define EVL_TIMEOUT		(1 << 18)
#ifdef notyet
#define EVL_SIGNAL		(1 << 19)
#define EVL_WAIT		(1 << 20)
#endif
#define EVL_WORK		(1 << 21)
#define EVL_PERSIST		(1 << 22)

#endif /* _LIB_EVL_H */

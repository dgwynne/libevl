
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

#ifndef _LIB_EVL_OPTIONS_H_
#define _LIB_EVL_OPTIONS_H_

struct evl_ops;

#if 1 || defined(EVL_HAS_KQUEUE)
extern const struct evl_ops evl_ops_kq;
#ifndef EVL_DEFAULT_OPS
#define EVL_DEFAULT_OPS	(&evl_ops_kq)
#endif
#endif

#if 0
extern const struct evl_ops evl_ops_poll;
#ifndef EVL_DEFAULT_OPS
#define EVL_DEFAULT_OPS	(&evl_ops_poll)
#endif
#endif

#endif /* _LIB_EVL_OPTIONS_H_ */

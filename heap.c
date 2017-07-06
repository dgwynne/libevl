/*	$OpenBSD$ */

/*
 * Copyright (c) 2016 David Gwynne <dlg@openbsd.org>
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

#include <sys/_null.h>

#include "heap.h"

static inline struct _heap_entry *
heap_n2e(const struct _heap_type *t, void *node)
{
	unsigned long addr = (unsigned long)node;

	return ((struct _heap_entry *)(addr + t->t_offset));
}

static inline void *
heap_e2n(const struct _heap_type *t, struct _heap_entry *he)
{
	unsigned long addr = (unsigned long)he;

	return ((void *)(addr - t->t_offset));
}

void
_heap_init(struct _heap *h)
{
	h->h_root = NULL;
}

int
_heap_empty(struct _heap *h)
{
	return (h->h_root == NULL);
}

static struct _heap_entry *
_heap_merge(const struct _heap_type *t,
    struct _heap_entry *he1, struct _heap_entry *he2)
{
	struct _heap_entry *hi, *lo;
	struct _heap_entry *child;

	if (he1 == NULL)
		return (he2);
	if (he2 == NULL)
		return (he1);

	if (t->t_compare(heap_e2n(t, he1), heap_e2n(t, he2)) >= 0) {
		hi = he1;
		lo = he2;
	} else {
		lo = he1;
		hi = he2;
	}

	child = lo->he_child;

	hi->he_left = lo;
	hi->he_nextsibling = child;
	if (child != NULL)
		child->he_left = hi;
	lo->he_child = hi;
	lo->he_left = NULL;
	lo->he_nextsibling = NULL;

	return (lo);
}

static inline void
_heap_sibling_remove(struct _heap_entry *he)
{
	if (he->he_left == NULL)
		return;

	if (he->he_left->he_child == he) {
		if ((he->he_left->he_child = he->he_nextsibling) != NULL)
			he->he_nextsibling->he_left = he->he_left;
	} else {
		if ((he->he_left->he_nextsibling = he->he_nextsibling) != NULL)
			he->he_nextsibling->he_left = he->he_left;
	}

	he->he_left = NULL;
	he->he_nextsibling = NULL;
}

static inline struct _heap_entry *
_heap_2pass_merge(const struct _heap_type *t, struct _heap_entry *root)
{
	struct _heap_entry *node, *next = NULL;
	struct _heap_entry *tmp, *list = NULL;

	node = root->he_child;
	if (node == NULL)
		return (NULL);

	root->he_child = NULL;

	/* first pass */
	for (next = node->he_nextsibling; next != NULL;
	    next = (node != NULL ? node->he_nextsibling : NULL)) {
		tmp = next->he_nextsibling;
		node = _heap_merge(t, node, next);

		/* insert head */
		node->he_nextsibling = list;
		list = node;
		node = tmp;
	}

	/* odd child case */
	if (node != NULL) {
		node->he_nextsibling = list;
		list = node;
	}

	/* second pass */
	while (list->he_nextsibling != NULL) {
		tmp = list->he_nextsibling->he_nextsibling;
		list = _heap_merge(t, list, list->he_nextsibling);
		list->he_nextsibling = tmp;
	}

	list->he_left = NULL;
	list->he_nextsibling = NULL;

	return (list);
}

void
_heap_insert(const struct _heap_type *t, struct _heap *h, void *node)
{
	struct _heap_entry *he = heap_n2e(t, node);

	he->he_left = NULL;
	he->he_child = NULL;
	he->he_nextsibling = NULL;

	h->h_root = _heap_merge(t, h->h_root, he);
}

void
_heap_remove(const struct _heap_type *t, struct _heap *h, void *node)
{
	struct _heap_entry *he = heap_n2e(t, node);

	if (he->he_left == NULL) {
		_heap_extract(t, h);
		return;
	}

	_heap_sibling_remove(he);
	h->h_root = _heap_merge(t, h->h_root, _heap_2pass_merge(t, he));
}

void *
_heap_first(const struct _heap_type *t, struct _heap *h)
{
	struct _heap_entry *first = h->h_root;

	if (first == NULL)
		return (NULL);

	return (heap_e2n(t, first));
}

void *
_heap_extract(const struct _heap_type *t, struct _heap *h)
{
	struct _heap_entry *first = h->h_root;

	if (first == NULL)
		return (NULL);

	h->h_root = _heap_2pass_merge(t, first);

	return (heap_e2n(t, first));
}

void *
_heap_cextract(const struct _heap_type *t, struct _heap *h, const void *key)
{
	struct _heap_entry *first = h->h_root;
	void *node;

	if (first == NULL)
		return (NULL);

	node = heap_e2n(t, first);
	if (t->t_compare(node, key) > 0)
		return (NULL);

	h->h_root = _heap_2pass_merge(t, first);

	return (node);
}

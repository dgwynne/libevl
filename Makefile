#	$OpenBSD: Makefile,v 1.43 2016/03/30 06:38:42 jmc Exp $

.include <bsd.own.mk>

LIB=	evl
SRCS=	evl.c
SRCS+=	evl-kqueue.c
#SRCS+=	evl-poll.c
SRCS+=	heap.c
HDRS=	evl.h
MAN=	evl_init.3 evl_io_create.3 evl_tmo_create.3

# use more warnings than defined in bsd.own.mk
CDIAGFLAGS+=	-Wbad-function-cast
CDIAGFLAGS+=	-Wcast-align
CDIAGFLAGS+=	-Wcast-qual
CDIAGFLAGS+=	-Wextra
CDIAGFLAGS+=	-Wmissing-declarations
CDIAGFLAGS+=	-Wuninitialized
CDIAGFLAGS+=	-Wno-unused-parameter

CFLAGS+= -I${.CURDIR} ${CDIAGFLAGS}

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>

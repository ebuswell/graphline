.PHONY: shared static all install-headers install-pkgconfig install-shared install-static install-static-strip install-shared-strip install-all-static install-all-shared install-all-static-strip install-all-shared-strip install install-strip uninstall clean check-shared check-static check

.SUFFIXES: .o .pic.o

include config.mk

VERSION=0.1

OBJS=src/graphline.o
PICOBJS=src/graphline.pic.o
TESTOBJS=src/test.o
HEADER=include/graphline.h

all: shared graphline.pc

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

.c.pic.o:
	${CC} ${CFLAGS} -fPIC -c $< -o $@

libgraphline.so: ${PICOBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -fPIC -shared ${PICOBJS} ${LIBS} -o libgraphline.so

libgraphline.a: ${OBJS}
	rm -f libgraphline.a
	${AR} ${ARFLAGS}c libgraphline.a ${OBJS}

unittest-shared: libgraphline.so ${TESTOBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -Wl,-rpath,`pwd` ${TESTOBJS} ${LIBS} -L`pwd` -lgraphline -o unittest-shared

unittest-static: libgraphline.a ${TESTOBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -static ${TESTOBJS} ${STATIC} -L`pwd` -lgraphline -o unittest-static

graphline.pc: graphline.pc.in config.mk Makefile
	sed -e 's!@prefix@!${PREFIX}!g' \
	    -e 's!@libdir@!${LIBDIR}!g' \
	    -e 's!@includedir@!${INCLUDEDIR}!g' \
	    -e 's!@version@!${VERSION}!g' \
	    graphline.pc.in >graphline.pc

shared: libgraphline.so

static: libgraphline.a

install-headers:
	(umask 022; mkdir -p ${DESTDIR}${INCLUDEDIR})
	install -m 644 ${HEADER} ${DESTDIR}${INCLUDEDIR}/graphline.h

install-pkgconfig: graphline.pc
	(umask 022; mkdir -p ${DESTDIR}${PKGCONFIGDIR})
	install -m 644 graphline.pc ${DESTDIR}${PKGCONFIGDIR}/graphline.pc

install-shared: shared
	(umask 022; mkdir -p ${DESTDIR}${LIBDIR})
	install -m 755 libgraphline.so ${DESTDIR}${LIBDIR}/libgraphline.so.${VERSION}
	ln -frs ${DESTDIR}${LIBDIR}/libgraphline.so.${VERSION} ${DESTDIR}${LIBDIR}/libgraphline.so

install-static: static
	(umask 022; mkdir -p ${DESTDIR}${LIBDIR})
	install -m 644 libgraphline.a ${DESTDIR}${LIBDIR}/libgraphline.a

install-shared-strip: install-shared
	strip --strip-unneeded ${DESTDIR}${LIBDIR}/libgraphline.so.${VERSION}

install-static-strip: install-static
	strip --strip-unneeded ${DESTDIR}${LIBDIR}/libgraphline.a

install-all-static: static graphline.pc install-static install-headers install-pkgconfig

install-all-shared: shared graphline.pc install-shared install-headers install-pkgconfig

install-all-shared-strip: install-all-shared install-shared-strip

install-all-static-strip: install-all-static install-static-strip

install: install-all-shared

install-strip: install-all-shared-strip

uninstall: 
	rm -f ${DESTDIR}${LIBDIR}/libgraphline.so.${VERSION}
	rm -f ${DESTDIR}${LIBDIR}/libgraphline.so
	rm -f ${DESTDIR}${LIBDIR}/libgraphline.a
	rm -f ${DESTDIR}${PKGCONFIGDIR}/graphline.pc
	rm -f ${DESTDIR}${INCLUDEDIR}/graphline.h

clean:
	rm -f graphline.pc
	rm -f libgraphline.so
	rm -f libgraphline.a
	rm -f ${OBJS}
	rm -f ${PICOBJS}
	rm -f ${TESTOBJS}
	rm -f unittest-shared
	rm -f unittest-static

check-shared: unittest-shared
	./unittest-shared

check-static: unittest-static
	./unittest-static

check: check-shared

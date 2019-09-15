# Makefile - requires GNU make
# try running gmake instead of make

# directories
BINROOT = /usr/local/bin
MANROOT = /usr/local/man/man1

# tools
CC = gcc
INSTALL = install
MKDIR = mkdir
AR = ar

# flags
CFLAGS = -Wall -I. -L.
LDFLAGS =
ARFLAGS = rvc

### all
all: hexdiff

### options
opt:
	@echo "### CC      = ${CC}"
	@echo "### CFLAGS  = ${CFLAGS}"
	@echo "### LDFLAGS = ${LDFLAGS}"
	@echo "### BINROOT = ${BINROOT}"
	@echo "### MANROOT = ${MANROOT}"
	@echo "### INSTALL = ${INSTALL}"

# object files
OBJS = llq.o sbuf.o sbuf_diff.o sbuf_cache.o llq_num.o

### object files
llq.o: llq.c llq.h
	${CC} ${CFLAGS} -c llq.c -o llq.o

sbuf.o: sbuf.c sbuf.h
	${CC} ${CFLAGS} -c sbuf.c -o sbuf.o

sbuf_diff.o: sbuf_diff.c sbuf_diff.h
	${CC} ${CFLAGS} -c sbuf_diff.c -o sbuf_diff.o

sbuf_cache.o: sbuf_cache.c sbuf_cache.h
	${CC} ${CFLAGS} -c sbuf_cache.c -o sbuf_cache.o

llq_num.o: llq_num.c llq_num.h
	${CC} ${CFLAGS} -c llq_num.c -o llq_num.o

### program
hexdiff: ${OBJS} hexdiff.c
	@echo "### hexdiff"
	${CC} ${CFLAGS} ${OBJS} hexdiff.c ${LDFLAGS} -o $@

### install
install: hexdiff
	@echo "### install"
	${MKDIR} -m 0755 -p ${BINROOT}
	${INSTALL} -m 0755 hexdiff ${BINROOT}/hexdiff
	${MKDIR} -m 0755 -p ${MANROOT}
	${INSTALL} -m 0644 hexdiff.1 ${MANROOT}/hexdiff.1

### uninstall
uninstall:
	@echo "### uninstall"
	rm -f ${BINROOT}/hexdiff
	rm -f ${MANROOT}/hexdiff.1

### clean
clean:
	@echo "### clean"
	rm -f *.o
	rm -f hexdiff

#lhsm-restore: lhsm-restore.c
#	gcc -o lhsm-restore -std=c99 -Wall lhsm-restore.c -llustreapi -lpthreads

TARGET=lhsm-restore

DIR_TOP=$(PWD)
DIR_ZLOG=$(DIR_TOP)/zlog
DIR_ZLOG_SRC=$(DIR_ZLOG)/src

INC_SRCH_PATH :=
INC_SRCH_PATH += $(DIR_ZLOG_SRC)

LIB_SRCH_PATH :=
LIB_SRCH_PATH += $(DIR_ZLOG_SRC)

LD := ld
MAKE := make


SUBDIRS = zlog

subdirs: $(SUBDIRS)

$(SUBDIRS):
	cd $@ && make


CFLAGS   = -std=c99 -Wall
LDFLAGS +=-L$(LIB_SRCH_PATH)
LIBS    := -lpthread -lcrypto $(DIR_ZLOG_SRC)/libzlog.a


.PHONY: all clean $(SUBDIRS)

all: $(TARGET)

HEADERS=$(wildcard *.h)
SOURCES =$(wildcard *.c)

#%o: %c $(HEADERS) Makefile
#	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): subdirs $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LIBS) -o $@

clean:
	rm -rf *.o
	rm -rf $(TARGET)
	cd zlog/src && make clean


# lhsm-restore: lhsm-restore.c lhsm-restore-stub.h
# 	gcc -g -o lhsm-restore -std=c99 -pthread -lcrypto -Wall lhsm-restore.c

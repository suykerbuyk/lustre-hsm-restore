#lhsm-restore: lhsm-restore.c
#	gcc -o lhsm-restore -std=c99 -Wall lhsm-restore.c -llustreapi -lpthreads
lhsm-restore: lhsm-restore.c lhsm-restore-stub.h
	gcc -g -o lhsm-restore -std=c99 -pthread -lcrypto -Wall lhsm-restore.c

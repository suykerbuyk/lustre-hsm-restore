lhsm-restore: lhsm-restore.c
	gcc -o lhsm-restore -std=c99 -Wall lhsm-restore.c -llustreapi -lpthreads

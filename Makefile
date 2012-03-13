effs_test: effs.o fisk.o effs_test.o
	gcc -g -Wall effs.o fisk.o effs_test.o -o effs_test

#effs_shell: effs.o fisk.o shell.o
#	gcc -g -Wall effs.o fisk.o shell.o -o effs_shell

effs_test.o: effs_test.c
	gcc -c -g -Wall effs_test.c

shell.o: shell.c
	gcc -c -g -Wall shell.c

effs.o: effs.c
	gcc -c -g -Wall effs.c

fisk.o: fisk.c
	gcc -c -g -Wall fisk.c

clean:
	rm -f *.o
	rm -f diskfile
	rm -f effs
	rm -f effs_test
	rm -f effs_shell

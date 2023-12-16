CFLAG = -Wall

EXEC = tema3

build: tema3.o peer.o tracker.o
	mpicc ${CFLAG} -o ${EXEC} $?

tema3.o: tema3.c
	mpicc ${CFLAG} -c -o $@ $?

peer.o: peer.c
	mpicc ${CFLAG} -c -o $@ $?

tracker.o: tracker.c
	mpicc ${CFLAG} -c -o $@ $?

clean:
	rm -rf ${EXEC} *.o

CC=gcc
CFLAGS=-g -O2 -I./src -I./include
AR=ar
OBJ=./src/

STACKSIZE=-DCORO_STACK_SIZE=4096
TRACINGON=-DCORO_ENABLE_TRACE

all:
	$(CC) $(CFLAGS) $(STACKSIZE) $(TRACINGON) -o ./src/context.o -c ./src/context.S  
	$(CC) $(CFLAGS) $(STACKSIZE) $(TRACINGON) -o ./src/greencoro.o -c ./src/greencoro.c 
	$(CC) $(CFLAGS) $(STACKSIZE) $(TRACINGON) -o ./src/tracer.o -c ./src/tracer.c
	$(AR) cr ./lib/libgreencoro.a ./src/*.o
	ls ./src/*.o > /dev/null 2>&1 && rm ./src/*.o

clean:
	ls ./lib/libgreencoro.a > /dev/null 2>&1 && rm ./lib/libgreencoro.a

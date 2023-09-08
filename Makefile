CC = gcc
CFLAGS = -std=c99 -g -Wall -pthread -fsanitize=address,undefined

all: ttts ttt_1 ttt_2

ttt_1: ttt_1.o protocol.o
	$(CC) $(CFLAGS) $^ -o $@

ttt_1.o protocol.o: protocol.h

ttt_2: ttt_2.o protocol.o
	$(CC) $(CFLAGS) $^ -o $@

ttt_2.o protocol.o: protocol.h

ttts: ttts.o protocol.o
	$(CC) $(CFLAGS) $^ -o $@

ttts.o protocol.o: protocol.h

clean:
	rm -rf *.o ttt_1 ttt_2 ttts
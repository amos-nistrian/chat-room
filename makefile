#
#comments --here--
#
#

CC = gcc
CFLAGS =  -g -fmax-errors=2  -Wall # wall means show all warnings, -g: -fmax-errors=2 sets the max errors to be displayed to the first 2
LDFLAGS = -g -lm
OBJECTS = prog3_server.o prog3_participant.o prog3_observer.o trie.o
EXES = prog3_server prog3_participant prog3_observer

prog3_server:	prog3_server.o trie.o
	$(CC) prog3_server.c trie.c -o prog3_server $(LDFLAGS)

prog3_participant:	prog3_participant.o
	$(CC) prog3_participant.c -o prog3_participant $(LDFLAGS)

prog3_observer:	prog3_observer.o
	$(CC) prog3_observer.c -o prog3_observer $(LDFLAGS)

all:
	$(MAKE) $(EXES)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXES)

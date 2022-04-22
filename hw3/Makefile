SRCS=mm_alloc.c mm_test.c
EXECUTABLES=malloc_test

CC=gcc
CFLAGS=-g -Wall
LDFLAGS=

OBJS=$(SRCS:.c=.o)

all: $(EXECUTABLES) run

run: malloc_test
	./malloc_test

$(EXECUTABLES): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@  

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(EXECUTABLES) $(OBJS)

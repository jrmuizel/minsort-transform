CC = gcc
CFLAGS = -O2 -Wall -DNDEBUG
#CFLAGS = -ggdb
LDFLAGS = -lJudy

all: minsort_rev_transform

minsort_rev_transform: minsort_rev_transform.o map_file.o
	$(CC) -o $@ $+ $(LDFLAGS)

install: minsort_rev_transform
	cp minsort_rev_transform $(HOME)/bin/

clean:
	$(RM) *.o minsort_rev_transform

CC = g++

CFLAGS = -Wall -Werror -pedantic -g -O3

SRC = \
	xmlparse.cc \
	index_state.cc

TST = \

HDR = $(SRC:.cc=.hh)
OBJ = $(SRC:.cc=.o)

all: indexer

test: $(TST)

%.o: %.cc %.hh
	$(CC) -c $(CFLAGS) -o $@ $<

test_%: $(OBJ) test_%.cc
	$(CC) $(CFLAGS) -o $@ $^

indexer: $(OBJ) indexer.cc
	$(CC) $(CFLAGS) -o $@ $^

DSYM = $(addsuffix .dSYM, $(TST) indexer)
clean:
	rm -rfv indexer $(TST) $(DSYM) $(OBJ)


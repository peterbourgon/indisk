CC = g++

CFLAGS = -Wall -Werror -pedantic -lpthread -g # -O3

SRC = \
	definitions.cc \
	xmlparse.cc \
	index_state.cc \
	thread.cc \

TST = \

HDR = $(SRC:.cc=.hh)
OBJ = $(SRC:.cc=.o)

all: indexer reader

test: $(TST)

%.o: %.cc %.hh
	$(CC) -c $(CFLAGS) -o $@ $<

test_%: $(OBJ) test_%.cc
	$(CC) $(CFLAGS) -o $@ $^

indexer: $(OBJ) indexer.cc
	$(CC) $(CFLAGS) -o $@ $^

reader: $(OBJ) reader.cc
	$(CC) $(CFLAGS) -o $@ $^

DSYM = $(addsuffix .dSYM, $(TST) indexer reader)
clean:
	rm -rfv indexer reader $(TST) $(DSYM) $(OBJ)


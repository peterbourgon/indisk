CC = g++

CFLAGS = -Wall -Werror -pedantic -O3

LIB = -lpthread

SRC = \
	def.cc \
	xml.cc \
	idx.cc \
	thread.cc \

TST = \
	test_stream \
	test_idx \

HDR = $(SRC:.cc=.hh)
OBJ = $(SRC:.cc=.o)

all: indexer reader

test: $(TST)

%.o: %.cc %.hh
	$(CC) -c $(CFLAGS) -o $@ $<

test_%: $(OBJ) test_%.cc
	$(CC) $(CFLAGS) $(LIB) -o $@ $^

indexer: $(OBJ) indexer.cc
	$(CC) $(CFLAGS) $(LIB) -o $@ $^

reader: $(OBJ) reader.cc
	$(CC) $(CFLAGS) $(LIB) -o $@ $^

debug_indexer:
	g++ -ggdb -o indexer def.cc xml.cc idx.cc thread.cc indexer.cc

DSYM = $(addsuffix .dSYM, $(TST) indexer reader)
clean:
	rm -rfv indexer reader $(TST) $(DSYM) $(OBJ)


CC = g++

CFLAGS = -Wall -Werror -pedantic -O3

LIB = -lpthread

SRC = \
	def.cc \
	xml.cc \
	idx.cc \
	search.cc \
	thread.cc \

MOD = \
	pymodule.cc \

TST = \
	test_stream \
	test_idx \

HDR = $(SRC:.cc=.hh)
OBJ = $(SRC:.cc=.o)

# TODO conditional for Linux
LFLAGS = -Wall -Werror -O3 -dynamiclib
PYTHON_MODULE = indisk.dylib
FINAL_PYTHON_MODULE = $(PYTHON_MODULE:.dylib=.so)

all: indexer reader $(PYTHON_MODULE) $(TST)

test: $(TST)

%.o: %.cc %.hh
	$(CC) -c $(CFLAGS) -o $@ $<

test_%: $(OBJ) test_%.cc
	$(CC) $(CFLAGS) $(LIB) -o $@ $^

indexer: $(OBJ) indexer.cc
	$(CC) $(CFLAGS) $(LIB) -o $@ $^

reader: $(OBJ) reader.cc
	$(CC) $(CFLAGS) $(LIB) -o $@ $^

$(PYTHON_MODULE): $(OBJ) $(MOD)
	# unfortunately can't be -pedantic, because Python.h is not strictly valid
	$(CC) $(LFLAGS) -lpython2.7 -o $@ $^
	mv $(PYTHON_MODULE) $(FINAL_PYTHON_MODULE)

debug_indexer:
	g++ -ggdb -o indexer def.cc xml.cc idx.cc thread.cc indexer.cc

debug_test_idx:
	g++ -ggdb -o test_idx def.cc xml.cc idx.cc thread.cc test_idx.cc

DSYM = $(addsuffix .dSYM, $(TST) indexer reader)
clean:
	rm -rf indexer reader 
	rm -rf $(TST) $(DSYM) $(OBJ) 
	rm -rf $(FINAL_PYTHON_MODULE)



CXXFLAGS = -O0 -W -Wall -ggdb
CXX = g++

# LDFLAGS=-m32 -static -D_FILE_OFFSET_BITS=64
LDFLAGS=-static -D_FILE_OFFSET_BITS=64 -ggdb -O0

FILES = fstest.o dir.o file.o filesystem.o

all: fstest

fstest: $(FILES)
	$(CXX) $(LDFLAGS) -o $@ $(FILES)

fstest.o: fstest.cc

clean:
	rm -f fstest *.o

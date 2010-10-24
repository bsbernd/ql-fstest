
CXXFLAGS = -O2 -W -Wall -ggdb 
CXX = g++

# LDFLAGS=-m32 -static -D_FILE_OFFSET_BITS=64
LDFLAGS=-D_FILE_OFFSET_BITS=64 -ggdb -O2 -lpthread

FILES = fstest.o dir.o file.o filesystem.o

all: fstest

fstest: $(FILES)
	$(CXX) $(LDFLAGS) -o $@ $(FILES)

fstest.o: fstest.cc

clean:
	rm -f fstest *.o

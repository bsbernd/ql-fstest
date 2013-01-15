
CXXFLAGS = -O2 -W -Wall -ggdb  -DFORTIFY_SOURCE=2 #-DDEBUG=1
CXX = g++

# LDFLAGS=-m32 -static -D_FILE_OFFSET_BITS=64
LDFLAGS=-D_FILE_OFFSET_BITS=64 -ggdb -O2 -lpthread

FILES = fstest.o dir.o file.o filesystem.o

all: fstest

fstest: $(FILES)
	$(CXX) -o $@ $(FILES) $(LDFLAGS)

fstest.o: fstest.cc

clean:
	rm -f fstest *.o

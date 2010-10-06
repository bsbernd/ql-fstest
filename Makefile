all: fstest

CXXFLAGS = -O2 -W -Wall -g 
CXX=g++

# LDFLAGS=-m32 -static -D_FILE_OFFSET_BITS=64
LDFLAGS=-static -D_FILE_OFFSET_BITS=64

fstest: fstest.o
	$(CXX) $(LDFLAGS) -o $@ $+

fstest.o: fstest.cc

clean:
	rm -f fstest fstest.o

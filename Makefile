FLAGS        = -rdynamic -O2 -W -Wall -ggdb3  -DFORTIFY_SOURCE=2 #-DDEBUG=1
FFLAGS_DEBUG = -rdynamic -O0 -W -Wall -ggdb3 -DFORTIFY_SOURCE=2 -DDEBUG=1
CXX ?= g++

# LDFLAGS=-m32 -static -D_FILE_OFFSET_BITS=64
LDFLAGS=-D_FILE_OFFSET_BITS=64 -ggdb -O2 -lpthread

FILES = fstest.cc dir.cc file.cc filesystem.cc

all: fstest

fstest: $(FILES)
	$(CXX) -o $@ $(FILES) $(FLAGS) $(LDFLAGS)

debug: $(FILES)
	$(CXX) $(FFLAGS_DEBUG) -o fstest $(FILES) $(FFLAGS_DEBUG) $(LDFLAGS)

fstest.o: fstest.cc

clang_check:
	scan-build make 
	make clean
	scan-build --use-c++=/usr/bin/clang++ make 

clean:
	rm -f fstest *.o



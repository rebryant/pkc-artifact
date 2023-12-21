CC=gcc
CXX=g++
OPT=-O2
#OPT=-O0
CFLAGS=-g $(OPT) 
CPPFLAGS=-g $(OPT) -std=c++11 -I..

UFILES= utils/Options.o utils/System.o
CFILES= core/Solver.o
MFILE = core/Main.o
LIB=glucose.a
TARG=glucose
LIBS = $(LIB) -lz

all: lib prog

lib: $(LIB)

$(LIB):
	cd utils ; make 
	cd core ; make 
	ar cr $(LIB) $(UFILES) $(CFILES)
	cp -p core/Solver.h .

prog: $(TARG)

$(TARG): $(LIB)
	cd utils ; make 
	cd core ; make 
	$(CXX) $(CFLAGS) -o $(TARG) $(MFILE) $(LIBS)

all: $(FILES)

clean:
	cd utils ; make clean 
	cd core ; make clean 
	rm -f *.o *~
	rm -f $(LIB) $(TARG) Solver.h




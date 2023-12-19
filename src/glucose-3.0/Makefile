CC=gcc
CXX=g++
OPT=-O2
#OPT=-O0
CFLAGS=-g $(OPT) -Wno-nullability-completeness
CPPFLAGS=-g $(OPT) -Wno-nullability-completeness -std=c++11 -Wno-reserved-user-defined-literal -I..

UFILES= utils/Options.o utils/System.o
CFILES= core/Solver.o
MFILE = core/Main.o
LIB=glucose.a
TARG=glucose
LIBS = $(LIB) -lz

all: lib prog

lib: $(LIB)

$(LIB):
	pushd utils ; make ; popd
	pushd core ; make ; popd
	ar cr $(LIB) $(UFILES) $(CFILES)
	cp -p core/Solver.h .

prog: $(TARG)

$(TARG): $(LIB)
	pushd utils ; make ; popd
	pushd core ; make ; popd
	$(CXX) $(CFLAGS) -o $(TARG) $(MFILE) $(LIBS)

all: $(FILES)

clean:
	pushd utils ; make clean ; popd
	pushd core ; make clean ; popd
	rm -f *.o *~
	rm -f $(LIB) $(TARG) Solver.h




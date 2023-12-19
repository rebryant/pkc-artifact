CC=gcc
CXX=g++
OPT=-O2
#OPT=-O0
CFLAGS=-g $(OPT) -Wno-nullability-completeness
CPPFLAGS=-g $(OPT) -Wno-nullability-completeness -std=c++11 -Wno-reserved-user-defined-literal -I..
GDIR = glucose-3.0
GINC = -I $(GDIR)
LIBS =  $(GDIR)/glucose.a -lz

all:  pkc

path.h: find_path.sh
	./find_path.sh > path.h

$(GDIR)/Solver.h:
	pushd $(GDIR); make lib

$(GDIR)/glucose.a:
	pushd $(GDIR); make lib

report.o: report.h report.c path.h
	$(CC) $(CFLAGS) -c report.c

counters.o: counters.h counters.c
	$(CC) $(CFLAGS) -c counters.c

q25.o: q25.h q25.c
	$(CC) $(CFLAGS) -c q25.c

files.o: files.hh report.h files.cpp
	$(CXX) $(CPPFLAGS) $(GINC) -c files.cpp 

pog.o: pog.hh counters.h report.h pog.cpp
	$(CXX) $(CPPFLAGS) $(GINC) -c pog.cpp 

compile.o: compile.hh pog.hh counters.h report.h files.hh compile.cpp $(GDIR)/Solver.h
	$(CXX) $(CPPFLAGS) $(GINC) -c compile.cpp

project.o: project.hh pog.hh compile.hh report.h counters.h files.hh project.cpp
	$(CXX) $(CPPFLAGS) $(GINC) -c project.cpp

pkc: pkc.cpp files.o report.o compile.o counters.o pog.o project.o q25.o $(GDIR)/glucose.a
	$(CXX) $(CPPFLAGS) $(GINC) -o pkc pkc.cpp files.o report.o compile.o counters.o pog.o project.o q25.o $(LIBS)

.SUFFIXES: .c .cpp .o

.c.o:
	$(CC) $(CFLAGS) $(INC) -c $<

.cpp.o:
	$(CXX) $(CPPFLAGS) $(INC) -c $<

clean:
	cd $(GDIR); make clean
	rm -f *.o *~
	rm -f pkc
	rm -rf *.dSYM
	rm -f path.h



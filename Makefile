# Top-level Makefile

install:
	cd src ; make all

test:
	cd test-data ; make test 

clean:
	cd src ; make clean 
	cd tools ; make clean 
	cd benchmarks ; make clean 
	cd test-data ; make clean 
	rm -f *.o *~




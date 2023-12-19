# Top-level Makefile

install:
	pushd src ; make all

clean:
	pushd src ; make clean ; popd
	pushd tools ; make clean ; popd
	pushd benchmarks ; make clean ; popd
	rm -f *.o *~




# Top-level Makefile

install:
	pushd src ; make all; popd

test:
	pushd test-data ; make test ; popd

clean:
	pushd src ; make clean ; popd
	pushd tools ; make clean ; popd
	pushd benchmarks ; make clean ; popd
	pushd test-data ; make clean ; popd
	rm -f *.o *~




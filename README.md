# Artifact version of PKC projecting knowledge compiler

This is an artifact version of the PKC projecting knowledge
compiler.  It includes source code for PKC plus data from the paper

  R.E. Bryant, ``Algorithms for Projected Knowledge Compilation,''
  TACAS 2024.

Included are both the actual data used in the paper, as well as the
ability to reproduce all or parts of that data.

ACCESSING

The most recent version of this artifact is maintained as:

    https://github.com/rebryant/pkc-artifact

INSTALLATION

Creating an executable version of PKC requires compiling the provided
code as well as obtaining an executable version of the D4 knowledge
compiler, version 2 (d4v2)  The provided code can be compiled by running
"make install"

There are two ways to obtain d4v2:

1. Use the provided executable.  This artifact includes a statically
   linked executable compiled for 64-bit Ubuntu Linux.  This should
   run without any special installation steps on a compatible Linux
   machine, including the TACAS 23 virtual machine.

2. Download and compile the code from github:
   	    git clone https://github.com/crillab/d4v2

   Follow their instructions for compiling the code.  It requires
   installing the ninja package manager and the boost and gmp libraries.

   Put a symbolic link named "d4v2" to the executable program either
   somewhere covered by the PATH environment variable (e.g.,
   /usr/local/bin), or by replacing the file "d4v2" in the src
   subdirectory with a symbolic link named "d4v2".

TESTING

After installation, running "make test" will run PKC on some simple
examples in the "test-data" subdirectory.

RUNNING

Two sets of benchmarks are supplied, as described in the benchmark
subdirectory.  PKC can be run either in limited "demo" form, or
to generate the full data presented in the paper.

The following are the approximate resource requirements for running
the program on the two benchmark sets:

  BENCH                         DEMO         FULL
  Majority                      10 minutes      4 hours
  Model Counting Competitions    5 minutes    170 hours

EXAMINING DATA

The raw data files for the benchmark runs are also in the benchmark
subdirectory.

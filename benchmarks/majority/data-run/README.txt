This directory contains scripts to run the PKC projecting knowledge
compiler on a set of scalable benchmarks encoding the Majority
function.  The paper summarizes our results in Fig. 3.

The Majority function for N Boolean inputs yields 1 when > N/2 of the
inputs are set to 1.  Two different encodings of the function are
tested.  Both use O(N^2) intermediate encoding variables that get
projected away.

* Tseitin (majt-N).  Every encoding variable uses a full Tseitin
  encoding.  This requires up to 4 clauses per variable.  Six
  different methods of compiling these are tested.

* Plaisted-Greenbaum (majp-N).  A more optimized encoding, requiring
  at most 2 clauses per variable.  Five different methods of compiling
  these are tested.

Two different tests are supported:

* "make demo": Run for each compilation method for increasing values of N, stopping
  once a time limit of 10 seconds per compilation is exceeded.  The results
  are summarized by the files tseitin-summary.csv and plaisted-summary.csv

* "make full": Run for each compilation method for increasing values of N,
  stopping once a time limit of 1000 seconds per compilation is
  exceeded.  This reproduces the results of the paper.  The results
  are summarized by the files tseitin-summary.csv and
  plaisted-summary.csv

Running "make clean" removes all generated files.

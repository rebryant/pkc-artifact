This directory contains data and demonstration code for running the
PKC projecting knowledge compiler on a set of scalable benchmarks
encoding the Majority function.

The Majority function for N Boolean inputs yields 1 when > N/2 of the
inputs are set to 1.  Two different encodings of the function are
tested.  Both use O(N^2) intermediate encoding variables that get
projected away.

Subdirectories:

data-run:
	Run PKC and summarize the results for these benchmarks.  This
	can either be a brief demonstration run ("make demo") or the
	full generation ("make full") of the data presented in the
	paper.

data-paper:
	Raw data used to generate Fig. 3 of the paper


This directory contains the raw data from running the PKC projecting
knowledge compiler on a set of scalable benchmarks encoding the
Majority function.  The data included here is the basis for Fig. 3 in
the paper.

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

The raw data from the program runs are provided in files with the
following naming conventions, where N is the scaling parameter:

TSEITIN
	Split-D:   majt-N.pkc_tnone_purify_defer_d4v2_log
	Split-DT:  majt-N.pkc_tdetect_purify_defer_d4v2_log
	Repair-A:  majt-N.pkc_tnone_purify_incr_d4v2_log
	Repair-B:  majt-N.pkc_tdetect_purify_incr_d4v2_log
	Recompile: majt-N.pkc_tnone_purify_mono_d4v2_log
	Trim:      majt-N.pkc_tnone_purify_trim_d4v2_log

PLAISTED-GREENBAUM
	Split-D:   majp-N.pkc_tnone_purify_defer_d4v2_log
	Split-DT:  majp-N.pkc_tpromote_purify_defer_d4v2_log
	Repair-A:  majp-N.pkc_tnone_purify_incr_d4v2_log
	Repair-B:  majp-N.pkc_tpromote_purify_incr_d4v2_log
	Recompile: majp-N.pkc_tnone_purify_mono_d4v2_log

Running "make data" generates summaries of the results in files as
files tseitin-summary.csv and plaisted-summary.csv.

Running "make clean" removes all CSV files.

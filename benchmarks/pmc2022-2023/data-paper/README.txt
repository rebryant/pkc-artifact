This directory contains the raw data from running the PKC projecting
knowledge compiler on a subset of the formulas used in the 2022 and
2023 projected model counting competitions.

The raw data from the program runs are provided in files with the
following naming conventions, where BENCH is the benchmark formula name.

	Split-D:   BENCH.pkc_tnone_purify_defer_d4v2_log
	Split-DT:  BENCH.pkc_tpromote_purify_defer_d4v2_log
	Repair-A:  BENCH.pkc_tnone_purify_incr_d4v2_log
	Repair-B:  BENCH.pkc_tpromote_purify_incr_d4v2_log
	Recompile: BENCH.pkc_tnone_purify_mono_d4v2_log

Running "make data" generates summaries of the results as files
seconds-summary.csv and clauses-summary.csv.

Running "make clean" removes all CSV files.

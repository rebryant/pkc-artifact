INTERP = python3
HDIR = ../../..
TOOLS = $(HDIR)/tools
DRIVE = $(TOOLS)/pkc_drive.py
GRAB = $(TOOLS)/grab_data.py
MERGE = $(TOOLS)/merge_csv.py
SHOW = $(TOOLS)/show_csv.py

# Demonstration of benchmark
demo:	drun data
	echo "ELAPSED TIME (seconds)"
	$(INTERP) $(SHOW) seconds-summary.csv
	echo "COMPILED OUTPUT SIZES (clauses)"
	$(INTERP) $(SHOW) clauses-summary.csv

# Reproduce results of paper
full:	frun data
	echo "ELAPSED TIME (seconds)"
	$(INTERP) $(SHOW) seconds-summary.csv
	echo "COMPILED OUTPUT SIZES (clauses)"
	$(INTERP) $(SHOW) clauses-summary.csv

clean:
	rm -f *.csv *~ zzzz*
	rm -f *log

# Run subset of formulas
drun:
	echo "D4"
	$(INTERP) $(DRIVE) -X -x -P 0 -T n -m c -l run15.txt
	echo "Split-D"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m d -l run15.txt
	echo "Split-DT"
	$(INTERP) $(DRIVE) -X -x -P 2 -T p -m d -l run15.txt
	echo "Recompile"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m m -l run15.txt
	echo "Repair"
	$(INTERP) $(DRIVE) -X -x -P 2 -T p -m i -l run15.txt

# Run full set of formulas
frun:
	echo "D4"
	$(INTERP) $(DRIVE) -X -x -P 0 -T n -m c -l run124.txt
	echo "Split-D"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m d -l run124.txt
	echo "Split-DT"
	$(INTERP) $(DRIVE) -X -x -P 2 -T p -m d -l run124.txt
	echo "Recompile"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m m -l run124.txt
	echo "Repair"
	$(INTERP) $(DRIVE) -X -x -P 2 -T p -m i -l run124.txt


data:
# D4
	$(INTERP) $(GRAB) "Time TOTAL" *.pkc_tnone_nopre_compile_d4v2_log > seconds-d4.csv
	$(INTERP) $(GRAB) "Final POG Clauses" *.pkc_tnone_nopre_compile_d4v2_log > clauses-defining.csv
	$(INTERP) $(MERGE) -f seconds-d4.csv clauses-defining.csv > d4-results.csv

# PKC Recompile (=mono)
	$(INTERP) $(GRAB) "Time TOTAL" *.pkc_tnone_purify_mono_d4v2_log > seconds-pkc-recompile.csv
	$(INTERP) $(GRAB) "Final POG Clauses" *.pkc_tnone_purify_mono_d4v2_log > clauses-pkc-recompile.csv
	$(INTERP) $(MERGE) -d $(TOUT) seconds-d4.csv seconds-pkc-recompile.csv > seconds-summary-recompile.csv
	$(INTERP) $(MERGE) -f -t -2:1:1 clauses-defining.csv clauses-pkc-recompile.csv > clauses-summary-recompile.csv

# PKC Repair (=incr)
	$(INTERP) $(GRAB) "Time TOTAL" *.pkc_tpromote_purify_incr_d4v2_log > seconds-pkc-repair.csv
	$(INTERP) $(GRAB) "Final POG Clauses" *.pkc_tpromote_purify_incr_d4v2_log > clauses-pkc-repair.csv
	$(INTERP) $(MERGE) -d $(TOUT) seconds-d4.csv seconds-pkc-repair.csv > seconds-summary-repair.csv
	$(INTERP) $(MERGE) -f -t -2:1:1 clauses-defining.csv clauses-pkc-repair.csv > clauses-summary-repair.csv

# PKC SplitD (=defer)
	$(INTERP) $(GRAB) "Time TOTAL" *.pkc_tnone_purify_defer_d4v2_log > seconds-pkc-splitd.csv
	$(INTERP) $(GRAB) "Final POG Clauses" *.pkc_tnone_purify_defer_d4v2_log > clauses-pkc-splitd.csv
	$(INTERP) $(MERGE) -d $(TOUT) seconds-d4.csv seconds-pkc-splitd.csv > seconds-summary-splitd.csv
	$(INTERP) $(MERGE) -f -t -2:1:1 clauses-defining.csv clauses-pkc-splitd.csv > clauses-summary-splitd.csv

# PKC SplitDT (=tdetect_defer)
	$(INTERP) $(GRAB) "Time TOTAL" *.pkc_tpromote_purify_defer_d4v2_log > seconds-pkc-splitdt.csv
	$(INTERP) $(GRAB) "Final POG Clauses" *.pkc_tpromote_purify_defer_d4v2_log > clauses-pkc-splitdt.csv
	$(INTERP) $(MERGE) -d $(TOUT) seconds-d4.csv seconds-pkc-splitdt.csv > seconds-summary-splitdt.csv
	$(INTERP) $(MERGE) -f -t -2:1:1 clauses-defining.csv clauses-pkc-splitdt.csv > clauses-summary-splitdt.csv

# Summaries
	$(INTERP) $(MERGE) -l Bench,D4,Split-D,Split-DT,Recompile,Repair seconds-d4.csv seconds-pkc-splitd.csv \
		seconds-pkc-splitdt.csv seconds-pkc-recompile.csv seconds-pkc-repair.csv > seconds-summary.csv
	$(INTERP) $(MERGE) -l Bench,D4,Split-D,Split-DT,Recompile,Repair clauses-defining.csv clauses-pkc-splitd.csv \
		clauses-pkc-splitdt.csv clauses-pkc-recompile.csv clauses-pkc-repair.csv > clauses-summary.csv

INTERP = python3
N=5
HDIR = ../../..
GENERATORS = $(HDIR)/benchmarks/generators
THRESH= $(GENERATORS)/gen_threshold.py
TOOLS = $(HDIR)/tools
DRIVE = $(TOOLS)/pkc_drive.py
GRAB = $(TOOLS)/grab_scale_data.py
MERGE = $(TOOLS)/merge_csv.py
SHOW = $(TOOLS)/show_csv.py
DLIMIT=10
FLIMIT=1000

# Demonstration of benchmark
demo:	generate drun data
	echo "TSEITIN ENCODING: ELAPSED TIME (seconds)"
	$(INTERP) $(SHOW) tseitin-summary.csv
	echo "PLAISTED-GREENBAUM ENCODING: ELAPSED TIME (seconds)"
	$(INTERP) $(SHOW) plaisted-summary.csv


# Reproduce results of paper
full:	generate frun data

majt:
	$(INTERP) $(THRESH) -v 2 -t -n $(N) -r majt-$(N)

majp:
	$(INTERP) $(THRESH) -v 2 -n $(N) -r majp-$(N)

clean:
	rm -f *.csv *~ zzzz*
	rm -f *.cnf *log

generate:
	./gen.sh

# Run each benchmark for short amount of time
drun:
	echo "Majority, Tseitin Encoding, Split-D"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m d -t $(DLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, Split-DT"
	$(INTERP) $(DRIVE) -X -x -P 2 -T d -m d -t $(DLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, Recompile"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m m -t $(DLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, RepairA"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m i -t $(DLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, RepairB"
	$(INTERP) $(DRIVE) -X -x -P 2 -T d -m i -t $(DLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, Trim"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m t -t $(DLIMIT) majt*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, Split-D"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m d -t $(DLIMIT) majp*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, Split-DT"
	$(INTERP) $(DRIVE) -X -x -P 2 -T p -m d -t $(DLIMIT) majp*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, Recompile"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m m -t $(DLIMIT) majp*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, RepairA"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m i -t $(DLIMIT) majp*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, RepairB"
	$(INTERP) $(DRIVE) -X -x -P 2 -T p -m i -t $(DLIMIT) majp*.cnf

# Run each benchmark for time limit of paper
frun:
	echo "Majority, Tseitin Encoding, Split-D"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m d -t $(FLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, Split-DT"
	$(INTERP) $(DRIVE) -X -x -P 2 -T d -m d -t $(FLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, Recompile"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m m -t $(FLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, RepairA"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m i -t $(FLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, RepairB"
	$(INTERP) $(DRIVE) -X -x -P 2 -T d -m i -t $(FLIMIT) majt*.cnf
	echo "Majority, Tseitin Encoding, Trim"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m t -t $(FLIMIT) majt*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, Split-D"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m d -t $(FLIMIT) majp*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, Split-DT"
	$(INTERP) $(DRIVE) -X -x -P 2 -T p -m d -t $(FLIMIT) majp*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, Recompile"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m m -t $(FLIMIT) majp*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, RepairA"
	$(INTERP) $(DRIVE) -X -x -P 2 -T n -m i -t $(FLIMIT) majp*.cnf
	echo "Majority, Plaisted-Greenbaum Encoding, RepairB"
	$(INTERP) $(DRIVE) -X -x -P 2 -T p -m i -t $(FLIMIT) majp*.cnf

data:
	# Tseitin encoded
	$(INTERP) $(GRAB) "Time TOTAL"        majt-*.pkc_tnone_purify_defer_d4v2_log > seconds-pkc-tseitin-splitd.csv
	$(INTERP) $(GRAB) "Time TOTAL"        majt-*.pkc_tdetect_purify_defer_d4v2_log > seconds-pkc-tseitin-splitdt.csv
	$(INTERP) $(GRAB) "Time TOTAL"        majt-*.pkc_tnone_purify_incr_d4v2_log > seconds-pkc-tseitin-repaira.csv
	$(INTERP) $(GRAB) "Time TOTAL"        majt-*.pkc_tdetect_purify_incr_d4v2_log > seconds-pkc-tseitin-repairb.csv
	$(INTERP) $(GRAB) "Time TOTAL"        majt-*.pkc_tnone_purify_mono_d4v2_log > seconds-pkc-tseitin-recompile.csv
	$(INTERP) $(GRAB) "Time TOTAL"        majt-*.pkc_tnone_purify_trim_d4v2_log > seconds-pkc-tseitin-trim.csv
	$(INTERP) $(MERGE) -l N,SplitD,SplitDT,RepairA,RepairB,Recompile,Trim seconds-pkc-tseitin-splitd.csv \
		seconds-pkc-tseitin-splitdt.csv seconds-pkc-tseitin-repaira.csv seconds-pkc-tseitin-repairb.csv \
		seconds-pkc-tseitin-recompile.csv seconds-pkc-tseitin-trim.csv \
		> tseitin-summary.csv

	# Plaisted encoded
	$(INTERP) $(GRAB) "Time TOTAL"        majp-*.pkc_tnone_purify_defer_d4v2_log > seconds-pkc-plaisted-splitd.csv
	$(INTERP) $(GRAB) "Time TOTAL"        majp-*.pkc_tpromote_purify_defer_d4v2_log > seconds-pkc-plaisted-splitdt.csv
	$(INTERP) $(GRAB) "Time TOTAL"        majp-*.pkc_tnone_purify_incr_d4v2_log > seconds-pkc-plaisted-repaira.csv
	$(INTERP) $(GRAB) "Time TOTAL"        majp-*.pkc_tpromote_purify_incr_d4v2_log > seconds-pkc-plaisted-repairb.csv
	$(INTERP) $(GRAB) "Time TOTAL"        majp-*.pkc_tnone_purify_mono_d4v2_log > seconds-pkc-plaisted-recompile.csv
	$(INTERP) $(MERGE) -l N,SplitD,SplitDT,RepairA,RepairB,Recompile seconds-pkc-plaisted-splitd.csv \
		seconds-pkc-plaisted-splitdt.csv seconds-pkc-plaisted-repaira.csv seconds-pkc-plaisted-repairb.csv \
		seconds-pkc-plaisted-recompile.csv \
		> plaisted-summary.csv

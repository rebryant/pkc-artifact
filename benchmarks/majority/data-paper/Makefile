INTERP = python3
HDIR = ../../..
TOOLS = $(HDIR)/tools
GRAB = $(TOOLS)/grab_scale_data.py
MERGE=$(TOOLS)/merge_csv.py

clean:
	rm -f *~ *.csv

# Generate table of results using data from paper
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



This directory contains useful tools for running the PKC projecting
knowledge compiler and for analyzing its results.

Programs:

pkc_drive.py:
	Driver program to run PKC on one or more .cnf files.  It generates a log file
	with lots of data
	
To reproduce the paper results, the following settings will work for input file FILE.cnf:

TRIM (Only use when all projection variables are Tseitin variables):
  Usage:  pkc_drive.py -T p -m t FILE.cnf
  Log:    FILE.pkc_tpromote_purify_incr_d4v2_log

SPLIT-D:
  Usage:  pkc_drive.py -T n -m d FILE.cnf
  Log:    FILE.pkc_tnone_purify_defer_d4v2_log

SPLIT-DT:
  Usage:  pkc_drive.py -T p -m d FILE.cnf
  Log:    FILE.pkc_tpromote_purify_defer_d4v2_log

RECOMPILE:
  Usage:  pkc_drive.py -T n -m m FILE.cnf
  Log:    FILE.pkc_tnone_purify_mono_d4v2_log

REPAIR:
  Usage:  pkc_drive.py -T p -m i FILE.cnf
  Log:    FILE.pkc_tpromote_purify_incr_d4v2_log

grab_data.py
grab_scale_data.py
merge_csv.py
	Used to extract and format data from the log files
	

/*========================================================================
  Copyright (c) 2023 Randal E. Bryant, Carnegie Mellon University
  
  Permission is hereby granted, free of
  charge, to any person obtaining a copy of this software and
  associated documentation files (the "Software"), to deal in the
  Software without restriction, including without limitation the
  rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following
  conditions:
  
  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
========================================================================*/

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

#include "report.h"
#include "counters.h"
#include "files.hh"
#include "project.hh"


void usage(const char *name) {
    lprintf("Usage: %s [-h] [-m i|t|m|d|c|p] [-P PRE] [-T n|d|p] [-k] [-1] [-v VERB] [-L LOG] [-O OPT] [-b BLIM] FORMULA.cnf [FORMULA.pog]\n", name);
    lprintf("  -h          Print this information\n");
    lprintf("  -m          Select mode: i: incremental, t: trim, m: monolithic, d: defer splitting on projection variables,\n");
    lprintf("                 c: compile without projection, p: stop after preprocessing\n");
    lprintf("  -1          Use original d4, rather than d4 version 2\n");
    lprintf("  -P PRE      Specify preprocessing level: (0:None, 1:+BCP, 2:+Pure lit, >=3:+BVE(P-2)\n");
    lprintf("  -T TSE      Specify use of Tseitin variables (n=none, d=detect, p=promote)\n");
    lprintf("  -k          Keep intermediate files\n");
    lprintf("  -v VERB     Set verbosity level\n");
    lprintf("  -L LOG      Record all results to file LOG\n");
    lprintf("  -O OPT      Select optimization level (0 None, 1:+Reuse, 2:+Analyze vars, 3:+Built-in KC, 4:+Subsumption check)\n");
    lprintf("  -b BLIM     Set upper bound on size (in clauses) of problem for which use built-in KC\n");
}

// Program options
bool keep = false;
pkc_mode_t mode = PKC_INCREMENTAL;
int optlevel = 4;
int preprocess_level = 4;
bool tseitin_detect = true;
bool tseitin_promote = true;
int trace_variable = 0;
int bkc_limit = 70;
bool use_d4v2 = true;

char pkc_mode_char[PKC_NUM] = {'i', 't', 'm', 'd', 'c', 'p'};
const char *pkc_mode_descr[PKC_NUM] = {"incremental", "trim", "monolithic", "deferred", "compile", "preprocess" };

const char *prefix = "c PKC:";

q25_ptr ucount = NULL;
q25_ptr wcount = NULL;

static void stat_report(double elapsed) {
    if (verblevel < 1)
	return;
    lprintf("%s Input Formula\n", prefix);
    int uv = get_count(COUNT_UNUSED_VAR);
    int dv = get_count(COUNT_DATA_VAR);
    int ntv = get_count(COUNT_NONTSEITIN_VAR);
    int tdv = get_count(COUNT_TSEITIN_DETECT_VAR);
    int tpv = get_count(COUNT_TSEITIN_PROMOTE_VAR);
    int ev = get_count(COUNT_ELIM_VAR);
    lprintf("%s    Declared Variables     : %d\n", prefix, uv+dv+ntv+tdv+tpv+ev);
    lprintf("%s    Data variables         : %d\n", prefix, dv);
    lprintf("%s    Eliminated variables   : %d\n", prefix, ev);
    lprintf("%s    Tseitin variables:\n", prefix);
    lprintf("%s       Tseitin original    : %d\n", prefix, tdv);
    lprintf("%s       Tseitin promoted    : %d\n", prefix, tpv);
    lprintf("%s       Tseitin TOTAL       : %d\n", prefix, tdv+tpv);
    lprintf("%s    Other projection vars  : %d\n", prefix, ntv);
    lprintf("%s    Unused vars            : %d\n", prefix, uv);
    lprintf("%s    Problem Clauses:\n", prefix);
    int ic = get_count(COUNT_INPUT_CLAUSE);
    int pc = get_count(COUNT_PROMOTE_CLAUSE);
    int ec = get_count(COUNT_BVE_ELIM_CLAUSE);
    int nc = get_count(COUNT_BVE_NEW_CLAUSE);
    lprintf("%s       Input clauses       : %d\n", prefix, ic);
    lprintf("%s       BVE Elim clauses    : %d\n", prefix, ec);
    lprintf("%s       BVE Added clauses   : %d\n", prefix, nc);
    lprintf("%s       Promoting clauses   : %d\n", prefix, pc);
    lprintf("%s       Clause TOTAL        : %d\n", prefix, ic-ec+nc+pc);
    lprintf("%s Preprocessing\n", prefix);
    lprintf("%s   Tseitin variable tests:   %d\n", prefix, get_count(COUNT_TSEITIN_TEST));

    if (mode == PKC_PREPROCESS) {
	int sat_count = get_count(COUNT_SAT_CALL);
	int clause_min = get_histo_min(HISTO_SAT_CLAUSES);
	int clause_max = get_histo_max(HISTO_SAT_CLAUSES);
	double clause_avg = get_histo_avg(HISTO_SAT_CLAUSES);
	lprintf("%s SAT calls\n", prefix);
	lprintf("%s    SAT TOTAL              : %d\n", prefix, sat_count);
	if (sat_count > 0) {
	    lprintf("%s    SAT Clause MIN         : %d\n", prefix, clause_min);
	    lprintf("%s    SAT Clause AVG         : %.2f\n", prefix, clause_avg);
	    lprintf("%s    SAT Clause MAX         : %d\n", prefix, clause_max);
	}
	double preprocess_time = get_timer(TIME_PREPROCESS);
	double sat_time = get_timer(TIME_SAT);
	double classify_time = get_timer(TIME_CLASSIFY)-sat_time;
	double other_time = elapsed-(preprocess_time+sat_time+classify_time);
	lprintf("%s Time\n", prefix);
	lprintf("%s    Preprocess (BCP+BVE)   : %.2f\n", prefix, preprocess_time);
	lprintf("%s    Classify/promote vars  : %.2f\n", prefix, classify_time);
	lprintf("%s    SAT time               : %.2f\n", prefix, sat_time);
	lprintf("%s    Other time             : %.2f\n", prefix, other_time);
	lprintf("%s    Time TOTAL             : %.2f\n", prefix, elapsed);
	return;
    }

    lprintf("%s Initial POG\n", prefix);
    int ps, pp;
    long pe;
    lprintf("%s    Initial POG Sum        : %d\n", prefix, ps = get_count(COUNT_POG_INITIAL_SUM));
    lprintf("%s    Initial POG Product    : %d\n", prefix, pp = get_count(COUNT_POG_INITIAL_PRODUCT));
    lprintf("%s    Initial POG Nodes      : %d\n", prefix, ps+pp);
    lprintf("%s    Initial POG Edges      : %ld\n", prefix, pe = get_long_count(COUNT_POG_INITIAL_EDGES));
    lprintf("%s    Initial POG Clauses    : %ld\n", prefix, ps+pp+pe);

    lprintf("%s POG nodes generated\n", prefix);
    lprintf("%s    Total POG Sum          : %d\n", prefix, ps = get_count(COUNT_POG_SUM));
    lprintf("%s    Total POG Product      : %d\n", prefix, pp = get_count(COUNT_POG_PRODUCT));
    lprintf("%s    Total POG Nodes        : %d\n", prefix, ps+pp);
    lprintf("%s    Total POG Edges        : %ld\n", prefix, pe = get_long_count(COUNT_POG_EDGES));
    lprintf("%s    Total POG Clauses      : %ld\n", prefix, ps+pp+pe);

    lprintf("%s Final POG\n", prefix);
    lprintf("%s    Final POG Sum          : %d\n", prefix, ps = get_count(COUNT_POG_FINAL_SUM));
    lprintf("%s    Final POG Product      : %d\n", prefix, pp = get_count(COUNT_POG_FINAL_PRODUCT));
    lprintf("%s    Final POG Nodes        : %d\n", prefix, ps+pp);
    lprintf("%s    Final POG Edges        : %ld\n", prefix, pe = get_count(COUNT_POG_FINAL_EDGES));
    lprintf("%s    Final POG Clauses      : %ld\n", prefix, ps+pp+pe);

    
    int sat_count = get_count(COUNT_SAT_CALL);
    int clause_min = get_histo_min(HISTO_SAT_CLAUSES);
    int clause_max = get_histo_max(HISTO_SAT_CLAUSES);
    double clause_avg = get_histo_avg(HISTO_SAT_CLAUSES);

    if (mode == PKC_INCREMENTAL || mode == PKC_DEFERRED) {
	lprintf("%s SAT calls\n", prefix);
	lprintf("%s    SAT TOTAL              : %d\n", prefix, sat_count);
	if (sat_count > 0) {
	    lprintf("%s    SAT Clause MIN         : %d\n", prefix, clause_min);
	    lprintf("%s    SAT Clause AVG         : %.2f\n", prefix, clause_avg);
	    lprintf("%s    SAT Clause MAX         : %d\n", prefix, clause_max);
	}
    }

    int kc_count = get_count(COUNT_KC_CALL);
    clause_min = get_histo_min(HISTO_KC_CLAUSES);
    clause_max = get_histo_max(HISTO_KC_CLAUSES);
    clause_avg = get_histo_avg(HISTO_KC_CLAUSES);
    lprintf("%s External KC calls\n", prefix);
    lprintf("%s    External KC TOTAL      : %d\n", prefix, kc_count);
    if (kc_count > 0) {
	lprintf("%s    XKC Clause MIN         : %d\n", prefix, clause_min);
	lprintf("%s    XKC Clause AVG         : %.2f\n", prefix, clause_avg);
	lprintf("%s    XKC clause MAX         : %d\n", prefix, clause_max);
    }

    int bkc_count = get_count(COUNT_BUILTIN_KC);
    clause_min = get_histo_min(HISTO_BUILTIN_KC_CLAUSES);
    clause_max = get_histo_max(HISTO_BUILTIN_KC_CLAUSES);
    clause_avg = get_histo_avg(HISTO_BUILTIN_KC_CLAUSES);
    lprintf("%s Builtin KC calls\n", prefix);
    lprintf("%s    Builtin KC TOTAL       : %d\n", prefix, bkc_count);
    if (bkc_count > 0) {
	lprintf("%s    BKC Clause MIN         : %d\n", prefix, clause_min);
	lprintf("%s    BKC Clause AVG         : %.2f\n", prefix, clause_avg);
	lprintf("%s    BKC clause MAX         : %d\n", prefix, clause_max);
    }

    int pog_min = get_histo_min(HISTO_POG_NODES);
    int pog_max = get_histo_max(HISTO_POG_NODES);
    double pog_avg = get_histo_avg(HISTO_POG_NODES);
    lprintf("%s KC added POG nodes\n", prefix);
    lprintf("%s    KC Invocations         : %d\n", prefix, kc_count + bkc_count);
    if (bkc_count + kc_count > 0) {
	lprintf("%s    KC POG MIN             : %d\n", prefix, pog_min);
	lprintf("%s    KC POG AVG             : %.2f\n", prefix, pog_avg);
	lprintf("%s    KC POG MAX             : %d\n", prefix, pog_max);
    }

    if (mode == PKC_INCREMENTAL || mode == PKC_DEFERRED) {
	lprintf("%s Node Traversals:\n", prefix);
	int vp, vsd, vsm, vst, vss, vsc, vse, vs;
	lprintf("%s       Total Product       : %d\n", prefix, vp =  get_count(COUNT_VISIT_PRODUCT));
	lprintf("%s         Data Sum          : %d\n", prefix, vsd = get_count(COUNT_VISIT_DATA_SUM));
	lprintf("%s         Mutex Sum         : %d\n", prefix, vsm = get_count(COUNT_VISIT_MUTEX_SUM));    
	lprintf("%s         Tautology Sum     : %d\n", prefix, vst = get_count(COUNT_VISIT_TAUTOLOGY_SUM));    
	lprintf("%s         Subsumed Sum      : %d\n", prefix, vss = get_count(COUNT_VISIT_SUBSUMED_SUM));    
	lprintf("%s         Counted SS Sum    : %d\n", prefix, vsc = get_count(COUNT_VISIT_COUNTED_SUM));    
	lprintf("%s         Excluding Sum     : %d\n", prefix, vse = get_count(COUNT_VISIT_EXCLUDING_SUM));    
	lprintf("%s       Total Sum           : %d\n", prefix, vs = vsd + vsm + vst + vss + vsc + vse);
	lprintf("%s    Traverse TOTAL         : %d\n", prefix, vp+vs);

	lprintf("%s PKC Optimizations:\n", prefix);
	lprintf("%s    Built-in KC            : %d\n", prefix, get_count(COUNT_BUILTIN_KC));
	lprintf("%s    Only data variables    : %d\n", prefix, get_count(COUNT_PKC_DATA_ONLY));
	lprintf("%s    Only projection vars   : %d\n", prefix, get_count(COUNT_PKC_PROJECT_ONLY));
	lprintf("%s    Result reuse           : %d\n", prefix, get_count(COUNT_PKC_REUSE));
    }

    double preprocess_time = get_timer(TIME_PREPROCESS);
    double classify_time = get_timer(TIME_CLASSIFY);
    double init_kc_time = get_timer(TIME_INITIAL_KC);
    double kc_time = get_timer(TIME_KC);
    double builtin_kc_time = get_timer(TIME_BUILTIN_KC);
    double sat_time = get_timer(TIME_SAT);
    double ring_time = get_timer(TIME_RING_EVAL);
    double other_time = elapsed-(preprocess_time+classify_time+init_kc_time+kc_time+builtin_kc_time+sat_time+ring_time);
    lprintf("%s Time\n", prefix);
    lprintf("%s    Preprocess (BCP+BVE)   : %.2f\n", prefix, preprocess_time);
    lprintf("%s    Classify/promote vars  : %.2f\n", prefix, classify_time);
    lprintf("%s    Initial KC time        : %.2f\n", prefix, init_kc_time);
    lprintf("%s    Other external KC time : %.2f\n", prefix, kc_time);
    lprintf("%s    Builtin KC time        : %.2f\n", prefix, builtin_kc_time);
    lprintf("%s    SAT time               : %.2f\n", prefix, sat_time);
    lprintf("%s    Ring evaluation time   : %.2f\n", prefix, ring_time);
    lprintf("%s    Other time             : %.2f\n", prefix, other_time);
    lprintf("%s    Time TOTAL             : %.2f\n", prefix, elapsed);
}

static int run(double start, const char *cnf_name, const char *pog_name) {
    Project proj(cnf_name, mode, use_d4v2, preprocess_level, tseitin_detect, tseitin_promote, optlevel, bkc_limit);
    if (mode == PKC_PREPROCESS)
	return 0;
    if (trace_variable != 0)
	proj.set_trace_variable(trace_variable);
    if (verblevel >= 5) {
	printf("Initial POG:\n");
	proj.show(stdout);
    }
    report(1, "Time %.2f: Initial compilation completed\n", tod() - start);
    proj.projecting_compile(preprocess_level);
    if (verblevel >= 5) {
	printf("Projected POG:\n");
	proj.show(stdout);
    }
    proj.write(pog_name);
    report(1, "Time %.2f: Projecting compilation completed\n", tod() - start);
    ucount = proj.count(false);
    report(1, "Time %.2f: Unweighted count completed\n", tod() - start);
    wcount = proj.count(true);
    report(1, "Time %.2f: Everything completed\n", tod() - start);
    return 0;
}

int main(int argc, char *const argv[]) {
    FILE *cnf_file = NULL;
    int nbkc_limit = bkc_limit;
    int c;
    char flag;
    while ((c = getopt(argc, argv, "hkP:T:1m:v:L:O:b:")) != -1) {
	switch (c) {
	case 'h':
	    usage(argv[0]);
	    return 0;
	case 'v':
	    verblevel = atoi(optarg);
	    break;
	case 'm':
	    flag = optarg[0];
	    for (int imode = 0; imode <= PKC_NUM; imode++) {
		if (imode == PKC_NUM) {
		    lprintf("Invalid PKC mode '%c'\n", flag);
		    usage(argv[0]);
		    return 1;
		} else if (flag == pkc_mode_char[imode]) {
		    mode = (pkc_mode_t) imode;
		    if (mode == PKC_MONOLITHIC)
			// By default, disable builtin PKC when in monolithic mode
			nbkc_limit = 0;
		    break;
		}
	    }
	    break;
	case '1':
	    use_d4v2 = false;
	    break;
	case 'P':
	    preprocess_level = atoi(optarg);
	    break;
	case 'T':
	    if (strlen(optarg) != 1) {
		lprintf("Invalid Tseitin directive '%s'\n", optarg);
		usage(argv[0]);
		return 1;
	    }
	    if (optarg[0] == 'n') {
		tseitin_detect = false;
		tseitin_promote = false;
	    } else if (optarg[0] == 'd') {
		tseitin_promote = false;
	    } else if (optarg[0] != 'p') {
		lprintf("Invalid Tseitin directive '%s'\n", optarg);
		usage(argv[0]);
		return 1;
	    }
	    break;
	case 'k':
	    keep = true;
	    break;
	case 'L':
	    set_logname(optarg);
	    break;
	case 'O':
	    optlevel = atoi(optarg);
	    break;
	case 'b':
	    nbkc_limit = atoi(optarg);
	    break;
	default:
	    lprintf("Unknown commandline option '%c'\n", c);
	    usage(argv[0]);
	    break;
	}

    }
    // Set this when all options declared
    bkc_limit = nbkc_limit;
    int argi = optind;
    if (argi >= argc) {
	lprintf("Name of input CNF file required\n");
	usage(argv[0]);
	return 1;
    }
    const char *cnf_name = argv[argi++];

    const char *pog_name = NULL;
    if (argi < argc)
	pog_name = argv[argi++];

    if (argi < argc) {
	lprintf("Unknown argument '%s'\n", argv[argi]);
	usage(argv[0]);
	return 1;
    }

    lprintf("%s Program options\n", prefix);
    lprintf("%s   Mode                      %s\n", prefix, pkc_mode_descr[(int) mode]);
    lprintf("%s   D4 version                %s\n", prefix, use_d4v2 ? "v2" : "original");
    lprintf("%s   Preprocess level          %d\n", prefix, preprocess_level);
    const char *tseitin_mode = "promote";
    if (!tseitin_promote)
	tseitin_mode = "detect";
    if (!tseitin_detect)
	tseitin_mode = "none";
    lprintf("%s   Tseitin variable handling %s\n", prefix, tseitin_mode);
    lprintf("%s   Optimization level        %d\n", prefix, optlevel);
    lprintf("%s   Builtin KC limit          %d\n", prefix, bkc_limit);
    if (trace_variable != 0)
	lprintf("%s   Trace variable            %d\n", prefix, trace_variable);
    double start = tod();
    if (!keep)
	fmgr.enable_flush();
    int result = run(start, cnf_name, pog_name);
    stat_report(tod()-start);
    if (ucount != NULL) {
	lprintf("Unweighted count:");
	q25_write(ucount, stdout);
	lprintf("\n");
	q25_free(ucount);
    }
    if (wcount != NULL) {
	lprintf("Weighted count:");
	q25_write(wcount, stdout);
	lprintf("\n");
	q25_free(wcount);
    }
    return result;
}

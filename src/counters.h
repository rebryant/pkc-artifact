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


#pragma once

// Count all the interesting stuff

typedef enum { 

    COUNT_UNUSED_VAR, COUNT_DATA_VAR, COUNT_NONTSEITIN_VAR, COUNT_TSEITIN_DETECT_VAR, COUNT_TSEITIN_PROMOTE_VAR, COUNT_ELIM_VAR,
    COUNT_INPUT_CLAUSE, 
    COUNT_BVE_ELIM_CLAUSE, COUNT_BVE_NEW_CLAUSE,
    COUNT_PROMOTE_CLAUSE,
    COUNT_TSEITIN_TEST,
    COUNT_POG_INITIAL_PRODUCT, COUNT_POG_INITIAL_SUM, COUNT_POG_INITIAL_EDGES,
    COUNT_POG_FINAL_PRODUCT, COUNT_POG_FINAL_SUM, COUNT_POG_FINAL_EDGES,
    COUNT_POG_PRODUCT, COUNT_POG_SUM, COUNT_POG_EDGES,
    COUNT_VISIT_PRODUCT,
    COUNT_VISIT_DATA_SUM, COUNT_VISIT_TAUTOLOGY_SUM, COUNT_VISIT_MUTEX_SUM,
    COUNT_VISIT_EXCLUDING_SUM, COUNT_VISIT_SUBSUMED_SUM, COUNT_VISIT_COUNTED_SUM,
    COUNT_SAT_CALL, COUNT_BUILTIN_KC,  COUNT_KC_CALL,
    COUNT_PKC_DATA_ONLY, COUNT_PKC_PROJECT_ONLY, COUNT_PKC_REUSE,
    COUNT_NUM
} counter_t;

typedef enum { TIME_PREPROCESS, TIME_SAT, TIME_BCP, TIME_CLASSIFY, TIME_KC, TIME_BUILTIN_KC, TIME_INITIAL_KC, TIME_RING_EVAL, TIME_NUM } runtimer_t;

typedef enum { HISTO_SAT_CLAUSES, HISTO_KC_CLAUSES, HISTO_BUILTIN_KC_CLAUSES, HISTO_POG_NODES, HISTO_NUM } histogram_t;

/* Allow this headerfile to define C++ constructs if requested */
#ifdef __cplusplus
#define CPLUSPLUS
#endif

#ifdef CPLUSPLUS
extern "C" {
#endif

void incr_count(counter_t counter);
void incr_count_by(counter_t counter, int val);
int get_count(counter_t counter);
long get_long_count(counter_t counter);

void incr_timer(runtimer_t timer, double secs);
void reset_timer(runtimer_t timer);
double get_timer(runtimer_t timer);

void incr_histo(histogram_t h, int datum);
void reset_histo(histogram_t h);
int get_histo_min(histogram_t h);
int get_histo_max(histogram_t h);    
int get_histo_count(histogram_t h);
double get_histo_avg(histogram_t h);

#ifdef CPLUSPLUS
}
#endif


/* EOF */
    

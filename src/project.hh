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

#include "pog.hh"
#include "compile.hh"

typedef enum { PKC_INCREMENTAL, PKC_TSEITIN, PKC_MONOLITHIC, PKC_DEFERRED, PKC_COMPILE, PKC_PREPROCESS, PKC_NUM } pkc_mode_t;

class Project {
private:
    Pog *pog;
    Compiler *compiler;
    int root_literal;
    std::unordered_map<int,int> result_cache;
    std::unordered_map<int,q25_ptr> *input_weights;

    pkc_mode_t mode;

    // Optimization levels for standard mode:
    // 0 : None
    // 1 : Reuse previous results
    // 2 : Detect data-only and projection-only variables in CNF
    // 3 : Run builtin KC for small problems
    // 4 : Perform subsumption check when performing sum reductions
    int optlevel;

    // Debugging support
    int trace_variable;

public:
    Project(const char *cnf_name, pkc_mode_t mode, bool use_d4v2, int preprocessing_level, bool tseitin_detect, bool tseitin_promote, int optlevel, int bkc_limit);
    ~Project();
    void projecting_compile(int preprocess_level);
    bool write(const char *pog_name);

    // Perform weighted or unweighted model counting
    // Return NULL if weighted but no weights declared
    q25_ptr count(bool weighted);

    // Debugging support
    void show(FILE *outfile) { pog->show(root_literal, outfile); }

    void set_trace_variable(int var) { trace_variable = var; pog->set_trace_variable(var); }


private:
    // Perform ordinary knowledge compilation by invoking D4

    // Traversal
    bool sums_to_tautology(std::vector<int> &root_literals);

    int traverse_sum(int edge);
    int traverse_product(int edge);
    int traverse(int edge);

    // Perform weighted or unweighted model counting
    // Return NULL if weighted but no weights declared
    q25_ptr subgraph_count(bool weighted, int root_edge);

    // Use as part of subsumption check
    bool equal_counts(int root_edge1, int root_edge2);

};

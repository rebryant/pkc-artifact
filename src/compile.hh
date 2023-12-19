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

#include <set>
#include <unordered_set>

#include "pog.hh"


// Get header info from CNF file
bool read_cnf_header(const char* cnf_name, int &nvar, int &nclause);

// Possible actions to record on action stack
typedef enum {
    ACTION_START_CONTEXT,      // Start of new context
    ACTION_CONFLICT,           // Found conflict
    ACTION_DEACTIVATE_CLAUSE,  // Deactivated clause
    ACTION_BCP,                // Derived unit literal by BCP
    ACTION_ASSERT,             // Asserted unit literal externally
    ACTION_ASSERT_FROM_BCP,    // Convert BCP unit literal into asserted literal
    ACTION_UQUANTIFY,          // Universally quantify variable
    ACTION_ACTIVE_CLAUSES,     // New set of active clauses
    ACTION_NUM                 // Count
} action_t;

struct action_record {
    action_t action;
    int ele;  // Clause ID or literal
};

// Have ability to store active clause set + map on stack
struct active_record {
    std::set<int> *active_clauses;
    std::unordered_map<int,std::unordered_set<int>> *literal_clauses;
};

// Variable types
typedef enum {
    VAR_UNUSED,           // Not in any clause ever
    VAR_DATA,             // Declared data variable
    VAR_NONTSEITIN,       // Projection variable, but not Tseitin
    VAR_TSEITIN_DETECT,   // Detected Tseitin variable
    VAR_TSEITIN_PROMOTE,  // Promoted Tseitin variable
    VAR_ELIM,             // Variable eliminated by BCP or BVE
    VAR_NUM       
} var_t;

class Cnf {
private:
    
    int nvar;
    // Type information about each variable.  Indexed by var-1.  Use for final information reporting
    var_t *variable_type;

    // For each clause, its starting index into the literal_sequence array
    // A final index beyond the last clause to make it easy to compute clause lengths
    std::vector<int> clause_offset;
    // Clause literals combined into single sequence
    std::vector<int> literal_sequence;

    // Map from literal to set of clauses containing it.
    // Used for (BCP), BVE, and Tseitin detection/promotion
    std::unordered_map<int,std::unordered_set<int>> *literal_clauses;

    // Support for KC
    bool has_conflict;
    // Record history of KC
    std::vector<action_record> action_stack;
    // History of active clause set
    std::vector<active_record> active_stack;
    // Set of clauses that are not satisfied and haven't reduced to units
    std::set<int> *active_clauses;
    // Units that were either derived by BCP or were asserted
    std::unordered_set<int> unit_literals;
    // Subset of units that are from BCP
    std::unordered_set<int> bcp_unit_literals;
    // Universally quantified variables
    std::unordered_set<int> uquantified_variables;
    // Debugging support
    int trace_variable;

public:
    Cnf();
    Cnf(int icount);

    bool import_file(FILE *infile, bool process_comments);

    ~Cnf();

    void initialize(int icount);
    // Delete sets for data variables and input weights
    void deallocate();

    int variable_count() { return nvar; }
    int nonunit_clause_count();
    int current_clause_count();
    int maximum_clause_id();
    int clause_length(int id);
    // Index literals from 0
    int get_literal(int cid, int lid);
    void swap_literals(int cid, int i, int j);
    // Both of these return true if successful
    bool show(FILE *outfile);
    bool write(FILE *outfile, bool show_data_and_tseitin_variables);
    
    // Add new clauses one literal at a time
    // Returns clause ID
    int new_clause();
    void add_literal(int lit);
    void finish();

    bool is_satisfiable();

    // Public access to extra information
    std::unordered_set<int> *data_variables;
    // Variables that were detected to have Tseitin property during preprocessing
    std::unordered_set<int> *tseitin_variables;
    // Projection variables that are not Tseitin variables
    std::unordered_map<int,q25_ptr> *input_weights;

    // KC Support
    void new_context();
    void pop_context();
    void assign_literal(int lit, bool bcp);
    void uquantify_variable(int var);
    int bcp(bool preprocess);
    
    // Check if clauses consist of disjoint literals
    // If condition holds, fill up vector with zero-separated sequences of literals
    // Assumes have run bcp & BVE (level 0) beforehand
    bool check_simple_pkc(std::vector<int> &clause_chunks);

    // Choose splitting variable
    int find_split(bool defer);

    bool is_data_variable(int var) { return data_variables->find(var) != data_variables->end(); }


    // Eliminate projection variables by bounded variable elimination
    // Replaces clauses containing them with resolvents
    // Return number of variables eliminated
    int bve(bool preprocess, int maxdegree);
    
    // Divide non-data variables into Tseitin & Projection variables
    // If promote, then perform Tseitin promotion
    void classify_variables(bool promote);

    // Get count of each variable type
    int get_variable_type_count(var_t type);

private:
    void set_variable_type(int var, var_t type);
    var_t get_variable_type(int var);

    bool tseitin_variable_test(int var, bool promote, 
			     std::unordered_set<int> &fanout_vars);
    // Add blocked clauses
    void blocked_clause_expand(int lit, std::vector<int> &clause_list);

    bool skip_literal(int lit);
    bool skip_clause(int cid);
    void trigger_conflict();
    int propagate_clause(int cid);

    void activate_clause(int cid);
    void deactivate_clause(int cid);

    // Cannot deactivate clause in middle of iterator
    // Instead, store in vector and later due batch of them
    void deactivate_clauses(std::vector<int> &remove);

    // Save set of active clauses; start new one; update literal_clauses
    void push_active(std::set<int> *nactive_clauses);

    // Resolution
    int resolve(int var, int cid1, int cid2);

};



class Compiler {
private:
    Pog *pog;
    // Limit on number of clauses for which do builtin KC
    int bkc_limit;
    bool use_d4v2;

public:
    Compiler(Pog *pog, bool use_d4v2);
    ~Compiler();

    void set_bkc_limit(int blim) { bkc_limit = blim; }

    // Encode portions of POG.
    // Detect data variables
    Cnf *clausify(std::vector<int> &root_literals);
    // Compile, integrate into POG and return pointer to root literal
    int compile(const char *cnf_name, std::unordered_set<int> *data_variables, bool trim);
    // Compile CNF representation.
    // When trim, convert literals of projection variables to tautology
    // For Tseitin variables, this serves as PKC
    // For others, can have non-mutually-exclusive sums
    int compile(Cnf *cnf, bool trim, bool defer);

private:
    // Internal knowledge compiler.
    // Performs reductions in anticipation of projection
    // assume_tseitin indicates that all projection variables are Tseitin variables
    int builtin_kc(Cnf *cnf, bool assume_tseitin, bool defer, bool toplevel);
};

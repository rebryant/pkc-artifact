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

#include <cstdio>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <limits.h>

#include "q25.h"

#define TAUTOLOGY INT_MAX
#define CONFLICT (-TAUTOLOGY)
// Used to convert literal to variable
#define IABS(x) ((x)<0?-(x):(x))
#define IMIN(x,y) ((x)<(y)?(x):(y))


#define MAX_VARIABLE (2 * 1000 * 1000 * 1000)

typedef enum { POG_NONE, POG_PRODUCT, POG_SUM, POG_NUM } pog_type_t;


// A POG edge is an integer, where the sign indicates whether it is
// positive or negated, and the magnitude indicates the edge destination.
// Edge destinations can be:
//  TAUTOLOGY
//  Variable: 1 <= var <= nvar
//  POG node values between nvar+1 and TAUTOLOGY-1

struct Node {
    int  offset;  // Offset into list of arguments
    pog_type_t type :      2;
    bool data_only :       1;
    bool projection_only : 1;
    int degree  :         28;
};

class Pog {
private:
    // Number of input variables
    int nvar;
    // Concatenation of all operation arguments
    std::vector<int> arguments;
    // List of nodes, indexed by var-nvar-1
    std::vector<Node> nodes;
    // Unique table.  Maps from hash of operation + arguments to edge.
    std::unordered_multimap<unsigned, int> unique_table;
    // Debugging support
    int trace_variable;
    

public:
    
    Pog(int n, std::unordered_set<int> *dvars, std::unordered_set<int> *tvars) 
    { nvar = n; data_variables = dvars; tseitin_variables = tvars; trace_variable = 0; }
    ~Pog() {}

    bool get_phase(int edge) { return edge > 0; }
    int get_var(int edge) { return IABS(edge); }
    bool is_node(int edge) { int var = get_var(edge); return var > nvar && var != TAUTOLOGY; }
    int node_index(int edge) { int var = get_var(edge); return is_node(var) ? var-nvar-1 : -1; }
    int get_degree(int edge) { int idx = node_index(edge); return idx < 0 ? 0 : nodes[idx].degree; }
    pog_type_t get_type(int edge) { int idx = node_index(edge); return idx < 0 ? POG_NONE : nodes[idx].type; }
    bool is_sum(int edge) { return get_type(edge) == POG_SUM; }
    bool only_data_variables(int edge) { 
	return is_node(edge) ?
	    nodes[node_index(edge)].data_only :
	    data_variables->find(get_var(edge)) != data_variables->end(); }
    bool only_projection_variables(int edge) { 
	return is_node(edge) ?
	    nodes[node_index(edge)].projection_only :
	    data_variables->find(get_var(edge)) == data_variables->end(); }

    int variable_count() { return nvar; }
    int node_count() { return nodes.size(); }
    int edge_count() { return arguments.size(); }

    bool is_data_variable(int var) { return data_variables->find(var) != data_variables->end(); }
    bool is_tseitin_variable(int var) { return tseitin_variables->find(var) != data_variables->end(); }


    int *get_arguments(int edge) {
	int idx = node_index(edge); 
	return idx < 0 ? NULL : &arguments[nodes[idx].offset];
    }

    int get_argument(int edge, int index) { 
	int idx = node_index(edge); 
	return idx < 0 ? 0 : arguments[nodes[idx].offset + index];
    }

    int get_decision_variable(int edge);

    void get_variables(int root, std::unordered_set<int> &vset);

    // Creating a node
    void start_node(pog_type_t type);
    void add_argument(int edge);
    // Return edge for either newly created or existing node
    int finish_node();

    // Extract subgraph with designated root edge and write to file
    // Can have FILE = NULL, in which case does not actually perform the write
    bool write(int root_edge, FILE *outfile);

    // Use to perform both weighted and unweighted model counting
    q25_ptr ring_evaluate(int root_edge, std::unordered_map<int,q25_ptr> &weights);

    // Read NNF file and integrate into POG.  Return edge to new root
    // Optionally perform Tseitin trimming
    int load_nnf(FILE *infile, std::unordered_set<int> *data_variables);

    // Simple KC when formula is conjunction of independent clauses
    // Argument is sequence of clause literals, separated by zeros
    int simple_kc(std::vector<int> &clause_chunks);

    // Sets of data & tseitin variables kept public
    std::unordered_set<int> *data_variables;
    std::unordered_set<int> *tseitin_variables;    

    // Debugging 
    void set_trace_variable(int var) { trace_variable = var; }
    // If root = 0, dump entire POG.  Otherwise just those nodes reachable from root
    void show(int root, FILE *outfile);

    void show_edge(FILE *outfile, int edge);
    
    // Collect Ids of all nodes reachable from root
    void visit(int edge, std::set<int> &visited);

    // Find subgraph with specified root.  Return mapping from old edges to new edges
    void get_subgraph(std::vector<int> &root_edges, std::map<int,int> &node_remap);

private:

    unsigned node_hash(int var);
    bool node_equal(int var1, int var2);

    // Create a POG representation of a clause
    int build_disjunction(std::vector<int> &args);


};

// Routine to aid the management of q25_ptr's
q25_ptr qmark(q25_ptr q, std::vector<q25_ptr> &qlog);
void qflush(std::vector<q25_ptr> &qlog);

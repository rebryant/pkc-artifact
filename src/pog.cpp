/*========================================================================
  Copyright (c) 2022, 2023 Randal E. Bryant, Carnegie Mellon University
  
  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:
  
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

#include <cstdlib>
#include <ctype.h>
#include <cstring>
#include <map>
#include <algorithm>
#include "report.h"
#include "counters.h"
#include "pog.hh"


// Put literals in ascending order of the variables
static bool abs_less(int x, int y) {
    return IABS(x) < IABS(y);
}

/// Support for NNF reading

// Try to read single alphabetic character from line
// If not found, then push back unread character and return 0
// If hit EOF, then return this
static int get_token(FILE *infile) {
    int c;
    while (true) {
	c = fgetc(infile);
	if (isalpha(c) || c == EOF)
	    return c;
	else if (isspace(c))
	    continue;
	else {
	    ungetc(c, infile);
	    return 0;
	}
    }
}

// Read sequence of numbers from line of input
// Consume end of line character
// Return false if non-numeric value encountered
static bool read_numbers(FILE *infile, std::vector<int> &vec, int *rc) {
    vec.resize(0);
    while (true) {
	int c = fgetc(infile);
	int val;
	if (c == '\n' || c == EOF) {
	    *rc = c;
	    return true;
	} else if (isspace(c))
	    continue;
	else {
	    ungetc(c, infile);
	    if (fscanf(infile, "%d", &val) == 1) {
		vec.push_back(val);
	    } else
		return false;
	}
    }
    // Won't hit this
    return false;
}

typedef enum { NNF_NONE, NNF_TRUE, NNF_FALSE, NNF_AND, NNF_OR, NNF_NUM } nnf_type_t;
const char *nnf_type_name[NNF_NUM] = { "NONE", "TRUE", "FALSE", "AND", "OR" };
const char nnf_type_char[NNF_NUM] = { '\0', 't', 'f', 'a', 'o' };

// Index regular nodes starting at NODE_START
#define NODE_START (500 * 1000 * 1000)
// Index added conjunctions at XNODE_START
#define XNODE_START (1000 * 1000 * 1000 + 1)

// Graph representation derived from NNF file
class Nnf {
public:
    int nvar;

    // Each node represented by vector consisting of type + arguments
    // Indexed by node ID
    std::map<int,std::vector<int>*> nodes;

    // Index of root
    int root_id;

    Nnf(int n, FILE *infile);
    ~Nnf();

    // Debugging support
    void show(FILE *outfile);

    // Topologically order NNF nodes reachable from root with root at end
    void topo_order(std::vector<int> &ids);

    // Support for topological ordering
    void visit(int nid, std::vector<int> &ids, std::unordered_set<int> &visited);

};

Nnf::Nnf(int n, FILE *infile) {
    nvar = n;
    root_id = 0;
    // Set of nodes with at least one parent
    std::unordered_set<int> node_with_parent;
    // How many xnodes have been generated
    int xcount = 0;

    // Capture arguments for each line
    std::vector<int> largs;
    int line_number = 0;
    // Statistics
    int nnf_node_count = 0;
    int nnf_explicit_node_count = 0;
    int nnf_edge_count = 0;
    while (true) {
	nnf_type_t ntype = NNF_NONE;
	line_number++;
	int c = get_token(infile);
	int rc = 0;
	if (c == EOF)
	    break;
	if (c != 0) {
	    for (int t = NNF_TRUE; t < NNF_NUM; t++)
		if (c == nnf_type_char[t])
		    ntype = (nnf_type_t) t;
	    if (ntype == NNF_NONE)
		err(true, "Line #%d.  Unknown D4 NNF command '%c'\n", line_number, c);
	    nnf_node_count++;
	    nnf_explicit_node_count++;
	    bool ok = read_numbers(infile, largs, &rc);
	    if (!ok)
		err(true, "Line #%d.  Couldn't parse numbers\n", line_number);
	    else if (largs.size() == 0 && rc == EOF)
		break;
	    else if (largs.size() != 2)
		err(true, "Line #%d.  Expected 2 numbers.  Found %d\n", line_number, largs.size());
	    else if (largs.back() != 0)
		err(true, "Line #%d.  Line not zero-terminated\n", line_number);
	    else {
		int nid = NODE_START + largs[0];
		std::vector<int> *node = new std::vector<int>;
		nodes[nid] = node;
		node->push_back((int) ntype);
		report(6, "Line #%d.  Created NNF type %s node %d from NNF node %d\n",
		       line_number, nnf_type_name[ntype], nid, largs[0]); 
	    }
	} else {
	    nnf_edge_count++;
	    bool ok = read_numbers(infile, largs, &rc);
	    if (!ok)
		err(true, "Line #%d.  Couldn't parse numbers\n", line_number);
	    else if (largs.size() == 0 && rc == EOF)
		break;
	    else if (largs.size() < 3)
		err(true, "Line #%d.  Expected at least 3 numbers.  Found %d\n", line_number, largs.size());
	    else if (largs.back() != 0)
		err(true, "Line #%d.  Line not zero-terminated\n", line_number);
	    // Find parent
	    int pnid = largs[0] + NODE_START;
	    auto fid = nodes.find(pnid);
	    if (fid == nodes.end())
		err(true, "Line #%d.  Invalid NNF node Id %d\n", line_number, largs[0]);
	    std::vector<int> *parent = fid->second;
	    // Find second node
	    int cnid = largs[1] + NODE_START;
	    fid = nodes.find(cnid);
	    if (fid == nodes.end())
		err(true, "Line #%d.  Invalid NNF node Id %d\n", line_number, largs[1]);
	    if (largs.size() > 3) {
		// Must construct AND node to hold literals
		int xid = XNODE_START + xcount++;
		std::vector<int> *xnode = new std::vector<int>;
		nodes[xid] = xnode;
		xnode->push_back(NNF_AND);
		for (int i = 2; i < largs.size()-1; i++)
		    xnode->push_back(largs[i]);
		xnode->push_back(cnid);
		report(6, "Line #%d. Created node %d to hold literals between nodes %d and %d\n",
		       line_number, xid, pnid, cnid);
		cnid = xid;
	    }
	    parent->push_back(cnid);
	    node_with_parent.insert(cnid);
	    report(6, "Line #%d.  Adding edge between nodes %d and %d\n", line_number, pnid, cnid);
	}
    }
    for (auto kv : nodes) {
	int nid = kv.first;
	std::vector<int> *node = kv.second;
	// Check OR nodes
	if ((*node)[0] == NNF_OR && node->size() == 2 && root_id == 0
	    && node_with_parent.find(nid) == node_with_parent.end()) {
	    root_id = nid;
	    report(6, "Setting root to %d\n", nid);
	}
    }
    if (root_id == 0)
	err(true, "Failed to find root node in NNF file\n");
    report(4, "Read D4 NNF file with %d nodes (%d explicit) and %d edges\n",
	   nnf_node_count, nnf_explicit_node_count, nnf_edge_count);
}

Nnf::~Nnf() {
    for (auto kv : nodes) {
	std::vector<int> *node = kv.second;
	delete node;
    }
}

void Nnf::show(FILE *outfile) {
    std::vector<int> ids;
    topo_order(ids);
    for (int nid : ids) {
	auto fid = nodes.find(nid);
	std::vector<int> *node = fid->second;
	int type = (*node)[0];
	fprintf(outfile, "%d: %s(", nid, nnf_type_name[type]);
	for (int i = 1; i < node->size(); i++) {
	    int cid = (*node)[i];
	    if (i == 1)
		fprintf(outfile, "%d", cid);
	    else
		fprintf(outfile, ", %d", cid);
	}
	fprintf(outfile, ")\n");
    }
    fprintf(outfile, "Root = %d\n", root_id);
}

// Topologically order NNF nodes reachable from root with root at end
void Nnf::topo_order(std::vector<int> &ids) {
    ids.clear();
    std::unordered_set<int> visited;
    visit(root_id, ids, visited);
}

    // Support for topological ordering
void Nnf::visit(int nid, std::vector<int> &ids, std::unordered_set<int> &visited) {
    if (nid < NODE_START) 
	return;
    if (visited.find(nid) != visited.end())
	return;
    visited.insert(nid);
    auto fid = nodes.find(nid);
    if (fid == nodes.end())
	err(true, "visit: Encountered invalid NNF node ID: %d\n", nid);
    std::vector<int> *node = fid->second;
    for (int i = 1; i < node->size(); i++)
	visit((*node)[i], ids, visited);
    ids.push_back(nid);
}


// Support for computing hash function over POG arguments
// Represent by signature over modular field

#define STATE_BYTES 256
#define CHUNK_SIZE 1024
static const unsigned hash_modulus = 2147483647U;
static char hash_state[STATE_BYTES];

static std::vector<unsigned> var_hash;

// Hash signatures for POG operations

static unsigned pog_hash[POG_NUM];

static void init_hash(int val) {
    int var = IABS(val);
    if (var > MAX_VARIABLE)
	err(true, "Attempt to create variable %d exceeds maximum of %d\n", var, MAX_VARIABLE);
    if (var_hash.size() == 0) {
	// Initialization
	initstate(1, hash_state, STATE_BYTES);
	for (int i = 0; i < POG_NUM; i++) {
	    pog_hash[i] = random() % hash_modulus;
	}
    }
    if (var >= var_hash.size()) {
	// Add more random values
	size_t osize = var_hash.size();
	size_t nsize = osize + (1 + (var - osize)/CHUNK_SIZE) * CHUNK_SIZE;
	var_hash.resize(nsize);
#if 0
	const char *ostate = setstate(hash_state);
#endif
	for (unsigned i = osize; i < nsize; i++)
	    var_hash[i] = random() % hash_modulus;
#if 0
	setstate(ostate);
#endif
    }
}

static unsigned next_hash_int(unsigned sofar, int val) {
    init_hash(val);
    int var = IABS(val);
    unsigned vval = var_hash[var];
    unsigned long  lval = val < 0 ? hash_modulus - vval : vval;
    return (lval * sofar) % hash_modulus;
}

unsigned Pog::node_hash(int var) {
    int idx = node_index(var);
    if (idx < 0)
	return 0;
    init_hash(var);
    unsigned sofar = pog_hash[(int) nodes[idx].type];
    int offset = nodes[idx].offset;
    int degree = nodes[idx].degree;
    for (int i = 0; i < degree; i++) {
	sofar = next_hash_int(sofar, arguments[offset + i]);
    }
    return sofar;
}

bool Pog::node_equal(int var1, int var2) {
    int idx1 = node_index(var1);
    int idx2 = node_index(var2);
    if (idx1 == idx2)
	return true;
    if (idx1 < 0 || idx2 < 0)
	return false;
    if (nodes[idx1].type != nodes[idx2].type)
	return false;
    int degree = nodes[idx1].degree;
    if (degree != nodes[idx2].degree)
	return false;
    int adx1 = nodes[idx1].offset;
    int adx2 = nodes[idx2].offset;
    for (int i = 0; i < degree; i++)
	if (arguments[adx1+i] != arguments[adx2+i])
	    return false;
    return true;
}

int Pog::get_decision_variable(int edge) {
    if (!is_sum(edge))
	return 0;
    int edge1 = get_argument(edge, 0);
    int n1 = 1;
    int *lits1 = &edge1;
    if (is_node(edge1)) {
	n1 = get_degree(edge1);
	lits1 = get_arguments(edge1);
    } 
    int edge2 = get_argument(edge, 1);
    int n2 = 2;
    int *lits2 = &edge2;
    if (is_node(edge2)) {
	n2 = get_degree(edge2);
	lits2 = get_arguments(edge2);
    } 
    for (int i1 = 0; i1 < n1; i1++) {
	int lit1 = lits1[i1];
	for (int i2 = 0; i2 < n2; i2++) {
	    int lit2 = lits2[i2];
	    if (lit1 == -lit2)
		return get_var(lit1);
	}
    }
    err(false, "Couldn't get decision variable for edge %d\n", edge);
    lprintf("Edge: ");
    show_edge(stdout, edge);
    lprintf("Edge1: ");
    show_edge(stdout, edge1);
    lprintf("Edge2: ");
    show_edge(stdout, edge2);
    err(true, "FATAL\n");
    return 0;
}

void Pog::start_node(pog_type_t type) {
    if (type != POG_PRODUCT && type != POG_SUM)
	err(true, "Trying to create node of unknown type %d\n", (int) type);
    // Create prototype node at end of list of nodes.  May retract later
    int nidx = nodes.size();
    nodes.resize(nidx + 1);
    nodes[nidx].offset = arguments.size();
    nodes[nidx].type = type;
    nodes[nidx].degree = 0;
    nodes[nidx].data_only = true;
    nodes[nidx].projection_only = true;
}

void Pog::add_argument(int edge) {
    int nidx = nodes.size()-1;
    pog_type_t type = nodes[nidx].type;
    int degree = nodes[nidx].degree;
    // See if already have dominating value
    if (degree == 1) {
    	int offset = nodes[nidx].offset;
    	int cedge = arguments[offset];
    	if (type == POG_PRODUCT && cedge == CONFLICT || type == POG_SUM && cedge == TAUTOLOGY)
    	    return;
	// Check for sum operation with complementary arguments
	if (type == POG_SUM && cedge == -edge) {
	    // Convert to tautology
	    arguments[offset] = TAUTOLOGY;
	    return;
	}
    }
    // Don't add non-dominating constants
    if (type == POG_PRODUCT && edge == TAUTOLOGY || type == POG_SUM && edge == CONFLICT)
	return;
    // Create unique argument for dominating constant
    if (type == POG_SUM && edge == TAUTOLOGY || type == POG_PRODUCT && edge == CONFLICT) {
	int aindex = nodes[nidx].offset;
	arguments.resize(aindex+1);
	arguments[aindex] = edge;
	nodes[nidx].degree = 1;
	return;
    }
    nodes[nidx].data_only = nodes[nidx].data_only && only_data_variables(edge);
    nodes[nidx].projection_only = nodes[nidx].projection_only && only_projection_variables(edge);
    // Merge arguments to product operation
    if (is_node(edge) && type == POG_PRODUCT && get_type(edge) == POG_PRODUCT && get_phase(edge)) {
	int edegree = get_degree(edge);
	for (int i = 0; i < edegree; i++)
	    arguments.push_back(get_argument(edge, i));
	nodes[nidx].degree += edegree;
    } else {
	arguments.push_back(edge);
	nodes[nidx].degree++;
    }
}

int Pog::finish_node() {
    int edge = 0;
    bool retract = false;
    int nidx = nodes.size()-1;
    pog_type_t type = nodes[nidx].type;
    int degree = nodes[nidx].degree;
    if (degree == 0) {
	// Operation with no arguments
	edge = type == POG_SUM ? CONFLICT : TAUTOLOGY;
	retract = true;
    } else if (degree == 1) {
	// Either single argument or dominating constant
	int offset = nodes[nidx].offset;
	edge = arguments[offset];
	retract = true;    
    } else {
	// Order arguments
	std::sort(arguments.end()-degree, arguments.end(), abs_less);
	// Look in hash table
	edge = nidx + nvar + 1;
	unsigned h = node_hash(edge);
	auto bucket = unique_table.equal_range(h);
	for (auto iter = bucket.first; iter != bucket.second; iter++) {
	    int oedge = iter->second;
	    if (node_equal(edge, oedge)) {
		edge = oedge;
		retract = true;
	    }
	}
	if (!retract) {
	    unique_table.insert({h, edge});
	    pog_type_t type = get_type(edge);
	    incr_count(type == POG_SUM ? COUNT_POG_SUM : COUNT_POG_PRODUCT);
	    incr_count_by(COUNT_POG_EDGES, degree);
	    if (verblevel >= 5) {
		report(5, "Added POG node ");
		show_edge(stdout, edge);
	    }
	}
    }
    if (retract) {
	arguments.resize(arguments.size() - degree);
	nodes.resize(nidx);
    }
    return edge;
}

int Pog::load_nnf(FILE *infile, std::unordered_set<int> *data_variables) {
    Nnf nnf(nvar, infile);
    if (verblevel >= 6) {
	nnf.show(stdout);
    }
    // Build POG from NNF graph
    // Recursively traverse from root
    // Mapping from NNF nodes to POG nodes
    std::vector<int> nnf_ids;
    nnf.topo_order(nnf_ids);
    // Mapping from NNF node IDs to POG edges
    std::unordered_map<int,int> nnid2edge;
    int edge = 0;
    for (int nnid : nnf_ids) {
	auto nfid = nnf.nodes.find(nnid);
	if (nfid == nnf.nodes.end())
	    err(true, "load_nnf.  Couldn't find NNF node %d\n", nnid);
	std::vector<int> *node = nfid->second;
	nnf_type_t ntype = (nnf_type_t) (*node)[0];
	edge = 0;
	switch(ntype) {
	case NNF_TRUE:
	    edge = TAUTOLOGY;
	    break;
	case NNF_FALSE:
	    edge = CONFLICT;
	    break;
	case NNF_AND:
	case NNF_OR:
	    start_node(ntype == NNF_AND ? POG_PRODUCT : POG_SUM);
	    for (int i = 1; i < node->size(); i++) {
		int nnf_arg = (*node)[i];
		int pog_arg = nnf_arg;
		// Negative values must be negated variables
		if (nnf_arg >= NODE_START) {
		    auto fid = nnid2edge.find(nnf_arg);
		    if (fid == nnid2edge.end())
			err(true, "Couldn't find NNF node with ID %d\n", nnf_arg);
		    pog_arg = fid->second;
		} else if (data_variables) {
		    // See if this is the literal of a projection variable
		    int nnf_var = IABS(nnf_arg);
		    if (data_variables->find(nnf_var) == data_variables->end())
			pog_arg = TAUTOLOGY;
		}
		add_argument(pog_arg);
	    }
	    edge = finish_node();
	    break;
	default:
	    err(true, "Invalid NNF node type %d\n", ntype);
	}
	nnid2edge[nnid] = edge;
	report(6, "NNF node %d --> POG edge %d\n", nnid, edge);
    }
    // Guaranteed that final result is root
    return edge;
}

void Pog::show_edge(FILE *outfile, int edge) {
    int var = get_var(edge);
    if (is_node(edge)) {
	int nidx = node_index(edge);
	if (nidx < 0)
	    err(true, "Couldn't find edge %d\n", edge);
	int degree = nodes[nidx].degree;
	pog_type_t type = nodes[nidx].type;
	int var = IABS(edge);
	fprintf(outfile, "%s%s_%d(", edge < 0 ? "-" : "", type == POG_PRODUCT ? "PRODUCT" : "SUM", var);
	for (int lid = 0; lid < degree; lid++) {
	    int clit = get_argument(edge, lid);
	    fprintf(outfile, lid == 0 ? "%d" : ", %d", clit);
	}
	fprintf(outfile, ")");
	if (nodes[nidx].data_only)
	    fprintf(outfile, "D");
	if (nodes[nidx].projection_only)
	    fprintf(outfile, "P");
	fprintf(outfile, "\n");
    } else {

	fprintf(outfile, "%sV%d\n", edge < 0 ? "-" : "", var);
    }
}


void Pog::visit(int edge, std::set<int> &visited) {
    if (!is_node(edge))
	return;
    int var = get_var(edge);
    if (visited.find(var) != visited.end())
	return;
    visited.insert(var);
    int degree = get_degree(edge);
    for (int i = 0; i < degree; i++)
	visit(get_argument(edge, i), visited);
}

void Pog::get_variables(int root, std::unordered_set<int> &vset) {
    if (!is_node(root)) {
	vset.insert(get_var(root));
	return;
    }
    std::set<int> visited;
    visit(root, visited);
    for (int edge : visited) {
	int degree = get_degree(edge);
	for (int i = 0; i < degree; i++) {
	    int cvar = get_var(get_argument(edge, i));
	    if (!is_node(cvar))
		vset.insert(cvar);
	}
    }
}

void Pog::show(int root, FILE *outfile) {
    std::set<int> visited;
    if (is_node(root)) {
	visit(root, visited);
	for (int edge : visited)
	    show_edge(outfile, edge);
    }
    fprintf(outfile, "ROOT %d\n", root);
}

void Pog::get_subgraph(std::vector<int> &root_edges, std::map<int,int> &node_remap) {
    node_remap.clear();
    // Collect all reachable nodes
    std::set<int> visited;
    for (int redge : root_edges)
	visit(redge, visited);
    int next_id = nvar+1;
    for (int oid : visited) {
	node_remap[oid] = next_id++;
    }
}

q25_ptr qmark(q25_ptr q, std::vector<q25_ptr> &qlog) {
    qlog.push_back(q);
    return q;
}

void qflush(std::vector<q25_ptr> &qlog) {
    for (q25_ptr val : qlog)
	q25_free(val);
    qlog.clear();
}

// weights should include weights of all data variables and their negations
q25_ptr Pog::ring_evaluate(int root_edge, std::unordered_map<int,q25_ptr> &weights) {
    std::unordered_map<int, q25_ptr> eweights;
    // Record allocated q25_ptr's both for each node and for entire evaluation
    std::vector<q25_ptr> qlog, eqlog;
    for (auto iter : weights)
	eweights[iter.first] = iter.second;
    std::set<int> visited;
    visit(root_edge, visited);
    for (int edge : visited) {
	int id = get_var(edge);
	int degree = get_degree(id);
	bool sum = is_sum(id);
	q25_ptr val = sum ? q25_from_32(0) : q25_from_32(1);
	for (int i = 0; i < degree; i++) {
	    int cedge = get_argument(id, i);
	    auto fid = eweights.find(cedge);
	    if (fid == eweights.end()) {
		int cvar = get_var(cedge);
		if (is_node(cvar)) {
		    err(false, "Couldn't find weight for edge %d representing POG node\n", cedge);
		} else {
		    if (data_variables->find(cvar) == data_variables->end()) {
			err(false, "Encountered projection variable %d as child of node %d\n", cvar, id);
			fprintf(stdout, "  Node: ");
			show_edge(stdout, id);
		    } else
			err(false, "Couldn't find weight for edge %d representing input variable\n", cedge);
		}
		qmark(val, qlog); qflush(qlog); qflush(eqlog);
		return q25_from_32(0);
	    }
	    q25_ptr wt = fid->second;
	    qmark(val, qlog);
	    val = sum ? q25_add(val, wt) : q25_mul(val, wt);
	}
	eweights[id] = qmark(val, eqlog);
	eweights[-id] = qmark(q25_one_minus(val), eqlog);
	qflush(qlog);
    }
    q25_ptr rval = NULL;
    if (root_edge == TAUTOLOGY)
	rval = q25_from_32(1);
    else if (root_edge == CONFLICT)
	rval = q25_from_32(0);
    else {
	auto fid = eweights.find(root_edge);
	if (fid == eweights.end()) {
	    err(false, "Couldn't find weight for root edge %d\n", root_edge);
	    qflush(eqlog);
	    return q25_from_32(0);
	}
	q25_ptr wt = fid->second;
	rval = q25_copy(wt);
    }
    qflush(eqlog);
    return rval;
}


// Extract subgraph with designated root edge and write to file
bool Pog::write(int root_edge, FILE *outfile) {
    if (outfile == NULL) {
	// Go through motions to capture stats
	if (!is_node(root_edge))
	    return true;
	std::vector<int> roots;
	roots.push_back(root_edge);
	std::map<int,int> node_remap;
	get_subgraph(roots, node_remap);
	int orvar = get_var(root_edge);
	int nrvar = node_remap[orvar];
	int nroot_edge = root_edge > 0 ? nrvar : -nrvar;
	for (auto kv : node_remap) {
	    int oid = kv.first;
	    incr_count(is_sum(oid) ? COUNT_POG_FINAL_SUM : COUNT_POG_FINAL_PRODUCT);
	    int degree = get_degree(oid);
	    incr_count_by(COUNT_POG_FINAL_EDGES, degree);
	}
	return true;
    }
    // The real thing
    if (!is_node(root_edge)) {
	int var = get_var(root_edge);
	if (var == TAUTOLOGY) {
	    int nrvar = nvar+1;
	    fprintf(outfile, "p %d\n", nrvar);
	    fprintf(outfile, "r %d\n", root_edge > 0 ? nrvar : -nrvar);
	} else {
	    fprintf(outfile, "r %d\n", root_edge);
	}
	return true;
    }
    std::vector<int> roots;
    roots.push_back(root_edge);
    std::map<int,int> node_remap;
    get_subgraph(roots, node_remap);
    int orvar = get_var(root_edge);
    int nrvar = node_remap[orvar];
    int nroot_edge = root_edge > 0 ? nrvar : -nrvar;
    fprintf(outfile, "r %d\n", nroot_edge);

    for (auto kv : node_remap) {
	int oid = kv.first;
	int nid = kv.second;
	fprintf(outfile, "%c %d", is_sum(oid) ? 's' : 'p', nid);
	incr_count(is_sum(oid) ? COUNT_POG_FINAL_SUM : COUNT_POG_FINAL_PRODUCT);
	int degree = get_degree(oid);
	incr_count_by(COUNT_POG_FINAL_EDGES, degree);
	for (int i = 0; i < degree; i++) {
	    int oedge = get_argument(oid, i);
	    int nedge = oedge;
	    if (is_node(oedge)) {
		int ovar = get_var(oedge);
		int nvar = node_remap[ovar];
		nedge = oedge > 0 ? nvar : -nvar;
	    }
	    fprintf(outfile, " %d", nedge);
	}
	fprintf(outfile, "\n");
    }
    return true;
}

// Simple KC when formula is conjunction of independent clauses
// Argument is sequence of clause literals, separated by zeros
int Pog::simple_kc(std::vector<int> &clause_chunks) {
    std::vector<int> arguments;
    std::vector<int> clause;
    for (int lit : clause_chunks) {
	if (lit == 0) {
	    arguments.push_back(build_disjunction(clause));
	    clause.clear();
	} else
	    clause.push_back(lit);
    }
    if (arguments.size() == 0)
	return TAUTOLOGY;
    else if (arguments.size() == 1)
	return arguments[0];
    start_node(POG_PRODUCT);
    for (int alit : arguments)
	add_argument(alit);
    int nedge = finish_node();
    return nedge;
}

int Pog::build_disjunction(std::vector<int> &args) {
    int nedge = 0;
    if (args.size() == 0)
	nedge = CONFLICT;
    else if (args.size() == 1)
	nedge = args[0];
    else {
	// DeMorgan's construction
	start_node(POG_PRODUCT);
	for (int clit : args)
	    add_argument(-clit);
 	nedge = -finish_node();
    }
    return nedge;
}

    

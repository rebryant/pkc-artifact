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

#include <cstdlib>
#include <cstdio>
#include <ctype.h>
#include <cstring>
#include <queue>
#include <algorithm>

#include "report.h"
#include "counters.h"
#include "files.hh"
#include "compile.hh"
// From glucose
#include "Solver.h"

// Experimental version of BVE used randomization to break ties with variable selection
// Interesting and elegant, but not an improvement.
// Disabled here

#define RANDOM_BVE 0
#define STACK_BVE 0

// Implementation of FIFO queues that don't store duplicates
template <typename T> class unique_queue {
private:
    std::queue<T>q;
    std::unordered_set<T>elements;

    void quick_push(T val) { q.push(val); elements.insert(val); }

public:

    unique_queue() {}

    unique_queue(std::set<T> &vals) {
	for (T val : vals)
	    quick_push(val);
    }

    unique_queue(std::unordered_set<T> &vals) {
	for (T val : vals)
	    quick_push(val);
    }

    unique_queue(std::set<T> *vals) {
	for (T val : *vals)
	    quick_push(val);
    }

    unique_queue(std::unordered_set<T> *vals) {
	for (T val : *vals)
	    quick_push(val);
    }

    
    bool push(T val) {
	bool new_val = !is_member(val);
	if (new_val)
	    quick_push(val);
	return new_val;
    }

    bool empty() {
	return q.empty();
    }

    bool is_member(T val) {
	return elements.find(val) != elements.end();
    }

    T get_and_pop() {
	T val = q.front();
	elements.erase(val);
	q.pop();
	return val;
    }

};

#if RANDOM_BVE
///////////////////////////////////////////////////////////////////
// Managing pairs of ints packed into 64-bit word
// Use these both as edge identifier
// and as cost+unique value
//
// When combining cost functions with unique identifiers
// Have upper 32 bits represent cost
// and lower 32 bits represent unique value, 
//   assigned either sequentially or via a pseudo-random sequence
///////////////////////////////////////////////////////////////////

/*
  32-bit words packed into pairs
 */
static int64_t pack(int upper, int lower) {
    return ((int64_t) upper << 32) | lower;
}


static int64_t ordered_pack(int x1, int x2) {
    return x1 < x2 ? pack(x1, x2) : pack(x2, x1);
}

static int upper(int64_t pair) {
    return (int) (pair>>32);
}

static int lower(int64_t pair) {
    return (int) (pair & 0xFFFFFFFF);
}
#endif

#if RANDOM_BVE
// A handy class for generating random numbers with own seed
// Pseudo-random number generator based on the Lehmer MINSTD RNG
// Use to both randomize selection and to provide a unique value
// for each cost.

class Sequencer {

public:
    Sequencer(int s) { seed = s; }

    Sequencer(void) { seed = default_seed; }

    Sequencer(Sequencer &s) { seed = s.seed; }

    void set_seed(uint64_t s) { seed = s == 0 ? 1 : s; next() ; next(); }

    uint32_t next() { seed = (seed * mval) % groupsize; return seed; }

    // Return next pseudo random value in interval [0.0, 1.0)
    double pseudo_double() { uint32_t val = next(); return (double) val / (double) groupsize; }

    // Return next pseudo random value in range [0, m)
    int pseudo_int(int m) { return (int) (m * pseudo_double()); }

private:
    uint64_t seed;
    const uint64_t mval = 48271;
    const uint64_t groupsize = 2147483647LL;
    const uint64_t default_seed = 123456;

};
#endif


//////////////// Reading CNF FILE ///////////////////

// Put literals in ascending order of the variables
static bool abs_less(int x, int y) {
    return IABS(x) < IABS(y);
}


static int skip_line(FILE *infile) {
    int c;
    while ((c = getc(infile)) != EOF) {
	if (c == '\n')
	    return c;
    }
    return c;
}

// Skip over spaces, newlines, etc., until find something interesting
// Return last character encountered
static int find_token(FILE *infile) {
    int c;
    while ((c = getc(infile)) != EOF) {
	if (!isspace(c)) {
	    ungetc(c, infile);
	    break;
	}
    }
    return c;
}

// Read string token:
// Skip over spaces.
// Read contiguous non-space characters and store in dest.
// Set len to number of characters read.
// Return false if EOF encountered without getting string
static bool find_string_token(FILE *infile, char *dest, int maxlen, int *len) {
    int c;
    int rlen = 0;
    while ((c = getc(infile)) != EOF && rlen < maxlen-1) {
	if (isspace(c)) {
	    if (rlen > 0) {
		// Past token
		ungetc(c, infile);
		break;
	    }
	} else {
	    *(dest+rlen) = c;
	    rlen++;
	}
    }
    *(dest+rlen) = '\0';
    *len = rlen;
    return (c != EOF);
}

// Process comment, looking additional data variables & weights
// Return last character
static void process_comment(FILE *infile, std::unordered_set<int> *data_variables , std::unordered_map<int,q25_ptr> *input_weights) {
    char buf[50];
    int len;
    if (find_string_token(infile, buf, 50, &len) && len == 1 && strncmp(buf, "p", 1) == 0
	&& find_string_token(infile, buf, 50, &len)) {
	if (len == 4 && strncmp(buf, "show", 4) == 0) {
	    int var = -1;
	    while (var != 0) {
		if (fscanf(infile, "%d", &var) != 1) {
		    err(false, "Couldn't read data variable\n");
		    break;
		} else if (var != 0) {
		    data_variables->insert(var);
		}
	    }
	}
	else if (len == 6 && strncmp(buf, "weight", 6) == 0) {
	    int lit = 0;
	    if (fscanf(infile, "%d", &lit) != 1) {
		err(false, "Couldn't read weight literal (skipping)\n");
		skip_line(infile);
		return;
	    }
	    find_token(infile);
	    q25_ptr wt = q25_read(infile);
	    if (!q25_is_valid(wt)) {
		err(false, "Couldn't read weight for literal %d (skipping)\n", lit);
		skip_line(infile);
		return;
	    }
	    (*input_weights)[lit] = wt;
	    int zero;
	    if (fscanf(infile, "%d", &zero) != 1 || zero != 0) {
		err(false, "Couldn't read terminating zero in weight declaration for literal %d (accepting weight)\n", lit);
	    }

	}
    }
    skip_line(infile);
}		

Cnf::Cnf() {
    variable_type = NULL;
    data_variables = NULL;
    tseitin_variables = NULL;
    active_clauses = NULL;
    literal_clauses = NULL;
    input_weights = NULL;
    initialize(0);
}

Cnf::Cnf(int input_count) {
    variable_type = NULL;
    data_variables = NULL;
    tseitin_variables = NULL;
    active_clauses = NULL;
    literal_clauses = NULL;
    input_weights = NULL;
    initialize(input_count);
}

Cnf::~Cnf() {
    // Don't deallocate these.  They get used elsewhere
    //    delete data_variables;
    //    delete input_weights; 
}

void Cnf::initialize(int input_count) {
    nvar = input_count;
    if (variable_type)
	delete variable_type;
    variable_type = new var_t[nvar];
    for (int v = 1; v <= nvar; v++)
	set_variable_type(v, VAR_UNUSED);
    clause_offset.clear();
    literal_sequence.clear();
    if (data_variables)
	data_variables->clear();
    else
	data_variables = new std::unordered_set<int>;
    if (tseitin_variables) {
	tseitin_variables->clear();
    } else
	tseitin_variables = new std::unordered_set<int>;
    if (active_clauses) {
	active_clauses->clear();
    } else
	active_clauses = new std::set<int>;
    if (literal_clauses) {
	literal_clauses->clear();
    } else
	literal_clauses = new std::unordered_map<int,std::unordered_set<int>>;
    if (input_weights)
	input_weights->clear();
    else
	input_weights = new std::unordered_map<int, q25_ptr>;
    new_clause();
    has_conflict = false;
    action_stack.clear();
    new_context();
    unit_literals.clear();
    bcp_unit_literals.clear();
    uquantified_variables.clear();
}

// Must explicitly deallocate sets
void Cnf::deallocate() {
    delete variable_type;
    delete data_variables;
    delete tseitin_variables;
    delete active_clauses;
    delete literal_clauses;
    delete input_weights;
}

int Cnf::new_clause() {
    int cid = clause_offset.size();
    clause_offset.push_back(literal_sequence.size());
    if (cid > 0)
	active_clauses->insert(cid);
    return cid;
}

void Cnf::add_literal(int lit) {
    literal_sequence.push_back(lit);
    clause_offset.back() ++;
    int cid = clause_offset.size()-1;
    (*literal_clauses)[lit].insert(cid);
    int var = IABS(lit);
    if (get_variable_type(var) == VAR_UNUSED)
	set_variable_type(var, VAR_NONTSEITIN);
}

void Cnf::finish() {
    report(3, "CNF representation with %d inputs and %d clauses constructed\n",
	   variable_count(), maximum_clause_id());
}

bool Cnf::import_file(FILE *infile, bool process_comments) { 
    int expectedNclause = 0;
    bool read_failed = false;
    bool got_header = false;
    int c;
    bool eof = false;
    // Look for CNF header
    while ((c = getc(infile)) != EOF) {
	if (isspace(c)) 
	    continue;
	if (c == 'c') {
	    if (process_comments)
		process_comment(infile, data_variables, input_weights);
	    else
		skip_line(infile);
	    continue;
	}
	if (c == EOF) {
	    err(false, "Not valid CNF file.  No header line found\n");
	    return false;
	}
	if (c == 'p') {
	    char field[20];
	    if (fscanf(infile, "%s", field) != 1) {
		err(false, "Not valid CNF file.  Invalid header line\n");
		return false;
	    }
	    if (strcmp(field, "cnf") != 0) {
		err(false, "Not valid CNF file.  Header line shows type is '%s'\n", field);
		return false;
	    }
	    if (fscanf(infile, "%d %d", &nvar, &expectedNclause) != 2) {
		err(false, "Invalid CNF header\n");
		return false;
	    } 
	    initialize(nvar);
	    c = skip_line(infile);
	    got_header = true;
	    break;
	}
	if (c == EOF) {
	    err(false, "Invalid CNF.  EOF encountered before reading any clauses\n");
	    return false;
	}
    }
    if (!got_header) {
	err(false, "Not valid CNF.  No header line found\n");
	return false;
    }
    while (maximum_clause_id() < expectedNclause) {
	// Setup next clause
	new_clause();
	bool starting_clause = true;
	while (true) {
	    int lit;
	    int c = find_token(infile);
	    if (c == EOF) {
		err(false, "Unexpected end of file\n");
		return false;
	    } else if (c == 'c' && starting_clause) {
		c = getc(infile);
		if (process_comments)
		    process_comment(infile, data_variables, input_weights);
		else
		    skip_line(infile);
		continue;
	    }
	    else if (fscanf(infile, "%d", &lit) != 1) {
		err(false, "Couldn't find literal or 0\n");
		return false;
	    }
	    if (lit == 0)
		break;
	    else 
		add_literal(lit);
	    starting_clause = false;
	}
    }
    while ((c = getc(infile)) != EOF) {
	if (isspace(c)) 
	    continue;
	if (c == 'c') {
	    if (process_comments)
		process_comment(infile, data_variables, input_weights);
	    else
		skip_line(infile);

	}
    }
    // If no data variables declared, assume all input variables are data variables
    if (data_variables->size() == 0) {
	for (int v = 1; v <= variable_count(); v++)
	    data_variables->insert(v);
    }
    for (int v : *data_variables)
	set_variable_type(v, VAR_DATA);
    incr_count_by(COUNT_INPUT_CLAUSE, maximum_clause_id());
    return true;
}

int Cnf::clause_length(int cid) {
    if (cid < 1 || cid > maximum_clause_id())
	err(true, "Invalid clause ID: %d\n", cid);
    return clause_offset[cid] - clause_offset[cid-1];
}

int Cnf::maximum_clause_id() {
    return clause_offset.size() - 1;
}

int Cnf::nonunit_clause_count() {
    return active_clauses->size();
}

int Cnf::current_clause_count() {
    return active_clauses->size() + bcp_unit_literals.size();
}

int Cnf::get_literal(int cid, int lid) {
    int len = clause_length(cid);
    int offset = clause_offset[cid-1];
    int lit = 0;
    if (lid >= 0 && lid < len)
	lit = literal_sequence[offset+lid];
    else
	err(true, "Invalid literal index %d for clause #%d.  Clause length = %d\n", lid, cid, len);
    return lit;
}

void Cnf::swap_literals(int cid, int i, int j) {
    int offset = clause_offset[cid-1];
    int tlit = literal_sequence[offset+i];
    literal_sequence[offset+i] = literal_sequence[offset+j];
    literal_sequence[offset+j] = tlit;
}

bool Cnf::show(FILE *outfile) {
    for (int lit : bcp_unit_literals)
	fprintf(outfile, "  UNIT: %d\n", lit);
    for (int cid : *active_clauses) {
	if (skip_clause(cid))
	    continue;
	int len = clause_length(cid);
	fprintf(outfile, "  %d:", cid);
	for (int lid = 0; lid < len; lid++) {
	    int lit = get_literal(cid, lid);
	    if (!skip_literal(lit))
		fprintf(outfile, " %d", get_literal(cid, lid));
	}
	fprintf(outfile, "\n");
    }
    return true;
}

bool Cnf::write(FILE *outfile, bool show_data_and_tseitin_variables) {
    int nvar = variable_count();
    // Figure out which unit literals are data variables
    std::vector<int> data_literals;
    int removed_literals = 0;
    for (int lit : bcp_unit_literals) {
	int var = IABS(lit);
	if (is_data_variable(var))
	    data_literals.push_back(lit);
	else
	    removed_literals++;
    }
    if (show_data_and_tseitin_variables && data_variables) {
	fprintf(outfile, "c t pmc\n");
	fprintf(outfile, "c p show");
	for (int v : *data_variables) 
	    fprintf(outfile, " %d", v);
	for (int v : *tseitin_variables)
	    fprintf(outfile, " %d", v);
	fprintf(outfile, " 0\n");
    }
    fprintf(outfile, "p cnf %d %d\n", nvar, current_clause_count()-removed_literals);
    for (int lit : data_literals)
	fprintf(outfile, "%d 0\n", lit);
    for (int cid : *active_clauses) {
	if (skip_clause(cid))
	    // Hopefully won't need to do this
	    fprintf(outfile, "1 -1 0\n");
	int len = clause_length(cid);
	for (int lid = 0; lid < len; lid++) {
	    int lit = get_literal(cid, lid);
	    if (!skip_literal(lit))
		fprintf(outfile, "%d ", lit); 
	}
	fprintf(outfile, "0\n");
    }
    return true;
}

bool Cnf::is_satisfiable() {
    if (verblevel >= 5) {
	printf("Calling is_satisfiable for clauses:\n");
	show(stdout);
    }
    bcp(false);
    if (has_conflict)
	return false;

    double start = tod();
    int clause_count = 0;
    Glucose::Solver solver;
    solver.verbosity = 0;
    auto plit = new Glucose::Lit[nvar];
    auto nlit = new Glucose::Lit[nvar];
    for (int v = 1; v <= nvar; v++) {
	Glucose::Var gvar = solver.newVar(true, true);
	plit[v-1] = Glucose::mkLit(gvar, true);
	nlit[v-1] = Glucose::mkLit(gvar, false);
    }
    Glucose::vec<Glucose::Lit> gclause;
    for (int lit : bcp_unit_literals) {
	int var = IABS(lit);
	gclause.clear();
	Glucose::Lit glit = lit > 0 ? plit[var-1] : nlit[var-1];
	gclause.push(glit);
	solver.addClause(gclause);
	clause_count++;
    }
    for (int cid : *active_clauses) {
	if (skip_clause(cid))
	    continue;
	gclause.clear();
	int len = clause_length(cid);
	for (int lid = 0; lid < len; lid++) {
	    int lit = get_literal(cid, lid);
	    if (skip_literal(lit))
		continue;
	    int var = IABS(lit);
	    Glucose::Lit glit = lit > 0 ? plit[var-1] : nlit[var-1];
	    gclause.push(glit);
	}
	solver.addClause(gclause);
	clause_count++;
    }
    bool result = solver.solve();
    double elapsed = tod() - start;
    incr_timer(TIME_SAT, elapsed);
    incr_count(COUNT_SAT_CALL);
    incr_histo(HISTO_SAT_CLAUSES, clause_count);
    delete[] plit;
    delete[] nlit;
    report(5, "Calling SAT solver on problem with %d variables and %d clauses yields %s\n",
	   nvar, clause_count, result ? "SAT" : "UNSAT");
    return result;
}

void Cnf::new_context() {
    action_stack.push_back({ACTION_START_CONTEXT, 0});    
}

void Cnf::pop_context() {
    while (true) {
	action_record ar = action_stack.back();
	active_record avr;
	action_stack.pop_back();
	switch (ar.action) {
	case ACTION_START_CONTEXT:      // Start of new context
	    return;
	case ACTION_CONFLICT:           // Found conflict
	    has_conflict = false;
	    break;
	case ACTION_DEACTIVATE_CLAUSE:  // Deactivated clause
	    activate_clause(ar.ele);
	    break;
	case ACTION_BCP:                // Derived unit literal by BCP
	    bcp_unit_literals.erase(ar.ele);
	    unit_literals.erase(ar.ele);
	    break;
	case ACTION_ASSERT:             // Asserted literal externally
	    unit_literals.erase(ar.ele);
	    break;
	case ACTION_ASSERT_FROM_BCP:    // Converted BCP unit literal into asserted literal
	    bcp_unit_literals.insert(ar.ele);
	    break;
	case ACTION_UQUANTIFY:           // Variable was universally quantfied
	    uquantified_variables.erase(ar.ele);
	    break;
	case ACTION_ACTIVE_CLAUSES:     // Changed set of active clauses
	    delete active_clauses;
	    delete literal_clauses;
	    avr = active_stack.back();
	    active_stack.pop_back();
	    active_clauses = avr.active_clauses;
	    literal_clauses = avr.literal_clauses;
	    break;
	default:
	    err(true, "Unknown action on action stack.  Value = %d\n", ar.action);
	}
    }
}

void Cnf::assign_literal(int lit, bool bcp) {
    int var = IABS(lit);
    if (var == 0 || var > nvar) {
	err(true, "Can't assign literal %d\n", lit);
    }
    bool was_unit = unit_literals.find(lit) != unit_literals.end();
    bool was_bcp_unit = bcp_unit_literals.find(lit) != bcp_unit_literals.end();
    
    if (unit_literals.find(-lit) != unit_literals.end()) {
	// Conflict
	trigger_conflict();
	return;
    }
    if (bcp) {
	if (was_unit) {
	    err(false, "Attempt to set literal %d by BCP that is already unit\n", lit);
	} else {
	    unit_literals.insert(lit);
	    bcp_unit_literals.insert(lit);
	    action_stack.push_back({ACTION_BCP, lit});
	}
    } else {
	if (was_unit && !was_bcp_unit) {
	    err(false, "Attempt to assert literal %d that is already unit\n", lit);
	} else {
	    if (was_bcp_unit) {
		bcp_unit_literals.erase(lit);
		action_stack.push_back({ACTION_ASSERT_FROM_BCP, lit});
	    } else {
		unit_literals.insert(lit);
		action_stack.push_back({ACTION_ASSERT, lit});
	    }
	}
    }
}

void Cnf::uquantify_variable(int var) {
    uquantified_variables.insert(var);
    action_stack.push_back({ACTION_UQUANTIFY, var});
}

void Cnf::activate_clause(int cid) {
    int len = clause_length(cid);
    for (int lid = 0; lid < len; lid++) {
	int lit = get_literal(cid, lid);
	(*literal_clauses)[lit].insert(cid);
    }
    active_clauses->insert(cid);
}

void Cnf::push_active(std::set<int> *nactive_clauses) {
    active_stack.push_back({active_clauses,literal_clauses});
    action_stack.push_back({ACTION_ACTIVE_CLAUSES,0});
    active_clauses = nactive_clauses;
    literal_clauses = new std::unordered_map<int,std::unordered_set<int>>;
    for (int cid : *active_clauses) {
	int len = clause_length(cid);
	for (int lid = 0; lid < len; lid++) {
	    int lit = get_literal(cid, lid);
	    if (!skip_literal(lit))
		(*literal_clauses)[lit].insert(cid);
	}
    }
}


// Mark clause for deactivation once iterator completes
// Clause is no longer considered part of clausal state
void Cnf::deactivate_clause(int cid) {
    int len = clause_length(cid);
    for (int lid = 0; lid < len; lid++) {
	int lit = get_literal(cid, lid);
	(*literal_clauses)[lit].erase(cid);
    }
    active_clauses->erase(cid);
    action_stack.push_back({ACTION_DEACTIVATE_CLAUSE, cid});
}

void Cnf::deactivate_clauses(std::vector<int> &remove) {
    for (int cid : remove)
	deactivate_clause(cid);
}

bool Cnf::skip_clause(int cid) {
    int len = clause_length(cid);
    for (int lid = 0; lid < len; lid++) {
	int lit = get_literal(cid, lid);
	if (unit_literals.find(lit) != unit_literals.end())
	    return true;
    }
    return false;
}

bool Cnf::skip_literal(int lit) {
    if  (unit_literals.find(-lit) != unit_literals.end())
	return true;
    int var = IABS(lit);
    if (uquantified_variables.find(var) != uquantified_variables.end())
	return true;
    return false;
}

void Cnf::trigger_conflict() {
    has_conflict = true;
    action_stack.push_back({ACTION_CONFLICT, 0});
}

// Return TAUTOLOGY, CONFLICT, propagated unit, or zero
int Cnf::propagate_clause(int cid) {
    int len = clause_length(cid);
    int result = CONFLICT;
    for (int lid = 0; lid < len; lid++) {
	int lit = get_literal(cid, lid);
	if (unit_literals.find(lit) != unit_literals.end()) {
	    result = TAUTOLOGY;
	    break;
	}
	if (skip_literal(lit))
	    continue;
	if (result == CONFLICT)
	    result = lit;
	else
	    result = 0;
    }
    return result;
}


int Cnf::bcp(bool preprocess) {
    unique_queue<int> clause_queue(active_clauses);
    int count = 0;
    while (!has_conflict && !clause_queue.empty()) {
	int cid = clause_queue.get_and_pop();
	if (active_clauses->find(cid) == active_clauses->end())
	    continue;
	int rval = propagate_clause(cid);
	if (rval == CONFLICT)
	    trigger_conflict();
	else if (rval == 0)
	    continue;
	else if (rval == TAUTOLOGY)
	    deactivate_clause(cid);
	else {
	    int lit = rval;
	    int var = IABS(lit);
	    if (preprocess)
		set_variable_type(var, VAR_ELIM);
	    std::vector<int> remove;
	    assign_literal(lit, true);
	    deactivate_clause(cid);
	    for (int ocid : (*literal_clauses)[lit]) {
		if (active_clauses->find(ocid) != active_clauses->end())
		    remove.push_back(ocid);
	    }
	    deactivate_clauses(remove);
	    if (literal_clauses->find(-lit) != literal_clauses->end()) {
		for (int ocid : (*literal_clauses)[-lit])
		    if (active_clauses->find(ocid) != active_clauses->end())
			clause_queue.push(ocid);
	    }
	    count++;
	}
    }
    return count;
}

void Cnf::set_variable_type(int var, var_t type) {
    if (var <= 0 || var > nvar)
	err(true, "Attempted to set type of variable %d to %d\n", var, (int) type);
    variable_type[var-1] = type;
}

var_t Cnf::get_variable_type(int var) {
    if (var <= 0 || var > nvar)
	err(true, "Attempted to get type of variable %d\n", var);
    return variable_type[var-1];
}


int Cnf::get_variable_type_count(var_t type) {
    int count = 0;
    for (int v = 1; v <= nvar; v++) {
	if (get_variable_type(v) == type)
	    count++;
    }
    return count;
}

bool Cnf::check_simple_pkc(std::vector<int> &clause_chunks) {
    // Assume have performed BCP
    // Assume any pure projection variable has already been made into a BCP unit
    clause_chunks.clear();
    std::unordered_set<int> vset;
    bool ok = true;
    bool conflict = false;
    for (int cid : *active_clauses) {
	if (skip_clause(cid))
	    continue;
	int len = clause_length(cid);
	int plen = 0;
	for (int lid = 0; ok && lid < len; lid++) {
	    int lit = get_literal(cid, lid);
	    if (skip_literal(lit))
		continue;
	    int var = IABS(lit);
	    if (vset.find(var) != vset.end())
		ok = false;
	    else {
		vset.insert(var);
		clause_chunks.push_back(lit);
		plen++;
		conflict = false;
	    }
	}
	if (ok) {
	    clause_chunks.push_back(0);
	    if (plen == 0) {
		conflict = true;
		break;
	    }
	}
	else
	    break;
    }
    if (conflict) {
	clause_chunks.resize(2);
	clause_chunks[0] = clause_chunks[1] = 0;
	return ok;
    }
    if (ok) {
	for (int lit : bcp_unit_literals) {
	    if (!is_data_variable(IABS(lit)))
		continue;
	    clause_chunks.push_back(lit);
	    clause_chunks.push_back(0);
	}
    }
    return ok;
}

// Choose splitting variable.  Choose first bipolar literal.  If none, return some unipolar literal
int Cnf::find_split(bool defer) {
    std::set<int> literals;
    for (int cid: *active_clauses) {
	if (skip_clause(cid))
	    continue;
	int len = clause_length(cid);
	for (int lid = 0; lid < len; lid++) {
	    int lit = get_literal(cid, lid);
	    if (skip_literal(lit))
		continue;
	    literals.insert(lit);
	}
    }
    // Choose first positive bipolar one
    for (int lit : literals) {
	if (lit > 0)
	    break;
	if (literals.find(-lit) != literals.end())
	    return IABS(lit);
    }
    // Choose arbitrary unipolar literal
    for (int lit : literals)
	return IABS(lit);
    // Shouldn't get here
    err(false, "Couldn't find any literal while looking for splitting variable\n");
    return 0;
}

///// Support for bounded variable elimination

int Cnf::resolve(int var, int cid1, int cid2) {
    // Merge two sets of literals
    std::vector<int> mlits;
    int len1 = clause_length(cid1);
    for (int lid1 = 0; lid1 < len1; lid1++) {
	int lit1 = get_literal(cid1, lid1);
	int var1 = IABS(lit1);
	if (var1 == var)
	    continue;
	if (skip_literal(lit1))
	    continue;
	mlits.push_back(lit1);
    }
    int len2 = clause_length(cid2);
    for (int lid2 = 0; lid2 < len2; lid2++) {
	int lit2 = get_literal(cid2, lid2);
	int var2 = IABS(lit2);
	if (var2 == var)
	    continue;
	if (skip_literal(lit2))
	    continue;
	mlits.push_back(lit2);
    }
    std::sort(mlits.begin(), mlits.end(), abs_less);
    int last_lit = 0;
    // Generate literals for resolvent
    std::vector<int> nlits;
    for (int lit : mlits) {
	if (lit == last_lit)
	    continue;
	if (lit == -last_lit) {
	    // Tautology
	    report(5, "Resolving clauses %d and %d (variable %d) yields tautology\n", cid1, cid2, var);
	    return 0;
	}
	nlits.push_back(lit);
	last_lit = lit;
    }
    int cid = new_clause();
    for (int lit : nlits)
	add_literal(lit);
    report(5, "Resolving clauses %d and %d (variable %d) yields clause %d\n", cid1, cid2, var, cid);
    return cid;
}

#if RANDOM_BVE
int Cnf::bve(bool preprocess, int maxdegree) {
    // Limit on number of added claues.  Based on number when have balanced elimination
    int maxadded = maxdegree*maxdegree - 2*maxdegree;
    // Projection variables
    std::unordered_set<int> proj_variables;
    // Variables with sufficiently low degree, queued by degree+random number
    std::map<int64_t,int> candidate_variables;
    std::unordered_map<int,int64_t> inverse_map;
    int seed = 123456;
    Sequencer seq(seed);
    int eliminated_count = 0;
    for (int cid: *active_clauses) {
	int len = clause_length(cid);
	for (int lid = 0; lid < len; lid++) {
	    int lit = get_literal(cid, lid);
	    int var = IABS(lit);
	    if (skip_literal(lit))
		continue;
	    if (is_data_variable(var))
		continue;
	    if (proj_variables.find(var) != proj_variables.end())
		continue;
	    proj_variables.insert(var);
	    int degree = IMIN((*literal_clauses)[lit].size(), (*literal_clauses)[-lit].size());;
 	    if (degree <= maxdegree) {
		int64_t key = pack(degree, seq.next());
		candidate_variables[key] = var;
		inverse_map[var] = key;
	    }
	    report(5, "Projection variable %d.  Degree = %d\n", var, degree);
	}
    }
    // Iteratively eliminate variables where one literal is of low degree
    while (candidate_variables.size() > 0) {
	auto find = candidate_variables.begin();
	int64_t key = find->first;
	int64_t var = find->second;
	candidate_variables.erase(key);
	inverse_map.erase(var);
	int dpos = (*literal_clauses)[var].size();
	int dneg = (*literal_clauses)[-var].size();
	int degree = IMIN(dpos,dneg);
	// Literal with lower degree
	int lit = dneg < dpos ? -var : var;
	int deprecated_clause_count =  dpos + dneg;
	int max_delta_clause_count = dpos * dneg - deprecated_clause_count;
	if (max_delta_clause_count > maxadded)
	    // Skip.  Might generate too many clauses
	    continue;
	// Perform BVE on var
	int new_clause_count = 0;
	eliminated_count++;
	if (preprocess)
	    set_variable_type(var, VAR_ELIM);
	std::unordered_set<int>change_variables;
	std::vector<int>deprecate_clauses;
	for (int cid1 : (*literal_clauses)[lit]) {
	    deprecate_clauses.push_back(cid1);
	    int len1 = clause_length(cid1);
	    for (int lid1 = 0; lid1 < len1; lid1++) {
		int lit1 = get_literal(cid1, lid1);
		if (skip_literal(lit1))
		    continue;
		if (lit1 == lit)
		    continue;
		int var1 = IABS(lit1);
		if (is_data_variable(var1))
		    continue;
		change_variables.insert(var1);
	    }
	}
	for (int cid2 : (*literal_clauses)[-lit]) {
	    deprecate_clauses.push_back(cid2);
	    int len2 = clause_length(cid2);
	    for (int lid2 = 0; lid2 < len2; lid2++) {
		int lit2 = get_literal(cid2, lid2);
		if (skip_literal(lit2))
		    continue;
		if (lit2 == -lit)
		    continue;
		int var2 = IABS(lit2);
		if (is_data_variable(var2))
		    continue;
		change_variables.insert(var2);
	    }
	}
	for (int cid1 : (*literal_clauses)[lit]) {
	    for (int cid2 : (*literal_clauses)[-lit]) {
		int ncid = resolve(var, cid1, cid2);
		if (ncid > 0)
		    new_clause_count++;
	    }
	}
	deactivate_clauses(deprecate_clauses);
	for (int ovar : change_variables) {
	    auto find = inverse_map.find(ovar);
	    if (find != inverse_map.end()) {
		int64_t key = find->second;
		inverse_map.erase(ovar);
		candidate_variables.erase(key);
	    }
	    int odegree = IMIN((*literal_clauses)[ovar].size(), (*literal_clauses)[-ovar].size());
	    if (odegree <= maxdegree) {
		int64_t okey = pack(odegree, seq.next());
		candidate_variables[okey] = ovar;
		inverse_map[ovar] = okey;
		report(5, "Projection variable %d.  Degree = %d\n", ovar, odegree);
	    }
	}
	if (degree == 0 && bcp_unit_literals.find(-lit) == bcp_unit_literals.end())
	    // Pure literal
	    assign_literal(-lit, true);
	report(3, "BVE on variable %d deprecated %d clauses and added %d new ones\n", var, deprecated_clause_count, new_clause_count);
	if (preprocess) {
	    incr_count_by(COUNT_BVE_ELIM_CLAUSE, deprecated_clause_count);
	    incr_count_by(COUNT_BVE_NEW_CLAUSE, new_clause_count);
	}
    }
    return eliminated_count;
}
#endif // RANDOM_BVE

#if !RANDOM_BVE
int Cnf::bve(bool preprocess, int maxdegree) {
    // Limit on number of added claues.  Based on number when have balanced elimination
    int maxadded = maxdegree*maxdegree - 2*maxdegree;
    // Projection variables
    std::unordered_set<int> proj_variables;
    // Variables with sufficiently low degree
#if STACK_BVE
    std::vector<int> degree_variables[maxdegree+1];
#else
    std::unordered_set<int> degree_variables[maxdegree+1];
#endif
    // Eliminated variables
    std::unordered_set<int> eliminated_variables;
    for (int cid: *active_clauses) {
	int len = clause_length(cid);
	for (int lid = 0; lid < len; lid++) {
	    int lit = get_literal(cid, lid);
	    int var = IABS(lit);
	    if (skip_literal(lit))
		continue;
	    if (is_data_variable(var))
		continue;
	    if (proj_variables.find(var) != proj_variables.end())
		continue;
	    proj_variables.insert(var);
	    int degree = IMIN((*literal_clauses)[lit].size(), (*literal_clauses)[-lit].size());;
 	    if (degree <= maxdegree)
#if STACK_BVE
		degree_variables[degree].push_back(var);
#else
		degree_variables[degree].insert(var);
#endif
	    report(5, "Projection variable %d.  Degree = %d\n", var, degree);
	}
    }
    // Iteratively eliminate variables where one literal is of low degree
    while (true) {
	int var = 0;
	int lit = 0;  // Literal with lower degree
	int degree = 0;
	// Find variable contained in fewest number of clauses for some phase
	for (int d = 0; var == 0 && d <= maxdegree; d++) {
#if STACK_BVE
	    while (degree_variables[d].size() > 0) {
		int dvar = degree_variables[d].back();
		degree_variables[d].pop_back();
		int dpos = (*literal_clauses)[dvar].size();
		int dneg = (*literal_clauses)[-dvar].size();
		if (eliminated_variables.find(dvar) == eliminated_variables.end() 
		    && (dpos == d ||dneg == d)) {
		    var = dvar;
		    lit = dpos <= dneg ? var : -var;
		    degree = d;
		    break;
		}
	    }

#else	    
	    std::vector<int> dequeue_variables;
	    for (int dvar : degree_variables[d]) {
		dequeue_variables.push_back(dvar);
		int dpos = (*literal_clauses)[dvar].size();
		int dneg = (*literal_clauses)[-dvar].size();
		if (eliminated_variables.find(dvar) == eliminated_variables.end() 
		    && (dpos == d ||dneg == d)) {
		    var = dvar;
		    lit = dpos <= dneg ? var : -var;
		    degree = d;
		    break;
		}
	    }
	    // House cleaning.  Variables found or wrongly classified
	    for (int dvar : dequeue_variables)
		degree_variables[d].erase(dvar);
#endif
	}
	if (var == 0)
	    break;
	int dpos = (*literal_clauses)[var].size();
	int dneg = (*literal_clauses)[-var].size();
	int deprecated_clause_count =  dpos + dneg;
	int max_delta_clause_count = dpos * dneg - deprecated_clause_count;
	if (max_delta_clause_count > maxadded)
	    // Skip.  Might generate too many clauses
	    continue;
	// Perform BVE on var
	int new_clause_count = 0;
	eliminated_variables.insert(var);
	if (preprocess)
	    set_variable_type(var, VAR_ELIM);
	std::unordered_set<int>change_variables;
	std::vector<int>deprecate_clauses;
	for (int cid1 : (*literal_clauses)[lit]) {
	    deprecate_clauses.push_back(cid1);
	    int len1 = clause_length(cid1);
	    for (int lid1 = 0; lid1 < len1; lid1++) {
		int lit1 = get_literal(cid1, lid1);
		if (skip_literal(lit1))
		    continue;
		if (lit1 == lit)
		    continue;
		int var1 = IABS(lit1);
		if (is_data_variable(var1))
		    continue;
		change_variables.insert(var1);
	    }
	}
	for (int cid2 : (*literal_clauses)[-lit]) {
	    deprecate_clauses.push_back(cid2);
	    int len2 = clause_length(cid2);
	    for (int lid2 = 0; lid2 < len2; lid2++) {
		int lit2 = get_literal(cid2, lid2);
		if (skip_literal(lit2))
		    continue;
		if (lit2 == -lit)
		    continue;
		int var2 = IABS(lit2);
		if (is_data_variable(var2))
		    continue;
		change_variables.insert(var2);
	    }
	}
	for (int cid1 : (*literal_clauses)[lit]) {
	    for (int cid2 : (*literal_clauses)[-lit]) {
		int ncid = resolve(var, cid1, cid2);
		if (ncid > 0)
		    new_clause_count++;
	    }
	}
	deactivate_clauses(deprecate_clauses);
	for (int ovar : change_variables) {
	    int odegree = IMIN((*literal_clauses)[ovar].size(), (*literal_clauses)[-ovar].size());
	    if (odegree <= maxdegree) {
#if STACK_BVE
		degree_variables[odegree].push_back(ovar);
#else	      
		degree_variables[odegree].insert(ovar);
#endif
		report(5, "Projection variable %d.  Degree = %d\n", ovar, odegree);
	    }
	}
	if (degree == 0 && bcp_unit_literals.find(-lit) == bcp_unit_literals.end())
	    // Pure literal
	    assign_literal(-lit, true);
	report(3, "BVE on variable %d deprecated %d clauses and added %d new ones\n", var, deprecated_clause_count, new_clause_count);
	if (preprocess) {
	    incr_count_by(COUNT_BVE_ELIM_CLAUSE, deprecated_clause_count);
	    incr_count_by(COUNT_BVE_NEW_CLAUSE, new_clause_count);
	}
    }
    return (int) eliminated_variables.size();
}
#endif // !RANDOM_BVE




///// Support for Tseitin promotion

// Determine next index
static bool increment_indices(std::vector<int> &lengths, std::vector<int> &indices) {
    bool incremented = false;
    for (int i = 0; i < lengths.size(); i++) {
	if (indices[i] < lengths[i]-1) {
	    indices[i]++;
	    incremented = true;
	    break;
	} else
	    indices[i] = 0;
    }
    return incremented;
}

// Generate set of blocked clauses covering this literal & clauses
void Cnf::blocked_clause_expand(int lit, std::vector<int> &clause_list) {
    std::vector<int> clause_lengths;
    std::vector<int> clause_indices;
    for (int cid : clause_list) {
	// Stick all uninteresting literals at the end
	int len = clause_length(cid);
	int lid = 0;
	while (lid < len) {
	    int clit = get_literal(cid, lid);
	    if (clit == lit || skip_literal(clit))
		swap_literals(cid, lid, --len);
	    else
		lid++;
	}
	clause_lengths.push_back(len);
	clause_indices.push_back(0);
    }
    int running = true;
    int first_cid = 0;
    int last_cid = 0;
    while (running) {
	int ncid = new_clause();
	if (first_cid == 0)
	    first_cid = ncid;
	last_cid = ncid;
	add_literal(-lit);
	for (int i = 0; i < clause_list.size(); i++) {
	    int cid = clause_list[i];
	    int idx = clause_indices[i];
	    int clit = get_literal(cid, idx);
	    add_literal(-clit);
	}
	incr_count(COUNT_PROMOTE_CLAUSE);
	running = increment_indices(clause_lengths, clause_indices);
    }
    report(4, "Added blocked clauses #%d .. %d to promote variable %d\n", first_cid, last_cid, IABS(lit));
}


// Detect whether variable is already Tseitin variable.  When promote is true, also attempt convert variable to Tseitin variable
// Fill list of fanout nodes in event of success, so that these can then be tested
bool Cnf::tseitin_variable_test(int var, bool promote, std::unordered_set<int> &fanout_vars) {
    // Construct sets of clauses that contain only data & known Tseitin variables
    // For the variable
    std::set<int> *dt_var_clause_set = new std::set<int>;
    // For each phase
    std::vector<int> dt_lit_clause_list[2];
    // The other data and Tseitin literals that occur in these clauses
    std::unordered_set<int> dt_otherlit_set[2];
    // Clear the fanouts
    fanout_vars.clear();
    for (int phase = 0; phase <= 1; phase ++) {
	int lit = (2*phase - 1) * var;
	for (int cid : (*literal_clauses)[lit]) {
	    if (skip_clause(cid))
		continue;
	    int len = clause_length(cid);
	    bool include = true;
	    std::vector<int> other_lits;
	    for (int lid = 0; lid < len; lid++) {
		int clit = get_literal(cid, lid);
		if (skip_literal(clit))
		    continue;
		int cvar = IABS(clit);
		if (cvar == var)
		    continue;
		if (data_variables->find(cvar) != data_variables->end() 
		    || tseitin_variables->find(cvar) != tseitin_variables->end()) 

		    other_lits.push_back(clit);
		else {
		    include = false;
		    fanout_vars.insert(cvar);
		    continue;
		}
	    }
	    if (include) {
		dt_var_clause_set->insert(cid);
		dt_lit_clause_list[phase].push_back(cid);
		for (int clit : other_lits)
		    dt_otherlit_set[phase].insert(clit);
	    }
	}
    }
    bool sat = true;
    if (dt_var_clause_set->size() >= 1) {
	new_context();
	push_active(dt_var_clause_set);
	uquantify_variable(var);
	sat = is_satisfiable();
	if (verblevel >= 5) {
	    report(5, "Tseitin test gives %s for variable %d on clauses:", sat ? "failure" : "success", var);
	    for (int cid : *dt_var_clause_set)
		printf(" %d", cid);
	    printf("\n");
	}
	pop_context();
    }
    if (!sat)
	return true;
    if (!promote) {
	fanout_vars.clear();
	return false;
    }
    // See if can promote variable to Tseitin variable
    for (int phase = 0; phase <=  1; phase++) {
	int lit = (2*phase - 1) * var;
	// Make sure no other clauses contain this literal
	if (dt_lit_clause_list[phase].size() < (*literal_clauses)[lit].size())
	    continue;
	// Make sure all other literals in these clauses are pure
	bool pure = true;
	for (int olit : dt_otherlit_set[phase]) {
	    if (olit < 0)
		// Only look to one side of negagtion
		continue;
	    if (dt_otherlit_set[phase].find(-olit) != dt_otherlit_set[phase].end()) {
		pure = false;
		break;
	    }
	}
	// Go for it!
	if (pure) {
	    blocked_clause_expand(lit, dt_lit_clause_list[phase]);
	    set_variable_type(var, VAR_TSEITIN_PROMOTE);
	    report(3, "Promoted variable %d.  Fanout size = %d\n", var, (int) fanout_vars.size());
	    return true;
	}
    }
    fanout_vars.clear();
    return false;
}

void Cnf::classify_variables(bool promote) {
    double start = tod();
    tseitin_variables->clear();
    // Set and queue of (potentially Tseitin) projection variables
    // Original list by definition occurrence, but then add fanouts of newly discovered/created variables
    unique_queue<int> pvar_queue;
    // Fanouts of identified or promoted Tseitin variables
    std::unordered_set<int> fanout_vars;
    // Variables that didn't get detected/promoted
    std::unordered_set<int> non_tseitin_vars;

    // Build mappings and ordered list of projection variables
    for (int cid : *active_clauses) {
	if (skip_clause(cid))
	    continue;
	int len = clause_length(cid);
	for (int lid = 0; lid < len; lid++) {
	    int lit = get_literal(cid, lid);
	    if (skip_literal(lit))
		continue;
	    int var = IABS(lit);
	    if (data_variables->find(var) != data_variables->end())
		continue;
	    if (pvar_queue.push(var))
		non_tseitin_vars.insert(var);
	}
    }
    while (!pvar_queue.empty()) {
	int var = pvar_queue.get_and_pop();
	if (tseitin_variable_test(var, promote, fanout_vars)) {
	    if (get_variable_type(var) != VAR_TSEITIN_PROMOTE)
		set_variable_type(var, VAR_TSEITIN_DETECT);
	    tseitin_variables->insert(var);
	    non_tseitin_vars.erase(var);
	}
	for (int fvar : fanout_vars) {
	    if (pvar_queue.push(fvar)) 
		report(3, "Added fanout variable %d for Tseitin variable %d\n", fvar, var);
	}
	incr_count(COUNT_TSEITIN_TEST);
    }
    report(3, "c Failed to detect/promote %d variables\n", (int) non_tseitin_vars.size());
    if (verblevel >= 5) {
	printf("c Non-Tseitin vars:");
	for (int ntvar : non_tseitin_vars)
	    printf(" %d", ntvar);
	printf("\n");
    }
    incr_timer(TIME_CLASSIFY, tod()-start);
    reset_timer(TIME_SAT);
}

Compiler::Compiler(Pog *pg, bool d4v2) {
   pog = pg;
   bkc_limit = 0;
   use_d4v2 = d4v2;
}

Compiler::~Compiler() {
}

// Encode portions of POG.  
// Mark as data variables those arguments that aren't nodes
Cnf *Compiler::clausify(std::vector<int> &root_literals) {
    std::map<int,int> node_remap;
    pog->get_subgraph(root_literals, node_remap);
    if (verblevel >= 5) {
	printf("Running clausify.  Map =");
	for (auto kv : node_remap)
	    printf(" %d-->%d", kv.first, kv.second);
	printf("\n");
    }

    Cnf *cnf = new Cnf(pog->variable_count() + node_remap.size());
    cnf->data_variables = new std::unordered_set<int>;
    for (auto kv : node_remap) {
	int onid = kv.first;
	int nnid = kv.second;
	int degree = pog->get_degree(onid);
	bool is_sum = pog->is_sum(onid);
	cnf->new_clause();
	cnf->add_literal(is_sum ? -nnid : nnid);
	for (int idx = 0; idx < degree; idx++) {
	    int oclit = pog->get_argument(onid, idx);
	    int ocvar = pog->get_var(oclit);
	    bool is_node = pog->is_node(ocvar);
	    int ncvar = is_node ? node_remap[ocvar] : ocvar;
	    int nclit = oclit < 0 ? -ncvar : ncvar;
	    cnf->add_literal(is_sum ? nclit : -nclit);
	    if (!is_node)
		cnf->data_variables->insert(ocvar);
	}
	for (int idx = 0; idx < degree; idx++) {
	    cnf->new_clause();
	    cnf->add_literal(is_sum ? nnid : -nnid);
	    int oclit = pog->get_argument(onid, idx);
	    int ocvar = pog->get_var(oclit);
	    int ncvar = pog->is_node(ocvar) ? node_remap[ocvar] : ocvar;
	    int nclit = oclit < 0 ? -ncvar : ncvar;
	    cnf->add_literal(is_sum ? -nclit : nclit);
	}
    }
    for (int orid : root_literals) {
	cnf->new_clause();
	int orvar = pog->get_var(orid);
	bool is_node = pog->is_node(orvar);
	int nrvar = is_node ? node_remap[orvar] : orvar;
	int nrid = orid < 0 ? -nrvar : nrvar;
	cnf->add_literal(nrid);
	if (!is_node)
	    cnf->data_variables->insert(orvar);
    }
    cnf->finish();
    return cnf;
}

// Compile, integrate into POG and return pointer to root literal
int Compiler::compile(Cnf *cnf, bool trim, bool defer) {
    int root = 0;
    report(3, "Calling compile.  %d clauses (%d non-unit).  trim=%s, defer=%s\n",
    	   cnf->current_clause_count(), cnf->nonunit_clause_count(), b2a(trim), b2a(defer));
    if (defer && !use_d4v2) 
	err(true, "Defer mode not supported with D4 v1");
    if (cnf->nonunit_clause_count() <= bkc_limit)
	root = builtin_kc(cnf, trim, defer, true);
    else {
	const char *cnf_name = fmgr.build_name("cnf", true);
	FILE *cnf_file = fopen(cnf_name, "w");
	if (!cnf_file)
	    err(true, "Couldn't open CNF file '%s'\n", cnf_name);
	cnf->write(cnf_file, use_d4v2 && defer);
	fclose(cnf_file);
	root = compile(cnf_name, cnf->data_variables, trim);
	incr_histo(HISTO_KC_CLAUSES, cnf->current_clause_count());
    }
    return root;
}


int Compiler::compile(const char *cnf_name, std::unordered_set<int> *data_variables, bool trim) {
    report(4, "Compiling CNF file %s.  Trim: %s\n", cnf_name, b2a(trim && data_variables));
    if (verblevel >= 4 && data_variables) {
	printf("   Data variables:");
	for (int var : *data_variables)
	    printf(" %d", var);
	printf("\n");
    }
    static const char *program_path = NULL;
    if (program_path == NULL) {
	const char *pname = use_d4v2 ? "d4v2" : "d4";
	program_path = find_program_path(pname);
	if (program_path == NULL)
	    err(true, "Can't find executable file for program %s\n", pname);
	report(2, "Using path %s for %s\n", program_path, pname);
    }
    const char *var_name = NULL;
    char cmd[200];
    const char *nnf_name = fmgr.build_name("nnf", false);
    double start = tod();
    if (use_d4v2) {
	snprintf(cmd, 200, "%s -i %s -m ddnnf-compiler --dump-ddnnf %s > /dev/null", program_path, cnf_name, nnf_name);
    } else {
	snprintf(cmd, 200, "%s %s -dDNNF -out=%s > /dev/null", program_path, cnf_name, nnf_name);
    }
    report(4, "Running '%s'\n", cmd);
    FILE *pipe = popen(cmd, "w");
    int rc = pclose(pipe);
    double elapsed = tod()-start;
    incr_timer(TIME_KC, elapsed);
    incr_count(COUNT_KC_CALL);
    report(3, "Running D4 on %s required %.3f seconds.  Return code = %d\n", cnf_name, elapsed, rc);
    FILE *nnf_file = fopen(nnf_name, "r");
    if (!nnf_file)
	err(true, "Couldn't open NNF file '%s'\n", nnf_name);
    int osize = pog->node_count();
    int root = pog->load_nnf(nnf_file, trim ? data_variables : NULL);
    int dsize = pog->node_count() - osize;
    fclose(nnf_file);
    report(3, "Imported NNF file '%s'.  Root literal = %d.  Added %d nodes\n", nnf_name, root, dsize);
    if (verblevel >= 5)
	pog->show(root, stdout);
    incr_histo(HISTO_POG_NODES, dsize);
    fmgr.flush();
    return root;
}

// Internal knowledge compiler.
// Performs reductions in anticipation of projection
// trim indicates that all projection variables should be eliminated
int Compiler::builtin_kc(Cnf *cnf, bool trim, bool defer, bool top_level) {
    int ccount = cnf->current_clause_count();
    double start = 0.0;
    int osize = pog->node_count();
    if (top_level) {
	report(3, "Invoking builtin KC.  %d clauses (%d non-unit)\n", ccount,
	       cnf->nonunit_clause_count());
	incr_count(COUNT_BUILTIN_KC);
	incr_histo(HISTO_BUILTIN_KC_CLAUSES, ccount);
	if (verblevel >= 4) {
	    char fname[30];
	    snprintf(fname, 30, "tmp-%d.cnf", ccount);
	    if (verblevel >= 4) {
		report(4, "CNF for top-level BKC:\n");
		cnf->show(stdout);
	    }
	    FILE *cfile = fopen(fname, "w");
	    cnf->write(cfile, true);
	    report(3, "Wrote to file %s\n", fname);
	    fclose(cfile);
	}
	start = tod();
    }
    std::vector<int> clause_chunks;
    if (cnf->check_simple_pkc(clause_chunks)) {
	report(3, "Builtin KC on %d clauses.  Reduced to pure clauses\n", ccount);
	if (verblevel >= 3) {
	    report(3, "Chunk:");
	    for (int lit : clause_chunks)
		printf(" %d", lit);
	    printf("\n");
	}
	return  pog->simple_kc(clause_chunks);
    }
    int svar = cnf->find_split(defer);
    int child[2];
    bool is_data = cnf->is_data_variable(svar);
    report(5, "Builtin KC on %d clauses.  Splitting on variable %d\n", ccount, svar);
    for (int phase = -1 ; phase <= 1; phase += 2) {
	int slit = svar * phase;
	cnf->new_context();
	cnf->assign_literal(slit, false);
	int bcount = cnf->bcp(false);
	int pcount = cnf->bve(false, 0);
	report(5, "Builtin KC on %d clauses (splitting literal %d).  BCP found %d units.  BVE found %d pure\n", ccount, slit, bcount, pcount);
	if (verblevel >= 5) {
	    report(5, "CNF post BCP/BVE:\n");
	    cnf->show(stdout);
	}
	int cedge = builtin_kc(cnf, trim, defer, false);
	if (is_data || !trim) {
	    pog->start_node(POG_PRODUCT);
	    pog->add_argument(slit);
	    pog->add_argument(cedge);
	    cedge = pog->finish_node();
	}
	child[(phase+1)/2] = cedge;
	cnf->pop_context();
    }
    pog->start_node(POG_SUM);
    for (int i = 0; i < 2; i++) {
	pog->add_argument(child[i]);
    }
    int root = pog->finish_node();
    report(5, "Builtin KC on %d clauses.  Returning edge %d\n", ccount, root);
    if (top_level) {
	double elapsed = tod() - start;
	incr_timer(TIME_BUILTIN_KC, elapsed);
	int dsize= pog->node_count() - osize;
	incr_histo(HISTO_POG_NODES, dsize);
    }
    return root;
}

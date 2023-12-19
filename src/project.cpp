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
#include "project.hh"
#include "report.h"
#include "counters.h"
#include "files.hh"

Project::Project(const char *cnf_name, pkc_mode_t md, bool use_d4v2, int preprocess_level, bool tseitin_detect, bool tseitin_promote, int opt, int bkc_limit) {
    mode = md;
    optlevel = opt;
    trace_variable = 0;
    Cnf cnf;
    FILE *infile = fopen(cnf_name, "r");
    if (!infile)
	err(true, "Couldn't open CNF file '%s'\n", cnf_name);
    if (!cnf.import_file(infile, mode != PKC_COMPILE)) {
	fclose(infile);
	err(true, "Couldn't read input file '%s'\n", cnf_name);
    }
    fclose(infile);
    fmgr.set_root(cnf_name);
    report(1, "CNF file loaded %d declared variables, %d clauses, %d data variables\n",
	   cnf.variable_count(), cnf.current_clause_count(), cnf.data_variables->size());
    int ucount = 0;
    int ecount = 0;
    double pstart = tod();
    if (preprocess_level >= 1) {
	ucount = cnf.bcp(true);
    }
    if (preprocess_level >= 2) {
	int maxdegree = preprocess_level >= 3 ? 1 : 0;
	ecount = cnf.bve(true, maxdegree);
    }
    report(1, "Initial BCP/BVE found %d unit literals and eliminated %d variables\n", ucount, ecount);
    if (tseitin_detect || tseitin_promote) {
	cnf.classify_variables(tseitin_promote);
	int tcount = cnf.tseitin_variables->size();
	report(1, "Variable analysis found and/or created %d Tseitin variables\n", tcount);
    }
    if (preprocess_level >= 4) {
	int maxdegree = preprocess_level - 2;
	ecount = cnf.bve(true, maxdegree);
	report(1, "Second BVE (maxdegree %d) eliminated %d variables\n", maxdegree, ecount);
    }
    incr_count_by(COUNT_UNUSED_VAR, cnf.get_variable_type_count(VAR_UNUSED));
    incr_count_by(COUNT_DATA_VAR, cnf.get_variable_type_count(VAR_DATA));
    incr_count_by(COUNT_NONTSEITIN_VAR, cnf.get_variable_type_count(VAR_NONTSEITIN));
    incr_count_by(COUNT_TSEITIN_DETECT_VAR, cnf.get_variable_type_count(VAR_TSEITIN_DETECT));
    incr_count_by(COUNT_TSEITIN_PROMOTE_VAR, cnf.get_variable_type_count(VAR_TSEITIN_PROMOTE));
    incr_count_by(COUNT_ELIM_VAR, cnf.get_variable_type_count(VAR_ELIM));
    report(1, "After preprocessing, have %d+%d Tseitin and %d non-Tseitin projection variables\n", 
	   get_count(COUNT_TSEITIN_DETECT_VAR),
	   get_count(COUNT_TSEITIN_PROMOTE_VAR),
	   get_count(COUNT_NONTSEITIN_VAR));
    incr_timer(TIME_PREPROCESS, tod()-pstart);
    reset_timer(TIME_BCP);
    if (mode == PKC_PREPROCESS)
	return;
    pog = new Pog(cnf.variable_count(), cnf.data_variables, cnf.tseitin_variables);
    input_weights = cnf.input_weights;
    compiler = new Compiler(pog, use_d4v2);
    // For incremental mode, don't eliminate variables when uncertain
    // For other modes, eliminate all projection variables
    bool trim = mode == PKC_MONOLITHIC || mode == PKC_TSEITIN || mode == PKC_DEFERRED;
    bool defer = mode == PKC_DEFERRED;
    // Use default bkc limit.  Will cause call to external KC at top level
    root_literal = compiler->compile(&cnf, trim, defer);
    // Now allow BKC during traversal
    compiler->set_bkc_limit(bkc_limit);
    report(1, "Initial POG created.  %d nodes, %d edges,  %d clauses. Root literal = %d\n", 
	   pog->node_count(), pog->edge_count(), pog->node_count() + pog->edge_count(),
	   root_literal);
    incr_count_by(COUNT_POG_INITIAL_SUM, get_count(COUNT_POG_SUM));
    incr_count_by(COUNT_POG_INITIAL_PRODUCT, get_count(COUNT_POG_PRODUCT));
    incr_count_by(COUNT_POG_INITIAL_EDGES, get_count(COUNT_POG_EDGES));
    incr_timer(TIME_INITIAL_KC, get_timer(TIME_KC) + get_timer(TIME_BUILTIN_KC));
    reset_timer(TIME_KC);
    reset_timer(TIME_BUILTIN_KC);
    fmgr.flush();
}

Project::~Project() {
    delete pog;
    delete compiler;
}

void Project::projecting_compile(int preprocess_level) {
    // Already done for trim, defer, and compile
    if (mode == PKC_MONOLITHIC) {
	if (!pog->is_node(root_literal)) {
	    if (root_literal == TAUTOLOGY)
		report(2, "First compilation yielded tautology\n");
	    else if (root_literal == CONFLICT)
		report(2, "First compilation yielded conflict\n");
	    else 
		report(2, "First compilation yielded literal %d\n", root_literal);
	    return;
	}

	// Tautology check
	std::vector<int> root_literals;
	root_literals.push_back(root_literal);
 	if (sums_to_tautology(root_literals)) {
	    root_literal = TAUTOLOGY;
	    report(2, "SAT test detected tautology at root\n");
	    return;
	}
	Cnf *mcnf = compiler->clausify(root_literals);
	int ucount = 0;
	int ecount = 0;
	if (preprocess_level >= 1) {
	    ucount = mcnf->bcp(false);
	    if (preprocess_level >= 2) {
		int maxdegree = preprocess_level - 2;
		ecount = mcnf->bve(false, maxdegree);
	    }
	}
	report(2, "Recompile.  %d unit literals, %d eliminated variables.  %d variables remain.  %d non-unit clauses\n",
	       ucount, ecount, mcnf->variable_count()-(ucount+ecount), mcnf->nonunit_clause_count());
	root_literal = compiler->compile(mcnf, true, false);
	mcnf->deallocate();
	delete mcnf;
    } else if (mode == PKC_INCREMENTAL)
	root_literal = traverse(root_literal);
}

bool Project::write(const char *pog_name) {
    FILE *pog_file = NULL;
    if (pog_name) {
	pog_file = fopen(pog_name, "w");
	if (!pog_file) {
	    pog_file = fopen(pog_name, "w");
	    if (!pog_file)
		err(true, "Couldn't open file '%s'\n", pog_name);
	}
    }
    // When no name given, will still call write to get final stats
    bool ok = pog->write(root_literal, pog_file);
    if (pog_file)
	fclose(pog_file);
    return ok;
}
 
q25_ptr Project::subgraph_count(bool weighted, int root_edge) {
    // Pointers that should be deleted
    std::vector<q25_ptr> qlog, eqlog;
    if (weighted && (!input_weights || input_weights->size() == 0))
	return NULL;
    double start = tod();
    q25_ptr rescale = q25_from_32(1);
    std::unordered_map<int,q25_ptr> weights;
    for (int var : *(pog->data_variables)) {
	q25_ptr pwt = NULL;
	q25_ptr nwt = NULL;
	q25_ptr sum = NULL;
	if (weighted) {
	    auto fid = input_weights->find(var);
	    if (fid != input_weights->end()) 
		pwt = fid->second;
	    else
		err(false, "Couldn't find weight for input %d\n", var);
	    fid = input_weights->find(-var);
	    if (fid != input_weights->end())
		nwt = fid->second;
	    if (!pwt && !nwt) {
		pwt = qmark(q25_from_32(1), qlog);
		nwt = qmark(q25_from_32(1), qlog);
		sum = qmark(q25_from_32(2), qlog);
	    } else if (!pwt) {
		pwt = q25_one_minus(nwt);
		sum = qmark(q25_from_32(1), qlog);
	    } else if (!nwt) {
		nwt = q25_one_minus(pwt);
		sum = qmark(q25_from_32(1), qlog);
	    } else
		sum = qmark(q25_add(pwt, nwt), qlog);
	} else {
	    // These won't be the final weights
	    nwt = qmark(q25_from_32(1), qlog);
	    pwt = qmark(q25_from_32(1), qlog);
	    sum = qmark(q25_from_32(2), qlog);
	}
	if (q25_is_one(sum)) {
	    weights[ var] = qmark(pwt, eqlog);
	    weights[-var] = qmark(nwt, eqlog);
	} else {
	    q25_ptr recip = qmark(q25_recip(sum), qlog);
	    if (!q25_is_valid(recip)) {
		err(false, "Could not get reciprocal of summed weights for variable %d.  Sum = ", var);
		q25_write(sum, stdout);
		printf("\n");
		err(true, "Cannot recover\n");
	    }
	    rescale = q25_mul(qmark(rescale, qlog), sum);
	    weights[ var] = qmark(q25_mul(pwt, recip), eqlog);
	    weights[-var] = qmark(q25_mul(nwt, recip), eqlog);
	}
	qflush(qlog);
    }
    q25_ptr rval = qmark(pog->ring_evaluate(root_edge, weights), eqlog);
    q25_ptr cval = q25_mul(qmark(rescale, eqlog), rval);
    qflush(eqlog);
    double elapsed = tod()-start;
    incr_timer(TIME_RING_EVAL, elapsed);
    return cval;
}

q25_ptr Project::count(bool weighted) {
    return subgraph_count(weighted, root_literal);
}

bool Project::equal_counts(int root_edge1, int root_edge2) {
    q25_ptr count1 = subgraph_count(false, root_edge1);
    q25_ptr count2 = subgraph_count(false, root_edge2);	
    bool result = q25_compare(count1, count2) == 0;
    q25_free(count1); q25_free(count2);
    return result;
}

bool Project::sums_to_tautology(std::vector<int> &root_literals) {
    std::vector<int> nroot_literals;
    for (int root : root_literals)
	nroot_literals.push_back(-root);
    Cnf *tcnf = compiler->clausify(nroot_literals);
    bool result = !tcnf->is_satisfiable();
    tcnf->deallocate();
    delete tcnf;
    return result;
}

int Project::traverse(int edge) {
    report(5, "Traversing edge %d\n", edge);

    if (!pog->is_node(edge)) {
	int var = pog->get_var(edge);
	if (var == TAUTOLOGY)
	    return edge;
	if (pog->is_data_variable(var))
	    return edge;
	// Projected literal satisfied
	return TAUTOLOGY;
    }

    if (optlevel >= 1) {
	auto fid = result_cache.find(edge);
	if (fid != result_cache.end()) {
	    int nedge = fid->second;
	    incr_count(COUNT_PKC_REUSE);
	    return nedge;
	}
    }
    if (optlevel >= 2) {
	if (pog->only_data_variables(edge)) {
	    incr_count(COUNT_PKC_DATA_ONLY);
	    return edge;
	}
	if (pog->only_projection_variables(edge)) {
	    incr_count(COUNT_PKC_PROJECT_ONLY);
	    return TAUTOLOGY;
	}
    }

    int nedge = pog->is_sum(edge) ? traverse_sum(edge) : traverse_product(edge);
    result_cache[edge] = nedge;
    return nedge;
}

int Project::traverse_sum(int edge) {
    int edge1 = pog->get_argument(edge, 0);
    int edge2 = pog->get_argument(edge, 1);
    int dvar = pog->get_decision_variable(edge);
    int nedge = 0;
    int rlevel = dvar == trace_variable ? 2 : 5;
    const char *descr = "";
    report(rlevel, "Traversing Sum node %d.  Splitting on variable %d with children %d and %d\n",
	   edge, dvar, edge1, edge2);
    int nedge1 = traverse(edge1);
    if (nedge1 == TAUTOLOGY) {
	incr_count(COUNT_VISIT_SUBSUMED_SUM);
	report(rlevel, "Traversal of Sum node %d yielded tautology.  First argument became tautology\n", edge);
	return nedge1;
    }
    int nedge2 = traverse(edge2);
    if (nedge2 == TAUTOLOGY) {
	incr_count(COUNT_VISIT_SUBSUMED_SUM);
	report(rlevel, "Traversal Sum node %d yielded tautology.  Second argument became tautology\n", edge);
	return nedge2;
    }
    if (nedge1 == nedge2) {
	incr_count(COUNT_VISIT_SUBSUMED_SUM);
	report(rlevel, "Traversal Sum node %d yielded %d.  Identical arguments\n", edge, nedge1);
	return nedge1;
    }
    std::vector<int> roots;
    roots.push_back(nedge1);
    roots.push_back(nedge2);
    if (sums_to_tautology(roots)) {
	incr_count(COUNT_VISIT_TAUTOLOGY_SUM);
	report(rlevel, "Traversal Sum node %d yielded edges %d and %d summing to tautology\n", edge, nedge1, nedge2);
	return TAUTOLOGY;
    }
    if (pog->is_data_variable(dvar)) {
	descr = "data";
	incr_count(COUNT_VISIT_DATA_SUM);
	report(rlevel, "Traversing Sum node %d gives child edges %d and %d. Split on data variable %d\n", edge, nedge1, nedge2, dvar);
    } else if (pog->is_tseitin_variable(dvar)) {
	descr = "tseitin";
	incr_count(COUNT_VISIT_MUTEX_SUM);
	report(rlevel, "Traversing Sum node %d gives child edges %d and %d. Split on Tseitin variable %d\n", edge, nedge1, nedge2, dvar);
    } else {
	report(rlevel, "Traversing Sum node %d gives child edges %d and %d. Split on projection variable %d\n", edge, nedge1, nedge2, dvar);
	Cnf *xcnf = compiler->clausify(roots);
	report(rlevel, "Mutex test.  Traversing edge %d.  Calling SAT solver\n", edge);
	if (!xcnf->is_satisfiable()) {
	    descr = "mutex";
	    incr_count(COUNT_VISIT_MUTEX_SUM);
	} else {
	    report(rlevel, "Traversing edge %d.  Calling compiler\n", edge);
	    int uroot = compiler->compile(xcnf, optlevel >= 2, false);
	    if (uroot == CONFLICT) {
		report(rlevel, "Traversing edge %d.  KC gives conflict\n", edge);
		descr = "mutex";
		incr_count(COUNT_VISIT_MUTEX_SUM);
	    } else {
		report(rlevel, "Traversing edge %d.  KC gives edge %d\n", edge, uroot);
		int xroot = traverse(uroot);
		// Subsumption checks
		if (xroot == nedge1) {
		    report(rlevel, "Traversal of Sum node %d.  Intersection %d identical to first argument.  Return %d by subsumption\n",
			   edge, xroot, nedge2);
		    incr_count(COUNT_VISIT_SUBSUMED_SUM);
		    return nedge2;
		} else if (xroot == nedge2) {
		    report(rlevel, "Traversal of Sum node %d.  Intersection %d identical to second argument.  Return %d by subsumption\n",
			   edge, xroot, nedge1);
		    incr_count(COUNT_VISIT_SUBSUMED_SUM);
		    return nedge1;
		} else if (optlevel >= 4 && equal_counts(xroot, nedge1)) {
		    report(rlevel, "Traversal of Sum node %d.  Intersection %d has same number of models as first argument.  Return %d by subsumption\n",
			   edge, xroot, nedge2);
		    incr_count(COUNT_VISIT_COUNTED_SUM);
		    return nedge2;
		} else if (optlevel >= 4 && equal_counts(xroot, nedge2)) {
		    report(rlevel, "Traversal of Sum node %d.  Intersection %d has same number of models as second argument.  Return %d by subsumption\n",
			   edge, xroot, nedge1);
		    incr_count(COUNT_VISIT_COUNTED_SUM);
		    return nedge1;
		}
		pog->start_node(POG_SUM);
		pog->add_argument(-nedge1);
		pog->add_argument(xroot);
		int mroot = pog->finish_node();
		pog->start_node(POG_SUM);
		pog->add_argument(-mroot);
		pog->add_argument(nedge2);
		nedge = pog->finish_node();
		descr = "excluding";
		incr_count(COUNT_VISIT_EXCLUDING_SUM);
	    }
	}
	xcnf->deallocate();
	delete xcnf;
    }
    if (nedge == 0) {
	pog->start_node(POG_SUM);
	pog->add_argument(nedge1);
	pog->add_argument(nedge2);
	nedge = pog->finish_node();
    }
    report(rlevel, "Traversal of Sum node %d yielded edge %d.  Sum type = %s\n", edge, nedge, descr);
    return nedge;
}

int Project::traverse_product(int edge) {
    int degree = pog->get_degree(edge);
    int ncedge[degree];
    for (int idx = 0; idx < degree; idx++) {
	int cedge = pog->get_argument(edge, idx);
	ncedge[idx] = traverse(cedge);
    }
    pog->start_node(POG_PRODUCT);
    for (int idx = 0; idx < degree; idx++)
	pog->add_argument(ncedge[idx]);
    int nedge = pog->finish_node();
    report(5, "Traversal of Product node %d yielded edge %d\n", edge, nedge);
    incr_count(COUNT_VISIT_PRODUCT);
    return nedge;
}

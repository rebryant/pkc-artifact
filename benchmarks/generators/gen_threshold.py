#!/usr/bin/python3

#####################################################################################
# Copyright (c) 2021 Marijn Heule, Randal E. Bryant, Carnegie Mellon University
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
# associated documentation files (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge, publish, distribute,
# sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all copies or
# substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
# NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
# OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
########################################################################################

import sys
import  getopt
import math

import readwrite


# Generate CNF or POG file for k/n threshold constraints
def usage(name):
    print("Usage: %s [-h] [-v VLEVEL] [-p] [-t] -r ROOT -n N [-k K]" % name) 
    print("  -h       Print this message")
    print("  -v LEVEL Set verbosity level")
    print("  -p       Generate POG representation")
    print("  -P       Generate PBIP declaration file, in addition to CNF")
    print("  -t       Use Tseitin encoding of clauses")
    print("  -r ROOT  Specify root name for files.  Will generate ROOT.cnf or ROOT.pog")
    print("  -n N     Specify number of elements")
    print("  -k K     Specify lower bound (default = majority")

Tautology = 1000 * 1000 * 1000
Conflict = -Tautology

class NodeException(Exception):
    msg = ""

    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        return "Node Exception (%s)" % self.msg

def cleanClause(lits):
    nlits = []
    for lit in lits:
        if lit == Tautology:
            return Tautology
        elif lit != Conflict:
            nlits.append(lit)
    return nlits

class Ntype:
    andn, orn, iten, iton = range(4)

tnames = ("AND", "OR", "ITE", "ITO")

class Gtype:
    plaisted, tseitin, pog = range(3)

gnames = ("Plaisted", "Tseitin", "POG")

class Node:
    id = None
    ntype = None
    children = []
    mark = False
    
    def __init__(self, ntype, id, children):
        self.ntype = ntype
        self.id = id
        self.children = children
        self.mark = False

    def __str__(self):
        symbol = tnames[self.ntype] if self.ntype >= 0 and self.ntype < 4 else "NONE"
        clist = [str(child) for child in self.children]
        return symbol + str(self.id) + "(" + ", ".join(clist) + ")"

    def remap(self, map):
        nid = map[self.id] if self.id in map else self.id
        nchildren = [map[child] if child in map else child for child in self.children]
        return Node(self.ntype, nid, nchildren)

    # Measure size as 1+degree
    def size(self):
        return len(self.children) + 1

    def clausify(self, gtype):
        clist = []
        if self.ntype == Ntype.andn:
            for child in self.children:
                clist.append([child, -self.id])
            if gtype != Gtype.plaisted:
                clist.append([-child for child in self.children] +  [self.id])
        elif self.ntype == Ntype.orn:
            clist.append(self.children + [-self.id])
            if gtype != Gtype.plaisted:
                for child in self.children:
                    clist.append([-child, self.id])
        elif self.ntype == Ntype.iten:
            i, t, e = self.children
            clist.append([-i, t, -self.id])
            clist.append([i,  e, -self.id])
            if gtype != Gtype.plaisted:
                clist.append([-i, -t, self.id])
                clist.append([i,  -e, self.id])
        elif self.ntype == Ntype.iton:
            i, t, o = self.children
            clist.append([   t, -self.id])
            clist.append([i, o, -self.id])
            if gtype ==  Gtype.tseitin:
                clist.append([-i, -t, self.id])
                clist.append([-o, self.id])
            if gtype == Gtype.pog:
                raise NodeException("Can't do POG encoding of ITO")
        nclist = []
        for clause in clist:
            nc = cleanClause(clause)
            if nc != Tautology:
                nclist.append(nc)
        return nclist

    def pogWrite(self, pwriter):
        if self.ntype == Ntype.andn:
            pwriter.doAnd(self.children)
        elif self.ntype == Ntype.orn:
            pwriter.doOr(self.children)
        else:
            raise NodeException("Can't generate POG containing %s nodes" % ("ITE" if self.ntype == Ntype.iten else "ITO"))

class NodeManager:
    nvar = 0
    nodeList = []
    gtype = Gtype.plaisted

    def __init__(self, nvar, nodeList = [], gtype = Gtype.plaisted):
        self.nvar = nvar
        self.nodeList = nodeList
        self.gtype = gtype

    def addNode(self, ntype, children):
        nextVar = self.nvar + len(self.nodeList) + 1
        node = Node(ntype, nextVar, children)
        self.nodeList.append(node)
        return nextVar

    def doAnd(self, children):
        clist = [child for child in children if child != Tautology]
        if Conflict in clist:
            return Conflict
        if len(clist) == 1:
            return clist[0]
        return self.addNode(Ntype.andn, clist)

    def doOr(self, children):
        clist = [child for child in children if child != Conflict]
        if Tautology in clist:
            return Tautology
        if len(clist) == 1:
            return clist[0]
        return self.addNode(Ntype.orn, clist)

    def doIte(self, children):
        if len(children) != 3:
            raise NodeException("ITE with %d arguments not allowed" % len(children))
        i, t, e = children
        if t == e:
            return t
        elif t == Tautology and e == Conflict:
            return i
        elif t == Conflict and e == Tautology:
            return -i
        elif self.gtype != Gtype.pog and t == Tautology:
            return self.doOr([i, e])
        elif t == Conflict:
            return self.doAnd([-i, e])
        elif self.gtype != Gtype.pog and e == Tautology:
            return self.doOr([-i, t])
        elif e == Conflict:
            return self.doAnd([i, t])
        else:
            return self.addNode(Ntype.iten, children)

    def doIto(self, children):
        if len(children) != 3:
            raise NodeException("ITO with %d arguments not allowed" % len(children))
        i, t, o = children
        if o == Tautology:
            return Tautology
        elif t == o:
            return t
        elif t == Tautology and o == Conflict:
            return i
        elif t == Tautology:
            return self.doOr([i, o])
        elif t == Conflict:
            return o
        elif o == Conflict:
            return self.doAnd([i, t])
        else:
            return self.addNode(Ntype.iton, children)

    def isNode(self, edge):
        return abs(edge) > self.nvar and abs(edge) != Tautology

    def getNode(self, edge):
        id = abs(edge) - self.nvar - 1
        return self.nodeList[id]

    def showEdge(self, edge):
        var = abs(edge)
        sign = "-" if edge < 0 else ""
        sval = str(self.getNode(edge)) if self.isNode(edge) else str(var)
        return sign + sval

    def size(self):
        sizes = [node.size() for node in self.nodeList]
        return sum(sizes)

    def show(self):
        for node in self.nodeList:
            print(str(node))

    def getRoot(self):
        return self.nvar + len(self.nodeList)

    def remapEdge(self, edge, map):
        var = abs(edge)
        if var in map:
            var = map[var]
        return var if edge > 0 else -var

    def doMark(self, edge):
        if self.isNode(edge):
            node = self.getNode(edge)
            if not node.mark:
                for child in node.children:
                    self.doMark(child)
                node.mark = True

    def subgraph(self):
        root = self.getRoot()
        for node in self.nodeList:
            node.mark = False
        self.doMark(root)
        return [node for node in self.nodeList if node.mark]
        
    def prune(self):
        newList = self.subgraph()
        map = { newList[i].id : self.nvar + i + 1 for i in range(len(newList)) }
        newNodes = [ node.remap(map) for node in newList]
        return NodeManager(self.nvar, newNodes, self.gtype)

    # Expand ITEs into ANDs and ORs
    # Convert ORs of input literals into negated product
    def expand(self, iteOnly = True):
        nmgr = NodeManager(self.nvar, [], self.gtype)
        map = {}
        for node in self.nodeList:
            if node.ntype in [Ntype.andn, Ntype.orn]:
                nid = nmgr.nvar + len(nmgr.nodeList) + 1
                map[node.id] = nid
                nmgr.nodeList.append(node.remap(map))
            elif node.ntype == Ntype.iten:
                i, t, e = [self.remapEdge(child, map) for child in node.children]
                it = nmgr.doAnd([i, t])
                ie = nmgr.doAnd([-i, e])
                nid = nmgr.doOr([it, ie])
                map[node.id] = nid
            elif node.ntype == Ntype.iton:
                if iteOnly:
                    raise NodeException("Expansion of ITO node %d disabled" % (str(node)))
                i, t, o = [self.remapEdge(child, map) for child in node.children]
                it = nmgr.doAnd([i, t])
                nid = nmgr.doOr([it, o])
                map[node.id] = nid
        return nmgr

    def genCnf(self, froot, verbLevel, descr = None):
        cwriter = readwrite.LazyCnfWriter(froot + ".cnf", verbLevel = verbLevel)
        cwriter.doComment("t pmc")
        slist = [str(i) for i in range(1, self.nvar+1)]
        cwriter.doComment("p show %s 0" % " ".join(slist))
        cwriter.newVariables(self.nvar + len(self.nodeList))
        if descr is not None:
            cwriter.doComment(descr)
        if verbLevel >= 2:
            cwriter.doComment("%s encoding of graph with %d input variables and %d nodes" % (gnames[self.gtype], self.nvar, len(self.nodeList)))
        root = self.nvar + len(self.nodeList)
        if verbLevel >= 2:
            cwriter.doComment("Unit clause for root")
        cwriter.doClause([root])
        for node in self.nodeList:
            if (verbLevel >= 2):
                cwriter.doComment("Encode node %s" % str(node))
            clist = node.clausify(self.gtype)
            for clause in clist:
                cwriter.doClause(clause)
        cwriter.finish()
        return (cwriter.variableCount, cwriter.clauseCount)

    def genPog(self, froot, verbLevel, descr = None):
        if self.gtype != Gtype.pog:
            raise NodeException("Must build network in POG mode to generate POG")
        pwriter = readwrite.PogWriter(self.nvar, froot + ".pog", verbLevel = verbLevel)
        if descr is not None:
            pwriter.doComment(descr)
        if verbLevel >= 2:
            pwriter.doComment("POG representation of graph with %d variables and %d nodes.  Size = %d" % (self.nvar, len(self.nodeList), self.size()))
        for node in self.nodeList:
            node.pogWrite(pwriter)
        rid = self.nodeList[-1].id if len(self.nodeList) > 0 else 0
        pwriter.doRoot(rid)
        pwriter.finish()

       

class Threshold:
    N = 0
    K = 0
    nmgr = None
    expanded = False
    gtype = Gtype.plaisted

    def __init__(self, N, K=None, gtype = Gtype.plaisted):
        self.N = N
        if K is None:
            self.K = N//2 + 1
        else:
            K = self.K
        self.nmgr = NodeManager(N, [], gtype = gtype)
        self.expanded = False
        self.gtype = gtype

    def show(self):
        self.nmgr.show()

    def solutions(self):
        count = 0
        for m in range(self.K, self.N+1):
            try:
                count += math.comb(self.N, m)
            except:
                pass
        return count

    def build(self, verbLevel):
        edgeDict = {}
        k = self.K
        n = self.N
        # Construct network of ITE values
        for i in range(1,k+1):
            edgeDict[(i,0)] = Conflict
        for j in range(0,n+1):
            edgeDict[(0,j)] = Tautology
        for i in range(1,k+1):
            for j in range(1,n+1):
                iedge = j
                tedge = edgeDict[(i-1,j-1)]
                eedge = edgeDict[(i  ,j-1)]
                nedge = self.nmgr.doIto([iedge, tedge, eedge]) if self.gtype != Gtype.pog else self.nmgr.doIte([iedge, tedge, eedge])
                edgeDict[(i,j)] = nedge
                if verbLevel >= 4:
                    print("Edge(%d,%d):  %s" % (i, j, self.nmgr.showEdge(nedge)))

    def prune(self):
        self.nmgr = self.nmgr.prune()

    def expand(self):
        iteOnly = self.gtype != Gtype.plaisted
        self.nmgr = self.nmgr.expand(iteOnly)
        self.expanded = True

    def genCnf(self, froot, verbLevel):
        descr = "Thresh(%d, %d)" % (self.N, self.K)
        try:
            sols = self.solutions()
            descr += ".  %d solutions" % (self.solutions())
        except:
            pass
        return self.nmgr.genCnf(froot, verbLevel, descr)

    def genPog(self, froot, verbLevel):
        descr = "Thresh(%d, %d).  %d solutions" % (self.N, self.K, self.solutions())
        if not self.expand:
            self.expand()
        self.nmgr.genPog(froot, verbLevel, descr)

    def genPbip(self, froot, clauseCount):
        fname = froot + ".pbip"
        try:
            outfile = open(fname, "w")
        except:
            print("ERROR: Could not open file '%s'" % fname)
            return
        outfile.write("* PBIP declaration of %d/%d threshold formula\n" % (self.K, self.N))
        outfile.write("i")
        for i in range(1, self.N+1):
            outfile.write(" 1 x%d" % i)
        clist = [str(i) for i in range(1, clauseCount+1)]
        outfile.write(" >= %d ; %s\n" % (self.K, " ".join(clist)))
        outfile.close()
        print("c File %s written" % fname)


def run(name, args):
    verbLevel = 1
    doPog = False
    doPbip = False
    gtype = Gtype.plaisted
    froot = None
    N = None
    K = None
    optlist, args = getopt.getopt(args, "hv:pPtr:n:k:")
    for (opt, val) in optlist:
        if opt == '-h':
            usage(name)
            return
        elif opt == '-v':
            verbLevel = int(val)
        elif opt == '-p':
            doPog = True
            gtype = Gtype.pog
        elif opt == '-P':
            doPbip = True
        elif opt == '-t':
            gtype = Gtype.tseitin
        elif opt == '-r':
            froot = val
        elif opt == '-n':
            N = int(val)
        elif opt == '-k':
            K = int(val)
        else:
            print("Unknown option '%s'" % opt)
            usage(name)
            return
    if N is None:
        print("Must have value for n")
        usage(name)
        return
    if froot is None:
        print("Must have root name")
        usage(name)
        return
    t = Threshold(N, K, gtype)
    if verbLevel >= 1:
        print("Building threshold network for N=%d K=%d (%d solutions)." % (t.N, t.K, t.solutions()))
    t.build(verbLevel)
    if verbLevel >= 3:
        print("After building:")
        t.show()
    t.prune()
    if verbLevel >= 3:
        print("After pruning:")
        t.show()
    if (doPog):
        t.expand()
        if verbLevel >= 3:
            print("After expanding:")
            t.show()
        t.genPog(froot, verbLevel)
        if verbLevel >= 1:
            print("Generated POG.  Size = %d" % t.nmgr.size())
    else:
        (variableCount, clauseCount) = t.genCnf(froot, verbLevel)
        if doPbip:
            t.genPbip(froot, clauseCount)

if __name__ == "__main__":
    run(sys.argv[0], sys.argv[1:])

        

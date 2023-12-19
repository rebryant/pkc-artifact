#!/usr/bin/python3

#####################################################################################
# Copyright (c) 2023 Randal E. Bryant, Carnegie Mellon University
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


## Merge set of CSV file into single file with headings
## Each source file must have lines of form key,value
## The files must all have the keys in the same order
## The keys for the first entry must be a superset of the rest.

## Optionally filter out keys for which one more fields is empty

import sys
import getopt
import csv

def eprint(s):
    sys.stderr.write(s + '\n')


def usage(name):
    eprint("Usage: %s [-h] [-f] [-d VAL] [-s I1:I2:...:Ik] [-r I1:I2] [-t I:T1:T2] [-o I:T] [-l L0,L1,L2,...] FILE1.csv FILE2.csv ... FILEn.csv" % name)
    eprint("  -f            Filter out lines that have at least one field missing")
    eprint("  -d VAL        Use default of VAL when field missing")
    eprint("  -s I1:I2:...:Ik Sum the values from specified columns 1..k and add as new column")
    eprint("  -r I1:I2      Form the ratio between the values from specified columns 1 and 2 and add as new column")
    eprint("  -t I:T1:T2    Compare value from column with value T1.  If greater, set to T2.  If I negative, invert sense of threshold.  Add as new column")
    eprint("  -o I:T        Compare value from column with value T1.  If greater, omit entire line.  If I negative, invert sense of test")
    eprint("  -h            Print this message")
    eprint("  -l LABELS     Provide comma-separated set of heading labels")
    eprint("  FILE1.csv ... Source files")
    eprint("  (If give numeric value, then it gets used for all entries")

# Growing set of result lines, indexed by key
globalEntries = {}
# Number of columns in entries
globalCount = 0

# Process a single file, building entries
# Return dictionary of entries + count of number of entries per line
def processFile(fname):
    entries = {}
    columnCount = 0
    try:
        infile = open(fname)
        creader = csv.reader(infile)
    except:
        eprint("Couldn't open CSV file '%s'" % fname)
        sys.exit(1)
    row = 0
    for fields in creader:
        row += 1
        key = fields[0]
        entry = fields[1:]
        dcount = len(entry)
        if columnCount == 0:
            columnCount = dcount
        elif dcount != columnCount:
            eprintf("File %s, row %d.  Expecting %d entries.  Found %d" % (fname, row, columnCount, dcount))
            sys.exit(1)
        entries[key] = entry
    infile.close()
    return (entries, columnCount)
    
# Merge two sets of entries.
# When they both don't have the same keys, then either form superset or subset
def merge(entries1, count1, entries2, count2, subset = True, default = None):
    entries = {}
    dfill = "" if default is None else default
    for k in entries1.keys():
        entry1 = entries1[k]
        if k in entries2:
            entry2 = entries2[k]
            entries[k] = entry1 + entry2
        elif not subset:
            entry2 = [dfill] * count2
            entries[k] = entry1 + entry2
    if not subset:
        for k in entries2.keys():
            if k in entries1:
                continue
            entry1 = [dfill] * count1
            entry2 = entries2[k]
            entries[k] = entry1 + entry2
    return entries
        
def mergeConstant(entries1, value):
    entries = {}
    for k in entries1.keys():
        entry1 = entries1[k]
        entries[k] = entry1 + [str(value)]
    return entries

def nextFile(fname, first, subset, default):
    global globalEntries, globalCount
    value = None
    try:
        value = int(fname)
    except:
        pass
    if value is not None:
        globalEntries = mergeConstant(globalEntries, value)
        globalCount += 1
        return

    entries, ccount = processFile(fname)
    if first:
        globalEntries = entries
        globalCount = ccount
    else:
        globalEntries = merge(globalEntries, globalCount, entries, ccount, subset, default)
        globalCount += ccount

def sumEntries(sumList):
    global globalEntries, globalCount
    for k in globalEntries.keys():
        fields = globalEntries[k]
        sfields = [fields[i] for i in range(globalCount) if i+1 in sumList]
        try:
            nums = [float(field) if len(field) > 0 else 0.0 for field in sfields]
        except:
            print("Couldn't sum fields for line with key %s.  Summing over %s" % (k, str(sfields)))
            sys.exit(1)
        sval = sum(nums)
        fields.append(sval)
    globalCount += 1

def divideEntries(ratioList):
    global globalEntries, globalCount
    for k in globalEntries.keys():
        fields = globalEntries[k]
        try:
            sfields = [fields[i-1] for i in ratioList]
        except:
            print("Couldn't get fields %s from line with key %s.  Fields: %s" % (str(ratioList), k, str(fields)))
            sys.exit(1)
        try:
            nums = [float(field) if len(field) > 0 else 0.0 for field in sfields]
        except:
            print("Couldn't divide fields for line with key %s.  Dividing over %s" % (k, str(sfields)))
            sys.exit(1)
        rval = 1000 * 1000.0 if nums[1] == 0 else nums[0]/nums[1]
        fields.append(rval)
    globalCount += 1

def omitEntries(omitTuple):
    global globalEntries, globalCount
    idx, thresh = omitTuple
    gt = True
    if idx < 0:
        gt = False
        idx = -idx
    omitList = []
    for k in globalEntries.keys():
        fields = globalEntries[k]
        try:
            sfield = fields[idx-1]
        except:
            print("Couldn't get field #%d from line with key %s.  Fields: %s" % (idx, k, str(fields)))
            sys.exit(1)
        try:
            vfield = float(sfield)
        except:
            print("Couldn't convert value '%s' to float.  Fields: %s" % (sfield, str(fields)))
            sys.exit(1)
        if gt and vfield > thresh or not gt and vfield <= thresh:
            omitList.append(k)
    for k in omitList:
        del globalEntries[k]


def thresholdEntries(thresholdTuple):
    global globalEntries, globalCount
    idx, thresh, nval = thresholdTuple
    for k in globalEntries.keys():
        fields = globalEntries[k]
        try:
            sfield = fields[abs(idx)-1]
        except:
            print("Couldn't get field #%d from line with key %s.  Fields: %s" % (idx, k, str(fields)))
            sys.exit(1)
        try:
            vfield = float(sfield)
        except:
            print("Couldn't convert value '%s' to float.  Fields: %s" % (sfield, str(fields)))
            sys.exit(1)
        if idx >= 0:
            tval = vfield if vfield <= thresh else nval
        else:
            tval = vfield if vfield >= thresh else nval
        fields.append(tval)

def numOrString(val):
    result = val
    try:
        result = int(val)
    except:
        pass
    return result

def build(lstring, flist, doFilter, default, sumList, ratioList, thresholdTuple, omitTuple):
    first = True
    for fname in flist:
        nextFile(fname, first, doFilter, default)
        first = False
    if sumList is not None:
        sumEntries(sumList)
    if ratioList is not None:
        divideEntries(ratioList)
    if thresholdTuple is not None:
        thresholdEntries(thresholdTuple)
    if omitTuple is not None:
        omitEntries(omitTuple)
    if len(lstring) > 0:
        print(lstring)
    for k in sorted(globalEntries.keys(), key = lambda x:numOrString(x)):
        sfields = [k] + [str(field) for field in globalEntries[k]]
        print(",".join(sfields))

def run(name, args):
    doFilter = False
    default = None
    sumList = None
    ratioList = None
    thresholdTuple = None
    omitTuple = None
    lstring = ""
    optList, args = getopt.getopt(args, "hfd:s:r:t:o:l:")
    for (opt, val) in optList:
        if opt == '-h':
            usage(name)
            sys.exit(0)
        elif opt == '-f':
            doFilter = True
        elif opt == '-d':
            default = val
        elif opt == '-s' or opt == '-r' or opt == '-t' or opt == '-o':
            fields = val.split(':')
            try:
                ivals = [int(field) for field in fields]
                if opt == '-s':
                    sumList = ivals
                elif opt == '-r':
                    ratioList = ivals
                    if len(ratioList) != 2:
                        eprint("Ratio can only be between two elements")
                        usage(name)
                        sys.exit(1)
                elif opt == '-t':
                    thresholdTuple = tuple(ivals)
                    if len(thresholdTuple) != 3:
                        eprint("Thresholding requires three values: index, threshold, and new value")
                        usage(name)
                        sys.exit(1)
                else:
                    omitTuple = tuple(ivals)
                    if len(omitTuple) != 2:
                        eprint("Omitting requires two values: index and threshold")
                        usage(name)
                        sys.exit(1)

            except Exception as ex:
                eprint("Couldn't extract column numbers from argument '%s' (%s)" % (val % str(ex)))
                usage(name)
                sys.exit(1)
        elif opt == '-l':
            lstring = val
        else:
            eprint("Unknown option '%s'" % opt)
            usage(name)
            sys.exit(1)
    build(lstring, args, doFilter, default, sumList, ratioList, thresholdTuple, omitTuple)

if __name__ == "__main__":
    run(sys.argv[0], sys.argv[1:])

        

        
    
    

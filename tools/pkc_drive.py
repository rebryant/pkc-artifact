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

import getopt
import sys
import os.path
import subprocess
import datetime
import time
import glob

def usage(name):
    print("Usage: %s [-h] [-f] [-x] [-X] [-1] [-P PRE] [-T (n|d|p)] [-v VERB] [-m i|t|m|d|c|p] [-t TIME] [-l NFILE] [FILE.EXT ...]" % name)
    print("  -h       Print this message")
    print("  -f       Force regeneration of all files")
    print("  -x       Exit after first error (including timeout)")
    print("  -X       Remove all temporary files generated by PKC")
    print("  -1       Use D4 version 1")
    print("  -P PRE   Specify preprocessing level: (0:None, 1:+BCP, 2:+pure lit, >=3:+BVE(P-2)")
    print("  -T TSE   Specify use of Tseitin variables (n=none, d=detect, p=promote)")
    print("  -v       Set verbosity level")
    print("  -m       Select mode: i: incremental, t: trim, m: monolithic, d: defer splitting on projection variables, ")
    print("             c: compile without projection, p: preprocess only")
    print("  -t TIME  Limit time for each of the programs")
    print("  -l NFILE Specify file containing root names")
    print("  EXT can be any extension for wild-card matching (e.g., cnf, nnf)")

# Defaults
standardTimeLimit = 60

verbLevel = 1
force = False
nameFile = None
deleteTemporaries = False
exitWhenError = False
modeFlag = 'i'
tseitinFlag = 'p'

# Number of seconds to wait after terminating process to delete temporary files
waitTime = 3

modeDict = {'i':"incr", 'm':"mono", 't':"trim", 'd':"defer", 'c':"compile", 'p':"preprocess"}

tseitinDict = { 'n':"tnone", 'd':"tdetect", 'p':"tpromote" }

d4Flag = "-2"
d4Dict = {'-1': "d4v1", '-2' : "d4v2" }

preprocessLevel = 2

preprocessLabelDict = {
    0: "nopre",
    1: "bpc",
    2: "purify", 
}

def preprocessLabel(level):
    if level in preprocessLabelDict:
        return preprocessLabelDict[level]
    return "bve" + str(level-2)

# Pathnames
def genProgramPath():
    ppath = os.path.abspath(__file__)
    parts = ppath.split('/')
    if len(parts) >= 2:
        parts[-2] = 'src'
        parts[-1] = 'pkc'
    npath = "/".join(parts)
    print("Running program '%s'" % npath)
    return npath



timeLimit = 4000

commentChar = 'c'

tempPrefix = "zzzz-"
tempExtensions = ["nnf", "cnf"]

def trim(s):
    while len(s) > 0 and not s[0].isalnum():
        s = s[1:]
    while len(s) > 0 and s[-1] in '\r\n':
        s = s[:-1]
    return s

def setTimeLimit(t):
    global timeLimit
    timeLimit = t

def killJobs(matchString):
    psCmd = ["ps", "-a"]
    pp = subprocess.run(psCmd, capture_output = True, text=True)
    if pp.returncode != 0:
        return "WARNING: Couldn't find job number"
    lines = pp.stdout.split("\n")
    result = "Jobs: "
    jcount = 0
    for line in lines:
        line = trim(line)
        if line.find(matchString) >= 0:
            fields = line.split()
            jobName = fields[0]
            killCmd = ["kill", jobName]
            kp = subprocess.run(killCmd)
            if kp.returncode == 0:
                result += jobName + ":killed "
            else:
                result += jobName + ":not killed "
            jcount += 1
    if jcount == 0:
        result += "None"
    return result

def removeTempFiles():
    for ext in tempExtensions:
        files = glob.glob(tempPrefix + "*." + ext)
        count = 0
        for file in files:
            try:
                os.remove(file)
                count += 1
            except:
                pass
        if verbLevel > 1 and len(files) > 0:
            print("Removed %d/%d files of the form %s*.%s" % (count, len(files), tempPrefix, ext))
                
def runProgram(prefix, root, commandList, logFile, extraLogName = None):
    tlimit = timeLimit
    result = ""
    cstring = " ".join(commandList)
    print("%s. %s: Running '%s' with time limit of %d seconds" % (root, prefix, cstring, tlimit))
    logFile.write("%s LOG: Running %s\n" % (prefix, cstring))
    logFile.write("%s LOG: Time limit %d seconds\n" % (prefix, tlimit))
    start = datetime.datetime.now()
    try:
        cp = subprocess.run(commandList, capture_output = True, timeout=tlimit, text=True)
    except subprocess.TimeoutExpired as ex:
        # Incorporate information recorded by external logging
        if (extraLogName is not None):
            try:
                xlog = open(extraLogName, "r")
                for line in xlog:
                    logFile.write(line)
                xlog.close()
            except:
                pass
        print("%s. %s Program timed out after %d seconds" % (root, prefix, tlimit))
        result += "%s ERROR: Timeout after %d seconds\n" % (prefix, tlimit)
        delta = datetime.datetime.now() - start
        seconds = delta.seconds + 1e-6 * delta.microseconds
        result += "%s LOG: Elapsed time = %.3f seconds\n" % (prefix, seconds)
        result += "%s OUTCOME: Timeout\n" % (prefix)
        logFile.write(result)
        logFile.close()
        print("%s KILL: %s" % (prefix, killJobs(tempPrefix + root)))
        return False
    ok = True
    if cp.returncode != 0:
        result += "%s ERROR: Return code = %d\n" % (prefix, cp.returncode)
        result += "%s LOG: %s" % (prefix, killJobs(root))
        ok = False
    outcome = "normal" if ok else "failed"
    delta = datetime.datetime.now() - start
    seconds = delta.seconds + 1e-6 * delta.microseconds
    result += "%s LOG: Elapsed time = %.3f seconds\n" % (prefix, seconds)
    result += "%s OUTCOME: %s\n" % (prefix, outcome)
    print("%s. %s: OUTCOME: %s" % (root, prefix, outcome))
    print("%s. %s: Elapsed time: %.3f seconds" % (root, prefix, seconds))
    logFile.write(cp.stdout)
    logFile.write(result)
    if deleteTemporaries:
        if waitTime > 0:
            print("%s LOG: Waiting %d seconds for subprocess to terminate" % (prefix, waitTime))
            time.sleep(waitTime)
        removeTempFiles()
    if not ok:
        print("%s KILL: %s" % (prefix, killJobs(tempPrefix + root)))        
    return ok

def genLogName(root, home):
    extension = "pkc_"
    tlabel = tseitinDict[tseitinFlag]
    extension += tlabel + "_"
    plabel = preprocessLabel(preprocessLevel)
    extension += plabel + "_"
    extension += modeDict[modeFlag] + "_"
    extension += d4Dict[d4Flag] + "_"
    extension += "log"
    logName = home + "/" + root + "." + extension
    return logName

def runPkc(root, home):
    cnfName = home + "/" + root + ".cnf"
    logName = genLogName(root, home)
    extraLogName = "backup.log"
    if not force and os.path.exists(logName):
        print("Already have file '%s'.  Skipping" % logName)
        return True
    cmd = [genProgramPath()]
    cmd += ["-v", str(verbLevel)]
    cmd += ["-L", extraLogName]
    cmd += ["-P", str(preprocessLevel)]
    cmd += ["-T", tseitinFlag]
    cmd += ["-m", modeFlag]
    if d4Flag != "-2":
        cmd += [d4Flag]
    cmd += [cnfName]
    try:
        logFile = open(logName, "w")
    except:
        print("%s ERROR:Couldn't open file '%s'" % (root, logName))
        return
    ok = runProgram("PKC", root, cmd, logFile, extraLogName = extraLogName)
    print("File %s written" % logName)
    return ok

def stripSuffix(fname):
    fields = fname.split(".")
    if len(fields) > 1:
        fields = fields[:-1]
    return ".".join(fields)

def runBatch(home, fileList, force):
    roots = [stripSuffix(f) for f in fileList]
    roots = [r for r in roots if r is not None]
    print("Running on roots %s" % roots)
    for r in roots:
        if not runPkc(r, home) and exitWhenError:
            print("Error encountered.  Exiting")
            break

def run(name, args):
    global verbLevel, force, nameFile, deleteTemporaries, modeFlag, d4Flag, preprocessLevel, tseitinFlag, exitWhenError
    home = "."
    optList, args = getopt.getopt(args, "hfXxv:m:1t:l:P:T:")
    for (opt, val) in optList:
        if opt == '-h':
            usage(name)
            return
        if opt == '-v':
            verbLevel = int(val)
        elif opt == '-P':
            preprocessLevel = int(val)
        elif opt == '-T':
            if val in tseitinDict:
                tseitinFlag = val
            else:
                print("Unknown Tseitin flag '%s'" % val)
                usage(name)
                return
        elif opt == '-1':
            d4Flag = opt
        elif opt == '-f':
            force = True
        elif opt == '-x':
            exitWhenError = True
        elif opt == '-m':
            if val not in modeDict:
                print("Unknown mode '%s'" % val)
                usage(name)
                return
            modeFlag = val
        elif opt == '-X':
            deleteTemporaries = True
        elif opt == '-t':
            setTimeLimit(int(val))
        elif opt == '-l':
            nameFile = val
        else:
            print("Unknown option '%s'" % opt)
            usage(name)
            return
    fileList = args
    if nameFile is not None:
        try:
            nfile = open(nameFile, 'r')
        except:
            print("Couldn't open name file '%s'" % nameFile)
            usage(name)
            return
        for line in nfile:
            fname = trim(line)
            fileList.append(fname)
        nfile.close
            
    runBatch(home, fileList, force)
    if deleteTemporaries:
        removeTempFiles()

if __name__ == "__main__":
    run(sys.argv[0], sys.argv[1:])
    sys.exit(0)


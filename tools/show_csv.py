#!/usr/bin/python3

# Print table based on CSV file
import sys

# Default field width
fieldWidth = 10
# Space between fields
space = 2

def trim(s):
    while len(s) > 0 and s[-1] in '\n\r':
        s = s[:-1]
    return s
   
def formatLine(fields, widths):
    s = ""
    for i in range(len(fields)):
        s += " " * (widths[i]-len(fields[i])) + fields[i]
    return s
                  

def format(file):
    lines = []    
    widths = []
    for line in file:
        line = trim(line)
        fields = line.split(',')
        if len(fields) > len(widths):
            widths += [fieldWidth] * (len(line) - len(widths))
        for i in range(len(fields)):
            widths[i] = max(widths[i], space + len(fields[i]))
        lines.append(fields)
    file.close()
    # print the lines
    for fields in lines:
        print(formatLine(fields, widths))

        
def run(name, args):
    file = sys.stdin
    if len(args) > 0:
        if args[0] == '-h' or len(args) > 1:
            print("Usage: %s [FILE.csv]" % name)
            return
        else:
            try:
                file = open(args[0], 'r')
            except:
                print("Can't open file '%s'" % args[1])
                return
    format(file)

if __name__ == "__main__":
    run(sys.argv[0], sys.argv[1:])
    sys.exit(0)
    
              

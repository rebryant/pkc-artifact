This directory contains the source files for the PKC knowledge compiler.

Running "make" should generate the executable and install it in this directory

SUBDIRECTORIES:

	glucose-3.0
Source code for Glucose SAT solver.  Obtained from:

       https://github.com/audemard/glucose/releases/tag/3.0


FILES:

	pkc.cpp
The main program file
 
	pog.{hh,cpp}
Construct and manipulate a POG representation of the generated result

        compile.{hh,cpp}
Perform compilation using D4

	project.{hh,cpp}
Perform projection

	files.{hh,cpp}
Manage temporary files

	report.{h,c}
Useful logging and reporting utilities

        counters.{h,c}
Track counts and time spent on different tasks

        q25.{h,c}
Represent and manipulate rational numbers of the form a * 2^b * 5^c

        find_path.sh
Used by the compiler to record path of this directory during compilation

        d4v2_ubuntu64_static
Executable file for D4 version 2 for 64-bit Linux

        d4v2
Symbolic link to executable file for D4 version 2



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


#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "files.hh"
#include "report.h"

// File management
// Share instance of file manager globally
File_manager fmgr;

File_manager::File_manager() {
    root = archive_string("zzzz-temporary");
    sequence_number = 1000000;
    buflen = 1000;
    buf = (char *) malloc(buflen);
    allow_flush = false;
}


// Use file name to construct root for temporary names
void File_manager::set_root(const char *fname) {
    // Trim any leading path directories
    int lpos = strlen(fname);
    while (lpos >= 0 && fname[lpos] != '/')
	lpos--;
    lpos++;
    snprintf(buf, strlen(fname) + 10, "zzzz-%s", fname+lpos);
    // Chop off extension
    int rpos = strlen(buf);
    while (rpos >= 0 && buf[rpos] != '.')
	rpos--;
    if (rpos > 0)
	buf[rpos] = '\0';
    root = archive_string(buf);
}

const char *File_manager::build_name(const char *extension, bool new_sequence) {
    if (new_sequence)
	sequence_number++;
    snprintf(buf, buflen, "%s-%d.%s", root, sequence_number, extension);
    const char *result = archive_string(buf);
    names.push_back(result);
    return result;
}

void File_manager::flush() {
    if (!allow_flush)
	return;
    for (const char *fname : names) {
	if (remove(fname) != 0)
	    err(false, "Attempt to delete file %s failed.  Error code = %d\n", fname, errno);
	free((void *) fname);
    }
    names.clear();
}

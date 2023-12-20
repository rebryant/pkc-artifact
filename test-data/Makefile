SDIR = ../src
PKC = $(SDIR)/pkc

FILES = eg-proj.pkc_log majt-09.pkc_log majp-11.pkc_log

test: clean $(FILES)

.SUFFIXES: .cnf .pkc_log

.cnf.pkc_log:
	$(PKC) $< | tee $@

clean:
	rm -f *~
	rm -f *.pkc_log

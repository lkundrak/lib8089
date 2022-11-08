TARGETS = dis89 dis89.1

all: $(TARGETS)

8089.o dis89.o: 8089.h
dis89: dis89.o 8089.o

%.1: %.pod
	pod2man --center 'Development Tools' \
		--section 1 --date 2022-06-04 --release 1 $< >$@

%.pdf: %.1
	groff -Tpdf -mman $< >$@

clean:
	rm -f $(TARGETS) *.o *.pdf

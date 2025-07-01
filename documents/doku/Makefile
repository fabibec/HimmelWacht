TEX=pdflatex
BIB=biber
SRC=main
TARGET=dokumentation
OUTDIR=output

all: build clean

build:
	mkdir -p $(OUTDIR)
	$(TEX) -output-directory=$(OUTDIR) $(SRC)
	$(BIB) --output-directory $(OUTDIR) $(SRC)
	$(TEX) -output-directory=$(OUTDIR) $(SRC)
	$(TEX) -output-directory=$(OUTDIR) $(SRC)
	mv $(OUTDIR)/$(SRC).pdf $(OUTDIR)/$(TARGET).pdf

clean:
	rm -f $(OUTDIR)/*.log \
	      $(OUTDIR)/*.out \
	      $(OUTDIR)/*.toc \
	      $(OUTDIR)/*.lof \
	      $(OUTDIR)/*.lot \
	      $(OUTDIR)/*.fls \
	      $(OUTDIR)/*.fdb_latexmk \
	      $(OUTDIR)/*.synctex.gz \
	      $(OUTDIR)/*.blg \
	      $(OUTDIR)/*.run.xml

cleanall: clean
	rm -f $(OUTDIR)/*.aux $(OUTDIR)/*.bbl $(OUTDIR)/*.bcf $(OUTDIR)/*.pdf

.PHONY: all build clean
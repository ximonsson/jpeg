include makefile.include

JPGMAKE = makefile.jpegcodec
TRANSCODEMAKE = makefile.transcode
PLAYERMAKE = makefile.player

REPORTDIR = report


all: hw asm std

hw:
	make -f $(JPGMAKE) hw

asm:
	make -f $(JPGMAKE) asm

std:
	make -f $(JPGMAKE) std

transcode: asm
	make -f $(TRANSCODEMAKE)

player: asm
	make -f $(PLAYERMAKE)

clean:
	rm -r $(BUILD)/* $(BIN)/*

report:
	@cd $(REPORTDIR)
	gnuplot $(REPORTDIR)/results-jpeg.gp
	makeindex $(REPORTDIR)/report.tex
	pdflatex $(REPORTDIR)/report.tex

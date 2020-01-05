_dummy := $(shell mkdir -p obj)

CFLAGS = -c -Ilibjpeg -MMD -O3
LDFLAGS =
LIBJPEG := libjpeg/obj/libjpeg.a
ODIR = obj

EXEBASE = blarghrad
EXE = $(ODIR)/blarghrad
all: $(EXE)

clean:
	rm -f $(ODIR)/*.o $(ODIR)/$(EXEBASE)

FILES = $(ODIR)/blarghrad.o $(ODIR)/verify.o $(ODIR)/cmdlib.o $(ODIR)/patches.o $(ODIR)/scriplib.o $(ODIR)/threads.o $(ODIR)/lightmap.o $(ODIR)/polylib.o $(ODIR)/bspfile.o $(ODIR)/mathlib.o $(ODIR)/trace.o

$(EXE) : $(FILES)
	g++ -pthread -o $(EXE) $(LDFLAGS) $(FILES) $(LIBJPEG) -lm

$(ODIR)/blarghrad.o : blarghrad.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/verify.o : verify.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/cmdlib.o : cmdlib.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/patches.o : patches.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/scriplib.o : scriplib.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/threads.o : threads.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/lightmap.o : lightmap.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/polylib.o : polylib.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/bspfile.o : bspfile.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/mathlib.o : mathlib.cpp
	g++ $(CFLAGS) $? -o $@
$(ODIR)/trace.o : trace.cpp
	g++ $(CFLAGS) $? -o $@

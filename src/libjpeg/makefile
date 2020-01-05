_dummy := $(shell mkdir -p obj)

CFLAGS = -c -MMD
LDFLAGS =
ODIR = obj

EXEBASE = jpeg
EXE = $(ODIR)/libjpeg
all: $(EXE)

clean:
	rm -f $(ODIR)/*.o $(ODIR)/$(EXEBASE)

SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c, $(ODIR)/%.o, $(SOURCES))

$(EXE) : $(OBJECTS)
	ar rcs $(EXE).a $(OBJECTS)

all: $(OBJECTS)

$(ODIR)/%.o: %.c
	cc $(CFLAGS) -c $< -o $@

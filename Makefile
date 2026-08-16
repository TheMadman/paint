LDLIBS=-lSDL3 -lgamesh -lsrvsh

all: paint

install: all
	$(INSTALL_PROGRAM) paint $(bindir)/paint

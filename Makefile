LDLIBS=-lSDL3 -lgamesh -lsrvsh
INSTALL=install
INSTALL_PROGRAM=$(INSTALL)
PREFIX=/usr/local
bindir=$(PREFIX)/bin

all: paint

install: all | $(bindir)
	$(INSTALL_PROGRAM) paint $(bindir)/paint

$(bindir):
	mkdir -p $@

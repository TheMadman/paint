LDLIBS=-lSDL3 -lgamesh -lsrvsh -ladt
INSTALL=install
INSTALL_PROGRAM=$(INSTALL)
PREFIX=/usr/local
bindir=$(PREFIX)/bin

all: paint-canvas paint-ui

install: all | $(bindir)
	$(INSTALL_PROGRAM) paint $(bindir)/paint
	$(INSTALL_PROGRAM) paint.srvsh $(bindir)/paint.srvsh
	$(INSTALL_PROGRAM) paint-canvas $(bindir)/paint-canvas
	$(INSTALL_PROGRAM) paint-ui $(bindir)/paint-ui

$(bindir):
	mkdir -p $@

LDLIBS=-lSDL3 -lgamesh -lsrvsh
INSTALL=install
INSTALL_PROGRAM=$(INSTALL)
PREFIX=/usr/local
bindir=$(PREFIX)/bin

all: paint-canvas

install: all | $(bindir)
	$(INSTALL_PROGRAM) paint $(bindir)/paint
	$(INSTALL_PROGRAM) paint.srvsh $(bindir)/paint.srvsh
	$(INSTALL_PROGRAM) paint-canvas $(bindir)/paint-canvas

$(bindir):
	mkdir -p $@

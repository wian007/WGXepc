CC      ?= gcc
CFLAGS  ?= -O2 -Wall
PREFIX  ?= /usr/local
SBINDIR  = $(PREFIX)/sbin
UNITDIR  = /etc/systemd/system

all: wgxepc

wgxepc: WGXepc.c
	$(CC) $(CFLAGS) -o $@ $<

install: wgxepc
	install -D -m 755 wgxepc $(DESTDIR)$(SBINDIR)/wgxepc

install-service: install
	install -D -m 755 systemd/99-fansandled.sh $(DESTDIR)$(SBINDIR)/99-fansandled.sh
	install -D -m 644 systemd/wgxepc-fansandled.service $(DESTDIR)$(UNITDIR)/wgxepc-fansandled.service
	@echo "Now run: sudo systemctl daemon-reload && sudo systemctl enable --now wgxepc-fansandled.service"

uninstall:
	rm -f $(DESTDIR)$(SBINDIR)/wgxepc $(DESTDIR)$(SBINDIR)/99-fansandled.sh
	rm -f $(DESTDIR)$(UNITDIR)/wgxepc-fansandled.service

clean:
	rm -f wgxepc

.PHONY: all install install-service uninstall clean

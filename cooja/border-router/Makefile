CONTIKI_PROJECT = border-router
all: $(CONTIKI_PROJECT)

MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_LITE
MAKE_MAC = MAKE_MAC_CSMA
MAKE_WITH_SLIP = 1

MODULES += os/services/rpl-border-router

PROJECTDIRS += webserver
PROJECT_SOURCEFILES += httpd-simple.c webserver.c

CONTIKI = /home/mohsin/contiki-ng
include $(CONTIKI)/Makefile.include

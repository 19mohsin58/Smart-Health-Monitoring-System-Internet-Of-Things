CONTIKI_PROJECT = sensor-node
all: $(CONTIKI_PROJECT)

MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_LITE
MAKE_MAC = MAKE_MAC_CSMA

MODULES += os/net/app-layer/mqtt
CFLAGS += -Wno-error -Wno-format

CONTIKI = /home/mohsin/contiki-ng
include $(CONTIKI)/Makefile.include

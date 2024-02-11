DPDK_FLAGS = $(shell pkg-config --libs libdpdk)

all: client server

client: client.c
	gcc -o $@ $< ../../lib/libfstack.a -I../../lib/ $(DPDK_FLAGS) -lcrypto

server: server.c
	gcc -o $@ $< ../../lib/libfstack.a -I../../lib/ $(DPDK_FLAGS) -lcrypto
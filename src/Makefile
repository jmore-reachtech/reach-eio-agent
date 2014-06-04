sources = eio_agent.c \
	eio_local.c \
	eio_server_socket.c \
	eio_tio_socket.c \
	logmsg.c

headers = eio_agent.h

LDFLAGS=-pthread

CFLAGS=-Wall

ifeq ($(DEBUG_DEF),1)
	DEBUG = -g -O0 -DDEBUG
else
	DEBUG = -O2
endif

all: eio-agent

eio-agent: $(sources) $(headers)
	$(CC) -DEIO_VERSION='"$(AGENT_VERSION)"' $(CFLAGS) $(LDFLAGS) $(DEBUG) -o $@ $(sources)

clean:
	$(RM) eio-agent

.PHONY: all clean

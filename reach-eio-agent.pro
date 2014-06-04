TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += src/eio_agent.c \
        src/eio_local.c \
        src/eio_tio_socket.c \
        src/eio_server_socket.c \
        src/logmsg.c

HEADERS += src/eio_agent.h


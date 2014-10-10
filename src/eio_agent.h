#ifndef EIO_AGENT_H
#define EIO_AGENT_H

#include <syslog.h>
#include <sys/stat.h>


/* functions defined in eio_server_socket.c */
int eioServerSocketInit(unsigned short port, int *addressFamily);
int eioServerSocketAccept(int serverFd, int addressFamily);
int eioServerSocketRead(int newFd, char *msgBuff, size_t bufferSize);
int eioServerSocketReadLine(int fd, void *buffer, int n);
void eioServerSocketWrite(int socketFd, const char *buff);


/* functions defined in eio_tio_socket.c */
int eioTioSocketInit(int *addressFamily,
    const char *unixSocketPath);
int eioTioSocketAccept(int serverFd, int addressFamily);
int eioTioSocketRead(int newFd, char *msgBuff, size_t bufferSize);
void eioTioSocketWrite(int socketFd, const char *buff);

/* functions defined in eio_local.c */
char *eioHandleLocal(char *qmlString);

/* functions exported from logmsg.c */
void LogOpen(const char *ident, int logToSyslog, const char *logFilePath,
    int verboseFlag);
void LogMsg(int level, const char *fmt, ...);

#define EIO_DEFAULT_SERVER_AGENT_PORT 7880
#define EIO_AGENT_UNIX_SOCKET "/tmp/sioSocket"

#define EIO_BUFFER_SIZE 2048

#endif  /* EIO_AGENT_H */

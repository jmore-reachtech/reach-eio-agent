#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h> 
#include <sys/un.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "eio_agent.h"

#define MAXPENDING 1

static void eioDieWithError(char *errorMessage)
{
    LogMsg(LOG_ERR, "Exiting: %s\n", errorMessage);
    exit(1);
}


static int eioCreateTCPServerSocket(unsigned short port)
{
    int sock;
    struct sockaddr_in echoServAddr;

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        eioDieWithError("socket() failed");
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET; 
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&echoServAddr,
        sizeof(echoServAddr)) < 0) {
        eioDieWithError("bind() failed");
    }

    if (listen(sock, MAXPENDING) < 0) {
        eioDieWithError("listen() failed");
    }

    return sock;
}


int eioServerSocketAccept(int serverFd, int addressFamily)
{
    /* define a variable to hold the client's address for either family */
    union {
        struct sockaddr_un unixClientAddr;
        struct sockaddr_in inetClientAddr;
    } clientAddr;
    socklen_t clientLength = sizeof(clientAddr);

    const int clientFd = accept(serverFd, (struct sockaddr *)&clientAddr,
        &clientLength);
    if (clientFd >= 0) {
        switch (addressFamily) {
        case AF_UNIX:
            LogMsg(LOG_INFO, "Handling Unix client\n");
            break;

        case AF_INET:
            LogMsg(LOG_INFO, "Handling TCP client %s\n",
                inet_ntoa(clientAddr.inetClientAddr.sin_addr));
            break;

        default:
            break;
        }
    }

    return clientFd;
}


int eioServerSocketInit(unsigned short port, int *addressFamily)
{
    int listenFd = -1;
    listenFd = eioCreateTCPServerSocket(port);
    *addressFamily = AF_INET;
    return listenFd;
}


/**
 * Reads a single message from the socket connected to the 
 * tcp/ip server port. If no message is ready to be received, the call
 * will block until one is available. 
 * 
 * @param socketFd the file descriptor of for the already open 
 *                 socket connecting to the tio-agent
 * @param msgBuff address of a contiguous array into which the 
 *                message will be written upon receipt from the
 *                tio-agent
 * @param bufferSize the number of bytes in msgBuff
 * 
 * @return int 0 if no message to return (handled here), -1 if 
 *         recv() returned an error code (close connection) or
 *         >0 to indicate msgBuff has that many characters
 *         filled in
 */
int eioServerSocketRead(int socketFd, char *msgBuff, size_t bufferSize)
{
    int cnt;

    if ((cnt = recv(socketFd, msgBuff, bufferSize, 0)) <= 0) {
        LogMsg(LOG_INFO, "%s(): recv() failed, client closed\n", __FUNCTION__);
        close(socketFd);
        return -1;
    } else {
        msgBuff[cnt] = 0;
        LogMsg(LOG_INFO, "%s: buff = %s", __FUNCTION__, msgBuff);
        return cnt;
    }
}


int eioServerSocketReadLine(int fd, void *buffer, int n)
{
    int numRead;                    /* # of bytes fetched by last read() */
    int totRead;                     /* Total bytes read so far */
    char *buf;
    char ch;

    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    buf = buffer;                       /* No pointer arithmetic on "void *" */

    totRead = 0;
    for (;;) {
        numRead = read(fd, &ch, 1);

        if (numRead == -1) {
            if (errno == EINTR)         /* Interrupted --> restart read() */
                continue;
            else
                return -1;              /* Some other error */

        } else if (numRead == 0) {      /* EOF */
            if (totRead == 0)           /* No bytes read; return 0 - client disconnected*/
                return -1;
            else                        /* Some bytes read; add '\0' */
                break;
        } else if (totRead == 0 && (ch == '\n' || ch == '\r') ) {  /* Empty message */
            return 0;
        } else {                        /* 'numRead' must be 1 if we get here */
            if (totRead < n - 1 && ch != '\n' && ch != '\r' ) {      /* Discard > (n - 1) bytes */
                totRead++;
                *buf++ = ch;
            }

            if (ch == '\n' || ch == '\r')
            {
                totRead++;
                *buf++ = '\n';
                break;
            }
        }
    }

    *buf = '\0';
    LogMsg(LOG_INFO, "%s: buff = %s", __FUNCTION__, buffer);
    return totRead;
}



void eioServerSocketWrite(int socketFd, const char *buff)
{
    int cnt = strlen(buff);

    if (send(socketFd, buff, cnt, 0) != cnt) {
        LogMsg(LOG_ERR, "socket_send_to_client(): send() failed, %d\n",
            socketFd);
        perror("what's messed up?");
    }
    else
    {
        LogMsg(LOG_INFO, "%s: sent = %s", __FUNCTION__, buff);
    }
}



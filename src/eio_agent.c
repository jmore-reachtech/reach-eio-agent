#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "eio_agent.h"

/* module-wide "global" variables */
static int keepGoing;
static const char *progName;

static void eioDumpHelp();
static void eioAgent(unsigned short tcpPort, const char *unixSocketPath);
static inline int max(int a, int b) { return (a > b) ? a : b; }

int main(int argc, char *argv[])
{
    int daemonFlag = 0;
    unsigned short tcpServerPort = 0;
    const char *logFilePath = 0;
    /*
     * syslog isn't installed on the target so it's disabled in this program
     * by requiring an argument to -o|--log.
     */
    int logToSyslog = 0;
    int verboseFlag = 0;

    /* allocate memory for progName since basename() modifies it */
    const size_t nameLen = strlen(argv[0]) + 1;
    char arg0[nameLen];
    memcpy(arg0, argv[0], nameLen);
    progName = basename(arg0);

    while (1) {
        static struct option longOptions[] = {
            { "daemon",      no_argument,       0, 'd' },
            { "log",         required_argument, 0, 'o' },
            { "server_port", required_argument, 0, 's' },
            { "verbose",     no_argument,       0, 'v' },
            { "help",        no_argument,       0, 'h' },
            { 0,            0, 0,  0  }
        };
        int c = getopt_long(argc, argv, "d:o:s:vh?", longOptions, 0);

        if (c == -1) {
            break;  // no more options to process
        }

        switch (c) {
        case 'd':
            daemonFlag = 1;
            break;

        case 'o':
            if (optarg == 0) {
                logToSyslog = 1;
                logFilePath = 0;
            } else {
                logToSyslog = 0;
                logFilePath = optarg;
            }
            break;
        case 's':
            tcpServerPort = (optarg == 0) ? EIO_DEFAULT_SERVER_AGENT_PORT : atoi(optarg);
            break;

        case 'v':
            verboseFlag = 1;
            break;

        case '?':
        case 'h':
        default:
            eioDumpHelp();
            exit(1);
        }
    }

    /* set up logging to syslog or file; will be STDERR not told otherwise */
    LogOpen(progName, logToSyslog, logFilePath, verboseFlag);

    if (daemonFlag) {
        daemon(0, 1);
    }

    eioAgent(tcpServerPort, EIO_AGENT_UNIX_SOCKET);

    return 0;
}

static void eioDumpHelp()
{
    fprintf(stderr, "EIO Agent %s \n\n", EIO_VERSION);

    fprintf(stderr, "usage: %s [options]\n"
            "  where options are:\n"
            "    -d         | --daemon            run in background\n"
            "    -o<path>   | --logfile=<path>    log to file instead of stderr\n"
            "    -s[<port>] | --server_port[=<port>] TCP socket, default = %d\n"
            "    -v         | --verbose           print progress messages\n"
            "    -h         | -? | --help         print usage information\n",
            progName, EIO_DEFAULT_SERVER_AGENT_PORT);
}

static void eioInterruptHandler(int sig)
{
    keepGoing = 0;
}

/**
 * This is the main loop function.  It opens and configures the
 * TCP/IP Server port and opens the TIO socket using a Unix
 * domain and enters a select loop waiting for connections.
 *
 * @param tcpServerPort the port number to open for
 *                accepting connections from the server port;
 *
 * @param unixSocketPath the file system path to use for a Unix domain socket;
 */
static void eioAgent(unsigned short tcpServerPort, const char *unixSocketPath)
{
    int n;
    fd_set currFdSet;

    FD_ZERO(&currFdSet);

    /********************************** Set up TIO Socket ***********************************/
    int connectedTIOFd = -1;  /* not currently connected */

    {
        /* install a signal handler to remove the socket file */
        struct sigaction a;
        memset(&a, 0, sizeof(a));
        a.sa_handler = eioInterruptHandler;
        if (sigaction(SIGINT, &a, 0) != 0) {
            LogMsg(LOG_ERR, "sigaction() failed, errno = %d\n", errno);
            exit(1);
        }
    }

    /* open the tio socket */
    int addressTIOFamily = 0;
    const int listenTIOFd = eioTioSocketInit(&addressTIOFamily,
                                             unixSocketPath);
    if (listenTIOFd < 0) {
        /* open failed, can't continue */
        LogMsg(LOG_ERR, "could not open tio socket\n");
        return;
    }
    else
    {
        LogMsg(LOG_INFO, "TIO Unix Socket Open\n");
    }

    FD_SET(listenTIOFd, &currFdSet);

    /********************************** Set up TCP/IP Server Socket ***********************************/
    int connectedServerFd = -1;  /* not currently connected */

    /* open the server socket */
    int addressServerFamily = 0;
    const int listenServerFd = eioServerSocketInit(tcpServerPort, &addressServerFamily);
    if (listenServerFd < 0) {
        /* open failed, can't continue */
        LogMsg(LOG_ERR, "could not open server socket\n");
        return;
    }
    else
    {
        LogMsg(LOG_INFO, "Server Socket Open\n");
    }

    FD_SET(listenServerFd, &currFdSet);

    n = max(listenServerFd, listenTIOFd) + 1;

    /* execution remains in this loop until a fatal error or SIGINT */
    keepGoing = 1;

    while (keepGoing) {
        /*
         * This is the select loop which waits for characters to be received on
         * the tcp/ip server socket and on either the listen socket (meaning
         * an incoming connection is queued) or on a connected socket
         * descriptor.
         */

        //Wait for a connection to the server if none then it will block on the accept

        /* check for a new server connection to accept */
        if (FD_ISSET(listenServerFd, &currFdSet)) {
            /* new connection is here, accept it */
            connectedServerFd = eioServerSocketAccept(listenServerFd, addressServerFamily);
            if (connectedServerFd >= 0) {
                FD_CLR(listenServerFd, &currFdSet);
                FD_SET(connectedServerFd, &currFdSet);
                n = max(connectedServerFd,
                        ((connectedTIOFd >= 0) ? connectedTIOFd : listenTIOFd)) + 1;
            }
        }

        while (1) {

            if (connectedServerFd < 0)
                break;

            /* wait indefinitely for someone to blink */
            fd_set readFdSet = currFdSet;
            const int sel = select(n, &readFdSet, 0, 0, 0);

            if (sel == -1) {
                if (errno == EINTR) {
                    break;  /* drop out of inner while */
                } else {
                    LogMsg(LOG_ERR, "select() returned -1, errno = %d\n", errno);
                    exit(1);
                }
            } else if (sel <= 0) {
                continue;
            }


            /* check for packet received on the server socket */
            if ((connectedServerFd >= 0) && FD_ISSET(connectedServerFd, &readFdSet)) {
                /* tcp/ip server has something to relay to tio agent */
                char msgBuff[EIO_BUFFER_SIZE];
                const int readCount = eioServerSocketReadLine(connectedServerFd, msgBuff,
                                                          sizeof(msgBuff));
                if (readCount < 0) {
                    FD_CLR(connectedServerFd, &currFdSet);
                    FD_SET(listenServerFd, &currFdSet);
                    n = max(listenServerFd,
                            ((connectedTIOFd >= 0) ? connectedTIOFd : listenTIOFd)) + 1;
                    connectedServerFd = -1;
                } else if (readCount > 0) {
                    if (connectedTIOFd >= 0)
                        eioTioSocketWrite(connectedTIOFd, msgBuff);
                }
            }


            /* check for a new tio connection to accept */
            if (FD_ISSET(listenTIOFd, &readFdSet)) {
                /* new connection is here, accept it */
                connectedTIOFd = eioTioSocketAccept(listenTIOFd, addressTIOFamily);
                if (connectedTIOFd >= 0) {
                    FD_CLR(listenTIOFd, &currFdSet);
                    FD_SET(connectedTIOFd, &currFdSet);
                    n = max(connectedTIOFd,
                        ((connectedServerFd >= 0) ? connectedServerFd : listenServerFd)) + 1;
                }
            }

            /* check for packet received on the tio socket */
            if ((connectedTIOFd >= 0) && FD_ISSET(connectedTIOFd, &readFdSet)) {
                /* connected tio_agent has something to relay to tcp/ip server port */
                char msgBuff[EIO_BUFFER_SIZE];
                const int readCount = eioTioSocketRead(connectedTIOFd, msgBuff,
                                                       sizeof(msgBuff));
                if (readCount < 0) {
                    FD_CLR(connectedTIOFd, &currFdSet);
                    FD_SET(listenTIOFd, &currFdSet);
                    n = max(listenTIOFd,
                            ((connectedServerFd >= 0) ? connectedServerFd : listenServerFd)) + 1;
                    connectedTIOFd = -1;
                } else if (readCount > 0) {
                    if (connectedServerFd >= 0)
                        eioServerSocketWrite(connectedServerFd, msgBuff);
                }
            }
        }

    }

    LogMsg(LOG_INFO, "cleaning up\n");

    if (connectedTIOFd >= 0) {
        close(connectedTIOFd);
    }
    if (listenTIOFd >= 0) {
        close(listenTIOFd);
    }

    if (connectedServerFd >= 0) {
        close(connectedServerFd);
    }
    if (listenServerFd >= 0) {
        close(listenServerFd);
    }


    /* best effort removal of socket */
    const int rv = unlink(unixSocketPath);
    if (rv == 0) {
        LogMsg(LOG_INFO, "socket file %s unlinked\n", unixSocketPath);
    } else {
        LogMsg(LOG_INFO, "socket file %s unlink failed\n", unixSocketPath);
    }

}

/*
 * imagemount.c	- Attach and optionally mount a filesystem block image.
 */
/*
 * Copyright (c) 2010, Ideal World, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */
#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif /* HAVE_CONFIG_H */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <linux/nbd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#ifdef HAVE_SYS_CAPABILITY_H
#    include <sys/capability.h>
#endif /* HAVE_SYS_CAPABILITY_H */
#include "libimage.h"
#include "sysdep_posix.h"

#ifndef RUNDIR
#    define RUNDIR "/var/run"
#endif /* RUNDIR */

/*
 * Initial size of the read buffer.  Grows to larger values as required.
 */
#define READBUF_INITIAL 8192
/*
 * NTOHLL - ntohl for 64 bit values.
 */
#define NTOHLL(_x) be64toh(_x)

#ifdef HAVE_SYS_CAPABILITY_H
/*
 * List of required capabilities.
 */
static const cap_value_t required_capabilities[] = {
    CAP_SYS_ADMIN,
};
#endif /* HAVE_SYS_CAPABILITY_H */

/*
 * Run context for program.
 */
typedef struct nbd_context {
    char *   svc_progname;
    char *   svc_mount;
    char *   svc_mtype;
    char *   nbd_dev;
    int      nbd_fh;
    int      nbd_timeout;
    int      svc_fh;
    int      svc_verbose;
    int      svc_daemon_mode;
    int      svc_rdonly;
    int      svc_tolerant;
    int      svc_raw_available;
    uint64_t svc_blocksize;
    uint64_t svc_blockcount;
    uint64_t svc_offsetmask;
    uint64_t svc_blockmask;
    pid_t    svc_toreap;
} nbd_context_t;

/*
 * Initialize the interface to syslog (if required).
 */
static void
loginit(nbd_context_t *ncp) {
    if (ncp->svc_daemon_mode)
        openlog(ncp->svc_progname, LOG_PID, LOG_DAEMON);
}

/*
 * Generate a log message if the level threshold is met.  If running in
 * daemon mode, use syslog, otherwise log to stderr.
 */
static void
logmsg(nbd_context_t *ncp, int level, char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    if (ncp->svc_verbose >= level) {
        if (ncp->svc_daemon_mode) {
            vsyslog((level < 0) ? LOG_ERR
                                : ((level == 0) ? LOG_INFO : LOG_DEBUG),
                    fmt, ap);
        } else {
            vfprintf(stderr, fmt, ap);
        }
    }
    va_end(ap);
}

static const char pidfiletrailer[] = "pid";

/* simple strrchr */
static inline const char *
my_strrchr(const char *s, char c) {
    const char *xs;

    for (xs = &s[strlen(s) - 1]; (xs > s) && (*xs != c); xs--)
        ;
    if (*xs == c)
        xs++;
    return (xs);
}

/*
 * Create the pid file.
 */
static inline void
create_pid_file(nbd_context_t *ncp, pid_t cpid, char **pidfilenamep) {
    const char *pbasename = my_strrchr(ncp->svc_progname, '/');
    const char *dbasename = my_strrchr(ncp->nbd_dev, '/');
    size_t pfnamelen = strlen(pbasename) + strlen(dbasename) + strlen(RUNDIR) +
                       strlen(pidfiletrailer) + 4;
    if ((*pidfilenamep = (char *)malloc(pfnamelen))) {
        FILE *pidfp;
        sprintf(*pidfilenamep, "%s/%s.%s.%s", RUNDIR, pbasename, dbasename,
                pidfiletrailer);
        if ((pidfp = fopen(*pidfilenamep, "w"))) {
            fprintf(pidfp, "%d\n", cpid);
            fclose(pidfp);
        }
    }
}

/*
 * Remove the pid file.
 */
static inline void
remove_pid_file(char *pidfilename) {
    unlink(pidfilename);
    free(pidfilename);
}

/*
 * Open the nbd device.
 */
static inline int
nbdev_open(nbd_context_t *ncp) {
    int error = 0;
    if (ncp->nbd_fh < 0) {
        if ((ncp->nbd_fh = open(ncp->nbd_dev, O_RDWR)) < 0)
            error = errno;
    }
    return (error);
}

/*
 * nbd_connect	- Connect the nbd device.
 */
static int
nbd_connect(nbd_context_t *ncp, void *pctx) {
    int spair[2];
    int error = EINVAL;

    if (ncp && (ncp->svc_fh == -1)) {
        if ((error = socketpair(PF_UNIX, SOCK_STREAM, 0, spair)) == 0) {
            if ((error = nbdev_open(ncp)) == 0) {
                /*
                 * setup the nbd connection.
                 */
                ncp->svc_blocksize  = image_blocksize(pctx);
                ncp->svc_blockcount = image_blockcount(pctx);
                ncp->svc_offsetmask = ncp->svc_blocksize - 1;
                ncp->svc_blockmask  = ~ncp->svc_offsetmask;
                /*
                 * if requested, set NBD connection timeout - avoid slow NBD
                 * server disconnect
                 */
                if (ncp->nbd_timeout >= 0) {
                    if (ioctl(ncp->nbd_fh, NBD_SET_TIMEOUT, ncp->nbd_timeout) ==
                        -1) {
                        error = errno;
                        logmsg(ncp, 2,
                               "nbd_connect: ioctl NBD_SET_TIMEOUT fail with "
                               "%d (%s)\n",
                               error, strerror(error));
                    }
                    logmsg(ncp, 1, "NBD_TIMEOUT %d\n", ncp->nbd_timeout);
                }
                if ((ioctl(ncp->nbd_fh, NBD_CLEAR_SOCK) == -1) ||
                    (ioctl(ncp->nbd_fh, NBD_SET_SOCK, spair[0]) == -1) ||
                    (ioctl(ncp->nbd_fh, NBD_SET_BLKSIZE, ncp->svc_blocksize) ==
                     -1) ||
                    (ioctl(ncp->nbd_fh, NBD_SET_SIZE_BLOCKS,
                           ncp->svc_blockcount) == -1)) {
                    error = errno;
                    logmsg(ncp, 2,
                           "nbd_connect: ioctl chain fail with %d (%s)\n",
                           error, strerror(error));
                } else {
                    switch (fork()) {
                    case -1:
                        error = errno;
                        perror("fork");
                        break;

                    case 0:
                        close(spair[1]);
                        if (ioctl(ncp->nbd_fh, NBD_DO_IT) == -1) {
                            error = errno;
                            logmsg(ncp, 2,
                                   "nbd_connect: child: ioctl DOIT fail "
                                   "with %d (%s)\n",
                                   error, strerror(error));
                        }
                        logmsg(ncp, 2, "nbd_connect: child: DOIT done\n");
                        close(ncp->nbd_fh);
                        close(spair[0]);
                        exit(0);
                        break;
                    default:
                        /*
                         * spair[0] is connected to the nbd side of things.
                         */
                        close(spair[0]);
                        ncp->svc_fh = spair[1];
                        break;
                    }
                }
            } else {
                logmsg(ncp, 2, "nbd_connect: nbdev_open fail with %d (%s)\n",
                       error, strerror(error));
            }
        } else {
            error = errno;
            logmsg(ncp, 2, "nbd_connect: socketpair fail with %d (%s)\n", error,
                   strerror(error));
        }
    }
    if (error)
        logmsg(ncp, 1, "nbd_connect: fail with %d (%s)\n", error,
               strerror(error));
    return (error);
}

/*
 * Disconnect the nbd device.
 */
static void
nbd_disconnect(nbd_context_t *ncp, void *pctx) {
    if (ioctl(ncp->nbd_fh, NBD_DISCONNECT) == -1) {
        logmsg(ncp, 1, "nbd_disconnect: fail with %d (%s)\n", errno,
               strerror(errno));
    } else {
        logmsg(ncp, 2, "nbd_disconnect\n");
    }
}

/*
 * Signal handling.
 */

/*
 * Handle termination signals.  Set finish flag and let service request loop
 * finish up and potentially umount the filesystem.
 */
static int *finishflag = (int *)NULL;
void
nbd_finish(int sig, siginfo_t *si, void *vp) {
    if (finishflag)
        *finishflag = 1;
}

/*
 * Handle child termination.  Set the toreapp value to allow the service
 * request preamble to reap the particular child.
 */
static pid_t *toreapp = (pid_t *)NULL;
static int *  diedp   = (int *)NULL;
static void
nbd_reapchild(int sig, siginfo_t *si, void *vp) {
    if (si && si->si_pid && toreapp) {
        *toreapp = si->si_pid;
    }
    if (diedp) {
        (*diedp)++;
    }
}

/*
 * nbd_service_requests - the main work processing loop.  This is very
 * serial and very unrobust.
 */
static int
nbd_service_requests(nbd_context_t *ncp, void *pctx) {
    char *           readbuf      = (char *)malloc(READBUF_INITIAL);
    char *           pidfile      = (char *)NULL;
    size_t           readbuf_size = READBUF_INITIAL;
    int              error        = 0;
    volatile int     timetoleave  = 0;
    volatile int     someonedied  = 0;
    volatile pid_t   whodied      = 0;
    struct sigaction newsig, oldsig;

    /*
     * Prepare termination signal handlers.
     */
    finishflag = (int *)&timetoleave;
    memset(&newsig, 0, sizeof(newsig));
    newsig.sa_sigaction = nbd_finish;
    sigaction(SIGINT, &newsig, &oldsig);
    sigaction(SIGHUP, &newsig, &oldsig);
    sigaction(SIGTERM, &newsig, &oldsig);
    sigaction(SIGQUIT, &newsig, &oldsig);

    /*
     * Make sure we have a read buffer.
     */
    if (!readbuf) {
        timetoleave = 3;
        error       = ENOMEM;
    }

    /*
     * If we're setting up a mount, then for a child to do the mount and
     * prepare the signal handler to be notified when the child completes.
     */
    if (ncp->svc_mount) {
        pid_t cpid;

        newsig.sa_sigaction = nbd_reapchild;
        newsig.sa_flags     = SA_RESETHAND | SA_SIGINFO;
        diedp               = (int *)&someonedied;
        toreapp             = (pid_t *)&whodied;
        sigaction(SIGCHLD, &newsig, &oldsig);
        switch ((cpid = fork())) {
        case 0:
            exit(mount(ncp->nbd_dev, ncp->svc_mount,
                       (ncp->svc_mtype) ? ncp->svc_mtype : "ext2",
                       (ncp->svc_rdonly) ? MS_RDONLY : 0, ""));
            break;
        case -1:
            error = errno;
            logmsg(ncp, -1, "%s: cannot fork to mount: %s\n", ncp->svc_progname,
                   strerror(error));
            /*
             * We ignore this failure.  Someone else can try the mount.
             */
            break;
        default:
            ncp->svc_toreap = cpid;
            break;
        }
    }

    /*
     * create pid file
     */
    create_pid_file(ncp, getpid(), &pidfile);

    if (ncp->svc_mount) {
        logmsg(ncp, 0, "Mount \"%s\" is ready.\n", ncp->svc_mount);
        logmsg(ncp, 0,
               "When finished with the imagemount, run `umount %s` then "
               "`nbd-client -d %s to cleanup.\n",
               ncp->svc_mount, ncp->nbd_dev);
    } else if (ncp->nbd_dev) {
        logmsg(ncp, 0, "Device \"%s\" is now ready for further operations.\n",
               ncp->nbd_dev);
        logmsg(ncp, 0,
               "When finished with the imagemount, run `nbd-client -d %s` to "
               "disconnect from the NBD server.\n",
               ncp->nbd_dev);
    }
    /*
     * Do work until we're completely done.
     */
    while (timetoleave < 3) {
        struct nbd_request request;
        size_t             rlength;

        /*
         * If signalled that someone died, reap the child to avoid zombies.
         */
        if (someonedied) {
            int   existat;
            pid_t corpse;

            if (whodied) {
                corpse = waitpid(whodied, &existat, WNOHANG);
            } else {
                corpse = wait(&existat);
            }
            someonedied = 0;
            whodied     = 0;
            logmsg(ncp, 2, "%s: pid %d finished with %d\n", ncp->svc_progname,
                   corpse, existat);
            if (ncp->svc_toreap == corpse)
                ncp->svc_toreap = 0;
        }

        /*
         * Read a request.
         */
        if ((rlength = read(ncp->svc_fh, &request, sizeof(request))) ==
            sizeof(request)) {
            struct nbd_reply reply;
            char *           replyappend = (char *)NULL;
            /*
             * Calculate the start/end blocks.
             */
            off_t    offset         = NTOHLL(request.from);
            size_t   length         = ntohl(request.len);
            uint64_t startblockoffs = offset & ncp->svc_blockmask;
            uint64_t sboffs         = offset & ncp->svc_offsetmask;
            uint64_t endblockoffs = (offset + length - 1) & ncp->svc_blockmask;
            uint64_t eboffs       = (offset + length - 1) & ncp->svc_offsetmask;
            uint64_t startblock   = startblockoffs / ncp->svc_blocksize;
            uint64_t blockcount =
                (endblockoffs - startblockoffs) / ncp->svc_blocksize;
            uint64_t req_readbuf;

            /*
             * Adjust block count.
             */
            if (length)
                blockcount++;

            /*
             * Calculate the required read buffer and adjust if necessary.
             */
            if (endblockoffs > startblockoffs) {
                req_readbuf = blockcount * ncp->svc_blocksize;
                while (req_readbuf > readbuf_size) {
                    char *nrbuf = malloc(req_readbuf);

                    if (nrbuf) {
                        readbuf_size = req_readbuf;
                        free(readbuf);
                        readbuf = nrbuf;
                    } else {
                        if (req_readbuf < 0x80000000UL) {
                            logmsg(
                                ncp, 0,
                                "[%s] retrying allocation of %d byte buffer\n",
                                ncp->svc_progname, req_readbuf);
                            sleep(10);
                        } else {
                            logmsg(ncp, 0,
                                   "[%s] not retrying allocation of %d byte "
                                   "buffer\n",
                                   ncp->svc_progname, req_readbuf);
                            timetoleave = 1;
                            error       = ENOMEM;
                            break;
                        }
                    }
                }
            }

            /*
             * Verify that the message was correctly formed.
             */
            if (request.magic == htonl(NBD_REQUEST_MAGIC)) {
                switch (ntohl(request.type)) {
                case NBD_CMD_DISC:
                    logmsg(ncp, 1, "NBD_SHUTDOWN\n");
                    timetoleave = 1;
                    break;
                case NBD_CMD_WRITE:
                    logmsg(ncp, 1, "NBD_WRITE0x%x@0x%x\n", length, offset);
                    if (sboffs) {
                        /* partial leading block write, ech. */
                        if (!(error = image_seek(pctx, startblock))) {
                            error = image_readblocks(pctx, readbuf, 1);
                        }
                    } else {
                        error = 0;
                    }
                    if (!error) {
                        if ((eboffs != ncp->svc_offsetmask) &&
                            (blockcount != 1)) {
                            /* partial trailing block write, double ech. */
                            if (!(error = image_seek(
                                      pctx, startblock + blockcount - 1))) {
                                error = image_readblocks(
                                    pctx,
                                    readbuf +
                                        ((blockcount - 1) * ncp->svc_blocksize),
                                    1);
                            }
                        }
                        if (!error) {
                            char *   rembp  = readbuf;
                            uint64_t remlen = length;
                            for ((rembp = readbuf, remlen = length);
                                 !error && remlen;) {
                                /*
                                 * Retry the read if we're interrupted but not
                                 * if it's time to leave.
                                 */
                                while (((rlength = read(ncp->svc_fh, rembp,
                                                        remlen)) == -1) &&
                                       (errno == EINTR) && (!timetoleave))
                                    ;
                                if (rlength == remlen) {
                                    if (!(error =
                                              image_seek(pctx, startblock)) &&
                                        !(error = image_writeblocks(
                                              pctx, readbuf, blockcount))) {
                                        logmsg(
                                            ncp, 2,
                                            "NBD_WRITE image write success\n");
                                    } else {
                                        logmsg(
                                            ncp, 1,
                                            "NBD_WRITE: write fail %d (%s)\n",
                                            error, strerror(error));
                                    }
                                } else {
                                    if (rlength >= 0) {
                                        logmsg(ncp, 1,
                                               "NBD_WRITE fail: short read of "
                                               "data\n");
                                    } else {
                                        error = errno;
                                        logmsg(ncp, 1,
                                               "NBD_WRITE fail: read fail %d "
                                               "(%s)\n",
                                               error, strerror(error));
                                    }
                                }
                                remlen -= rlength;
                                rembp += rlength;
                            }
                        } else {
                            logmsg(ncp, 1,
                                   "NBD_WRITE: priming read fail %d (%s)\n",
                                   error, strerror(error));
                        }
                    } else {
                        logmsg(ncp, 1,
                               "NBD_WRITE: lead priming read fail %d (%s)\n",
                               error, strerror(error));
                    }
                    break;
                case NBD_CMD_READ:
                    logmsg(ncp, 1, "NBD_READ 0x%x@0x%x\n", length, offset);
                    if (!(error = image_seek(pctx, startblock)) &&
                        !(error =
                              image_readblocks(pctx, readbuf, blockcount))) {
                        logmsg(ncp, 2, "NBD_READ image read success\n");
                        replyappend = &readbuf[sboffs];
                    } else {
                        logmsg(ncp, 2, "NBD_READ image read fail %d (%s)\n",
                               error, strerror(error));
                    }
                    break;
                default:
                    error = EINVAL;
                    break;
                }

                reply.magic = htonl(NBD_REPLY_MAGIC);
                reply.error = error;
                memcpy(reply.handle, request.handle, 8);

                /*
                 * Retry the write if we're interrupted but if it's not time
                 * to leave.
                 */
                while (((rlength = write(ncp->svc_fh, &reply, sizeof(reply))) ==
                        -1) &&
                       (errno == EINTR) && (!timetoleave))
                    ;
                if (rlength == sizeof(reply)) {
                    /*
                     * Write the reply appendage if it's a read and there
                     * was no error.
                     */
                    if ((request.type == htonl(NBD_CMD_READ)) &&
                        (reply.error == 0)) {
                        /*
                         * Write the data reply.
                         */
                        while (((rlength = write(ncp->svc_fh, replyappend,
                                                 length)) == -1) &&
                               (errno == EINTR) && (!timetoleave))
                            ;
                        if (rlength == length) {
                            error = 0;
                        } else {
                            if (rlength == -1) {
                                error = errno;
                                logmsg(ncp, 0,
                                       "[%s] reply addendum write error: %s\n",
                                       ncp->svc_progname, strerror(error));
                            }
                        }
                    }
                } else {
                    if ((rlength == -1) && (errno != EINTR)) {
                        error = errno;
                        logmsg(ncp, 0, "[%s] reply write error: %s\n",
                               ncp->svc_progname, strerror(error));
                    }
                }
            } else {
                logmsg(ncp, 1, "[%s] Bad message from kernel: %08x\n",
                       ncp->svc_progname, request.magic);
                /*
                 * It's unclear what to do here.  We've obviously lost sync
                 * with the kernel.  Will reading 32-bit quantities until we
                 * get back in sync work?  I dunno.  Without the following
                 * snippet, we read nbd_requests until we (hopefully) get
                 * back in sync.  The question then is if the kernel is
                 * waiting for a response, then we're completely hosed.
                 */
#ifdef POTENTIAL_DISASTER
                timetoleave = 1;
                error       = EIO;
#endif /* POTENTIAL_DISASTER */
            }
        } else {
            if ((rlength == -1) && (errno != EINTR)) {
                error = errno;
                logmsg(ncp, 0, "[%s] kernel command read error: %s\n",
                       ncp->svc_progname, strerror(error));
            }
        }

        /*
         * Check to see if we received a termination signal.
         */
        if (timetoleave == 1) {
            /*
             * Getting ready to punt.
             */
            if (ncp->svc_mount) {
                pid_t cpid;

                newsig.sa_sigaction = nbd_reapchild;
                newsig.sa_flags     = SA_RESETHAND | SA_SIGINFO;
                diedp               = (int *)&timetoleave;
                toreapp             = (pid_t *)&whodied;
                sigaction(SIGCHLD, &newsig, &oldsig);
                timetoleave = 2;
                switch ((cpid = fork())) {
                case 0:
                    /*
                     * If we had an error, try to force the unmount.
                     */
                    if (error) {
                        exit(umount2(ncp->svc_mount, MNT_FORCE));
                    } else {
                        exit(umount(ncp->svc_mount));
                    }
                    break;
                case -1:
                    error = errno;
                    logmsg(ncp, -1, "%s: cannot fork to unmount: %s\n",
                           ncp->svc_progname, strerror(error));
                    timetoleave = 3; /* Just punt. */
                    break;
                default:
                    ncp->svc_toreap = cpid;
                    break;
                }
            } else {
                timetoleave = 3;
            }
        }
    }
    if (ncp->svc_toreap) {
        int   existat;
        pid_t corpse;

        if (whodied) {
            corpse = waitpid(whodied, &existat, WNOHANG);
        } else {
            corpse = wait(&existat);
        }
        logmsg(ncp, 2, "%s: pid %d finished with %d\n", ncp->svc_progname,
               corpse, existat);
    }

    /*
     * Remove the pid file.
     */
    remove_pid_file(pidfile);
    return (error);
}

/*
 * See if we have the required capabilities.
 */
static int
nbd_check_capabilities(nbd_context_t *ncp, void *pctx) {
    int error = 0;
#ifdef HAVE_SYS_CAPABILITY_H
    cap_t effset = cap_get_proc();

    if (effset) {
        cap_flag_value_t fisset;
        int              i;

        for (i = 0; i < (sizeof(required_capabilities) /
                         sizeof(required_capabilities[0]));
             i++) {
            if (!(error = cap_get_flag(effset, required_capabilities[i],
                                       CAP_EFFECTIVE, &fisset))) {
                if (fisset != CAP_SET) {
                    error = EPERM;
                    break;
                }
            }
        }
        cap_free(effset);
    } else {
        error = errno;
    }
#endif /* HAVE_SYS_CAPABILITY_H */
    return (error);
}

/*
 * Enter daemon mode if requested.
 */
static int
nbd_daemon_mode(nbd_context_t *ncp, void *pctx) {
    int error = 0;

    if (ncp->svc_daemon_mode) {
        logmsg(ncp, 0,
               "Daemonizing process. Further log output will be written to the "
               "system log.\n");
        if (daemon(0, 1) < 0) {
            error = errno;
            logmsg(ncp, -1, "%s: daemon failed: %s\n", ncp->svc_progname,
                   strerror(error));
        } else {
            logmsg(ncp, 0, "%s: Successfully daemonized.", ncp->svc_progname);
        }
    }
    return (error);
}

/*
 * The main routine.
 */
int
main(int argc, char *argv[]) {
    int           option;
    extern char * optarg;
    char *        file  = (char *)NULL;
    char *        cfile = (char *)NULL;
    int           error = 0;
    nbd_context_t nc;

    memset(&nc, 0, sizeof(nc));
    nc.nbd_fh          = -1;
    nc.nbd_timeout     = -1;
    nc.svc_fh          = -1;
    nc.svc_daemon_mode = 1;

    /*
     * Parse options.
     */
    while ((option = getopt(argc, argv, "c:d:f:v:i:m:t:DrwTR")) != -1) {
        switch (option) {
        case 'c':
            cfile = optarg;
            break;
        case 'd':
            nc.nbd_dev = optarg;
            break;
        case 'f':
            file = optarg;
            break;
        case 'v':
            sscanf(optarg, "%d", &nc.svc_verbose);
            break;
        case 'i':
            sscanf(optarg, "%d", &nc.nbd_timeout);
            break;
        case 'm':
            nc.svc_mount = optarg;
            break;
        case 't':
            nc.svc_mtype = optarg;
            break;
        case 'D':
            nc.svc_daemon_mode = !nc.svc_daemon_mode;
            break;
        case 'r':
            nc.svc_rdonly = 1;
            break;
        case 'w':
            nc.svc_rdonly = 0;
            break;
        case 'T':
            nc.svc_tolerant = !nc.svc_tolerant;
            break;
        case 'R':
            nc.svc_raw_available = !nc.svc_raw_available;
            break;
        default:
            error = 1;
            break;
        }
    }
    if (nc.svc_daemon_mode) {
        printf("Launched in daemon mode: All logging output being written to "
               "the system log.\n");
    }

    /*
     * If successful, then do it!.
     */
    if (!error && nc.nbd_dev && file) {
        void *pctx = (void *)NULL;
        /*
         * Open the image.
         */
        if (!(error =
                  image_open(file, cfile,
                             (nc.svc_rdonly) ? SYSDEP_OPEN_RO : SYSDEP_OPEN_RW,
                             &posix_dispatch, nc.svc_raw_available, &pctx))) {
            logmsg(&nc, 0, "Preparing \"%s\"...\n", file);
            /*
             * Set tolerant mode (if specified).
             */
            if (nc.svc_tolerant) {
                image_tolerant_mode(pctx);
            }
            /*
             * Verify the image.
             */
            if (!(error = image_verify(pctx))) {
                nc.svc_progname = argv[0];
                /*
                 * Initialize the logger and check capabilities.
                 */
                loginit(&nc);
                if (!(error = nbd_check_capabilities(&nc, pctx))) {
                    /*
                     * Enter daemon mode - no more stderr messages if so.
                     */
                    if (!(error = nbd_daemon_mode(&nc, pctx))) {
                        /*
                         * Connect to the nbd device.
                         */
                        if (!(error = nbd_connect(&nc, pctx))) {
                            /*
                             * Process requests.
                             */
                            error = nbd_service_requests(&nc, pctx);
                            if (error) {
                                if (error != EINTR) {
                                    logmsg(&nc, 0, "%s: complete: %s\n",
                                           argv[0], strerror(error));
                                } else {
                                    logmsg(&nc, 0, "%s: completing.\n",
                                           argv[0]);
                                }
                            }
                            /*
                             * Disconnect from the nbd device.
                             */
                            nbd_disconnect(&nc, pctx);
                        } else {
                            logmsg(&nc, -1, "%s: cannot connect: %s\n",
                                   nc.nbd_dev, strerror(error));
                            logmsg(&nc, 0,
                                   "To disconnect a previous NBD device from "
                                   "the NBD server run: `nbd-client -d %s`\n",
                                   nc.nbd_dev);
                        }
                    } else {
                        fprintf(stderr, "%s: cannot connect: %s\n", nc.nbd_dev,
                                strerror(error));
                    }
                } else {
                    fprintf(stderr, "%s: not capable: %s\n", argv[0],
                            strerror(error));
                }
            } else {
                fprintf(stderr, "%s: cannot verify: %s\n", file,
                        strerror(error));
            }
            image_close(pctx);
        } else {
            fprintf(stderr, "%s: cannot open: %s\n", file, strerror(error));
        }
    } else {
        fprintf(stderr,
                "%s: usage %s -d disk -f file [-c cfile] "
                "[-m mount [-t type]] [-i timeout] [-v verbose] [-Drw]\n",
                argv[0], argv[0]);
    }
    return (error);
}

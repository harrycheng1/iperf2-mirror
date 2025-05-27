/*---------------------------------------------------------------
 * Copyright (c) 1999,2000,2001,2002,2003
 * The Board of Trustees of the University of Illinois
 * All Rights Reserved.
 *---------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software (Iperf) and associated
 * documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and
 * the following disclaimers.
 *
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimers in the documentation and/or other materials
 * provided with the distribution.
 *
 *
 * Neither the names of the University of Illinois, NCSA,
 * nor the names of its contributors may be used to endorse
 * or promote products derived from this Software without
 * specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ________________________________________________________________
 * National Laboratory for Applied Network Research
 * National Center for Supercomputing Applications
 * University of Illinois at Urbana-Champaign
 * http://www.ncsa.uiuc.edu
 * ________________________________________________________________
 *
 * socket.c
 * by Mark Gates <mgates@nlanr.net>
 * -------------------------------------------------------------------
 * set/getsockopt and read/write wrappers
 * ------------------------------------------------------------------- */

#include "headers.h"
#include "util.h"
#if HAVE_DECL_SO_TXTIME
#include <linux/net_tstamp.h>
#include <linux/errqueue.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------
 * Attempts to reads n bytes from a socket.
 * Returns number actually read, or -1 on error.
 * If number read < inLen then we reached EOF.
 *
 * from Stevens, 1998, section 3.9
 * ------------------------------------------------------------------- */
ssize_t readn (int inSock, void *outBuf, size_t inLen) {
    size_t  nleft;
    ssize_t nread;
    char *ptr;

    assert(inSock >= 0);
    assert(outBuf != NULL);
    assert(inLen > 0);

    ptr   = (char*) outBuf;
    nleft = inLen;

    while (nleft > 0) {
        nread = read(inSock, ptr, nleft);
        if (nread < 0) {
            if (errno == EINTR)
                nread = 0;  /* interupted, call read again */
            else
                return SOCKET_ERROR;  /* error */
        } else if (nread == 0)
            break;        /* EOF */

        nleft -= nread;
        ptr   += nread;
    }

    return(inLen - nleft);
} /* end readn */

/* -------------------------------------------------------------------
 * Similar to read but supports recv flags
 * Returns number actually read, or -1 on error.
 * If number read < inLen then we reached EOF.
 * from Stevens, 1998, section 3.9
 * ------------------------------------------------------------------- */
int recvn (int inSock, char *outBuf, int inLen, int flags) {
    int  nleft;
    int nread = 0;
    char *ptr;

    assert(inSock >= 0);
    assert(outBuf != NULL);
    assert(inLen > 0);

    ptr   = outBuf;
    nleft = inLen;
#if (HAVE_DECL_MSG_PEEK)
    if (flags & MSG_PEEK) {
	while ((nleft != nread) && !sInterupted) {
	    nread = recv(inSock, ptr, nleft, flags);
	    switch (nread) {
	    case SOCKET_ERROR :
		// Note: use TCP fatal error codes even for UDP
		if (FATALTCPREADERR(errno)) {
		    WARN_errno(1, "recvn peek");
		    nread = SOCKET_ERROR;
		    sInterupted = 1;
		    goto DONE;
		}
#ifdef HAVE_THREAD_DEBUG
		WARN_errno(1, "recvn peek non-fatal");
#endif
		break;
	    case 0:
		WARN(1, "recvn peek checking connection status");

#ifdef MSG_DONTWAIT
		    // Distinguish between no data available vs connection closed
		    {
			char test_byte;
			int test_recv = recv(inSock, &test_byte, 1, MSG_DONTWAIT);
			if (test_recv == 0) {
			    // Definitely closed - peer performed orderly shutdown
#ifdef HAVE_THREAD_DEBUG
			    WARN(1, "recvn peek peer close confirmed");
#endif
			    goto DONE;
			} else if (test_recv < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
			    // Just no data available, connection still open, continue waiting for data
			    break;
			}
			// For other errors, fall through to peer close
		    }
#else
		goto DONE;
#endif
		break;
	    default :
		break;
	    }
	}
    } else
#endif
	{
	    while ((nleft > 0) && !sInterupted) {
#if (HAVE_DECL_MSG_WAITALL)
		nread = recv(inSock, ptr, nleft, MSG_WAITALL);
#else
		nread = recv(inSock, ptr, nleft, 0);
#endif
		switch (nread) {
		case SOCKET_ERROR :
		    // Note: use TCP fatal error codes even for UDP
		    if (FATALTCPREADERR(errno)) {
			WARN_errno(1, "recvn");
			nread = SOCKET_ERROR;
			sInterupted = 1;
			goto DONE;
		    } else {
			nread = IPERF_SOCKET_ERROR_NONFATAL;
			goto DONE;
		    }
#ifdef HAVE_THREAD_DEBUG
		    WARN_errno(1, "recvn non-fatal");
#endif
		    break;
		case 0:
#ifdef HAVE_THREAD_DEBUG
		    WARN(1, "recvn peer close");
#endif
		    nread = inLen - nleft;
		    goto DONE;
		    break;
		default :
		    nleft -= nread;
		    ptr   += nread;
		    break;
		}
		nread = inLen - nleft;
	    }
	}
  DONE:
    return(nread);
} /* end recvn */

/* -------------------------------------------------------------------
 * Attempts to write  n bytes to a socket.
 * returns number actually written, or -1 on error.
 * number written is always inLen if there is not an error.
 *
 * from Stevens, 1998, section 3.9
 * ------------------------------------------------------------------- */

int writen (int inSock, const void *inBuf, int inLen, int *count) {
    int nleft;
    int nwritten;
    const char *ptr;

    assert(inSock >= 0);
    assert(inBuf != NULL);
    assert(inLen > 0);
    assert(count != NULL);

    ptr   = (char*) inBuf;
    nleft = inLen;
    nwritten = 0;

    while ((nleft > 0) && !sInterupted) {
        nwritten = write(inSock, ptr, nleft);
	(*count)++;
	switch (nwritten) {
	case SOCKET_ERROR :
	    // check for a fatal error vs an error that should retry
	    if ((errno != EINTR) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
		nwritten = inLen - nleft;
		fprintf(stdout, "FAIL: writen errno = %d (bytes=%d)\n", errno, nwritten);
//		sInterupted = 1;
		goto DONE;
	    }
	    break;
	case 0:
	    // write timeout - retry
	    break;
	default :
	    nleft -= nwritten;
	    ptr   += nwritten;
	    break;
	}
	nwritten = inLen - nleft;
    }
  DONE:
    return (nwritten);
} /* end writen */

/* -------------------------------------------------------------------
 * Write data with scheduled transmit time and/or IP TOS using control msgs
 * Returns number of bytes written, or -1 on error.
 * delay_ns: nanoseconds from now when packet should be transmitted (0 = no delay)
 * tos_value: IP Type of Service value (0-255, or -1 to skip TOS setting)
 * ------------------------------------------------------------------- */
#if HAVE_DECL_SO_TXTIME
int writemsg_delay_tos(int inSock, const void *inBuf, int inLen, uint64_t delay_ns, int tos_value) {
    struct msghdr msg;
    struct iovec iov;
    char control[CMSG_SPACE(sizeof(uint64_t)) + CMSG_SPACE(sizeof(int))];
    struct cmsghdr *cmsg;
    struct timespec now;
    uint64_t txtime;
    int result;

    assert(inSock >= 0);
    assert(inBuf != NULL);
    assert(inLen > 0);
    assert(tos_value >= -1 && tos_value <= 255);

    /* Set up iovec */
    iov.iov_base = (void*)inBuf;
    iov.iov_len = inLen;

    /* Set up message header */
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control); /* Set to full buffer size for CMSG_NXTHDR */

    /* Initialize control buffer */
    memset(control, 0, sizeof(control));
    cmsg = CMSG_FIRSTHDR(&msg);

    /* Add SO_TXTIME control message if delay requested */
    if (delay_ns > 0) {
        /* Get current time */
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
	    WARN_errno(1, "writemsg_delay clock_gettime failed");
	    return -1;
	}

        /* Calculate transmit time */
        txtime = (uint64_t)now.tv_sec * 1000000000ULL + now.tv_nsec + delay_ns;

        if (cmsg == NULL) {
            WARN(1, "writemsg_delay CMSG_FIRSTHDR failed for TXTIME");
            return -1;
        }

        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_TXTIME;
        cmsg->cmsg_len = CMSG_LEN(sizeof(uint64_t));
        *((uint64_t*)CMSG_DATA(cmsg)) = txtime;
        cmsg = CMSG_NXTHDR(&msg, cmsg);
    }
    /* Add IP_TOS control message if TOS value specified */
    if (tos_value >= 0) {
	if (cmsg == NULL) {
	    WARN(1, "writemsg_delay insufficient control buffer space for TOS");
	    return -1;
	}

	cmsg->cmsg_level = IPPROTO_IP;
	cmsg->cmsg_type = IP_TOS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	*((int*)CMSG_DATA(cmsg)) = tos_value;
	cmsg = CMSG_NXTHDR(&msg, cmsg);
    }
    /* Calculate actual control length used */
    if (cmsg != NULL) {
        msg.msg_controllen = (char*)cmsg - (char*)control;
    } else {
        /* Calculate based on what we added */
        msg.msg_controllen = 0;
        if (delay_ns > 0) msg.msg_controllen += CMSG_SPACE(sizeof(uint64_t));
        if (tos_value >= 0) msg.msg_controllen += CMSG_SPACE(sizeof(int));
    }

    /* Send the message */
    result = sendmsg(inSock, &msg, 0);
    if (result < 0) {
	if (errno == EINVAL) {
	    WARN(1, "writemsg_delay: control message not configured on socket");
	} else if (errno == ENOTSUP) {
	    WARN(1, "writemsg_delay: control message not supported by kernel/driver");
	} else if (errno == EPERM) {
	    WARN(1, "writemsg_delay: permission denied (may need CAP_NET_ADMIN for some options)");
	} else {
	    WARN_errno(1, "writemsg_delay sendmsg failed");
	}
	return -1;
    }
    return result;
} /* end writemsg_delay */
#else
/* Stub implementation for non-Linux systems */
int writemsg_delay_tos(int inSock, const void *inBuf, int inLen, uint64_t delay_ns, int tos_value) {
    WARN(1, "writemsg_delay: control messages not supported on this platform");
    /* Fall back to regular write */
    return write(inSock, inBuf, inLen);
}
#endif

/* -------------------------------------------------------------------
 * Convenience wrapper for writemsg_delay with only TOS (no delay)
 * Returns number of bytes written, or -1 on error.
 * tos_value: IP Type of Service value (0-255)
 * ------------------------------------------------------------------- */
int writemsg_tos(int inSock, const void *inBuf, int inLen, int tos_value) {
    return writemsg_delay_tos(inSock, inBuf, inLen, 0, tos_value);
}
int writemsg_delay(int inSock, const void *inBuf, int inLen, uint64_t delay_ns) {
    return writemsg_delay_tos(inSock, inBuf, inLen, delay_ns, -1);
}

#ifdef __cplusplus
} /* end extern "C" */
#endif

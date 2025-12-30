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
 * Socket.cpp
 * by       Ajay Tirumala <tirumala@ncsa.uiuc.edu>
 * and      Mark Gates <mgates@nlanr.net>
 * ------------------------------------------------------------------- */

#define HEADERS()

#include "headers.h"

#include "SocketAddr.h"
#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int SockAddr_getAFdomain(iperf_sockaddr *inSockAddr) {
    return (SockAddr_isIPv6(inSockAddr) ?
#if HAVE_IPV6
                                        AF_INET6
#else
                                        AF_INET
#endif
                                        : AF_INET);
}

/* -------------------------------------------------------------------
 * Create a socket address. If inHostname is not null, resolve that
 * address and fill it in. Fill in the port number. Use IPv6 ADDR_ANY
 * if that is what is desired.
 * ------------------------------------------------------------------- */
void SockAddr_remoteAddr(struct thread_Settings *inSettings) {
    if (SockAddr_isZeroAddress(&inSettings->peer) == 0) {
        if (inSettings->mHost != NULL) {
            SockAddr_setHostname(inSettings->mHost, &inSettings->peer, &inSettings->size_peer,
                                 isIPV6(inSettings));
            if (inSettings->incrdstip)
                SockAddr_incrAddress(&inSettings->peer, inSettings->incrdstip);
        } else {
#if HAVE_IPV6
            if (isIPV6(inSettings)) {
                ((struct sockaddr *)&inSettings->peer)->sa_family = AF_INET6;
            } else {
                ((struct sockaddr *)&inSettings->peer)->sa_family = AF_INET;
            }
        }
        if (SockAddr_isIPv6(&inSettings->peer)) {
            inSettings->size_peer = sizeof(struct sockaddr_in6);
        } else {
            inSettings->size_peer = sizeof(struct sockaddr_in);
        }
#else
        }
        ((struct sockaddr *)&inSettings->peer)->sa_family = AF_INET;
        inSettings->size_peer = sizeof(struct sockaddr_in);
#endif
        SockAddr_setPort(&inSettings->peer, inSettings->mPort);
    }
}
// end SocketAddr

void SockAddr_localAddr(struct thread_Settings *inSettings) {
    SockAddr_zeroAddress(&inSettings->local);

    if (inSettings->mLocalhost != NULL) {
        SockAddr_setHostname(inSettings->mLocalhost, &inSettings->local, &inSettings->size_local,
                             isIPV6(inSettings));
        if (inSettings->incrsrcip) SockAddr_incrAddress(&inSettings->local, inSettings->incrsrcip);
    } else {
#if HAVE_IPV6
        if (isIPV6(inSettings)) {
            ((struct sockaddr *)&inSettings->local)->sa_family = AF_INET6;
        } else {
            ((struct sockaddr *)&inSettings->local)->sa_family = AF_INET;
        }
    }

    if (SockAddr_isIPv6(&inSettings->local)) {
        inSettings->size_local = sizeof(struct sockaddr_in6);
    } else {
        inSettings->size_local = sizeof(struct sockaddr_in);
    }
#else
        ((struct sockaddr *)&inSettings->local)->sa_family = AF_INET;
    }
    inSettings->size_local = sizeof(struct sockaddr_in);
#endif
    /*
     *  This section handles the *local* port binding (which is messy)
     *  Quintuple is Proto:LocalIP:LocalPort:DstIP:DstPort
     *
     *  There are three threads being Client, Listener and Server
     *  mPort comes from the -p command (which defaults to 5001)
     *  mLocalhost indicates -B set requesting a local binding
     *  mBindPort comes from -B IP:<port> (where port defaults to 0)
     *  Multicast IP address, e.g. 239.1.1.1, is set per a -B
     *  Zero will cause the OS to auto assign a LocalPort
     *  For iperf -s; Windows uses listener thread, *nix a server thread
     *  (so, effectively, Listener and Server threads are the same)
     *  Client threads support either auto assignment (default) or
     *  user specified (via -B)
     */
    if (inSettings->mLocalhost == NULL) {
        if (inSettings->mThreadMode == kMode_Client) {
            /*
             * Client thread, -p and no -B,
             * OS will auto assign a free local port
             */
            SockAddr_setPortAny(&inSettings->local);
        } else {
            /* Server or Listener thread, -p and no -B */
            SockAddr_setPort(&inSettings->local, inSettings->mPort);
        }
    } else {
        // -B was set
        if (inSettings->mThreadMode == kMode_Client) {
            /* Client thread */
            if (inSettings->mBindPort) {
                /*
                 * User specified port so use it
                 */
                SockAddr_setPort(&inSettings->local,
                                 (inSettings->mBindPort + inSettings->incrsrcport));
            } else {
                /*
                 * No user specified port, let OS assign a free one
                 */
                SockAddr_setPortAny(&inSettings->local);
            }
        } else {
            /*
             * Server or Listener thread, both always use -p port
             * any -B port will be ignored
             */
            SockAddr_setPort(&inSettings->local, inSettings->mPort);
        }
    }
}

// end SocketAddr

/* -------------------------------------------------------------------
 * Resolve the hostname address and fill it in.
 * ------------------------------------------------------------------- */
void SockAddr_setHostname(const char *inHostname, iperf_sockaddr *inSockAddr, Socklen_t *addr_size,
                          int isIPv6) {
    // ..I think this works for both ipv6 & ipv4... we'll see
    bool found = false;
    int ret_ga;
    struct addrinfo *res = NULL, *itr;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    if (!isIPv6) {
        hints.ai_family = AF_INET;
        ret_ga = getaddrinfo(inHostname, NULL, &hints, &res);
        if (ret_ga == 0) {
            if (res && res->ai_addr) {
                itr = res;
                // Now search for a IPv4 Address
                while (itr != NULL) {
                    if (itr->ai_family == AF_INET) {
                        memcpy(inSockAddr, (itr->ai_addr), (itr->ai_addrlen));
                        *addr_size = (Socklen_t)sizeof(struct sockaddr_in);
                        freeaddrinfo(res);
                        found = true;
                        break;
                    } else {
                        itr = itr->ai_next;
                    }
                }
            }
        }
    }
#if HAVE_IPV6
    if (!found) {
        hints.ai_family = AF_INET6;
        ret_ga = getaddrinfo(inHostname, NULL, &hints, &res);
        if (ret_ga == 0) {
            if (res && res->ai_addr) {
                // Now search for a IPv6 Address
                itr = res;
                while (itr != NULL) {
                    if (itr->ai_family == AF_INET6) {
                        memcpy(inSockAddr, (itr->ai_addr), (itr->ai_addrlen));
                        freeaddrinfo(res);
                        *addr_size = (Socklen_t)sizeof(struct sockaddr_in6);
                        found = true;
                        break;
                    } else {
                        itr = itr->ai_next;
                    }
                }
            }
        }
    }
#endif  // IPV6
    // getaddrinfo didn't find an address, fallback to gethostbyname for v4
    if (!found && !isIPv6) {
        // first try just converting dotted decimal
        // on Windows gethostbyname doesn't understand dotted decimal
        struct sockaddr_in *sockaddr = (struct sockaddr_in *)inSockAddr;
        int rc = inet_pton(AF_INET, inHostname, &sockaddr->sin_addr);
        sockaddr->sin_family = AF_INET;
        if (rc == 0) {
            struct hostent *hostP = gethostbyname(inHostname);
            if (hostP == NULL) {
                /* this is the same as herror() but works on more systems */
                const char *format;
                switch (h_errno) {
                    case HOST_NOT_FOUND:
                        format = "%s: Unknown host\n";
                        break;
                    case NO_ADDRESS:
                        format = "%s: No address associated with name\n";
                        break;
                    case NO_RECOVERY:
                        format = "%s: Unknown server error\n";
                        break;
                    case TRY_AGAIN:
                        format = "%s: Host name lookup failure\n";
                        break;
                    default:
                        format = "%s: Unknown resolver error\n";
                        break;
                }
                fprintf(stderr, format, inHostname);
                exit(1);
                return;  // TODO throw
            }
            found = true;
            memcpy(&sockaddr->sin_addr, *(hostP->h_addr_list), (hostP->h_length));
        }
    }
    if (!found) {
        if (!isIPv6) {
            fprintf(stderr, "ERROR: failed to find an ip address for host '%s'\n", inHostname);
        } else {
            fprintf(stderr, "ERROR: failed to find an ipv6 address for host '%s'\n", inHostname);
        }
        exit(1);
    }
}
// end setHostname

/* -------------------------------------------------------------------
 * Copy the IP address into the string.
 * ------------------------------------------------------------------- */
void SockAddr_getHostAddress(iperf_sockaddr *inSockAddr, char *outAddress, size_t len) {
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET) {
        inet_ntop(AF_INET, &(((struct sockaddr_in *)inSockAddr)->sin_addr), outAddress, len);
    }
#if HAVE_IPV6
    else {
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)inSockAddr)->sin6_addr), outAddress, len);
    }
#endif
}
// end getHostAddress

/* -------------------------------------------------------------------
 * Set the address to any (generally all zeros).
 * ------------------------------------------------------------------- */
void SockAddr_setAddressAny(iperf_sockaddr *inSockAddr) {
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET)
        memset(&(((struct sockaddr_in *)inSockAddr)->sin_addr), 0, sizeof(struct in_addr));
#if HAVE_IPV6
    else
        memset(&(((struct sockaddr_in6 *)inSockAddr)->sin6_addr), 0, sizeof(struct in6_addr));
#endif
}
// end setAddressAny

/* -------------------------------------------------------------------
 * Incr the address by value
 * ------------------------------------------------------------------- */
void SockAddr_incrAddress(iperf_sockaddr *inSockAddr, int value) {
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET)
        ((struct sockaddr_in *)inSockAddr)->sin_addr.s_addr += htonl(value);
#if HAVE_IPV6
    else {
        uint32_t *lower = (uint32_t *)&((struct sockaddr_in6 *)inSockAddr)->sin6_addr.s6_addr[12];
        *lower += htonl(value);
    }
#endif
}
// end setAddressAny

/* -------------------------------------------------------------------
 * Set the port to the given port. Handles the byte swapping.
 * ------------------------------------------------------------------- */
void SockAddr_setPort(iperf_sockaddr *inSockAddr, unsigned short inPort) {
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET)
        ((struct sockaddr_in *)inSockAddr)->sin_port = htons(inPort);
#if HAVE_IPV6
    else
        ((struct sockaddr_in6 *)inSockAddr)->sin6_port = htons(inPort);
#endif
}
// end setPort

/* -------------------------------------------------------------------
 * Set the port to zero, which lets the OS pick the port.
 * ------------------------------------------------------------------- */
void SockAddr_setPortAny(iperf_sockaddr *inSockAddr) { SockAddr_setPort(inSockAddr, 0); }
// end setPortAny

/* -------------------------------------------------------------------
 * Return the port. Handles the byte swapping.
 * ------------------------------------------------------------------- */
unsigned short SockAddr_getPort(iperf_sockaddr *inSockAddr) {
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET)
        return ntohs(((struct sockaddr_in *)inSockAddr)->sin_port);
#if HAVE_IPV6
    else
        return ntohs(((struct sockaddr_in6 *)inSockAddr)->sin6_port);
#endif
    return 0;
}
// end getPort

/* -------------------------------------------------------------------
 * Return the IPv4 Internet Address from the sockaddr_in structure
 * ------------------------------------------------------------------- */
struct in_addr *SockAddr_get_in_addr(iperf_sockaddr *inSockAddr) {
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET)
        return &(((struct sockaddr_in *)inSockAddr)->sin_addr);

    fprintf(stderr, "FATAL: get_in_addr called on IPv6 address\n");
    return NULL;
}

/* -------------------------------------------------------------------
 * Return the IPv6 Internet Address from the sockaddr_in6 structure
 * ------------------------------------------------------------------- */
#if HAVE_IPV6
struct in6_addr *SockAddr_get_in6_addr(iperf_sockaddr *inSockAddr) {
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET6)
        return &(((struct sockaddr_in6 *)inSockAddr)->sin6_addr);

    fprintf(stderr, "FATAL: get_in6_addr called on IPv4 address\n");
    return NULL;
}
#endif

/* -------------------------------------------------------------------
 * Return the size of the appropriate address structure.
 * ------------------------------------------------------------------- */
Socklen_t SockAddr_get_sizeof_sockaddr(iperf_sockaddr *inSockAddr) {
#if HAVE_IPV6
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET6) {
        return (sizeof(struct sockaddr_in6));
    }
#endif
    return (sizeof(struct sockaddr_in));
}
// end get_sizeof_sockaddr

/* -------------------------------------------------------------------
 * Return if IPv6 socket
 * ------------------------------------------------------------------- */
int SockAddr_isIPv6(iperf_sockaddr *inSockAddr) {
#if HAVE_IPV6
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET6) {
        return 1;
    }
#endif
    return 0;
}
// end get_sizeof_sockaddr

/* -------------------------------------------------------------------
 * Return true if the address is multicast ip address.
 * ------------------------------------------------------------------- */
int SockAddr_isMulticast(iperf_sockaddr *inSockAddr) {
#if HAVE_IPV6
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET6) {
        return (IN6_IS_ADDR_MULTICAST(&(((struct sockaddr_in6 *)inSockAddr)->sin6_addr)));
    } else
#endif
    {

#ifdef IN_MULTICAST
        // 224.0.0.0 to 239.255.255.255 (e0.00.00.00 to ef.ff.ff.ff)
        // convert from network to host byte order
        uint32_t maddr = ntohl((uint32_t)(((struct sockaddr_in *)inSockAddr)->sin_addr.s_addr));
        return (IN_MULTICAST(maddr));
#else
        const unsigned long kClassD_Mask = 0xf0000000L;
        const unsigned long kMulticast = 0xe0000000L;
        return ((((ntohl(((struct sockaddr_in *)inSockAddr)->sin_addr.s_addr)) & kClassD_Mask) ==
                 kMulticast));
#endif
    }
}
// end isMulticast

/* -------------------------------------------------------------------
 * Return true if the address is multicast ip address.
 * ------------------------------------------------------------------- */
int SockAddr_isLinklocal(iperf_sockaddr *inSockAddr) {
#if HAVE_IPV6
    if (((struct sockaddr *)inSockAddr)->sa_family == AF_INET6) {
        return (IN6_IS_ADDR_LINKLOCAL(&(((struct sockaddr_in6 *)inSockAddr)->sin6_addr)));
    } else
#endif
    {
        return 0;
    }
}

/* -------------------------------------------------------------------
 * Zero out the address structure.
 * ------------------------------------------------------------------- */
void SockAddr_zeroAddress(iperf_sockaddr *inSockAddr) {
    memset(inSockAddr, 0, sizeof(iperf_sockaddr));
}

int SockAddr_isZeroAddress(iperf_sockaddr *inSockAddr) {
    iperf_sockaddr zeroSockAddr;
    memset(&zeroSockAddr, 0, sizeof(iperf_sockaddr));
    return (memcmp((void *)inSockAddr, (void *)&zeroSockAddr, sizeof(iperf_sockaddr)));
}

/* -------------------------------------------------------------------
 * Compare two sockaddrs and return true if they are equal
 * ------------------------------------------------------------------- */
int SockAddr_are_Equal(iperf_sockaddr *first, iperf_sockaddr *second) {
    if (((struct sockaddr *)first)->sa_family == AF_INET &&
        ((struct sockaddr *)second)->sa_family == AF_INET) {
        // compare IPv4 adresses
        return (
            ((long)((struct sockaddr_in *)first)->sin_addr.s_addr ==
             (long)((struct sockaddr_in *)second)->sin_addr.s_addr) &&
            (((struct sockaddr_in *)first)->sin_port == ((struct sockaddr_in *)second)->sin_port));
    }
#if HAVE_IPV6
    if (((struct sockaddr *)first)->sa_family == AF_INET6 &&
        ((struct sockaddr *)second)->sa_family == AF_INET6) {
        // compare IPv6 addresses
        return (!memcmp(((struct sockaddr_in6 *)first)->sin6_addr.s6_addr,
                        ((struct sockaddr_in6 *)second)->sin6_addr.s6_addr,
                        sizeof(struct in6_addr)) &&
                (((struct sockaddr_in6 *)first)->sin6_port ==
                 ((struct sockaddr_in6 *)second)->sin6_port));
    }
#endif
    return 0;
}

/* -------------------------------------------------------------------
 * Compare two sockaddrs and return true if the hosts are equal
 * ------------------------------------------------------------------- */
int SockAddr_Hostare_Equal(iperf_sockaddr *first, iperf_sockaddr *second) {
    if (((struct sockaddr *)first)->sa_family == AF_INET &&
        ((struct sockaddr *)second)->sa_family == AF_INET) {
        // compare IPv4 adresses
        return ((long)((struct sockaddr_in *)first)->sin_addr.s_addr ==
                (long)((struct sockaddr_in *)second)->sin_addr.s_addr);
    }
#if HAVE_IPV6
    if (((struct sockaddr *)first)->sa_family == AF_INET6 &&
        ((struct sockaddr *)second)->sa_family == AF_INET6) {
        // compare IPv6 addresses
        return (!memcmp(((struct sockaddr_in6 *)first)->sin6_addr.s6_addr,
                        ((struct sockaddr_in6 *)second)->sin6_addr.s6_addr,
                        sizeof(struct in6_addr)));
    }
#endif
    return 0;
}

/* -------------------------------------------------------------------
 * Find the interface name of a connected socket (when not already set)
 * Can be forced with -B <ip>%<name> (server), -c <ip>%<name> (client)
 * Note that kernel maps, e.g. via routing tables, to the actual device
 * so these can change.  Assume they won't change during the life
 * of a thread.
 *
 * Store (and cache) the results in the thread settings structure
 * Return 0 if set, -1 if not
 * ------------------------------------------------------------------- */
int SockAddr_Ifrname(struct thread_Settings *inSettings) {
#ifdef HAVE_GETIFADDRS
    if (inSettings->mIfrname == NULL) {
        struct sockaddr_storage myaddr;
        struct ifaddrs *ifaddr;
        struct ifaddrs *ifa;
        socklen_t addr_len;
        addr_len = sizeof(struct sockaddr_storage);
        getsockname(inSettings->mSock, (struct sockaddr *)&myaddr, &addr_len);
        getifaddrs(&ifaddr);

        // look which interface contains the desired IP per getsockname() which
        // sets myaddr When found, ifa->ifa_name contains the name of the
        // interface (eth0, eth1, ppp0...)
        if (myaddr.ss_family == AF_INET) {
            // v4 socket family (supports v4 only)
            struct sockaddr_in *addr = (struct sockaddr_in *)&myaddr;
            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if ((ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_INET)) {
                    struct sockaddr_in *inaddr = (struct sockaddr_in *)ifa->ifa_addr;
                    if ((inaddr->sin_addr.s_addr == addr->sin_addr.s_addr) && (ifa->ifa_name)) {
                        // Found v4 address in v4 addr family, copy it to thread
                        // settings structure
                        inSettings->mIfrname = calloc(strlen(ifa->ifa_name) + 1, sizeof(char));
                        strcpy(inSettings->mIfrname, ifa->ifa_name);
                        break;
                    }
                }
            }
        } else if (myaddr.ss_family == AF_INET6) {
            // v6 socket family (supports both v4 and v6)
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&myaddr;
            // Link local address are shared amongst all devices
            // Try to pull the interface from the destination
            if ((inSettings->mThreadMode == kMode_Client) &&
                (IN6_IS_ADDR_LINKLOCAL(&addr->sin6_addr))) {
                char *results;
                char *copy = (char *)malloc(strlen(inSettings->mHost) + 1);
                strcpy(copy, (const char *)inSettings->mHost);
                if (((results = strtok(copy, "%")) != NULL) &&
                    ((results = strtok(NULL, "%")) != NULL)) {
                    inSettings->mIfrname = calloc(strlen(results) + 1, sizeof(char));
                    strcpy(inSettings->mIfrname, results);
                }
                free(copy);
            } else if ((inSettings->mThreadMode == kMode_Server) &&
                       (IN6_IS_ADDR_V4MAPPED(&addr->sin6_addr))) {
                for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                    if ((ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_INET)) {
                        struct sockaddr_in *inaddr = (struct sockaddr_in *)ifa->ifa_addr;
                        uint32_t v4;
                        memcpy(&v4, &addr->sin6_addr.s6_addr[12], 4);
                        if ((ifa->ifa_name) && (inaddr->sin_addr.s_addr == v4)) {
                            // Found v4 address in v4 addr family, copy it to
                            // thread settings structure
                            inSettings->mIfrname = calloc(strlen(ifa->ifa_name) + 1, sizeof(char));
                            strcpy(inSettings->mIfrname, ifa->ifa_name);
                            break;
                        }
                    }
                }
            } else {
                // Hunt the v6 interfaces
                for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                    if ((ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_INET6)) {
                        struct sockaddr_in6 *inaddr = (struct sockaddr_in6 *)ifa->ifa_addr;
                        if ((ifa->ifa_name) &&
                            (IN6_ARE_ADDR_EQUAL(&addr->sin6_addr, &inaddr->sin6_addr))) {
                            // Found v6 address in v6 addr family, copy it to
                            // thread settings structure
                            inSettings->mIfrname = calloc(strlen(ifa->ifa_name) + 1, sizeof(char));
                            strcpy(inSettings->mIfrname, ifa->ifa_name);
                            break;
                        }
                    }
                }
            }
        }
        freeifaddrs(ifaddr);
    }
#endif
    return ((inSettings->mIfrname == NULL) ? -1 : 0);
}

#ifdef __cplusplus
} /* end extern "C" */
#endif

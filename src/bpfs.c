/*---------------------------------------------------------------
 * Copyright (c) 2025
 * Broadcom Corporation
 * All Rights Reserved.
 *---------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated
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
 * Neither the name of Broadcom Coporation,
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
 *
 * bpfs.c
 * Suppport for bpfs
 *
 * by Robert J. McMahon (rjmcmahon@rjmcmahon.com, bob.mcmahon@broadcom.com)
 * -------------------------------------------------------------------
 */
#include "headers.h"
#include "util.h"
#ifdef __cplusplus
extern "C" {
#endif

#if defined(HAVE_LINUX_FILTER_H) && defined(HAVE_AF_PACKET)
int SockBPF_Drop_All (int sock) {
    struct sock_filter udp_filter[] = {
        { 0x6, 0, 0, 0x00000000 },
    };
    struct sock_fprog bpf = {
        .len = (sizeof(udp_filter) / sizeof(struct sock_filter)),
        .filter = udp_filter,
    };
    return(setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)));
}

//
// [root@ryzen3950 iperf2-code]# tcpdump ip and udp dst port 5001 -e -dd -i any
// tcpdump: data link type LINUX_SLL2
// { 0x28, 0, 0, 0x00000000 },
// { 0x15, 0, 8, 0x00000800 },
// { 0x30, 0, 0, 0x0000001d },
// { 0x15, 0, 6, 0x00000011 },
// { 0x28, 0, 0, 0x0000001a },
// { 0x45, 4, 0, 0x00001fff },
// { 0xb1, 0, 0, 0x00000014 },
// { 0x48, 0, 0, 0x00000016 },
// { 0x15, 0, 1, 0x00001389 },
// { 0x6, 0, 0, 0x00040000 },
// { 0x6, 0, 0, 0x00000000 },

//	{ 0x28, 0, 0, 0x00000014 },
//	{ 0x45, 4, 0, 0x00001fff },
//	{ 0xb1, 0, 0, 0x0000001e },
//	{ 0x48, 0, 0, 0x00000010 },
//	{ 0x15, 0, 1, 0x00001389 },


int SockBPF_v4_Accept (int sock, uint16_t port) {
    // tcpdump udp dst port 5001 -dd to get c code filter
    // UDP port is the 5 and 13 bytecodes (5001 = 0x1389)
    // see linux/filter.h
    struct sock_filter udp_filter[] = {
	{ 0x28, 0, 0, 0x0000000c },
	{ 0x15, 0, 8, 0x00000800 },
	{ 0x30, 0, 0, 0x00000017 },
	{ 0x15, 0, 6, 0x00000011 },
	{ 0x28, 0, 0, 0x00000014 },
	{ 0x45, 4, 0, 0x00001fff },
	{ 0xb1, 0, 0, 0x0000000e },
	{ 0x48, 0, 0, 0x00000010 },
	{ 0x15, 0, 1, 0x00001389 },
	{ 0x6, 0, 0, 0x00040000 },
	{ 0x6, 0, 0, 0x00000000 },
    };
    udp_filter[8].k = port;
    struct sock_fprog bpf = {
	.len = (sizeof(udp_filter) / sizeof(struct sock_filter)),
	.filter = udp_filter,
    };
    return(setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)));
}

//[root@ryzen3950 iperf2-code]# tcpdump  udp dst port 5001 and dst host 1.1.1.1 -dd
//Warning: assuming Ethernet
//{ 0x28, 0, 0, 0x0000000c },
//{ 0x15, 11, 0, 0x000086dd },
//{ 0x15, 0, 10, 0x00000800 },
//{ 0x30, 0, 0, 0x00000017 },
//{ 0x15, 0, 8, 0x00000011 },
//{ 0x28, 0, 0, 0x00000014 },
//{ 0x45, 6, 0, 0x00001fff },
//{ 0xb1, 0, 0, 0x0000000e },
//{ 0x48, 0, 0, 0x00000010 },
//{ 0x15, 0, 3, 0x00001389 },
//{ 0x20, 0, 0, 0x0000001e },
//{ 0x15, 0, 1, 0x01010101 },
//{ 0x6, 0, 0, 0x00040000 },
//{ 0x6, 0, 0, 0x00000000 },
//
// BPF for TAP interaces with explicit v4 IP and UDP dst port
int SockBPF_Accept_V4_TAP (int sock, uint32_t dstip, uint16_t port) {
    struct sock_filter udp_filter[] = {
	{ 0x28, 0, 0, 0x0000000c },
	{ 0x15, 11, 0, 0x000086dd },
	{ 0x15, 0, 10, 0x00000800 },
	{ 0x30, 0, 0, 0x00000017 },
	{ 0x15, 0, 8, 0x00000011 },
	{ 0x28, 0, 0, 0x00000014 },
	{ 0x45, 6, 0, 0x00001fff },
	{ 0xb1, 0, 0, 0x0000000e },
	{ 0x48, 0, 0, 0x00000010 },
	{ 0x15, 0, 3, 0x00001389 },
	{ 0x20, 0, 0, 0x0000001e },
	{ 0x15, 0, 1, 0x00000000 },
	{ 0x6, 0, 0, 0x00040000 },
	{ 0x6, 0, 0, 0x00000000 },
    };
    udp_filter[12].k = htonl(dstip);
    udp_filter[9].k = htons(port);
    struct sock_fprog bpf = {
	.len = (sizeof(udp_filter) / sizeof(struct sock_filter)),
	.filter = udp_filter,
    };
    return(setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)));
}

// Simulate the UDP connect for the AF_PACKET (or PF_PACKET)
//
int SockBPF_v4_Connect (int sock, uint32_t dstip, uint32_t srcip, uint16_t dstport, uint16_t srcport) {
    // Use full quintuple, proto, src ip, dst ip, src port, dst port
    // ip proto is already set per the PF_PACKET ETH_P_IP
    // tcpdump udp and ip src 127.0.0.1 and ip dst 127.0.0.2 and src port 5001 and dst port 5002  -dd
    //
    // tcpdump udp and ip src 127.0.0.1 and ip dst 127.0.0.2 and src port 5001 and dst port 5002 -d
    //  (000) ldh      [12]
    //  (001) jeq      #0x86dd          jt 17	jf 2
    //  (002) jeq      #0x800           jt 3	jf 17
    //  (003) ldb      [23]
    //  (004) jeq      #0x11            jt 5	jf 17
    //  (005) ld       [26]
    //  (006) jeq      #0x7f000001      jt 7	jf 17
    //  (007) ld       [30]
    //  (008) jeq      #0x7f000002      jt 9	jf 17
    //  (009) ldh      [20]
    //  (010) jset     #0x1fff          jt 17	jf 11
    //  (011) ldxb     4*([14]&0xf)
    //  (012) ldh      [x + 14]
    //  (013) jeq      #0x1389          jt 14	jf 17
    //  (014) ldh      [x + 16]
    //  (015) jeq      #0x138a          jt 16	jf 17
    //  (016) ret      #262144
    //  (017) ret      #0
    //
    struct sock_filter udp_filter[] = {
	{ 0x28, 0, 0, 0x0000000c },
	{ 0x15, 15, 0, 0x000086dd },
	{ 0x15, 0, 14, 0x00000800 },
	{ 0x30, 0, 0, 0x00000017 },
	{ 0x15, 0, 12, 0x00000011 },
	{ 0x20, 0, 0, 0x0000001a },
	{ 0x15, 0, 10, 0x7f000001 },
	{ 0x20, 0, 0, 0x0000001e },
	{ 0x15, 0, 8, 0x7f000002 },
	{ 0x28, 0, 0, 0x00000014 },
	{ 0x45, 6, 0, 0x00001fff },
	{ 0xb1, 0, 0, 0x0000000e },
	{ 0x48, 0, 0, 0x0000000e },
	{ 0x15, 0, 3, 0x00001389 },
	{ 0x48, 0, 0, 0x00000010 },
	{ 0x15, 0, 1, 0x0000138a },
	{ 0x6, 0, 0, 0x00040000 },
	{ 0x6, 0, 0, 0x00000000 },
    };
    udp_filter[6].k = htonl(srcip);
    udp_filter[8].k = htonl(dstip);
    udp_filter[13].k = htons(srcport);
    udp_filter[15].k = htons(dstport);
    struct sock_fprog bpf = {
	.len = (sizeof(udp_filter) / sizeof(struct sock_filter)),
	.filter = udp_filter,
    };
    return(setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)));
}
int SockBPF_v4_Connect_TAP (int sock, uint32_t dstip, uint32_t srcip, uint16_t dstport, uint16_t srcport) {
    // [root@ryzen3950 iperf2-code]# tcpdump ip and src 127.0.0.1 and dst 127.0.0.2 and udp src port 5001 and dst port 5002  -d
    // Warning: assuming Ethernet
    // (000) ldh      [12]
    // (001) jeq      #0x800           jt 2	jf 16
    // (002) ld       [26]
    // (003) jeq      #0x7f000001      jt 4	jf 16
    // (004) ld       [30]
    // (005) jeq      #0x7f000002      jt 6	jf 16
    // (006) ldb      [23]
    // (007) jeq      #0x11            jt 8	jf 16
    // (008) ldh      [20]
    // (009) jset     #0x1fff          jt 16	jf 10
    // (010) ldxb     4*([14]&0xf)
    // (011) ldh      [x + 14]
    // (012) jeq      #0x1389          jt 13	jf 16
    // (013) ldh      [x + 16]
    // (014) jeq      #0x138a          jt 15	jf 16
    // (015) ret      #262144
    // (016) ret      #0
    // [root@ryzen3950 iperf2-code]# tcpdump ip and src 127.0.0.1 and dst 127.0.0.2 and udp src port 5001 and dst port 5002  -dd
    // Warning: assuming Ethernet
    struct sock_filter udp_filter[] = {
	{ 0x28, 0, 0, 0x0000000c },
	{ 0x15, 0, 14, 0x00000800 },
	{ 0x20, 0, 0, 0x0000001a },
	{ 0x15, 0, 12, 0x7f000001 },
	{ 0x20, 0, 0, 0x0000001e },
	{ 0x15, 0, 10, 0x7f000002 },
	{ 0x30, 0, 0, 0x00000017 },
	{ 0x15, 0, 8, 0x00000011 },
	{ 0x28, 0, 0, 0x00000014 },
	{ 0x45, 6, 0, 0x00001fff },
	{ 0xb1, 0, 0, 0x0000000e },
	{ 0x48, 0, 0, 0x0000000e },
	{ 0x15, 0, 3, 0x00001389 },
	{ 0x48, 0, 0, 0x00000010 },
	{ 0x15, 0, 1, 0x0000138a },
	{ 0x6, 0, 0, 0x00040000 },
	{ 0x6, 0, 0, 0x00000000 },
    };
    udp_filter[3].k = htonl(srcip);
    udp_filter[5].k = htonl(dstip);
    udp_filter[12].k = htons(srcport);
    udp_filter[14].k = htons(dstport);
    struct sock_fprog bpf = {
	.len = (sizeof(udp_filter) / sizeof(struct sock_filter)),
	.filter = udp_filter,
    };
    return(setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)));
}

int SockBPF_v4_Connect_BPF (int sock, uint32_t dstip, uint32_t srcip, uint16_t dstport, uint16_t srcport) {
    // Use full quintuple, proto, src ip, dst ip, src port, dst port
    // ip proto is already set per the PF_PACKET ETH_P_IP
    // tcpdump udp and ip src 127.0.0.1 and ip dst 127.0.0.2 and src port 5001 and dst port 5002 -dd

    struct sock_filter udp_filter[] = {
	{ 0x28, 0, 0, 0x0000000c },
	{ 0x15, 15, 0, 0x000086dd },
	{ 0x15, 0, 14, 0x00000800 },
	{ 0x30, 0, 0, 0x00000017 },
	{ 0x15, 0, 12, 0x00000011 },
	{ 0x20, 0, 0, 0x0000001a },
	{ 0x15, 0, 10, 0x7f000001 },
	{ 0x20, 0, 0, 0x0000001e },
	{ 0x15, 0, 8, 0x7f000002 },
	{ 0x28, 0, 0, 0x00000014 },
	{ 0x45, 6, 0, 0x00001fff },
	{ 0xb1, 0, 0, 0x0000000e },
	{ 0x48, 0, 0, 0x0000000e },
	{ 0x15, 0, 3, 0x00001389 },
	{ 0x48, 0, 0, 0x00000010 },
	{ 0x15, 0, 1, 0x0000138a },
	{ 0x6, 0, 0, 0x00000000 },
	{ 0x6, 0, 0, 0x00000000 },
    };
    udp_filter[6].k = htonl(srcip);
    udp_filter[8].k = htonl(dstip);
    udp_filter[13].k = htons(srcport);
    udp_filter[15].k = htons(dstport);
    struct sock_fprog bpf = {
	.len = (sizeof(udp_filter) / sizeof(struct sock_filter)),
	.filter = udp_filter,
    };
    return(setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)));
}
#if HAVE_IPV6
//
// v6 Connected BPF, use 32 bit values
//
int SockBPF_v6_Connect (int sock, struct in6_addr *dst, struct in6_addr *src, uint16_t dstport, uint16_t srcport) {
    // Use full quintuple, proto, src ip, dst ip, src port, dst port
    // tcpdump udp and ip6 src fe80::428d:5cff:fe6a:2d85 and ip6 dst fe80::428d:5cff:fe6a:2d86 and src port 5001 and dst port 5002 -dd
    //
    //tcpdump udp and ip6 src fe80::428d:5cff:fe6a:2d85 and ip6 dst fe80::428d:5cff:fe6a:2d86 and src port 5001 and dst port 5002 -d
    //  (000) ldh      [12]
    //  (001) jeq      #0x86dd          jt 2	jf 32
    //  (002) ldb      [20]
    //  (003) jeq      #0x11            jt 7	jf 4
    //  (004) jeq      #0x2c            jt 5	jf 32
    //  (005) ldb      [54]
    //  (006) jeq      #0x11            jt 7	jf 32
    //  (007) ld       [22]
    //  (008) jeq      #0xfe800000      jt 9	jf 32
    //  (009) ld       [26]
    //  (010) jeq      #0x0             jt 11	jf 32
    //  (011) ld       [30]
    //  (012) jeq      #0x428d5cff      jt 13	jf 32
    //  (013) ld       [34]
    //  (014) jeq      #0xfe6a2d85      jt 15	jf 32
    //  (015) ld       [38]
    //  (016) jeq      #0xfe800000      jt 17	jf 32
    //  (017) ld       [42]
    //  (018) jeq      #0x0             jt 19	jf 32
    //  (019) ld       [46]
    //  (020) jeq      #0x428d5cff      jt 21	jf 32
    //  (021) ld       [50]
    //  (022) jeq      #0xfe6a2d86      jt 23	jf 32
    //  (023) ldb      [20]
    //  (024) jeq      #0x84            jt 27	jf 25
    //  (025) jeq      #0x6             jt 27	jf 26
    //  (026) jeq      #0x11            jt 27	jf 32
    //  (027) ldh      [54]
    //  (028) jeq      #0x1389          jt 29	jf 32
    //  (029) ldh      [56]
    //  (030) jeq      #0x138a          jt 31	jf 32
    //  (031) ret      #262144
    //  (032) ret      #0
    //
    struct sock_filter udp_filter[] = {
	{ 0x28, 0, 0, 0x0000000c },
	{ 0x15, 0, 30, 0x000086dd },
	{ 0x30, 0, 0, 0x00000014 },
	{ 0x15, 3, 0, 0x00000011 },
	{ 0x15, 0, 27, 0x0000002c },
	{ 0x30, 0, 0, 0x00000036 },
	{ 0x15, 0, 25, 0x00000011 },
	{ 0x20, 0, 0, 0x00000016 },
	{ 0x15, 0, 23, 0xfe800000 },
	{ 0x20, 0, 0, 0x0000001a },
	{ 0x15, 0, 21, 0x00000000 },
	{ 0x20, 0, 0, 0x0000001e },
	{ 0x15, 0, 19, 0x428d5cff },
	{ 0x20, 0, 0, 0x00000022 },
	{ 0x15, 0, 17, 0xfe6a2d85 },
	{ 0x20, 0, 0, 0x00000026 },
	{ 0x15, 0, 15, 0xfe800000 },
	{ 0x20, 0, 0, 0x0000002a },
	{ 0x15, 0, 13, 0x00000000 },
	{ 0x20, 0, 0, 0x0000002e },
	{ 0x15, 0, 11, 0x428d5cff },
	{ 0x20, 0, 0, 0x00000032 },
	{ 0x15, 0, 9, 0xfe6a2d86 },
	{ 0x30, 0, 0, 0x00000014 },
	{ 0x15, 2, 0, 0x00000084 },
	{ 0x15, 1, 0, 0x00000006 },
	{ 0x15, 0, 5, 0x00000011 },
	{ 0x28, 0, 0, 0x00000036 },
	{ 0x15, 0, 3, 0x00001389 },
	{ 0x28, 0, 0, 0x00000038 },
	{ 0x15, 0, 1, 0x0000138a },
	{ 0x6, 0, 0, 0x00040000 },
	{ 0x6, 0, 0, 0x00000000 },
    };
    udp_filter[8].k = htonl((*src).s6_addr32[0]);
    udp_filter[10].k = htonl((*src).s6_addr32[1]);
    udp_filter[12].k = htonl((*src).s6_addr32[2]);
    udp_filter[14].k = htonl((*src).s6_addr32[3]);
    udp_filter[16].k = htonl((*dst).s6_addr32[0]);
    udp_filter[18].k = htonl((*dst).s6_addr32[1]);
    udp_filter[20].k = htonl((*dst).s6_addr32[2]);
    udp_filter[22].k = htonl((*dst).s6_addr32[3]);
    udp_filter[28].k = htons(srcport);
    udp_filter[30].k = htons(dstport);
    struct sock_fprog bpf = {
	.len = (sizeof(udp_filter) / sizeof(struct sock_filter)),
	.filter = udp_filter,
    };
    return(setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)));
}
#  endif // HAVE_V6
#endif // HAVE_LINUX_FILTER

#ifdef __cplusplus
} /* end extern "C" */
#endif

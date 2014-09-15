#ifndef __udp_common__
#define __udp_common__

#include <stdlib.h> // EXIT_FAILURE, exit()
#include <stdio.h> // fprintf(), stderr
#include <string.h> // memset()
#include <time.h> // timeval

#include <netinet/in.h> // sockaddr_in, ip_mreq, INADDR_ANY, IPPROTO_IP
                        // IP_ADD_MEMBERSHIP, IP_DROP_MEMBERSHIP, IP_MULTICAST_LOOP
#include <sys/socket.h> // AF_INET, setsockopt(), SOL_SOCKET, SO_BROADCAST, SO_RCVTIMEO
#include <arpa/inet.h> // htons(), htonl()

#define BUFFER_SIZE      (4096)
#define MSG_HEADER       0xFEFEFEFE
#define DEFAULT_PAYLOAD  (512)

struct message {
    unsigned int       header;
    unsigned long long seq_num;
    unsigned long long sec;
    unsigned long long nsec;
    char payload[DEFAULT_PAYLOAD];
};

void init_socket(struct sockaddr_in * sock_in,
                 const char * addr_name,
                 const int port) {
    memset(sock_in, 0, sizeof(struct sockaddr_in));
    sock_in -> sin_family = AF_INET;
    sock_in -> sin_addr.s_addr = inet_addr(addr_name);
    sock_in -> sin_port = htons(port);
}

void init_socket_any(struct sockaddr_in * sock_in,
                     const int port) {
    memset(sock_in, 0, sizeof(struct sockaddr_in)); // or bzero
    sock_in -> sin_family = AF_INET;
    sock_in -> sin_addr.s_addr = INADDR_ANY;
    sock_in -> sin_port = htons(port);
}

void mcast_add_membership_on_socket(const int socket,
                                    const char * ip) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ip);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, & mreq, sizeof(mreq)) < 0) {
        fprintf(stderr, "Error on setting multicast membership on socket.\n");
        exit(EXIT_FAILURE);
    }
}

void mcast_drop_membership_on_socket(const int socket,
                                     const char * ip) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ip);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, & mreq, sizeof(mreq)) < 0) {
        fprintf(stderr, "Error on dropping multicast membership on socket.\n");
        exit(EXIT_FAILURE);
    }
}

void mcast_enable_loop_on_socket(const int socket,
                                 const unsigned int enable_loopback) {
    if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_LOOP,
                   (unsigned char *) & enable_loopback, sizeof(unsigned char)) < 0) {
        fprintf(stderr, "Error on setting multicast loopback on socket.\n");
        exit(EXIT_FAILURE);
    }
}

void bcast_enable_on_socket(const int socket,
                            const int enable_broadcast) {
    if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST,
                      & enable_broadcast, sizeof(enable_broadcast))) {
        fprintf(stderr, "Error on setting broadcast on socket.\n");
        exit(EXIT_FAILURE);
    }
}

void timeout_set_on_socket(const int socket,
                           const unsigned int timeout_s,
                           const unsigned long timeout_us) {
    struct timeval tv;
    tv.tv_sec = timeout_s;
    tv.tv_usec = timeout_us;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, & tv, sizeof(tv)) < 0) {
        fprintf(stderr, "Error setting timeout on socket.\n");
        exit(EXIT_FAILURE);
    }
}

#endif

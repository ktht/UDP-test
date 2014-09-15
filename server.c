#include <stdlib.h> // EXIT_SUCCESS, EXIT_FAILURE, exit()
#include <stdio.h> // fprintf(), stderr
#include <unistd.h> // getopt(), optarg, optopt
#include <signal.h> // signal(), SIGINT

#include <netinet/in.h> // sockaddr_in, PF_INET, IPPROTO_UDP
#include <sys/socket.h> // SOCK_DGRAM, socket(), sendto(), recvfrom(), close()

#include "dbg.h" // DEBUG_PRINT(), LOG()
#include "udp_common.h"

#define USAGE(exec_name) \
 "Usage: " exec_name " [options]\n" \
 "Options:\n" \
 "  -p <port number>                -- port number \n" \
 "  [-w <timeout>] (=0)             -- timeout between sending the packets (ms)\n" \
 "  [-m <multicast address>]        -- enable multicast\n" \
 "  [-l]                            -- enable loopback\n"

/* initialize variables */
int                udp_socket = -1;
int                port_number = -1;
char               multicast_ip[32];
unsigned int       enable_multicast = 0;
int                enable_loopback = 0;
char               buffer[BUFFER_SIZE];

void intHandler(int return_code) {
    if(enable_multicast)
        mcast_drop_membership_on_socket(udp_socket, multicast_ip);
    if(udp_socket)
        close(udp_socket);
    exit(return_code);
}

int main(int argc, char ** argv) {
    
    /* handle ^C behavior */
    signal(SIGINT, intHandler);
    
    /* parse command line arguments */
    int cmd_flag, err_count = 0;
    while ((cmd_flag = getopt(argc, argv, ":p:m:l")) != -1) {
        switch (cmd_flag) {
            case 'p':
                port_number = strtol(optarg, NULL, 10);
                break;
            case 'm':
                enable_multicast = 1;
                strcpy(multicast_ip, optarg);
                break;
            case 'l':
                enable_loopback = 1;
                break;
            case ':':
                fprintf(stderr, "Option -%c requires an operand.\n", optopt);
                ++err_count;
                break;
            case '?':
                fprintf(stderr, "Unrecognized option: -%c\n", optopt);
                ++err_count;
                break;
        }
    }
    if (err_count > 0 || port_number == -1) {
        fprintf(stderr, USAGE("%s"), argv[0]);
        exit(EXIT_FAILURE);
    }
    
    /* print information */
    LOG("Port number:       %i\n", port_number);
    LOG("Enable multicast:  %s\n", enable_multicast ? "yes" : "no");
    if(enable_multicast)
        LOG("Multicast address: %s\n", multicast_ip);
    LOG("Enable loopback:   %s\n", enable_loopback ? "yes" : "no");
    
    /* create sockets */
    struct sockaddr_in socket_addr;
    struct sockaddr_in peer_addr;
    socklen_t socket_addr_size = sizeof(struct sockaddr_in);
    socklen_t peer_addr_size = sizeof(struct sockaddr_in);
    
    /* initialize server sockets */
    LOG("Initialize socket.\n");
    init_socket_any(&socket_addr, port_number);
    if ((udp_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        fprintf(stderr, "Error creating on UDP socket.\n");
        return EXIT_FAILURE;
    }
    
    /* add to multicast if possible */
    if (enable_multicast) {
        mcast_add_membership_on_socket(udp_socket, multicast_ip);
    }
    mcast_enable_loop_on_socket(udp_socket, enable_loopback);
    
    /* associate the socket with a port */
    LOG("Bind the socket.\n");
    if ((bind(udp_socket, (struct sockaddr *) & socket_addr, socket_addr_size)) == -1) {
        fprintf(stderr, "Error on binding UDP socket.\n");
        return EXIT_FAILURE;
    }
    
    /* send and receive datagrams */
    LOG("Start echo server.\n");
    int read_size = 0;
    while (TRUE) {
        /* receive signal from the client */
        read_size = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0,
                             (struct sockaddr *) & peer_addr, & peer_addr_size);
        if (read_size == -1)
            break;
        const struct message * msg = (struct message *) buffer;
        
        /* if not our packet, skip it */
        if(msg -> header != MSG_HEADER)
            continue;
        
        LOG("Recieved packet from: %s\tPacket nr: %llu\n",
            inet_ntoa(peer_addr.sin_addr), msg -> seq_num);
        
        /* send back to the client */
        if (sendto(udp_socket, buffer, read_size, 0,
                    (struct sockaddr *) & peer_addr, peer_addr_size) == -1) {
            fprintf(stderr, "Error sending packet to %s\n",
                            inet_ntoa(peer_addr.sin_addr));
        }
    }
    
    intHandler(EXIT_SUCCESS);
    
    return EXIT_SUCCESS;
}

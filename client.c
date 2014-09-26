#include <stdlib.h> // EXIT_SUCCESS, EXIT_FAILURE, exit()
#include <stdio.h>  // FILE, fprintf(), fflush(), fclose(), stderr
#include <unistd.h> // getopt(), optarg, optopt
#include <signal.h> // signal(), SIGINT
#include <time.h>   // timespec, clock_gettime(), CLOCK_MONOTONIC,
                    // time_t, tm, time(), localtime(), asctime()

#include <netinet/in.h> // sockaddr, sockaddr_in, PF_INET, IPPROTO_UDP
#include <sys/socket.h> // SOCK_DGRAM, socket(), sendto(), recvfrom(), close()

#include "dbg.h" // DEBUG_PRINT(), LOG()
#include "udp_common.h"

#define USAGE(exec_name) \
  "Usage: " exec_name " [options]\n" \
  "Options:\n" \
  "  -s <server address>           -- server address\n" \
  "  -p <port number>              -- port number\n" \
  "  [-i <interval>] (=1E7)        -- time interval between packets (ns)\n" \
  "  [-n <number of packets>] (=0) -- number of packets to send (default: inf)\n" \
  "  [-w <timeout>] (=0)           -- socket timeout (ms)\n" \
  "  [-t <ToS code>] (=0)          -- ToS code (decimal)\n" \
  "  [-P <payload size>]           -- payload size\n" \
  "  [-f <file name>]              -- log file name\n" \
  "  [-m]                          -- enable multicast\n" \
  "  [-b]                          -- enable broadcast\n" \
  "  [-l]                          -- enable loopback\n" \
  "  [-r]                          -- record system clock (default: time difference)\n" \
  "  [-S]                          -- send only\n"

/* initialize variables */
int        udp_socket  = -1;
int        port_number = -1;
char       server_addr_name[32];

socklen_t  enable_loopback  = 0;
socklen_t  enable_broadcast = 0;
bool       enable_multicast = FALSE;
bool       record_sys_clock = FALSE;
bool       send_only        = FALSE;

int        socket_tos       = 0;
u_ll       timeout          = 0;
u_ll       interval         = DEFAULT_INTERVAL;
u_int      payload_size     = DSRD_PKG_SIZE - sizeof(struct message);
char       buffer[BUFFER_SIZE];

u_ll       total_responses  = 0;
u_ll       max_responses    = 0;
u_ll       missing_packages = 0;
FILE *     log_file         = NULL;

void intHandler(int return_code) {
    if(enable_multicast)
        mcast_drop_membership_on_socket(udp_socket, server_addr_name);
    if(udp_socket)
        close(udp_socket);
    if(log_file) {
        /* print packet statistics */
        if (send_only != TRUE) {
            long double packet_loss = (long double) missing_packages / total_responses;
            fprintf(log_file, "Received:    %llu\t",    total_responses);
            fprintf(log_file, "Missed:      %llu\t",    missing_packages);
            fprintf(log_file, "Packet loss: %.3Lf%%\n", packet_loss);
        }

        /* print current date and time */
        time_t raw_time;
        struct tm * time_info;
        time(& raw_time);
        time_info = localtime(& raw_time);
        fprintf(log_file, "%s", asctime(time_info));

        /* flush and close the file */
        fflush(log_file);
        fclose(log_file);

        LOG("Communication end.\n");
    }
    exit(return_code);
}

int main(int argc, char ** argv) {

    /* handle ^C behavior */
    signal(SIGINT, intHandler);

    /* parse command line arguments */
    int cmd_flag, err_count = 0, server_addr_given = 0;
    while ((cmd_flag = getopt(argc, argv, ":s:p:i:n:w:t:P:f:mblrS")) != -1) {
        switch (cmd_flag) {
            case 's':
                server_addr_given = 1;
                strcpy(server_addr_name, optarg);
                break;
            case 'p':
                port_number   = strtol(optarg, NULL, 10);
                break;
            case 'i':
                interval      = strtoul(optarg, NULL, 10);
                break;
            case 'n':
                max_responses = strtoul(optarg, NULL, 10);
                break;
            case 'w':
                timeout       = strtoul(optarg, NULL, 10);
                break;
            case 't':
                socket_tos    = strtoul(optarg, NULL, 10);
                break;
            case 'P':
                payload_size  = strtoul(optarg, NULL, 10);
                break;
            case 'f':
                log_file = fopen(optarg, "w");
                break;
            case 'm':
                enable_multicast = TRUE;
                break;
            case 'b':
                enable_broadcast = 1;
                break;
            case 'l':
                enable_loopback  = 1;
                break;
            case 'r':
                record_sys_clock = TRUE;
                break;
            case 'S':
                send_only        = TRUE;
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
    if (err_count > 0 || port_number == -1 || server_addr_given == 0) {
        fprintf(stderr, USAGE("%s"), argv[0]);
        exit(EXIT_FAILURE);
    }

    /* print information */
    LOG("Server address:        %s\n",   server_addr_name);
    LOG("Port number:           %i\n",   port_number);
    LOG("Packet interval (ns):  %llu\n", interval);
    LOG("Max number of packets: %llu\n", max_responses);
    LOG("Socket timeout (ms):   %llu\n", timeout);
    LOG("Socket ToS:            %u\n",   socket_tos);
    LOG("Payload size:          %u\n",   payload_size);
    LOG("Enable multicast:      %s\n",   enable_multicast ? "yes" : "no");
    LOG("Enable broadcast:      %s\n",   enable_broadcast ? "yes" : "no");
    LOG("Enable loopback:       %s\n",   enable_loopback  ? "yes" : "no");
    LOG("Record system clock:   %s\n",   record_sys_clock ? "yes" : "no");
    LOG("Send only mode:        %s\n",   send_only        ? "yes" : "no");

    /* create socket */
    struct sockaddr_in server_addr, rcv_addr;
    socklen_t server_addr_size = sizeof(struct sockaddr_in);
    socklen_t rcv_addr_size    = sizeof(struct sockaddr_in);

    /* initialize socket */
    LOG("Initialize socket.\n");
    init_socket(& server_addr, server_addr_name, port_number);
    if ((udp_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        fprintf(stderr, "Error initializing UDP socket.\n");
        exit(EXIT_FAILURE);
    }

    /* set ToS */
    int tos = (socket_tos & 0x3F) << 2;
    if (tos) {
        if(setsockopt(udp_socket, IPPROTO_IP, IP_TOS, & tos, sizeof(tos)) < 0)
            fprintf(stderr, "Failed to set IP_TOS to %i.\n", tos);
    }

    /* add to broad/multicast if possible */
    if (enable_multicast) {
        mcast_add_membership_on_socket(udp_socket, server_addr_name);
    }
    mcast_enable_loop_on_socket(udp_socket, enable_loopback);
    bcast_enable_on_socket(udp_socket, enable_broadcast);

    /* set timeout */
    const u_int  timeout_s  = (u_long) (timeout / 1E3);
    const u_long timeout_us = (timeout % (u_long) 1E3) * (u_long) 1E3;
    timeout_set_on_socket(udp_socket, timeout_s, timeout_us);

    /* set time variables */
    struct timespec sent_time;
    clock_gettime(AVAILABLE_MONOTONIC_CLOCK, & sent_time);

    /* talk to server */
    LOG("Start communicating with the server.\n");
    int  read_size = 0;
    u_ll seq_num   = 0;
    while (TRUE) {

        /* get current time */
        struct timespec present_time;
        clock_gettime(AVAILABLE_MONOTONIC_CLOCK, & present_time);

        /* if enough time is passed */
        if ((present_time.tv_nsec + present_time.tv_sec * 1E9) -
            (sent_time.tv_nsec + sent_time.tv_sec * 1E9) >= interval) {

            /* construct the message */
            struct message msg;
            msg.header  = MSG_HEADER;
            msg.sec     = present_time.tv_sec;
            msg.nsec    = present_time.tv_nsec;
            msg.seq_num =
                #ifdef ARM
                    swap_uint64(seq_num)
                #else
                    seq_num
                #endif
            ;
            ++seq_num;

            /* send the packet */
            if (sendto(udp_socket, &msg, sizeof(struct message) + payload_size, 0,
                    (struct sockaddr *) & server_addr, server_addr_size) == -1) {
                fprintf(stderr, "Error on sending UDP packet.\n");
            }

            /* get current time */
            clock_gettime(AVAILABLE_MONOTONIC_CLOCK, & present_time);

            /* save sent time */
            sent_time = present_time;

            LOG("Sent packet number %llu to %s.\n", seq_num - 1, server_addr_name);

            const struct message * response = NULL;
            if (send_only != 1) {
                /* get response from the server/multicast address */
                read_size = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0,
                                    (struct sockaddr *) & rcv_addr, & rcv_addr_size);
                if (read_size < 1)
                    break;
                response = (struct message *) buffer;
                LOG("Received packet nr %llu from %s.\n", seq_num - 1, server_addr_name);

                /* check if the packet has gone missing */
                u_ll response_seq_num =
                    #ifdef ARM
                        swap_uint64(response -> seq_num)
                    #else
                        response -> seq_num
                    #endif
                ;
                if (response_seq_num != seq_num - 1) {
                    fprintf(stderr, "Packet no %llu has gone missing.\n", seq_num - 1);
                    ++missing_packages;
                }
                ++total_responses;
            }

            /* write results */
            if (log_file) {
                if (record_sys_clock) {
                    const u_ll current_time = present_time.tv_sec * 1E9 +
                                              present_time.tv_nsec;
                    fprintf(log_file, "%llu,%llu\n", seq_num - 1, current_time);
                }
                else {
                    const u_ll time_difference =
                                  (present_time.tv_sec - response -> sec) * 1E9 +
                                  (present_time.tv_nsec - response -> nsec);
                    fprintf(log_file, "%llu,%.3f\n", seq_num - 1, time_difference / 1E3);
                }
                fflush(log_file);
            }

            /* terminate if enough sent */
            if (total_responses >= max_responses && max_responses)
                break;
        }
    }

    intHandler(EXIT_SUCCESS);

    return EXIT_SUCCESS;
}

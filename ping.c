/* Ping: Sends ICMP echo requests to host

    brief: -makes use of raw sockets
           -features count of messages to be sent, by default infinite()
           -features interval between echo requests, default is 1 sec
           -features timeout for response, default is 200 

    Usage: sudo ./ping <address/hostname> [-c count] [-i interval(secs)] [-W timeout(mSecs)]

    NOTE: needs sudo privilages

  Author: Amol Kulkarni - amolkulk@andrew.cmu.edu
*/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <time.h>

#define HOSTLEN 256
#define SERVLEN 8
#define SINGLE_PROTOCOL 0
#define MAXLINE 64
#define MAX_IP_LEN (65536)
#define IP_PKT_SIZE (20)
#define ICMP_ECHO_REQ_TYPE (8)
#define MS_TO_US (1000)


/* Structure of an ICMP packet */
typedef struct __attribute__ ((packed)){
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t ID;
    uint16_t sequence;
    struct timeval timestamp;
    char data[56];
} icmp_packet;

static uint32_t pkt_count = 0;
static uint32_t interval = 1; //1 sec interval between each echo request
static uint32_t timeout = 200; //200 mSec interval timeout to wait for response
static uint32_t pkts_sent = 0;
static uint32_t pkts_lost = 0;
static bool is_count_given = false;

/* static helper functions */
static uint16_t get_checksum(void*, int);
static void do_ping();
static void usage();

/* 
brief: signal handler for ctrl-c, print stats and terminate
*/
void sigint_handler(int sig) {
  printf(" Total Packets Sent: %d \t Packets lost: %d\n", pkts_sent, pkts_lost);
  exit(-1);
}


int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sigint_handler);
    int c;

    /* Check command line args */
    if (argc < 2) {
      usage();
    }
    if(getuid() != 0){
      printf("Need root permissions\n");
      usage();
    }
    while((c = getopt(argc, argv, ":c:i:W:")) != -1) {
      switch(c) {
        case 'c':
          pkt_count = atoi(optarg);
          is_count_given = true;
          break;
        case 'i':
          interval = atoi(optarg);
          break;
        case 'W':
          timeout = atoi(optarg);
          break;

        case '?':
          usage();
      }
    }

    do_ping(argv[optind]);

    return 0;
}

/*
params: host -- hostname/address to ping

brief:  Send echo requests to the geiven host & check for echo reponse

*/
static void do_ping(char *host){

    printf("Sending ping echo request to %s\n", host);
    // Get the addressig information
    struct addrinfo addr_info;
    struct addrinfo *res_info;
    addr_info.ai_flags = 0;
    addr_info.ai_family = AF_INET;
    addr_info.ai_socktype = SOCK_RAW;
    addr_info.ai_protocol = 1; //ICMP
    addr_info.ai_canonname = NULL;
    addr_info.ai_next = NULL;
    addr_info.ai_addr = NULL;
    
    if(getaddrinfo(host, NULL, &addr_info, &res_info) != 0 ) {
        printf("ERROR in getaddrinfo\n");
        exit(-1);
    }
    if(!res_info) { 
        printf("ERROR in getaddrinfo\n"); 
        exit(-1);
    }

    // Open a connection
    int sockfd;
    sockfd = socket(res_info->ai_family, res_info->ai_socktype, res_info->ai_protocol);
    if(sockfd < 0) { 
        printf("ERROR in socket %d\n", errno);
        exit(-1);
    }

    struct timeval resp_timeout;
    resp_timeout.tv_sec = 0;
    resp_timeout.tv_usec = timeout*MS_TO_US;

    struct timeval tx_time;

    icmp_packet *tx_packet = (icmp_packet*)malloc(sizeof(icmp_packet));
    if(!icmp_packet){
        printf("Error in mallocing memory for icmp packet\n");
        exit(-1);
    }
    uint8_t* rx_buffer = malloc(MAX_IP_LEN);
    if(!rx_buffer){
        printf("Error in mallocing memory for rx_buffer\n");
        exit(-1);
    }

    fd_set sockfds;;
    FD_ZERO(&sockfds);
    FD_SET(sockfd, &sockfds);

    while(1){
      // Init the packet to be sent
      memset(tx_packet, 0, sizeof(icmp_packet));
      tx_packet->type = ICMP_ECHO_REQ_TYPE;
      tx_packet->code = 0x0; // ECHO REQUEST format
      tx_packet->ID = 0;
      tx_packet->sequence = pkts_sent;

      if( is_count_given && pkts_sent >= pkt_count){
        break;
      }
      if(gettimeofday(&tx_time, NULL) < 0){
        printf("Error in getting time");
        exit(-1);
      }

      memcpy(&tx_packet->timestamp, &tx_time, sizeof(tx_time));

      tx_packet->checksum = get_checksum(tx_packet, sizeof(icmp_packet));

      setsockopt(sockfd, IPPROTO_IP, SO_RCVTIMEO, &resp_timeout, sizeof(resp_timeout));

      if(sendto(sockfd, tx_packet, sizeof(icmp_packet), 0, res_info->ai_addr, res_info->ai_addrlen) < 0){
        fprintf(stderr, "%s\n", strerror(errno));
        exit(-1);
      }
      else {
        pkts_sent++;
        // wait for timeout
        if(select(32, &sockfds, NULL, NULL, &resp_timeout) == 0){
          printf("Timed out waiting for packet : %d\n", pkts_sent);
          pkts_lost++;
          continue;
        }
        else{
          // read from socket
          if( recvfrom(sockfd, rx_buffer, MAX_IP_LEN, 0, NULL, NULL) < 0){
            printf("Unable to receive packet\n");
            exit(-1);
          }
          else {
            struct timeval rx_time;
            if(gettimeofday(&rx_time, NULL) < 0){
              printf("Error in getting time");
              exit(-1);
            }

            //get the icmp packet and ignore the IP packet
            icmp_packet *rx_packet = (icmp_packet*)(rx_buffer + IP_PKT_SIZE);

            double diff_time = (float)(rx_time.tv_usec - rx_packet->timestamp.tv_usec)/(float)1000;
            printf("Echo Reply received from %s (RTT : %f mSec)\n", host, diff_time);
            sleep(interval);
          }
        }
      }
    }
    printf(" Total Packets Sent %d Packets lost %d\n", pkts_sent, pkts_lost);
    free(rx_buffer);
    free(tx_packet);
    close(sockfd);
    freeaddrinfo(res_info);
}

/*
params: buf -- buffer for which checksum is to be calculated
        size-- size of the given buffer

ret val: checksum of the buffer

brief:  Calculate the checksum by adding 2 bytes at a time
        then folding it into a uint16_t

reference: http://cial.csie.ncku.edu.tw/course/2011_Spring_Router_Design/download/00-Short-description-of-the-Internet-checksum.pdf
*/
static uint16_t get_checksum(void* buf, int size) {
    uint16_t* msg = (uint16_t*) buf;
    uint32_t sum = 0;
    uint16_t checksum = 0;
    while(size > 1){
      sum += *msg;
      msg++;
      size -= 2;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    checksum = ~sum;
    return checksum;
}

/*
brief: show the right usage & possible options
*/
static void usage(){
    fprintf(stderr, "usage: sudo ./ping <address/hostname> [-c count] [-i interval(secs)] [-W timeout(mSecs)] \n");
    exit(-1);
}

/* Enter description of program */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#define HOSTLEN 256
#define SERVLEN 8
#define SINGLE_PROTOCOL 0
#define MAXLINE 64
#define MAX_IP_LEN (65536)
#define IP_PKT_SIZE (20)
/* Typedef for convenience */
typedef struct sockaddr SA;

/* Structure of an ICMP packet */
typedef struct __attribute__ ((packed)){
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t ID;
    uint16_t sequence;
    struct timeval timestamp;
    char data[56];
} ICMP;

static uint16_t pkt_count;
static uint16_t get_checksum(void*, int);
static void do_ping();

int main(int argc, char** argv) {
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    int c;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <address>\n", argv[0]);
        exit(1);
    }
    while((c = getopt(argc, argv, ":c:")) != -1) {
      switch(c) {
        case 'c':
          pkt_count = atoi(optarg);
          break;
      }
    }

    /* Check command line args */


    do_ping(argv[optind]);

    return 0;
}

static void do_ping(char *host){

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
    
    if(getaddrinfo(host, NULL, &addr_info, &res_info) < 0 ) {
      printf("ERROR in getaddrinfo\n"); 
    }
    if(!res_info) { 
      printf("ERROR in getaddrinfo\n"); 
    }

    // Open a connection
    int sockfd;
    sockfd = socket(res_info->ai_family, res_info->ai_socktype, res_info->ai_protocol); // Err handling reqd
    if(sockfd < 0) { 
      printf("ERROR in socket %d\n", errno);
    }

    struct timeval tx_time;

    int cnt = 0;
    while(1){
      // Send a ping packet to the server
      ICMP *tx_packet = (ICMP*)malloc(sizeof(ICMP));
      memset(tx_packet, 0, sizeof(ICMP));
      tx_packet->type = 0x8;
      tx_packet->code = 0x0; // ECHO REQUEST format
      tx_packet->ID = 0;
      tx_packet->sequence = cnt++;

      if(cnt > pkt_count){
        break;
      }
      if(gettimeofday(&tx_time, NULL) < 0){
        printf("Error in getting time");
      }

      memcpy(&tx_packet->timestamp, &tx_time, sizeof(tx_time));

      tx_packet->checksum = get_checksum(tx_packet, sizeof(ICMP));

      int optval = 64;
      setsockopt(sockfd, IPPROTO_IP, IP_TTL, &optval, sizeof(optval));

      uint8_t* rx_buffer = malloc(MAX_IP_LEN);
      if(sendto(sockfd, tx_packet, sizeof(ICMP), 0, res_info->ai_addr, res_info->ai_addrlen) < 0){
        fprintf(stderr, "%s\n", strerror(errno)); 
      }
      else { 
        int rx_len = recvfrom(sockfd, rx_buffer, MAX_IP_LEN, 0, NULL, NULL);
        struct timeval rx_time;
        if(gettimeofday(&rx_time, NULL) < 0){
          printf("Error in getting time");
        }

        struct packet *ip_packet = (struct packet*)rx_buffer;
        ICMP *rx_packet = (ICMP*)(rx_buffer + IP_PKT_SIZE);//get the icmp packet and ignore the IP packet

        printf("Echo Reply received from %s rtt : %ld uSec\n", host, (rx_time.tv_usec - rx_packet->timestamp.tv_usec));
        sleep(1);
      }
    }
    close(sockfd);
    freeaddrinfo(res_info);

}

//https://www.csee.usf.edu/~kchriste/tools/checksum.c
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

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

#define HOSTLEN 256
#define SERVLEN 8
#define SINGLE_PROTOCOL 0
#define MAXLINE 64

/* Typedef for convenience */
typedef struct sockaddr SA;

/* Structure of an ICMP packet */
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t ID;
    uint16_t sequence;
    char padding[56];
} ICMP;


uint16_t get_checksum(void*, int);

int main(int argc, char** argv) {
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    /* Gotta implement appropriate signal handling for SIGINT */

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <address>\n", argv[0]);
        exit(1);
    }

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
    int ret = getaddrinfo(argv[1], NULL, &addr_info, &res_info);
    if(ret < 0) { printf("ERROR in getaddrinfo\n"); }
    if(res_info == NULL) { printf("ERROR in getaddrinfo\n"); }

    printf("The address is : %s, the protocol is %d\n", argv[1], res_info->ai_protocol);
    // Open a connection
    int sockfd;
    sockfd = socket(res_info->ai_family, res_info->ai_socktype, res_info->ai_protocol); // Err handling reqd
    if(sockfd < 0) { 
      printf("ERROR in socket %d\n", errno);
    }

    int bytes;
    // Send a ping packet to the server
    ICMP *tx_packet;
    memset(tx_packet, 0, sizeof(ICMP));
    //memset(tx_packet->padding, 0, sizeof(tx_packet->padding));
    tx_packet->type = 0x8;
    tx_packet->code = 0x0; // ECHO REQUEST format
    tx_packet->ID = 0;
    tx_packet->sequence = 0;
    tx_packet->checksum = get_checksum(tx_packet, sizeof(ICMP));

    int optval = 64;
    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &optval, sizeof(optval));

    bytes = sendto(sockfd, tx_packet, sizeof(ICMP), 0, res_info->ai_addr, res_info->ai_addrlen);
    if(bytes < 0) { fprintf(stderr, "%s\n", strerror(errno)); }
    else { 
      ICMP *rx_packet = (ICMP*)malloc(sizeof(ICMP));
      printf("bytes sent : %d\n", bytes); 
      int rx_len = recvfrom(sockfd, rx_packet, sizeof(rx_packet), 0, NULL, NULL);
      printf("bytes rxed %d\n", rx_len);
    }

    close(sockfd);
    freeaddrinfo(res_info);

    return 0;
}

uint16_t get_checksum(void* buf, int size) {
    uint16_t* msg = (uint16_t*) buf;
    uint16_t checksum = 0;
    while(size){
      checksum += *msg;
      msg++;
      size -= 2;
    }
    printf("checksum is %x\n", checksum);
    printf("checksum is %x\n", ~checksum);
    //checksum = ((checksum & 0xff) << 8)| ((checksum>>8) &0xff);
    //checksum = htons(checksum);
    printf("checksum is %x\n", ~checksum);
    return ~checksum;
}

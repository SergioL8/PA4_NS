// YOUR CLIENT CODE
/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>
#include <sys/time.h>

#define BUFSIZE 16384 // 8192

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}


void send_file(char* filename, int sockfd, int clientlen, struct sockaddr_in clientaddr);
void receive_file(char* filename, int sockfd, struct sockaddr_in serveraddr, int serverlen);


int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char command[7];
    char filename[48];
    int words_read;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* get a message from the user */
    while(1) {
        bzero(buf, BUFSIZE);
        bzero(command, 7);
        bzero(filename, 48);
        printf("Please enter msg: ");
        fgets(buf, BUFSIZE, stdin);

        /* send the message to the server */
        serverlen = sizeof(serveraddr);

        /* Trim the inputs return/newline*/
        buf[strcspn(buf, "\r\n")] = '\0';
        words_read = sscanf(buf, "%6s %47s", command, filename); // Separate command and filename

        if (strcmp(command, "exit") == 0) {
            n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
            if (n < 0) 
            error("ERROR in sendto");
            close(sockfd);
            exit(0);
        } else if (strcmp(command, "get") == 0) {

            /* Check that file name was included with the command*/
            if (words_read < 2) { error("Error: No file name specified with delete command. \n"); }

            n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");

            receive_file(filename, sockfd, serveraddr, serverlen);
            continue;

        } else if (strcmp(command, "put") == 0) {

            /* Check that file name was included with the command*/
            if (words_read < 2) { error("Error: No file name specified with delete command. \n"); }
            n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            send_file(filename, sockfd, serverlen, serveraddr);
            continue;

        } else {
            n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
        }

        n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
        if (n < 0) error("ERROR in recvfrom");
        printf("Echo from server: %s", buf);
    }
    return 0;
}



void receive_file(char* filename, int sockfd, struct sockaddr_in serveraddr, int serverlen) {

    char buf[BUFSIZE+4];
    int file_size;
    int bytes_received  = 0;
    int n;
    uint32_t ack = 0;

    /* Open and create file to write to it */
    FILE* file = fopen(filename, "wb");
    if (file == NULL) error("Error opening file to write");

    n = recvfrom(sockfd, &file_size, sizeof(int), 0, &serveraddr, &serverlen);
    if (n < 0) error("ERROR in recvfrom");
    if (n != sizeof(int)) error("Did not receive correct file size");

    /* Send acknowledge of the first packet*/
    n = sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *) &serveraddr, serverlen);
    if (n < 0) error("ERROR in sendto");

    while (file_size > bytes_received) {

        /* Receive data */
        n = recvfrom(sockfd, buf, BUFSIZE+4, 0, &serveraddr, &serverlen);
        if (n < 0) error("ERROR in recvfrom");

        /* Parse ack number and payload*/
        uint32_t received_seq; memcpy(&received_seq, buf, 4);
        char* payload = buf + 4;
        int payload_size = n - 4;

        /* If received seq is the same as previous discard the data and resend ack*/
        if (received_seq <= ack) {
            n = sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            continue;
        }

        /* Write to file*/
        size_t bytes_written = fwrite(payload, 1, payload_size, file);
        if ((int)bytes_written != n-4) error("Error writting file");
        bytes_received += payload_size;

        /* Send the acknowledge*/
        ack = received_seq;
        n = sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *) &serveraddr, serverlen);
        if (n < 0) error("ERROR in sendto");
    }
    fclose(file);
}







void send_file(char* filename, int sockfd, int clientlen, struct sockaddr_in clientaddr) {
  
  int n;
  uint32_t ack;
  uint32_t expected_seq = 0;
  char packet[BUFSIZE +4];


  /* Open file */
  FILE *file = fopen(filename, "rb");
  if (file == NULL) error("ERROR opening file");

  /* Get file size */
  fseek(file, 0, SEEK_END);
  int file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  /* Turn on timeout for the socket */
  struct timeval tv = { .tv_sec = 0, .tv_usec = 4000};
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  /* Send initial packet with the file size*/
  while(1) {
    n = sendto(sockfd, &file_size, sizeof(file_size), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) error("ERROR in sendto");

    n = recvfrom(sockfd, &ack, sizeof(ack), 0, &clientaddr, &clientlen);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue; // no ACK arrived so retransmit
      } else {
        error("ERROR in recvfrom");
      }
    }
    
    
    if (ack == expected_seq) {
      expected_seq++;
      break;
    }
  }  

  size_t bytes_read;
  char buf[BUFSIZE];
  while ((bytes_read = fread(buf, 1, sizeof(buf), file)) > 0) {

    memcpy(packet, &expected_seq, sizeof(expected_seq));
    memcpy(packet+4, buf, bytes_read);

    while (1) {
      n = sendto(sockfd, packet, bytes_read+4, 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) error("ERROR in sendto");

      n = recvfrom(sockfd, &ack, sizeof(ack), 0, &clientaddr, &clientlen);
      if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          continue; // no ACK arrived so retransmit
        } else {
          error("ERROR in recvfrom");
        }
      }

      if (ack == expected_seq) {
        expected_seq++;
        break;
      }
    }
  }
  fclose(file);

  /* Turn of timeout for the socket */
  struct timeval tv0 = { .tv_sec = 0, .tv_usec = 0};
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv0, sizeof(tv0));
}
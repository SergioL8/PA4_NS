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
#include <arpa/inet.h>
#include <math.h>
// #include <openssl/md5.h>

#define BUFSIZE 4096
#define MAX_N_SERVERS 10

int get_ports(int ports[MAX_N_SERVERS]);
void put_request(int sock_fds[MAX_N_SERVERS], char *filename, int n_servers);
int open_socket(char* ip, int port);
void get_request(int sock_fds[MAX_N_SERVERS], char *filename, int n_servers);
void list_request();
// int hash_filename(char *filename);
unsigned long hash_filename(char *filename);
int recv_line(int fd, char *buf, int maxlen);


int main(int argc, char **argv) {

    /* Variable declaraton */
    char *ip_address = "127.0.0.1";
    char *request_type;
    char *filename;
    int ports[MAX_N_SERVERS];
    int n_ports;
    int sock_fds[MAX_N_SERVERS];
    int sock_fd;
    int connected_servers = 0;

    
    /* check command line arguments */
    if (argc < 2) { fprintf(stderr,"usage: %s <command> <filename>\n", argv[0]); return -1; }
    request_type = argv[1];
    filename = (argc >= 3) ? argv[2] : NULL;

    /* get ports from config file */
    n_ports = get_ports(ports);

    /* open a scoket per server */
    for (int i = 0; i < n_ports; i++) {
        if ((sock_fd = open_socket(ip_address, ports[i])) >= 0) {
            sock_fds[connected_servers++] = sock_fd;
        }
    }

    if (strcmp(request_type, "put") == 0) {
        if (connected_servers < n_ports) { printf("%s put failed\n", filename); return 1; }
        put_request(sock_fds, filename, connected_servers);
    } else if (strcmp(request_type, "get") == 0) {
        get_request(sock_fds, filename, connected_servers);
    } else if (strcmp(request_type, "list") == 0) {
        list_request();
    } else {
        printf("Request type not recognize, please choose between put, get or list\n");
        return 1;
    }
    return 0;
}


int get_ports(int ports[10]) {
    FILE *fp = fopen("dfc.conf", "r");
    if (!fp) { printf("Error opening .conf file\n"); return 0; }

    char line[256];
    int i = 0;

    while(fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%*s %*s %*[^:]:%d", &ports[i])) {
            i++;
        }
    }
    fclose(fp);
    return i;
}


int open_socket(char* ip, int port) {

    struct sockaddr_in server_addr;
    int client_fd;

    /* open socket */
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) { printf("Error opening socket. \n"); return -1; } // Check socket was opened successfully

    /* Set server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr); // connect to localhost

    /* Connect to server */
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error connecting socket. \n"); return -1;
    }

    return client_fd;
}


// int hash_filename(char *filename) {
//     unsigned char digest[MD5_DIGEST_LENGTH]; // 16 bytes
//     MD5((unsigned char *)filename, strlen(filename), digest);
//     return digest[0] % 4; // use first byte for the modulus
// }


unsigned long hash_filename(char *filename) {
    unsigned long hash = 5381;
    int c;
    while ((c = *filename++))
        hash = ((hash << 5) + hash) + c;
    return hash % 4;
}


int recv_line(int fd, char *buf, int maxlen) {
    int idx = 0;
    char ch;
    while (idx < maxlen - 1) {
        int n = recv(fd, &ch, 1, 0);
        if (n <= 0) return n;   // 0 = closed, -1 = error
        if (ch == '\n') break;  // stop at delimiter, don't store it
        buf[idx++] = ch;
    }
    buf[idx] = '\0';
    return idx;
}


void put_request(int sock_fds[MAX_N_SERVERS], char *filename, int n_servers) {

    /* variable declaration */
    long f_size;
    long part_size;
    int bytes_read;
    int total_bytes_read = 0;
    int bytes_to_read;
    int hash = 0;
    char buffer[BUFSIZE];
    char command[1024];
    char header[1024];
    int parts[2];

    /* send put request pakcet */
    sprintf(command, "put %s\n", filename);
    for (int i = 0; i < n_servers; i++) {
        send(sock_fds[i], command, strlen(command), 0);
    }
    
    /* open file */
    FILE *fp = fopen(filename, "rb");
    if (!fp) { printf("Error opening file in put\n"); return; }

    /* get the file size and part size */
    fseek(fp, 0, SEEK_END);
    f_size = ftell(fp);
    part_size = ceil((double)f_size / n_servers);
    rewind(fp);

    /* hash filname */
    hash = hash_filename(filename);

    /* clean string */
    char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    /* iterate over every server */
    for (int i = 0; i < n_servers; i++) {

        /* get parts for this server */
        parts[0] = (i - hash + n_servers) % n_servers;
        parts[1] = (parts[0] + 1) % n_servers;

        printf("Server: %d; parts: (%d, %d)\n", i+1, parts[0], parts[1]);

        for (int j =  0; j < 2; j++) {
            long actual_size;
            /* The last part will probably have a smaller size*/
            if (parts[j] == n_servers  -1) {
                actual_size = f_size - (n_servers -1) * part_size;
            } else {
                actual_size = part_size;
            }

            /* send header */
            sprintf(header, "%s_%d %ld\n", base, parts[j]+1, actual_size);
            send(sock_fds[i], header, strlen(header), 0);

            /* seek to the part position */
            fseek(fp, parts[j]*part_size, SEEK_SET);

            /* chunk_size might be smaller than the buf size in first iteration */
            bytes_to_read = fmin(actual_size, BUFSIZE);

            /* Read until we reach EOF or bytes_to_read == 0 for current chunk */
            while((bytes_read = fread(buffer, 1, bytes_to_read, fp)) > 0) {
                send(sock_fds[i], buffer, bytes_read, 0); // Send the data
                total_bytes_read += bytes_read;
                bytes_to_read = fmin(actual_size - total_bytes_read, BUFSIZE); // next read is 4096 
            }
            total_bytes_read = 0;
        }
        close(sock_fds[i]);
    }
    return;
}


void get_request(int sock_fds[MAX_N_SERVERS], char *filename, int n_servers) {

    /* variable declaration */
    char command[1024];
    char header[1024];
    char part_name[1024];
    char buffer[BUFSIZE];
    long part_size;

    char *base = strrchr(filename, '/');
    base = base ? base +1 : filename;

    /* send get request pakcet to all servers */
    sprintf(command, "get %s\n", base);
    for (int i = 0; i < n_servers; i++) {
        send(sock_fds[i], command, strlen(command), 0);
    }

    /* receive parts from each server */
    for (int i = 0; i < n_servers; i++) {

        /* keep reading headers */
        while(recv_line(sock_fds[i], header, sizeof(header)) > 0) {

            /* divide part_name and part size */
            sscanf(header, "%s %ld", part_name, &part_size);

            /* open file to write this part */
            FILE *fp = fopen(part_name, "wb");
            if (!fp) { printf("Error opening file in while: %s\n", part_name); break; }

            /* receive exactly part_size bytes */
            long bytes_received = 0;
            while (bytes_received < part_size) {
                long remaining = part_size - bytes_received;
                int to_read = (remaining < BUFSIZE) ? remaining : BUFSIZE;
                int n = recv(sock_fds[i], buffer, to_read, 0);
                if (n <= 0) break;
                fwrite(buffer, 1, n, fp);
                bytes_received += n;
            }
            fclose(fp);
        }
        close(sock_fds[i]);
    }

    /* create final file */
    FILE *fp = fopen(base, "wb");
    if (!fp) { printf("Error opening main final file: %s\n", filename); return; }

    /* strip extension from filename */
    char *dot = strrchr(base, '.'); // find the last '.'
    if (dot) *dot = '\0';

    for (int j = 1; ; j++) {

        sprintf(part_name, "%s_%d", base, j);

        /* open part file */
        FILE *pfp = fopen(part_name, "rb");
        if (!pfp) {
            printf("Error opening part file: %s\n", part_name);
            break;
        }

        int bytes_read = 0;
        while((bytes_read = fread(buffer, 1, BUFSIZE, pfp)) > 0) {
            fwrite(buffer, 1, bytes_read, fp);
        }
        fclose(pfp);
        remove(part_name);
    }
    fclose(fp);
    return;
}

void list_request() {
    return;
}
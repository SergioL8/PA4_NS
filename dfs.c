// YOUR SERVER CODE
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <dirent.h>

#define BUFSIZE 4096


typedef struct {
    int sock_fd;
    char* server_dir;
} ThreadArgs;


void *handle_client_thread(void *arg);
void put_request(int client_fd, char* server_dir);
void get_request(int client_fd, char* server_dir, char* filename);
int recv_line(int fd, char *buf, int maxlen);


int main(int argc, char **argv) {

    // Variable Declaraction
    int port_number, server_fd;
    struct sockaddr_in addr;

    // Check correct number of command line arguments
    if (argc != 3) {
        printf("Wrong of command line argument. \n Usage: ./dfs {SEVER_DIRECTORY} {PORT NUMBER} \n");
        return -1;
    }

    char* server_dir = argv[1]; // Read path to server directory from command line argument
    port_number = atoi(argv[2]); // Read port number from command line arguments

    server_fd = socket(AF_INET, SOCK_STREAM, 0); // Open theh socket
    if (server_fd == -1) { printf("Error opening socket. \n"); return -1; } // Check socket was opened successfully

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // Allow to reuse local address

    memset(&addr, 0, sizeof(addr)); // Clear the entire structue 
    addr.sin_family = AF_INET; // AF_INET -> IPv4
    addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any network interface
    addr.sin_port = htons(port_number); // Set the port number

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Error binding. \n");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 10) < 0) {
        printf("Error listening. \n");
        close(server_fd);
        return -1;
    }

    printf("Server listening on port %d\n", port_number);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        /* Accept client */
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("Error in client accept.");
            continue;
        }

        /* Create thread */
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->sock_fd = client_fd;
        args->server_dir = server_dir;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client_thread, args);
        pthread_detach(thread);
    }
    close(server_fd);
    return 0;
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


void *handle_client_thread(void *arg) {

    /* Variable declaraion */
    char command[8];
    char filename[256]; filename[0] = '\0'; // Default to empty in case there's no filename for list
    char buffer[4096];

    /* Unpack the arguments */
    ThreadArgs *args = (ThreadArgs *)arg;
    int client_fd = args->sock_fd;
    char* server_dir = args->server_dir;
    free(args);
    
    /* Receive command */
    if (recv_line(client_fd, buffer, sizeof(buffer)) <= 0) {
        close(client_fd);
        return NULL;
    }

    printf("Command received in handle client: %s\n", buffer);

    /* Separate command and filename */
    sscanf(buffer, "%s %s", command, filename);

    /* Select action according to command */
    if (strcmp(command, "put") == 0) {
        put_request(client_fd, server_dir);
    } else if (strcmp(command, "get") == 0) {
        get_request(client_fd, server_dir, filename);
    } else if (strcmp(command, "list") == 0) {
        char *body = "Command received is list\n";
        send(client_fd, body, strlen(body), 0); // Send response
    } else {
        char *body = "Error 400. Command not supported.\n";
        send(client_fd, body, strlen(body), 0); // Send response
    }
    close(client_fd);
    return NULL;
}


void put_request(int client_fd, char* server_dir) {

    /* Variable declaration */
    int bytes_received;
    int total_bytes_received = 0;
    long part_size;
    char buffer[BUFSIZE];
    char header[1024];
    char part_name[256];
    char path[1024];
    
    for (int i = 0; i < 2; i++) {

        /* receive header 1 */
        if (recv_line(client_fd, header, sizeof(header)) <= 0) {
            printf("Error reading header\n"); return;
        }

        printf("Server dir: %s | Header: %s\n", server_dir, header);
        
        /* get the part_name and the part size */
        sscanf(header, "%s %ld", part_name, &part_size);

        /* create the file*/
        sprintf(path, "%s/%s", server_dir, part_name);
        FILE *fp = fopen(path, "wb");
        if (!fp) { printf("Error opening file\n"); return; }

        /* Read exactly the size of the part for this server */
        while(total_bytes_received < part_size) {

            /* Compute how much we have to read in case we are at the end of the part */
            long remaining = part_size - total_bytes_received;
            int to_read = (remaining < BUFSIZE) ? remaining : BUFSIZE;

            /* Receive data */
            bytes_received = recv(client_fd, buffer, to_read, 0);
            if (bytes_received < 0) { printf("Error in recv"); close(client_fd); fclose(fp); return; }

            /* Write to file */
            fwrite(buffer, 1, bytes_received, fp);
            total_bytes_received += bytes_received;
        }
        fclose(fp);
        total_bytes_received = 0;
    }
}



void get_request(int client_fd, char* server_dir, char* filename) {

    /* variable declaration */
    char header[1024];
    char path[1024];
    struct stat st;
    char buffer[BUFSIZE];
    long bytes_read = 0;

    /* remove extension */
    char *dot = strrchr(filename, '.'); // find the last '.'
    if (dot) *dot = '\0';               // replace it with null terminator

    /* open directory*/
    DIR *dir = opendir(server_dir);
    if (!dir) { printf("Error opening directory: %s\n", server_dir); return; }

    /* loop over the directory */
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, filename, strlen(filename)) == 0) {

            /* Create path and open with stat*/
            sprintf(path, "%s/%s", server_dir, entry->d_name);
            stat(path, &st);

            /* open file */
            FILE *fp = fopen(path, "rb");
            if (!fp) { printf("Error opening file: %s\n", path); continue; }

            /* send header: "partname size\n" */
            sprintf(header, "%s %lld\n", entry->d_name, st.st_size);
            send(client_fd, header, strlen(header), 0);

            /* read and send file */
            while ((bytes_read = fread(buffer, 1, BUFSIZE, fp)) > 0) {
                send(client_fd, buffer, bytes_read, 0);
            }
        }
    }
    closedir(dir);
}
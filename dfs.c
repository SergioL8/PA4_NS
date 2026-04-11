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


void *handle_client_thread(void *arg);
void send_response(int client_fd, int code, char *body, char *content_type);
void send_file(int client_fd, FILE * fp, char* path);


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
        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        *pclient = client_fd;

        pthread_create(&tid, NULL, handle_client_thread, pclient);
        pthread_detach(tid);
    }
    close(server_fd);
    return 0;
}


void *handle_client_thread(void *arg) {

    /* Variable declaraion */
    struct stat info;
    char command[8];
    char filename[256]; filename[0] = '\0'; // Default to empty in case there's no filename for list

    /* Unpack the arguments */
    int client_fd = *(int *)arg;
    free(arg);
    
    /* Receive data */
    char buffer[4096];
    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        printf("Error in recv");
        close(client_fd);
        return NULL;
    }

    /* Null-terminate buffer so we can treat it like a string */
    buffer[bytes_received] = '\0';

    /* Separate command and filename */
    int count = sscanf(buffer, "%s %s", command, filename);

    /* Print command and filename*/
    printf("Command: %s\n File name: %s\n", command, filename);

    /* Select action according to command */
    if (strcmp(command, "get") == 0) {
        printf("Command received is get\n");
    } else if (strcmp(command, "put") == 0) {
        printf("Command received is put\n");
    } else if (strcmp(command, "list") == 0) {
        printf("Command received is list\n");
    } else {
        char *body = "Error 400. Command not supported.\n";
        send_response(client_fd, 400, body, "text/plain");
        send(client_fd, body, strlen(body), 0); // Send response
    }
    close(client_fd);
    return NULL;


    
    // char method[16];
    // char uri[256];
    // char version[16];
    // int parts = sscanf(buffer, "%15s %255s %15s", method, uri, version);

    // printf("Uri: %s \n", uri);

    // if (parts != 3) {
    //     char *body = "The request could not be parsed or is malformed.\n";
    //     send_response(client_fd, 400, body, "text/plain");
    // } else if (strcmp(method, "GET")) { // strcmp returns 0 if they are equal
    //     char *body = "A method other than GET was requested.\n";
    //     send_response(client_fd, 405, body, "text/plain");
    // } else if (strcmp(version, "HTTP/1.1") && strcmp(version, "HTTP/1.0")) {
    //     char *body = "An HTTP version other than 1.1 or 1.0 was requested.\n";
    //     send_response(client_fd, 505, body, "text/plain");
    
    // } else {
    //     // Create the path
    //     char path[512];
    //     snprintf(path, sizeof(path), "./www%s", uri);

    //     // Check if the path is a folder
    //     if (stat(path, &info) == 0 && S_ISDIR(info.st_mode)) {
            
    //         strncat(path, "index.html", sizeof(path) - strlen(path) - 1); // Append index.html to path

    //         FILE *testfp = fopen(path, "rb");
    //         if (testfp == NULL) { // File doesn't exists so we change path to index.htm try bellow
    //             char *p = strstr(path, "index.html"); // Remove index.html from path
    //             if (p != NULL) *p = '\0';
    //             strncat(path, "index.htm", sizeof(path) - strlen(path) - 1); // Append index.htm to path
    //         } else {
    //             fclose(testfp); // File exists so we close this test since we are going to open it bellow again
    //         }
    //     }

    //     // Check if the file is accessible/exists
    //     FILE *fp = fopen(path, "rb");
    //     if (fp == NULL) {
    //         if (errno == EACCES) {
    //             char *body = "The requested file cannot be access due to a file permission issue.\n";
    //             send_response(client_fd, 403, body, "text/plain");
    //         } else {
    //             char *body = "Error 404. The requested file canont be found in the document tree.\n";
    //             send_response(client_fd, 404, body, "text/plain");
    //         }
    //     } else {
    //         send_file(client_fd, fp, path);
    //         fclose(fp);
    //     }
    // }
    // close(client_fd); // 6. Close client
    // return NULL;
}





void send_response(int client_fd, int code, char *body, char *content_type) {

    char response[512];
    int body_length = strlen(body);
    int response_length = 0;

    switch(code) {
        case 200:
            response_length = snprintf(
                response,
                sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                content_type,
                body_length,
                body
            );
        break;

        case 400:
            response_length = snprintf(
                response,
                sizeof(response),
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                content_type,
                body_length,
                body
            );
        break;

        case 403:
            response_length = snprintf(
                response,
                sizeof(response),
                "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                content_type,
                body_length,
                body
            );
        break;

        case 404:
            response_length = snprintf(
                response,
                sizeof(response),
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                content_type,
                body_length,
                body
            );
        break;

        case 405:
            response_length = snprintf(
                response,
                sizeof(response),
                "HTTP/1.1 405 Method Not Allowed \r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                content_type,
                body_length,
                body
            );
        break;

        case 505:
            response_length = snprintf(
                response,
                sizeof(response),
                "HTTP/1.1 505 HTTP Version Not Supported \r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                content_type,
                body_length,
                body
            );
        break;
    }
    send(client_fd, response, response_length, 0); // Send response
}




void send_file(int client_fd, FILE * fp, char* path) {

    // Variable declaration
    char *content_type;
    char chunk[4096];
    long file_size;
    size_t bytes_read; 
    int response_length = 0;


    // Get the content type
    char *ext = strrchr(path, '.');
    if (ext == NULL) content_type = "text/html";
    else if (strcmp(ext, ".html") == 0) content_type = "text/html";
    else if (strcmp(ext, ".txt")  == 0) content_type = "text/plain";
    else if (strcmp(ext, ".png")  == 0) content_type = "image/png";
    else if (strcmp(ext, ".gif")  == 0) content_type = "image/gif";
    else if (strcmp(ext, ".jpg")  == 0) content_type = "image/jpg";
    else if (strcmp(ext, ".ico")  == 0) content_type = "image/x-icon";
    else if (strcmp(ext, ".css")  == 0) content_type = "text/css";
    else if (strcmp(ext, ".js")   == 0) content_type = "application/javascript";
    else content_type = "text/plain";

    // Get size of the file
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Build reponse
    char response[512];
    response_length = snprintf(
        response,
        sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "\r\n",
        content_type,
        file_size
    );

    // Send
    send(client_fd, response, response_length, 0); // Send response

    // Loop to finish reading file
    while ((bytes_read = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        send(client_fd, chunk, bytes_read, 0);
    }
}
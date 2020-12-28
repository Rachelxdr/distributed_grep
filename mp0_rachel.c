#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#define TARGET "172.17.0.2"
#define PORT "9000"
#define BACK_LOG 10
#define MAX_INPUT_SIZE 50
#define MAX_OUTPUT_SIZE 500

void* client_start(void* arg) {
    char* cmd = malloc(MAX_INPUT_SIZE);
    cmd = (char*) arg;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int s = getaddrinfo(TARGET, PORT, &hints, &res);

    if (s != 0) {
        fprintf(stderr,"getaddrinfo client: %s\n", gai_strerror(s));
        pthread_exit(NULL);
    }
    struct addrinfo* util = res;
    int server_fd;
    while (util != NULL) {
        server_fd = socket(util->ai_family, util->ai_socktype, 0);
        if (server_fd == -1) {
            perror("client socket");
            util = util->ai_next;
            continue;
        }

        int connect_ret = connect(server_fd, util->ai_addr, util->ai_addrlen);
        if (connect_ret == -1) {
            perror("client connect");
            util = util->ai_next;
            continue;
        }
        break;
    }

    if (util == NULL) {
        perror("Failed to create client socket");
        exit(1);
    }
   
    ssize_t bytes_sent = send(server_fd, cmd, MAX_INPUT_SIZE, 0);
    
    if (bytes_sent == -1) {
        perror("client send");
        exit(1);
    }
    char result[MAX_OUTPUT_SIZE];
    ssize_t bytes_received;
    while ((bytes_received = recv(server_fd, result, MAX_OUTPUT_SIZE, 0)) != 0) {
        result[bytes_received] = '\0';
        printf("%s", result);
        bzero(result, sizeof(result));
    }

   
    close(server_fd);
    pthread_exit(NULL);
}


void* server_start(void* arg) {
    int s;
    int sock_fd;
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    s = getaddrinfo(NULL, PORT, &hints, &result);

    if (s) {
        fprintf(stderr, "getaddrinfo server: %s\n", gai_strerror(s));
        exit(1);
    }

    struct addrinfo* util = result;
    while (util != NULL) {
        sock_fd = socket(util->ai_family, util->ai_socktype, util->ai_protocol);
        
        if (sock_fd < 0) {
            perror("server socket");
            util = util->ai_next;
            continue;
        }

        int optval = 1;
        int retval = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        if (retval == -1) {
            perror("server setsockopt");
            exit(1);
        }


        if (bind(sock_fd, result->ai_addr, result->ai_addrlen) != 0) {
            perror("server bind");
            util = util->ai_next;
            continue;
        }
        break;

    }

    if (util == NULL) {
        perror("Server failed to bind");
        exit(1);
    }

    if (listen(sock_fd, BACK_LOG) != 0) { 
        perror("server listen");

        exit(1);
    }

    struct sockaddr_in* result_addr = (struct sockaddr_in*) util->ai_addr;
    printf("Server setup completed, sock_fd: %d, internet address: %s, port: %d\n", sock_fd, inet_ntoa(result_addr->sin_addr), ntohs(result_addr->sin_port));

    while(1) {

        socklen_t addr_size;
        struct sockaddr incoming_info;
        addr_size = sizeof(incoming_info);
        int client_socket = accept(sock_fd, &incoming_info, &addr_size);

        char recv_buffer[128];
        memset(recv_buffer, '\0', MAX_INPUT_SIZE);

        
        read(client_socket, recv_buffer, MAX_INPUT_SIZE);
        printf("[DEBUG] %s\n", recv_buffer);
        
        FILE* fd = popen(recv_buffer, "r");
        if (!fd) {
            perror("popen failed!");
            exit(1);
        }
        char result[1024];
        while (fgets(result, 100, fd) != NULL) {
            ssize_t bytes_sent = send(client_socket, result, strlen(result), 0);
            if (bytes_sent == -1) {
                perror("server send response failed");
                exit(1);
            }
        }
        close(client_socket);

    }
    
}


int main() {
    pthread_t client;
    pthread_t server;

    if (pthread_create(&server, NULL, server_start, (void*)0) != 0) {
        perror("server thread create");
        exit(1);
    }

    while (1) {
        char* stdin_buffer = malloc(MAX_INPUT_SIZE);
        fgets(stdin_buffer, MAX_INPUT_SIZE, stdin);
        size_t len = strlen(stdin_buffer);
        if (stdin_buffer[len - 1] == '\n') {
            stdin_buffer[len - 1] = '\0';
        }
        if (strcmp(stdin_buffer, "exit") == 0){
            pthread_join(client, NULL);
            break;
        } else {
            if (pthread_create(&client, NULL, client_start, (void*)stdin_buffer)) {
                perror("client thread create");
                exit(1);
            }
        }
    }
}
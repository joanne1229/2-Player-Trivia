/*** socket/demo2/client.c ***/
/*
    NAME: JOANNE WANG
    PLEDGE: I pledge my honor that I have abided by the Stevens Honor Syste,
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <getopt.h>

#define DEFAULT_IP_ADDRESS "127.0.0.1"
#define DEFAULT_PORT 25555
#define MAX_BUFFER 1024


void parse_connect(int argc, char** argv, int* server_fd) {
    // Default values
    char *ip_address = DEFAULT_IP_ADDRESS;
    int port_number = DEFAULT_PORT;

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
        switch (opt) {
            case 'i':
                ip_address = optarg;
                break;
            case 'p':
                port_number = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n", argv[0]);
                printf("-i IP_address Default to \"%s\";\n", DEFAULT_IP_ADDRESS);
                printf("-p port_number Default to %d;\n", DEFAULT_PORT);
                printf("-h Display this help info.\n");
                exit(0);
            default:
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                exit(1);
        }
    }

    // Create a socket
    *server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_fd == -1) {
        perror("Error creating socket");
        exit(1);
    }

    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = inet_addr(ip_address);

    // Connect to the server
    if (connect(*server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to server");
        exit(1);
    }

    printf("Connected to the server at %s:%d\n", ip_address, port_number);
}

int main(int argc, char *argv[]) {
    int server_fd;
    char buffer[1024];
    fd_set read_fds;
    int max_fd;

    parse_connect(argc, argv, &server_fd);

    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    max_fd = (server_fd > STDIN_FILENO) ? server_fd : STDIN_FILENO;

    while (1) {
        fd_set tmp_fds = read_fds;
        if (select(max_fd + 1, &tmp_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }

        if (FD_ISSET(server_fd, &tmp_fds)) {
            memset(buffer, 0, MAX_BUFFER);
            int bytes_received = recv(server_fd, buffer, MAX_BUFFER - 1, 0);
            if (bytes_received <= 0) {
                break;
            }
            printf("[Server]: %s", buffer);
        }

        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            memset(buffer, 0, MAX_BUFFER);
            fgets(buffer, MAX_BUFFER - 1, stdin);
            printf("[Client]: %s", buffer);
            fflush(stdout);
            send(server_fd, buffer, strlen(buffer), 0);
        }
    }




    close(server_fd);

    return 0;
}

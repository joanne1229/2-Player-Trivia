/*** socket/demo2/server.c ***/
/*
    NAME: Joanne Wang
    PLEDGE: I pledge my honor that I have abided by the Stevens Honor Syste,
*/
                    
/*** socket/demo2/server.c ***/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <getopt.h>

#define MAX_CONN 2 
#define DEFAULT_QUESTION_FILE "questions.txt"
#define DEFAULT_IP_ADDRESS "127.0.0.1"
#define DEFAULT_PORT 25555

struct Entry {
    char prompt[1024];
    char options[3][50];
    int answer_idx;
};

struct Player {
    int fd;
    int score;
    char name[128];
};


int prep_fds( fd_set* active_fds, int server_fd, int* client_fds ) {
    FD_ZERO(active_fds);
    FD_SET(server_fd, active_fds);

    int max_fd = server_fd;
    for (int i = 0; i < MAX_CONN; i ++) {
        if (client_fds[i] > -1)
            FD_SET(client_fds[i], active_fds);
        if (client_fds[i] > max_fd)
            max_fd = client_fds[i];
    }
    return max_fd;
}
int read_questions(struct Entry* arr, char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening question file");
        exit(1);
    }

    int num_questions = 0;
    char line[1024];

    while (fgets(line, sizeof(line), file)) {
        // Read prompt
        strcpy(arr[num_questions].prompt, line);

        // Read options
        fgets(line, sizeof(line), file);
        sscanf(line, "%s %s %s", arr[num_questions].options[0], arr[num_questions].options[1], arr[num_questions].options[2]);

        // Read answer index
        fgets(line, sizeof(line), file);
        sscanf(line, "%d", &arr[num_questions].answer_idx);

        num_questions++;

        // Skip empty line
        fgets(line, sizeof(line), file);
    }

    fclose(file);
    return num_questions;
}

void send_question(int fd, struct Entry question, int question_no) {
    char buffer[2048];
    snprintf(buffer, sizeof(buffer), "\nQuestion %d: %sPress 1: %s\nPress 2: %s\nPress 3: %s\n",
             question_no, question.prompt, question.options[0], question.options[1], question.options[2]);
    send(fd, buffer, strlen(buffer), 0);
}

void printscreen(struct Entry question, int question_no) {
    printf("Question %d: %s", question_no, question.prompt);
    printf("1: %s\n", question.options[0]);
    printf("2: %s\n", question.options[1]);
    printf("3: %s\n", question.options[2]);
}

int check_answer(char* answer, struct Entry question) {
    int selected_option = atoi(answer) - 1; // Convert answer to option index
    return (selected_option == question.answer_idx) ? 1 : 0; // Correct answer: 1, Incorrect answer: 0
}

void game_loop(int client_fds[], struct Entry questions[], int num_questions, struct Player players[], int num_players, fd_set* active_fds) {
    for (int question_index = 0; question_index < num_questions; question_index++) {
        struct Entry current_question = questions[question_index];
        for (int i = 0; i < num_players; i++) {
            send_question(client_fds[i], current_question, question_index + 1);
        }
        printscreen(current_question, question_index + 1);

        for (int i = 0; i < num_players; i++) {
            if (FD_ISSET(client_fds[i], active_fds)) {
                char answer[50];
                int bytes_received = recv(client_fds[i], answer, sizeof(answer), 0);
                if (bytes_received > 0) {
                    int score_change = check_answer(answer, current_question) ? 1 : -1;
                    players[i].score += score_change;
                } else {
                    printf("Connection lost with player %s.\n", players[i].name);
                    close(client_fds[i]); // Close the connection
                }
            }
        }

        //construct the broadcast message with the correct answer
        char broadcast_message[1024];
        snprintf(broadcast_message, sizeof(broadcast_message), "The correct answer is: %s\n", current_question.options[current_question.answer_idx]);

        //broadcast the message with correct answer
        for (int j = 0; j < num_players; j++) {
            send(client_fds[j], broadcast_message, strlen(broadcast_message), 0);
        }
    }


    // Determine the winner
    int max_score = -1;
    int winner_index = -1;
    for (int i = 0; i < num_players; i++) {
        if (players[i].score > max_score) {
            max_score = players[i].score;
            winner_index = i;
        }
    }

    // Announce the winner
    if (winner_index != -1) {
        printf("Congrats, %s! You won with a score of %d.\n", players[winner_index].name, players[winner_index].score);
    }

    // Close all connections
    for (int i = 0; i < num_players; i++) {
        close(client_fds[i]);
    }
}


int main(int argc, char *argv[]) {
    // Default values
    char *question_file = DEFAULT_QUESTION_FILE;
    char *ip_address = DEFAULT_IP_ADDRESS;
    int port_number = DEFAULT_PORT;
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "f:i:p:h")) != -1) {
        switch (opt) {
            case 'f':
                question_file = optarg;
                break;
            case 'i':
                ip_address = optarg;
                break;
            case 'p':
                port_number = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n", argv[0]);
                printf("-f question_file Default to \"%s\";\n", DEFAULT_QUESTION_FILE);
                printf("-i IP_address Default to \"%s\";\n", DEFAULT_IP_ADDRESS);
                printf("-p port_number Default to %d;\n", DEFAULT_PORT);
                printf("-h Display this help info.\n");
                return 0;
            default:
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                return 1;
        }
    }
    int    server_fd;
    int    client_fd;
    int    in_fd; 
    struct sockaddr_in server_addr;
    struct sockaddr_in in_addr;
    socklen_t addr_size = sizeof(in_addr);

    /* STEP 1
        Create and set up a socket
    */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(25555);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* STEP 2
        Bind the file descriptor with address structure
        so that clients can find the address
    */
    bind(server_fd,
            (struct sockaddr *) &server_addr,
            sizeof(server_addr));

    /* STEP 3
        Listen to at most 2 incoming connections
    */
   if (listen(server_fd, MAX_CONN) == 0) {
        printf("Welcome to 392 Trivia!\n");
    } else {
        perror("Error listening on socket");
        exit(1);
    }
    
    /* STEP 4
        Accept connections from clients
        to enable communication
    */
    fd_set active_fds; 
    int client_fds[MAX_CONN]; 
    int nconn = 0; 
    struct Entry questions[50];
    int num_questions = read_questions(questions, DEFAULT_QUESTION_FILE);

    printf("Loaded %d questions from %s\n", num_questions, question_file);

    // Accept connections from clients 
    for (size_t i = 0; i < MAX_CONN; i++) {
        client_fds[i] = -1;
    }

    printf("Waiting for players to join...\n");

    // for (size_t i = 0; i < MAX_CONN; i++) client_fds[i] = -1;

    char* receipt   = "Please type your name:\n";
    int   recvbytes = 0;
    char  buffer[1024];
    struct Player players[MAX_CONN];
    int num_players = 0;
    while (1) {
        int max_fd = prep_fds(&active_fds, server_fd, client_fds);
        // Monitor file descriptors for activity
        select(max_fd + 1, &active_fds, NULL, NULL, NULL);
        // Incoming connection detected
        if (FD_ISSET(server_fd, &active_fds)) {
            in_fd = accept(server_fd, (struct sockaddr*) &in_addr, &addr_size);
            if (nconn == MAX_CONN) {
                close(in_fd);
                fprintf(stderr, "Max connection reached!\n");
            } else {
                for (int i = 0; i < MAX_CONN; i++) { 
                    if (client_fds[i] == -1) {
                        client_fds[i] = in_fd; 
                        break;
                    }
                }
                printf("New connection detected!\n");
                nconn++;

                // Send the message to type the name to the client
                send(in_fd, receipt, strlen(receipt), 0);
            }
        }
        
        // Process messages from clients
        for (int i = 0; i < MAX_CONN; i++) {
            if (client_fds[i] != -1 && FD_ISSET(client_fds[i], &active_fds)) {
                recvbytes = read(client_fds[i], buffer, sizeof(buffer));
                if (recvbytes == 0) {
                    printf("Lost connection!\n"); 
                    close(client_fds[i]); 
                    client_fds[i] = -1; 
                    nconn--;
                    for (int j = 0; j < num_players; j++) {
                        if (players[j].fd == client_fds[i]) {
                            players[j] = players[num_players - 1];
                            num_players--;
                            break;
                        }
                    }
                } else {
                    buffer[recvbytes] = '\0';
                    printf("[Client]: %s\n", buffer);

                    // Send greeting message to client
                    printf("Hi %s!\n", buffer);
                    strcpy(players[num_players].name, buffer);
                    players[num_players].fd = client_fds[i];
                    players[num_players].score = 0;
                    num_players++;
                    if (num_players == nconn) {
                        printf("The game starts now!\n");
                        game_loop(client_fds, questions, num_questions, players, num_players,&active_fds);
 
                    }
                }  
            }
        }
    }

    printf("Ending...\n");
    close(server_fd);

    return 0;
}
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>



#define PORT "58001"
#define DEFAULT_PORT "58033"

int fd, errcode;
ssize_t n;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr;
char buffer[128];
char command[128];

// ./client -n "tejo.tecnico.ulisboa.pt" -p 58001

void parse_arguments(int argc, char *argv[], char **ASIP, char **ASport) {
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            *ASIP = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            *ASport = argv[++i];
        }
    }
}

void get_command(char *command, char *buffer) {
    int i = 0;
    while (buffer[i] != ' ' && buffer[i] != '\n') {
        command[i] = buffer[i];
        i++;
    }
    command[i] = '\0';
}

int main(int argc, char *argv[])
{
    char *ASIP = "localhost"; // Default IP
    char *ASport = DEFAULT_PORT; // Default port

    parse_arguments(argc, argv, &ASIP, &ASport);

    // Create a UDP socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
        exit(1);

    // Configure server address
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    // Use getaddrinfo to obtain address information
    errcode = getaddrinfo(ASIP, ASport, &hints, &res);
    if (errcode != 0)
        exit(1);

    // Wait for user input and process command
    while(1) {
        fgets(buffer, 128, stdin);
        
        get_command(command, buffer);

        if (strcmp(command, "login"))
            login_command(buffer);
        
        else if (strcmp(command, "list"))
            list_command(buffer);

        if (strcmp(buffer, "logout") == 0)
            logout_command(buffer);

    }

    // Send a message to the server
    n = sendto(fd, "Hello!\n", 7, 0, res->ai_addr, res->ai_addrlen);
    if (n == -1)
        exit(1);

    // Receive the echoed message from the server    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, 128, 0, (struct sockaddr *)&addr, &addrlen);
    if (n == -1)
        exit(1);

    // Print the echoed message
    write(1, "echo: ", 6);
    write(1, buffer, n);

    freeaddrinfo(res);
    close(fd);

    return 0;
}

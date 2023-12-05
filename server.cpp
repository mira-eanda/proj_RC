#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

using namespace std;

constexpr auto DEFAULT_PORT = "58033";

struct Arguments {
    const char *ASport = DEFAULT_PORT;
    bool verbose = false;
};

Arguments parse_arguments(int argc, char *argv[]) {
    Arguments args;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            args.ASport = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            args.verbose = true;
        }
    }
    return args;
}

int main(int argc, char *argv[]) {
    auto args = parse_arguments(argc, argv);

    char in_str[128];

    fd_set inputs, testfds;
    struct timeval timeout;

    int i, out_fds, n, errcode, ret;

    char prt_str[90];

    // socket variables
    struct addrinfo hints, *res;
    struct sockaddr_in udp_useraddr;
    socklen_t addrlen;
    int ufd;

    char host[NI_MAXHOST], service[NI_MAXSERV];

    // UDP SERVER SECTION
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    if ((errcode = getaddrinfo(NULL, DEFAULT_PORT, &hints, &res)) != 0)
        exit(1); // On error

    ufd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (ufd == -1)
        exit(1);

    if (bind(ufd, res->ai_addr, res->ai_addrlen) == -1) {
        sprintf(prt_str, "Bind error UDP server\n");
        write(1, prt_str, strlen(prt_str));
        exit(1); // On error
    }
    if (res != NULL)
        freeaddrinfo(res);

    FD_ZERO(&inputs);     // Clear input mask
    FD_SET(0, &inputs);   // Set standard input channel on
    FD_SET(ufd, &inputs); // Set UDP channel on

    while (1) {
        testfds = inputs; // Reload mask
        //        printf("testfds byte: %d\n",((char *)&testfds)[0]); // Debug
        memset((void *)&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 10;

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL,
                         (struct timeval *)&timeout);
        // testfds is now '1' at the positions that were activated
        //        printf("testfds byte: %d\n",((char *)&testfds)[0]); // Debug
        switch (out_fds) {
        case 0:
            printf("\n ---------------Timeout event-----------------\n");
            break;
        case -1:
            perror("select");
            exit(1);
        default:
            if (FD_ISSET(0, &testfds)) {
                fgets(in_str, 50, stdin);
                printf("---Input at keyboard: %s\n", in_str);
            }
            if (FD_ISSET(ufd, &testfds)) {
                addrlen = sizeof(udp_useraddr);
                ret = recvfrom(ufd, prt_str, 80, 0,
                               (struct sockaddr *)&udp_useraddr, &addrlen);
                if (ret >= 0) {
                    if (strlen(prt_str) > 0)
                        prt_str[ret - 1] = 0;
                    printf("Received through UDP: %s\n", prt_str);
                    errcode = getnameinfo((struct sockaddr *)&udp_useraddr,
                                          addrlen, host, sizeof host, service,
                                          sizeof service, 0);
                    if (errcode == 0)
                        printf("Sent by [%s:%s]\n", host, service);

                }
            }
        }
    }
}

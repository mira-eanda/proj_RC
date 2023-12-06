#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "server_commands.hpp"

using namespace std;

constexpr auto DEFAULT_PORT = "58033";
constexpr timeval TIMEOUT_INTERVAL = {10, 0}; // 10 seconds

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

    fd_set inputs, testfds;
    Connections conns{};

    addrinfo hints{};

    // Configure udp server address
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    conns.udp = init_udp_connection(hints, NULL, args.ASport);

    if ((bind(conns.udp.fd, conns.udp.addr->ai_addr,
              conns.udp.addr->ai_addrlen)) == -1) {
        cerr << "Bind error UDP server" << endl;
        exit(1);
    }

    FD_ZERO(&inputs);              // Clear input mask
    FD_SET(0, &inputs);            // Set standard input channel on
    FD_SET(conns.udp.fd, &inputs); // Set UDP channel on

    string line;
    timeval timeout{};

    char host[NI_MAXHOST], service[NI_MAXSERV];
    auto db = new Database();

    while (1) {
        testfds = inputs; // Reload mask
        timeout = TIMEOUT_INTERVAL;

        int out_fds = select(FD_SETSIZE, &testfds, NULL, NULL, &timeout);

        switch (out_fds) {
        case 0:
            cout << "Timeout" << endl;
            break;
        case -1:
            cerr << "Select error" << endl;
            exit(1);
        default:
            if (FD_ISSET(0, &testfds)) {
                getline(cin, line);
                if (line == "exit") {
                    cout << "Exiting..." << endl;
                    exit(0);
                }
            }
            if (FD_ISSET(conns.udp.fd, &testfds)) {
                auto res = receive_udp(conns);
                if (res.has_value()) {
                    if (args.verbose && getnameinfo(conns.udp.addr->ai_addr,
                                                    conns.udp.addr->ai_addrlen,
                                                    host, sizeof host, service,
                                                    sizeof service, 0) == 0) {
                        cout << "Received through UDP: " << res.value() << endl;
                        cout << "Sent by [" << host << ":" << service << "]"
                             << endl;
                    }
                    auto req = parse_request(res.value());
                    if (req.type == "LIN") {
                        handle_login(req, conns, db);
                    } else if (req.type == "LOU") {
                        handle_logout(req, conns, db);
                    }
                }
            }
        }
    }
}

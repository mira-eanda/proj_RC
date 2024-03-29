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
#include <filesystem>

#include "server_commands.hpp"

using namespace std;

constexpr auto DEFAULT_PORT = "58033";
constexpr timeval TIMEOUT_INTERVAL = {3, 0}; // 3 seconds

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

Connections conns{};
int tcp_fd;

void cleanup(int signum) {
    cout << "Exiting..." << endl;
    freeaddrinfo(conns.udp.addr);
    freeaddrinfo(conns.tcp.addr);
    close(conns.udp.fd);
    close(tcp_fd);
    exit(0);
}

int main(int argc, char *argv[]) {
    auto args = parse_arguments(argc, argv);

    fd_set inputs, testfds;

    addrinfo hints{};

    if (!filesystem::exists("assets")) {
        filesystem::create_directory("assets");
    }

    // Configure udp server address
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    conns.udp = init_udp_connection(hints, NULL, args.ASport);

    if ((bind(conns.udp.fd, conns.udp.addr->ai_addr,
              conns.udp.addr->ai_addrlen)) == -1) {
        cerr << "Bind error UDP: " << strerror(errno) << endl;
        exit(1);
    }

    tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(tcp_fd, SOL_SOCKET, SO_RCVTIMEO, &TIMEOUT_INTERVAL, 0);
    if (tcp_fd == -1) {
        cerr << "Error creating socket." << endl;
        cerr << "Error: " << strerror(errno) << endl;
        exit(1);
    }

    if (bind(tcp_fd, conns.udp.addr->ai_addr, conns.udp.addr->ai_addrlen) ==
        -1) {
        cerr << "Bind error TCP: " << strerror(errno) << endl;
        exit(1);
    }

    if (listen(tcp_fd, 5) == -1) {
        perror("TCP listen failed");
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&inputs);              // Clear input mask
    FD_SET(0, &inputs);            // Set standard input channel on
    FD_SET(conns.udp.fd, &inputs); // Set UDP channel on
    FD_SET(tcp_fd, &inputs);       // Set TCP channel on

    string line;
    timeval timeout{};

    char host[NI_MAXHOST], service[NI_MAXSERV];
    auto db = new Database();

    int max_fd = max(conns.udp.fd, tcp_fd);

    signal(SIGINT, cleanup);
    signal(SIGABRT, cleanup);

    cout << "Server listening on port " << args.ASport << endl;
    while (1) {
        testfds = inputs; // Reload mask

        int out_fds = select(FD_SETSIZE, &testfds, NULL, NULL, NULL);

        switch (out_fds) {
        case 0:
            break;
        case -1:
            cerr << "Select error" << endl;
            cleanup(0);
            exit(1);
        default:
            if (FD_ISSET(0, &testfds)) {
                getline(cin, line);
                if (line == "exit") {
                    cleanup(0);
                    return 0;
                }
            }
            if (FD_ISSET(conns.udp.fd, &testfds)) {
                auto res = receive_udp(conns);
                if (res.has_value()) {
                    auto req = parse_request(res.value());
                    if (args.verbose && getnameinfo(conns.udp.addr->ai_addr,
                                                    conns.udp.addr->ai_addrlen,
                                                    host, sizeof host, service,
                                                    sizeof service, 0) == 0) {
                        cout << "----------------------------------------"
                             << endl;
                        cout << "NEW UDP REQUEST:" << endl;
                        cout << "\tSent by [" << host << ":" << service << "]"
                             << endl;
                        cout << "\t" << req.type << " " << req.message << endl;
                    }
                    db->close_ended_auctions();
                    if (req.type == "LIN") {
                        handle_login(req, conns, db, args.verbose);
                    } else if (req.type == "LOU") {
                        handle_logout(req, conns, db, args.verbose);
                    } else if (req.type == "UNR") {
                        handle_unregister(req, conns, db, args.verbose);
                    } else if (req.type == "LST") {
                        handle_list(req, conns, db, args.verbose);
                    } else if (req.type == "LMA") {
                        handle_my_auctions(req, conns, db, args.verbose);
                    } else if (req.type == "LMB") {
                        handle_my_bids(req, conns, db, args.verbose);
                    } else if (req.type == "SRC") {
                        handle_show_record(req, conns, db, args.verbose);
                    }
                    if (args.verbose) {
                        cout << "----------------------------------------"
                             << endl;
                    }
                }
            }
            if (FD_ISSET(tcp_fd, &testfds)) {
                sockaddr addr;
                socklen_t addrlen;
                int clientSocket = accept(tcp_fd, &addr, &addrlen);

                if (clientSocket == -1) {
                    perror("Error accepting TCP connection");
                } else {
                    if (args.verbose &&
                        getnameinfo(&addr, addrlen, host, sizeof host, service,
                                    sizeof service, 0) == 0) {
                        cout << "----------------------------------------"
                             << endl;

                        cout << "Accepted connection from [" << host << ":"
                             << service << "]" << endl;
                        cout << "----------------------------------------"
                             << endl;
                    }
                    FD_SET(clientSocket, &inputs);
                    max_fd = max(max_fd, clientSocket + 1);
                }
            }
            for (int i = tcp_fd + 1; i < max_fd; ++i) {
                if (FD_ISSET(i, &testfds)) {
                    auto r = receive_tcp(i);
                    if (!r) {
                        cout << "Failed receiving." << endl;
                    } else {
                        db->close_ended_auctions();
                        auto req = parse_request(r.value());
                        if (args.verbose) {
                            cout << "----------------------------------------"
                                 << endl;
                            cout << "NEW TCP REQUEST:" << endl;
                            cout << "\t" << req.type << " " << req.message
                                 << endl;
                        }
                        if (req.type == "OPA") {
                            handle_open(req, i, db, args.verbose);
                        } else if (req.type == "CLS") {
                            handle_close(req, i, db, args.verbose);
                        } else if (req.type == "BID") {
                            handle_bid(req, i, db, args.verbose);
                        } else if (req.type == "SAS") {
                            handle_show_asset(req, i, db, args.verbose);
                        }
                        if (args.verbose) {
                            cout << "----------------------------------------"
                                 << endl;
                        }
                    }
                    close(i);
                    FD_CLR(i, &inputs);
                }
            }
        }
    }
}

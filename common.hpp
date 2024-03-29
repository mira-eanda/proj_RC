#ifndef COMMON_HPP
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <optional>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;

const timeval TIMEOUT = {3, 0}; // 3 seconds

struct UDPConnection {
    int fd;
    struct addrinfo *addr;
};

struct TCPConnection {
    struct addrinfo *addr;
};

struct Connections {
    UDPConnection udp;
    TCPConnection tcp;
};

bool send_udp(const string &command, Connections conns) {
    auto n = sendto(conns.udp.fd, command.c_str(), command.size(), 0,
                    conns.udp.addr->ai_addr, conns.udp.addr->ai_addrlen);
    if (n == -1) {
        cerr << "Error sending message to AS." << endl;
        cerr << "Error: " << strerror(errno) << endl;
    }
    return n != -1;
}

optional<string> receive_udp(Connections conns) {
    char buffer[6000];
    auto n = recvfrom(conns.udp.fd, buffer, 6000, 0, conns.udp.addr->ai_addr,
                      &conns.udp.addr->ai_addrlen);
    if (n == -1) {
        cerr << "Error receiving message from AS." << endl;
        return {};
    }
    return string(buffer, n);
}

UDPConnection init_udp_connection(addrinfo hints, const char *IP,
                                  const char *port) {
    UDPConnection udp{};
    udp.fd = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);
    if (udp.fd == -1) {
        cerr << "Error creating socket." << endl;
        exit(1);
    }
    if (setsockopt(udp.fd, SOL_SOCKET, SO_RCVTIMEO, &TIMEOUT, sizeof(TIMEOUT)) < 0) {
        perror("Error");
        exit(1);
    }

    int errcode = getaddrinfo(IP, port, &hints, &(udp.addr));
    if (errcode != 0) {
        cerr << "An error occured: " << gai_strerror(errcode) << endl;
        exit(1);
    }
    return udp;
}

int init_tcp_connection(Connections conns) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &TIMEOUT, 0);
    if (fd == -1) {
        cerr << "Error creating socket." << endl;
        cerr << "Error: " << strerror(errno) << endl;
        exit(1);
    }

    auto n = connect(fd, conns.tcp.addr->ai_addr, conns.tcp.addr->ai_addrlen);
    if (n == -1) {
        cerr << "Error connecting to AS." << endl;
        cerr << "Error: " << strerror(errno) << endl;
        return -1;
    }
    return fd;
}

bool send_tcp(const string &command, int fd) {
    auto n = write(fd, command.c_str(), command.size());
    if (n == -1) {
        cerr << "Error sending message to AS." << endl;
        cerr << "Error: " << strerror(errno) << endl;
    }
    return n != -1;
}

optional<string> receive_tcp(int fd) {
    int n;
    string data;
    char buffer[128];
    n = recv(fd, buffer, 128, 0);
    if (n == -1) {
        cerr << "Error receiving message from AS." << endl;
        cerr << "Error: " << strerror(errno) << endl;
        return {};
    }
    if (n == 0) {
        return {};
    }
    return string(buffer, n);
}
#endif
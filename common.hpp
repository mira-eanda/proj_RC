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
    if (udp.fd == -1)
        exit(1);

    int errcode = getaddrinfo(IP, port, &hints, &(udp.addr));
    if (errcode != 0) {
        cerr << "An error occured: " << gai_strerror(errcode) << endl;
        exit(1);
    }
    return udp;
}
#endif
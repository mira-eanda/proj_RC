#include <arpa/inet.h>
#include <iostream>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "commands.h"

using namespace std;

constexpr auto PORT = "58001";
constexpr auto DEFAULT_PORT = "58033";

// ./client -n "tejo.tecnico.ulisboa.pt" -p 58001

struct Arguments {
    const char *ASIP = "localhost";
    const char *ASport = DEFAULT_PORT;
};

Arguments parse_arguments(int argc, char *argv[]) {
    Arguments args;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            args.ASIP = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            args.ASport = argv[++i];
        }
    }
    return args;
}

struct Command {
    std::string name;
    std::vector<std::string> args;
};

Command parse_command(const std::string &input) {
    Command command;

    std::istringstream iss(input);
    iss >> command.name;

    std::string arg;
    while (iss >> arg) {
        command.args.push_back(arg);
    }

    return command;
}

int main(int argc, char *argv[]) {

    auto args = parse_arguments(argc, argv);

    int errcode;
    struct addrinfo hints;
    char buffer[128];
    Connections connections;

    optional<User> user = {};

    cout << "Connecting to UDP server..." << endl;

    // Create a UDP socket
    connections.udp.fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (connections.udp.fd == -1)
        exit(1);

    // Configure udp server address
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    // Use getaddrinfo to obtain address information
    errcode =
        getaddrinfo(args.ASIP, args.ASport, &hints, &(connections.udp.addr));
    if (errcode != 0) {
        cerr << "An error occured: " << gai_strerror(errcode) << endl;
        exit(1);
    }

    cout << "Connecting to TCP server..." << endl;

    // Configure tcp server address
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Use getaddrinfo to obtain address information
    errcode =
        getaddrinfo(args.ASIP, args.ASport, &hints, &(connections.tcp.addr));
    if (errcode != 0) {
        cerr << "An error occured: " << gai_strerror(errcode) << endl;
        exit(1);
    }

    cout << "\e[1;1H\e[2J";
    cout << "Welcome to the RC Auctions!" << endl;

    // Wait for user input and process command
    while (1) {
        cout << "> ";
        cin.getline(buffer, 128);

        auto cmd = parse_command(buffer);

        if (cmd.name == "login") {
            user = login(cmd.args, connections);
        } else if (cmd.name == "list" || cmd.name == "l") {
            list(cmd.args, connections);
        } else if (cmd.name == "myauctions" || cmd.name == "ma") {
            list_my_auctions(cmd.args, connections, user);
        } else if (cmd.name == "show_record" || cmd.name == "sr") {
            show_record(cmd.args, connections);
        } else if (cmd.name == "show_asset" || cmd.name == "sa") {
            show_asset(cmd.args, connections);
        } else if (cmd.name == "mybids" || cmd.name == "mb") {
            my_bids(cmd.args, connections, user);
        } else if (cmd.name == "open") {
            open(cmd.args, connections, user);
        } else if (cmd.name == "bid" || cmd.name == "b") {
            bid(cmd.args, connections, user);
        } else if (cmd.name == "logout") {
            logout(cmd.args, connections, user);
        } else if (cmd.name == "unregister") {
            unregister(cmd.args, connections, user);
        } else if (cmd.name == "exit") {
            exit_cli(cmd.args, connections, user);
        } else {
            cout << "Unknown command." << endl;
        }
    }

    freeaddrinfo(connections.udp.addr);
    freeaddrinfo(connections.tcp.addr);
    close(connections.udp.fd);

    return 0;
}

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

    int fd, errcode;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    char buffer[128];

    optional<User> user = {};

    // Create a UDP socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
        exit(1);

    // Configure server address
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    cout << "Connecting to server..." << endl;

    // Use getaddrinfo to obtain address information
    errcode = getaddrinfo(args.ASIP, args.ASport, &hints, &res);
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

        cout << "Command: " << cmd.name << endl;
        if (cmd.name == "login") {
            user = login(cmd.args, fd, res);
        } else if (cmd.name == "list") {
            // list_command(buffer, fd, res);
        } else if (cmd.name == "logout") {
            logout(cmd.args, fd, res, user);
        } else if (cmd.name == "unregister") {
            unregister(cmd.args, fd, res, user);
        } else if (cmd.name == "exit") {
            exit_cli(cmd.args, fd, res, user);
        } else {
            cout << "Unknown command." << endl;
        }
    }

    freeaddrinfo(res);
    close(fd);

    return 0;
}

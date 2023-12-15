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

#include "commands.hpp"
#include "deps/linenoise.hpp"

using namespace std;

constexpr auto DEFAULT_PORT = "58033";
const auto history_path = "history.txt";

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
    string name;
    vector<string> args;
};

Command parse_command(const string &input) {
    Command command;

    istringstream iss(input);
    iss >> command.name;

    string arg;
    while (iss >> arg) {
        command.args.push_back(arg);
    }

    return command;
}

int main(int argc, char *argv[]) {

    auto args = parse_arguments(argc, argv);

    addrinfo hints{};
    Connections connections{};

    optional<User> user = {};

    cout << "Connecting to UDP server..." << endl;

    // Configure udp server address
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    connections.udp = init_udp_connection(hints, args.ASIP, args.ASport);

    cout << "Connecting to TCP server..." << endl;

    // Configure tcp server address
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Use getaddrinfo to obtain address information
    int errcode =
        getaddrinfo(args.ASIP, args.ASport, &hints, &(connections.tcp.addr));
    if (errcode != 0) {
        cerr << "An error occured: " << gai_strerror(errcode) << endl;
        exit(1);
    }

    cout << "\e[1;1H\e[2J";
    cout << "Welcome to the RC Auctions!" << endl;

    linenoise::LoadHistory(history_path);

    linenoise::SetHistoryMaxLen(100);
    while (true) {
        string line;
        auto exit = linenoise::Readline("> ", line);
        auto cmd = parse_command(line);

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
            open_auction(cmd.args, connections, user);
        } else if (cmd.name == "close") {
            close(cmd.args, connections, user);
        } else if (cmd.name == "bid" || cmd.name == "b") {
            bid(cmd.args, connections, user);
        } else if (cmd.name == "logout") {
            logout(cmd.args, connections, user);
        } else if (cmd.name == "unregister") {
            unregister(cmd.args, connections, user);
        } else if (cmd.name == "exit" || exit) {
            exit_cli(cmd.args, connections, user);
        } else {
            cout << "Unknown command." << endl;
        }
        linenoise::AddHistory(line.c_str());
        linenoise::SaveHistory(history_path);
    }


    freeaddrinfo(connections.udp.addr);
    freeaddrinfo(connections.tcp.addr);
    close(connections.udp.fd);

    return 0;
}

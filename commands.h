#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace std;

constexpr int MAX_UID = 6;
constexpr int MAX_PASSWORD = 8;

enum status_t { OK, NOK, REG, UNR };

struct Response {
    string type;
    status_t status;
    string message;
};

struct User {
    string uid;
    string password;
};

struct Auction {
    int AID;
    int state;
};

bool validate_args(const vector<string> &args, int expected) {
    if (args.size() != expected) {
        cerr << "Invalid number of args for command." << std::endl;
        return false;
    }
    return true;
}

bool validate_auth(optional<User> &user) {
    if (!user) {
        cerr << "You must be logged in to perform this action." << endl;
        return false;
    }
    return true;
}

optional<Response> parse_response(const string &response, const string type) {
    Response r;
    r.type = response.substr(0, 3);

    if (r.type != type) {
        cerr << "Unexpected response type: " << r.type << endl;
        return {};
    }
    if (response[4] == 'O') {
        r.status = OK;
    } else if (response[4] == 'N') {
        r.status = NOK;
    } else if (response[4] == 'R') {
        r.status = REG;
    } else if (response[4] == 'U') {
        r.status = UNR;
    } else {
        cerr << "Invalid response: " << response << endl;
        return {};
    }

    r.message = response.substr(6);

    return r;
}

optional<Response> send_command(const string &command, int fd,
                                struct addrinfo *res, const string &type) {
    auto n = sendto(fd, command.c_str(), command.size(), 0, res->ai_addr,
                    res->ai_addrlen);
    if (n == -1) {
        cerr << "Error sending message to AS." << endl;
        exit(1);
    }

    char buffer[128];
    n = recvfrom(fd, buffer, 128, 0, res->ai_addr, &res->ai_addrlen);
    if (n == -1) {
        cerr << "Error receiving message from AS." << endl;
        exit(1);
    }

    return parse_response(buffer, type);
}

constexpr auto numeric = "0123456789";
constexpr auto alphanumeric =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ ";

optional<User> login(vector<string> &args, int fd, struct addrinfo *res) {
    if (!validate_args(args, 2)) {
        return {};
    }

    auto uid = args[0];
    auto password = args[1];

    if (uid.size() != MAX_UID) {
        cerr << "UID must be " << MAX_UID << " characters long." << endl;
        return {};
    }
    if (password.size() != MAX_PASSWORD) {
        cerr << "Password must be " << MAX_PASSWORD << " characters long."
             << endl;
        return {};
    }

    if (uid.find_first_not_of(numeric) != string::npos) {
        cerr << "UID must be numeric." << endl;
        return {};
    }

    if (password.find_first_not_of(alphanumeric) != string::npos) {
        cerr << "Password must be alphanumeric." << endl;
        return {};
    }

    auto message = "LIN " + uid + " " + password + "\n";
    auto response = send_command(message, fd, res, "RLI");
    if (!response) {
        return {};
    }
    auto status = response->status;

    if (status == OK) {
        cout << "successful login" << endl;
        return User{uid, password};
    } else if (status == NOK) {
        cout << "incorrect login" << endl;
    } else if (status == REG) {
        cout << "new user registered" << endl;
        return User{uid, password};
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
    return {};
}

void logout(vector<string> &args, int fd, struct addrinfo *res,
            optional<User> &user) {
    if (!validate_args(args, 0) || !validate_auth(user)) {
        return;
    }

    auto message = "LOU " + user->uid + " " + user->password + "\n";
    auto response = send_command(message, fd, res, "RLO");
    if (!response) {
        return;
    }
    auto status = response->status;

    if (status == OK) {
        cout << "successful logout" << endl;
        user = {};
    } else if (status == NOK) {
        cout << "user not logged in" << endl;
    } else if (status == UNR) {
        cout << "unknown user" << endl;
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

void unregister(vector<string> &args, int fd, struct addrinfo *res,
                optional<User> &user) {
    if (!validate_args(args, 0) || !validate_auth(user)) {
        return;
    }

    auto message = "UNR " + user->uid + " " + user->password + "\n";
    auto response = send_command(message, fd, res, "RUR");
    if (!response) {
        return;
    }
    auto status = response->status;

    if (status == OK) {
        cout << "successful unregister" << endl;
        user = {};
    } else if (status == NOK) {
        cout << "user not logged in" << endl;
    } else if (status == UNR) {
        cout << "unknown user" << endl;
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

vector<Auction> parse_listed_auctions(const string &buffer) {
    vector<Auction> auctions;
    std::istringstream iss(buffer);
    int AID, state;
    while (iss >> AID >> state) {
        auctions.push_back({AID, state});
    }
    return auctions;
}

void print_auctions(const vector<Auction> &auctions) {
    auto filtered = auctions | views::filter([](auto auction) {
                        return auction.state == 1;
                    });

    if (filtered.empty()) {
        cout << "no auctions are currently active" << endl;
        return;
    }
    for (auto auction : filtered) {
        if (auction.state == 1) {
            cout << auction.AID << " - "
                 << "active" << endl;
        }
    }
}

void list(vector<string> &args, int fd, struct addrinfo *res) {
    if (!validate_args(args, 0)) {
        return;
    }

    auto message = "LST\n";
    auto response = send_command(message, fd, res, "RLS");
    if (!response) {
        return;
    }

    auto status = response->status;
    if (status == NOK) {
        cout << "no auctions are currently active" << endl;
    } else if (status == OK) {
        auto auctions = parse_listed_auctions(response->message);
        print_auctions(auctions);
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

void list_my_auctions(vector<string> &args, int fd, struct addrinfo *res,
                      optional<User> &user) {
    if (!validate_args(args, 0) || !validate_auth(user)) {
        return;
    }

    auto message = "LMA " + user->uid + "\n";
    auto response = send_command(message, fd, res, "RMA");
    if (!response) {
        return;
    }

    auto status = response->status;
    cout << "status: " << status << endl;

    if (status == NOK) {
        cout << "no auctions are currently active" << endl;
    } else if (status == OK) {
        auto auctions = parse_listed_auctions(response->message);
        print_auctions(auctions);
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

void show_record(vector<string> &args, int fd, struct addrinfo *res) {
    if (!validate_args(args, 1)) {
        return;
    }
}

void exit_cli(vector<string> &args, int fd, struct addrinfo *res,
              optional<User> &user) {
    if (!validate_args(args, 0)) {
        return;
    }

    if (user) {
        logout(args, fd, res, user);
    }

    cout << "Exiting..." << endl;
    exit(0);
}

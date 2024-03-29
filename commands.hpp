#ifndef COMMANDS_HPP
#include <arpa/inet.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "common.hpp"

using namespace std;

constexpr auto NAME_SIZE = 10;
constexpr auto NAME_ERROR =
    "name must be up to 10 alphanumeric characters long.";
constexpr auto START_VALUE_SIZE = 6;
constexpr auto START_VALUE_ERROR = "start_value must be up to 6 digits long.";
constexpr auto TIMEACTIVE_SIZE = 5;
constexpr auto TIMEACTIVE_ERROR = "timeactive must be up to 5 digits long.";
constexpr auto MAX_BIDS = 50;

constexpr int MAX_UID = 6;
constexpr int MAX_PASSWORD = 8;

// enum status_t { OK, NOK, REG, UNR, NLG, EAU, EOW, END, REF, ILG, ACC };

struct Response {
    string type;
    string status;
    string message;
};

struct Auction {
    string AID;
    bool state;
};

struct File {
    string name;
    long int size;
    // string data;
};

struct User {
    string uid;
    string password;
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
    istringstream iss(response);
    iss >> r.type;
    iss >> r.status;

    if (r.type != type) {
        cerr << "Unexpected response type: " << r.type << endl;
        return {};
    }

    r.message = response.substr(6);

    return r;
}

optional<Response> send_udp_command(const string &command, Connections conns,
                                    const string &type) {
    if (!send_udp(command, conns)) {
        return {};
    }
    auto res = receive_udp(conns);
    if (!res) {
        return {};
    }
    return parse_response(res.value(), type);
}

optional<Response> send_tcp_command(const string &command, Connections conns,
                                    const string &type) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &TIMEOUT, 0);
    if (fd == -1)
        exit(1);

    auto n = connect(fd, conns.tcp.addr->ai_addr, conns.tcp.addr->ai_addrlen);
    if (n == -1) {
        cerr << "Error connecting to AS." << endl;
        cerr << "Error: " << strerror(errno) << endl;
        return {};
    }

    n = write(fd, command.c_str(), command.size());
    if (n == -1) {
        cerr << "Error sending message to AS." << endl;
        cerr << "Error: " << strerror(errno) << endl;
        return {};
    }

    string data;
    char buffer[128];
    while (true) {
        n = read(fd, buffer, 128);
        if (n == -1) {
            cerr << "Error receiving message from AS." << endl;
            return {};
        }
        if (n == 0) {
            break;
        }
        data += string(buffer, n);
    }

    close(fd);

    return parse_response(data, type);
}

constexpr auto numeric = "0123456789";
constexpr auto alphanumeric =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ ";

optional<User> login(vector<string> &args, Connections conns) {
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
    auto response = send_udp_command(message, conns, "RLI");
    if (!response) {
        return {};
    }
    auto status = response->status;

    if (status == "OK") {
        cout << "successful login" << endl;
        return User{uid, password};
    } else if (status == "NOK") {
        cout << "incorrect login" << endl;
    } else if (status == "REG") {
        cout << "new user registered" << endl;
        return User{uid, password};
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
    return {};
}

void logout(vector<string> &args, Connections conns, optional<User> &user) {
    if (!validate_args(args, 0) || !validate_auth(user)) {
        return;
    }

    auto message = "LOU " + user->uid + " " + user->password + "\n";
    auto response = send_udp_command(message, conns, "RLO");
    if (!response) {
        return;
    }
    auto status = response->status;

    if (status == "OK") {
        cout << "successful logout" << endl;
        user = {};
    } else if (status == "NOK") {
        cout << "user not logged in" << endl;
    } else if (status == "UNR") {
        cout << "unknown user" << endl;
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

void unregister(vector<string> &args, Connections conns, optional<User> &user) {
    if (!validate_args(args, 0) || !validate_auth(user)) {
        return;
    }

    auto message = "UNR " + user->uid + " " + user->password + "\n";
    auto response = send_udp_command(message, conns, "RUR");
    if (!response) {
        return;
    }
    auto status = response->status;

    if (status == "OK") {
        cout << "successful unregister" << endl;
        user = {};
    } else if (status == "NOK") {
        cout << "user not logged in" << endl;
    } else if (status == "UNR") {
        cout << "unknown user" << endl;
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

vector<Auction> parse_listed_auctions(const string &buffer) {
    vector<Auction> auctions;
    std::istringstream iss(buffer);
    string AID, state;
    while (iss >> AID >> state) {
        auto auction = Auction{AID, state == "1"};
        auctions.push_back(auction);
    }
    return auctions;
}

void print_auctions(const vector<Auction> &auctions) {
    auto filtered = vector<Auction>();
    for (auto auction : auctions) {
        if (auction.state == true) {
            filtered.push_back(auction);
        }
    }

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

void list(vector<string> &args, Connections conns) {
    if (!validate_args(args, 0)) {
        return;
    }

    auto message = "LST\n";
    auto response = send_udp_command(message, conns, "RLS");
    if (!response) {
        return;
    }

    auto status = response->status;
    if (status == "NOK") {
        cout << "no auctions are currently active" << endl;
    } else if (status == "OK") {
        auto auctions = parse_listed_auctions(response->message);
        print_auctions(auctions);
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

void list_my_auctions(vector<string> &args, Connections conns,
                      optional<User> &user) {
    if (!validate_args(args, 0) || !validate_auth(user)) {
        return;
    }

    auto message = "LMA " + user->uid + "\n";
    auto response = send_udp_command(message, conns, "RMA");
    if (!response) {
        return;
    }

    auto status = response->status;

    if (status == "NOK") {
        cout << "no auctions are currently active" << endl;
    } else if (status == "OK") {
        auto auctions = parse_listed_auctions(response->message);
        print_auctions(auctions);
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

struct BidInfo {
    int bidder_UID;
    int bid_value;
    string bid_date_time;
    int bid_sec_time;
};

struct EndInfo {
    string end_date_time;
    int end_sec_time;
};

struct AuctionInfo {
    int host_UID;
    string auction_name;
    string asset_fname;
    int start_value;
    string start_date_time;
    int timeactive;
    vector<BidInfo> bids;
    optional<EndInfo> end;
};

optional<AuctionInfo> parse_auction_info(const string &buffer) {
    AuctionInfo info;
    std::istringstream iss(buffer);
    string date, time;
    iss >> info.host_UID >> info.auction_name >> info.asset_fname >>
        info.start_value >> date >> time >> info.timeactive;

    info.start_date_time = date + " " + time;

    string bid;
    while (iss >> bid) {
        if (bid == "E") {
            EndInfo end;
            iss >> date >> time >> end.end_sec_time;
            end.end_date_time = date + " " + time;
            info.end = end;
            break;
        } else {
            BidInfo bid_info;
            iss >> bid_info.bidder_UID >> bid_info.bid_value >> date >> time >>
                bid_info.bid_sec_time;
            bid_info.bid_date_time = date + " " + time;
            info.bids.push_back(bid_info);
        }
    }

    return info;
}

void show_record(vector<string> &args, Connections conns) {
    if (!validate_args(args, 1)) {
        return;
    }
    auto aid = args[0];

    auto message = "SRC " + aid + "\n";
    auto response = send_udp_command(message, conns, "RRC");
    if (!response) {
        return;
    }
    auto status = response->status;

    if (status == "NOK") {
        cout << "auction not found" << endl;
    } else if (status == "OK") {
        auto info = parse_auction_info(response->message);
        if (!info) {
            cerr << "Error parsing auction info." << endl;
            return;
        }
        cout << "--- Auction #" << aid << " ---" << endl;

        cout << "host_UID: " << info->host_UID << endl;
        cout << "auction_name: " << info->auction_name << endl;
        cout << "asset_fname: " << info->asset_fname << endl;
        cout << "start_value: " << info->start_value << endl;
        cout << "start_date_time: " << info->start_date_time << endl;
        cout << "timeactive: " << info->timeactive << endl;
        if (info->end) {
            cout << "end_date_time: " << info->end->end_date_time << endl;
            cout << "end_sec_time: " << info->end->end_sec_time << endl;
        }

        for (size_t i = 0; i < info->bids.size() && i < MAX_BIDS; i++) {
            auto bid = info->bids[i];
            cout << "--- Bid info ---" << endl;
            cout << "bidder_UID: " << bid.bidder_UID << endl;
            cout << "bid_value: " << bid.bid_value << endl;
            cout << "bid_date_time: " << bid.bid_date_time << endl;
            cout << "bid_sec_time: " << bid.bid_sec_time << endl;
        }
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

struct Asset {
    string name;
    int size;
};

optional<Asset> parse_asset(const string &buffer) {
    Asset asset;

    istringstream iss(buffer);
    iss >> asset.name >> asset.size;
    iss.get();

    ofstream file(asset.name);
    file << iss.rdbuf();
    file.close();
    return asset;
}

void show_asset(vector<string> &args, Connections conns) {
    if (!validate_args(args, 1)) {
        return;
    }
    auto aid = args[0];

    auto message = "SAS " + aid + "\n";
    auto response = send_tcp_command(message, conns, "RSA");
    if (!response) {
        return;
    }

    auto status = response->status;
    if (status == "NOK") {
        cout << "auction not found" << endl;
    } else if (status == "OK") {
        auto asset = parse_asset(response->message);
        if (!asset) {
            cerr << "Error parsing asset." << endl;
            return;
        }
        cout << "--- Asset #" << aid << " ---" << endl;
        cout << "name: " << asset->name << endl;
        cout << "size: " << asset->size << endl;
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

void my_bids(vector<string> &args, Connections conns, optional<User> &user) {
    if (!validate_args(args, 0) || !validate_auth(user)) {
        return;
    }

    auto message = "LMB " + user->uid + "\n";
    auto response = send_udp_command(message, conns, "RMB");
    if (!response) {
        return;
    }

    auto status = response->status;

    if (status == "NOK") {
        cout << "no ongoing bids" << endl;
    } else if (status == "NLG") {
        cout << "user not logged in" << endl;
    } else if (status == "OK") {
        auto auctions = parse_listed_auctions(response->message);
        print_auctions(auctions);
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

bool validate_input(const std::string &input, size_t max_length,
                    const std::string &allowed_chars,
                    const std::string &error_message) {
    if (input.size() > max_length ||
        input.find_first_not_of(allowed_chars) != std::string::npos) {
        std::cerr << error_message << std::endl;
        return false;
    }
    return true;
}

optional<File> get_file_info(const string &fname) {
    ifstream file(fname, ios::binary | ios::ate);
    if (!file.is_open()) {
        cerr << "Error opening file: " << fname << endl;
        return {};
    }

    auto size = file.tellg();
    file.seekg(0, ios::beg);

    /*
    auto data = string(size, ' ');
    file.read(&data[0], size);
    */

    return File{fname, size};
}

void open_auction(vector<string> &args, Connections conns,
                  optional<User> &user) {
    if (!validate_args(args, 4)) {
        return;
    }

    if (!validate_auth(user)) {
        return;
    }

    // open name asset_fname start_value timeactive
    auto name = args[0];
    auto asset_fname = args[1];
    auto start_value = args[2];
    auto timeactive = args[3];

    if (!validate_input(name, NAME_SIZE, alphanumeric, NAME_ERROR) ||
        !validate_input(start_value, START_VALUE_SIZE, numeric,
                        START_VALUE_ERROR) ||
        !validate_input(timeactive, TIMEACTIVE_SIZE, numeric,
                        TIMEACTIVE_ERROR)) {
        return;
    }

    auto file = get_file_info(asset_fname);
    if (!file) {
        return;
    }
    int file_fd = open(asset_fname.c_str(), O_RDONLY);
    if (file_fd == -1) {
        cerr << "Error opening file: " << asset_fname << endl;
        return;
    }

    // OPA UID password name start_value timeactive Fname Fsize Fdata
    auto message = "OPA " + user->uid + " " + user->password + " " + name +
                   " " + start_value + " " + timeactive + " " + file->name +
                   " " + to_string(file->size) + " ";

    int fd = init_tcp_connection(conns);

    send_tcp(message, fd);
    sendfile(fd, file_fd, 0, file->size);
    send_tcp("\n", fd);

    auto res = receive_tcp(fd);
    if (!res) {
        return;
    }
    auto response = parse_response(res.value(), "ROA");

    auto status = response->status;
    auto aid = response->message;
    if (status == "NOK") {
        cout << "auction could not be started" << endl;
    } else if (status == "NLG") {
        cout << "user not logged in" << endl;
    } else if (status == "OK") {
        cout << "auction started" << endl;
        cout << "AID: " << aid << endl;
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

void bid(vector<string> &args, Connections conns, optional<User> &user) {
    if (!validate_args(args, 2) || !validate_auth(user)) {
        return;
    }

    auto aid = args[0];
    auto value = args[1];

    auto message = "BID " + user->uid + " " + user->password + " " + aid + " " +
                   value + "\n";
    auto response = send_tcp_command(message, conns, "RBD");
    if (!response) {
        return;
    }

    auto status = response->status;
    if (status == "NOK") {
        cout << "auction not active" << endl;
    } else if (status == "NLG") {
        cout << "user not logged in" << endl;
    } else if (status == "REF") {
        cout << "larger bid has already been placed before" << endl;
    } else if (status == "ILG") {
        cout << "not allowed to bid on your own auction" << endl;
    } else if (status == "ACC") {
        cout << "bid accepted" << endl;
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

void close(vector<string> &args, Connections conns, optional<User> &user) {
    if (!validate_args(args, 1) || !validate_auth(user)) {
        return;
    }

    auto aid = args[0];

    auto message = "CLS " + user->uid + " " + user->password + " " + aid + "\n";

    auto response = send_tcp_command(message, conns, "RCL");
    if (!response) {
        return;
    }

    auto status = response->status;
    if (status == "NOK") {
        cout << "auction could not be closed" << endl;
    } else if (status == "NLG") {
        cout << "user not logged in" << endl;
    } else if (status == "OK") {
        cout << "auction closed" << endl;
    } else if (status == "EAU") {
        cout << "auction does not exist" << endl;
    } else if (status == "EOW") {
        cout << "auction is not owned by user" << endl;
    } else if (status == "END") {
        cout << "auction has already finished" << endl;
    } else {
        cerr << "Unexpected response status: " << status << endl;
    }
}

void exit_cli(vector<string> &args, Connections conns, optional<User> &user) {
    if (!validate_args(args, 0)) {
        return;
    }

    if (user) {
        logout(args, conns, user);
    }

    cout << "Exiting..." << endl;
    exit(0);
}

#endif
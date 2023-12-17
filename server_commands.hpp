#ifndef SERVER_COMMANDS_HPP
#include "database.hpp"

struct Request {
    string type;
    string message;
};

Request parse_request(const string &input) {
    Request req;

    req.type = input.substr(0, 3);
    req.message = input.substr(4, input.size() - 4);

    return req;
}

optional<User> parse_user(istringstream &iss) {
    User user;

    iss >> user.uid;

    if (iss >> user.password) {
        return user;
    } else {
        return {user};
    }
}

void handle_login(Request &req, Connections conns, Database *db, bool v) {
    istringstream iss(req.message);
    auto user = parse_user(iss);
    if (user.has_value()) {

        auto db_user = db->get_user(user.value().uid);
        if (db_user.has_value()) {
            if (db_user.value().password == user.value().password) {
                db->set_logged_in(user.value().uid, true);
                if (v) {
                    cout << "User " << user.value().uid << " logged in."
                         << endl;
                }
                send_udp("RLI OK\n", conns);
            } else {
                if (v) {
                    cout << "Invalid password." << endl;
                }
                send_udp("RLI NOK\n", conns);
            }
        } else {
            db->add_user(user.value().uid, user.value().password);
            if (v) {
                cout << "User " << user.value().uid << " registered." << endl;
            }
            send_udp("RLI REG\n", conns);
        }
    } else {
        if (v) {
            cout << "Invalid user." << endl;
        }
        send_udp("RLI NOK\n", conns);
    }
}

void handle_logout(const Request &req, Connections conns, Database *db,
                   bool v) {
    istringstream iss(req.message);
    auto user = parse_user(iss);

    if (user.has_value()) {
        if (db->validate_user(user.value())) {
            if (v) {
                cout << "User " << user.value().uid << " logged out." << endl;
            }
            db->set_logged_in(user.value().uid, false);
            send_udp("RLO OK\n", conns);
        } else {
            cout << "Unknown user." << endl;
            send_udp("RLO NOK\n", conns);
        }
    } else {
        cout << "Invalid user." << endl;
        send_udp("ERR: invalid user\n", conns);
    }
}

void handle_unregister(const Request &req, Connections conns, Database *db,
                       bool v) {
    istringstream iss(req.message);
    auto user = parse_user(iss);
    if (user.has_value()) {
        if (db->validate_user(user.value())) {
            db->delete_user(user.value().uid);
            if (v) {
                cout << "User " << user.value().uid << " unregistered." << endl;
            }
            send_udp("RUR OK\n", conns);
        } else {
            cout << "Unknown user." << endl;
            send_udp("RUR UNR\n", conns);
        }
    } else {
        cout << "Invalid user." << endl;
        send_udp("ERR: invalid user\n", conns);
    }
}

void handle_list(const Request &req, Connections conns, Database *db, bool v) {
    auto auctions = db->get_auctions();
    if (auctions.size() == 0) {
        cout << "No auctions." << endl;
        send_udp("RLS NOK\n", conns);
    } else {
        string msg = "RLS OK";
        for (auto auction : auctions) {
            msg += " " + auction.aid + " " + to_string(auction.open);
        }
        msg += "\n";
        if (v) {
            cout << "Sending list of " << auctions.size() << " auctions."
                 << endl;
        }
        send_udp(msg, conns);
    }
}

void handle_my_auctions(const Request &req, Connections conns, Database *db,
                        bool v) {
    istringstream iss(req.message);
    auto u = parse_user(iss);

    if (u.has_value()) {
        auto user = db->get_user(u.value().uid);
        if (db->validate_user(user.value())) {
            auto auctions = db->get_auctions_by_user(user.value().uid);
            if (auctions.size() == 0) {
                cout << "No auctions." << endl;
                send_udp("RMA NOK\n", conns);
            } else {
                string msg = "RMA OK";
                for (auto auction : auctions) {
                    msg += " " + auction.aid + " " + to_string(auction.open);
                }
                msg += "\n";
                if (v) {
                    cout << "Sending list of " << auctions.size()
                         << " auctions." << endl;
                }
                send_udp(msg, conns);
            }
        } else {
            cout << "Unknown user." << endl;
            send_udp("RMA NLG\n", conns);
        }
    } else {
        cout << "Invalid user." << endl;
        send_udp("ERR: invalid user\n", conns);
    }
}

void handle_my_bids(const Request &req, Connections conns, Database *db,
                    bool v) {
    istringstream iss(req.message);
    auto u = parse_user(iss);

    if (!u) {
        cout << "Invalid user." << endl;
        send_udp("ERR: invalid user\n", conns);
        return;
    }

    auto user = db->get_user(u.value().uid);

    if (!db->validate_user(user.value())) {
        cout << "Unknown user." << endl;
        send_udp("RMB NLG\n", conns);
        return;
    }

    auto auctions = db->get_bids_by_user(user.value().uid);
    if (auctions.size() == 0) {
        send_udp("RMB NOK\n", conns);
    } else {
        string msg = "RMB OK";
        for (auto aid : auctions) {
            msg += " " + aid + " " + to_string(db->check_auction_open(aid));
        }
        msg += "\n";
        if (v) {
            cout << "Sending list of " << auctions.size() << " auctions."
                 << endl;
        }
        send_udp(msg, conns);
    }
}

void send_auction_record(const Auction &auction, Connections conns,
                         Database *db) {
    string msg = "RRC OK " + auction.uid + " " + auction.auction_name + " " +
                 auction.asset_fname + " " + to_string(auction.start_value) +
                 " " + auction.start_date_time + " " +
                 to_string(auction.timeactive);

    auto bids = db->get_bids_of_auction(auction);
    if (bids.size() > 0) {
        for (auto bid : bids) {
            msg += " B " + bid.uid + " " + to_string(bid.value) + " " +
                   bid.bid_date_time + " " + to_string(bid.bid_sec_time);
        }
    }

    if (!(auction.open)) {
        msg += " E " + auction.end->end_date_time + " " +
               to_string(auction.end->end_sec_time);
    }
    msg += "\n";
    send_udp(msg, conns);
}

optional<Auction> parse_auction(const string &input) {
    Auction auction;

    istringstream iss(input);
    iss >> auction.aid;

    if (iss >> auction.uid) {
        iss >> auction.auction_name;
        iss >> auction.asset_fname;
        iss >> auction.start_value;
        iss >> auction.start_date_time;
        iss >> auction.timeactive;
        return auction;
    } else {
        return {auction};
    }
}

void handle_show_record(const Request &req, Connections conns, Database *db,
                        bool v) {
    string aid = req.message;
    cout << aid << endl;
    aid = aid.substr(0, aid.size() - 1);

    auto auction = db->get_auction(aid);

    if (auction.has_value()) {
        if (v) {
            cout << "Sending record of auction " << aid << endl;
        }
        send_auction_record(auction.value(), conns, db);
    } else {
        send_udp("RRC NOK\n", conns);
    }
}

optional<Auction> parse_open_message(istringstream &iss) {
    Auction auction;
    string password;

    iss >> auction.auction_name;
    iss >> auction.start_value;
    iss >> auction.timeactive;
    iss >> auction.asset_fname;
    iss >> auction.asset_fsize;

    return auction;
}

void handle_open(const Request &req, int tcp_fd, Database *db, bool v) {
    istringstream iss(req.message);
    auto user = parse_user(iss);

    if (!user) {
        cout << "Invalid user." << endl;
        send_tcp("ERR: invalid user\n", tcp_fd);
        return;
    }

    auto db_user = db->get_user(user->uid);
    if (!db->validate_user(db_user.value())) {
        send_tcp("ROA NLG\n", tcp_fd);
        return;
    }
    auto auction = parse_open_message(iss);

    if (!auction) {
        cout << "Auction could not be started." << endl;
        send_tcp("ROA NOK\n", tcp_fd);
        return;
    }

    if (db->get_auctions().size() >= 999) {
        cout << "Too many auctions." << endl;
        send_tcp("ROA NOK\n", tcp_fd);
        return;
    }

    if (v) {
        cout << "User " << user->uid << " opened an auction." << endl;
    }

    auction->uid = user->uid;
    auction->aid = db->generate_aid();
    auction->open = true;
    auction->start_date_time = get_current_time();

    string path = "assets/" + auction->aid + "_" + auction->asset_fname;
    ofstream file(path, ios::binary);
    if (!file) {
        cout << "Error opening file." << endl;
        send_tcp("ROA NOK\n", tcp_fd);
        return;
    }

    iss.ignore(1);

    // write the rest of the message left in iss
    int leftover = req.message.size() - iss.tellg();
    char buffer[128];
    req.message.copy(buffer, leftover, iss.tellg());
    file.write(buffer, leftover);

    // read the rest of the file
    if (leftover < auction->asset_fsize) {
        char buffer[1024];
        int bytes_read;
        int bytes_written = leftover;
        while ((bytes_read = read(tcp_fd, buffer, 1024)) > 0) {
            file.write(buffer, bytes_read);
            bytes_written += bytes_read;
            if (bytes_written >= auction->asset_fsize) {
                break;
            }
        }
    }
    file.close();

    db->add_auction(auction.value());
    send_tcp("ROA OK " + auction->aid + "\n", tcp_fd);
}

void handle_close(const Request &req, int tcp_fd, Database *db, bool v) {
    istringstream iss(req.message);
    string aid;
    auto user = parse_user(iss);

    if (!user) {
        cout << "Invalid user." << endl;
        send_tcp("RCL NOK\n", tcp_fd);
        return;
    }

    iss >> aid;

    auto db_user = db->get_user(user.value().uid);

    if (!db->validate_user(db_user.value())) {
        cout << "User not logged in." << endl;
        send_tcp("RCL NLG\n", tcp_fd);
        return;
    }

    auto auction = db->get_auction(aid);

    if (!auction) {
        cout << "Auction not found." << endl;
        send_tcp("RCL EAU\n", tcp_fd);
        return;
    }

    if (auction.value().uid != user.value().uid) {
        cout << "User does not own auction." << endl;
        send_tcp("RCL EOW\n", tcp_fd);
        return;
    }

    if (!auction.value().open) {
        cout << "Auction already closed." << endl;
        send_tcp("RCL END\n", tcp_fd);
        return;
    }

    db->close_auction(auction.value());
    if (v) {
        cout << "Auction " << auction.value().aid << " closed." << endl;
    }
    send_tcp("RCL OK\n", tcp_fd);
}

void handle_bid(const Request &req, int tcp_fd, Database *db, bool v) {
    istringstream iss(req.message);
    string aid;
    int value;
    auto user = parse_user(iss);

    if (!user) {
        cout << "Invalid user." << endl;
        send_tcp("RBD NOK\n", tcp_fd);
        return;
    }

    iss >> aid;
    iss >> value;

    auto db_user = db->get_user(user.value().uid);

    if (!db->validate_user(db_user.value())) {
        cout << "User not logged in." << endl;
        send_tcp("RBD NLG\n", tcp_fd);
        return;
    }

    auto auction = db->get_auction(aid);

    if (!auction) {
        cout << "Auction not found." << endl;
        send_tcp("RBD NOK\n", tcp_fd);
        return;
    }

    if (!auction.value().open) {
        cout << "Auction already closed." << endl;
        send_tcp("RBD NOK\n", tcp_fd);
        return;
    }

    if (auction.value().uid == user.value().uid) {
        cout << "User cannot bid on his own auction." << endl;
        send_tcp("RBD ILG\n", tcp_fd);
        return;
    }

    // check values of current bids
    auto bids = db->get_bids_of_auction(auction.value());
    if (bids.size() > 0) {
        int max_bid = 0;
        for (auto bid : bids) {
            if (bid.value > max_bid) {
                max_bid = bid.value;
            }
        }
        if (value <= max_bid) {
            cout << "Bid value too low." << endl;
            send_tcp("RBD REF\n", tcp_fd);
            return;
        }
    } else if (bids.size() == 0) {
        if (value <= auction.value().start_value) {
            cout << "Bid value too low." << endl;
            send_tcp("RBD REF\n", tcp_fd);
            return;
        }
    }

    Bid bid;
    bid.uid = user.value().uid;
    bid.value = value;
    bid.bid_date_time = get_current_time();
    bid.bid_sec_time =
        get_end_sec_time(auction.value().start_date_time, bid.bid_date_time);

    db->add_bid(bid, aid);

    if (v) {
        cout << "User " << user.value().uid << " bidded on auction " << aid
             << " with value " << value << endl;
    }

    send_tcp("RBD ACC\n", tcp_fd);
}

void handle_show_asset(const Request &req, int tcp_fd, Database *db, bool v) {
    string aid = req.message;
    aid = aid.substr(0, aid.size() - 1);

    if (v) {
        cout << "User requested to show asset of auction " << aid << endl;
    }

    auto auction = db->get_auction(aid);

    if (!auction) {
        cout << "Auction not found." << endl;
        send_tcp("RSA NOK\n", tcp_fd);
        return;
    }
    if (auction.value().asset_fname == "") {
        cout << "Auction has no asset." << endl;
        send_tcp("RSA NOK\n", tcp_fd);
        return;
    }

    string file_name =
        "assets/" + auction.value().aid + "_" + auction.value().asset_fname;

    int file_fd = open(file_name.c_str(), O_RDONLY);

    auto message = "RSA OK " + auction.value().asset_fname + " " +
                   to_string(auction.value().asset_fsize) + " ";
    send_tcp(message, tcp_fd);

    auto n = sendfile(tcp_fd, file_fd, 0, auction.value().asset_fsize);
    if (n == -1) {
        cout << "Error sending asset." << endl;
        return;
    }
    send_tcp("\n", tcp_fd);

    close(file_fd);
}

#endif
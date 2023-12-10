#ifndef SERVER_COMMANDS_HPP
#include "database.hpp"

struct Request {
    string type;
    string message;
    string uid;
};

Request parse_request(const string &input) {
    Request req;

    istringstream iss(input);
    iss >> req.type;

    string arg;
    while (iss >> arg) {
        req.message += arg + " ";
    }

    return req;
}

optional<User> parse_user(const string &input) {
    User user;

    istringstream iss(input);
    iss >> user.uid;

    if (iss >> user.password) {
        return user;
    } else {
        return {user}; 
    }
}


void handle_login(Request &req, Connections conns, Database *db) {
    auto user = parse_user(req.message);
    if (user.has_value()) {
        cout << "User " << user.value().uid << " logged in." << endl;

        auto db_user = db->get_user(user.value().uid);
        if (db_user.has_value()) {
            if (db_user.value().password == user.value().password) {
                db->set_logged_in(user.value().uid, true);
                send_udp("RLI OK\n", conns);
            } else {
                cout << "Invalid password." << endl;
                send_udp("RLI NOK\n", conns);
            }
        } else {
            db->add_user(user.value().uid, user.value().password);
            cout << "User " << user.value().uid << " registered." << endl;
            send_udp("RLI REG\n", conns);
        }
    } else {
        cout << "Invalid user." << endl;
        send_udp("RLI NOK\n", conns);
    }
}

void handle_logout(const Request &req, Connections conns, Database *db) {
    auto user = parse_user(req.message);
    if (user.has_value()) {
        cout << "User " << user.value().uid << " logged out." << endl;
        if (db->validate_user(user.value())) {
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

void handle_unregister(const Request &req, Connections conns, Database *db) {
    auto user = parse_user(req.message);
    if (user.has_value()) {
        cout << "User " << user.value().uid << " unregistered." << endl;
        if (db->validate_user(user.value())) {
            db->delete_user(user.value().uid);
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

void handle_list(const Request &req, Connections conns, Database *db) {
    auto auctions = db->get_auctions();
    if (auctions.size() == 0) {
        send_udp("RLS NOK\n", conns);
    } else {
        string msg = "RLS OK";
        for (auto auction : auctions) {
            msg += " " + auction.aid + " " + to_string(auction.open);
        }
        msg += "\n";
        cout << "msg: " << msg << endl;
        send_udp(msg, conns);
    }
}

void handle_my_auctions(const Request &req, Connections conns, Database *db) {
    auto u = parse_user(req.message);

    if (u.has_value()) {
        cout << "User " << u.value().uid << " requested their auctions." << endl;
        auto user = db->get_user(u.value().uid);
        if (db->validate_user(user.value())) {
            auto auctions = db->get_auctions_by_user(user.value().uid);
            if (auctions.size() == 0) {
                send_udp("RMA NOK\n", conns);
            } else {
                string msg = "RMA OK";
                for (auto auction : auctions) {
                    msg += " " + auction.aid + " " + to_string(auction.open);
                }
                msg += "\n";
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



void send_auction_record(const Auction &auction, Connections conns) {
    string msg = "RRC OK " + auction.uid + " " + auction.auction_name + " " + 
        auction.asset_fname + " " + to_string(auction.start_value) + " " +
        auction.start_date_time + " " + to_string(auction.timeactive) + "\n";
    
    if (auction.bids.size() > 0) {
        //sort
        for (auto bid : auction.bids) {
            msg += "B " + bid.uid + " " + to_string(bid.value) + " " + 
                bid.bid_date_time + " " + to_string(bid.bid_sec_time) + "\n";
        }
    }
    if (!(auction.open)) {
        msg += "E " + auction.end->end_date_time + " " + to_string(auction.end->end_sec_time) + "\n";
    }
    cout << "msg: " << msg << endl;
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

void handle_show_record(const Request &req, Connections conns, Database *db) {
    auto auc = parse_auction(req.message);

    auto auction = db->get_auction(auc.value().aid);
    if (auction.has_value()) {
        cout << "Auction found." << endl;
        send_auction_record(auction.value(), conns);
    } else {
        send_udp("RRC NOK\n", conns);
    }
}

#endif
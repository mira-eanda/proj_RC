#ifndef SERVER_COMMANDS_HPP
#include "database.hpp"

struct Request {
    string type;
    string message;
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
    iss >> user.uid >> user.password;

    return user;
}

void handle_login(const Request &req, Connections conns, Database *db) {
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
#endif
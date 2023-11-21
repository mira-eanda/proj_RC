#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
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

/*
login UID password – following this command the User application sends
a  message  to  the  AS,  using  the  UDP  protocol,  confirm  the  ID,  UID,
and password  of  this  user, or  register  it  if  this  UID  is  not  present
in  the  AS database. The  result  of  the  request  should  be  displayed:
successful  login,  incorrect  login attempt, or new user registered. Login.
Each user is identified by a user ID UID, a 6-digit IST student number, and a
password composed of 8 alphanumeric (only letters and digits) characters. When
the AS receives a login request  it will inform of a successful or incorrect
login attempt or, in case the UID was not present at the AS, register a new
user.
*/

enum status_t { OK, NOK, REG, UNR };

struct Response {
    string type;
    status_t status;
};

struct User {
    string uid;
    string password;
};

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

    return r;
}

constexpr auto numeric = "0123456789";
constexpr auto alphanumeric =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ ";

optional<User> login(vector<string> &args, int fd, struct addrinfo *res) {
    if (args.size() != 2) {
        cerr << "Invalid number of args for login command." << std::endl;
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

    // send login request to AS
    // Send a message to the server
    auto message = "LIN " + uid + " " + password + "\n";
    auto n = sendto(fd, message.c_str(), message.size(), 0, res->ai_addr,
                    res->ai_addrlen);
    if (n == -1) {
        cerr << "Error sending message to AS." << endl;
        exit(1);
    }

    // Receive a message from the server
    char buffer[128];
    n = recvfrom(fd, buffer, 128, 0, res->ai_addr, &res->ai_addrlen);
    if (n == -1) {
        cerr << "Error receiving message from AS." << endl;
        exit(1);
    }

    auto response = parse_response(buffer, "RLI");
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
    if (args.size() != 0) {
        cerr << "Invalid number of args for logout command." << std::endl;
        return;
    }

    if (!user) {
        cerr << "You must be logged in to logout." << endl;
        return;
    }

    // send logout request to AS
    // Send a message to the server
    auto message = "LOU " + user->uid + " " + user->password + "\n";
    auto n = sendto(fd, message.c_str(), message.size(), 0, res->ai_addr,
                    res->ai_addrlen);
    if (n == -1) {
        cerr << "Error sending message to AS." << endl;
        exit(1);
    }

    // Receive a message from the server
    char buffer[128];
    n = recvfrom(fd, buffer, 128, 0, res->ai_addr, &res->ai_addrlen);
    if (n == -1) {
        cerr << "Error receiving message from AS." << endl;
        exit(1);
    }

    auto response = parse_response(buffer, "RLO");
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
    if (args.size() != 0) {
        cerr << "Invalid number of args for unregister command." << std::endl;
        return;
    }

    if (!user) {
        cerr << "You must be logged in to unregister." << endl;
        return;
    }

    // send unregister request to AS
    // Send a message to the server
    auto message = "UNR " + user->uid + " " + user->password + "\n";
    auto n = sendto(fd, message.c_str(), message.size(), 0, res->ai_addr,
                    res->ai_addrlen);
    if (n == -1) {
        cerr << "Error sending message to AS." << endl;
        exit(1);
    }

    // Receive a message from the server
    char buffer[128];
    n = recvfrom(fd, buffer, 128, 0, res->ai_addr, &res->ai_addrlen);
    if (n == -1) {
        cerr << "Error receiving message from AS." << endl;
        exit(1);
    }

    auto response = parse_response(buffer, "RUR");
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

void exit_cli(vector<string> &args, int fd, struct addrinfo *res,
          optional<User> &user) {
    if (args.size() != 0) {
        cerr << "Invalid number of args for exit command." << std::endl;
        return;
    }

    if (user) {
        logout(args, fd, res, user);
    }

    cout << "Exiting..." << endl;
    exit(0);
}
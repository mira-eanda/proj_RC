#ifndef DATABASE_HPP
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>

#include "common.hpp"

using namespace std;

constexpr auto SAVE_FILE = "database.txt";

class Database {
  public:
    Database() { load_database(); }
    bool add_user(const string &uid, const string &password) {
        if (users.find(uid) != users.end()) {
            return false;
        }
        users[uid] = {uid, password, true};
        store_database();
        return true;
    }

    void set_logged_in(const string &uid, bool logged_in) {
        if (users.find(uid) == users.end()) {
            return;
        }
        users[uid].logged_in = logged_in;
        store_database();
    }

    optional<User> get_user(const string &uid) {
        if (users.find(uid) == users.end()) {
            return {};
        }
        return users[uid];
    }

    bool validate_user(User user) {
        if (users.find(user.uid) == users.end()) {
            return false;
        }
        return users[user.uid].password == user.password;
    }

  private:
    unordered_map<string, User> users;
    const string filename = SAVE_FILE;

    void store_database() {
        ofstream file(filename);
        if (file.is_open()) {
            for (const auto &entry : users) {
                file << entry.second.serialize() << '\n';
            }
            file.close();
        }
    }

    void load_database() {
        ifstream file(filename);
        if (file.is_open()) {
            string line;
            while (getline(file, line)) {
                User user;
                if (line.empty()) {
                    continue;
                }
                user = User::deserialize(line);
                users[user.uid] = user;
                cout << "Loaded user: " << user.uid << ' ' << user.password
                     << endl;
            }
            file.close();
        }
    }
};

#endif // DATABASE_HPP
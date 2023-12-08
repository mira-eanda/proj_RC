#ifndef DATABASE_HPP
#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>

#include "common.hpp"
#include "deps/json.hpp"

using namespace std;
using json = nlohmann::json;

namespace nlohmann {
template <typename T> struct adl_serializer<optional<T>> {
    static void to_json(json &j, const optional<T> &opt) {
        if (!opt) {
            j = nullptr;
        } else {
            j = *opt;
        }
    }

    static void from_json(const json &j, optional<T> &opt) {
        if (j.is_null()) {
            opt = {};
        } else {
            opt = j.template get<T>();
        }
    }
};
} // namespace nlohmann

struct User {
    string uid;
    string password;
    bool logged_in = false;
    vector<string> auctions;
    vector<string> bids;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(User, uid, password, logged_in);

struct Bid {
    string uid;
    int value;
    string bid_date_time;
    int bid_sec_time;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Bid, uid, value);

struct End {
    string end_date_time;
    int end_sec_time;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(End, end_date_time, end_sec_time);

struct Auction {
    string aid;
    string uid;
    string auction_name;
    string asset_fname;
    int start_value;
    string start_date_time;
    int timeactive;
    bool open = false;
    vector<Bid> bids;
    optional<End> end;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Auction, uid, auction_name, asset_fname,
                                   start_value, start_date_time, timeactive,
                                   bids, end);

struct Data {
    unordered_map<string, User> users;
    unordered_map<string, Auction> auctions;
    unordered_map<string, Bid> bids;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Data, users, auctions, bids);

constexpr auto SAVE_FILE = "database.json";

class Database {
  public:
    Database() { load_database(); }
    bool add_user(const string &uid, const string &password) {
        if (data.users.find(uid) != data.users.end()) {
            return false;
        }
        data.users[uid] = {uid, password, true};
        store_database();
        return true;
    }

    void set_logged_in(const string &uid, bool logged_in) {
        if (data.users.find(uid) == data.users.end()) {
            return;
        }
        data.users[uid].logged_in = logged_in;
        store_database();
    }

    void delete_user(const string &uid) {
        if (data.users.find(uid) == data.users.end()) {
            return;
        }
        data.users.erase(uid);
        store_database();
    }

    optional<User> get_user(const string &uid) {
        if (data.users.find(uid) == data.users.end()) {
            return {};
        }
        return data.users[uid];
    }

    bool validate_user(User user) {
        if (data.users.find(user.uid) == data.users.end()) {
            return false;
        }
        return data.users[user.uid].password == user.password;
    }

    void add_auction(const Auction &auction) {
        data.auctions[auction.aid] = auction;
        store_database();
    }

    vector<Auction> get_auctions_by_user(const string &uid) {
        vector<Auction> auctions;
        for (auto auction : data.auctions) {
            if (auction.second.uid == uid) {
                auctions.push_back(auction.second);
            }
        }
        return auctions;
    }

    optional<Auction> get_auction(const string &aid) {
        if (data.auctions.find(aid) == data.auctions.end()) {
            return {};
        }
        return data.auctions[aid];
    }
    
  private:
    Data data;
    const string filename = SAVE_FILE;

    void store_database() {
        json j = data;
        ofstream file(filename);
        file << j.dump(4);
        file.close();
    }

    void load_database() {
        ifstream file(filename);
        if (file.is_open()) {
            json j;
            file >> j;
            data = j;

            file.close();
        }
    }
};

#endif // DATABASE_HPP
#ifndef DATABASE_HPP
#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <chrono>

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

struct Bid {
    string uid;
    string aid;
    int value;
    string bid_date_time;
    int bid_sec_time;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Bid, aid, value);

struct User {
    string uid;
    string password;
    bool logged_in = false;
    vector<string> host_auctions;
    vector<Bid> bids;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(User, uid, password, logged_in,
                                   host_auctions, bids);

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
    int asset_fsize;
    int start_value;
    string start_date_time;
    int timeactive;
    bool open = false;
    vector<Bid> bids;
    optional<End> end;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Auction, aid, uid, auction_name, asset_fname,
                                   start_value, start_date_time, timeactive,
                                   open, bids, end);

struct Data {
    unordered_map<string, User> users;
    unordered_map<string, Auction> auctions;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Data, users, auctions);

constexpr auto SAVE_FILE = "database.json";


string get_current_time() {
    time_t now = time(0);
    tm *ltm = gmtime(&now);
    char buffer[80];
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", ltm);
    return string(buffer);
}

bool check_auction_ended(const Auction &auction) {
    // current time
    auto now = chrono::system_clock::now();
    auto now_time_t = chrono::system_clock::to_time_t(now);

    // auction start time
    struct tm tm_start;
    istringstream iss(auction.start_date_time);
    iss >> get_time(&tm_start, "%Y-%m-%d %H:%M:%S");
    auto start_time_t = mktime(&tm_start);

    // expected end time
    auto end_time_t = start_time_t + auction.timeactive;

    cout << "now_time_t: " << now_time_t << endl;
    cout << "start_time_t: " << start_time_t << endl;
    cout << "end_time_t: " << end_time_t << endl;

    return now_time_t > end_time_t;
}

int get_end_sec_time(const string &start, const string &end) {
    struct tm tm_start, tm_end;

    strptime(start.c_str(), "%Y-%m-%d %H:%M:%S", &tm_start);
    strptime(end.c_str(), "%Y-%m-%d %H:%M:%S", &tm_end);

    time_t start_time = mktime(&tm_start);
    time_t end_time = mktime(&tm_end);

    return difftime(end_time, start_time);
}

class Database {
  public:
    Database() { load_database(); }

    void close_ended_auctions() {
        cout << "Checking for ended auctions..." << endl;
        for (auto record : data.auctions) {
            if (record.second.open) {
                if (check_auction_ended(record.second)) {
                    cout << "Auction " << record.first << " ended." << endl;
                    close_auction(record.second);
                }
            }
        }
        store_database();
    }

    void close_auction(const Auction &auction) {
        End end;
        end.end_date_time = get_current_time();
        end.end_sec_time =
            get_end_sec_time(auction.start_date_time, end.end_date_time);

        data.auctions[auction.aid].open = false;
        data.auctions[auction.aid].end = end;
    }

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

    void update_auction(const Auction &auction) {
        data.auctions[auction.aid] = auction;
        store_database();
    }

    vector<Auction> get_auctions() {
        vector<Auction> auctions;
        for (auto auction : data.auctions) {
            auctions.push_back(auction.second);
        }
        return auctions;
    }

    bool check_auction_open(const string &aid) {
        auto auction = get_auction(aid);
        return auction.value().open;
    }

    vector<Bid> get_bids_of_auction(Auction auction) {
        vector<Bid> bids;
        for (auto bid : auction.bids) {
            bids.push_back(bid);
        }
        sort(bids.begin(), bids.end(),
             [](const Bid &a, const Bid &b) { return a.value < b.value; });
        return bids;
    }

    vector<Bid> get_bids_by_user(const string &uid) {
        auto user = get_user(uid);
        vector<Bid> bids;
        for (auto bid : user.value().bids) {
            bids.push_back(bid);
        }
        return bids;
    }

    void add_bid_to_user(const Bid &bid, const string &uid) {
        data.users[uid].bids.push_back(bid);
        data.auctions[bid.aid].bids.push_back(bid);
        store_database();
    }

    string generate_aid() {
        int max_aid = 0;
        for (const auto &auction_pair : data.auctions) {
            int aid = stoi(auction_pair.first);
            max_aid = max(max_aid, aid);
        }
        max_aid++;

        stringstream ss;
        ss << setw(3) << setfill('0') << max_aid;
        return ss.str();
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
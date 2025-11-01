#ifndef FILE_HEADER_H
#define FILE_HEADER_H

#include <bits/stdc++.h>
#include <string>
using namespace std;

// ------------------------------------------------------- FILE MANAGER -------------------------------------------------------
struct Address {
    string ip;
    int port;
};

struct FileInfo {
    // Order: name|path|owner|group|size|piece_size|full_SHA|piece_SHA (comma separated)|piece_users (piece:comma separated users)
    string name;
    string path;
    string owner;
    string group;
    uint64_t size;
    uint64_t piece_size;
    string full_SHA;
    vector<string> piece_SHA;
    map<string, Address> seeder_users;
    map<string, string> user_file_map;

    // Serialize FileInfo to a string with '|' delimiter
    string toString() const {
        stringstream ss;
        ss << name << "|" << path << "|" << owner << "|" << group << "|"
           << size << "|" << piece_size << "|" << full_SHA << "|";

        for (size_t i = 0; i < piece_SHA.size(); ++i) {
            ss << piece_SHA[i];
            if (i + 1 < piece_SHA.size()) ss << ",";
        }
        ss << "|";

        bool first = true;
        for (const auto& [user, addr] : seeder_users) {
            if (!first) ss << ";";
            ss << user << ":" << (addr.ip.empty() ? "0.0.0.0" : addr.ip) << ":" << addr.port;
            first = false;
        }
        ss << "|";

        first = true;
        for (const auto& [user, file] : user_file_map) {
            if (!first) ss << ";";
            ss << user << ":" << file;
            first = false;
        }

        return ss.str();
    }

    // Deserialize a string to FileInfo
    static FileInfo fromString(const std::string& data) {
        FileInfo fileInfo;
        std::stringstream ss(data);
        std::string token;

        // Basic fields
        getline(ss, fileInfo.name, '|');
        getline(ss, fileInfo.path, '|');
        getline(ss, fileInfo.owner, '|');
        getline(ss, fileInfo.group, '|');

        std::string size_str, piece_size_str;
        getline(ss, size_str, '|');
        getline(ss, piece_size_str, '|');
        fileInfo.size = size_str.empty() ? 0 : std::stoull(size_str);
        fileInfo.piece_size = piece_size_str.empty() ? 0 : std::stoull(piece_size_str);

        getline(ss, fileInfo.full_SHA, '|');

        // piece_SHA (comma-separated)
        std::string piece_sha_str;
        getline(ss, piece_sha_str, '|');
        {
            std::stringstream piece_ss(piece_sha_str);
            std::string piece;
            while (getline(piece_ss, piece, ',')) {
                if (!piece.empty()) fileInfo.piece_SHA.push_back(piece);
            }
        }

        // seeder_users (user:ip:port;...)
        std::string seeder_str;
        getline(ss, seeder_str, '|');
        {
            std::stringstream su_ss(seeder_str);
            std::string entry;
            while (getline(su_ss, entry, ';')) {
                size_t first_colon = entry.find(':');
                size_t second_colon = entry.find(':', first_colon + 1);
                if (first_colon != std::string::npos && second_colon != std::string::npos) {
                    std::string user = entry.substr(0, first_colon);
                    std::string ip = entry.substr(first_colon + 1, second_colon - first_colon - 1);
                    int port = std::stoi(entry.substr(second_colon + 1));
                    fileInfo.seeder_users[user] = Address{ip, port};
                }
            }
        }

        // user_file_map (user:file;...)
        std::string ufm_str;
        getline(ss, ufm_str, '|');
        {
            std::stringstream ufm_ss(ufm_str);
            std::string entry;
            while (getline(ufm_ss, entry, ';')) {
                size_t colon = entry.find(':');
                if (colon != std::string::npos) {
                    std::string user = entry.substr(0, colon);
                    std::string file = entry.substr(colon + 1);
                    fileInfo.user_file_map[user] = file;
                }
            }
        }

        return fileInfo;
    }
};


#endif
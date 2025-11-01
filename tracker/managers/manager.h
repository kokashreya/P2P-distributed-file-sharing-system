#ifndef MANAGER_H
#define MANAGER_H

#include <bits/stdc++.h>
#include <string.h>
#include "../headers/logger_header.h"
#include <mutex>
using namespace std;

class Tracker;

struct Address {
    string ip;
    int port;
};



// ------------------------------------------------------- USER MANAGER -------------------------------------------------------
class UserManager {
private:
    unordered_map<string, string> users;
    unordered_map<string, Address> logged_in;
    mutex mtx; 


public:
    bool registerUser(const string& username, const string& password);
    bool isUser(const string& username);
    bool login(const string& username, const string& password, const Address& addr);
    bool logout(const string& username);
    bool isLoggedIn(const string& username);
    bool getUserAddress(const string& username, Address& addr);
    void showLoggedInUsers();
};

// ------------------------------------------------------- GROUP MANAGER -------------------------------------------------------
struct GroupInfo {
    string owner;
    vector<string> members;
    unordered_set<string> pending; 
};

class GroupManager {
private:
    unordered_map<string, GroupInfo> groups;
    unordered_map<string, unordered_set<string>> owner_groups; // Maps owner to the list of group's owners 
    mutex group_mtx;

public:
    bool createGroup(const string& owner,const string& group_name);
    string getOwnerName(const string &group_name);
    bool isGroupAvailabel(const string &group_name);
    vector<string> groupList();
    bool requestToJoin(const string& user, const string& group_name );
    bool isMemberOfGroup(const string& user, const string& group_name );
    unordered_map<string, unordered_set<string>> showPendingRequests(const string& user, const string& group_name );
    bool isGroupOwner(const string &user, const string &group_name);
    bool approveRequest(const string& approver, const string& user, const string& group_name);
    bool isPenddingRequest(const string &user,const string &group_name);
    bool leaveGroup( const string& user, const string& group_name);
    void showGroup(const string& group_name);
};

// ------------------------------------------------------- FILE MANAGER -------------------------------------------------------


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

class FileManager{
private:

    unordered_map<string,unordered_map<string,FileInfo>> group_files;
    mutex file_mtx;

public:
    bool addFile(const string& owner,const string& group,const string& filename,const FileInfo& fileInfo);
    bool isFileExist(const string& group,const string& filename);
    FileInfo getFileInfo(const string& group,const string& filename);
    vector<string> listFilesInGroup(const string& group);
    map<string, Address> seeder_list(const string& group,const string& filename);
    bool add_new_seeder(const string& group,const string& filename,const string& username,const Address& addr,string new_file_path);
    bool remove_seeder(const string& username,const string& group,const string& filename);

};


// ------------------------------------------------------- COMMAND MANAGER -------------------------------------------------------

class CommandManager {
private:
    Tracker* tracker;
    shared_ptr<UserManager> um;
    shared_ptr<GroupManager> gm;
    shared_ptr<FileManager> fm;
    shared_ptr<Logger> logger;
  
 

public:
    CommandManager(Tracker* tracker,  UserManager* um, GroupManager* gm, FileManager* fm, Logger* logger);
    bool login_command(string& reply,string username,string password,Address* client_address,string sync_prefix);
    bool create_user_command(string& reply, string username, string password, Address* client_address, string sync_prefix);
    bool logout_command(string& reply,string username,Address* client_address,string  sync_prefix);

    bool create_group_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix);
    bool list_group_command(string& reply,Address* client_address,string  sync_prefix);
    bool join_group_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix);
    bool leave_group_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix);
    bool list_request_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix);
    bool accept_request_command(string& reply,string ownername,string username,string group_id,Address* client_address,string  sync_prefix);
    bool upload_file_command(string& reply,string username,string group_id,string filename,Address* client_address,string  sync_prefix);
    vector<string> extract_files_which_have_one_or_more_seeders(string group_id,vector<string> file_list);
    bool upload_file_data(string& reply,string username,string group_id,string filename,FileInfo finfo,Address* client_address,string  sync_prefix);
    bool list_files_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix);
    bool download_file_command(string& reply,string username,string group_id,string filename,Address* client_address,string  sync_prefix);
    map<string, Address> get_available_seeders(map<string, Address> seeder_map);
    bool update_file_info(string& reply,string username,string group_id,string filename,string new_file_path,Address* client_address,string  sync_prefix);
    bool stop_share(string& reply,string username,string group_id,string filename,Address* client_address,string  sync_prefix);
    bool sync_handler(string cmd);

};

// ------------------------------------------------------- CLIENT MANAGER -------------------------------------------------------


class ClientManager {
private:
    Tracker* tracker;
    shared_ptr<UserManager> um;
    shared_ptr<GroupManager> gm;
    shared_ptr<FileManager> fm;
    shared_ptr<Logger> logger;
    shared_ptr<CommandManager> command_manager;
    int socket_id;
    string ip;
    int port;
    bool logged_in;
    string username;
    Address client_address;
    bool socket_closed = false;
    
    
    bool send_message(string message);
    bool login_loop();
    bool while_loop();
    void notify_sync(string message);
    void init_unexpected_close();

public:
    ClientManager(Tracker* tracker,UserManager* um,GroupManager* gm, FileManager* fm, Logger* logger, CommandManager* command_manager,int socket_id, string ip,int port);
    void start_communication();

};



#endif
#include "manager.h"

bool FileManager::addFile(const string& owner, const string& group, const string& filename,const FileInfo& fileInfo) {
    lock_guard<mutex> lock(file_mtx);
    if (group_files[group].count(filename) > 0) {
        return false;  // File already exists
    }
    group_files[group][filename] = fileInfo;
    return true;
}

bool FileManager::isFileExist(const string& group, const string& filename) {
    lock_guard<mutex> lock(file_mtx);
    return group_files.count(group) > 0 && group_files[group].count(filename) > 0;
}

FileInfo FileManager::getFileInfo(const string& group, const string& filename) {
    lock_guard<mutex> lock(file_mtx);
    if (group_files.count(group) > 0 && group_files[group].count(filename) > 0) {
        return group_files[group][filename];
    }
    FileInfo emptyFile;
    return emptyFile;  // Return an empty FileInfo if not found
}

vector<string>  FileManager::listFilesInGroup(const string& group) {
    lock_guard<mutex> lock(file_mtx);
    vector<string> files;
    if (group_files.count(group) > 0) {
        for (const auto& [filename, _] : group_files[group]) {
            files.push_back(filename);
        }
    }
    return files;
}

map<string, Address>  FileManager::seeder_list(const string& group, const string& filename) {
    lock_guard<mutex> lock(file_mtx);
    map<string, Address> seeders;
    if (group_files.count(group) > 0 && group_files[group].count(filename) > 0) {
        const FileInfo& finfo = group_files[group][filename];
        return finfo.seeder_users;
    }
    return seeders;
}

bool FileManager::add_new_seeder(const string& group,const string& filename,const string& username,const Address& addr,string new_file_path) {
    lock_guard<mutex> lock(file_mtx);
    if (group_files.count(group) > 0 && group_files[group].count(filename) > 0) {
        group_files[group][filename].seeder_users[username] = addr;
        group_files[group][filename].user_file_map[username] = new_file_path;

        for (auto& [user, addr] : group_files[group][filename].seeder_users) {
            cout<<"Seeder: " << user << " at " << addr.ip << ":" << addr.port << endl;
        }
        
        return true;
    }
    
    return false;
}

bool FileManager::remove_seeder(const string& username,const string& group,const string& filename) {
    lock_guard<mutex> lock(file_mtx);

    if (group_files.count(group) > 0 && group_files[group].count(filename) > 0) {
        auto& seeder_map = group_files[group][filename].seeder_users;
        auto& user_file_map = group_files[group][filename].user_file_map;

        
        if (seeder_map.erase(username) > 0) {
            user_file_map.erase(username); // Also remove from user_file_map
            if(seeder_map.empty()){
                group_files[group].erase(filename);
            }
            else{
                group_files[group][filename].owner = seeder_map.begin()->first ;
                group_files[group][filename].path = user_file_map[group_files[group][filename].owner];
            }
            return true;
        }

    }
    return false;
}

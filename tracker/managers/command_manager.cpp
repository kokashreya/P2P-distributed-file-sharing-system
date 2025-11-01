#include "manager.h"
#include "../headers/utils_header.h"
#include "../headers/tracker_header.h"
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>
#include <thread>
#include <vector>
#include <cstring>
#include <unistd.h>
using namespace std;

CommandManager:: CommandManager(Tracker* tracker, UserManager* um, GroupManager* gm, FileManager* fm, Logger* logger): tracker(tracker), um(um), gm(gm), fm(fm), logger(logger) {}


bool CommandManager::sync_handler(string cmd){

    
    vector<string> tokens;
    string reply;
    tokenize(cmd, tokens);
    if(tokens.empty()) {
        return false;
    }

    if(tokens.size() < 5 ) {
        return false;
    }

    string ip = tokens[1];
    string port_str = tokens[2];
    
    int port;
    try {
        port = stoi(port_str);
    } 
    catch (...) {
        logger->log("SYNC handler: Invalid port received: " + port_str, "", 0, "ERROR",true);
        return false;
    }

    // tokens[3].erase(std::remove(tokens[3].begin(), tokens[3].end(), '\0'), tokens[3].end());

    Address client_address{ip, port};
   
    
    if(tokens[3] == "login") {
        if(tokens.size() < 6) return false;
        string username = tokens[4];
        string password = tokens[5];
        return login_command(reply, username, password, &client_address, "SYNC_");
    }
    
    else if(tokens[3] == "create_user") {
        if(tokens.size() < 6) return false;
        string username = tokens[4];
        string password = tokens[5];
        return create_user_command(reply, username, password, &client_address, "SYNC_");
    }

    else if(tokens[3]=="logout"){
        string username=tokens[4];
        return logout_command(reply,username,&client_address,"SYNC_");
    }

    else if(tokens[3]=="create_group"){
        if(tokens.size() < 6) return false;
        string group_id=tokens[4];
        string username=tokens[5];
        return create_group_command(reply,username,group_id,&client_address,"SYNC_");
    }

    else if(tokens[3]=="join_group"){
        if(tokens.size() < 6) return false;
        string group_id=tokens[4];
        string username=tokens[5];
        return join_group_command(reply,username,group_id,&client_address,"SYNC_");
    }

    else if(tokens[3]=="accept_request"){
        if(tokens.size() < 7) return false;
        string group_id=tokens[4];
        string requestname=tokens[5];
        string username=tokens[6];
        return accept_request_command(reply,username,requestname,group_id,&client_address,"SYNC_");
    }

    else if(tokens[3]=="leave_group"){
        if(tokens.size() < 6) return false;
        string group_id=tokens[4];
        string username=tokens[5];
        return leave_group_command(reply,username,group_id,&client_address,"SYNC_");
    }

    else if(tokens[3]=="upload_file_data"){

        // remove intiaal command and trim it <SYNC IP PORT upload_file_data <file_info>>
        size_t pos = cmd.find("upload_file_data");
        std::string file_info = cmd.substr(pos + strlen("upload_file_data"));
        trim_whitespace(file_info);
        FileInfo finfo = FileInfo::fromString(file_info);
        string username=finfo.owner;
        string group_id=finfo.group;
        string file_name=finfo.name;
        return upload_file_data(reply,username,group_id,file_name,finfo,&client_address,"SYNC_");
    }

    else if(tokens[3]=="update_file_info"){
        if(tokens.size() < 7) return false;
        string group_id=tokens[4];
        string file_name=tokens[5];
        file_name = file_name.substr(file_name.find_last_of("/\\") + 1);
        string new_file_path=tokens[6];
        string username=tokens[7];
        cout<<cmd<<endl;
        return update_file_info(reply,username,group_id,file_name,new_file_path,&client_address,"SYNC_");
    }

    else if(tokens[3]=="stop_share"){
        if(tokens.size() < 6) return false;
        string group_id=tokens[4];
        string file_name=tokens[5];
        file_name = file_name.substr(file_name.find_last_of("/\\") + 1);
        string username=tokens[6];
        return stop_share(reply,username,group_id,file_name,&client_address,"SYNC_");
    }
    
    return false;

}

bool CommandManager::create_user_command(string& reply, string username, string password, Address* client_address, string sync_prefix = ""){
    if(um->isUser(username)) {
        reply = "Username already exists. Please choose a different username.\n";
        logger->log(sync_prefix + "Failed attempt to create user " + username, client_address->ip, client_address->port, "ERROR", true);
        return false;
    }

    if(um->registerUser(username, password)) {
        reply = "User created successfully. Please login now.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "New user registered: " + username, client_address->ip, client_address->port, tag, true);
        return true;
    } else {
        reply = "Failed to create user due to unknown error.\n";
        logger->log(sync_prefix + "Failed attempt to create user " + username, client_address->ip, client_address->port, "ERROR", true);
        return false;
    }
}

bool CommandManager::login_command(string& reply,string username,string password,Address* client_address,string  sync_prefix=""){
    
    if(um->isLoggedIn(username)) {
        reply = "You are already logged in.\n";
        return false;
    }

    if(um->login(username, password, *client_address)) {
        reply = "Login successful.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "User " + username + " logged in.", client_address->ip, client_address->port,tag, true);
        return true;
    }
    else if(um->isUser(username)) {
        reply = "Your password is wrong.\n";
        logger->log(sync_prefix + "Failed login attempt for user " + username, client_address->ip, client_address->port, "ERROR", false);
    }
    else {
        reply = "You are not a user. First call create_user command.\n";
        logger->log(sync_prefix + "Failed login attempt for user " + username, client_address->ip, client_address->port, "ERROR", false);
    }
    return false;

}

bool CommandManager::logout_command(string& reply,string username,Address* client_address,string  sync_prefix=""){
    if(!um->isLoggedIn(username)) {
        reply = "You are not logged in.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "User " + username + " try to logout. But not logged in", client_address->ip, client_address->port,tag, true);
        return true;
    }
    if(um->logout(username)){
        reply = "Logout successful.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "User " + username + " logout.", client_address->ip, client_address->port,tag, true);
        return true; 
    }
    reply = "Logout try unsuccessful.\n";
    string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
    logger->log(sync_prefix + "User " + username + " try to logout.", client_address->ip, client_address->port,tag, true);
    return false;
}

bool CommandManager::create_group_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix=""){
    if(gm->isGroupAvailabel(group_id)){
        reply = "Group id alredy registered.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to create by "+username, client_address->ip, client_address->port,tag, true);
        return false;
    }

    gm->createGroup(username,group_id);
    reply = "Group is registered as "+group_id + ".\n";
    string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
    logger->log(sync_prefix + "Group " + group_id + " create by "+username, client_address->ip, client_address->port,tag, true);
    return true;

}

bool CommandManager::list_group_command(string& reply,Address* client_address,string  sync_prefix=""){
    vector<string>  result=gm->groupList();
    if(result.empty()){
        reply="No group available\n";
        return true;
    }
    reply="";
    for(auto s:result){
        reply=reply+s+"\n";
    }
    return true;
}

bool CommandManager::join_group_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix=""){
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id+" is not available.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to jon by "+username, client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(gm->isGroupOwner(username,group_id)){
        reply = "You are alredy owner of Group name is "+group_id+".\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to join  by "+username+" but he is alredy owner", client_address->ip, client_address->port,tag, true);
        return false;

    }


    if(gm->isMemberOfGroup(username,group_id)){
        reply = "You are alredy member of Group name is "+group_id+".\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to join  by "+username+" but he is alredy member", client_address->ip, client_address->port,tag, true);
        return false;

    }

    gm->requestToJoin(username,group_id);
    reply = "Group name "+group_id + " join request is placed wait until Owner accept it.\n";
    string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
    logger->log(sync_prefix + "Group " + group_id + " join request placed by "+username, client_address->ip, client_address->port,tag, true);
    return true;

}

bool CommandManager::list_request_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix=""){
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id+" is not available.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to show request by "+username, client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!gm->isGroupOwner(username,group_id)){
        reply = "You are not owner of Group name is "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to show request  by "+username+" but he is not owner", client_address->ip, client_address->port,tag, true);
        return false;

    }

    unordered_set<string> group_reuest=gm->showPendingRequests(username,group_id)[group_id];
    if(group_reuest.empty()){
        reply="No request available\n";
        return true;
    }
    reply="";
    for(auto s:group_reuest){
        reply=reply+s+"\n";
    }
    return true;

}

bool CommandManager::accept_request_command(string& reply,string ownername,string requestedname,string group_id,Address* client_address,string  sync_prefix){
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id+" is not available.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to accept request by "+ownername, client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!gm->isGroupOwner(ownername,group_id)){
        reply = "You are not owner of Group name is "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to accept request by "+ownername+" but he is not owner", client_address->ip, client_address->port,tag, true);
        return false;

    }


    if(gm->isMemberOfGroup(requestedname,group_id)){
        reply = requestedname+" is alredy member of Group name is "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to accept request by "+ownername+" but he is alredy member", client_address->ip, client_address->port,tag, true);
        return false;

    }

    
    if(!gm->isPenddingRequest(requestedname,group_id)){
        reply = "There is not pendding request of "+ requestedname+" in group "+group_id+" .\n";
        return false;
    }

    gm->approveRequest(ownername,requestedname,group_id);
    reply = "You are succesfully accepted request of "+requestedname+" in "+group_id+".\n";
    string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
    logger->log(sync_prefix + "Group " + group_id + " accepted request by "+ownername+" of "+requestedname, client_address->ip, client_address->port,tag, true);
    return true;
}

bool CommandManager::leave_group_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix=""){
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id+" is not available.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to leave by "+username, client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!gm->isMemberOfGroup(username,group_id)){
        reply = "You are not member of Group name is "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to leave  by "+username+" but he is not member", client_address->ip, client_address->port,tag, true);
        return false;
    }

    bool is_owner=gm->isGroupOwner(username,group_id);
    gm->leaveGroup(username,group_id);
    string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id + " is leaved by you and you are last so group removed\n";
        logger->log(sync_prefix + "Group " + group_id + " is leave by "+username+" and you are last so group removed", client_address->ip, client_address->port,tag, true);
        return true;
    }
    

    string new_owner=gm->getOwnerName(group_id);

    if(is_owner && new_owner!=""){
        reply = "Group name "+group_id + " is leaved by you and new owner is user "+new_owner+"\n";
        logger->log(sync_prefix + "Group " + group_id + " is leaved by "+username + " and new owner is user "+ new_owner, client_address->ip, client_address->port,tag, true);
    }

    else{
        reply = "Group name "+group_id + " is leaved by you.\n";
        logger->log(sync_prefix + "Group " + group_id + " is leaved by "+username, client_address->ip, client_address->port,tag, true);
    }
    return true;

}

bool CommandManager::upload_file_command(string& reply,string username,string group_id,string filename,Address* client_address,string  sync_prefix=""){
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id+" is not available.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to upload file by "+username, client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!gm->isMemberOfGroup(username,group_id) && !gm->isGroupOwner(username,group_id)){
        reply = "You are not member of Group name is "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to upload file by "+username+" but he is not member", client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(fm->isFileExist(group_id,filename)){
        reply = "File name "+filename+" is already exist in group "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "File " + filename + " try to upload in group "+group_id+" by "+username+" but file is already exist", client_address->ip, client_address->port,tag, true);
        return false;
    }

    reply = "send_all_data.\n";
    return true;

}

bool CommandManager::upload_file_data(string& reply,string username,string group_id,string filename,FileInfo finfo,Address* client_address,string  sync_prefix=""){

    if(!upload_file_command(reply,username,group_id,filename,client_address,sync_prefix)){
        return false;
    }

    if(fm->addFile(username,group_id,filename,finfo)){
        reply = "File name "+filename+" is successfully added in group "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "File " + filename + " is uploaded in group "+group_id+" by "+username, client_address->ip, client_address->port,tag, true);
        return true;
    }

    reply = "File name "+filename+" is not added in group "+group_id+" because it already exists.\n";
    string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
    logger->log(sync_prefix + "File " + filename + " is not uploaded in group "+group_id+" by "+username, client_address->ip, client_address->port,tag, true);
    return false;
    
}

vector<string> CommandManager::extract_files_which_have_one_or_more_seeders(string group_id,vector<string> file_list) {
    vector<string> result;

    for (const string& file_entry : file_list) {
        map<string, Address> seeders = fm->seeder_list(group_id, file_entry);

        if (seeders.empty()) {
            continue; // no seeders at all
        }

        bool has_live_seeder = false;

        // check if at least one seeder is logged in
        for (const auto& [username, addr] : seeders) {
            if (um->isLoggedIn(username)) {
                has_live_seeder = true;
                break; // no need to check more
            }
        }

        if (has_live_seeder) {
            result.push_back(file_entry);
        }
    }

    return result;
}

bool CommandManager::list_files_command(string& reply,string username,string group_id,Address* client_address,string  sync_prefix=""){
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id+" is not available.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to list files by "+username, client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!gm->isMemberOfGroup(username,group_id) && !gm->isGroupOwner(username,group_id)){
        reply = "You are not member of Group name is "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to list files by "+username+" but he is not member", client_address->ip, client_address->port,tag, true);
        return false;
    }

    vector<string>  file_list=fm->listFilesInGroup(group_id);
    vector<string>  result=extract_files_which_have_one_or_more_seeders(group_id, file_list);
    if(result.empty()){
        reply="No file available in group "+group_id+"\n";
        return true;
    }
    reply="";
    for(auto s:result){
        reply=reply+s+"\n";
    }
    return true;

}

map<string, Address> CommandManager::get_available_seeders(map<string, Address> seeder_map) {
    map<string, Address> available_seeders;
    for (const auto& [username, addr] : seeder_map) {
        if (um->isLoggedIn(username)) {
            available_seeders[username] = addr;
        }
    }
    return available_seeders;
}

bool CommandManager::download_file_command(string &reply, string username, string group_id, string filename, Address *client_address, string sync_prefix){
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id+" is not available.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to download file by "+username, client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!gm->isMemberOfGroup(username,group_id)){
        reply = "You are not member of Group name is "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to download file by "+username+" but he is not member", client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!fm->isFileExist(group_id,filename)){
        reply = "File name "+filename+" is not exist in group "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "File " + filename + " try to download in group "+group_id+" by "+username+" but file is not exist", client_address->ip, client_address->port,tag, true);
        return false;
    }

    vector<string> file_list = extract_files_which_have_one_or_more_seeders(group_id, {filename});

    if(file_list.empty()){
        reply = "File name "+filename+" is not available for download in group no seeder available "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "File " + filename + " try to download in group "+group_id+" by "+username+" but no seeder available", client_address->ip, client_address->port,tag, true);
        return false;
    }

    FileInfo finfo=fm->getFileInfo(group_id,filename);
    finfo.seeder_users=get_available_seeders(finfo.seeder_users);
    finfo.seeder_users.erase(username); // remove self from seeder list if present
    reply="file_data "+ finfo.toString()+"\n";
    
    string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
    logger->log(sync_prefix + "File " + filename + " is sended to download in group "+group_id+" by "+username, client_address->ip, client_address->port,tag, true);
    return true;
}

bool CommandManager::update_file_info(string& reply,string username,string group_id,string filename,string new_file_path,Address* client_address,string  sync_prefix=""){
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id+" is not available.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to update file info by "+username, client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!gm->isMemberOfGroup(username,group_id) && !gm->isGroupOwner(username,group_id)){
        reply = "You are not member of Group name is "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to update file info by "+username+" but he is not member", client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!fm->isFileExist(group_id,filename)){
        reply = "File name "+filename+" is not exist in group "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "File " + filename + " try to update file info in group "+group_id+" by "+username+" but file is not exist", client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(fm->add_new_seeder(group_id, filename, username, *client_address,new_file_path)){
        reply = "File path for file name "+filename+" is successfully updated in group "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "File " + filename + " file path is updated in group "+group_id+" by "+username, client_address->ip, client_address->port,tag, true);
        return true;
    }
    reply = "Failed File path for file name "+filename+" is not updated in group "+group_id+" because of unknown error.\n";
    string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
    logger->log(sync_prefix + "File " + filename + " file path is not updated in group "+group_id+" by "+username, client_address->ip, client_address->port,tag, true);
    return false;

}

bool CommandManager::stop_share(string& reply,string username,string group_id,string filename,Address* client_address,string  sync_prefix=""){
    if(!gm->isGroupAvailabel(group_id)){
        reply = "Group name "+group_id+" is not available.\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to stop share file by "+username, client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!gm->isMemberOfGroup(username,group_id) && !gm->isGroupOwner(username,group_id)){
        reply = "You are not member of Group name is "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "Group " + group_id + " try to stop share file by "+username+" but he is not member", client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(!fm->isFileExist(group_id,filename)){
        reply = "File name "+filename+" is not exist in group "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "File " + filename + " try to stop share file in group "+group_id+" by "+username+" but file is not exist", client_address->ip, client_address->port,tag, true);
        return false;
    }

    if(fm->remove_seeder(username,group_id,filename)){
        reply = "You are successfully stopped sharing file name "+filename+" in group "+group_id+" .\n";
        string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
        logger->log(sync_prefix + "File " + filename + " sharing is stopped in group "+group_id+" by "+username, client_address->ip, client_address->port,tag, true);
        return true;
    }
    reply = "Failed to stop sharing file name "+filename+" in group "+group_id+" because of unknown error.\n";
    string tag = sync_prefix.length() > 0 ? "SYNC" : "INFO";
    logger->log(sync_prefix + "File " + filename + " sharing is not stopped in group "+group_id+" by "+username, client_address->ip, client_address->port,tag, true);
    return false;
}

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


static inline uint64_t htonll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN
    return (((uint64_t)htonl((uint32_t)(v & 0xffffffffULL))) << 32) |
           htonl((uint32_t)(v >> 32));
#else
    return v;
#endif
}
static inline uint64_t ntohll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN
    return (((uint64_t)ntohl((uint32_t)(v & 0xffffffffULL))) << 32) |
           ntohl((uint32_t)(v >> 32));
#else
    return v;
#endif
}

ClientManager:: ClientManager(Tracker* tracker, UserManager* um, GroupManager* gm,FileManager* fm, Logger* logger,CommandManager* command_manager,int socket_id, string ip, int port)
        : tracker(tracker), um(um), gm(gm), fm(fm), logger(logger),command_manager(command_manager),  socket_id(socket_id), ip(ip), port(port), logged_in(false) 
    {
        username = "";
        client_address = {ip, port};
    }
    
//this furniton notify call from it is trcaker parent funtion
void ClientManager::notify_sync(string message) {
        string new_mesage="SYNC " + ip +" "+to_string(port)+" " + message;
        tracker->start_sync(new_mesage);
}

bool ClientManager::send_message(string message){
    if(socket_closed) return false;
    string reply = message;
    int sent_bytes = send(socket_id, reply.c_str(), reply.size(), 0);
    if (sent_bytes == -1) {
        logger->log("Error Come during Sending Message", ip, port,"ERROR",true);
        return false;
    }
    return true;
}

//this loop continue run until use logged in
bool ClientManager::login_loop(){
    char buffer[1024];

    
    while(!logged_in){
        memset(buffer, 0, sizeof(buffer));

        // user send command to to login and create user, until sucesfully
        int bytes = recv(socket_id, buffer, sizeof(buffer)-1, 0);

        if(bytes<=0){
            logger->log("Client unexpected disconnection",ip,port,"FAILED",true);
            string reply;
            command_manager->logout_command(reply,username,&client_address,"");
            notify_sync("logout "+username);
            if(!socket_closed){
                close(socket_id);
                socket_closed = true;
            }
            return false;

        }

        // convert trime and tokennize it
        string message="";
        if (bytes > 0) {
            buffer[bytes] = '\0';
            message = buffer;
        }

        trim_whitespace(message);
        if(message.empty()) {
            send_message("Invalid command. Please try again.\n");
            continue;
        }

        //tokenize user message
        vector<string> tokens;
        string reply;
        tokenize(message, tokens);
        if(tokens.empty()) {
            send_message("Invalid command. Please try again.\n");
            continue;
        }
        
        // login token
        if(tokens[0] == "login") {
            if(tokens.size() != 3) {
                send_message("Usage: login <username> <password>\n");
                continue;
            }
            username = tokens[1];
            string password = tokens[2];
            logged_in=command_manager->login_command(reply,username,password,&client_address,"");
            if(logged_in){
                notify_sync(message);
            }
            send_message(reply);
            continue;

        } 
            
        else if(tokens[0] == "create_user") {
            if(tokens.size() != 3) {
                reply="Usage: create_user <username> <password>\n";
                send_message(reply);
                continue;
            }
            username = tokens[1];
            string password = tokens[2];
            if(command_manager->create_user_command(reply,username,password,&client_address,"")){
                send_message(reply);
                notify_sync(message);
            }
            else{
                send_message(reply);
            }
            continue;
            
        }

        else if (tokens[0] == "exit") {
            logger->log("Client Exited ",ip,port,"INFO",true);
            string reply;
            command_manager->logout_command(reply,username,&client_address,"");
            notify_sync("logout "+username);
            if(!socket_closed){
                close(socket_id);
                socket_closed = true;
            }
            return false;
        }

        else if (tokens[0] == "logout") {
            if(tokens.size() != 2) {
                reply="Usage: logout <username>\n";
                send_message(reply);
                continue;
            }
            username = tokens[1];
            command_manager->logout_command(reply,username,&client_address,"");
            send_message(reply);
            notify_sync("logout "+username);
            logged_in=false;
            continue;
        }

        else {
            reply = "Please login/Create first using the login/create_user command.\n";
            send_message(reply);
        }

        
    }
    return true;
}

void ClientManager::init_unexpected_close(){
    string reply;
    logger->log("Client unexpected disconnection",ip,port,"FAILED",true);
    command_manager->logout_command(reply,username,&client_address,"");
    notify_sync("logout "+username);
    if(!socket_closed){
        close(socket_id);
        socket_closed = true;
    }
}

// after login other command mannage
bool ClientManager::while_loop() {
    char buffer[1024];
    

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(socket_id, buffer, sizeof(buffer)-1, 0);

        if (bytes <= 0) {
            init_unexpected_close();
            break;
        }

        // convert trime and tokennize it
        string message="";
        if (bytes > 0) {
            buffer[bytes] = '\0';
            message = buffer;
        }

        trim_whitespace(message);
        if(message.empty()) {
            send_message("Invalid command. Please try again.\n");
            continue;
        }

        //tokenize user message
        vector<string> tokens;
        string reply;
        tokenize(message, tokens);
        if(tokens.empty()) {
            send_message("Invalid command. Please try again.\n");
            continue;
        }



        if (tokens[0] == "exit") {
            logger->log("Client Exited ",ip,port,"INFO",true);
            string reply;
            command_manager->logout_command(reply,username,&client_address,"");
            notify_sync("logout "+username);
            if(!socket_closed){
                close(socket_id);
                socket_closed = true;
            }
            break;
        }

        else if(tokens[0]=="logout"){
            string reply;
            command_manager->logout_command(reply,username,&client_address,"");
            send_message(reply);
            notify_sync("logout "+username);
            if(!login_loop()){
                if (!socket_closed) {
                    close(socket_id);
                    socket_closed = true;
                }
                logger->log("Error during logout communication", ip, port, "ERROR", true);
                return false;
            }
            continue;

        }

        else if(tokens[0]=="create_group"){
            if(tokens.size() != 2) {
                reply="Usage: create_group <group_id>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string reply;
            if(command_manager->create_group_command(reply,username,group_id,&client_address,"")){
                send_message(reply);
                notify_sync(message+" "+username);
            }
            else{
                send_message(reply);
            }
            continue;
        }

        else if(tokens[0]=="list_groups"){
            string reply;
            command_manager->list_group_command(reply,&client_address,"");
            send_message(reply);
            continue;
        }

        else if(tokens[0]=="join_group"){
            if(tokens.size() != 2) {
                reply="Usage: join_group <group_id>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string reply;
            if(command_manager->join_group_command(reply,username,group_id,&client_address,"")){
                send_message(reply);
                notify_sync(message+" "+username);
            }
            else{
                send_message(reply);
            }
            continue;
        }

        else if(tokens[0]=="list_requests"){
            if(tokens.size() != 2) {
                reply="Usage: list_requests <group_id>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string reply;
            command_manager->list_request_command(reply,username,group_id,&client_address,"");
            send_message(reply);
            continue;
        }

        else if(tokens[0]=="accept_request"){
            if(tokens.size() != 3) {
                reply="Usage: accept_request <group_id> <user_id>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string requestedname = tokens[2];
            string reply;
            if(command_manager->accept_request_command(reply,username,requestedname,group_id,&client_address,"")){
                send_message(reply);
                notify_sync(message+" "+username);
            }
            else{
                send_message(reply);
            }
            continue;
        }

        else if(tokens[0]=="leave_group"){
            if(tokens.size() != 2) {
                reply="Usage: leave_group <group_id>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string reply;
            if(command_manager->leave_group_command(reply,username,group_id,&client_address,"")){
                send_message(reply);
                notify_sync(message+" "+username);
            }
            else{
                send_message(reply);
            }
            continue;

        }

        else if(tokens[0]=="list_files"){
            if(tokens.size() != 2) {
                reply="Usage: list_files <group_id>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string reply;
            command_manager->list_files_command(reply,username,group_id,&client_address,"");
            send_message(reply);
            continue;
        }

        else if(tokens[0]=="upload_file"){
            if(tokens.size() != 3) {
                reply="Usage: upload_file <group_id> <file_name>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string file_path = tokens[2];
            string file_name = file_path.substr(file_path.find_last_of("/\\") + 1);
            string reply;

            if(command_manager->upload_file_command(reply,username,group_id,file_name,&client_address,"")){
                send_message(reply);
                //size_accroding max data in File_Info 4048
                uint64_t msg_len;
                recv(socket_id, &msg_len, sizeof(msg_len), MSG_WAITALL);
                msg_len = ntohll(msg_len);

                string buffer(msg_len, '\0');
                recv(socket_id, &buffer[0], msg_len, MSG_WAITALL);

                string file_info_command = buffer;

                trim_whitespace(file_info_command);
                if(file_info_command.empty()) {
                    send_message("File data is empty. Please try again.\n");
                    continue;
                }

                 // remove intiaal command and trim it  upload_file_data <file_info>
                string file_info = file_info_command.substr(file_info_command.find(' ') + 1);
                trim_whitespace(file_info);
                if(file_info.empty()) {
                    send_message("File data is empty. Please try again.\n");
                    continue;
                }
                FileInfo finfo = FileInfo::fromString(file_info);

                command_manager->upload_file_data(reply,username,group_id,file_name,finfo,&client_address,"");
                send_message(reply);
                notify_sync(file_info_command);
            }
            else{
                send_message(reply);
            }
            continue;
        }

        else if(tokens[0]=="download_file"){
            if(tokens.size() != 3) {
                reply="Usage: download_file <group_id> <file_name>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string file_name = tokens[2];
            string reply;
            command_manager->download_file_command(reply,username,group_id,file_name,&client_address,"");

            uint64_t msg_len = reply.size();
            uint64_t len_net = htonll(msg_len);
            send(socket_id, &len_net, sizeof(len_net), 0);
            send_message(reply);
            continue;
        }

        else if(tokens[0]=="update_file_info"){
            cout<<tokens.size()<<endl;
            for(auto t:tokens) cout<<t<<"--";
            cout<<endl;
            if(tokens.size() != 4) {
                cout<<message<<endl;
                reply="Usage: update_file_info <group_id> <file_name> <new_file_path>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string file_path = tokens[2];
            string file_name = file_path.substr(file_path.find_last_of("/\\") + 1);
            string new_file_path = tokens[3];
            string reply;
            if(command_manager->update_file_info(reply,username,group_id,file_name,new_file_path,&client_address,"")){
                send_message(reply);
                notify_sync(message+" "+username);
            }
            else{
                send_message(reply);
            }
            continue;
        
        }

        else if(tokens[0]=="stop_share"){
            if(tokens.size() != 3) {
                reply="Usage: stop_share <group_id> <file_name>\n";
                send_message(reply);
                continue;
            }
            string group_id = tokens[1];
            string file_path = tokens[2];
            string file_name = file_path.substr(file_path.find_last_of("/\\") + 1);
            string reply;
            if(command_manager->stop_share(reply,username,group_id,file_name,&client_address,"")){
                notify_sync(message+" "+username);
                send_message(reply);
            }
            else{
                send_message(reply);
            }
            
            continue;
        }

        else if(tokens[0]=="sync"){
            // remove intiaal command and trim it <SYNC IP PORT command>
            size_t pos = message.find("sync");
            std::string cmd = message.substr(pos + strlen("sync"));
            trim_whitespace(cmd);
            if(cmd.empty()) {
                send_message("Invalid SYNC command. Please try again.\n");
                continue;
            }
            command_manager->sync_handler(cmd);
            continue;
        }

        else if(tokens[0]=="login" || tokens[0]=="create_user"){
            reply="You are already logged in. Please logout first to login/create another user.\n";
            send_message(reply);
            continue;
        }
        
        reply="Please, Enter valid command.\n";

        send_message(reply);
    }

    return true;
}


void ClientManager::start_communication(){

    bool login_flag = login_loop();

    if (!login_flag) {
        if (!socket_closed) {
            close(socket_id);
            socket_closed = true;
        }
        logger->log("Error during login communication", ip, port, "ERROR", true);
        return;
    }

    while_loop();
    return;
}
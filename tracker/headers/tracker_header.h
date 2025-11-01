#pragma once
#ifndef TRACKER_HEADER_H
#define TRACKER_HEADER_H


#include "logger_header.h"
#include <iostream>
#include <string>
#include <netinet/in.h>
using namespace std;

struct Address;
class UserManager;
class GroupManager;
class FileManager;
class CommandManager;

class Tracker {
private:
    string tracker_file;
    string tracker_ip;
    int tracker_port;
    int tracker_id;
    int tracker_sock;
    shared_ptr<UserManager> um;
    shared_ptr<GroupManager> gm;
    shared_ptr<FileManager> fm;
    shared_ptr<Logger> logger;
    shared_ptr<CommandManager> command_manager;
    vector<Address> other_trackers;
    mutex sync_mutex;


    bool set_tracker_address();
    bool start_as_server();
    void handle_client(int client_sock);
    void assign_task_to_thread(int client_socket);
    void init_sync();
    void send_sync_message(const Address& address, const string& message,int& updated_count );
    void input_listener();
    

public:
    Tracker(const string& tracker ,const int id);
    ~Tracker();
    bool start();
    void start_sync(string message);
    bool stop();

};

#endif


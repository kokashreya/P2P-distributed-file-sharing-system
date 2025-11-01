#pragma once
#ifndef CLIENT_HEADER_H
#define CLIENT_HEADER_H

#include <iostream>
#include <bits/stdc++.h>
#include <string>
#include <mutex>
#include <netinet/in.h>
#include "./utils_header.h"
#include "./file_header.h"
using namespace std;


struct DownloadTask {
    string result;
    mutex m;
    bool done = false;
    int total_pieces = 0;
    int completed_pieces = 0;
    DownloadTask() : result("not started"), done(false) {}
};

class Client {
private:
    const int max_tracker = 3;
    string ip_address;
    int port_number;
    string tracker_file;
    string tracker_ip;
    int tracker_port;
    int tracker_id;
    int tracker_sock;
    int client_sock;
    struct sockaddr_in tracker_address{};
    string username;
    string password;
    bool logged_in=false;

    bool read_tracker();
    bool set_tracker_address();
    bool connect_to_tracker();
    void start_client_command_loop();
    void server_listener(int server_fd, struct sockaddr_in address, socklen_t addrlen);
    bool start_as_listener();
    void handle_client(int client_sock, mutex &file_mutex);
    void assign_task_to_thread(int client_socket,mutex &file_mutex);
    void reset_tracker();
    string handle_command(string command);
    string file_upload_command(string command);

    string file_download_command(string command, shared_ptr<DownloadTask> task_ptr, shared_ptr<mutex> results_mutex);
    void assign_download_task(string piece_sha, shared_ptr<std::map<string, Address>> seeder_ptr, int piece_index, shared_ptr<std::map<string, string>> file_path_map_ptr, string destination_file_name, uint64_t piece_size, uint64_t total_size, shared_ptr<std::unordered_map<int,bool>> download_results, shared_ptr<std::mutex> file_mutex, shared_ptr<DownloadTask> download_task, shared_ptr<std::mutex> results_mutex);
    string receive_full_file_data(int sock);
    bool download_piece(string piece_sha, shared_ptr<std::map<string, Address>> seeder_list_ptr, int piece_index, shared_ptr<std::map<string, string>> file_path_map_ptr, const string destination_file_name, uint64_t piece_size, uint64_t total_size, shared_ptr<std::mutex> file_mutex);
    int connect_with_timeout(const std::string& ip, int port, int timeout_sec);
    bool receive_piece(int sock, string& piece_content, uint64_t piece_size);
    bool write_content(const string file_path, int piece_index, const string& content, uint64_t piece_size, shared_ptr<std::mutex> file_mutex);

public:
    Client(const string& ip, int port, const string& tracker);
    bool start();
    bool stop();

};

#endif

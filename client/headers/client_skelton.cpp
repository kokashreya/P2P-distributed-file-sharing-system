#include "./client_header.h"
#include "./thread_header.h"
#include "./utils_header.h"
#include "./file_header.h"
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <thread>
#include <vector>
#include <cstring>
#include <unistd.h>

#include <mutex>
#include "client_header.h"

using namespace std;

mutex tracker_comm_mutex;

static uint64_t htonll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN
    return (((uint64_t)htonl((uint32_t)(v & 0xffffffffULL))) << 32) |
           htonl((uint32_t)(v >> 32));
#else
    return v;
#endif
}
static uint64_t ntohll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN
    return (((uint64_t)ntohl((uint32_t)(v & 0xffffffffULL))) << 32) |
           ntohl((uint32_t)(v >> 32));
#else
    return v;
#endif
}

vector<shared_ptr<DownloadTask>> download_history;
mutex download_history_mutex;


Client::Client(const string& ip, int port, const string& tracker){
    ip_address = ip;
    port_number = port;
    tracker_file = tracker;
    tracker_id=-1;
    tracker_ip="";
    tracker_port=-1;
    tracker_sock=-1;
    username="";
    password="";
}


//-------------------------------------------------------Client as Server Act----------------------------------------------------------//


void Client::handle_client(int client_sock, mutex &file_mutex) {
    string client_info;
    char small_buf[4096];
    ssize_t bytes = recv(client_sock, small_buf, sizeof(small_buf)-1, 0);
    if (bytes <= 0) { 
        close(client_sock); 
        return; 
    }
    client_info.assign(small_buf, (size_t)bytes);
    trim_whitespace(client_info);

    vector<string> tokens;
    tokenize(client_info, tokens);
    if (tokens.size() != 3 || tokens[0] != "get_piece") {
        cerr << "Invalid get_piece request: " << client_info << endl;
        close(client_sock);
        return;
    }

    string file_path = tokens[1];
    int piece_index = stoi(tokens[2]);

    lock_guard<mutex> lock(file_mutex);
    struct stat64 st{};
    if (stat64(file_path.c_str(), &st) == -1) { 
        perror("stat64"); close(client_sock); 
        return; 
    }

    const uint64_t global_piece_size = 512ULL*1024ULL;
    uint64_t total_pieces = (st.st_size + global_piece_size - 1)/global_piece_size;
    if (piece_index >= (int)total_pieces) { 
        close(client_sock); 
        return; 
    }


    uint64_t piece_size = (piece_index == (int)total_pieces-1)? st.st_size - (uint64_t)piece_index*global_piece_size : global_piece_size;

    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0) { 
        perror("open"); 
        close(client_sock); 
        return; 
    }
    off64_t offset = (off64_t)piece_index*global_piece_size;
    if (lseek64(fd, offset, SEEK_SET) == (off64_t)-1) { 
        perror("lseek64"); 
        close(fd); 
        close(client_sock); 
        return; 
    }

    // send piece length (64-bit)
    uint64_t net_len = htonll(piece_size);
    if (send(client_sock, &net_len, sizeof(net_len), 0) != sizeof(net_len)) {
        close(fd); close(client_sock); 
        return;
    }

    char buf[1024*1024];
    uint64_t remaining = piece_size;
    while (remaining > 0) {
        size_t chunk = min<uint64_t>(sizeof(buf), remaining);
        ssize_t r = read(fd, buf, chunk);
        if (r <= 0) { 
            perror("read"); 
            break; 
        }
        ssize_t s = send(client_sock, buf, r, 0);
        if (s <= 0) { 
            perror("send"); break; }
        remaining -= (uint64_t)s;
    }

    close(fd);
    close(client_sock);
}
// assign_task_to_thread: catch exceptions and ensure release_thread runs
void Client::assign_task_to_thread(int client_socket, mutex &file_mutex) {
    unique_lock<mutex> lock(thread_mutex);
    thread_cv.wait(lock, [] { return is_any_thread_available(); });
    int idx = get_available_thread();
    lock.unlock();

    thread_pool[idx] = thread([this, idx, client_socket, &file_mutex]() {
        try {
            handle_client(client_socket, file_mutex);
        } catch (const exception &ex) {
            cerr << "Exception in client handler thread: " << ex.what() << endl;
        } catch (...) {
            cerr << "Unknown exception in client handler thread\n";
        }
        release_thread(idx);
    });
    
    thread_pool[idx].detach();
}

// when Other client come then make thread of it send to hnadle client
void Client::server_listener(int server_fd, struct sockaddr_in address, socklen_t addrlen) {
    mutex file_mutex;
    while (true) {
        int other_client_sock = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (other_client_sock < 0) {
            continue;
        }
        
        // cout<<"Accepted connection from client: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << endl; 
        assign_task_to_thread(other_client_sock,file_mutex);

    }
}

// start as lister on given port to accept other client request
bool Client::start_as_listener()
{
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        cerr << "Failed to create socket.\n";
        return false;
    }

    // bind IP:PORT with give ip and port address for pubulic connetion
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip_address.c_str());
    address.sin_port = htons(port_number);

    cout<< "Client listening on " << ip_address << ":" << port_number << endl;

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }


    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        cerr << "Failed to bind socket.\n";
        return false;
    }

    if (listen(server_fd, 3) < 0) {
        cerr << "Failed to listen on socket.\n";
        return false;
    }

    thread listener_thread(&Client::server_listener, this, server_fd, address, addrlen);
    listener_thread.detach();

    return true;
}


//-------------------------------------------------------Client Start communication with Tracker----------------------------------------------------------//

// this function read tracker, by trcaker id
bool Client::read_tracker()
{
    string line = read_file_by_line_number(tracker_file, tracker_id + 1);
    if(line.empty()) {
        cerr << "Failed to read tracker information.\n";
        return false;
    }

    //check address validation
    if(!string_address_validation(line)) {
        cerr << "Invalid tracker address format in file: " << line << "\n";
        return false;
    }

    size_t colon_pos = line.find(':');
    size_t space_pos = line.find(' ', colon_pos);
    tracker_ip = line.substr(0, colon_pos);
    tracker_port = stoi(line.substr(colon_pos + 1, space_pos - colon_pos - 1));
    tracker_id+=1;
    return true;
}

// if successfully set tracker ip then make address struct 
bool Client::set_tracker_address(){
    if(!read_tracker()) {
        return false;
    }
    tracker_address.sin_family = AF_INET;
    tracker_address.sin_port = htons(tracker_port);
    inet_pton(AF_INET, tracker_ip.c_str(), &(tracker_address.sin_addr));
    return true;
}

// this funtion connect to tracker and send it public ip, but it connected with diffrent ip , because on public ip it is listening
bool Client::connect_to_tracker() {

    for (int i = 0; i < max_tracker; ++i) {
        tracker_id = i;
        if (!set_tracker_address()) {
            cerr << "Failed to get tracker address for tracker " << i << ".\n";
            continue;
        }

        tracker_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (tracker_sock < 0) {
            cerr << "Socket creation failed for tracker " << i << ".\n";
            continue;
        }

        if (connect(tracker_sock, (struct sockaddr*)&tracker_address, sizeof(tracker_address)) == 0) {
            cout << "Connected successfully to tracker " << i << " at " << tracker_ip << ":" << tracker_port << endl;
            // this ip port to tracker to save it for other
            string msg=ip_address + "  " + to_string(port_number) + "\n";
            int send_bytes=send(tracker_sock, msg.c_str(), msg.size(), MSG_NOSIGNAL);
            if (send_bytes == -1) {
                perror("send failed to client " );
                
            }
            return true;
        } else {
            cerr << "Failed to connect to tracker " << i << " at " << tracker_ip << ":" << tracker_port << endl;
            close(tracker_sock);
        }
    }
    cerr << "Could not connect to any tracker.\n";
    return false;
}

// this funtion wait until full message not recevie , becasu i am sending request for reconnect
string receive_full_message(int sock) {
    char buffer[1024];
    string result = "";
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';
        result += buffer;
        if (result.find('\n') != string::npos) {
            break;
        }
    }
    return result;
}

// if connection broken with tracker then this funtion try to find other tracker to connect it 
void Client::reset_tracker() {
    cout << "Attempting to reconnect to another tracker..." << endl;

    //if success fully connected with other tracker any aviable
    if (connect_to_tracker()) {
        cout << "Connected to new tracker at " << tracker_ip << ":" << tracker_port << endl;
        //if user priviouly connected or logged in then try to relogin by user authority
        if (!username.empty() && !password.empty() && logged_in) {
            cout << "Do you want to log in again as " << username << "? (Y/y to confirm): ";
            string response;
            getline(cin, response);

            if (!response.empty() && (response[0] == 'Y' || response[0] == 'y')) {
                
                // send logout first , because in other tracker SYNC sended by privious tracker on login , if try to relogin thenr give error, firts logout from new connected , it will send notify other tracker to logout 
                string logout_cmd = "logout " + username + "\n";
                send(tracker_sock, logout_cmd.c_str(), logout_cmd.size(), MSG_NOSIGNAL);

                string logout_res= receive_full_message(tracker_sock);
                logged_in=false;
                cout<<"You successfully logout from past session\n";

                // then try agin login in new traker, this also notify to other tracker, to update logged_in entries
                string login_cmd = "login " + username + " " + password + "\n";
                int send_bytes = send(tracker_sock, login_cmd.c_str(), login_cmd.size(), MSG_NOSIGNAL);
                if (send_bytes == -1) {
                    perror("Failed to send login command");
                    return;
                }

                string response_str= receive_full_message(tracker_sock);
                if (response_str.find("successful") != string::npos) {
                    logged_in=true;
                    cout << "Logged in successfully as " << username << " in other Tracker.\n";

                } else {
                    cout << "Unable to login, please try again.\n";
                }

            } else {
                cout << "Skipped login. Connected to the tracker but not logged in.\n";
            }
        }
        else{
            cout << "Please login to the tracker.\n";
        }
    } else {
        cout << "Failed to connect to any tracker after reset.\n";
    }
}


//-------------------------------------------------------Client Start----------------------------------------------------------//



                      // -------------------------------------upload-------------------------------------- //
string Client::file_upload_command(string command) {

    //intially send only command validate to tracker
    send(tracker_sock, command.c_str(), command.size(), MSG_NOSIGNAL);

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int n = recv(tracker_sock, buffer, sizeof(buffer) - 1, 0);

    if (n <= 0) {
        cout << "Disconnected from tracker.\n";
        reset_tracker();
        return "";
    }

    buffer[n] = '\0';

    string response(buffer);

    

    if(response.find("send_all_data") == string::npos) {
        return response; 
    }

    cout << "\nSending Meta Data to Tracker for Uploading File................" << endl;
    string file_data = generate_file_message(command,username,ip_address,port_number);
    if (file_data.empty()) {
        return "Failed to read file data.\n";
    }
    
    
    string new_command = "upload_file_data " + file_data + "\n";
    
    
    uint64_t msg_len = htonll(new_command.size());
    send(tracker_sock, &msg_len, sizeof(msg_len), 0);
    send(tracker_sock, new_command.c_str(), new_command.size(), MSG_NOSIGNAL);

    memset(buffer, 0, sizeof(buffer));
    n = recv(tracker_sock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        cout << "Disconnected from tracker.\n";
        reset_tracker();
        return "";
    }
    buffer[n] = '\0';
    response = string(buffer);
    
    return response;
}



                     //-------------------------------------- download ------------------------------------//

// this function connect to seeder with timeout if not connected in give time then try with other seeder
//----------- Client side: connect with timeout ----------
int Client::connect_with_timeout(const string& ip, int port, int timeout_sec) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    } 

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock; // success
}

// ---------- Client side: piece receive ----------
bool Client::receive_piece(int sock_fd, string &piece_data, uint64_t expected_size) {
    uint64_t net_size;
    if (recv(sock_fd, &net_size, sizeof(net_size), MSG_WAITALL) != sizeof(net_size))
        return false;

    uint64_t piece_size = ntohll(net_size);
    if (piece_size != expected_size) {
        cerr << "Piece size mismatch\n";
        return false;
    }

    piece_data.resize(piece_size);
    uint64_t bytes_received = 0;

    while (bytes_received < piece_size) {
        ssize_t chunk = recv(sock_fd, &piece_data[bytes_received], piece_size - bytes_received, 0);
        if (chunk <= 0) return false;
        bytes_received += static_cast<uint64_t>(chunk);
    }

    return true;
}

// ---------- Client side: write piece to file ----------
bool Client::write_content(const string path, int index, const string &data,uint64_t global_piece_size, shared_ptr<mutex> file_mutex) {
    lock_guard<mutex> lock(*file_mutex);
    int fd = open64(path.c_str(), O_WRONLY);
    if (fd < 0) { 
        perror("open64"); 
        return false; 
    }
    off64_t offset = (off64_t)index * (off64_t)global_piece_size;
    if (lseek64(fd, offset, SEEK_SET) == (off64_t)-1) { 
        perror("lseek64"); 
        close(fd); 
        return false; 
    }

    const char *buf = data.data();
    uint64_t left = data.size(), written = 0;
    while (written < left) {
        ssize_t w = write(fd, buf + written, left - written);
        if (w < 0) { 
            perror("write"); 
            close(fd); 
            return false; 
        }
        written += (uint64_t)w;
    }
    close(fd);
    return true;
}

// ---------- Client side: download one piece ----------
bool Client::download_piece(const string piece_sha, shared_ptr<map<string, Address>> seeders,int piece_index, shared_ptr<map<string,string>> file_paths,const string dest, uint64_t piece_size, uint64_t total_size,shared_ptr<mutex> file_mutex) {
    if (seeders->empty()) return false;

    int total_pieces = (total_size + piece_size - 1)/piece_size;
    uint64_t expected_size = (piece_index == total_pieces-1)? total_size - (uint64_t)piece_index*piece_size : piece_size;

    vector<string> keys;
    keys.reserve(seeders->size());
    for (const auto &p : *seeders) keys.push_back(p.first);

    int num_seeders = keys.size();
    int start_index = piece_index % num_seeders; // deterministic sprea

    // Try all seeders until one succeeds
    for (int i = 0; i < num_seeders; ++i) {
        const string &seeder = keys[(start_index + i) % num_seeders];
        Address address = (*seeders)[seeder];
        int sock = connect_with_timeout(address.ip, address.port, 10);
        if (sock < 0) {
            continue;
        }
        string req = "get_piece " + (*file_paths)[seeder] + " " + to_string(piece_index) + "\n";
        if (send(sock, req.c_str(), req.size(), MSG_NOSIGNAL) <= 0) { 
            close(sock); 
            continue; 
        }

        string piece;
        if (!receive_piece(sock, piece, expected_size)) { 
            close(sock); 
            continue; 
        }
        close(sock);

        if (calculate_SHA(piece) != piece_sha) 
        {
            continue;
        }
        return write_content(dest, piece_index, piece, piece_size, file_mutex);
    }
    return false;
}

// assign download task to thread pool
void Client::assign_download_task(const string piece_sha,shared_ptr<map<string, Address>> seeder_ptr,int piece_index,shared_ptr<map<string, string>> file_path_map_ptr,const string destination_file_name,uint64_t piece_size,uint64_t total_size,shared_ptr<unordered_map<int,bool>> download_results,shared_ptr<mutex> file_mutex, shared_ptr<DownloadTask> download_task,shared_ptr<mutex> results_mutex)
{
    unique_lock<mutex> lock(thread_mutex);
    thread_cv.wait(lock, [] { 
        return is_any_thread_available(); 
    });
    int idx = get_available_thread();
    lock.unlock();

    string dest_copy = destination_file_name;
    string sha_copy = piece_sha;

    thread_pool[idx] = thread([this, idx, sha_copy, seeder_ptr, piece_index, file_path_map_ptr,dest_copy, piece_size, total_size, download_results,file_mutex, download_task, results_mutex]() {
        bool success = false;
        try {
            success = download_piece(sha_copy, seeder_ptr, piece_index, file_path_map_ptr, dest_copy, piece_size, total_size, file_mutex);
        } catch (const std::exception &ex) {
            cerr << "Exception in download thread " << piece_index << ": " << ex.what() << endl;
            success = false;
        } catch (...) {
            cerr << "Unknown exception in download thread " << piece_index << endl;
            success = false;
        }

        try {
            lock_guard<mutex> guard(*results_mutex);
            (*download_results)[piece_index] = success;
            if (success) {
                lock_guard<mutex> task_guard(download_task->m);
                download_task->completed_pieces++;
            }
        } catch (...) {
            cerr << "Error updating download results for piece " << piece_index << endl;
        }

        release_thread(idx);
    });

    thread_pool[idx].detach();
}

// receive full message from socket, first read length then read full data
string Client::receive_full_file_data(int sock) {
    uint64_t len_net = 0;
    if (recv(sock, &len_net, sizeof(len_net), MSG_WAITALL) != (ssize_t)sizeof(len_net)) {
        cerr << "Failed to receive message length" << endl;
        return "";
    }
    
    uint64_t msg_len = ntohll(len_net);
    
    // Add safety check for message length
    if (msg_len == 0 || msg_len > 1024*1024) { // Max 1MB
        cerr << "Invalid message length: " << msg_len << endl;
        return "";
    }

    string ACK = "ACK\n";
    if (send(sock, ACK.c_str(), ACK.size(), MSG_NOSIGNAL) <= 0) {
        cerr << "Failed to send ACK" << endl;
        return "";
    }

    string result;
    result.reserve(msg_len); // Reserve instead of resize
    result.resize(msg_len);
    
    uint64_t total = 0;
    const size_t CHUNK_SIZE = 8192; // Read in smaller chunks
    
    while (total < msg_len) {
        size_t to_read = min(CHUNK_SIZE, (size_t)(msg_len - total));
        ssize_t n = recv(sock, &result[total], to_read, 0);
        if (n <= 0) {
            cerr << "Failed to receive data at offset " << total << endl;
            return "";
        }
        total += (uint64_t)n;
    }
    
    return result;
}

// string read_piece_from_file(const string &path, int index, uint64_t piece_size, uint64_t total_size) {
//     uint64_t total_pieces = (total_size + piece_size - 1) / piece_size;
//     uint64_t last_size = (index == (int)total_pieces - 1) ? (total_size - index * piece_size) : piece_size;
//     vector<char> buf(last_size);
//     int fd = open64(path.c_str(), O_RDONLY);
//     if (fd < 0) return "";
//     off64_t off = (off64_t) index * (off64_t) piece_size;
//     lseek64(fd, off, SEEK_SET);
//     ssize_t r = read(fd, buf.data(), last_size);
//     close(fd);
//     if (r != (ssize_t)last_size) return "";
//     return string(buf.begin(), buf.end());
// }

// drain_socket: read and discard all data from socket
void drain_socket(int sock) {
    char tmp[4096];
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK); // Set non-blocking
        
        while (true) {
            ssize_t n = recv(sock, tmp, sizeof(tmp), 0);
            if (n <= 0) break;
        }
        
        fcntl(sock, F_SETFL, flags); // Restore original flags
    }
}

// main function to download file, it first get file info from tracker then assign task to thread pool to download piece of file
string Client::file_download_command(string command, shared_ptr<DownloadTask> download_task, shared_ptr<mutex> results_mutex) {
                             //-----------------first get file info from tracker-----------------//

    //remove destination_path from command before sending to tracker
    size_t last_space = command.find_last_of(' ');
    if (last_space == string::npos) {
        return "Invalid command format. Usage: download_file <group id> <file path> <destination>\n";
    }
    string command_to_send = command.substr(0, last_space);
    std::lock_guard<std::mutex> lk(tracker_comm_mutex);
    drain_socket(tracker_sock);
    send(tracker_sock, command_to_send.c_str(), command_to_send.size(), MSG_NOSIGNAL);


    string file_info_command = receive_full_file_data(tracker_sock);
    trim_whitespace(file_info_command);
    if(file_info_command.empty()) {
        return "File data is empty. Please try again.\n";
    }
    // cout<<"Received response from tracker: "<<file_info_command<<endl;

    if(file_info_command.find("file_data") == string::npos) {
        return file_info_command+"\n"; 
    }
    string file_info = file_info_command.substr(file_info_command.find(' ') + 1);
    trim_whitespace(file_info);

    if(file_info.empty()) {
        return "File data is empty. Please try again.\n";
    }
    // cout<<"Received file info from tracker. Preparing to download..."<<endl;
    // cout<<"Debug: File Info: "<<file_info.size()<<endl;
    FileInfo finfo = FileInfo::fromString(file_info);
    
    for(auto &[user, addr] : finfo.seeder_users) {
        cout << "\nSeeder: " << user << " at " << addr.ip << ":" << addr.port;
    }
    cout<<"\n>";

                              // ---------------prepare meta data for download-----------------//

    
    string destination_file_name = command.substr(last_space + 1);
    if(destination_file_name.back() == '\n' || destination_file_name.back() == '\r') {
        destination_file_name.pop_back();
    }
    trim_whitespace(destination_file_name);
    


    struct stat path_stat;
    if (stat(destination_file_name.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        // It's a directory → append file name
        if (destination_file_name.back() != '/' && destination_file_name.back() != '\\') {
            destination_file_name += "/";
        }
        destination_file_name += finfo.name;
    } 
    
    trim_whitespace(destination_file_name);
    if(!create_file(destination_file_name, finfo.size)) {
        return "Failed to create destination file: " + destination_file_name + "\n";
    }
    // cout<<"Destination file ready: " << destination_file_name << endl;
    

    //------------------------------ assign task to thread download piece of file------------------------------//

    
    vector<int>  piece_order(finfo.piece_SHA.size());
    iota(piece_order.begin(), piece_order.end(), 0);
    random_device rd;
    mt19937 g(rd());
    shuffle(piece_order.begin(), piece_order.end(), g);
    auto download_results_ptr = make_shared<unordered_map<int,bool>>();
    auto file_mutex_ptr = make_shared<mutex>();
    auto seeder_ptr = make_shared<map<string, Address>>(finfo.seeder_users);
    auto file_path_map_ptr = make_shared<map<string, string>>(finfo.user_file_map);

    {
        lock_guard<mutex> task_guard(download_task->m);
        download_task->total_pieces = finfo.piece_SHA.size();
        download_task->completed_pieces = 0;
        download_task->done = false;
        download_task->result = "[R] "+finfo.group + " " +finfo.name;
    }



    for (size_t i = 0; i < piece_order.size(); i++) {
        string piece_sha = finfo.piece_SHA[piece_order[i]];
        // add sleep of 0.05 sec
        if (i % 100 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        assign_download_task(piece_sha, seeder_ptr, piece_order[i], file_path_map_ptr, destination_file_name, finfo.piece_size, finfo.size, download_results_ptr, file_mutex_ptr, download_task, results_mutex);

    }

    join_all_threads();
    drain_socket(tracker_sock);

    {
        lock_guard<mutex> guard(*results_mutex);
        for (auto &[piece_index, success] : *download_results_ptr) {
            if (!success) {
                download_task->result="[F] "+finfo.group + " " +finfo.name;
                download_task->done = true;
                 if (remove(destination_file_name.c_str()) != 0) {
                    perror("Error deleting file");
                } else {
                    cout<<download_task->result + "\n";
                }
                // cout<<"Download failed: piece " << piece_index << " corrupted or missing\n";
                return "Download failed: piece " + to_string(piece_index) + " corrupted or missing\n";
            }
        }
    }

    char full_path[PATH_MAX];
    string saved_full_path;
    if (realpath(destination_file_name.c_str(), full_path) != nullptr) {
        saved_full_path = string(full_path);
    }

    string full_file_sha = calculate_full_file_SHA(saved_full_path);
    // cout<<"Expected Full File SHA: " << finfo.full_SHA << endl;
    // cout<<"Calculated Full File SHA: " << full_file_sha << endl;

    // cout<<"Expected Size: " << finfo.size << endl;
    // struct stat file_stat;
    // if (stat(destination_file_name.c_str(), &file_stat) == 0) {
        // cout<<"Downloaded File Size: " << file_stat.st_size << endl;
    // }

    drain_socket(tracker_sock);
    
    if(full_file_sha != finfo.full_SHA) {
        // string piece_sha = read_piece_from_file(destination_file_name, 0, finfo.piece_size, finfo.size);
        // cout<<"First piece data (first 100 bytes or less): " << (piece_sha==finfo.piece_SHA[0]) << endl;
        lock_guard<mutex> tguard(download_task->m);
        download_task->result="[F] "+finfo.group + " " +finfo.name;
        download_task->done = true;
        // cout<<"Download failed: full file SHA mismatch\n";
        // if (remove(destination_file_name.c_str()) != 0) {
        //     perror("Error deleting file");
        // } else {
        //     cout<<download_task->result + "\n";
        // }
        return "[F]"+finfo.group + " " +finfo.name + "\n";
    }

    string command_to_update_fileinfo="update_file_info "+ finfo.group + " " + finfo.name + " " + saved_full_path ;
    // std::lock_guard<std::mutex> lk(tracker_comm_mutex);
    drain_socket(tracker_sock);
    send(tracker_sock, command_to_update_fileinfo.c_str(), command_to_update_fileinfo.size(), 0);

    int n;
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    while (true) {
        n = recv(tracker_sock, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);  
        if (n <= 0) {
            break;
        }
        buffer[n] = '\0'; 
    }
    string response(buffer);

    if (response.find("Failed") != string::npos) {
        // delete created_file
        lock_guard<mutex> tguard(download_task->m);
        download_task->result="[F] "+finfo.group + " " +finfo.name;
        cout<<download_task->result + "\n";
        cout<<"Download failed: unable to update tracker file info\n";
        download_task->done = true;
        if (remove(destination_file_name.c_str()) != 0) {
            perror("Error deleting file");
        }
    }
    else{
        lock_guard<mutex> tguard(download_task->m);
        download_task->result="[C] "+finfo.group + " " +finfo.name;
        download_task->done = true;
        string success_msg = "[C] " + finfo.group + " " + finfo.name + "\n>";
        cout<<success_msg;
    }
    drain_socket(tracker_sock);
    return response;
    
}



//---------------------------------------------------------command ------------------------------------------------------------//
// this funtion if login ,creae_user and logout maintain status in clicnt side,if succes come then update status
string Client::handle_command(string command) {

    //upload_file <group id> <file path>
    drain_socket(tracker_sock);
    if(command.find("upload_file") == 0){

        if(!logged_in){
            return "Please login first to upload file.\n";
        }
        vector<string> tokens;
        tokenize(command, tokens);

        if (tokens.size() != 3) {
            return "Invalid command format. Usage: upload_file <group id> <file path>\n";
        }

        string file_path = tokens[2];
        if(file_path.back() == '\n' || file_path.back() == '\r') {
            file_path.pop_back();
        }
        trim_whitespace(file_path);
        if(!validate_file_existence(file_path)){
            return "File does not exist. Please check the file path.\n";
        }

        return file_upload_command(command);

    }

    if(command.find("download_file") == 0){

        if(!logged_in){
            return "Please login first to download file.\n";
        }
        vector<string> tokens;
        tokenize(command, tokens);

        if (tokens.size() != 4) {
            return "Invalid command format. Usage: download_file <group id> <file path> <destination>\n";
        }
        

        auto task = make_shared<DownloadTask>();
        lock_guard<mutex> lg(download_history_mutex);
        download_history.push_back(task);

        auto results_mutex = make_shared<mutex>();
        thread([this, command, task, results_mutex]() {
            string res = file_download_command(command, task, results_mutex);
            cout << res << endl;
        }).detach();


        return "Download started in background. You will be notified upon completion.\n";
        // return file_download_command(command);

    }

    if(command.find("show_downloads")==0){

        vector<shared_ptr<DownloadTask>> history_copy;
        {
            lock_guard<mutex> lg(download_history_mutex);
            history_copy = download_history;
        }

        string out;
        for (auto &t : history_copy) {
            lock_guard<mutex> tg(t->m);
            if (t->done) out += t->result + "\n";
            else out += "Download Status: " + to_string(t->completed_pieces) + "/" + to_string(t->total_pieces) + " pieces completed.\n" + t->result + "\n";
        }
        if (out.empty()) return "No download history available.\n";
        return out;
    }

    drain_socket(tracker_sock);
    send(tracker_sock, command.c_str(), command.size(), MSG_NOSIGNAL);
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int n = recv(tracker_sock, buffer, sizeof(buffer) - 1, 0);

    if (n <= 0) {
        cout << "Disconnected from tracker.\n";
        reset_tracker();
        return "";
    }

    buffer[n] = '\0';

    string response(buffer);

    if (command.find("login") == 0) {
        if (response.find("Login successful") != string::npos) {
            vector<string> tokens;
            tokenize(command, tokens);
            if (tokens.size() >= 3) {
                username = tokens[1];
                password = tokens[2];
            }
            logged_in=true;
        } 
    }

    else if (command.find("create_user") == 0) {
        if (response.find("User created successfully") != string::npos) {
            vector<string> tokens;
            tokenize(command, tokens);
            if (tokens.size() >= 3) {
                username = tokens[1];
                password = tokens[2];
            }
            logged_in=false;
            
        }
    }

    else if (command.find("logout") == 0) {
        
        if (response.find("Logout successful") != string::npos) {
            logged_in=false;
        }
    }

    return response;
}

                  // ---------------------------------------------start-------------------------------------------//
// this communicate with tracker continusoly
void Client::start_client_command_loop() {
    string command;
    while (true) {
        cout << ">";
        getline(cin, command);
        trim_whitespace(command);
        if(command.empty()) {
            continue;
        }
        string msg=command + "\n";

        if (command == "exit") {
            send(tracker_sock, msg.c_str(), msg.size(), MSG_NOSIGNAL);
            cout << "Exiting client command loop.\n";
            break;
        }
        cout << "Tracker: ";
        string reply=handle_command(msg);

        cout << reply;
    }
}

//this start funtion to call publicly to start funtion
bool Client::start()
{

    if(!start_as_listener()) {
        cerr << "Failed to start client as listener.\n";
        return false;
    }


    if(!connect_to_tracker()) {
        cerr << "Failed to connect to tracker.\n";
        return false;
    }

    start_client_command_loop();
    return true;
}

bool Client::stop()
{
    // Implement any necessary cleanup here
    close(tracker_sock);
    clear_thread_pool();
    cout << "Client stopped.\n";
    return true;
}




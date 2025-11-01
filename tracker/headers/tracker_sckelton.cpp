#include "./tracker_header.h"
#include "./thread_header.h"
#include "./utils_header.h"

#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <thread>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <algorithm>

using namespace std;
std::atomic<bool> running(true);

Tracker::Tracker(const string& tracker,const int id) {
    tracker_file = tracker;
    tracker_id = id;
    tracker_ip = "";
    tracker_port = -1;
    tracker_sock = -1;


    thread_pool.resize(MAX_THREADS);
    thread_available.assign(MAX_THREADS, true);

    // make shared varible which same for accrosee all client threds
    um = make_shared<UserManager>();
    gm = make_shared<GroupManager>();
    fm = make_shared<FileManager>();
    logger = make_shared<Logger>();
    logger->initialize("log_file_" + to_string(id) + ".txt");

    command_manager = make_shared<CommandManager>(this,  um.get(), gm.get(), fm.get(), logger.get());

}
//------------------------------------------------------Tracker Syncronization-------------------------------------------------------//


// this funtion read tracker_info file and add all other tracker ip in vector to nofiy them
void Tracker::init_sync() {
    int fd = open(tracker_file.c_str(), O_RDONLY);
    if (fd == -1) {
        logger->log("Cannot open tracker file: " + tracker_file,
                    tracker_ip, tracker_port, "ERROR", true);
        return;
    }

    char buffer[1024];
    ssize_t bytes_read;
    string current;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            if (c == '\n') {
                Address address = get_address(current);
                if (!address.ip.empty() &&
                    (address.ip != tracker_ip || address.port != tracker_port)) {
                    other_trackers.push_back(address);
                }
                current.clear();
            } else {
                current.push_back(c);
            }
        }
    }

    // Handle last line if file doesn’t end with '\n'
    if (!current.empty()) {
        Address address = get_address(current);
        if (!address.ip.empty() &&
            (address.ip != tracker_ip || address.port != tracker_port)) {
            other_trackers.push_back(address);
        }
    }

    close(fd);

    for(auto& addr : other_trackers) {
        logger->log("Other tracker for sync: " + addr.ip + ":" + to_string(addr.port),
                    tracker_ip, tracker_port, "INFO", true);
    }

    logger->log("Initialized " + to_string(other_trackers.size()) +
                " other trackers for sync",
                tracker_ip, tracker_port, "INFO", true);
}

// this funtion make thread for each tracker to send notify
void Tracker::start_sync(string message){

    vector<thread> threads;
    int updated_count = 0;

    string sync_message = message;

    for (const Address& address : other_trackers) {
        // this equvivalient to make thredy extrenalu and push_back in vector of thred , and updated coutn all place go ref so i can se  where it is updated
        threads.emplace_back(&Tracker::send_sync_message, this, address, sync_message, ref(updated_count));
    }

    // Detach all threads to run independently
    for (thread& t : threads) {
        t.detach();
    }

}

// this funtion connect other tracker and wait for 5 second, if not connect then go ahead
void Tracker::send_sync_message(const Address& address, const string& message,int& updated_count ){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logger->log("SYNC Socket creation failed for " + address.ip + ":"+to_string(address.port),"",0,"ERROR");
        return;
    }

    
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(address.port);
    addr.sin_addr.s_addr = inet_addr(address.ip.c_str());

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger->log("SYNC Cannot connect to tracker  " + address.ip + ":"+to_string(address.port),"",0,"ERROR");
        close(sock);
        return;
    }


    size_t msg_len = message.size();
    string size_command = "SYNC_SIZE " + to_string(msg_len) + "\n";
   
    send(sock, size_command.c_str(), size_command.size(), 0);

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int n =recv(sock, buffer, sizeof(buffer) - 1, 0); // just to clear buffer
    if (n <= 0) {
        cout << "Disconnected from tracker during SYNC_SIZE acknowledgment.\n";
        close(sock);
        return;
    }
    buffer[n] = '\0';
    size_t total_sent = 0;
    const size_t CHUNK_SIZE = 64 * 1024;
    while (total_sent < msg_len) {
        size_t to_send = min(CHUNK_SIZE, msg_len - total_sent);
        ssize_t n = send(sock, message.c_str() + total_sent, to_send, 0);
        if (n <= 0) {
            cerr << "Send failed or connection closed\n";
            break;
        }
        total_sent += n;
    }
    close(sock);
}


//-------------------------------------------------------Tracker Send Responce----------------------------------------------------------//


void Tracker::handle_client(int client_socket) {

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    int bytes = recv(client_socket, buffer, sizeof(buffer)-1, 0);
    string client_info="";
    if (bytes > 0) {
        buffer[bytes] = '\0';
        client_info = buffer;
    }

    vector<string> tokens;
    tokenize(client_info, tokens); 
    
    
    //identify it is SYNC message form other traker then direclt go to command manager not need to make client manager 
        
    if (!tokens.empty() && tokens[0] == "SYNC_SIZE") {
        string reply = "ACK\n";
        send(client_socket, reply.c_str(), reply.size(), 0);
        
        size_t msg_len = std::stoull(tokens[1]);
       
        vector<char> full_message(msg_len);
        size_t total_received = 0;

        while (total_received < msg_len) {
            ssize_t n = recv(client_socket,
                            full_message.data() + total_received,
                            msg_len - total_received,
                            0);
            if (n <= 0) {
                cerr << "Connection closed or recv error\n";
                close(client_socket);
                return;
            }
            total_received += n;
        }

       
        string sync_mess(full_message.data(), total_received);
        
        command_manager->sync_handler(sync_mess);
        
    }
    // this is firts messge form cline t whihc send IP PORT formate mesage to infor tracker this address it listening address if any one other want to connect
    else{
        string ip = tokens[0];
        string port_str = tokens[1];
        int port = stoi(tokens[1]);
        logger->log("New Client Connected",ip,port,"INFO",true);
        ClientManager* cm = new ClientManager(this,um.get(), gm.get(),fm.get(),logger.get(),  command_manager.get(),  client_socket, ip, port);
        cm->start_communication();
        return ;
        
    }


    
}

// this furnion avible thread form thread pool and assign task
void Tracker::assign_task_to_thread(int client_socket) {
    unique_lock<mutex> lock(thread_mutex);
    // Wait until at least one thread is available
    thread_cv.wait(lock, [] {
        return is_any_thread_available();
    });


    // Get available thread index
    int idx = get_available_thread();

    lock.unlock();
    
    
    // Launch the thread to handle the client
    thread_pool[idx] = thread([this, idx, client_socket]() {
        handle_client(client_socket);
        
        release_thread(idx);
    });
    thread_pool[idx].detach();
   
}

// this start menas here tracker take address accroding id to start form tracker.info 
bool Tracker::start_as_server() {
    tracker_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tracker_sock < 0) {
        logger->log("Failed to create socket.",tracker_ip, tracker_port, "ERROR", true);
        return false;
    }

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(tracker_ip.c_str());
    address.sin_port = htons(tracker_port);

    int opt = 1;
    if (setsockopt(tracker_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger->log("setsockopt(SO_REUSEADDR) failed",tracker_ip, tracker_port, "ERROR", true);

    }

    if (bind(tracker_sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
        logger->log("Failed to bind socket.",tracker_ip, tracker_port, "ERROR", true);
        close(tracker_sock);
        return false;
    }

    if (listen(tracker_sock, 3) < 0) {
        logger->log("Failed to listen on socket.",tracker_ip, tracker_port, "ERROR", true);
        close(tracker_sock);
        return false;
    }

    socklen_t addrlen = sizeof(address);
    logger->log(("Tracker listening on " + tracker_ip+":"+to_string(tracker_port)+ " other trackers for sync"), tracker_ip, tracker_port,"INFO",true);
    while (true) {
        int other_client_sock = accept(tracker_sock, (struct sockaddr *)&address, &addrlen);
        
        if (other_client_sock < 0) {
            
            logger->log("New Request accept failed", tracker_ip, tracker_port, "ERROR", true);
            continue;
        }

        
        
        // forward to tradeing part
        assign_task_to_thread(other_client_sock);

    }

    return true;
}

//-------------------------------------------------------Tracker Start----------------------------------------------------------//

// this funtion set tracker address
bool Tracker::set_tracker_address() {
   
    string line = read_file_by_line_number(tracker_file, tracker_id);
    size_t colon_pos = line.find(':');
    size_t space_pos = line.find(' ', colon_pos);
    if (colon_pos == string::npos || space_pos == string::npos) {
        logger->log("Invalid tracker entry format: "+line ,tracker_ip, tracker_port, "ERROR", true);
        return false;
    }

    tracker_ip = line.substr(0, colon_pos);
    tracker_port = stoi(line.substr(colon_pos + 1, space_pos - colon_pos - 1));

    if (!ip_address_validation(tracker_ip, tracker_port)) {
        logger->log("Invalid tracker address: "+ tracker_ip +":"+to_string(tracker_port) ,tracker_ip, tracker_port, "ERROR", true);
        return false;
    }

    return true;
}

void Tracker::input_listener() {
    std::string cmd;
    while (running) {
        if (std::cin >> cmd) {
            if (cmd == "quit" || cmd == "exit") {
                std::cout << "Shutting down...\n";
                stop();
                raise(SIGINT);
            }
            else {
                std::cout << "Unknown command: " << cmd << std::endl;
            }
        }
    }
}

bool Tracker::start() {
    

    if(!set_tracker_address()) {
        logger->log("Failed to set tracker address.",tracker_ip, tracker_port, "ERROR", true);
        
        return false;
    }
    init_sync();

    std::thread inputThread(&Tracker::input_listener, this);
    
    if (!start_as_server()) {
        logger->log("Failed to start tracker as listener.",tracker_ip, tracker_port, "ERROR", true);
        running=false;
        inputThread.join(); 
        return false;
    }

    inputThread.join(); 
    return true;
}

bool Tracker::stop() {
    running = false;
    close(tracker_sock);
    clear_thread_pool();
    join_all_threads();
    return true;
}

//-------------------------------------------------------Tracker Stop----------------------------------------------------------//

Tracker::~Tracker() {

    logger->log("Tracker stopped successfully.",tracker_ip, tracker_port, "INFO", true);
    close(tracker_sock);
    clear_thread_pool();
    join_all_threads();
    
        

    
}
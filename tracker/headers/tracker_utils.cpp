#include "./utils_header.h"
#include "../managers/manager.h"
#include <sys/stat.h>
#include <string>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sstream>
#include <vector>
using namespace std;

bool file_validation(const string& filepath) {
    struct stat file_state;

    if(stat(filepath.c_str(),&file_state)==-1){
        cerr << "File does not exist: " << filepath << "\n";
        return false;
    }

    if(file_state.st_size == 0) {
        cerr << "File is empty: " << filepath << "\n";
        return false;
    }

    return true;
}

bool ip_address_validation(const string& ip_address, const int port) {

    if (port < 1024 || port > 65535) {
        cerr << "Port number must be between 1024 and 65535.\n";
        return false;
    }

    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ip_address.c_str(), &(sa.sin_addr));
    if(result == 0) {
        cerr << "Invalid IP address format: " << ip_address << "\n";
        return false;
    }
    else if (result < 0) { 
        perror("inet_pton failed");
        return false;
    }


    return true;
}

string read_file_by_line_number(const string& filepath, int line_number) {
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("open failed");
        return "";
    }

    string line;
    string current;
    char buffer[1024];
    ssize_t bytes_read;
    int current_line = 0;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];

            if (c == '\n') {
                current_line++;
                if (current_line == line_number) {
                    close(fd);
                    return current;
                }
                current.clear();
            } else {
                current.push_back(c);
            }
        }
    }

    if (!current.empty()) {
        current_line++;
        if (current_line == line_number) {
            close(fd);
            return current;
        }
        current.clear();    
    }

    close(fd);
    return ""; 
}

bool string_address_validation(const string& line){
    size_t colon_pos = line.find(':');
        size_t space_pos = line.find(' ', colon_pos);
        if (colon_pos == string::npos || space_pos == string::npos) {
            return false;
        }

        string ip = line.substr(0, colon_pos);
        string port_str = line.substr(colon_pos + 1, space_pos - colon_pos - 1);
        string id_str = line.substr(space_pos + 1);

        if(!ip_address_validation(ip, stoi(port_str))) {
            return false;
        }
        return true;
}

Address get_address(const std::string& line) {
    Address address;
    if (!string_address_validation(line)) {
        return Address{"", 0};
    }

    size_t colon_pos = line.find(':');
    size_t space_pos = line.find(' ', colon_pos);
    address.ip = line.substr(0, colon_pos);
    address.port = std::stoi(line.substr(colon_pos + 1, space_pos - colon_pos - 1));

    return address;
}

bool tracker_file_entry_validation(const string& filepath) {
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("open failed");
        return false;
    }

    char buffer[1024];
    ssize_t bytes_read;
    string current;
    int line_number = 0;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];

            if (c == '\n') {
                line_number++;
                if (!string_address_validation(current)) {
                    cerr << "Invalid entry in tracker file at line "<< line_number << ": " << current << "\n";
                    close(fd);
                    return false;
                }
                current.clear();
            } else {
                current.push_back(c);
            }
        }
    }


    if (!current.empty()) {
        line_number++;
        if (!string_address_validation(current)) {
            cerr << "Invalid entry in tracker file at line "
                 << line_number << ": " << current << "\n";
            close(fd);
            return false;
        }
    }

    close(fd);
    return true;
}

bool client_argument_validation(int argc, char *argv[]) {
    if(argc != 4) {
        cerr << "Usage: " << argv[0] << " <server_ip> <server_port>  <tracker.txt>\n";
        return false;
    }

    if(!file_validation(argv[3])) {
        return false;
    }

    if(!tracker_file_entry_validation(argv[3])) {
        return false;
    }

    if(!ip_address_validation(argv[1], stoi(argv[2]))) {
        return false;
    }

    return true;
}

int no_entries_in_file(const string& filepath) {
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("open failed");
        return -1;
    }

    char buffer[1024];
    ssize_t bytes_read;
    int count = 0;
    bool has_data = false;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\n') {
                count++;
                has_data = false;
            } else {
                has_data = true;
            }
        }
    }

    // If file doesn’t end with '\n' but had data, count the last line
    if (has_data) {
        count++;
    }

    close(fd);
    return count;
}

bool tracker_argument_validation(int argc, char *argv[]) {
    if(argc != 3) {
        cerr << "Usage: " << argv[0] << " <tracker.txt> <tracker_no>\n";
        return false;
    }

    if(!file_validation(argv[1])) {
        return false;
    }

    if(!tracker_file_entry_validation(argv[1])) {
        return false;
    }
    if(stoi(argv[2])-1 >= no_entries_in_file(argv[1])) {
        cerr << "Tracker number must be a positive integer.\n";
        return false;
    }
    return true;
}

void trim_whitespace(string &str){
    size_t start = str.find_first_not_of(" \n\r\t");
    size_t end = str.find_last_not_of(" \n\r\t");
    if (start == string::npos || end == string::npos) {
        str = "";
    } else {
        str = str.substr(start, end - start + 1);
    }
}

void to_lowercase(string &str){
    for (char &c : str) {
        c = tolower(c);
    }
}

void sanitize(string &s) {
    s.erase(remove(s.begin(), s.end(), '\0'), s.end());
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) {
        s.clear();
        return;
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    s = s.substr(start, end - start + 1);
}

void tokenize(const string& str, vector<string>& tokens){
    stringstream ss(str);
    string token;
    while (ss >> token) {
        sanitize(token);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
}